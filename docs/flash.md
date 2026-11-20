# USB installation, updates and rollback

Select an exact board profile and its actual flash size. ESP32 DevKit and WROOM-32U both identify as ESP32; chip-family detection cannot distinguish those boards. ESP-01 and ESP-01S labels also do not prove flash capacity. Read the chip's flash ID before choosing a profile. [Supported maps and hardware limits](compatibility.md)

The application has no OTA updater. ESP32 uses one `factory` application at `0x10000`, without `otadata`, OTA slots or `boot_app0.bin`. ESP8266 writes its application at `0`. The configuration and pending-transaction journal occupy a separate preserved storage region.

## First installation

Build from a standalone folder with `pio run -e <environment>`, or use that profile's `install` parts in release `manifest.json`. For a local USB upload, run `pio run -e <environment> -t upload` inside the folder. ESP32 install writes the bootloader, partition table and factory application. ESP8266 install writes the application only. A blank device initializes its storage on first boot; a nonblank unreadable store fails closed.

A new installation onto unrelated firmware needs its previous data backed up and storage cleared deliberately before provisioning. The supplied updater never runs a whole-chip erase or formats a nonblank unreadable store. Installing a firmware image does not authorize discarding an outstanding transaction.

Use the manifest-aware USB service for installs, updates and migrations. Generic ESP Web Tools descriptors are not release assets: a generic new install can erase the chip, and ESP32 framework images need additional merged-image handling outside this service's verified contract. Do not upload a filesystem image over provisioned ESP8266 storage. [ESP Web Tools installation behavior](https://esphome.github.io/esp-web-tools/)

## Upgrade from the earlier dual-app ESP32 layout

Run one explicit migration using `migrate` parts, even when the previous firmware booted from `app0`. Migration writes the new bootloader, factory partition table and application. An application-only write cannot turn an old OTA partition table into a factory layout. Once the factory table is installed, the bootloader selects its factory application and the old OTA selector and `app1` contents are unreferenced. No OTA selector erase is needed.

The established configuration offsets remain `0x290000` on the 4 MiB ESP32, C3 and WROOM profiles and `0xc90000` on the 16 MiB S3. `0x650000` was the S3's old second application offset, never its configuration offset. Migration does not erase configuration. The new firmware can read a supported legacy configuration record at the same offset and migrate it atomically, preserving pending transaction reconciliation. See [storage architecture](architecture.md).

The migration service accepts `--from-layout esp32-4m-dual-app-v1` on the three established 4 MiB boards, or `--from-layout esp32-s3-16m-dual-app-v1` on S3. This is an explicit statement that the device runs that old FirmwareEmbed map. Before executing any migration write, the service reads the physical partition table and verifies the old app slots, OTA selector and unchanged configuration bounds; a mismatched table is rejected. Unknown maps and moves to smaller flash are separate migrations requiring an exported configuration and reconciled pending state.

For an existing FirmwareEmbed ESP8266 4 MiB image, use `esp12e` with
`--mode install` once. It writes only the application and keeps the original
LittleFS range. The first boot migrates the records in place. Later updates can
use `--mode update` with the device's `GET_INFO` reply.

## Configuration-preserving application updates

Capture `GET_INFO` from the connected device immediately before flashing, and select the same board and layout. Use its `update` list with `eraseAll: false`. Every update contains exactly one application image. The service validates the board, layout, physical/profile flash size, storage bounds, readable storage schema, each asset's SHA-256 and every sector the write will erase. A mismatched board/layout is rejected. esptool additionally probes the physical chip family and flash size before execution; ESP32 updates read and verify the physical factory partition table before writing.

Install Python dependencies with `python3 -m pip install 'esptool>=4,<5' pyserial`. Capture device info using the [configurator](serial-api.md), and save its `info ...` output or JSON to `device-info.txt`. Inspect the plan before executing:

```sh
python3 scripts/flash.py release/manifest.json --profile esp32dev --port /dev/cu.usbserial-DEVICE --device-info device-info.txt
python3 scripts/flash.py release/manifest.json --profile esp32dev --port /dev/cu.usbserial-DEVICE --device-info device-info.txt --execute
```

For an established dual-app device's one-time factory migration:

```sh
python3 scripts/flash.py release/manifest.json --profile esp32dev --port /dev/cu.usbserial-DEVICE --mode migrate --from-layout esp32-4m-dual-app-v1
```

Add `--execute` after reviewing the plan. For a blank board, select `--mode install`. Close other serial programs before execution. USB boot/download mode and reliable board power are required; an interrupted application update needs another USB write before the application can run. There is no second-image OTA recovery slot.

## Manual rollback and schema changes

A rollback uses a previously saved release manifest and its matching binary files. Use the same application-only service with a fresh `GET_INFO`; it accepts the rollback only when the target declares that it can read the device's current storage schema and uses the exact same board, flash map and storage region. Version ordering alone does not prove compatibility.

Storage schema 2 separates configuration commits from frequent pending-journal writes. A compatible image can migrate schema 1 forward. An older schema-1 image cannot interpret schema 2 and must fail closed. Rolling back to the original dual-app/schema-1 firmware requires the saved matching boot/partition/application assets and an independently reconciled, supported configuration baseline; it is not an application-only rollback. Never restore an old pending snapshot onto a device that may have submitted later transactions. Do not erase the store to bypass a schema error.

## Reproducible release assets

Standalone source folders intentionally fetch current default branches for development. `dependencies.lock.json` fixes exactly one full commit SHA for AploEmbed and one for MiningEmbed for release CI. Update that file only to reviewed commits. The same pair is checked out for native tests, pinned into every isolated board build, and recorded in the release manifest. CI embeds one release version into the firmware and staged profile before assembly. Local builds identify as `dev` unless `APLO_RELEASE_VERSION` is set.

The workflow runs host regressions, compiles the supported matrix, gates static RAM and flash usage, and assembles manifests plus `SHA256SUMS`. `manifest.json` schema 2 contains each profile's `install`, `migrate` and `update` lists, board/version/layout identity, storage/schema compatibility, resource use and SHA-256 for every image. Relative assets must stay beside the manifest. Checksum files detect corruption; obtain the manifest and assets from a trusted release source. Tags matching `vMAJOR.MINOR[.PATCH]` publish only after all configured gates pass. [Workflow](../.github/workflows/firmware.yml)

esptool erases flash in 4096-byte sectors, so both artifact staging and the USB service validate rounded erase ends, not only the file's byte length. The service uses `write_flash --flash_size keep` and never issues `erase_flash`. [esptool 4 basic commands](https://docs.espressif.com/projects/esptool/en/release-v4/esp32/esptool/basic-commands.html)
