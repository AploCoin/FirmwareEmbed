# Firmware architecture

The repository has one application in `src/` and one PlatformIO profile per board in `boards.ini`. The board folders open that same application with a board-specific default. Firmware starts USB Serial and mounts storage before starting Wi-Fi. Wi-Fi association and SNTP progress independently of the USB configuration loop. Synchronous HTTPS calls can delay a Serial reply while a mining step is in progress.

- **AploEmbed** checks JSON-RPC responses, signs EIP-155 transactions, and provides endpoint-specific HTTPS trust. CA buffers live with the active configuration. The ESP8266 reuses a single BearSSL client and one parsed trust context and checks MFLN before writing an RPC request.
- **MiningEmbed** reads and validates work, signs in the selected mode, and polls the exact transaction hash. It calls the firmware journal before sending signed bytes and waits for a durable clear before using the next nonce. Restored records are checked against the active signer, chain, fee and mining calldata.
- **FirmwareEmbed** validates staged configurations and applies committed changes at restart. The NVS and LittleFS stores keep versioned, integrity-checked generations. A failed store or an unreadable record pauses mining. Replacing a configuration and retaining its outstanding transaction use the same durable record.

The RPC hosts are peers on the same chain. Use independent trusted CA chains if the primary and fallback use different roots. Both hosts need to serve the standard JSON-RPC methods plus the applicable native AploCoin contracts. The ESP8266 profile expects TLS 1.2 with 1024-byte MFLN. SNTP uses UDP port 123 and the configured servers.

The Serial interface treats the USB host as trusted during provisioning. Runtime authentication protects local configuration changes. Stock firmware stores key material in plain flash on the supported board profiles. Physical access to flash or a host that can capture the USB session exposes that material, especially consequential in Direct Private Key mode.
