#include "zenoh_utils.h"
#include <esp_log.h>
#include <esp_netif.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "Z_UTIL";

// Global zenoh listener for logging
static char primary_listener[64] = "N/A";

void zenoh_utils_set_primary_listener(const char* protocol, const char* ip, const char* port, const char* iface) {
    if (!protocol || !ip || !port || !iface) {
        return;
    }
    snprintf(primary_listener, sizeof(primary_listener), "%s/%s:%s#iface=%s",
             protocol, ip, port, iface);
}

const char* zenoh_utils_get_primary_listener() {
    return primary_listener;
}

// Get the active network interface
network_info_t active_network_interface(const char* log_prefix) {
    network_info_t local_network_info = {0};
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        ESP_LOGE(TAG, "%s | Could not get network interface handle.", log_prefix);
        return local_network_info;
    }

    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        ESP_LOGE(TAG, "%s | Failed to get IP info", log_prefix);
        return local_network_info;
    }

    // Store the formatted IP address in the global struct
    sprintf(local_network_info.ip_address, IPSTR, IP2STR(&ip_info.ip));

    // Store the interface name in the global struct
    if (esp_netif_get_netif_impl_name(netif, local_network_info.interface_name) != ESP_OK) {
        strncpy(local_network_info.interface_name, "N/A", sizeof(local_network_info.interface_name));
    }

    ESP_LOGI(TAG, "Active Iface: '%s', IP: %s",
        local_network_info.interface_name,
        local_network_info.ip_address);
        
    return local_network_info;
}

// Helper function to format a Zenoh ID into a printable string
void format_zid(const z_id_t *zid, char *buffer, size_t len) {
    for(size_t i = 0; i < sizeof(zid->id) && (i*2 + 2) < len; ++i) {
        sprintf(&buffer[i*2], "%02X", zid->id[i]);
    }
}
