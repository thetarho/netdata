// SPDX-License-Identifier: GPL-3.0-or-later

#include "baseten-internals.h"

extern const char* baseten_severity_to_string(deployment_severity_t severity);

void baseten_function_deployments(const char *transaction,
                                   char *function __maybe_unused,
                                   usec_t *stop_monotonic_ut __maybe_unused,
                                   bool *cancelled __maybe_unused,
                                   BUFFER *payload,
                                   HTTP_ACCESS access __maybe_unused,
                                   const char *source __maybe_unused,
                                   void *data __maybe_unused)
{
    time_t now = now_realtime_sec();
    BUFFER *wb = payload;

    collector_info("BASETEN: Function 'baseten-deployments' called (transaction: %s, source: %s)",
                   transaction, source ? source : "unknown");

    buffer_flush(wb);
    wb->content_type = CT_APPLICATION_JSON;
    wb->expires = now + config.update_every;
    buffer_json_initialize(wb, "\"", "\"", 0, true, BUFFER_JSON_OPTIONS_DEFAULT);

    buffer_json_member_add_uint64(wb, "status", HTTP_RESP_OK);
    buffer_json_member_add_string(wb, "type", "table");
    buffer_json_member_add_boolean(wb, "has_history", false);
    buffer_json_member_add_string(wb, "help", BASETEN_FUNCTION_DESCRIPTION);
    buffer_json_member_add_time_t(wb, "update_every", config.update_every);

    // Fetch data on-demand
    collector_info("BASETEN: Fetching fresh data from API...");

    struct baseten_model *models = NULL;
    struct baseten_deployment *all_deployments = NULL;

    if (baseten_fetch_models(&models) != 0) {
        collector_error("BASETEN: Failed to fetch models for function call");
        buffer_json_member_add_string(wb, "error", "Failed to fetch models from Baseten API");
        buffer_json_finalize(wb);
        wb->response_code = HTTP_RESP_INTERNAL_SERVER_ERROR;

        netdata_mutex_lock(&stdout_mutex);
        pluginsd_function_result_to_stdout(transaction, wb);
        netdata_mutex_unlock(&stdout_mutex);
        return;
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
            0, NULL, NAN, RRDF_FIELD_SORT_FIXED, NULL,
            RRDF_FIELD_SUMMARY_COUNT, RRDF_FIELD_FILTER_NONE,
            RRDF_FIELD_OPTS_DUMMY,
            NULL);
    }
    buffer_json_object_close(wb); // columns

    // Set default sort column
    buffer_json_member_add_string(wb, "default_sort_column", "model_name");

    buffer_json_finalize(wb);
    wb->response_code = HTTP_RESP_OK;

    collector_info("BASETEN: Function response prepared successfully (transaction: %s, deployments: %d)",
                   transaction, total_deployments);

    // Send response
    netdata_mutex_lock(&stdout_mutex);
    pluginsd_function_result_to_stdout(transaction, wb);
    netdata_mutex_unlock(&stdout_mutex);

    collector_info("BASETEN: Function response sent (transaction: %s)", transaction);

    // Free allocated memory
    baseten_free_models(models);
    baseten_free_deployments(all_deployments);
}
