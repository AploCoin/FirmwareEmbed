#pragma once

#if __has_include("DeviceConfig.h")
#include "DeviceConfig.h"
#else
#include "DeviceConfig.example.h"
#endif

#ifndef APLO_RPC_ROOT_CA
#define APLO_RPC_ROOT_CA nullptr
#endif
#ifndef APLO_RPC_PRIMARY_CA
#define APLO_RPC_PRIMARY_CA APLO_RPC_ROOT_CA
#endif
#ifndef APLO_RPC_FALLBACK_CA
#define APLO_RPC_FALLBACK_CA APLO_RPC_ROOT_CA
#endif
#ifndef APLO_NTP_PRIMARY
#define APLO_NTP_PRIMARY "pool.ntp.org"
#endif
#ifndef APLO_NTP_FALLBACK
#define APLO_NTP_FALLBACK "time.cloudflare.com"
#endif
