#include <Arduino.h>
#include <SessionKeys.h>
#include "BoardRuntime.h"
#include "DeviceSetup.h"
#include "FirmwareConfig.h"
#include "MiningApplication.h"
#include "SessionProvisioner.h"

namespace {
// These outlive the application's borrowed transport and signing references.
aplo::SessionKey sessionKey;
bool ready = false;
} // namespace

void setup() {
    BoardRuntime::beginSerial();
    Serial.println(F("\n=== Aplo Mining Firmware / restricted session ==="));
    miningembed::MiningSession session;
    if (!DeviceSetup::readSession(session)) {
        return;
    }
    Web3& client = DeviceSetup::connect();
    SessionProvisioner::importApprovedSession(sessionKey, session.signer.c_str());
    Serial.print(F("Session signer: "));
    Serial.println(sessionKey.Address().c_str());
    Serial.print(F("Funding/reward owner: "));
    Serial.println(session.owner.c_str());
    MiningApplication::begin(client, sessionKey, session);
    ready = true;
}

void loop() {
    if (ready && BoardRuntime::ensureWifiConnected(FirmwareConfig::wifiReconnectAttempts,
                                                   FirmwareConfig::wifiAttemptTimeoutMs,
                                                   FirmwareConfig::wifiRetryDelayMs)) {
        MiningApplication::tick();
    }
    BoardRuntime::cooperate();
    delay(FirmwareConfig::cycleDelayMs);
}
