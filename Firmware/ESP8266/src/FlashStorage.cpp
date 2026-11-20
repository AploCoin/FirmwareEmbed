#include "BoardRuntime.h"
#include <Arduino.h>
#include <LittleFS.h>
#include <cstring>

extern "C" uint32_t _FS_start, _FS_end, _FS_page, _FS_block;

namespace firmware {
namespace {
using miningembed::provisioning::DeviceConfig;
using miningembed::provisioning::FlashBackend;

constexpr const char* paths[] = {"/aplo0.bin", "/aplo1.bin", "/journal0.bin", "/journal1.bin"};
constexpr uintptr_t flashMapping = 0x40200000;

bool validFilesystemLayout() {
    const auto start = reinterpret_cast<uintptr_t>(&_FS_start);
    const auto end = reinterpret_cast<uintptr_t>(&_FS_end);
    const auto page = reinterpret_cast<uintptr_t>(&_FS_page);
    const auto block = reinterpret_cast<uintptr_t>(&_FS_block);
    return ESP.getFlashChipSize() == APLO_FLASH_BYTES &&
           ESP.getFlashChipRealSize() >= APLO_FLASH_BYTES &&
           start == flashMapping + APLO_STORAGE_OFFSET &&
           end == start + APLO_STORAGE_SIZE && APLO_STORAGE_SIZE >= 65536 &&
           !(start & 4095) && !(end & 4095) && page == 256 &&
           (block == 4096 || block == 8192) && end - flashMapping <= APLO_FLASH_BYTES;
}

// Formatting is permitted only for a fully erased linker-defined FS partition.
bool erasedFilesystem() {
    const auto start = reinterpret_cast<uintptr_t>(&_FS_start) - flashMapping;
    const auto end = reinterpret_cast<uintptr_t>(&_FS_end) - flashMapping;
    uint32_t words[64];
    for (uintptr_t offset = start; offset < end; offset += sizeof(words)) {
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
        if (!validFilesystemLayout()) {
            return false;
        }
        LittleFS.setConfig(LittleFSConfig().setAutoFormat(false));
        if (LittleFS.begin()) {
            return true;
        }
        return erasedFilesystem() && LittleFS.format() && LittleFS.begin();
    }

    Read read(unsigned slot, std::string& bytes) override {
        const char* path = paths[slot];
        if (!LittleFS.exists(path)) {
            return Read::Missing;
        }
        const size_t maximum = slot < 2 ? DeviceConfig::MaxRecord : DeviceConfig::MaxJournalRecord;
        File f = LittleFS.open(path, "r");
        if (!f || f.size() == 0 || f.size() > maximum) {
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
        return ok && LittleFS.rename("/aplo.tmp", paths[slot]);
    }

    bool verify(unsigned slot, const std::string& expected) override {
        File f = LittleFS.open(paths[slot], "r");
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
