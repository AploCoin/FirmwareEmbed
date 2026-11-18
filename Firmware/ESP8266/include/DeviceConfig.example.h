#pragma once
// Copy to DeviceConfig.h (ignored). Session key is imported over serial into RAM.
// The empty defaults compile but deliberately cannot mine.
#define APLO_WIFI_SSID ""
#define APLO_WIFI_PASSWORD ""
#define APLO_RPC_PRIMARY "https://rpc.example.invalid"
#define APLO_RPC_FALLBACK ""
// Set the public root PEM for each configured endpoint, or generate AploTrustProfile.h.
#define APLO_RPC_PRIMARY_CA nullptr
#define APLO_RPC_FALLBACK_CA nullptr
// Override with reachable LAN NTP servers in isolated networks.
#define APLO_NTP_PRIMARY "pool.ntp.org"
#define APLO_NTP_FALLBACK "time.cloudflare.com"
#define APLO_OWNER_ADDRESS ""
// Public authorization metadata from the owner's confirmed registration.
#define APLO_SESSION_ADDRESS ""
#define APLO_AUTHORIZED_CHAIN_ID ""
#define APLO_AUTHORIZED_GAPLO_BUDGET ""
#define APLO_SESSION_EXPIRY_BLOCK 0ULL
