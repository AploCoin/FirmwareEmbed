# FirmwareEmbed

Standalone AploCoin mining projects for embedded boards. Each project in [Firmware/](Firmware/) can be copied to a new directory and opened directly in PlatformIO. Its board code and pinned library dependencies travel with it.

| Board | Project | PlatformIO environment |
| --- | --- | --- |
| ESP8266 ESP-12E | [ESP8266](Firmware/ESP8266/) | `esp12e` |
| ESP32 DevKit | [ESP32](Firmware/ESP32/) | `esp32dev` |
| ESP32-C3 DevKitM-1 | [ESP32-C3](Firmware/ESP32-C3/) | `esp32-c3-devkitm-1` |
| ESP32-WROOM-32U | [ESP32-WROOM-32U](Firmware/ESP32-WROOM-32U/) | `esp32-wroom-32u` |
| ESP32-S3 N16R8 | [ESP32-S3-N16R8](Firmware/ESP32-S3-N16R8/) | `esp32-s3-n16r8` |

Open the chosen project in PlatformIO. The board README covers Wi-Fi, HTTPS RPC, session provisioning and upload. PlatformIO fetches the exact [AploEmbed](https://github.com/AploCoin/AploEmbed) and [MiningEmbed](https://github.com/AploCoin/MiningEmbed) commits listed in that board's platformio.ini.

Edit the selected project's source or build settings to customize a device. The ESP8266 Wi-Fi and heap handling, ESP32-C3 USB and radio settings, WROOM-32U flash and optional LAN time adapter, and ESP32-S3-N16R8 UART console, 16 MB quad flash and octal PSRAM settings are local to their respective folders. To add a board, copy the closest project into a new folder under `Firmware/` and adapt its own settings and runtime.

To package just one board from a Git checkout:

~~~sh
git archive --format=zip -o FirmwareEmbed-ESP8266.zip HEAD:Firmware/ESP8266
~~~

[Architecture](docs/architecture.md) · [Session lifecycle](docs/sessions.md)
