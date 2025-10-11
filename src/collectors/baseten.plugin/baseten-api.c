// SPDX-License-Identifier: GPL-3.0-or-later

#include "baseten-internals.h"
#include <json-c/json.h>

static CURL *curl_handle = NULL;

// CURL write callback
static size_t write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct memory_chunk *mem = (struct memory_chunk *)userp;

    char *ptr = reallocz(mem->memory, mem->size + realsize + 1);
    mem->memory = ptr;

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

int baseten_api_init(void)
{
    collector_info("BASETEN: Initializing API client...");

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    if (!curl_handle) {
        collector_error("BASETEN: Failed to initialize CURL");
        return -1;
    }

    collector_info("BASETEN: API client initialized successfully");
    return 0;
}

void baseten_api_cleanup(void)
{
    collector_info("BASETEN: Cleaning up API client...");

    if (curl_handle) {
        curl_easy_cleanup(curl_handle);
        curl_handle = NULL;
    }
    curl_global_cleanup();

    collector_info("BASETEN: API client cleanup complete");
}

static int baseten_api_request(const char *endpoint, struct memory_chunk *response)
{
    CURLcode res;
    long response_code = 0;
    char url[1024];
    struct curl_slist *headers = NULL;
    char auth_header[512];

    if (!curl_handle) {
        collector_error("BASETEN: CURL not initialized");
        return -1;
    }

    // Build URL
    snprintfz(url, sizeof(url), "%s%s", BASETEN_API_BASE_URL, endpoint);

    char api_key_preview[16] = {0};
    if (config.api_key && strlen(config.api_key) >= 8) {
        strncpy(api_key_preview, config.api_key, 8);
        strcat(api_key_preview, "...");
    }
    collector_info("BASETEN: Making API request to %s (API key: %s)", endpoint, api_key_preview);

    snprintfz(auth_header, sizeof(auth_header), "Authorization: Api-Key %s", config.api_key);
    headers = curl_slist_append(headers, auth_header);
    headers = curl_slist_append(headers, "Accept: application/json");

    // Initialize response buffer
    response->memory = mallocz(1);
    response->size = 0;

    // Configure CURL
    curl_easy_reset(curl_handle);
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)response);
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, (long)config.timeout);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYHOST, 2L);

    // Perform request
    res = curl_easy_perform(curl_handle);

    if (res != CURLE_OK) {
        collector_error("BASETEN: CURL request to %s failed: %s", endpoint, curl_easy_strerror(res));
        curl_slist_free_all(headers);
        freez(response->memory);
        response->memory = NULL;
        return -1;
    }

    curl_easy_getinfo(curl_handle, CURLINFO_RESPONSE_CODE, &response_code);
    curl_slist_free_all(headers);

    if (response_code != 200) {
        // Log error with response body (limited to first 500 chars for debugging)
        char error_preview[512] = {0};
        if (response->memory && response->size > 0) {
            size_t preview_len = response->size < 500 ? response->size : 500;
            strncpy(error_preview, response->memory, preview_len);
            collector_error("BASETEN: API endpoint %s returned HTTP %ld - Response: %s",
                          endpoint, response_code, error_preview);
        } else {
            collector_error("BASETEN: API endpoint %s returned HTTP %ld (no response body)",
                          endpoint, response_code);
        }
        freez(response->memory);
        response->memory = NULL;
        return -1;
    }

    collector_info("BASETEN: Successfully fetched data from %s (response size: %zu bytes)", endpoint, response->size);
    return 0;
}

int baseten_fetch_models(struct baseten_model **models)
{
    struct memory_chunk response;
    struct baseten_model *model_list = NULL;
    struct json_object *root, *models_array, *model_obj;
    int i, array_len;

    collector_info("BASETEN: Fetching models from API...");

    if (baseten_api_request(BASETEN_MODELS_ENDPOINT, &response) != 0) {
        collector_error("BASETEN: Failed to fetch models from API");
        return -1;
    }

    // Parse JSON
    root = json_tokener_parse(response.memory);
    freez(response.memory);

    if (!root) {
        collector_error("BASETEN: Failed to parse models JSON response");
        return -1;
    }

    if (!json_object_object_get_ex(root, "models", &models_array)) {
        collector_error("BASETEN: No 'models' array found in API response");
        json_object_put(root);
        return -1;
    }

    array_len = json_object_array_length(models_array);
    collector_info("BASETEN: Found %d models in API response", array_len);

    for (i = 0; i < array_len; i++) {
        model_obj = json_object_array_get_idx(models_array, i);

        struct baseten_model *model = callocz(1, sizeof(struct baseten_model));

        struct json_object *tmp;
        if (json_object_object_get_ex(model_obj, "id", &tmp))
            model->id = strdupz(json_object_get_string(tmp));

        if (json_object_object_get_ex(model_obj, "name", &tmp))
            model->name = strdupz(json_object_get_string(tmp));

        if (json_object_object_get_ex(model_obj, "instance_type_name", &tmp))
            model->instance_type_name = strdupz(json_object_get_string(tmp));

        if (json_object_object_get_ex(model_obj, "production_deployment_id", &tmp) &&
            !json_object_is_type(tmp, json_type_null))
            model->production_deployment_id = strdupz(json_object_get_string(tmp));

        if (json_object_object_get_ex(model_obj, "development_deployment_id", &tmp) &&
            !json_object_is_type(tmp, json_type_null))
            model->development_deployment_id = strdupz(json_object_get_string(tmp));

        if (json_object_object_get_ex(model_obj, "deployments_count", &tmp))
            model->deployments_count = json_object_get_int(tmp);

        // Add to linked list
        model->next = model_list;
        model_list = model;
    }

    json_object_put(root);
    *models = model_list;

    collector_info("BASETEN: Successfully parsed %d models", array_len);
    return 0;
}

int baseten_fetch_deployments(const char *model_id, struct baseten_deployment **deployments)
{
    struct memory_chunk response;
    struct baseten_deployment *deployment_list = NULL;
    struct json_object *root, *deployments_array, *deployment_obj;
    char endpoint[512];
    int i, array_len;

    collector_info("BASETEN: Fetching deployments for model %s...", model_id);

    snprintfz(endpoint, sizeof(endpoint), BASETEN_DEPLOYMENTS_ENDPOINT, model_id);

    if (baseten_api_request(endpoint, &response) != 0) {
        collector_error("BASETEN: Failed to fetch deployments for model %s", model_id);
        return -1;
    }

    // Parse JSON
    root = json_tokener_parse(response.memory);
    freez(response.memory);

    if (!root) {
        collector_error("BASETEN: Failed to parse deployments JSON for model %s", model_id);
        return -1;
    }

    if (!json_object_object_get_ex(root, "deployments", &deployments_array)) {
        collector_error("BASETEN: No 'deployments' array in response for model %s", model_id);
        json_object_put(root);
        return -1;
    }

    array_len = json_object_array_length(deployments_array);
    collector_info("BASETEN: Found %d deployments for model %s", array_len, model_id);

    for (i = 0; i < array_len; i++) {
        deployment_obj = json_object_array_get_idx(deployments_array, i);

        struct baseten_deployment *deployment = callocz(1, sizeof(struct baseten_deployment));

        struct json_object *tmp;
        if (json_object_object_get_ex(deployment_obj, "id", &tmp))
            deployment->id = strdupz(json_object_get_string(tmp));

        if (json_object_object_get_ex(deployment_obj, "name", &tmp))
            deployment->name = strdupz(json_object_get_string(tmp));

        if (json_object_object_get_ex(deployment_obj, "model_id", &tmp))
            deployment->model_id = strdupz(json_object_get_string(tmp));

        if (json_object_object_get_ex(deployment_obj, "is_production", &tmp))
            deployment->is_production = json_object_get_boolean(tmp);

        if (json_object_object_get_ex(deployment_obj, "is_development", &tmp))
            deployment->is_development = json_object_get_boolean(tmp);

        if (json_object_object_get_ex(deployment_obj, "active_replica_count", &tmp))
            deployment->active_replica_count = json_object_get_int(tmp);

        if (json_object_object_get_ex(deployment_obj, "status", &tmp))
            deployment->status = baseten_string_to_status(json_object_get_string(tmp));

        if (json_object_object_get_ex(deployment_obj, "environment", &tmp) && !json_object_is_type(tmp, json_type_null))
            deployment->environment = strdupz(json_object_get_string(tmp));

        // Add to linked list
        deployment->next = deployment_list;
        deployment_list = deployment;
    }

    json_object_put(root);
    *deployments = deployment_list;

    collector_info("BASETEN: Successfully parsed %d deployments for model %s", array_len, model_id);
    return 0;
}

// Structure to track parallel requests
struct parallel_request {
    CURL *handle;
    char *model_id;
    struct baseten_model *model;
    struct memory_chunk response;
    char url[1024];
    char auth_header[512];
    struct curl_slist *headers;
};

// Fetch all deployments in parallel using CURL multi interface
int baseten_fetch_all_deployments_parallel(struct baseten_model *models, struct baseten_deployment **all_deployments, int *total_count) {
    CURLM *multi_handle;
    int still_running = 0;
    int num_models = 0;
    struct parallel_request *requests = NULL;
    struct baseten_deployment *deployment_list = NULL;
    int total = 0;

    collector_info("BASETEN: Starting parallel deployment fetch...");

    // Count models
    struct baseten_model *m = models;
    while (m) {
        num_models++;
        m = m->next;
    }

    if (num_models == 0) {
        collector_info("BASETEN: No models to fetch deployments for");
        *all_deployments = NULL;
        *total_count = 0;
        return 0;
    }

    collector_info("BASETEN: Fetching deployments for %d models in parallel", num_models);

    // Allocate request structures
    requests = callocz(num_models, sizeof(struct parallel_request));

    // Initialize multi handle
    multi_handle = curl_multi_init();
    if (!multi_handle) {
        collector_error("BASETEN: Failed to initialize CURL multi handle");
        freez(requests);
        return -1;
    }

    // Set up parallel requests
    int idx = 0;
    m = models;
    while (m) {
        struct parallel_request *req = &requests[idx];

        req->model = m;
        req->model_id = m->id;
        req->handle = curl_easy_init();

        if (!req->handle) {
            collector_error("BASETEN: Failed to create CURL handle for model %s", m->id);
            idx++;
            m = m->next;
            continue;
        }

        // Build URL
        snprintfz(req->url, sizeof(req->url), "%s/models/%s/deployments", BASETEN_API_BASE_URL, m->id);

        // Set up auth header
        snprintfz(req->auth_header, sizeof(req->auth_header), "Authorization: Api-Key %s", config.api_key);
        req->headers = curl_slist_append(NULL, req->auth_header);
        req->headers = curl_slist_append(req->headers, "Accept: application/json");

        // Initialize response buffer
        req->response.memory = mallocz(1);
        req->response.size = 0;

        // Configure CURL handle
        curl_easy_setopt(req->handle, CURLOPT_URL, req->url);
        curl_easy_setopt(req->handle, CURLOPT_HTTPHEADER, req->headers);
        curl_easy_setopt(req->handle, CURLOPT_WRITEFUNCTION, write_memory_callback);
        curl_easy_setopt(req->handle, CURLOPT_WRITEDATA, (void *)&req->response);
        curl_easy_setopt(req->handle, CURLOPT_TIMEOUT, (long)config.timeout);
        curl_easy_setopt(req->handle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(req->handle, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(req->handle, CURLOPT_SSL_VERIFYHOST, 2L);

        // Add to multi handle
        curl_multi_add_handle(multi_handle, req->handle);

        idx++;
        m = m->next;
    }

    // Perform parallel requests
    curl_multi_perform(multi_handle, &still_running);

    while (still_running) {
        int numfds = 0;
        CURLMcode mc = curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);

        if (mc != CURLM_OK) {
            collector_error("BASETEN: curl_multi_wait() failed: %s", curl_multi_strerror(mc));
            break;
        }

        curl_multi_perform(multi_handle, &still_running);
    }

    collector_info("BASETEN: All parallel requests completed, processing responses...");

    // Process responses
    for (int i = 0; i < num_models; i++) {
        struct parallel_request *req = &requests[i];

        if (!req->handle) continue;

        long response_code = 0;
        curl_easy_getinfo(req->handle, CURLINFO_RESPONSE_CODE, &response_code);

        if (response_code == 200 && req->response.memory && req->response.size > 0) {
            // Parse JSON response
            struct json_object *root = json_tokener_parse(req->response.memory);

            if (root) {
                struct json_object *deployments_array;
                if (json_object_object_get_ex(root, "deployments", &deployments_array)) {
                    int array_len = json_object_array_length(deployments_array);

                    collector_info("BASETEN: Model %s: %d deployments", req->model_id, array_len);

                    for (int j = 0; j < array_len; j++) {
                        struct json_object *deployment_obj = json_object_array_get_idx(deployments_array, j);
                        struct baseten_deployment *deployment = callocz(1, sizeof(struct baseten_deployment));

                        struct json_object *tmp;
                        if (json_object_object_get_ex(deployment_obj, "id", &tmp))
                            deployment->id = strdupz(json_object_get_string(tmp));

                        if (json_object_object_get_ex(deployment_obj, "name", &tmp))
                            deployment->name = strdupz(json_object_get_string(tmp));

                        if (json_object_object_get_ex(deployment_obj, "model_id", &tmp))
                            deployment->model_id = strdupz(json_object_get_string(tmp));

                        if (json_object_object_get_ex(deployment_obj, "is_production", &tmp))
                            deployment->is_production = json_object_get_boolean(tmp);

                        if (json_object_object_get_ex(deployment_obj, "is_development", &tmp))
                            deployment->is_development = json_object_get_boolean(tmp);

                        if (json_object_object_get_ex(deployment_obj, "active_replica_count", &tmp))
                            deployment->active_replica_count = json_object_get_int(tmp);

                        if (json_object_object_get_ex(deployment_obj, "status", &tmp))
                            deployment->status = baseten_string_to_status(json_object_get_string(tmp));

                        if (json_object_object_get_ex(deployment_obj, "environment", &tmp) && !json_object_is_type(tmp, json_type_null))
                            deployment->environment = strdupz(json_object_get_string(tmp));

                        // Link to model
                        deployment->model = req->model;

                        // Add to linked list
                        deployment->next = deployment_list;
                        deployment_list = deployment;
                        total++;
                    }
                }

                json_object_put(root);
            } else {
                collector_error("BASETEN: Failed to parse JSON for model %s", req->model_id);
            }
        } else {
            collector_error("BASETEN: Model %s returned HTTP %ld or empty response", req->model_id, response_code);
        }

        // Cleanup
        if (req->response.memory)
            freez(req->response.memory);
        if (req->headers)
            curl_slist_free_all(req->headers);
        curl_multi_remove_handle(multi_handle, req->handle);
        curl_easy_cleanup(req->handle);
    }

    curl_multi_cleanup(multi_handle);
    freez(requests);

    *all_deployments = deployment_list;
    *total_count = total;

    collector_info("BASETEN: Parallel fetch complete - fetched %d total deployments", total);
    return 0;
}
