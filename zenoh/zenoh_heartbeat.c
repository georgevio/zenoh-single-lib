#include "zenoh_heartbeat.h"

#if HEARTBEAT_ON // The entire file is conditionally compiled

#include <esp_log.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "Z_HEART";

static z_owned_publisher_t heartbeat_publisher;
static z_owned_subscriber_t subscriber_heartbeat;
static TaskHandle_t heartbeat_task_handle = NULL;

static void heartbeat_task(void *arg) {
    EventGroupHandle_t event_group = (EventGroupHandle_t)arg;
    ESP_LOGD(TAG, "HEARTBEAT started. Waiting for Zenoh resources...");

    xEventGroupWaitBits(event_group, ZENOH_DECLARED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGD(TAG, "Zenoh resources ready. Starting heartbeat loop.");

    uint32_t heartbeat_counter = 0;
    char heartbeat_msg[64];

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
        heartbeat_counter++;
        snprintf(heartbeat_msg, sizeof(heartbeat_msg), "%s #%lu", HEARTBEAT_MESSAGE, heartbeat_counter);

        // CORRECTED: Use the local heartbeat_publisher directly
        ESP_LOGI("Z_MANAGER", "\033[38;5;214m🡆 OUT\033[0m:'%s' at '%s'", heartbeat_msg, HEARTBEAT_CHANNEL);
        z_owned_bytes_t payload;
        z_bytes_copy_from_str(&payload, heartbeat_msg);
        z_publisher_put(z_loan(heartbeat_publisher), z_move(payload), NULL);
    }
}

static void sub_heartbeat_handler(z_loaned_sample_t* sample, void* arg) {
    (void)arg;
    z_view_string_t keystr;
    z_keyexpr_as_view_string(z_sample_keyexpr(sample), &keystr);
    z_owned_string_t payload_str;
    z_bytes_to_string(z_sample_payload(sample), &payload_str);

    ESP_LOGI(TAG, "\033[38;5;199m💓 HB IN\033[0m: '%.*s' on '%.*s'",
            (int)z_string_len(z_loan(payload_str)),
            z_string_data(z_loan(payload_str)),
            (int)z_string_len(z_loan(keystr)),
            z_string_data(z_loan(keystr)));

    z_drop(z_move(payload_str));
}

void zenoh_heartbeat_init(z_loaned_session_t *session, EventGroupHandle_t event_group) {
    ESP_LOGD(TAG, "Heartbeat Initializing...");

    z_view_keyexpr_t hb_pub_key;
    z_view_keyexpr_from_str_unchecked(&hb_pub_key, HEARTBEAT_CHANNEL);
    if (z_declare_publisher(session, &heartbeat_publisher, z_loan(hb_pub_key), NULL) < 0) {
        ESP_LOGE(TAG, "❗Unable to declare heartbeat publisher at '%s'❗", HEARTBEAT_CHANNEL);
    } else {
        ESP_LOGI(TAG, "📡 Heartbeat Publisher for 💓 at %s", HEARTBEAT_CHANNEL);
    }

    z_owned_closure_sample_t sub_closure_heartbeat;
    z_closure(&sub_closure_heartbeat, sub_heartbeat_handler, NULL, NULL);
    z_view_keyexpr_t ke_sub_heartbeat;
    z_view_keyexpr_from_str_unchecked(&ke_sub_heartbeat, HEARTBEAT_CHANNEL);
    if (z_declare_subscriber(session, &subscriber_heartbeat, z_loan(ke_sub_heartbeat), z_move(sub_closure_heartbeat), NULL) < 0) {
        ESP_LOGE(TAG, "❗Unable to declare subscriber on '%s'❗", HEARTBEAT_CHANNEL);
    } else {
        ESP_LOGI(TAG, "📥 Subscriber for 💓 on '%s'", HEARTBEAT_CHANNEL);
    }

    xTaskCreate(heartbeat_task, "heartbeat_task", 4096, event_group, 5, &heartbeat_task_handle);
}

void zenoh_heartbeat_stop() {
    if (heartbeat_task_handle != NULL) {
        vTaskDelete(heartbeat_task_handle);
        heartbeat_task_handle = NULL;
    }
    z_drop(z_move(heartbeat_publisher));
    z_drop(z_move(subscriber_heartbeat));
}

#endif // HEARTBEAT_ON