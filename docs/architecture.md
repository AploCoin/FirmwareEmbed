# Firmware architecture

Each folder in Firmware is a standalone PlatformIO project with its own board settings. The application in src/main.cpp connects Wi-Fi, synchronizes time, configures the HTTPS trust roots and imports the restricted session key from Serial into RAM. BoardRuntime owns the platform's Wi-Fi association and reconnect behavior. The WROOM-32U project also offers its own LAN SNTP adapter.

The ESP32-S3-N16R8 project keeps the console on UART0 for a USB-UART bridge, selects quad 16 MB flash with octal 8 MB PSRAM, and disables Wi-Fi sleep.

AploEmbed supplies HTTPS/RPC, session-key handling, native authorization and transaction signing. MiningEmbed parses public mining authorization and runs the bounded read/search/submit cycle. Its step method polls an unresolved submission before starting any new work. Firmware chooses when to call it and what to show on Serial. An uncertain send stays locked to its transaction hash. After resetting the board, reconcile old submissions with the owner before continuing.

Change the target board's files to adjust its peripherals, memory or scheduling. Common protocol and mining behavior belongs in the libraries. Each board pins its own dependency revisions in platformio.ini.
