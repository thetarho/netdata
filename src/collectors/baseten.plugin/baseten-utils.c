// SPDX-License-Identifier: GPL-3.0-or-later

#include "baseten-internals.h"

// Config file instance for this plugin
struct config netdata_config = APPCONFIG_INITIALIZER;

void baseten_free_models(struct baseten_model *models) {
    while (models) {
        struct baseten_model *next = models->next;

        freez(models->id);
        freez(models->name);
        freez(models->instance_type_name);
        freez(models->production_deployment_id);
        freez(models->development_deployment_id);
        freez(models);

        models = next;
    }
}

void baseten_free_deployments(struct baseten_deployment *deployments) {
    while (deployments) {
        struct baseten_deployment *next = deployments->next;

        freez(deployments->id);
        freez(deployments->name);
        freez(deployments->model_id);
        freez(deployments->environment);
        freez(deployments);

        deployments = next;
    }
}

deployment_status_t baseten_string_to_status(const char *status_str) {
    if (!status_str)
        return STATUS_UNKNOWN;

    if (strcmp(status_str, "ACTIVE") == 0)
        return STATUS_ACTIVE;
    if (strcmp(status_str, "SCALED_TO_ZERO") == 0)
        return STATUS_SCALED_TO_ZERO;
    if (strcmp(status_str, "INACTIVE") == 0)
        return STATUS_INACTIVE;
    if (strcmp(status_str, "DEPLOYING") == 0)
        return STATUS_DEPLOYING;
    if (strcmp(status_str, "FAILED") == 0)
        return STATUS_FAILED;

    return STATUS_UNKNOWN;
}

const char* baseten_status_to_string(deployment_status_t status) {
    switch (status) {
        case STATUS_ACTIVE:
            return "Active";
        case STATUS_SCALED_TO_ZERO:
            return "Scaled to Zero";
        case STATUS_INACTIVE:
            return "Inactive";
        case STATUS_DEPLOYING:
            return "Deploying";
        case STATUS_FAILED:
            return "Failed";
        default:
            return "Unknown";
    }
}

deployment_severity_t baseten_get_severity(deployment_status_t status) {
    switch (status) {
        case STATUS_ACTIVE:
            return DEPLOYMENT_SEVERITY_NORMAL;  // Green/normal - healthy
        case STATUS_DEPLOYING:
            return DEPLOYMENT_SEVERITY_NOTICE;  // Blue - in progress
        case STATUS_SCALED_TO_ZERO:
            return DEPLOYMENT_SEVERITY_WARNING; // Yellow/orange - attention needed
        case STATUS_INACTIVE:
            return DEPLOYMENT_SEVERITY_WARNING; // Yellow/orange - not running
        case STATUS_FAILED:
            return DEPLOYMENT_SEVERITY_ERROR;   // Red - critical issue
        default:
            return DEPLOYMENT_SEVERITY_NORMAL;  // Unknown status
    }
}

const char* baseten_severity_to_string(deployment_severity_t severity) {
    switch (severity) {
        case DEPLOYMENT_SEVERITY_NORMAL:
            return "normal";
        case DEPLOYMENT_SEVERITY_WARNING:
            return "warning";
        case DEPLOYMENT_SEVERITY_ERROR:
            return "error";
        case DEPLOYMENT_SEVERITY_NOTICE:
            return "notice";
        default:
            return "normal";
    }
}

int baseten_load_config(void) {
    // Get config directory from environment (passed by Netdata)
    char *user_config_dir = getenv("NETDATA_USER_CONFIG_DIR");
    if (!user_config_dir)
        user_config_dir = CONFIG_DIR;

    char *stock_config_dir = getenv("NETDATA_STOCK_CONFIG_DIR");
    if (!stock_config_dir)
        stock_config_dir = LIBCONFIG_DIR;

    char filename[FILENAME_MAX + 1];
    snprintfz(filename, FILENAME_MAX, "%s/%s", user_config_dir, CONFIG_FILENAME);

    collector_info("BASETEN: Loading configuration from %s", filename);

    // Load the config file (overwrite_used=0, section_name=NULL)
    inicfg_load(&netdata_config, filename, 0, NULL);

    // Read configuration values
    // API key - check environment variable first (Docker-friendly), then config file
    const char *env_api_key = getenv("NETDATA_BASETEN_API_KEY");
    if (env_api_key && strlen(env_api_key) > 0) {
        config.api_key = strdupz(env_api_key);
        collector_info("BASETEN: Using API key from environment variable NETDATA_BASETEN_API_KEY");
    } else {
        const char *api_key_from_conf = inicfg_get(&netdata_config, CONFIG_SECTION_BASETEN, CONFIG_KEY_API_KEY, "");
        if (api_key_from_conf && strlen(api_key_from_conf) > 0) {
            config.api_key = strdupz(api_key_from_conf);
            collector_info("BASETEN: Using API key from %s", filename);
        } else {
            config.api_key = NULL;
        }
    }

    if (!config.api_key || strlen(config.api_key) == 0) {
        collector_error("BASETEN: API key not configured. Please set '%s' in section '%s' in %s or environment variable NETDATA_BASETEN_API_KEY",
                       CONFIG_KEY_API_KEY, CONFIG_SECTION_BASETEN, filename);
        return -1;
    }

    // Update every - with default
    config.update_every = (int)inicfg_get_number(&netdata_config, CONFIG_SECTION_BASETEN,
                                                  CONFIG_KEY_UPDATE_EVERY, BASETEN_UPDATE_EVERY);
    if (config.update_every <= 0)
        config.update_every = BASETEN_UPDATE_EVERY;

    // Timeout - with default
    config.timeout = (int)inicfg_get_number(&netdata_config, CONFIG_SECTION_BASETEN,
                                            CONFIG_KEY_TIMEOUT, BASETEN_DEFAULT_TIMEOUT);
    if (config.timeout <= 0)
        config.timeout = BASETEN_DEFAULT_TIMEOUT;

    collector_info("BASETEN: Configuration loaded - update_every=%d, timeout=%d",
                   config.update_every, config.timeout);

    return 0;
}
