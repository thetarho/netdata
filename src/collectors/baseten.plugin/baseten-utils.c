// SPDX-License-Identifier: GPL-3.0-or-later

#include "baseten-internals.h"

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
            return "ACTIVE";
        case STATUS_SCALED_TO_ZERO:
            return "SCALED_TO_ZERO";
        case STATUS_INACTIVE:
            return "INACTIVE";
        case STATUS_DEPLOYING:
            return "DEPLOYING";
        case STATUS_FAILED:
            return "FAILED";
        default:
            return "UNKNOWN";
    }
}

deployment_severity_t baseten_get_severity(deployment_status_t status) {
    switch (status) {
        case STATUS_ACTIVE:
            return DEPLOYMENT_SEVERITY_NORMAL;
        case STATUS_SCALED_TO_ZERO:
        case STATUS_INACTIVE:
            return DEPLOYMENT_SEVERITY_WARNING;
        case STATUS_FAILED:
            return DEPLOYMENT_SEVERITY_ERROR;
        case STATUS_DEPLOYING:
            return DEPLOYMENT_SEVERITY_NOTICE;
        default:
            return DEPLOYMENT_SEVERITY_NORMAL;
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
    // First check environment variable (useful for Docker deployments)
    const char *env_api_key = getenv("NETDATA_BASETEN_API_KEY");
    if (env_api_key && strlen(env_api_key) > 0) {
        config.api_key = (char *)env_api_key;
        collector_info("BASETEN: Using API key from environment variable NETDATA_BASETEN_API_KEY");
    } else {
        // Fall back to config file
        config.api_key = appconfig_get(&netdata_config, CONFIG_SECTION_BASETEN, CONFIG_KEY_API_KEY, "");
    }

    if (!config.api_key || strlen(config.api_key) == 0) {
        collector_error("BASETEN: API key not configured. Please set '%s' in section '%s' or environment variable NETDATA_BASETEN_API_KEY",
                       CONFIG_KEY_API_KEY, CONFIG_SECTION_BASETEN);
        return -1;
    }

    config.update_every = (int)appconfig_get_number(&netdata_config, CONFIG_SECTION_BASETEN,
                                                     CONFIG_KEY_UPDATE_EVERY, BASETEN_UPDATE_EVERY);

    config.timeout = (int)appconfig_get_number(&netdata_config, CONFIG_SECTION_BASETEN,
                                               CONFIG_KEY_TIMEOUT, BASETEN_DEFAULT_TIMEOUT);

    collector_info("BASETEN: Configuration loaded - update_every=%d, timeout=%d",
                   config.update_every, config.timeout);

    return 0;
}
