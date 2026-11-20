#include "BoardRuntime.h"
#include <nvs.h>
#include <nvs_flash.h>

namespace firmware {
namespace {
using miningembed::provisioning::DeviceConfig;
using miningembed::provisioning::FlashBackend;

constexpr const char* slots[] = {"slot0", "slot1", "journal0", "journal1"};

class PlatformFlash final : public FlashBackend {
    nvs_handle_t nvs_ = 0;

public:
    bool begin() override {
        return nvs_flash_init_partition("aplo_cfg") == ESP_OK &&
               nvs_open_from_partition("aplo_cfg", "device", NVS_READWRITE, &nvs_) == ESP_OK;
    }

    Read read(unsigned slot, std::string& bytes) override {
        size_t n = 0;
        const auto result = nvs_get_blob(nvs_, slots[slot], nullptr, &n);
        if (result == ESP_ERR_NVS_NOT_FOUND) {
            return Read::Missing;
        }
        const size_t maximum = slot < 2 ? DeviceConfig::MaxRecord : DeviceConfig::MaxJournalRecord;
        if (result != ESP_OK || n == 0 || n > maximum) {
            return Read::Error;
        }
        bytes.resize(n);
        return nvs_get_blob(nvs_, slots[slot], &bytes[0], &n) == ESP_OK &&
                       n == bytes.size() ? Read::Ok : Read::Error;
    }

    bool replace(unsigned slot, const std::string& bytes) override {
        return nvs_set_blob(nvs_, slots[slot], bytes.data(), bytes.size()) == ESP_OK &&
               nvs_commit(nvs_) == ESP_OK;
    }
};
} // namespace

miningembed::provisioning::FlashBackend& flashStorage() {
    static PlatformFlash storage;
    return storage;
}

} // namespace firmware
