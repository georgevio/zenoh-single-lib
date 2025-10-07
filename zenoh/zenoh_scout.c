#include "zenoh_scout.h"
#include "zenoh_config.h"
#include "zenoh_utils.h"
#include <zenoh-pico.h>
#include <esp_log.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "Z_SCUT";

// Scout Functionality Callbacks
static void scout_callback(z_loaned_hello_t *hello, void *context) {
    z_id_t zid = z_hello_zid(hello);
    char zid_str[sizeof(zid.id) * 2 + 1] = {0};
    format_zid(&zid, zid_str, sizeof(zid_str));
    ESP_LOGI(TAG, "SCOUT found peer '%s'", zid_str);
    (*(int *)context)++;
}

static void scout_drop(void *context) {
    int count = *(int *)context;
    free(context);
    ESP_LOGI(TAG, "Scout found %d Zenoh instances.", count);
    // NOTE: SCOUT CANNOT FIND AN ESP32 IN PEER MODE!
}

// Scout Public Function
void run_scout() {
    ESP_LOGD(TAG, "Starting Zenoh scout...");
    int *context = (int *)malloc(sizeof(int));
    *context = 0;

    network_info_t net_info = active_network_interface("SCOUT");
    z_owned_config_t config;
    z_config_default(&config);

    char scout_locator[64];
    snprintf(scout_locator, sizeof(scout_locator), "%s/%s:%s#iface=%s",
            ZENOH_PROTOCOL, ZENOH_LISTEN_BROADCAST_IP, 
            ZENOH_PORT, net_info.interface_name);
    ESP_LOGI(TAG, "SCOUT with locator: %s", scout_locator);
    zp_config_insert(z_loan_mut(config), Z_CONFIG_CONNECT_KEY, scout_locator);

    z_owned_closure_hello_t closure;
    z_closure_hello(&closure, scout_callback, scout_drop, context);
    z_scout(z_move(config), z_move(closure), NULL);
}
