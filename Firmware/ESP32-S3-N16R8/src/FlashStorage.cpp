#include "BoardRuntime.h"
#include <Preferences.h>
#include <nvs_flash.h>

namespace firmware {
namespace {
using miningembed::provisioning::DeviceConfig;
using miningembed::provisioning::FlashBackend;

class PlatformFlash final : public FlashBackend {
    Preferences nvs_;

public:
    bool begin() override {
        return nvs_flash_init_partition("aplo_cfg") == ESP_OK &&
               nvs_.begin("device", false, "aplo_cfg");
    }

    Read read(unsigned slot, std::string& bytes) override {
        const char* key = slot ? "slot1" : "slot0";
        if (!nvs_.isKey(key)) {
            return Read::Missing;
        }
        size_t n = nvs_.getBytesLength(key);
        if (n == 0 || n > DeviceConfig::MaxRecord) {
            return Read::Error;
        }
        bytes.resize(n);
        return nvs_.getBytes(key, &bytes[0], n) == n ? Read::Ok : Read::Error;
    }

    bool replace(unsigned slot, const std::string& bytes) override {
        return nvs_.putBytes(slot ? "slot1" : "slot0", bytes.data(), bytes.size()) == bytes.size();
    }
};
} // namespace

miningembed::provisioning::FlashBackend& flashStorage() {
    static PlatformFlash storage;
    return storage;
}

} // namespace firmware
