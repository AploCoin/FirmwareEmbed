#include <Arduino.h>
#include <AploPlatform.h>
#include <Web3.h>
#include <MiningEmbed.h>
#include <memory>
#include <new>
#include <time.h>
#include "DeviceConfig.h"
#include "SigningLatch.h"

namespace {
SigningLatch latch;
aplo::SessionKey key;
std::unique_ptr<Web3> web3;
std::unique_ptr<aplo::SessionKeys> rpc;
std::unique_ptr<miningembed::MiningEmbed> miner;
bool halted = false;
bool clockStarted = false;
uint32_t nextStep = 0;
uint32_t wifiAt = 0;

void halt(const __FlashStringHelper* reason) {
    halted = true;
    Serial.println(reason);
    key.Clear();
}

bool startMiner() {
    if (!Web3::isHttpsUrl(device::kPrimaryUrl) ||
        !Web3::isValidCertificate(device::kPrimaryCa) ||
        (device::kFallbackUrl[0] &&
         (!Web3::isHttpsUrl(device::kFallbackUrl) ||
          !Web3::isValidCertificate(device::kFallbackCa))) ||
        !key.Import(device::kPrivateKey)) {
        return false;
    }
    web3.reset(new (std::nothrow) Web3(device::kPrimaryUrl, device::kFallbackUrl));
    if (!web3) return false;
    web3->setCertificate(device::kPrimaryCa,
                         device::kFallbackUrl[0] ? device::kFallbackCa : nullptr);
    rpc.reset(new (std::nothrow) aplo::SessionKeys(*web3));
    if (!rpc) return false;
    if (device::kMode == device::Mode::Direct) {
        uint256_t chain;
        if (!miningembed::parseMiningChainId(device::kChainId, chain)) return false;
        miner.reset(new (std::nothrow) miningembed::MiningEmbed(
            *rpc, key, chain, SigningLatch::writePending, &latch));
    } else {
        miningembed::MiningSession session;
        if (!miningembed::parseMiningSession(device::kOwner, device::kSigner,
                device::kChainId, device::kGaploBudget, device::kExpiry, session)) return false;
        miner.reset(new (std::nothrow) miningembed::MiningEmbed(
            *rpc, key, session, SigningLatch::writePending, &latch));
    }
    return bool(miner);
}
} // namespace

void setup() {
    Serial.begin(115200);
    Serial.println(F("APLO-LITE-LAYOUT:" APLO_FLASH_LAYOUT));
    if (!latch.begin()) {
        halt(F("RECOVERY REQUIRED: signing latch contains residue or layout is invalid"));
        return;
    }
    if (!device::kConfigured) {
        halt(F("Unconfigured: edit private compile-time DeviceConfig and rebuild"));
        return;
    }
    if (!device::kSsid[0] || !device::kHashesPerStep ||
        device::kHashesPerStep > 2000 || device::kRetryMs > 60000) {
        halt(F("Invalid compile-time configuration"));
        return;
    }
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(device::kSsid, device::kPassword);
    wifiAt = millis();
}

void loop() {
    if (halted) { delay(25); return; }
    if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(uint32_t(0))) {
        clockStarted = false;
        if (millis() - wifiAt >= 45000) {
            WiFi.disconnect();
            WiFi.begin(device::kSsid, device::kPassword);
            wifiAt = millis();
        }
        delay(10);
        return;
    }
    wifiAt = millis();
    if (!clockStarted) {
        configTime(0, 0, device::kNtpPrimary, device::kNtpFallback);
        clockStarted = true;
    }
    if (time(nullptr) < 1700000000) { delay(10); return; }
    // Allocate the ESP8266 BearSSL client only after DHCP has completed.
    if (!miner && !startMiner()) { halt(F("Invalid key, authorization or HTTPS trust")); return; }
    if (int32_t(millis() - nextStep) < 0) { delay(1); return; }
    miningembed::Submission submission;
    const auto status = miner->step(device::kHashesPerStep, submission, millis());
    if (submission.state == miningembed::SubmissionState::Confirmed ||
        submission.state == miningembed::SubmissionState::Reverted) {
        Serial.println(submission.state == miningembed::SubmissionState::Confirmed
            ? F("confirmed") : F("reverted"));
    }
    uint32_t wait = device::kRetryMs;
    if (miner->unresolved()) wait = 1000;
    else if (status == miningembed::Status::NoCandidate ||
             status == miningembed::Status::StaleChallenge ||
             status == miningembed::Status::Ok) wait = 0;
    nextStep = millis() + wait;
    delay(1);
}
