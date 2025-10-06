#ifndef ZENOH_CONFIG_H
#define ZENOH_CONFIG_H

#define ZENOH_ENABLED 1
#ifndef Z_FEATURE_LINK_TCP
#define Z_FEATURE_LINK_TCP 1
#endif // Z_FEATURE_LINK_TCP

#define ZENOH_ROLE_CLIENT 1 // The device that sends data to the server (e.g., ESP32-CAM).
#define ZENOH_ROLE_SERVER 0 // The device that receives data (e.g., ESP32-S3).

// 1. DEFINE THE ROLE OF DEVICE
#define ZENOH_DEVICE_ROLE ZENOH_ROLE_SERVER

#define ZENOH_TRANSPORT_TCP_CLIENT 1 // Connects to a specific IP. Zenoh mode is 'client'.
#define ZENOH_TRANSPORT_TCP_PEER   2 // Listens for incoming connections. Zenoh mode is 'peer'.
#define ZENOH_TRANSPORT_UDP_PEER   3 // Listens on a multicast address. Zenoh mode is 'peer'.

// 2. DEFINE ZENOH PROTOCOL 
#define ZENOH_TRANSPORT ZENOH_TRANSPORT_UDP_PEER

// 3. DEFINE NETWORK ENDPOINTS
#define ZENOH_SERVER_IP "192.168.137.2" // IP of the ZENOH_ROLE_SERVER device.
// Change only for firewall bypass or testing!
#define ZENOH_PORT "7447"

// 4. FEATURE FLAGS: Keep unchanged to start with
#define PUBLISHER_ON 1
#define SUBSCRIBER_ON 1
#define SCOUT_ON 0 // it does not seem to work for UDP as supposed to!
#if ZENOH_DEVICE_ROLE == ZENOH_ROLE_CLIENT
    #define QUERYABLE_ON 1
#else // ZENOH_ROLE_SERVER
    #define QUERYABLE_ON 0
#endif

// DERIVED CONFIGURATIONS (DO NOT EDIT)
#if ZENOH_TRANSPORT == ZENOH_TRANSPORT_TCP_CLIENT
    #define ZENOH_MODE "client"
    #define ZENOH_PROTOCOL "tcp"
    // Client role must use TCP Client transport
    #if ZENOH_DEVICE_ROLE != ZENOH_ROLE_CLIENT
        #error "ZENOH_TRANSPORT_TCP_CLIENT is only valid for a device with ZENOH_DEVICE_ROLE ZENOH_ROLE_CLIENT"
    #endif
#elif ZENOH_TRANSPORT == ZENOH_TRANSPORT_TCP_PEER
    #define ZENOH_MODE "peer"
    #define ZENOH_PROTOCOL "tcp"
    // Server role must use TCP Peer transport
    #if ZENOH_DEVICE_ROLE != ZENOH_ROLE_SERVER
        #error "ZENOH_TRANSPORT_TCP_PEER is only valid for a device with ZENOH_DEVICE_ROLE ZENOH_ROLE_SERVER"
    #endif
#elif ZENOH_TRANSPORT == ZENOH_TRANSPORT_UDP_PEER
    #define ZENOH_MODE "peer"
    #define ZENOH_PROTOCOL "udp"
    #define ZENOH_MULTICAST_IP "224.0.0.251" // Zenoh default multicast address. Do not change.
#endif

// Key Expressions for the application protocol
#define KEYEXPR_ANNOUNCE "faces/announcements"
#define KEYEXPR_DATA_QUERY "faces/data"
#define KEYEXPR_RESULTS "faces/results"

// Conditional key expressions based on role
#if ZENOH_DEVICE_ROLE == ZENOH_ROLE_CLIENT
#define KEYEXPR_PUB KEYEXPR_ANNOUNCE
#define KEYEXPR_SUB KEYEXPR_RESULTS
#define KEYEXPR_QUERYABLE KEYEXPR_DATA_QUERY
#else // Server configuration
#define KEYEXPR_PUB KEYEXPR_RESULTS
#define KEYEXPR_SUB KEYEXPR_ANNOUNCE
#endif

// Periodic Heartbeat. Is heartbeat needed?
#define HEARTBEAT_ON 0
#define HEARTBEAT_CHANNEL "heartbeats"

#if ZENOH_DEVICE_ROLE == ZENOH_ROLE_CLIENT
#define HEARTBEAT_MESSAGE "ESP32-CAM-Heartbeat"
#define HEARTBEAT_INTERVAL_MS 61000 //primes and different to avoid collisions
#else
#define HEARTBEAT_MESSAGE "ESP32S3-Heartbeat"
#define HEARTBEAT_INTERVAL_MS 73000 // primes and different to avoid collisions
#endif

// Event Group Bits
#define ZENOH_CONNECTED_BIT       (1 << 1)
#define ZENOH_DECLARED_BIT        (1 << 2)
#define ZENOH_STOP_BIT            (1 << 3)
#define TRANSFER_COMPLETE_BIT     (1 << 4)

#endif // ZENOH_CONFIG_H