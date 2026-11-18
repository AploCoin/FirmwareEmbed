#pragma once
// Copy to DeviceConfig.h (ignored). Session key is imported over serial into RAM.
// The empty defaults compile but deliberately cannot mine.
#define APLO_WIFI_SSID ""
#define APLO_WIFI_PASSWORD ""
#define APLO_RPC_PRIMARY "https://rpc.example.invalid"
#define APLO_RPC_FALLBACK ""
// APLO_TRUST_PROFILE sets the RPC URLs and CA for each endpoint.
// Without a profile, supply a public PEM root trusted by both configured URLs.
#define APLO_RPC_ROOT_CA ""
// Override with reachable LAN NTP servers in isolated networks.
#define APLO_NTP_PRIMARY "pool.ntp.org"
#define APLO_NTP_FALLBACK "time.cloudflare.com"
#define APLO_OWNER_ADDRESS ""
// Public authorization metadata from the owner's confirmed registration.
#define APLO_SESSION_ADDRESS ""
#define APLO_AUTHORIZED_CHAIN_ID ""
#define APLO_AUTHORIZED_GAPLO_BUDGET ""
#define APLO_SESSION_EXPIRY_BLOCK 0ULL

// Optional local SNTP adapter for a LAN IPv4 address and custom port.
// #define APLO_LOCAL_NTP_HOST "192.0.2.10"
// #define APLO_LOCAL_NTP_PORT 12300
