#pragma once
// This ignored header is the only file containing local credentials.
#if defined(APLO_BUILD_VALIDATION)
#include "BuildValidationConfig.h"
#elif __has_include("DeviceConfig.h")
#include "DeviceConfig.h"
#else
#include "DeviceConfig.example.h"
#endif

#ifndef APLO_RPC_ROOT_CA
#define APLO_RPC_ROOT_CA ""
#endif

#ifndef APLO_NTP_PRIMARY
#define APLO_NTP_PRIMARY "pool.ntp.org"
#endif
#ifndef APLO_NTP_FALLBACK
#define APLO_NTP_FALLBACK "time.cloudflare.com"
#endif

#ifdef APLO_TRUST_ENDPOINTS_HEADER
#include APLO_TRUST_ENDPOINTS_HEADER
#endif

namespace FirmwareConfig {
constexpr const char* wifiSsid = APLO_WIFI_SSID;
constexpr const char* wifiPassword = APLO_WIFI_PASSWORD;
#ifdef APLO_TRUST_ENDPOINTS_HEADER
constexpr const char* rpcPrimary = APLO_TRUST_PRIMARY_URL;
constexpr const char* rpcFallback = APLO_TRUST_FALLBACK_URL;
#else
constexpr const char* rpcPrimary = APLO_RPC_PRIMARY;
constexpr const char* rpcFallback = APLO_RPC_FALLBACK;
#endif
constexpr const char* rpcRootCa = APLO_RPC_ROOT_CA;
constexpr const char* ntpPrimary = APLO_NTP_PRIMARY;
constexpr const char* ntpFallback = APLO_NTP_FALLBACK;
constexpr const char* ownerAddress = APLO_OWNER_ADDRESS;
constexpr const char* sessionAddress = APLO_SESSION_ADDRESS;
constexpr const char* authorizedChainId = APLO_AUTHORIZED_CHAIN_ID;
constexpr const char* authorizedGaploBudget = APLO_AUTHORIZED_GAPLO_BUDGET;
constexpr uint64_t sessionExpiryBlock = APLO_SESSION_EXPIRY_BLOCK;
constexpr uint32_t hashesPerCycle = 2000;
constexpr unsigned long cycleDelayMs = 1000;
constexpr uint8_t receiptPollAttempts = 30;
constexpr unsigned long receiptPollDelayMs = 1000;
constexpr uint8_t wifiConnectAttempts = 3;
constexpr unsigned long wifiAttemptTimeoutMs = 45000;
constexpr uint8_t wifiReconnectAttempts = 2;
constexpr unsigned long wifiRetryDelayMs = 5000;
} // namespace FirmwareConfig
