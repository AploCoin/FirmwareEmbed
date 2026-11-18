#include "SessionProvisioner.h"
#include <Arduino.h>
#include <MiningWork.h>
#include <SessionKeys.h>
#include <cstring>

namespace {
struct ApprovedSession {
    aplo::SessionKey& key;
    const char* address;
};

bool importSession(const char* hex, void* context) {
    ApprovedSession& session = *static_cast<ApprovedSession*>(context);
    uint8_t secret[32] = {};
    const bool decoded = miningembed::decodeHex(hex, secret, sizeof(secret), sizeof(secret));
    const bool imported = decoded && session.key.Import(secret);
    volatile uint8_t* cursor = secret;
    for (size_t i = 0; i < sizeof(secret); ++i) {
        cursor[i] = 0;
    }
    uint8_t expected[20];
    uint8_t actual[20];
    const bool matches =
        imported &&
        miningembed::decodeHex(session.address, expected, sizeof(expected), sizeof(expected)) &&
        miningembed::decodeHex(session.key.Address().c_str(), actual, sizeof(actual),
                               sizeof(actual)) &&
        std::memcmp(expected, actual, sizeof(expected)) == 0;
    if (!matches) {
        session.key.Clear();
    }
    return matches;
}
} // namespace

void SessionProvisioner::importApprovedSession(aplo::SessionKey& key, const char* address) {
    ApprovedSession session = {key, address};
    while (!importFromSerial(importSession, &session)) {
        Serial.println(
            F("Session key invalid or differs from the approved session address. Retry."));
    }
}
