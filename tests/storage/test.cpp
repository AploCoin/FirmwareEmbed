#include "BoardRuntime.h"
#include "DeviceConfig.h"
#include "SerialConfig.h"
#include <Arduino.h>
#include <trezor/sha3.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
uint32_t testMillis = 0;
NativeSerial Serial;

// Deterministic fixture randomness for public-key multiplication blinding only.
extern "C" uint32_t random32() {
    static uint32_t state = 0x735a29d1;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

namespace aplo {
bool SessionPlatformRandom(uint8_t*, size_t) {
    return false;
}
} // namespace aplo

namespace {
using namespace firmware;
const std::string token(64, '1');
const std::string other(64, '2');
const std::string pem = "-----BEGIN CERTIFICATE-----\nFAKE\n-----END CERTIFICATE-----\n";

std::string hex(const std::string& bytes) {
    const char* digits = "0123456789abcdef";
    std::string s;
    for (uint8_t b : bytes) {
        s += digits[b >> 4];
        s += digits[b & 15];
    }
    return s;
}

class MemoryFlash : public FlashBackend {
public:
    std::string slots[2];
    bool exists[2] = {};
    unsigned writes = 0;
    bool mount = true, failBefore = false, failAfter = false, failReadback = false;

    bool begin() override {
        return mount;
    }

    Read read(unsigned slot, std::string& out) override {
        if (failReadback) {
            return Read::Error;
        }
        if (!exists[slot]) {
            return Read::Missing;
        }
        out = slots[slot];
        return Read::Ok;
    }

    bool replace(unsigned slot, const std::string& bytes) override {
        ++writes;
        if (failBefore) {
            return false;
        }
        slots[slot] = bytes;
        exists[slot] = true;
        return !failAfter;
    }
};

class Port : public Stream {
public:
    std::deque<uint8_t> in;
    std::string out;

    int available() override {
        return in.size();
    }

    int read() override {
        if (in.empty()) {
            return -1;
        }
        int c = in.front();
        in.pop_front();
        return c;
    }

    size_t write(uint8_t c) override {
        out += char(c);
        return 1;
    }

    void feed(const std::string& text) {
        for (uint8_t b : text) {
            in.push_back(b);
        }
    }

    std::string run(SerialConfig& serial, const std::string& input, const char* state = "mining") {
        out.clear();
        feed(input);
        while (!in.empty()) {
            serial.poll(*this, state);
        }
        return out;
    }
};

Config configured() {
    Config c;
    c.enabled = true;
    c.mode = Mode::Direct;
    c.ssid = "fixture";
    c.password = "password-secret";
    c.primaryUrl = "https://private.invalid/secret-path";
    c.primaryCa = pem;
    c.keyPresent = true;
    c.key[31] = 1;
    aplo::SessionKey key;
    assert(key.Import(c.key));
    c.owner = c.signer = key.Address();
    assert(DeviceConfig::tokenVerifier(token, c.operatorVerifier));
    assert(DeviceConfig::validate(c));
    return c;
}

MemoryFlash enrolled() {
    MemoryFlash flash;
    DeviceConfig store(&flash);
    assert(store.begin());
    assert(!store.ready() && !store.enrolled());
    assert(!store.enroll(std::string(64, '0')));
    assert(store.enroll(token));
    assert(store.restartRequested());
    assert(!store.ready());
    return flash;
}

MemoryFlash ready() {
    MemoryFlash flash = enrolled();
    DeviceConfig store(&flash);
    assert(store.begin());
    Config c = configured();
    assert(store.authenticate(token));
    assert(!store.authenticate(other));
    assert(store.commit(c));
    assert(store.restartRequested());
    assert(!store.ready());
    assert(store.config().primaryCa.empty());
    clearConfig(c);
    return flash;
}

miningembed::PendingTransaction pending() {
    miningembed::PendingTransaction p;
    p.nonce = 123;
    p.raw = "0xc0";
    p.proof[0] = 1;
    p.maxFee = uint256_t(100);
    uint8_t byte = 0xc0, hash[32];
    keccak_256(&byte, 1, hash);
    p.hash = "0x" + hex(std::string(reinterpret_cast<char*>(hash), 32));
    return p;
}

void storage() {
    MemoryFlash flash = ready();
    DeviceConfig store(&flash);
    assert(store.begin());
    assert(store.ready());
    const char* ca = store.config().primaryCa.c_str();
    auto p = pending();
    auto bad = p;
    bad.hash[2] = bad.hash[2] == '1' ? '2' : '1';
    assert(!DeviceConfig::writePending(&bad, &store));
    assert(!store.pending());
    bad = p;
    bad.nonce = UINT64_MAX;
    assert(!DeviceConfig::writePending(&bad, &store));
    assert(DeviceConfig::writePending(&p, &store));
    assert(store.pending()->nonce == 123);
    Config prohibited = store.config();
    prohibited.primaryUrl = "https://changed.invalid";
    assert(!store.commit(prohibited));
    assert(!store.factoryReset("NONE"));
    assert(store.config().primaryCa.c_str() == ca);
    DeviceConfig reboot(&flash);
    assert(reboot.begin());
    assert(reboot.pending()->raw == p.raw);
    assert(DeviceConfig::writePending(reboot.pending(), &reboot)); // Borrowed self-pointer is safe.
    assert(reboot.pending()->raw == p.raw);
    assert(DeviceConfig::writePending(nullptr, &reboot));
    assert(!reboot.pending());
    DeviceConfig cleared(&flash);
    assert(cleared.begin() && !cleared.pending() && cleared.ready());
    assert(cleared.factoryReset("NONE"));
    DeviceConfig reset(&flash);
    assert(reset.begin() && !reset.enrolled() && !reset.ready());
    assert(reset.enroll(other)); // Tombstone permits explicit new owner enrollment after reboot.
    DeviceConfig reenrolled(&flash);
    assert(reenrolled.begin() && reenrolled.authenticate(other));
}

void faults() {
    auto p = pending();
    for (bool after : {false, true}) {
        MemoryFlash flash = ready();
        DeviceConfig store(&flash);
        assert(store.begin());
        flash.failBefore = !after;
        flash.failAfter = after;
        assert(!DeviceConfig::writePending(&p, &store));
        assert(store.locked() && !store.ready());
        flash.failBefore = flash.failAfter = false;
        DeviceConfig reboot(&flash);
        assert(reboot.begin());
        assert(bool(reboot.pending()) == after);
    }
    MemoryFlash flash = ready();
    DeviceConfig store(&flash);
    assert(store.begin());
    flash.failReadback = true;
    assert(!DeviceConfig::writePending(&p, &store));
    assert(store.locked());
    flash.failReadback = false;
    DeviceConfig reboot(&flash);
    assert(reboot.begin() && reboot.pending());
    // Every byte in a committed record is covered by the schema or checksum.
    for (unsigned slot = 0; slot < 2; ++slot) {
        for (size_t i = 0; i < flash.slots[slot].size(); ++i) {
            MemoryFlash damaged = flash;
            damaged.slots[slot][i] ^= 0x20;
            DeviceConfig corrupt(&damaged);
            assert(!corrupt.begin() && corrupt.locked());
            unsigned before = damaged.writes;
            assert(!corrupt.enroll(token));
            assert(damaged.writes == before);
        }
    }
    // No truncated committed record may silently roll back to an older journal.
    for (size_t prefix = 0; prefix < flash.slots[1].size(); ++prefix) {
        MemoryFlash interrupted = flash;
        interrupted.slots[1].resize(prefix);
        DeviceConfig recovered(&interrupted);
        assert(!recovered.begin() && recovered.locked());
    }
    MemoryFlash future = flash;
    future.slots[1][4] = 2;
    DeviceConfig unknown(&future);
    assert(!unknown.begin());
    unsigned writes = future.writes;
    assert(!unknown.factoryReset(p.hash) && future.writes == writes);
    for (unsigned slot = 0; slot < 2; ++slot) {
        MemoryFlash missing = flash;
        missing.exists[slot] = false;
        DeviceConfig lost(&missing);
        assert(!lost.begin() && lost.locked());
    }
    MemoryFlash oversized = flash;
    oversized.slots[0].resize(DeviceConfig::MaxRecord + 1);
    DeviceConfig large(&oversized);
    assert(!large.begin());
    MemoryFlash blank;
    blank.mount = false;
    DeviceConfig failed(&blank);
    assert(!failed.begin() && !failed.enroll(token));
}

void pendingRepair() {
    MemoryFlash flash = ready();
    DeviceConfig store(&flash);
    assert(store.begin());
    auto p = pending();
    assert(DeviceConfig::writePending(&p, &store));
    Config edit = store.config();
    for (unsigned field = 0; field < 11; ++field) {
        edit = store.config();
        switch (field) {
        case 0:
            edit.mode = Mode::Session;
            break;
        case 1:
            edit.key[31] = 2;
            break;
        case 2:
            edit.keyPresent = false;
            break;
        case 3:
            edit.owner = "0x1111111111111111111111111111111111111111";
            break;
        case 4:
            edit.signer = edit.owner = "0x1111111111111111111111111111111111111111";
            break;
        case 5:
            edit.chainId = "1";
            break;
        case 6:
            edit.expiry = 100;
            break;
        case 7:
            edit.gaploBudget = "10";
            break;
        case 8:
            edit.enabled = false;
            break;
        case 9:
            edit.primaryUrl = "https://different.invalid";
            break;
        case 10:
            edit.fallbackUrl = "https://different.invalid";
            break;
        }
        unsigned writes = flash.writes;
        assert(!store.commit(edit));
        assert(writes == flash.writes);
    }
    edit = store.config();
    edit.ssid = "new-network";
    edit.password = "new-password";
    edit.primaryCa = pem + pem;
    edit.ntpPrimary = "192.0.2.1";
    edit.intervalMs = 2000;
    edit.hashesPerStep = 2000;
    assert(DeviceConfig::tokenVerifier(other, edit.operatorVerifier));
    const char* ca = store.config().primaryCa.c_str();
    assert(store.commit(edit));
    assert(store.pending()->hash == p.hash && store.config().primaryCa.c_str() == ca);
    DeviceConfig reboot(&flash);
    assert(reboot.begin() && reboot.pending());
    assert(reboot.pending()->raw == p.raw && reboot.config().password == "new-password");
    assert(reboot.authenticate(other) && !reboot.authenticate(token));
    assert(reboot.factoryReset(p.hash));
    DeviceConfig reset(&flash);
    assert(reset.begin() && !reset.pending() && !reset.enrolled());
}

void validation() {
    Config c = configured();
    c.mode = Mode::Session;
    assert(!DeviceConfig::validate(c));
    c.owner = "0x1111111111111111111111111111111111111111";
    c.gaploBudget = "1000000000000000000";
    c.expiry = 100;
    assert(DeviceConfig::validate(c));
    c.gaploBudget = "0";
    assert(!DeviceConfig::validate(c));
    c = configured();
    c.fallbackUrl = "https://fallback.invalid";
    assert(!DeviceConfig::validate(c));
    c.fallbackCa = pem;
    assert(DeviceConfig::validate(c));
    c.fallbackCa = "garbage";
    assert(!DeviceConfig::validate(c));
    c = configured();
    c.chainId = "1337";
    assert(DeviceConfig::validate(c));
    c.chainId = "01337";
    assert(!DeviceConfig::validate(c));
    c.chainId = "0";
    assert(!DeviceConfig::validate(c));
    c = configured();
    c.owner.clear();
    c.signer.clear();
    assert(DeviceConfig::validate(c));
    c.owner = "0x1111111111111111111111111111111111111111";
    assert(!DeviceConfig::validate(c));
    c = configured();
    c.primaryUrl = "http://invalid";
    assert(!DeviceConfig::validate(c));
    c = configured();
    c.key[31] = 2;
    assert(!DeviceConfig::validate(c));
    c = configured();
    c.primaryCa.resize(DeviceConfig::MaxPem + 1);
    assert(!DeviceConfig::validate(c));
    c = configured();
    c.primaryUrl += std::string("\0trailing", 9);
    assert(!DeviceConfig::validate(c));
    c = configured();
    c.keyPresent = false;
    assert(!DeviceConfig::validate(c));
    clearConfig(c);
}

void serial() {
    MemoryFlash flash;
    DeviceConfig blank(&flash);
    assert(blank.begin());
    SerialConfig init(blank);
    Port port;
    assert(
        port.run(init, "APLO1 1 ENROLL " + token + " I_HAVE_PHYSICAL_USB_CONTROL\n") ==
        "APLO1 1 OK restart\n"
    );
    DeviceConfig store(&flash);
    assert(store.begin());
    SerialConfig serial(store, []() -> firmware::BoardDiagnostics {
        return {3, 160, 4194304, 0, 28256, 28040};
    });
    auto status = port.run(serial, "APLO1 2 STATUS\n", "secret raw RPC error");
    assert(
        status.find("state=other") != std::string::npos &&
        status.find("secret") == std::string::npos
    );
    assert(
        port.run(serial, "APLO1 80 DIAG\n") ==
        "APLO1 80 OK diag reset_reason=3 uptime_ms=" + std::to_string(testMillis) +
            " cpu_mhz=160 flash_bytes=4194304 psram_bytes=0 heap_free=28256 heap_largest=28040\n"
    );
    assert(port.run(serial, "APLO1 81 AUTH " + token + "\n") == "APLO1 81 OK authenticated\n");
    assert(port.run(serial, "APLO1 82 BEGIN\n") == "APLO1 82 OK\n");
    assert(port.run(serial, "APLO1 83 ").empty());
    Port alternate;
    assert(alternate.run(serial, "APLO1 84 COMMIT\n") == "APLO1 84 ERR auth\n");
    assert(!serial.provisioningActive());
    assert(port.run(serial, "APLO1 85 BEGIN\n") == "APLO1 85 ERR auth\n");
    assert(port.run(serial, "APLO1 3 BEGIN\n").find("ERR auth") != std::string::npos);
    assert(port.run(serial, "APLO1 4 AUTH " + other + "\n").find("ERR auth") != std::string::npos);
    assert(port.run(serial, "APLO1 5 AUTH " + token + "\n").find("ERR auth") != std::string::npos);
    testMillis += 5000;
    assert(
        port.run(serial, "APLO1 6 AUTH " + token + "\n").find("OK authenticated") !=
        std::string::npos
    );
    assert(port.run(serial, "APLO1 7 BEGIN\n") == "APLO1 7 OK\n" && serial.provisioningActive());
    assert(port.run(serial, "APLO1 8 CA_END\n").find("ERR certificate") != std::string::npos);
    assert(port.run(serial, "APLO1 9 CA_BEGIN primary 4096\n") == "APLO1 9 OK\n");
    assert(port.run(serial, "APLO1 10 COMMIT\n").find("ERR upload") != std::string::npos);
    const std::string prefix = "-----BEGIN CERTIFICATE-----\n",
                      suffix = "-----END CERTIFICATE-----\n";
    std::string largePem = prefix + std::string(4096 - prefix.size() - suffix.size(), 'A') + suffix;
    assert(largePem.size() == 4096);
    for (size_t offset = 0; offset < largePem.size(); offset += 192) {
        std::string input = "APLO1 11 CA_CHUNK " + std::to_string(offset) + " " +
                            hex(largePem.substr(offset, 192)) + "\n";
        assert(port.run(serial, input) == "APLO1 11 OK\n");
        assert(port.run(serial, input) == "APLO1 11 OK\n");
    }
    assert(port.run(serial, "APLO1 12 CA_CHUNK 0 42\n").find("ERR chunk") != std::string::npos);
    assert(port.run(serial, "APLO1 13 CA_END\n") == "APLO1 13 OK\n");
    assert(port.run(serial, "APLO1 13 CA_BEGIN fallback 4096\n") == "APLO1 13 OK\n");
    for (size_t offset = 0; offset < largePem.size(); offset += 192) {
        assert(
            port.run(
                serial,
                "APLO1 13 CA_CHUNK " + std::to_string(offset) + " " +
                    hex(largePem.substr(offset, 192)) + "\n"
            ) == "APLO1 13 OK\n"
        );
    }
    assert(port.run(serial, "APLO1 13 CA_END\n") == "APLO1 13 OK\n");
    std::string secret = "wifi-password-sensitive";
    assert(port.run(serial, "APLO1 14 SET password " + hex(secret) + "\n") == "APLO1 14 OK\n");
    assert(port.out.find(secret) == std::string::npos && port.out.find(token) == std::string::npos);
    assert(
        port.run(serial, "APLO1 15 SET interval_ms " + hex("99") + "\n").find("ERR value") !=
        std::string::npos
    );
    assert(
        port.run(serial, "APLO1 16 COMMIT\n") == "APLO1 16 OK restart\n" &&
        !serial.provisioningActive()
    );
    DeviceConfig restored(&flash);
    assert(restored.begin() && restored.config().primaryCa == largePem);
    assert(restored.config().fallbackCa == largePem);
    assert(restored.config().password == secret);
    SerialConfig parser(restored);
    assert(
        port.run(parser, "APLO1 17 AUTH " + token + "\n").find("OK authenticated") !=
        std::string::npos
    );
    assert(port.run(parser, "APLO1 18 BEGIN\n").find("OK") != std::string::npos);
    testMillis += 60001;
    parser.poll(port, "paused");
    assert(!parser.provisioningActive());
    assert(
        port.run(
            parser,
            std::string(513, 'x') + "APLO1 20 RESET NONE ERASE_LOCAL_ONLY_I_HAVE_RECONCILED_CHAIN\n"
        ) == "APLO1 0 ERR framing\n"
    );
    port.run(parser, "APLO1 21 SET password ");
    testMillis += 2001;
    parser.poll(port, "paused");
    assert(port.run(parser, "abcd\n") == "APLO1 0 ERR framing\n");
    assert(port.run(parser, "APLO1 22 STATUS\r\n").find("OK status") != std::string::npos);
    assert(port.run(parser, "APLO1 23 STAT\rUS\n") == "APLO1 0 ERR framing\n");
    assert(
        port.run(parser, "APLO1 24 RESET NONE wrong_ack\n").find("ERR reset") != std::string::npos
    );
    assert(
        port.run(parser, "APLO1 25 RESET NONE ERASE_LOCAL_ONLY_I_HAVE_RECONCILED_CHAIN\n") ==
        "APLO1 25 OK restart\n"
    );
    // Enrollment window closes and never wraps back open after millis overflow.
    testMillis = 0;
    MemoryFlash lateFlash;
    DeviceConfig lateStore(&lateFlash);
    assert(lateStore.begin());
    SerialConfig late(lateStore);
    testMillis = 120000;
    assert(
        port.run(late, "APLO1 1 ENROLL " + token + " I_HAVE_PHYSICAL_USB_CONTROL\n")
            .find("ERR enrollment") != std::string::npos
    );
    testMillis = 0;
    assert(
        port.run(late, "APLO1 2 ENROLL " + token + " I_HAVE_PHYSICAL_USB_CONTROL\n")
            .find("ERR enrollment") != std::string::npos
    );
}
} // namespace

int main() {
    storage();
    faults();
    pendingRepair();
    validation();
    serial();
    puts("storage/serial regression tests passed");
}
