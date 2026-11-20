# FirmwareEmbed USB factory validation

The 2026-11-20 local matrix built all eight supported environments with PlatformIO 6.1.19, source-only standalone board folders, and clean Git archives of locked AploEmbed `01b5a77fb54086dc88881748efba1a3ccd0861d6` and MiningEmbed `0ec01eabf912550709f82620de494c18b2281da2`. The local firmware version is `usb-v2-local`. Figures below are bytes from the linker size report and generated application image; these are compilation and packaging results, not physical-device measurements.

| Environment | Flash used / application capacity | Static RAM used / available | Application image |
| --- | ---: | ---: | ---: |
| `esp32dev` | 1,041,053 / 2,621,440 | 47,952 / 327,680 | 1,047,632 |
| `esp32dev-2m` | 1,041,065 / 1,572,864 | 47,952 / 327,680 | 1,047,648 |
| `esp32-c3-devkitm-1` | 1,028,796 / 2,621,440 | 42,068 / 327,680 | 1,072,336 |
| `esp32-s3-n16r8` | 993,129 / 13,107,200 | 47,492 / 327,680 | 993,488 |
| `esp32-wroom-32u` | 1,041,069 / 2,621,440 | 47,952 / 327,680 | 1,047,648 |
| `esp12e` | 500,495 / 1,044,464 | 37,052 / 81,920 | 504,640 |
| `esp8266-2m` | 500,503 / 1,044,464 | 37,052 / 81,920 | 504,656 |
| `esp01-1m` | 500,495 / 892,912 | 37,052 / 81,920 | 504,640 |

All eight passed the release resource gates: at most 90% of reported flash/static RAM capacity, with the tighter 43,008-byte static RAM budget on ESP8266. Every application and install part also passed sector-rounded erase-range checks against its physical flash and preserved configuration region. Static RAM does not establish usable TLS heap or stack margin on a physical board.

| Diagnostic environment | Measured reason for exclusion |
| --- | --- |
| `esp01-512k` | Full application fails linking: `.irom0.text` will not fit `irom0_0_seg` in `eagle.flash.512k64.ld` (434,160-byte executable capacity with 64 KiB filesystem). The comparable 1 MiB image is 504,640 bytes. |
| `esp32dev-1m` | PlatformIO rejects 1,041,065 bytes of program content against the 851,968-byte factory slot. |

CI builds these two diagnostic profiles and accepts only a recognized flash-capacity failure. A compile error in unrelated source does not count as evidence that the device is too small. Neither diagnostic profile can be staged or published as supported firmware.

The 13 current host artifact/USB-service regressions passed. They cover immutable dependency pins, the eight-profile release/checksum round trip, matching board/version/layout identity, rejection of OTA partition tables, wrong board/map/schema, corrupted or unsafe asset paths, sector rounding, RAM budgets, and exact old/factory partition-table checks. A real eight-profile release was also assembled locally; every entry in `SHA256SUMS` was independently recalculated. Firmware ELF strings confirmed the WROOM board ID, layout and embedded version. Workflow YAML and all Bash run blocks parsed successfully.

The USB service's execution path probes the physical chip family and flash size and reads ESP32 partition tables before an update or migration. Host regressions validate its decision logic; hardware USB execution and power-cut recovery require the exact connected device. The generated manifest's `update` list is one application image, and its install/migration lists omit OTA selector images. No generic web-installer descriptors are produced.

## Native and hardware evidence boundaries

The current native sanitizer tests passed for SDK, mining and provisioning,
including interrupted migration, separate journal writes and owner-signed
Operator token rotation. Both TLS transport test suites also passed. Host
storage faults are injected at record boundaries.

The [flash guide](flash.md) defines the supported same-offset migration and schema-compatible manual rollback paths. A mocked interrupted-write test cannot prove recovery during a physical flash power cut. ESP-01/ESP-01S wiring, supply integrity, MFLN support and sustained mining remain hardware-specific checks. [Flash profile limits](compatibility.md)

## Previous hardware evidence

The earlier device trial reported 24 confirmed Session Key shares on ESP32-C3 over
native USB Serial/JTAG and 15 on ESP32-S3 N16R8 over its UART0 bridge. The ESP8266
trial reported 14 confirmed shares with a patched BearSSL watchdog and larger core
stacks. The merged revisions supply the S3 profile, ESP8266 at 160 MHz and the ECDSA
watchdog feed. The v1.0 board-test image uses the stock BearSSL stacks and needs its
own device test. The subsequent bench retest confirmed stock BearSSL TLS
and confirmed shares without a reboot for the earlier board image `9fecb7f`.
[SDK PR #30](https://github.com/AploCoin/AploEmbed/pull/30) ·
[ESP8266 PR #1](https://github.com/AploCoin/FirmwareEmbed/pull/1) ·
[S3 PR #2](https://github.com/AploCoin/FirmwareEmbed/pull/2)

## Hardware checks

1. On C3, send `APLO1 1 STATUS` over the native USB port from the earlier trial.
   Check the UART0 bridge on boards that expose it. On S3, collect `APLO1 2 DIAG`
   and verify the expected 16 MiB of flash and 8 MiB of PSRAM.
2. On ESP8266, use the same trusted HTTPS RPC if available. Confirm 1024-byte MFLN
   on the server, collect `DIAG` after a cold boot, wait for confirmed shares, then
   repeat after power cycling. If it reboots, capture the startup log and
   `reset_reason` before retrying. Record heap and the largest free block before
   configuration, after network setup and during mining.
3. On the test chain, verify both modes by receipt and signer nonce. Cycle power
   with a share pending and confirm reconciliation by hash before a new nonce.
   Use the application-only update and verify the configuration is still active.
