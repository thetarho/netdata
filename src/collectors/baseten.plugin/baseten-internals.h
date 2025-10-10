// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BASETEN_INTERNALS_H
#define BASETEN_INTERNALS_H

#include "libnetdata/libnetdata.h"
#include "libnetdata/required_dummies.h"
#include "database/rrd.h"

#include <curl/curl.h>

// Plugin constants
#define PLUGIN_BASETEN_NAME "baseten.plugin"
#define BASETEN_FUNCTION_NAME "deployments"
#define BASETEN_FUNCTION_DESCRIPTION "View Baseten AI model deployments with status, environment, and resource information"
#define BASETEN_DEFAULT_TIMEOUT 30
#define BASETEN_UPDATE_EVERY 60

// API endpoints
#define BASETEN_API_BASE_URL "https://api.baseten.co/v1"
#define BASETEN_MODELS_ENDPOINT "/models"
#define BASETEN_DEPLOYMENTS_ENDPOINT "/models/%s/deployments"

// Configuration
#define CONFIG_SECTION_BASETEN "plugin:baseten"
#define CONFIG_KEY_API_KEY "api key"
#define CONFIG_KEY_UPDATE_EVERY "update every"
#define CONFIG_KEY_TIMEOUT "timeout"

// Cache settings
#define BASETEN_CACHE_TTL 60  // seconds

// Status severity mappings
typedef enum {
    DEPLOYMENT_SEVERITY_NORMAL,
    DEPLOYMENT_SEVERITY_WARNING,
    DEPLOYMENT_SEVERITY_ERROR,
    DEPLOYMENT_SEVERITY_NOTICE
} deployment_severity_t;

// Deployment status enum
typedef enum {
    STATUS_ACTIVE,
    STATUS_SCALED_TO_ZERO,
    STATUS_INACTIVE,
    STATUS_DEPLOYING,
    STATUS_FAILED,
    STATUS_UNKNOWN
} deployment_status_t;

// Environment enum
typedef enum {
    ENV_PRODUCTION,
    ENV_DEVELOPMENT,
    ENV_STAGING,
    ENV_NONE
} deployment_environment_t;

// Data structures
struct baseten_model {
    char *id;
    char *name;
    char *instance_type_name;
    char *production_deployment_id;
    char *development_deployment_id;
    int deployments_count;
    time_t created_at;

    struct baseten_model *next;
};

struct baseten_deployment {
    char *id;
    char *name;
    char *model_id;
    char *environment;
    deployment_status_t status;
    int is_production;
    int is_development;
    int active_replica_count;
    time_t created_at;

    // Linked to model for enrichment
    struct baseten_model *model;

    struct baseten_deployment *next;
};

struct baseten_cache {
    struct baseten_model *models;
    struct baseten_deployment *deployments;
    time_t last_update;
    netdata_mutex_t mutex;
};

struct baseten_config {
    char *api_key;
    int update_every;
    int timeout;
};

// Memory buffer for CURL responses
struct memory_chunk {
    char *memory;
    size_t size;
};

// Global state
extern struct baseten_config config;
extern struct baseten_cache cache;
extern netdata_mutex_t stdout_mutex;
extern bool plugin_should_exit;

// Function prototypes - API client
int baseten_api_init(void);
void baseten_api_cleanup(void);
int baseten_fetch_models(struct baseten_model **models);
int baseten_fetch_deployments(const char *model_id, struct baseten_deployment **deployments);
int baseten_fetch_all_data(void);

// Function prototypes - data management
void baseten_free_models(struct baseten_model *models);
void baseten_free_deployments(struct baseten_deployment *deployments);
deployment_severity_t baseten_get_severity(deployment_status_t status);
const char* baseten_status_to_string(deployment_status_t status);
deployment_status_t baseten_string_to_status(const char *status_str);

// Function prototypes - table function
void baseten_function_deployments(const char *transaction, char *function,
                                  usec_t *stop_monotonic_ut, bool *cancelled,
                                  BUFFER *payload, HTTP_ACCESS access,
                                  const char *source, void *data);

// Function prototypes - configuration
int baseten_load_config(void);

#endif // BASETEN_INTERNALS_H
