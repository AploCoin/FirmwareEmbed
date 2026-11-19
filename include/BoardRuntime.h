#pragma once

#include <stdint.h>

namespace firmware {

enum class NetworkState { Wifi, Clock, Ready };

class BoardRuntime {
public:
    void
    begin(const char* ssid, const char* password, const char* ntpPrimary, const char* ntpFallback);
    NetworkState poll();

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
