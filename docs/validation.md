# FirmwareEmbed v1.0 verification

The 2026-11-20 local matrix built five detached board folders with PlatformIO 6.1.19, AploEmbed `1f7500a` and MiningEmbed `cce1737`. Figures are bytes from PlatformIO's linker size report and from the generated application file.

| Profile | Flash used / partition capacity | Static RAM used / available | Application image |
| --- | ---: | ---: | ---: |
| ESP32 DevKit | 1,037,033 / 1,310,720 | 47,776 / 327,680 | 1,043,616 |
| ESP32-C3-DevKitM-1 | 1,023,602 / 1,310,720 | 41,892 / 327,680 | 1,066,256 |
| ESP32-WROOM-32U | 1,037,057 / 1,310,720 | 47,776 / 327,680 | 1,043,632 |
| ESP8266 ESP-12E | 493,247 / 1,044,464 | 35,820 / 81,920 | 497,392 |
| ESP32-S3 N16R8 | 989,045 / 6,553,600 | 47,316 / 327,680 | 989,408 |

All five profiles passed. Each project built from a copy containing only that board's folder. The release gate caps reported flash and static RAM at 90% of their capacity, with a 43,008-byte static RAM budget on ESP8266.

| Check | Result |
| --- | --- |
| SDK native ASan/UBSan | JSON-RPC and receipt parsing, exact hash binding, large nonce and chain ID, ABI and signature vectors passed |
| SDK TLS, standard and ESP8266 build branches | Loopback HTTPS trust, CA-only runtime PEM, hostname and validity failures, failover, trust overlap and streamed receipt cases passed. The ESP8266 test uses a host transport for the handshake and models MFLN negotiation |
| SDK static regressions | 32 passed. The optional SDK example build matrix was skipped in this invocation |
| MiningEmbed native ASan/UBSan | Direct EIP-155 and delegated scope, fee and nonce checks, durable pre-send journal, power-loss simulation, exact replay and receipt states passed |
| Firmware storage and Serial ASan/UBSan | Both configurations, authorization and reset, 4,096-byte PEM uploads, malformed input, slot corruption, interrupted or ambiguous writes, pending-preserving repair, diagnostics, and authentication/staging isolation on a port switch passed with mocked flash and X.509 boundaries |
| Host configurator | APLO1 status framing, staged field and CA uploads, command order and redaction passed |
| Release packaging | Five board profiles, both C3 connection descriptors referencing identical binaries, ZIP integrity, SHA-256 for all assets, and configuration-preserving write ranges verified. The flash layout ID and install and upgrade offsets match the preceding v1.0 test package. Altered firmware, an invalid NVS layout and an over-budget ESP8266 RAM report were rejected |

The local serial inventory showed two macOS system ports and no ESP devices. On-board USB provisioning, sustained TLS/mining, free heap and stack margin, physical power cuts during a send or flash operation, and a flash-preserving firmware upgrade are pending hardware verification. GitHub Actions and GitHub Release publication are configured in [the workflow](../.github/workflows/firmware.yml). Current hosted verification is available in GitHub Actions. [Firmware builds](https://github.com/AploCoin/FirmwareEmbed/actions/workflows/firmware.yml)

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
