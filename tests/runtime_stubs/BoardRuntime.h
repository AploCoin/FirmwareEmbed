#pragma once
#include <Arduino.h>
#include <stdint.h>
namespace BoardRuntime {
void beginSerial();
void beginWifi(const char*, const char*);
bool connectWifi(uint8_t, unsigned long);
bool ensureWifiConnected(uint8_t, unsigned long, unsigned long);
bool synchronizeTime(const char* primary, const char* fallback);
void cooperate();
void reportMemory(const char*);
} // namespace BoardRuntime
