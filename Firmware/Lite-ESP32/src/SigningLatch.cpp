#include "SigningLatch.h"
#include <Arduino.h>
#include <cstring>
#include <esp_partition.h>
#include <esp_spi_flash.h>

namespace {
constexpr uint32_t offset = APLO_STORAGE_OFFSET;
constexpr uint32_t sectorSize = 4096;
const esp_partition_t* partition = nullptr;
}

bool SigningLatch::read(uint32_t at, uint32_t* words, size_t count) {
    return partition && esp_partition_read(partition, at, words, count * sizeof(uint32_t)) == ESP_OK;
}

bool SigningLatch::erased() {
    uint32_t words[32];
    for (uint32_t at = 0; at < sectorSize; at += sizeof(words)) {
        if (!read(at, words, 32)) return false;
        for (uint32_t word : words) if (word != UINT32_MAX) return false;
        yield();
    }
    return true;
}

bool SigningLatch::begin() {
    partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        esp_partition_subtype_t(0x40), "aplo_latch");
    const esp_partition_t* app = esp_partition_find_first(ESP_PARTITION_TYPE_APP,
        ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
    if (APLO_STORAGE_SIZE != sectorSize || APLO_FLASH_BYTES != 0x100000 ||
        ESP.getFlashChipSize() != APLO_FLASH_BYTES ||
        !partition || partition->address != offset || partition->size != sectorSize ||
        offset != 0xff000 || !app || app->address != 0x10000 ||
        app->size != offset - app->address) return false;
    ready_ = erased();
    return ready_;
}

bool SigningLatch::mark(const miningembed::PendingTransaction& pending) {
    if (!ready_ || active_ || !erased() || pending.hash.size() != 66) return false;
    // Public recovery hint only. The whole sector, including any torn record,
    // remains a lock at boot. No validity marker can turn residue into empty.
    uint32_t words[24];
    memset(words, 0xff, sizeof(words));
    words[0] = 0x314c5041; // APL1, latch schema 1
    words[1] = uint32_t(pending.nonce);
    words[2] = uint32_t(pending.nonce >> 32);
    memcpy(reinterpret_cast<char*>(words + 3), pending.hash.c_str(), 67);
    active_ = true; // Every failure retains the live signing lock as well.
    const bool written = esp_partition_write(partition, 0, words, sizeof(words)) == ESP_OK;
    uint32_t verify[24];
    if (!written || !read(0, verify, 24) || memcmp(words, verify, sizeof(words))) return false;
    return true;
}

bool SigningLatch::clear() {
    // MiningEmbed invokes this only after a terminal receipt for the exact
    // pending hash, on the original chain. An interrupted erase is safe there:
    // the transaction is resolved; any remaining bits cause a boot lock.
    if (!ready_ || !active_) return false;
    const bool cleared = esp_partition_erase_range(partition, 0, sectorSize) == ESP_OK;
    if (!cleared || !erased()) return false;
    active_ = false;
    return true;
}

bool SigningLatch::writePending(const miningembed::PendingTransaction* pending, void* context) {
    if (!context) return false;
    auto& self = *static_cast<SigningLatch*>(context);
    return pending ? self.mark(*pending) : self.clear();
}
