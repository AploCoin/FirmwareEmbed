#include <BoardRuntime.h>
#include <SessionProvisioner.h>
#include <assert.h>
#include <iostream>

TestSerial Serial;
unsigned yields = 0;
unsigned delays = 0;
struct Imported {
    std::string text;
    unsigned calls = 0;
    bool allow = true;
};
bool importer(const char* hex, void* context) {
    auto& out = *static_cast<Imported*>(context);
    ++out.calls;
    out.text = hex;
    return out.allow;
}

void check(const std::string& line, bool accepted, unsigned calls = 0, bool allow = true) {
    Serial = TestSerial();
    Serial.input = line;
    Serial.waitOnce = true;
    Imported out;
    out.allow = allow;
    assert(SessionProvisioner::importFromSerial(importer, &out) == accepted);
    assert(out.calls == calls);
    assert(yields && delays);
    if (calls) {
        assert(out.text.size() == 64);
        assert(Serial.output.find(out.text) == std::string::npos);
    }
}

int main() {
    // Synthetic input only. Tests never generate or store an operational key.
    const std::string hex(64, 'a');
    check(hex + "\n", true, 1);
    check(hex + "\r\n", true, 1);
    check(hex.substr(1) + "\n", false);
    check(hex + "b\n", false);
    check(std::string(63, 'a') + "g\n", false);
    check(hex.substr(1) + "b\ba\n", true, 1);
    check(hex + "\n", false, 1, false);
    std::cout << "Serial provisioning: 7 cases PASS; no key echo; cooperative wait.\n";
}
