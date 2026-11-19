# FirmwareEmbed v1.0 verification

The 2026-11-19 local matrix used PlatformIO 6.1.19 and the exact SDK and MiningEmbed commits in [libraries.ini](../libraries.ini). Git resolved the pins from the local repositories. Figures are bytes from PlatformIO's linker size report and from the generated application file.

| Profile | Flash used / partition capacity | Static RAM used / available | Application image |
| --- | ---: | ---: | ---: |
| ESP32 DevKit | 1,036,825 / 1,310,720 | 47,944 / 327,680 | 1,043,408 |
| ESP32-C3-DevKitM-1 | 1,006,180 / 1,310,720 | 41,676 / 327,680 | 1,047,392 |
| ESP32-WROOM-32U | 1,036,841 / 1,310,720 | 47,944 / 327,680 | 1,043,424 |
| ESP8266 ESP-12E | 490,495 / 1,044,464 | 36,004 / 81,920 | 494,640 |
| ESP32-S3 N16R8 | 988,261 / 6,553,600 | 47,476 / 327,680 | 988,624 |

All five profiles passed. The WROOM-32U board-folder entry point also built independently. The release gate caps reported flash and static RAM at 90% of their capacity, with a 43,008-byte static RAM budget on ESP8266.

| Check | Result |
| --- | --- |
| SDK native ASan/UBSan | JSON-RPC and receipt parsing, exact hash binding, large nonce and chain ID, ABI and signature vectors passed |
| SDK TLS, standard and ESP8266 build branches | Loopback HTTPS trust, CA-only runtime PEM, hostname and validity failures, failover, trust overlap and streamed receipt cases passed. The ESP8266 test uses a host transport for the handshake and models MFLN negotiation |
| SDK static regressions | 32 passed. The optional SDK example build matrix was skipped in this invocation |
| MiningEmbed native ASan/UBSan | Direct EIP-155 and delegated scope, fee and nonce checks, durable pre-send journal, power-loss simulation, exact replay and receipt states passed |
| Firmware storage and Serial ASan/UBSan | Both configurations, authorization and reset, 4,096-byte PEM uploads, malformed input, slot corruption, interrupted or ambiguous writes, and pending-preserving repair passed with mocked flash and X.509 boundaries |
| Release packaging | Five board profiles, ZIP integrity, SHA-256 for all assets, and configuration-preserving write ranges verified. Altered firmware, an invalid NVS layout and an over-budget ESP8266 RAM report were rejected |

The local serial inventory showed two macOS system ports and no ESP devices. On-board USB provisioning, sustained TLS/mining, free heap and stack margin, physical power cuts during a send or flash operation, and a flash-preserving firmware upgrade are pending hardware verification. GitHub Actions and GitHub Release publication are configured in [the workflow](../.github/workflows/firmware.yml). These checks used the local workflow inputs and build commands. The hosted workflow has yet to run against the new library pins.
