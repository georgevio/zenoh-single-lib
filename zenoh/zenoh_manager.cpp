#include "zenoh_manager.h"
#include "zenoh_handlers.h"
#include "zenoh_scout.h"
#include "zenoh_utils.h"
#include "zenoh_heartbeat.h"
#include <esp_log.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "esp_heap_caps.h"

#if ZENOH_ENABLED

static const char *TAG = "Z_MANAGER";

/**
 * @brief Free a payload buffer, choosing between malloc and heap_caps_malloc
 * based on the context pointer.
 * @param data The address of the payload buffer to free.
 * @param context NULL if the buffer was allocated with malloc, non-NULL if
 * allocated with heap_caps_malloc.
 */
static void payload_deleter(void *data, void *context) {
    ESP_LOGD("PAYLOAD_DELETER", "Freeing payload at address %p", data);
    if (context != NULL) {
        free(data); // Buffer came from malloc
    } else {
        heap_caps_free(data); // Buffer came from heap_caps_malloc
    }
}

// Module-local static variables
static z_owned_session_t session;
static TaskHandle_t zenoh_task_handle = NULL;
static EventGroupHandle_t app_event_group = NULL;

#if SUBSCRIBER_ON
static z_owned_subscriber_t main_subscriber;
#endif

#if PUBLISHER_ON
static z_owned_publisher_t main_publisher;
static bool g_publisher_declared = false;
#endif

#if QUERYABLE_ON
static z_owned_queryable_t queryable;
#endif



/**
 * @brief Helper function to declare a publisher on a given key expression.
 * This function is meant to be used by the main application to declare a
 * publisher. It takes a key expression as a string and uses it to declare a
 * publisher on the given session.
 *
 * @param s The session to use for declaring the publisher.
 * @param pub The publisher to declare.
 * @param keyexpr The key expression to use for declaring the publisher.
 * @return The result of the declaration operation.
 */
static z_result_t declare_publisher_helper(z_loaned_session_t* s, 
            z_owned_publisher_t *pub, const char *keyexpr) {
    z_view_keyexpr_t ke;
    z_view_keyexpr_from_str_unchecked(&ke, keyexpr);
    z_result_t res = z_declare_publisher(s, pub, z_loan(ke), NULL);
    if (res < 0) { ESP_LOGE(TAG, "❗Unable to declare publisher on '%s'❗", keyexpr); } 
    else { ESP_LOGI(TAG, "📡 Publisher on '%s'", keyexpr); }
    return res;
}

/**
 * @brief MAIN Task responsible for running the Zenoh client.
 * This task is responsible for setting up the Zenoh client, declaring
 * resources such as publishers and subscribers, and running the heartbeat
 * task.
 *
 * @param arg Pointer to a z_data_handler_t function that will be called
 * whenever new data is received from the network.
 *
 * @details This task is responsible for setting up the Zenoh client, declaring
 * resources such as publishers and subscribers, and running the heartbeat
 * task. It will continue running until the system is shut down or
 * restarted.
 */
static void zenoh_client_task(void *arg) {
    z_data_handler_t data_handler = (z_data_handler_t)arg;
    network_info_t net_info = active_network_interface("Z_IFACE");

    z_owned_config_t config;
    z_config_default(&config);

    if (strcmp(ZENOH_SET_MODE, "peer") == 0) {
        zp_config_insert(z_loan_mut(config), Z_CONFIG_MODE_KEY, "peer");
        zenoh_utils_set_primary_listener(ZENOH_PROTOCOL, 
                                         ZENOH_LISTEN_BROADCAST_IP, 
                                         ZENOH_PORT, 
                                         net_info.interface_name);
        zp_config_insert(z_loan_mut(config), 
                Z_CONFIG_LISTEN_KEY, zenoh_utils_get_primary_listener());
        ESP_LOGI(TAG, "🌐 PEER LISTENS on: %s", 
                    zenoh_utils_get_primary_listener());
    }
    
    if (z_open(&session, z_move(config), NULL) < 0) {
        ESP_LOGE(TAG, "❗FAILED to open Zenoh session❗");
        vTaskDelete(NULL); return;
    }
    ESP_LOGI(TAG, "✅ Zenoh Session Opened Successfully!✅");
    xEventGroupSetBits(app_event_group, ZENOH_CONNECTED_BIT);

    char session_zid_str[sizeof(z_id_t) * 2 + 1] = {0};
    z_id_t sid = z_info_zid(z_loan(session));
    format_zid(&sid, session_zid_str, sizeof(session_zid_str));
    ESP_LOGI(TAG, "My Zenoh ID is: %s", session_zid_str);
    
    zp_start_read_task(z_loan_mut(session), NULL);
    zp_start_lease_task(z_loan_mut(session), NULL);

#if SUBSCRIBER_ON
    z_owned_closure_sample_t sub_closure;
    z_closure(&sub_closure, data_handler, NULL, app_event_group);
    z_view_keyexpr_t ke;
    z_view_keyexpr_from_str_unchecked(&ke, KEYEXPR_SUB "/**"); 
    if (z_declare_subscriber(z_loan(session), &main_subscriber, z_loan(ke), z_move(sub_closure), NULL) < 0) {
        ESP_LOGE(TAG, "❗Unable to declare subscriber on '%s/**'❗", KEYEXPR_SUB);
    } else {
        ESP_LOGI(TAG, "📥 Subscriber on '%s/**'", KEYEXPR_SUB);
    }
#endif

#if PUBLISHER_ON
    if (declare_publisher_helper(z_loan(session), 
            &main_publisher, KEYEXPR_PUB "/**") >= 0) {
        g_publisher_declared = true;
    }
#endif

#if HEARTBEAT_ON
    zenoh_heartbeat_init(z_loan(session), app_event_group);
#endif
    
    ESP_LOGD(TAG, "All Zenoh resources declared.");
    xEventGroupSetBits(app_event_group, ZENOH_DECLARED_BIT);

    while (1) { vTaskDelay(portMAX_DELAY); }
}

/**
 * @brief Initializes and starts the Zenoh client task.
 * This function initializes the Zenoh client task and starts it.
 * The task will create a Zenoh session, declare required resources, and
 * start the heartbeat task.
 * @param event_group Handle to the event group that the task will use to signal
 * events to other tasks.
 * @param data_handler The data handler function to be used when processing incoming
 * data from the Zenoh server.
 */
void zenoh_client_init_and_start(EventGroupHandle_t event_group, z_data_handler_t data_handler) {
    ESP_LOGI(TAG, "Calling zenoh_client_init_and_start");
    if (zenoh_task_handle) {
        ESP_LOGW(TAG, "⚠️ Task already running. ⚠️");
        return;
    }
    app_event_group = event_group;
#if SCOUT_ON
    run_scout();
#endif
    xTaskCreate(zenoh_client_task, "zenoh_client_task", 8192, (void*)data_handler, 5, &zenoh_task_handle);
}

void zenoh_client_stop() {
    ESP_LOGI(TAG, "Calling zenoh_client_stop");
#if HEARTBEAT_ON
    zenoh_heartbeat_stop();
#endif
    if (zenoh_task_handle != NULL) { vTaskDelete(zenoh_task_handle); zenoh_task_handle = NULL; }
#if PUBLISHER_ON
    if(g_publisher_declared) { z_drop(z_move(main_publisher)); }
#endif

#if SUBSCRIBER_ON
    z_drop(z_move(main_subscriber));
#endif
    z_drop(z_move(session));
    ESP_LOGI(TAG, "Zenoh client stopped and resources released.");
}

#if PUBLISHER_ON
void zenoh_publish(const char *keyexpr, const char *payload_str) {
    if (g_publisher_declared) {
        z_view_keyexpr_t ke;
        z_view_keyexpr_from_str_unchecked(&ke, keyexpr);
        
        z_owned_bytes_t payload;
        z_bytes_copy_from_str(&payload, payload_str);

        ESP_LOGD(TAG, "\033[38;5;214m🡆 OUT\033[0m:'%s' at '%s'", payload_str, keyexpr);
        
        z_put_options_t options;
        z_put_options_default(&options);
        options.congestion_control = Z_CONGESTION_CONTROL_BLOCK;
        
        z_put(z_loan(session), z_loan(ke), z_move(payload), &options);
    } else {
        ESP_LOGE(TAG, "Publisher not declared. Cannot publish.");
    }
}

/**
 * @brief Publishes a binary payload to the Zenoh server.
 * This function publishes a binary payload to the Zenoh server at the specified
 * key expression. The payload is copied into a Zenoh-owned bytes object.
 * If the publisher has not been declared, this function will log an error and
 * free the provided payload.
 * @param keyexpr The key expression to publish the payload at.
 * @param payload The binary payload to publish.
 * @param len The length of the payload in bytes.
 */
void zenoh_publish_binary(const char *keyexpr, const uint8_t *payload, size_t len) {
     if (!g_publisher_declared) {
        ESP_LOGE(TAG, "Publisher not declared. Cannot publish.");
        free((void *)payload);
        return;
    }
    
    z_view_keyexpr_t ke;
    z_view_keyexpr_from_str_unchecked(&ke, keyexpr);

    z_owned_bytes_t z_payload;
    if (z_bytes_from_buf(&z_payload, (uint8_t *)payload, len, payload_deleter, (void*)1) != Z_OK) {
        ESP_LOGE(TAG, "Failed to create zenoh payload from buffer");
        free((void *)payload); 
        return;
    }
    
    z_put_options_t put_opts;
    z_put_options_default(&put_opts);
    put_opts.congestion_control = Z_CONGESTION_CONTROL_BLOCK;

    int res = z_put(z_loan(session), z_loan(ke), z_move(z_payload), &put_opts);
    
    if (res < 0) {
        ESP_LOGW(TAG, "z_put failed or dropped! (key: %s)", keyexpr);
    }
}

/**
 * @brief Publishes a face payload to the Zenoh server.
 * This function publishes a face payload to the Zenoh server at the specified
 * key expression. The payload is composed of a face_payload_header_t struct,
 * a keypoints array, and an image buffer. All three components are copied
 * into a Zenoh-owned bytes object before publishing.
 * If the publisher has not been declared, this function will log an error and
 * free the provided image buffer.
 * @param keyexpr The key expression to publish the payload at.
 * @param header The face payload header struct.
 * @param keypoints The keypoints array.
 * @param image_buffer The image buffer.
 * NOTE: NOT USED CURRENTLY.
 */
void zenoh_publish_face_payload(const char *keyexpr,
                                const face_payload_header_t *header,
                                const int *keypoints,
                                const uint8_t *image_buffer) {
    if (!g_publisher_declared) {
        ESP_LOGE(TAG, "Publisher not declared. Cannot publish.");
        heap_caps_free((void *)image_buffer);
        return;
    }

    z_owned_bytes_writer_t writer;
    z_bytes_writer_empty(&writer);

    z_bytes_writer_write_all(z_loan_mut(writer), (const uint8_t *)header, sizeof(face_payload_header_t));
    
    size_t keypoints_size = header->keypoints_count * sizeof(int);
    z_bytes_writer_write_all(z_loan_mut(writer), (const uint8_t *)keypoints, keypoints_size);

    z_owned_bytes_t image_bytes;
    z_bytes_from_buf(&image_bytes, (uint8_t *)image_buffer, header->image_len, payload_deleter, NULL);
    z_bytes_writer_append(z_loan_mut(writer), z_move(image_bytes));
    
    z_owned_bytes_t final_payload;
    z_bytes_writer_finish(z_move(writer), &final_payload);
    
    ESP_LOGI(TAG, "\033[38;5;214m🡆 OUT\033[0m: %d bytes at '%s'", (int)z_bytes_len(z_loan(final_payload)), keyexpr);

    z_view_keyexpr_t ke;
    z_view_keyexpr_from_str_unchecked(&ke, keyexpr);

    // --- ADDED RELIABILITY OPTIONS ---
    z_put_options_t options;
    z_put_options_default(&options);
    options.congestion_control = Z_CONGESTION_CONTROL_BLOCK;
    
    if (z_put(z_loan(session), z_loan(ke), z_move(final_payload), &options) < 0) {
        ESP_LOGE(TAG, "Failed to publish data!");
    }
}
#endif

#endif // ZENOH_ENABLED