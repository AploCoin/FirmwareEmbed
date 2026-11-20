# Flash profiles and compatibility

Each folder remains a standalone PlatformIO project. Select an alternate environment with `pio run -e <id>`. All supported profiles include the same two mining modes, TLS verification, USB provisioning and durable storage. Smaller flash does not select a weaker security mode.

| Folder / environment | Physical flash | Application capacity | Preserved configuration region | Status |
| --- | ---: | ---: | --- | --- |
| ESP32 / `esp32dev` | 4 MiB | `0x280000` | `0x290000` + `0x10000` | Supported factory map |
| ESP32 / `esp32dev-1m` | 1 MiB | `0xd0000` | `0xe0000` + `0x10000` | Diagnostic only; cannot fit full firmware |
| ESP32 / `esp32dev-2m` | 2 MiB | `0x180000` | `0x190000` + `0x10000` | Reduced-flash factory map |
| ESP32-C3 / `esp32-c3-devkitm-1` | 4 MiB | `0x280000` | `0x290000` + `0x10000` | UART bridge and native USB Serial/JTAG |
| ESP32-WROOM-32U / `esp32-wroom-32u` | 4 MiB | `0x280000` | `0x290000` + `0x10000` | External antenna module; UART bridge |
| ESP32-S3-N16R8 / `esp32-s3-n16r8` | 16 MiB, 8 MiB PSRAM | `0xc80000` | `0xc90000` + `0x10000` | UART bridge; exact N16R8 memory settings |
| ESP8266 / `esp12e` | 4 MiB | 1,044,464 bytes | `0x300000` + `0xfa000` | Existing LittleFS map retained |
| ESP8266 / `esp8266-2m` | 2 MiB | 1,044,464 bytes | `0x1e0000` + `0x1b000` | LittleFS `2m128` map |
| ESP8266 / `esp01-1m` | 1 MiB | 892,912 bytes | `0xdb000` + `0x20000` | ESP-01/ESP-01S with verified 1 MiB flash; DOUT |
| ESP8266 / `esp01-512k` | 512 KiB | 434,160 bytes | `0x6b000` + `0x10000` | Diagnostic build only; cannot fit full firmware |

The 512 KiB environment is deliberately excluded from release artifacts. Its linker script reserves 64 KiB of LittleFS for bounded records and atomic replacement, plus framework/SDK sectors. The complete application exceeds the remaining 434,160-byte executable capacity. Removing storage or TLS to force a build would violate the firmware contract. A 512 KiB ESP-01 needs a flash replacement or different hardware for this firmware.

A 1 MiB ESP32 has less than 1 MiB for its application after boot/partition/configuration reservations, while the measured full ESP32 application is already over 1 MiB. The diagnostic `esp32dev-1m` environment verifies this capacity failure; no 1 MiB ESP32 release profile is offered. The 2 MiB profile has a distinct configuration offset and layout; choosing it on an established 4 MiB device is not a configuration-preserving update.

ESP8266's linker mapping limits this application's executable region to about 1 MiB even on larger chips. The 2 MiB and 4 MiB profiles therefore have the same sketch limit and different filesystem positions. EEPROM, RF calibration and Wi-Fi SDK sectors remain outside the configuration filesystem. [ESP8266 Arduino 3.1.2 filesystem maps](https://arduino-esp8266.readthedocs.io/en/stable/filesystem.html)

ESP-01/ESP-01S needs a regulated 3.3 V supply with sufficient Wi-Fi current, suitable logic levels, boot straps, and an external USB-to-UART adapter. Do not infer usable flash, onboard USB, PSRAM or antenna arrangement from chip-family detection. These profiles validate software compilation and flash boundaries; a hardware mining/provisioning test is still required on the exact module, supply and RPC endpoint. ESP8266 HTTPS endpoints must support 1024-byte TLS MFLN.

[USB installation, migration and rollback](flash.md) · [Validation evidence](validation.md)
