#include "zenoh_manager.h"
#include "zenoh_scout.h"
#include "zenoh_utils.h"
#include "zenoh_heartbeat.h"
#include <esp_log.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "esp_heap_caps.h"
#include "esp_netif.h"

#if ZENOH_DEVICE_ROLE == ZENOH_ROLE_CLIENT
#include "shared_types.h"
#endif

#if ZENOH_ENABLED

static const char *TAG = "Z_MANAGER";

extern "C" {
    static void payload_deleter(void *data, void *context) {
        ESP_LOGD("PAYLOAD_DELETER", "Freeing payload at address %p", data);
        if (context != NULL) {
            free(data);
        } else {
            heap_caps_free(data);
        }
    }

#if ZENOH_DEVICE_ROLE == ZENOH_ROLE_CLIENT
    static face_to_send_t *g_current_face_for_query = NULL;
    void zenoh_set_queryable_data(face_to_send_t *face) {
        g_current_face_for_query = face;
    }

    static void client_query_handler(const z_loaned_query_t *query, void *context) {
        z_view_string_t key_view;
        z_keyexpr_as_view_string(z_query_keyexpr(query), &key_view);
        ESP_LOGI(TAG, "💡 Queryable received GET for '%.*s'", 
                (int)z_string_len(z_loan(key_view)), z_string_data(z_loan(key_view)));

        if (g_current_face_for_query != NULL && g_current_face_for_query->fb != NULL) {
            z_owned_bytes_t payload;
            z_bytes_from_buf(&payload, g_current_face_for_query->fb->buf, 
                              g_current_face_for_query->fb->len, NULL, NULL);
            z_query_reply(query, z_query_keyexpr(query), z_move(payload), NULL);
        } else {
            ESP_LOGE(TAG, "Query received but no data is staged for transfer!");
            z_owned_bytes_t err_payload;
            z_bytes_empty(&err_payload);
            z_query_reply_err(query, z_move(err_payload), NULL);
        }
    }
#endif // ZENOH_DEVICE_ROLE == ZENOH_ROLE_CLIENT
} // extern "C"

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


#define PRINT_CONFIG_VALUE(cfg, key_macro, key_name) \
    do { \
        const char *value = zp_config_get(cfg, key_macro); \
        ESP_LOGI("ZENOH_CONFIG", "  %-24s: %s", key_name, value ? value : "(not set)"); \
    } while(0)

static void print_zenoh_config(const z_loaned_config_t *config) {
    ESP_LOGI("ZENOH_CONFIG", "--- Zenoh Configuration (queried values) ---");
    PRINT_CONFIG_VALUE(config, Z_CONFIG_MODE_KEY, "Mode");
    PRINT_CONFIG_VALUE(config, Z_CONFIG_CONNECT_KEY, "Connect Endpoints");
    PRINT_CONFIG_VALUE(config, Z_CONFIG_LISTEN_KEY, "Listen Endpoints");
    PRINT_CONFIG_VALUE(config, Z_CONFIG_USER_KEY, "User");
    PRINT_CONFIG_VALUE(config, Z_CONFIG_PASSWORD_KEY, "Password");
    PRINT_CONFIG_VALUE(config, Z_CONFIG_MULTICAST_SCOUTING_KEY, "Multicast Scouting");
    PRINT_CONFIG_VALUE(config, Z_CONFIG_MULTICAST_LOCATOR_KEY, "Multicast Locator");
    PRINT_CONFIG_VALUE(config, Z_CONFIG_SCOUTING_TIMEOUT_KEY, "Scouting Timeout");
    PRINT_CONFIG_VALUE(config, Z_CONFIG_SCOUTING_WHAT_KEY, "Scouting What");
    PRINT_CONFIG_VALUE(config, Z_CONFIG_SESSION_ZID_KEY, "Session ZID");
    PRINT_CONFIG_VALUE(config, Z_CONFIG_ADD_TIMESTAMP_KEY, "Add Timestamp");
    ESP_LOGI("ZENOH_CONFIG", "------------------------------------------");
}

static z_result_t declare_publisher_helper(const z_loaned_session_t* s, 
        z_owned_publisher_t *pub, const char *keyexpr) {
    z_view_keyexpr_t ke;
    z_view_keyexpr_from_str_unchecked(&ke, keyexpr);
    z_result_t res = z_declare_publisher(s, pub, z_loan(ke), NULL);
    if (res < 0) { ESP_LOGE(TAG, "❗Unable to declare publisher on '%s'❗", keyexpr); } 
    else { ESP_LOGI(TAG, "📡 Publisher on '%s'", keyexpr); }
    return res;
}

static void zenoh_client_task(void *arg) {
    z_data_handler_t data_handler = (z_data_handler_t)arg;
    network_info_t net_info = active_network_interface("Z_IFACE");

    bool sesson_opened = false;
    while (!sesson_opened) { // Create a new config in EVERY attempt!
        z_owned_config_t config;
        z_config_default(&config);
        zp_config_insert(z_loan_mut(config), Z_CONFIG_MODE_KEY, ZENOH_MODE);

#if ZENOH_TRANSPORT == ZENOH_TRANSPORT_TCP_CLIENT
        // Zenoh Client (TCP) - Connects to a server
        char connect_endpoint[64];
        snprintf(connect_endpoint, sizeof(connect_endpoint), "tcp/%s:%s", ZENOH_SERVER_IP, ZENOH_PORT);
        zp_config_insert(z_loan_mut(config), Z_CONFIG_CONNECT_KEY, connect_endpoint);
        ESP_LOGI(TAG, "🔗 CLIENT CONNECTS to: %s (TCP)", connect_endpoint);

#elif ZENOH_TRANSPORT == ZENOH_TRANSPORT_TCP_PEER
        // Zenoh Peer (TCP) - Listens for clients
        char listen_endpoint[64];
        snprintf(listen_endpoint, sizeof(listen_endpoint), "tcp/0.0.0.0:%s", ZENOH_PORT);
        zp_config_insert(z_loan_mut(config), Z_CONFIG_LISTEN_KEY, listen_endpoint);
        ESP_LOGI(TAG, "🌐 SERVER LISTENS on: %s (TCP)", listen_endpoint);

#elif ZENOH_TRANSPORT == ZENOH_TRANSPORT_UDP_PEER
        // Zenoh Peer (UDP) - Listens on multicast
        char listen_endpoint[64];
        snprintf(listen_endpoint, sizeof(listen_endpoint), "udp/%s:%s#iface=%s",
                    ZENOH_MULTICAST_IP, ZENOH_PORT, net_info.interface_name);
        zp_config_insert(z_loan_mut(config), Z_CONFIG_LISTEN_KEY, listen_endpoint);
        ESP_LOGI(TAG, "🌐 PEER LISTENS on: %s (UDP)", listen_endpoint);
#endif

        print_zenoh_config(z_loan(config)); // can be disabled
        z_result_t res = z_open(&session, z_move(config), NULL);
        if (res < 0) {
            ESP_LOGE(TAG, "❗Zenoh session failed: %d), retry...❗", res);
            vTaskDelay(15000 / portTICK_PERIOD_MS); // Retry after 15 sec
        } else {
            sesson_opened = true;
        }
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
#endif //SUBSCRIBER_ON

#if PUBLISHER_ON
    if (declare_publisher_helper(z_loan(session), &main_publisher, KEYEXPR_PUB "/**") >= 0) {
        g_publisher_declared = true;
    }
#endif //PUBLISHER_ON

#if QUERYABLE_ON && (ZENOH_DEVICE_ROLE == ZENOH_ROLE_CLIENT)
    z_owned_closure_query_t query_closure;
    z_closure(&query_closure, (void (*)(z_loaned_query_t *, void*))client_query_handler, NULL, NULL);
    z_view_keyexpr_t ke_queryable;
    z_view_keyexpr_from_str_unchecked(&ke_queryable, KEYEXPR_QUERYABLE "/**");
    if (z_declare_queryable(z_loan(session), &queryable, z_loan(ke_queryable), z_move(query_closure), NULL) < 0) {
        ESP_LOGE(TAG, "❗Unable to declare queryable on '%s/**'❗", KEYEXPR_QUERYABLE);
    } else {
        ESP_LOGI(TAG, "💡 Queryable on '%s/**'", KEYEXPR_QUERYABLE);
    }
#endif

#if HEARTBEAT_ON
    zenoh_heartbeat_init(z_loan_mut(session), app_event_group);
#endif
    
    ESP_LOGD(TAG, "All Zenoh resources declared.");
    xEventGroupSetBits(app_event_group, ZENOH_DECLARED_BIT);

    while (1) { vTaskDelay(portMAX_DELAY); }
}

extern "C" {
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
        if (zenoh_task_handle != NULL) { vTaskDelete(zenoh_task_handle); 
                zenoh_task_handle = NULL; }
#if PUBLISHER_ON
            if(g_publisher_declared) { z_drop(z_move(main_publisher)); }
#endif

#if QUERYABLE_ON
            z_drop(z_move(queryable));
#endif

#if SUBSCRIBER_ON
            z_drop(z_move(main_subscriber));
#endif
        z_drop(z_move(session));
        ESP_LOGI(TAG, "Zenoh client stopped and resources released.");
    }

#if ZENOH_DEVICE_ROLE == ZENOH_ROLE_SERVER
    void zenoh_get_data(const char *keyexpr, void (*handler)(z_loaned_reply_t*, void*), void *arg) {
        ESP_LOGI(TAG, "➡️ GET request for '%s'", keyexpr);
        z_owned_closure_reply_t reply_closure; 
        z_closure(&reply_closure, handler, NULL, arg);
        
        z_get_options_t options;
        z_get_options_default(&options);
        
        z_view_keyexpr_t ke;
        z_view_keyexpr_from_str_unchecked(&ke, keyexpr);
        
        if (z_get(z_loan(session), z_loan(ke), "", z_move(reply_closure), &options) < 0) {
            ESP_LOGE(TAG, "❗Failed to send GET request for '%s'❗", keyexpr);
        }
    }
#endif // ZENOH_DEVICE_ROLE == ZENOH_ROLE_SERVER

    void zenoh_publish(const char *keyexpr, const char *payload_str) {
        if (g_publisher_declared) {
            z_view_keyexpr_t ke;
            z_view_keyexpr_from_str_unchecked(&ke, keyexpr);
            z_owned_bytes_t payload;
            z_bytes_copy_from_str(&payload, payload_str);
            ESP_LOGD(TAG, "\033[38;5;214m🡆 OUT\033[0m:'%s' at '%s'", payload_str, keyexpr);
            z_put(z_loan(session), z_loan(ke), z_move(payload), NULL);
        } else {
            ESP_LOGE(TAG, "Publisher not declared. Cannot publish.");
        }
    }

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
        int res = z_put(z_loan(session), z_loan(ke), z_move(z_payload), NULL);
        if (res < 0) {
            ESP_LOGW(TAG, "z_put failed or dropped! (key: %s)", keyexpr);
        }
    }
} // extern "C"

#endif // ZENOH_ENABLED