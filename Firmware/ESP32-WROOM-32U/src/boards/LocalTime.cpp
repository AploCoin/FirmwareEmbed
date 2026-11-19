#include <Arduino.h>

#include "FirmwareConfig.h"
#include "LocalTime.h"

#if defined(APLO_LOCAL_NTP_HOST) && defined(ESP32)

#include <WiFi.h>
#include <WiFiUdp.h>

#include <cstring>
#include <esp_system.h>
#include <sys/time.h>

#ifndef APLO_LOCAL_NTP_PORT
#define APLO_LOCAL_NTP_PORT 12300
#endif

namespace {

constexpr int sntpPacketSize = 48;
constexpr uint8_t sntpV4ClientHeader = 0x23;
constexpr uint8_t serverMode = 4;
constexpr uint8_t unsynchronizedLeapIndicator = 3;
constexpr uint8_t maxStratum = 15;

constexpr size_t originateTimestampOffset = 24;
constexpr size_t transmitTimestampOffset = 40;
constexpr size_t timestampSize = 8;

constexpr uint32_t ntpToUnixEpochSeconds = 2208988800UL;
constexpr uint32_t minimumUnixTime = 1700000000UL;

constexpr unsigned maxAttempts = 3;
constexpr unsigned long replyTimeoutMs = 3000;
constexpr unsigned long pollIntervalMs = 25;

} // namespace

bool synchronizeLocalTime() {
    IPAddress server;
    if (!server.fromString(APLO_LOCAL_NTP_HOST)) {
        return false;
    }

    WiFiUDP udp;
    if (!udp.begin(0)) {
        return false;
    }

    uint8_t request[sntpPacketSize] = {};
    request[0] = sntpV4ClientHeader;
    uint8_t* const requestTimestamp = request + transmitTimestampOffset;
    esp_fill_random(requestTimestamp, timestampSize);

    for (unsigned attempt = 0; attempt < maxAttempts; ++attempt) {
        udp.beginPacket(server, APLO_LOCAL_NTP_PORT);
        udp.write(request, sizeof(request));
        udp.endPacket();

        const unsigned long startedAt = millis();
        while (millis() - startedAt < replyTimeoutMs) {
            const int packetSize = udp.parsePacket();
            if (packetSize < sntpPacketSize || udp.remoteIP() != server ||
                udp.remotePort() != APLO_LOCAL_NTP_PORT) {
                delay(pollIntervalMs);
                continue;
            }

            uint8_t response[sntpPacketSize];
            if (udp.read(response, sizeof(response)) != sntpPacketSize) {
                continue;
            }

            const uint8_t mode = response[0] & 0x07;
            const uint8_t leapIndicator = response[0] >> 6;
            const uint8_t stratum = response[1];
            if (mode != serverMode || leapIndicator == unsynchronizedLeapIndicator ||
                stratum == 0 || stratum > maxStratum) {
                continue;
            }

            const uint8_t* const originateTimestamp = response + originateTimestampOffset;
            if (std::memcmp(originateTimestamp, requestTimestamp, timestampSize) != 0) {
                continue;
            }

            const uint8_t* const transmitTimestamp = response + transmitTimestampOffset;
            const uint32_t seconds = (static_cast<uint32_t>(transmitTimestamp[0]) << 24) |
                                     (static_cast<uint32_t>(transmitTimestamp[1]) << 16) |
                                     (static_cast<uint32_t>(transmitTimestamp[2]) << 8) |
                                     static_cast<uint32_t>(transmitTimestamp[3]);
            if (seconds < ntpToUnixEpochSeconds + minimumUnixTime) {
                continue;
            }

            timeval now{static_cast<time_t>(seconds - ntpToUnixEpochSeconds), 0};
            const bool synchronized = settimeofday(&now, nullptr) == 0;
            udp.stop();

            if (synchronized) {
                Serial.print(F("Local SNTP synchronized; Unix time="));
                Serial.println(static_cast<unsigned long>(time(nullptr)));
            }

            return synchronized;
        }
    }

    udp.stop();
    return false;
}

#endif
