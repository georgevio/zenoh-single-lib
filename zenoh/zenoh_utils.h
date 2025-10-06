#ifndef ZENOH_UTILS_H
#define ZENOH_UTILS_H

#include <zenoh-pico.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// network interface and IP
typedef struct {
    char ip_address[16];      // IPv4
    char interface_name[8];   // "st1" or 'st0"
} network_info_t;

/**
 * @brief Sets the primary listener string from its components.
 */
void zenoh_utils_set_primary_listener(const char* protocol, const char* ip, const char* port, const char* iface);

/**
 * @brief Gets the currently configured primary listener string.
 * @return A read-only pointer to the listener string.
 */
const char* zenoh_utils_get_primary_listener();

/**
 * @brief Gets the active network interface and IP address.
 * @param log_prefix A prefix for log messages.
 * @return A struct containing the network information.
 */
network_info_t active_network_interface(const char* log_prefix);

/**
 * @brief Helper function to format a Zenoh ID into a printable string.
 * @param zid Pointer to the Zenoh ID.
 * @param buffer Buffer to store the formatted string.
 * @param len Length of the buffer.
 */
void format_zid(const z_id_t *zid, char *buffer, size_t len);

#ifdef __cplusplus
}
#endif

#endif // ZENOH_UTILS_H
