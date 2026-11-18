#include <Arduino.h>
#include <SessionKeys.h>
#include <SessionProvisioner.h>
#include <cassert>
#include <iostream>

TestSerial Serial;
unsigned yields = 0;
unsigned delays = 0;

int main() {
    aplo::SessionKey key;
    Serial.input =
        std::string(64, '0') + "\n" + std::string(64, 'b') + "\n" + std::string(64, 'a') + "\n";
    SessionProvisioner::importApprovedSession(key, "0x2222222222222222222222222222222222222222");
    assert(key.imports == 3 && key.clears == 2 && key.firstByte == 0xaa);
    assert(Serial.output.find(std::string(64, 'a')) == std::string::npos);
    assert(Serial.output.find(std::string(64, 'b')) == std::string::npos);
    std::cout << "Session import: rejected scalar and signer mismatch clear key, then approved "
                 "import PASS "
                 "(scripted key boundary).\n";
}
