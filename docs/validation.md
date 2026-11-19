# FirmwareEmbed v1.0 verification

The 2026-11-19 local matrix used PlatformIO 6.1.19 and the exact SDK and MiningEmbed commits in [libraries.ini](../libraries.ini). Git resolved the pins from the local repositories. Figures are bytes from PlatformIO's linker size report and from the generated application file.

| Profile | Flash used / partition capacity | Static RAM used / available | Application image |
| --- | ---: | ---: | ---: |
| ESP32 DevKit | 1,037,521 / 1,310,720 | 47,952 / 327,680 | 1,044,096 |
| ESP32-C3-DevKitM-1 | 1,023,306 / 1,310,720 | 42,060 / 327,680 | 1,065,520 |
| ESP32-WROOM-32U | 1,037,553 / 1,310,720 | 47,952 / 327,680 | 1,044,128 |
| ESP8266 ESP-12E | 492,695 / 1,044,464 | 36,012 / 81,920 | 496,848 |
| ESP32-S3 N16R8 | 989,025 / 6,553,600 | 47,484 / 327,680 | 989,392 |

All five profiles passed. The WROOM-32U board-folder entry point also built independently. The release gate caps reported flash and static RAM at 90% of their capacity, with a 43,008-byte static RAM budget on ESP8266.

| Check | Result |
| --- | --- |
| SDK native ASan/UBSan | JSON-RPC and receipt parsing, exact hash binding, large nonce and chain ID, ABI and signature vectors passed |
| SDK TLS, standard and ESP8266 build branches | Loopback HTTPS trust, CA-only runtime PEM, hostname and validity failures, failover, trust overlap and streamed receipt cases passed. The ESP8266 test uses a host transport for the handshake and models MFLN negotiation |
| SDK static regressions | 32 passed. The optional SDK example build matrix was skipped in this invocation |
| MiningEmbed native ASan/UBSan | Direct EIP-155 and delegated scope, fee and nonce checks, durable pre-send journal, power-loss simulation, exact replay and receipt states passed |
| Firmware storage and Serial ASan/UBSan | Both configurations, authorization and reset, 4,096-byte PEM uploads, malformed input, slot corruption, interrupted or ambiguous writes, pending-preserving repair, diagnostics, and authentication/staging isolation on a port switch passed with mocked flash and X.509 boundaries |
| Release packaging | Five board profiles, both C3 connection descriptors referencing identical binaries, ZIP integrity, SHA-256 for all assets, and configuration-preserving write ranges verified. Altered firmware, an invalid NVS layout and an over-budget ESP8266 RAM report were rejected |

The local serial inventory showed two macOS system ports and no ESP devices. On-board USB provisioning, sustained TLS/mining, free heap and stack margin, physical power cuts during a send or flash operation, and a flash-preserving firmware upgrade are pending hardware verification. GitHub Actions and GitHub Release publication are configured in [the workflow](../.github/workflows/firmware.yml). These checks used the local workflow inputs and build commands. The hosted workflow has yet to run against the new library pins.

## Previous hardware evidence

The earlier device trial reported 24 confirmed Session Key shares on ESP32-C3 over
native USB Serial/JTAG and 15 on ESP32-S3 N16R8 over its UART0 bridge. The ESP8266
trial reported 14 confirmed shares with a patched BearSSL watchdog and larger core
stacks. The merged revisions supply the S3 profile, ESP8266 at 160 MHz and the ECDSA
watchdog feed. The v1.0 board-test image uses the stock BearSSL stacks and needs its
own cold-start evidence. [SDK PR #30](https://github.com/AploCoin/AploEmbed/pull/30) ·
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
