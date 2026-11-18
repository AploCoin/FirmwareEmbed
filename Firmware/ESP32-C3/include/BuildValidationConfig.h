#pragma once
// Non-secret fixture to keep the complete application reachable for size checks.
// Enabled only by -DAPLO_BUILD_VALIDATION=1. No private key exists in this file.
#define APLO_WIFI_SSID "APLO_BUILD_VALIDATION_NO_NETWORK"
#define APLO_WIFI_PASSWORD ""
#define APLO_RPC_PRIMARY "https://pub1.aplocoin.com"
#define APLO_RPC_FALLBACK "https://pub2.aplocoin.com"
#define APLO_OWNER_ADDRESS "0x1111111111111111111111111111111111111111"
#define APLO_SESSION_ADDRESS "0x2222222222222222222222222222222222222222"
#define APLO_AUTHORIZED_CHAIN_ID "1"
#define APLO_AUTHORIZED_GAPLO_BUDGET "1000000000000000000"
#define APLO_SESSION_EXPIRY_BLOCK 1ULL
