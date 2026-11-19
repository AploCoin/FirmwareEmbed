#pragma once

#include <stdint.h>

namespace firmware {

enum class NetworkState { Wifi, Clock, Ready };

struct BoardDiagnostics {
    uint32_t resetReason;
    uint32_t cpuMHz;
    uint32_t flashBytes;
    uint32_t psramBytes;
    uint32_t freeHeapBytes;
    uint32_t largestHeapBlockBytes;
};

class BoardRuntime {
public:
    void
    begin(const char* ssid, const char* password, const char* ntpPrimary, const char* ntpFallback);
    NetworkState poll();
    static BoardDiagnostics diagnostics();

private:
    const char* ssid_ = nullptr;
    const char* password_ = nullptr;
    const char* ntpPrimary_ = nullptr;
    const char* ntpFallback_ = nullptr;
    uint32_t wifiAt_ = 0;
    bool startStation_ = true;
    bool clockStarted_ = false;
};

} // namespace firmware
