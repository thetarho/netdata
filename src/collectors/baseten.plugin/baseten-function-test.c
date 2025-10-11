// SPDX-License-Identifier: GPL-3.0-or-later
// MINIMAL TEST VERSION - Just return empty table

#include "baseten-internals.h"

void baseten_function_deployments(const char *transaction,
                                   char *function __maybe_unused,
                                   usec_t *stop_monotonic_ut __maybe_unused,
                                   bool *cancelled __maybe_unused,
                                   BUFFER *payload,
                                   HTTP_ACCESS access __maybe_unused,
                                   const char *source __maybe_unused,
                                   void *data __maybe_unused)
{
    collector_info("BASETEN: TEST - Function called");

    BUFFER *wb = payload;
    time_t now = now_realtime_sec();

    collector_info("BASETEN: TEST - Flushing buffer");
    buffer_flush(wb);

    collector_info("BASETEN: TEST - Setting buffer properties");
    wb->content_type = CT_APPLICATION_JSON;
    wb->expires = now + 120;

    collector_info("BASETEN: TEST - Initializing JSON");
    buffer_json_initialize(wb, "\"", "\"", 0, true, BUFFER_JSON_OPTIONS_DEFAULT);

    collector_info("BASETEN: TEST - Adding status");
    buffer_json_member_add_uint64(wb, "status", HTTP_RESP_OK);

    collector_info("BASETEN: TEST - Adding type");
    buffer_json_member_add_string(wb, "type", "table");

    collector_info("BASETEN: TEST - Adding has_history");
    buffer_json_member_add_boolean(wb, "has_history", false);

    collector_info("BASETEN: TEST - Adding help");
    buffer_json_member_add_string(wb, "help", "Test function");

    collector_info("BASETEN: TEST - Adding update_every");
    buffer_json_member_add_time_t(wb, "update_every", 120);

    collector_info("BASETEN: TEST - Adding empty data array");
    buffer_json_member_add_array(wb, "data");
    buffer_json_array_close(wb);

    collector_info("BASETEN: TEST - Adding columns");
    buffer_json_member_add_object(wb, "columns");
    buffer_json_object_close(wb);

    collector_info("BASETEN: TEST - Finalizing JSON");
    buffer_json_finalize(wb);

    collector_info("BASETEN: TEST - Setting response code");
    wb->response_code = HTTP_RESP_OK;

    collector_info("BASETEN: TEST - Sending response");
    netdata_mutex_lock(&stdout_mutex);
    pluginsd_function_result_to_stdout(transaction, wb);
    netdata_mutex_unlock(&stdout_mutex);

    collector_info("BASETEN: TEST - Done");
}
