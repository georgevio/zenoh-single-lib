#ifndef ZENOH_MANAGER_H
#define ZENOH_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <zenoh-pico.h>
#include <zenoh-pico/api/handlers.h>
#include <stddef.h> 
#include <stdint.h> 

#include "zenoh_config.h"
#include "shared_payload.h"

#if defined(__cplusplus) && (ZENOH_DEVICE_ROLE == ZENOH_ROLE_CLIENT)
#include "shared_types.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*z_data_handler_t)(z_loaned_sample_t* sample, void* arg);

void zenoh_client_init_and_start(EventGroupHandle_t event_group, z_data_handler_t data_handler);
void zenoh_client_stop();

void zenoh_publish(const char *keyexpr, const char *payload_str);
void zenoh_publish_binary(const char *keyexpr, const uint8_t *payload, size_t len);

#if defined(__cplusplus) && (ZENOH_DEVICE_ROLE == ZENOH_ROLE_CLIENT)
void zenoh_set_queryable_data(face_to_send_t *face);
#endif

#if ZENOH_DEVICE_ROLE == ZENOH_ROLE_SERVER
void zenoh_get_data(const char *keyexpr, void (*handler)(z_loaned_reply_t*, void*), void *arg);
#endif

#ifdef __cplusplus
}
#endif

#endif // ZENOH_MANAGER_H