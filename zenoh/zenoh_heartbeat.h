#ifndef ZENOH_HEARTBEAT_H
#define ZENOH_HEARTBEAT_H

#include <zenoh-pico.h>
#include "zenoh_config.h"
#include "freertos/event_groups.h"

#if HEARTBEAT_ON

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes the heartbeat publisher, subscriber, and task.
 * @param session A loan to the active Zenoh session.
 * @param event_group The application event group to wait on for resource declaration.
 */
void zenoh_heartbeat_init(z_loaned_session_t *session, EventGroupHandle_t event_group);

/**
 * @brief Stops the heartbeat task and cleans up its resources.
 */
void zenoh_heartbeat_stop();

#ifdef __cplusplus
}
#endif

#endif // HEARTBEAT_ON
#endif // ZENOH_HEARTBEAT_H