#include "BoardRuntime.h"

#include <AploPlatform.h>
#include <time.h>

#include <esp_system.h>

namespace firmware {
namespace {
constexpr uint32_t wifiTimeoutMs = 45000;
constexpr uint32_t radioResetMs = 250;
constexpr time_t minimumUnixTime = 1700000000;
} // namespace

void BoardRuntime::begin(const Config& config) {
    ssid_ = config.ssid.c_str();
    password_ = config.password.c_str();
    ntpPrimary_ = config.ntpPrimary.c_str();
    ntpFallback_ = config.ntpFallback.empty() ? nullptr : config.ntpFallback.c_str();
    WiFi.persistent(false);
    wifiAt_ = millis();
}

NetworkState BoardRuntime::poll() {
    const uint32_t now = millis();
    if (startStation_ && static_cast<int32_t>(now - wifiAt_) >= 0) {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);
        WiFi.setSleep(false);
        WiFi.begin(ssid_, password_);
        startStation_ = false;
        wifiAt_ = now + wifiTimeoutMs;
    }

    if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(static_cast<uint32_t>(0))) {
        clockStarted_ = false;
        if (!startStation_ && static_cast<int32_t>(now - wifiAt_) >= 0) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
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
    result.resetReason = static_cast<uint32_t>(esp_reset_reason());
    result.flashBytes = ESP.getFlashChipSize();
    result.psramBytes = ESP.getPsramSize();
    result.largestHeapBlockBytes = ESP.getMaxAllocHeap();
    return result;
}

} // namespace firmware
