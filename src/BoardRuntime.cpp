#include "BoardRuntime.h"

#include <AploPlatform.h>
#include <time.h>

#if defined(ESP32)
#include <esp_system.h>
#endif

namespace firmware {
namespace {
constexpr uint32_t wifiTimeoutMs = 45000;
constexpr uint32_t radioResetMs = 250;
constexpr time_t minimumUnixTime = 1700000000;
} // namespace

void BoardRuntime::begin(
    const char* ssid,
    const char* password,
    const char* ntpPrimary,
    const char* ntpFallback
) {
    ssid_ = ssid;
    password_ = password;
    ntpPrimary_ = ntpPrimary;
    ntpFallback_ = ntpFallback[0] ? ntpFallback : nullptr;
    WiFi.persistent(false);
    wifiAt_ = millis();
}

NetworkState BoardRuntime::poll() {
    const uint32_t now = millis();
    if (startStation_ && static_cast<int32_t>(now - wifiAt_) >= 0) {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
#if defined(ESP8266)
        const IPAddress dhcp(static_cast<uint32_t>(0));
        WiFi.config(dhcp, dhcp, dhcp);
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
        WiFi.setSleep(false);
#endif
        WiFi.begin(ssid_, password_);
        startStation_ = false;
        wifiAt_ = now + wifiTimeoutMs;
    }

    if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(static_cast<uint32_t>(0))) {
        clockStarted_ = false;
        if (!startStation_ && static_cast<int32_t>(now - wifiAt_) >= 0) {
#if defined(ESP8266)
            WiFi.disconnect();
#else
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
#endif
            wifiAt_ = now + radioResetMs;
            startStation_ = true;
        }
        return NetworkState::Wifi;
    }

    wifiAt_ = now + wifiTimeoutMs;
    if (!clockStarted_) {
        configTime(0, 0, ntpPrimary_, ntpFallback_);
        clockStarted_ = true;
    }
    return time(nullptr) >= minimumUnixTime ? NetworkState::Ready : NetworkState::Clock;
}

BoardDiagnostics BoardRuntime::diagnostics() {
    BoardDiagnostics result{};
    result.cpuMHz = ESP.getCpuFreqMHz();
    result.freeHeapBytes = ESP.getFreeHeap();
#if defined(ESP8266)
    result.resetReason = ESP.getResetInfoPtr()->reason;
    result.flashBytes = ESP.getFlashChipRealSize();
    result.largestHeapBlockBytes = ESP.getMaxFreeBlockSize();
#else
    result.resetReason = static_cast<uint32_t>(esp_reset_reason());
    result.flashBytes = ESP.getFlashChipSize();
    result.psramBytes = ESP.getPsramSize();
    result.largestHeapBlockBytes = ESP.getMaxAllocHeap();
#endif
    return result;
}

} // namespace firmware
