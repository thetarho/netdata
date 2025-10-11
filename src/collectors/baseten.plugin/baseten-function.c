// SPDX-License-Identifier: GPL-3.0-or-later

#include "baseten-internals.h"

extern const char* baseten_severity_to_string(deployment_severity_t severity);

void baseten_function_deployments(const char *transaction,
                                   char *function,
                                   usec_t *stop_monotonic_ut __maybe_unused,
                                   bool *cancelled __maybe_unused,
                                   BUFFER *payload __maybe_unused,
                                   HTTP_ACCESS access __maybe_unused,
                                   const char *source __maybe_unused,
                                   void *data __maybe_unused)
{
    // Parse function parameters properly (following apps.plugin pattern)
    char *words[PLUGINSD_MAX_WORDS] = { NULL };
    size_t num_words = quoted_strings_splitter_whitespace(function, words, PLUGINSD_MAX_WORDS);
    bool info = false;

    // Parse parameters to detect "info" request
    for(int i = 1; i < PLUGINSD_MAX_WORDS; i++) {
        const char *keyword = get_word(words, num_words, i);
        if(!keyword) break;

        if(strcmp(keyword, "info") == 0) {
            info = true;
            break;
        }
    }

    collector_info("BASETEN: Function called (transaction: %s, info: %s)",
                   transaction, info ? "yes" : "no");

    // ALWAYS create a new buffer for response (following apps.plugin pattern)
    BUFFER *wb = buffer_create(4096, NULL);
    buffer_json_initialize(wb, "\"", "\"", 0, true, BUFFER_JSON_OPTIONS_DEFAULT);

    time_t now = now_realtime_sec();

    // Add common response fields
    buffer_json_member_add_uint64(wb, "status", HTTP_RESP_OK);
    buffer_json_member_add_string(wb, "type", "table");
    buffer_json_member_add_boolean(wb, "has_history", false);
    buffer_json_member_add_string(wb, "help", BASETEN_FUNCTION_DESCRIPTION);
    buffer_json_member_add_time_t(wb, "update_every", config.update_every);

    if(info) {
        // Info request - add accepted parameters and return
        collector_info("BASETEN: Returning function metadata (info request)");

        buffer_json_member_add_array(wb, "accepted_params");
        buffer_json_add_array_item_string(wb, "info");
        buffer_json_array_close(wb);

        goto close_and_send;
    }

    // Data request - fetch and return deployment data
    collector_info("BASETEN: Fetching deployment data from API...");

    struct baseten_model *models = NULL;
    struct baseten_deployment *all_deployments = NULL;

    if (baseten_fetch_models(&models) != 0) {
        collector_error("BASETEN: Failed to fetch models for function call");
        buffer_json_member_add_string(wb, "error", "Failed to fetch models from Baseten API");
        wb->response_code = HTTP_RESP_INTERNAL_SERVER_ERROR;
        goto close_and_send;
    }

    // Fetch deployments for each model
    struct baseten_model *model = models;
    int total_deployments = 0;

    while (model) {
        struct baseten_deployment *deployments = NULL;

        if (baseten_fetch_deployments(model->id, &deployments) == 0) {
            // Link deployments to model
            struct baseten_deployment *d = deployments;
            while (d) {
                d->model = model;
                total_deployments++;
                d = d->next;
            }

            // Append to all_deployments list
            if (deployments) {
                struct baseten_deployment *last = deployments;
                while (last->next) last = last->next;
                last->next = all_deployments;
                all_deployments = deployments;
            }
        }

        model = model->next;
    }

    collector_info("BASETEN: Building response table with %d deployments", total_deployments);

    // Start data array
    buffer_json_member_add_array(wb, "data");

    struct baseten_deployment *deployment = all_deployments;
    while (deployment) {
        // Each row is an array
        buffer_json_add_array_item_array(wb);

        // model_name
        buffer_json_add_array_item_string(wb, deployment->model ? deployment->model->name : "Unknown");

        // model_id
        buffer_json_add_array_item_string(wb, deployment->model_id);

        // deployment_name
        buffer_json_add_array_item_string(wb, deployment->name);

        // instance_type_name
        buffer_json_add_array_item_string(wb,
            deployment->model && deployment->model->instance_type_name ?
            deployment->model->instance_type_name : "Unknown");

        // environment (pill visualization)
        buffer_json_add_array_item_string(wb, deployment->environment ? deployment->environment : "none");

        // status (pill visualization)
        buffer_json_add_array_item_string(wb, baseten_status_to_string(deployment->status));

        // is_production
        buffer_json_add_array_item_string(wb, deployment->is_production ? "Yes" : "No");

        // is_development
        buffer_json_add_array_item_string(wb, deployment->is_development ? "Yes" : "No");

        // active_replicas
        buffer_json_add_array_item_uint64(wb, deployment->active_replica_count);

        // rowOptions for severity-based coloring
        buffer_json_add_array_item_object(wb);
        {
            deployment_severity_t severity = baseten_get_severity(deployment->status);
            buffer_json_member_add_string(wb, "severity", baseten_severity_to_string(severity));
        }
        buffer_json_object_close(wb); // rowOptions

        buffer_json_array_close(wb); // row

        deployment = deployment->next;
    }

    buffer_json_array_close(wb); // data

    // Define columns
    buffer_json_member_add_object(wb, "columns");
    {
        size_t field_id = 0;

        // model_name - searchable, sticky, unique key
        buffer_rrdf_table_add_field(
            wb, field_id++, "model_name", "Model Name",
            RRDF_FIELD_TYPE_STRING, RRDF_FIELD_VISUAL_VALUE, RRDF_FIELD_TRANSFORM_NONE,
            0, NULL, NAN, RRDF_FIELD_SORT_ASCENDING, NULL,
            RRDF_FIELD_SUMMARY_COUNT, RRDF_FIELD_FILTER_MULTISELECT,
            RRDF_FIELD_OPTS_VISIBLE | RRDF_FIELD_OPTS_STICKY,
            NULL);

        // model_id - searchable
        buffer_rrdf_table_add_field(
            wb, field_id++, "model_id", "Model ID",
            RRDF_FIELD_TYPE_STRING, RRDF_FIELD_VISUAL_VALUE, RRDF_FIELD_TRANSFORM_NONE,
            0, NULL, NAN, RRDF_FIELD_SORT_ASCENDING, NULL,
            RRDF_FIELD_SUMMARY_COUNT, RRDF_FIELD_FILTER_MULTISELECT,
            RRDF_FIELD_OPTS_VISIBLE | RRDF_FIELD_OPTS_UNIQUE_KEY,
            NULL);

        // deployment_name - searchable
        buffer_rrdf_table_add_field(
            wb, field_id++, "deployment_name", "Deployment Name",
            RRDF_FIELD_TYPE_STRING, RRDF_FIELD_VISUAL_VALUE, RRDF_FIELD_TRANSFORM_NONE,
            0, NULL, NAN, RRDF_FIELD_SORT_ASCENDING, NULL,
            RRDF_FIELD_SUMMARY_COUNT, RRDF_FIELD_FILTER_MULTISELECT,
            RRDF_FIELD_OPTS_VISIBLE,
            NULL);

        // instance_type_name - monospace display
        buffer_rrdf_table_add_field(
            wb, field_id++, "instance_type_name", "Instance Type",
            RRDF_FIELD_TYPE_STRING, RRDF_FIELD_VISUAL_VALUE, RRDF_FIELD_TRANSFORM_NONE,
            0, NULL, NAN, RRDF_FIELD_SORT_ASCENDING, NULL,
            RRDF_FIELD_SUMMARY_COUNT, RRDF_FIELD_FILTER_MULTISELECT,
            RRDF_FIELD_OPTS_VISIBLE | RRDF_FIELD_OPTS_FULL_WIDTH,
            NULL);

        // environment - pill, filterable
        buffer_rrdf_table_add_field(
            wb, field_id++, "environment", "Environment",
            RRDF_FIELD_TYPE_STRING, RRDF_FIELD_VISUAL_PILL, RRDF_FIELD_TRANSFORM_NONE,
            0, NULL, NAN, RRDF_FIELD_SORT_ASCENDING, NULL,
            RRDF_FIELD_SUMMARY_COUNT, RRDF_FIELD_FILTER_MULTISELECT,
            RRDF_FIELD_OPTS_VISIBLE,
            NULL);

        // status - pill, filterable
        buffer_rrdf_table_add_field(
            wb, field_id++, "status", "Status",
            RRDF_FIELD_TYPE_STRING, RRDF_FIELD_VISUAL_PILL, RRDF_FIELD_TRANSFORM_NONE,
            0, NULL, NAN, RRDF_FIELD_SORT_ASCENDING, NULL,
            RRDF_FIELD_SUMMARY_COUNT, RRDF_FIELD_FILTER_MULTISELECT,
            RRDF_FIELD_OPTS_VISIBLE,
            NULL);

        // is_production
        buffer_rrdf_table_add_field(
            wb, field_id++, "is_production", "Production",
            RRDF_FIELD_TYPE_STRING, RRDF_FIELD_VISUAL_VALUE, RRDF_FIELD_TRANSFORM_NONE,
            0, NULL, NAN, RRDF_FIELD_SORT_ASCENDING, NULL,
            RRDF_FIELD_SUMMARY_COUNT, RRDF_FIELD_FILTER_MULTISELECT,
            RRDF_FIELD_OPTS_VISIBLE,
            NULL);

        // is_development
        buffer_rrdf_table_add_field(
            wb, field_id++, "is_development", "Development",
            RRDF_FIELD_TYPE_STRING, RRDF_FIELD_VISUAL_VALUE, RRDF_FIELD_TRANSFORM_NONE,
            0, NULL, NAN, RRDF_FIELD_SORT_ASCENDING, NULL,
            RRDF_FIELD_SUMMARY_COUNT, RRDF_FIELD_FILTER_MULTISELECT,
            RRDF_FIELD_OPTS_VISIBLE,
            NULL);

        // active_replicas - sortable integer
        buffer_rrdf_table_add_field(
            wb, field_id++, "active_replicas", "Active Replicas",
            RRDF_FIELD_TYPE_INTEGER, RRDF_FIELD_VISUAL_VALUE, RRDF_FIELD_TRANSFORM_NUMBER,
            0, "replicas", NAN, RRDF_FIELD_SORT_DESCENDING, NULL,
            RRDF_FIELD_SUMMARY_SUM, RRDF_FIELD_FILTER_RANGE,
            RRDF_FIELD_OPTS_VISIBLE,
            NULL);

        // rowOptions dummy column for row severity
        buffer_rrdf_table_add_field(
            wb, field_id++, "rowOptions", "rowOptions",
            RRDF_FIELD_TYPE_NONE, RRDR_FIELD_VISUAL_ROW_OPTIONS, RRDF_FIELD_TRANSFORM_NONE,
            0, NULL, NAN, RRDF_FIELD_SORT_ASCENDING, NULL,
            RRDF_FIELD_SUMMARY_COUNT, RRDF_FIELD_FILTER_NONE,
            RRDF_FIELD_OPTS_DUMMY,
            NULL);
    }
    buffer_json_object_close(wb); // columns

    // Set default sort column
    buffer_json_member_add_string(wb, "default_sort_column", "model_name");

    collector_info("BASETEN: Response prepared with %d deployments", total_deployments);

    // Free allocated memory
    baseten_free_models(models);
    baseten_free_deployments(all_deployments);

close_and_send:
    // Finalize and send response (following apps.plugin pattern)
    buffer_json_finalize(wb);

    // Set buffer properties
    if (wb->response_code == 0)
        wb->response_code = HTTP_RESP_OK;
    wb->content_type = CT_APPLICATION_JSON;
    wb->expires = now + config.update_every;

    // Send response using standard API
    pluginsd_function_result_to_stdout(transaction, wb);

    // Free buffer
    buffer_free(wb);

    collector_info("BASETEN: Response sent successfully");
}
