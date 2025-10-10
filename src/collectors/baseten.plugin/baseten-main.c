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

    netdata_configured_host_prefix = getenv("NETDATA_HOST_PREFIX");
    if (verify_netdata_host_prefix(true) == -1)
        exit(1);

    // Load configuration
    if (baseten_load_config() != 0) {
        collector_error("BASETEN: Failed to load configuration. Exiting...");
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

    // Initial data fetch to verify API connectivity
    collector_info("BASETEN: Performing initial data fetch...");
    if (baseten_fetch_all_data() != 0) {
        collector_error("BASETEN: Initial data fetch failed. Check API key and connectivity.");
        fprintf(stdout, "DISABLE\n");
        fflush(stdout);
        cleanup();
        exit(1);
    }

    collector_info("BASETEN: Successfully fetched initial data");

    // Initialize functions event loop
    struct functions_evloop_globals *wg =
        functions_evloop_init(BASETEN_WORKER_THREADS, "BASETEN", &stdout_mutex, &plugin_should_exit);

    functions_evloop_add_function(
        wg, BASETEN_FUNCTION_NAME, baseten_function_deployments, BASETEN_DEFAULT_TIMEOUT, NULL);

    // Register function with netdata
    netdata_mutex_lock(&stdout_mutex);

    fprintf(
        stdout,
        PLUGINSD_KEYWORD_FUNCTION " GLOBAL \"%s\" %d \"%s\" \"cloud\" " HTTP_ACCESS_FORMAT " %d\n",
        BASETEN_FUNCTION_NAME,
        BASETEN_DEFAULT_TIMEOUT,
        BASETEN_FUNCTION_DESCRIPTION,
        (HTTP_ACCESS_FORMAT_CAST)(HTTP_ACCESS_SIGNED_ID | HTTP_ACCESS_SAME_SPACE),
        RRDFUNCTIONS_PRIORITY_DEFAULT);

    fflush(stdout);
    netdata_mutex_unlock(&stdout_mutex);

    collector_info("BASETEN: Plugin initialized successfully");

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
            collector_info("BASETEN: Refreshing data from API...");
            if (baseten_fetch_all_data() != 0) {
                collector_error("BASETEN: Failed to refresh data");
            }
            refresh_data_ut = 0;
        }
    }

    cleanup();
    exit(0);
}
