#include "DeviceSetup.h"
#include "BoardRuntime.h"
#include "ConfigValues.h"
#include "FirmwareConfig.h"

namespace {
void waitForWifi() {
    while (!BoardRuntime::connectWifi(FirmwareConfig::wifiConnectAttempts,
                                      FirmwareConfig::wifiAttemptTimeoutMs)) {
        Serial.println(F("WiFi unavailable during setup; retrying in 5 seconds..."));
        delay(FirmwareConfig::wifiRetryDelayMs);
    }
}

void prepareTls(Web3& client) {
#ifdef APLO_TRUST_PROFILE_HEADER
    client.setAutoCertificate();
#else
    if (*FirmwareConfig::rpcRootCa) {
        client.setCertificate(FirmwareConfig::rpcRootCa);
    } else {
        client.setAutoCertificate();
    }
#endif
    while (
        !BoardRuntime::synchronizeTime(FirmwareConfig::ntpPrimary, FirmwareConfig::ntpFallback)) {
        Serial.println(F("TLS paused: valid system time unavailable; retrying time sync."));
        waitForWifi();
    }
}
} // namespace

bool DeviceSetup::readSession(miningembed::MiningSession& session) {
    const FirmwareConfig::PublicConfig config = {FirmwareConfig::wifiSsid,
                                                 FirmwareConfig::rpcPrimary,
                                                 FirmwareConfig::rpcFallback,
                                                 FirmwareConfig::ownerAddress,
                                                 FirmwareConfig::sessionAddress,
                                                 FirmwareConfig::authorizedChainId,
                                                 FirmwareConfig::authorizedGaploBudget,
                                                 FirmwareConfig::sessionExpiryBlock};
    if (!FirmwareConfig::parsePublicConfig(config, session.chainId, session.gaploBudget)) {
        Serial.println(F("Configuration missing/invalid. Set WiFi, HTTPS RPC and public session "
                         "metadata in DeviceConfig.h."));
        return false;
    }
    session.owner = config.owner;
    session.signer = config.signer;
    session.expiry = config.expiry;
    return true;
}

Web3& DeviceSetup::connect() {
    BoardRuntime::beginWifi(FirmwareConfig::wifiSsid, FirmwareConfig::wifiPassword);
    waitForWifi();
    BoardRuntime::reportMemory("after WiFi / before Web3");

    // Construct only after DHCP: ESP8266 allocates its single BearSSL client here.
    static Web3 client(FirmwareConfig::rpcPrimary, FirmwareConfig::rpcFallback);
    BoardRuntime::reportMemory("after Web3 allocation");
    prepareTls(client);
    BoardRuntime::reportMemory("after TLS/time setup");
    return client;
}
