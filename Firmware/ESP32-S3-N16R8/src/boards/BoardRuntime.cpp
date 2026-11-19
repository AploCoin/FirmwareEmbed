#include "BoardRuntime.h"

namespace {

constexpr unsigned long wifiPollIntervalMs = 250;
constexpr unsigned long wifiAttemptPauseMs = 1000;
constexpr unsigned long wifiRadioResetDelayMs = 250;
constexpr time_t minimumTlsUnixTime = 1700000000;
constexpr unsigned long timeSyncTimeoutMs = 30000;
constexpr unsigned long timePollIntervalMs = 250;

unsigned long lastWifiRetryMs = 0;
const char* wifiSsid = nullptr;
const char* wifiPassword = nullptr;

bool wifiConnected() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    return WiFi.localIP() != IPAddress(static_cast<uint32_t>(0));
}

bool connectWifiOnce(unsigned long timeoutMs) {
    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
    delay(wifiRadioResetDelayMs);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    WiFi.begin(wifiSsid, wifiPassword);

    const unsigned long startedAt = millis();
    while (millis() - startedAt < timeoutMs) {
        if (wifiConnected()) {
            return true;
        }

        delay(wifiPollIntervalMs);
    }

    return false;
}

} // namespace

void BoardRuntime::beginSerial() {
    Serial.begin(115200);
    delay(1000);
}

void BoardRuntime::beginWifi(const char* ssid, const char* password) {
    wifiSsid = ssid;
    wifiPassword = password;
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
            delay(wifiAttemptPauseMs);
        }
    }

    Serial.print(F("WiFi failed, status="));
    Serial.println(WiFi.status());
    return false;
}

bool BoardRuntime::ensureWifiConnected(
    uint8_t maxAttempts,
    unsigned long timeoutMs,
    unsigned long retryDelayMs
) {
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

void BoardRuntime::cooperate() {
    delay(0);
}

bool BoardRuntime::synchronizeTime(const char* primary, const char* fallback) {
    configTime(0, 0, primary, fallback);

    const unsigned long startedAt = millis();
    while (time(nullptr) < minimumTlsUnixTime && millis() - startedAt < timeSyncTimeoutMs) {
        delay(timePollIntervalMs);
        cooperate();
    }

    return time(nullptr) >= minimumTlsUnixTime;
}
