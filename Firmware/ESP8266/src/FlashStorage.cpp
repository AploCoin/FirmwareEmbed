#include "BoardRuntime.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <cstring>

extern "C" uint32_t _FS_start, _FS_end, _FS_page, _FS_block;

namespace firmware {
namespace {
using miningembed::provisioning::DeviceConfig;
using miningembed::provisioning::FlashBackend;

// Formatting is permitted only for a fully erased linker-defined FS partition.
bool erasedFilesystem() {
    const uintptr_t map = 0x40200000;
    uintptr_t start = reinterpret_cast<uintptr_t>(&_FS_start),
              end = reinterpret_cast<uintptr_t>(&_FS_end);
    uintptr_t page = reinterpret_cast<uintptr_t>(&_FS_page),
              block = reinterpret_cast<uintptr_t>(&_FS_block);
    if (start < map + 0x10000 || end <= start || end - start < 65536 ||
        end - start > 2 * 1024 * 1024 || (start & 4095) || (end & 4095) || page != 256 ||
        (block != 4096 && block != 8192) || end - map > ESP.getFlashChipRealSize() ||
        end - map > ESP.getFlashChipSize()) {
        return false;
    }
    uint32_t words[64];
    for (uintptr_t offset = start - map; offset < end - map; offset += sizeof(words)) {
        if (!ESP.flashRead(offset, words, sizeof(words))) {
            return false;
        }
        for (uint32_t word : words) {
            if (word != UINT32_MAX) {
                return false;
            }
        }
        yield();
    }
    return true;
}

class PlatformFlash final : public FlashBackend {
public:
    bool begin() override {
        constexpr uint32_t profileFlashBytes = 4 * 1024 * 1024;
        if (ESP.getFlashChipSize() != profileFlashBytes ||
            ESP.getFlashChipRealSize() < profileFlashBytes) {
            return false;
        }
        LittleFS.setConfig(LittleFSConfig().setAutoFormat(false));
        if (LittleFS.begin()) {
            return true;
        }
        return erasedFilesystem() && LittleFS.format() && LittleFS.begin();
    }

    Read read(unsigned slot, std::string& bytes) override {
        const char* path = slot ? "/aplo1.bin" : "/aplo0.bin";
        if (!LittleFS.exists(path)) {
            return Read::Missing;
        }
        File f = LittleFS.open(path, "r");
        if (!f || f.size() == 0 || f.size() > DeviceConfig::MaxRecord) {
            return Read::Error;
        }
        size_t n = f.size();
        bytes.resize(n);
        return f.readBytes(&bytes[0], n) == n ? Read::Ok : Read::Error;
    }

    bool replace(unsigned slot, const std::string& bytes) override {
        // Incomplete .tmp is never a committed record. rename replaces one old slot atomically.
        File f = LittleFS.open("/aplo.tmp", "w");
        if (!f) {
            return false;
        }
        bool ok =
            f.write(reinterpret_cast<const uint8_t*>(bytes.data()), bytes.size()) == bytes.size();
        f.flush();
        f.close();
        return ok && LittleFS.rename("/aplo.tmp", slot ? "/aplo1.bin" : "/aplo0.bin");
    }

    bool verify(unsigned slot, const std::string& expected) override {
        File f = LittleFS.open(slot ? "/aplo1.bin" : "/aplo0.bin", "r");
        if (!f || f.size() != expected.size()) {
            return false;
        }
        char buffer[128];
        size_t at = 0;
        bool ok = true;
        while (at < expected.size()) {
            size_t count = expected.size() - at;
            if (count > sizeof(buffer)) {
                count = sizeof(buffer);
            }
            if (f.readBytes(buffer, count) != count ||
                memcmp(buffer, expected.data() + at, count)) {
                ok = false;
                break;
            }
            at += count;
        }
        miningembed::provisioning::wipe(buffer, sizeof(buffer));
        return ok;
    }
};
} // namespace

miningembed::provisioning::FlashBackend& flashStorage() {
    static PlatformFlash storage;
    return storage;
}

} // namespace firmware
