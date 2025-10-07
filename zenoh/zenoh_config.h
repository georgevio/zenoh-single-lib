#ifndef ZENOH_CONFIG_H
#define ZENOH_CONFIG_H

#define ZENOH_ENABLED 1
#ifndef Z_FEATURE_LINK_TCP
#define Z_FEATURE_LINK_TCP 1
#endif

/*
 * I_AM_CONSUMER_OR_SERVER: set per-device role
 *   1 == consumer (will connect to a server when using TCP)
 *   0 == server   (will listen on its own IP/iface when using TCP)
 */
#define I_AM_CONSUMER_OR_SERVER 1

// Transport: 1 = UDP (multicast peer mode), 0 = TCP (unicast)
#define ZENOH_USE_UDP 1

/*
 * ZENOH_MODE: "client" or "peer"
 * - "client": explicitly connect to a router / server IP (used for TCP consumer mode)
 * - "peer":  devices operate as peers; for UDP this uses multicast listen only
 */
#define ZENOH_MODE "peer"

/* Protocol selection derived from ZENOH_USE_UDP */
#if ZENOH_USE_UDP == 1
#define ZENOH_PROTOCOL "udp"
// multicast address used by UDP peer mode
#define ZENOH_LISTEN_BROADCAST_IP "224.0.0.251"
#else
#define ZENOH_PROTOCOL "tcp"
// leave empty when not using multicast
#define ZENOH_LISTEN_BROADCAST_IP ""
#endif

// SERVER IP (the consumer will connect to this address when acting as consumer in TCP)
#define ZENOH_SERVER_IP "192.168.137.37"

#define ZENOH_PORT "7447"

// Feature Flags
#define PUBLISHER_ON 1
#define SUBSCRIBER_ON 1 
#define SCOUT_ON 0 // it does not seem to work for UDP as supposed to!
#if I_AM_CONSUMER_OR_SERVER == 1
#define QUERYABLE_ON 1
#else
#define QUERYABLE_ON 0
#endif

// Key Expressions for the application protocol
#define KEYEXPR_ANNOUNCE "faces/announcements"
#define KEYEXPR_DATA_QUERY "faces/data"
#define KEYEXPR_RESULTS "faces/results"

// Conditional key expressions based on role
#if I_AM_CONSUMER_OR_SERVER == 1 // consumer configuration
#define KEYEXPR_PUB KEYEXPR_ANNOUNCE
#define KEYEXPR_SUB KEYEXPR_RESULTS
#define KEYEXPR_QUERYABLE KEYEXPR_DATA_QUERY
#else // Server configuration
#define KEYEXPR_PUB KEYEXPR_RESULTS
#define KEYEXPR_SUB KEYEXPR_ANNOUNCE
#endif

// Periodic Heartbeat. Is heartbeat needed?
#define HEARTBEAT_ON 1
#define HEARTBEAT_CHANNEL "heartbeats"

#if I_AM_CONSUMER_OR_SERVER == 1
#define HEARTBEAT_MESSAGE "ESP32-CAM-Heartbeat"
#define HEARTBEAT_INTERVAL_MS 61000 //primes and different to avoid collisions
#else
#define HEARTBEAT_MESSAGE "ESP32S3-Heartbeat"
#define HEARTBEAT_INTERVAL_MS 73000 //primes and different to avoid collisions
#endif

// Event Group Bits
#define ZENOH_CONNECTED_BIT       (1 << 1)
#define ZENOH_DECLARED_BIT        (1 << 2)
#define ZENOH_STOP_BIT            (1 << 3)
#define TRANSFER_COMPLETE_BIT     (1 << 4)

#endif // ZENOH_CONFIG_H