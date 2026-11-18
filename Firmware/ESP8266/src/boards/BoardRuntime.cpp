#include "BoardRuntime.h"

static WiFiEventHandler wifiDisconnectHandler;
static unsigned long lastWifiRetryMs = 0;
static const char* wifiSsid = nullptr;
static const char* wifiPassword = nullptr;

static bool wifiConnected() {
    return WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(static_cast<uint32_t>(0));
}

void BoardRuntime::beginWifi(const char* ssid, const char* password) {
    wifiSsid = ssid;
    wifiPassword = password;
    wifiDisconnectHandler =
        WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
            Serial.print(F("WiFi disconnected, reason="));
            Serial.println(event.reason);
        });
}

static bool connectWifiOnce(unsigned long timeoutMs) {
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.mode(WIFI_STA);
    WiFi.config(IPAddress(static_cast<uint32_t>(0)), IPAddress(static_cast<uint32_t>(0)),
                IPAddress(static_cast<uint32_t>(0)));
    const unsigned long effectiveTimeout = timeoutMs < 45000UL ? 45000UL : timeoutMs;

    WiFi.begin(wifiSsid, wifiPassword);
    const unsigned long startedAt = millis();
    while (millis() - startedAt < effectiveTimeout) {
        if (wifiConnected()) {
            return true;
        }
        delay(250);
        yield();
    }
    return false;
}

bool BoardRuntime::connectWifi(uint8_t maxAttempts, unsigned long timeoutMs) {
    if (wifiConnected()) {
        return true;
    }

    for (uint8_t attempt = 0; attempt < maxAttempts; ++attempt) {
        if (connectWifiOnce(timeoutMs)) {
            Serial.print(F("WiFi connected: "));
            Serial.println(WiFi.localIP());
            lastWifiRetryMs = 0;
            return true;
        }
        if (attempt + 1 < maxAttempts) {
            delay(1000);
        }
    }

    Serial.print(F("WiFi failed, status="));
    Serial.println(WiFi.status());
    return false;
}

bool BoardRuntime::ensureWifiConnected(uint8_t maxAttempts, unsigned long timeoutMs,
                                       unsigned long retryDelayMs) {
    if (wifiConnected()) {
        return true;
    }
    const unsigned long now = millis();
    if (lastWifiRetryMs != 0 && now - lastWifiRetryMs < retryDelayMs) {
        return false;
    }
    lastWifiRetryMs = now;
    Serial.println(F("WiFi disconnected; pausing work and reconnecting..."));
    return BoardRuntime::connectWifi(maxAttempts, timeoutMs);
}

void BoardRuntime::beginSerial() {
    Serial.begin(115200);
    delay(1000);
}

void BoardRuntime::cooperate() {
    delay(0);
}

bool BoardRuntime::synchronizeTime(const char* primary, const char* fallback) {
    configTime(0, 0, primary, fallback);
    const unsigned long start = millis();
    while (time(nullptr) < 1700000000 && millis() - start < 30000UL) {
        delay(250);
        cooperate();
    }
    return time(nullptr) >= 1700000000;
}
