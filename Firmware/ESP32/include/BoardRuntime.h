#pragma once

#include <AploPlatform.h>
#include <time.h>

namespace BoardRuntime {

void beginSerial();

void beginWifi(const char* ssid, const char* password);
bool connectWifi(uint8_t maxAttempts, unsigned long timeoutMs);
bool ensureWifiConnected(uint8_t maxAttempts, unsigned long timeoutMs, unsigned long retryDelayMs);

void cooperate();
bool synchronizeTime(const char* primary, const char* fallback);

} // namespace BoardRuntime
