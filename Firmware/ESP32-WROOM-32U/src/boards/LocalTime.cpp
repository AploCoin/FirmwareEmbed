#include <Arduino.h>
#include "FirmwareConfig.h"
#include "LocalTime.h"

#if defined(APLO_LOCAL_NTP_HOST) && defined(ESP32)
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_system.h>
#include <sys/time.h>

#ifndef APLO_LOCAL_NTP_PORT
#define APLO_LOCAL_NTP_PORT 12300
#endif

bool synchronizeLocalTime() {
    IPAddress server;
    if (!server.fromString(APLO_LOCAL_NTP_HOST)) {
        return false;
    }
    WiFiUDP udp;
    if (!udp.begin(0)) {
        return false;
    }
    uint8_t request[48] = {};
    request[0] = 0x23; // SNTPv4 client.
    esp_fill_random(request + 40, 8);
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        udp.beginPacket(server, APLO_LOCAL_NTP_PORT);
        udp.write(request, sizeof(request));
        udp.endPacket();
        const unsigned long start = millis();
        while (millis() - start < 3000UL) {
            if (udp.parsePacket() >= 48 && udp.remoteIP() == server &&
                udp.remotePort() == APLO_LOCAL_NTP_PORT) {
                uint8_t response[48];
                if (udp.read(response, sizeof(response)) != sizeof(response) ||
                    (response[0] & 7) != 4 || (response[0] >> 6) == 3 ||
                    response[1] == 0 || response[1] > 15 ||
                    memcmp(response + 24, request + 40, 8) != 0) {
                    continue;
                }
                const uint32_t seconds = (uint32_t(response[40]) << 24) |
                    (uint32_t(response[41]) << 16) | (uint32_t(response[42]) << 8) |
                    uint32_t(response[43]);
                if (seconds < 3908988800UL) { // Unix epoch >= 1700000000.
                    continue;
                }
                struct timeval now = {time_t(seconds - 2208988800UL), 0};
                const bool success = settimeofday(&now, nullptr) == 0;
                udp.stop();
                if (success) {
                    Serial.print(F("Local SNTP synchronized; Unix time="));
                    Serial.println(static_cast<unsigned long>(time(nullptr)));
                }
                return success;
            }
            delay(25);
        }
    }
    udp.stop();
    return false;
}
#endif
