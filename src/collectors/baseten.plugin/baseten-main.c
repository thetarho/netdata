// SPDX-License-Identifier: GPL-3.0-or-later

#include "baseten-internals.h"
#include "libnetdata/functions_evloop/functions_evloop.h"

#define BASETEN_WORKER_THREADS 2

// Required dummy functions for external plugins
void rrdset_thread_rda_free(void){}
void sender_thread_buffer_free(void){}
void query_target_free(void){}
void service_exits(void){}
void rrd_collector_finished(void){}

// Required by get_system_cpus()
const char *netdata_configured_host_prefix = "";

// Global state
struct baseten_config config = {0};
struct baseten_cache cache = {0};
netdata_mutex_t stdout_mutex;
bool plugin_should_exit = false;

static void __attribute__((constructor)) init_mutex(void) {
    netdata_mutex_init(&stdout_mutex);
    netdata_mutex_init(&cache.mutex);
}

static void __attribute__((destructor)) destroy_mutex(void) {
    netdata_mutex_destroy(&stdout_mutex);
    netdata_mutex_destroy(&cache.mutex);
}

static void cleanup(void) {
    baseten_api_cleanup();

    netdata_mutex_lock(&cache.mutex);
    baseten_free_models(cache.models);
    baseten_free_deployments(cache.deployments);
    cache.models = NULL;
    cache.deployments = NULL;
    netdata_mutex_unlock(&cache.mutex);
}

int main(int argc __maybe_unused, char **argv __maybe_unused) {
    nd_thread_tag_set(PLUGIN_BASETEN_NAME);
    nd_log_initialize_for_external_plugins(PLUGIN_BASETEN_NAME);
    netdata_threads_init_for_external_plugins(0);

    collector_info("BASETEN: Plugin starting up (version: %s)...", PLUGIN_BASETEN_NAME);

    netdata_configured_host_prefix = getenv("NETDATA_HOST_PREFIX");
    if (verify_netdata_host_prefix(true) == -1) {
        collector_error("BASETEN: Host prefix verification failed");
        exit(1);
    }

    // Load configuration
    collector_info("BASETEN: Loading configuration...");
    if (baseten_load_config() != 0) {
        collector_error("BASETEN: Failed to load configuration. Plugin will be disabled.");
        fprintf(stdout, "DISABLE\n");
        fflush(stdout);
        exit(0);
    }

    // Initialize API client
    if (baseten_api_init() != 0) {
        collector_error("BASETEN: Failed to initialize API client. Exiting...");
        fprintf(stdout, "DISABLE\n");
        fflush(stdout);
        exit(1);
    }

    // Initialize functions event loop FIRST (before data fetch)
    collector_info("BASETEN: Initializing functions event loop with %d worker threads...", BASETEN_WORKER_THREADS);
    struct functions_evloop_globals *wg =
        functions_evloop_init(BASETEN_WORKER_THREADS, "BASETEN", &stdout_mutex, &plugin_should_exit);

    functions_evloop_add_function(
        wg, BASETEN_FUNCTION_NAME, baseten_function_deployments, BASETEN_DEFAULT_TIMEOUT, NULL);

    collector_info("BASETEN: Registering function '%s' with Netdata...", BASETEN_FUNCTION_NAME);

    // Register function with netdata IMMEDIATELY (before data fetch to avoid timeout)
    netdata_mutex_lock(&stdout_mutex);

    fprintf(
        stdout,
        PLUGINSD_KEYWORD_FUNCTION " GLOBAL \"%s\" %d \"%s\" \"baseten\" " HTTP_ACCESS_FORMAT " %d\n",
        BASETEN_FUNCTION_NAME,
        BASETEN_DEFAULT_TIMEOUT,
        BASETEN_FUNCTION_DESCRIPTION,
        (HTTP_ACCESS_FORMAT_CAST)(HTTP_ACCESS_SIGNED_ID | HTTP_ACCESS_SAME_SPACE),
        RRDFUNCTIONS_PRIORITY_DEFAULT);

    fflush(stdout);
    netdata_mutex_unlock(&stdout_mutex);

    collector_info("BASETEN: Plugin registered with Netdata - function is now available");

    // NOW fetch initial data (after registration, so Netdata knows we're alive)
    // This prevents timeout during slow initial fetch of 70+ models
    collector_info("BASETEN: Performing initial data fetch in background...");
    if (baseten_fetch_all_data() != 0) {
        collector_error("BASETEN: Initial data fetch failed. Will retry in main loop.");
        // Don't exit - let the plugin continue and retry in the main loop
    } else {
        collector_info("BASETEN: Successfully fetched initial data");
    }

    collector_info("BASETEN: Plugin initialized successfully - entering main loop");

    // Main loop - send heartbeat and refresh data periodically
    usec_t send_newline_ut = 0;
    usec_t refresh_data_ut = 0;
    const bool tty = isatty(fileno(stdout)) == 1;

    heartbeat_t hb;
    heartbeat_init(&hb, USEC_PER_SEC);

    while (!plugin_should_exit) {
        usec_t dt_ut = heartbeat_next(&hb);
        send_newline_ut += dt_ut;
        refresh_data_ut += dt_ut;

        // Send newline heartbeat
        if (!tty && send_newline_ut > USEC_PER_SEC) {
            send_newline_and_flush(&stdout_mutex);
            send_newline_ut = 0;
        }

        // Refresh data periodically
        if (refresh_data_ut > (config.update_every * USEC_PER_SEC)) {
            collector_info("BASETEN: Periodic data refresh triggered (interval: %d seconds)", config.update_every);
            if (baseten_fetch_all_data() != 0) {
                collector_error("BASETEN: Periodic data refresh failed");
            } else {
                collector_info("BASETEN: Periodic data refresh completed successfully");
            }
            refresh_data_ut = 0;
        }
    }

    collector_info("BASETEN: Plugin shutting down...");
    cleanup();
    collector_info("BASETEN: Plugin shutdown complete");
    exit(0);
}
