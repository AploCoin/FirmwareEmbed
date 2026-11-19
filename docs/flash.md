# Installation and firmware updates

Choose the exact PlatformIO board profile. ESP32 DevKit and WROOM-32U both identify as ESP32 over USB, so chip-family discovery alone cannot select the profile. ESP32-C3-DevKitM-1 uses the USB-to-UART bridge on its Micro-USB connector. [Espressif DevKitM-1 guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c3/esp32-c3-devkitm-1/user_guide.html)

## First v1.0 installation

Use the selected profile's `install` parts from `manifest.json`. The ESP32 profiles include bootloader, partition table and OTA selector alongside the application. ESP8266 uses the application at offset 0 and initializes LittleFS on a provably erased filesystem region. Both modes are in the same application binary. For a local source build the equivalent is `pio run -e <board> -t upload`, or add `-c platformio-local.ini` when using sibling libraries.

The pre-v1 firmware used build-time settings and held the session key and transaction state in RAM. Move Wi-Fi, RPC/CA and approved key settings into the [USB configurator](serial-api.md) and reconcile any old outstanding share on the owner machine before enabling mining. First v1.0 installation on ESP32 adds a dedicated 64 KiB `aplo_cfg` NVS partition at `0x290000` (4 MB) or `0xc90000` (16 MB), from the start of the stock SPIFFS area. The old mining firmware left that area available. Apply the partition table only to the matching board and flash size. ESP8266 keeps the 4 MB `eagle.flash.4m1m.ld` map. Preserve any existing filesystem data separately if the device previously ran unrelated firmware.

## Configuration-preserving v1.x updates

Use the same profile and flash layout with `eraseAll: false` and that profile's `update` parts. Validate the SHA-256 for each part before writing. Update writes only the application at `0x10000` (ESP32 profiles, stock FirmwareEmbed app0 boot configuration) or `0` (ESP8266). Keep the bootloader, partition table, OTA selector and configuration region in place. A device set to boot an alternate ESP32 OTA slot needs its boot configuration reconciled before following this update path.

Schema v1 records are read by compatible images using the same layout. Unrecognized versions and unreadable nonblank stores fail closed. A future storage upgrade must ship a reader for the previous version and write the migrated record atomically before activation. A downgrade needs a reader that supports the current schema.

## Release assets

GitHub Actions builds all profiles, runs native tests, enforces flash/static-RAM budgets, and assembles binaries, manifests and `SHA256SUMS`. A validated version tag `vMAJOR.MINOR[.PATCH]` publishes the assets to its GitHub release after the checks pass. Make the exact SDK and mining commits in `libraries.ini` available on their respective GitHub repositories before running release CI.

- `manifest.json` is the versioned flasher index. Select `profiles[].id` explicitly and use only its `install` or `update` list. Each entry gives a relative asset path, byte offset, size and SHA-256. `storage` identifies the flash region that the flasher must preserve.
- `manifest-<board>.json` is a first-install descriptor for ESP Web Tools. Use **Skip erase** to preserve data and use the flasher index's application-only path for v1.x updates. [ESP Web Tools manifest documentation](https://esphome.github.io/esp-web-tools/)
- `<board>-firmware.bin` is the application for both modes. Other `.bin` assets contain the ESP32 boot/partition components referenced by the install descriptor.
