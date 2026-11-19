#include <Arduino.h>
#include <MiningEmbed.h>
#include <Web3.h>
#include <pgmspace.h>

#include "BoardRuntime.h"
#include "FirmwareConfig.h"

#if __has_include("AploTrustProfile.h")
#include "AploTrustProfile.h"
#define RPC_PRIMARY APLO_TRUST_PRIMARY_URL
#define RPC_FALLBACK APLO_TRUST_FALLBACK_URL
#define RPC_PRIMARY_CA APLO_TRUST_PRIMARY_CA
#define RPC_FALLBACK_CA APLO_TRUST_FALLBACK_CA
#else
#define RPC_PRIMARY APLO_RPC_PRIMARY
#define RPC_FALLBACK APLO_RPC_FALLBACK
#define RPC_PRIMARY_CA APLO_RPC_PRIMARY_CA
#define RPC_FALLBACK_CA APLO_RPC_FALLBACK_CA
#endif

namespace {

constexpr uint8_t wifiConnectAttempts = 3;
constexpr uint8_t wifiReconnectAttempts = 2;
constexpr unsigned long wifiTimeoutMs = 45000;
constexpr unsigned long wifiRetryDelayMs = 5000;

constexpr uint32_t hashesPerCycle = 2000;
constexpr unsigned long miningIntervalMs = 1000;

aplo::SessionKey sessionKey;
miningembed::MiningEmbed* miner = nullptr;

bool hasRpcConfiguration(const char* url, const char* ca) {
    if (ca == nullptr) {
        return false;
    }

    // The CA can live in PROGMEM on ESP8266.
    if (pgm_read_byte(ca) == 0) {
        return false;
    }

    return Web3::isHttpsUrl(url);
}

bool readConfiguration(
    const char* primaryCa,
    const char* fallbackCa,
    miningembed::MiningSession& session
) {
    if (APLO_WIFI_SSID[0] == '\0') {
        Serial.println(F("Set the Wi-Fi SSID in DeviceConfig.h."));
        return false;
    }

    if (!hasRpcConfiguration(RPC_PRIMARY, primaryCa)) {
        Serial.println(F("Set a valid primary HTTPS RPC URL and its CA."));
        return false;
    }

    const bool hasFallbackRpc = RPC_FALLBACK[0] != '\0';
    if (hasFallbackRpc && !hasRpcConfiguration(RPC_FALLBACK, fallbackCa)) {
        Serial.println(F("Set a valid fallback HTTPS RPC URL and its CA."));
        return false;
    }

    const bool sessionIsValid = miningembed::parseMiningSession(
        APLO_OWNER_ADDRESS,
        APLO_SESSION_ADDRESS,
        APLO_AUTHORIZED_CHAIN_ID,
        APLO_AUTHORIZED_GAPLO_BUDGET,
        APLO_SESSION_EXPIRY_BLOCK,
        session
    );

    if (!sessionIsValid) {
        Serial.println(F("Set the confirmed mining session metadata in DeviceConfig.h."));
        return false;
    }

    return true;
}

void waitForNetwork() {
    for (;;) {
        const bool wifiConnected = BoardRuntime::connectWifi(wifiConnectAttempts, wifiTimeoutMs);
        if (wifiConnected && BoardRuntime::synchronizeTime(APLO_NTP_PRIMARY, APLO_NTP_FALLBACK)) {
            return;
        }

        Serial.println(F("Waiting for Wi-Fi and certificate time."));
        delay(wifiRetryDelayMs);
    }
}

void reportShare(const miningembed::Submission& share, bool pollingReceipt) {
    using miningembed::SubmissionState;

    const __FlashStringHelper* message = nullptr;
    switch (share.state) {
    case SubmissionState::Confirmed:
        message = F("Receipt status=1, confirmed: ");
        break;
    case SubmissionState::Reverted:
        message = F("Receipt status=0, reverted: ");
        break;
    case SubmissionState::Broadcast:
        message = pollingReceipt ? nullptr : F("Broadcast, waiting for receipt: ");
        break;
    case SubmissionState::Uncertain:
        message = pollingReceipt ? nullptr : F("Broadcast uncertain, tracking hash: ");
        break;
    case SubmissionState::NotSent:
    case SubmissionState::Pending:
        break;
    }

    if (message == nullptr) {
        return;
    }

    Serial.print(message);
    Serial.println(share.hash.c_str());
}

void runMiningCycle() {
    if (miner == nullptr) {
        return;
    }

    if (!BoardRuntime::ensureWifiConnected(
            wifiReconnectAttempts,
            wifiTimeoutMs,
            wifiRetryDelayMs
        )) {
        return;
    }

    const bool pollingReceipt = miner->unresolved();
    miningembed::Submission share;
    const auto status = miner->step(hashesPerCycle, share);

    if (status != miningembed::Status::Ok && status != miningembed::Status::NoCandidate) {
        Serial.println(miningembed::statusName(status));
    }

    reportShare(share, pollingReceipt);
}

} // namespace

void setup() {
    BoardRuntime::beginSerial();

    miningembed::MiningSession session;
    const char* const primaryCa = RPC_PRIMARY_CA;
    const char* const fallbackCa = RPC_FALLBACK_CA;
    if (!readConfiguration(primaryCa, fallbackCa, session)) {
        return;
    }

    BoardRuntime::beginWifi(APLO_WIFI_SSID, APLO_WIFI_PASSWORD);
    waitForNetwork();

    // ESP8266 allocates its BearSSL client only after association and DHCP.
    static Web3 web3(RPC_PRIMARY, RPC_FALLBACK);
    web3.setCertificate(primaryCa, fallbackCa);

    Serial.println(
        F("After reset, reconcile any prior share on the owner machine before importing.")
    );

    do {
        Serial.println(
            F("Provision restricted SESSION key: 64 hex characters + newline (not echoed).")
        );
    } while (!sessionKey.ReadFromSerial(Serial, session.signer));

    Serial.println(F("Session key loaded. Starting mining."));

    static aplo::SessionKeys rpc(web3);
    static miningembed::MiningEmbed application(rpc, sessionKey, session);
    miner = &application;
}

void loop() {
    runMiningCycle();
    delay(miningIntervalMs);
}
