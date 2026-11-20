#pragma once

#include <provisioning/FirmwareRuntime.h>

namespace firmware {

using miningembed::provisioning::BoardDiagnostics;
using miningembed::provisioning::Config;
using miningembed::provisioning::NetworkState;

miningembed::provisioning::FlashBackend& flashStorage();

class BoardRuntime {
public:
    void begin(const Config& config);
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
