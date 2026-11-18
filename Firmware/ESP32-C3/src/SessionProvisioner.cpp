#include "SessionProvisioner.h"
#include "BoardRuntime.h"
#include <ctype.h>

namespace {
void wipe(char* buffer, size_t size) {
    volatile char* bytes = buffer;
    for (size_t index = 0; index < size; ++index) {
        bytes[index] = 0;
    }
}
} // namespace

bool SessionProvisioner::importFromSerial(ImportKey importKey, void* context) {
    Serial.println(F("After reboot: reconcile any prior transaction hash/nonce on the owner host "
                     "before importing."));
    Serial.println(
        F("Provision restricted SESSION key: 64 hex characters + newline (not echoed)."));
    Serial.println(F("Use a trusted local USB connection; do not enable terminal logging."));
    char buffer[65] = {};
    size_t length = 0;
    bool invalid = false;
    while (true) {
        while (Serial.available()) {
            const int next = Serial.read();
            if (next == '\r') {
                continue;
            }
            if (next == '\n') {
                const bool accepted = !invalid && length == 64 && importKey(buffer, context);
                wipe(buffer, sizeof(buffer));
                return accepted;
            }
            if (next == 8 || next == 127) {
                if (length) {
                    --length;
                    buffer[length] = 0;
                }
                continue;
            }
            if (!isxdigit(static_cast<unsigned char>(next)) || length == 64) {
                invalid = true;
                continue;
            }
            buffer[length] = static_cast<char>(next);
            ++length;
        }
        delay(10);
        BoardRuntime::cooperate();
    }
}
