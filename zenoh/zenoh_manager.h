#ifndef ZENOH_MANAGER_H
#define ZENOH_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <zenoh-pico.h>
#include <stddef.h> 
#include <stdint.h> 

#include "zenoh_config.h"
#include "shared_payload.h"

#ifdef __cplusplus
extern "C" {
#endif

// The main application must provide a function with this signature to handle incoming data.
typedef void (*z_data_handler_t)(z_loaned_sample_t* sample, void* arg);

// The init function now accepts a pointer to the application's data handler.
void zenoh_client_init_and_start(EventGroupHandle_t event_group, z_data_handler_t data_handler);

void zenoh_client_stop();

// Publishes a simple string
void zenoh_publish(const char *keyexpr, const char *payload_str);

// Publishes a raw binary buffer, accepting options for non-blocking sends
void zenoh_publish_binary(const char *keyexpr, const uint8_t *payload, size_t len, const z_publisher_put_options_t *options);

// Efficiently builds and publishes the complete face payload from its separate parts
void zenoh_publish_face_payload(const char *keyexpr,
                                const face_payload_header_t *header,
                                const int *keypoints,
                                const uint8_t *image_buffer);

// Queryable data provider API
// The application can register a callback that fills a z_owned_bytes_t payload
// when the device receives a GET query. The callback should return 0 on
// success and non-zero on failure. The callback must initialize *out on
// success (e.g., via z_bytes_from_buf). The context pointer is opaque.
typedef int (*zenoh_query_provider_t)(void *ctx, z_owned_bytes_t *out);
void zenoh_register_query_provider(zenoh_query_provider_t cb, void *ctx);
void zenoh_set_queryable_data(void *data); // legacy: store an opaque pointer

#ifdef __cplusplus
}
#endif

#endif // ZENOH_MANAGER_H