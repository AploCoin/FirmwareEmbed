# FirmwareEmbed v1.0

AploCoin mining firmware with two modes in one image per board. Set up the device over USB Serial to use an owner-authorized Session Key or a Direct Private Key. Wi-Fi, RPC endpoints, TLS trust, settings, the provisioned key and outstanding transactions are stored in onboard flash.

| Board | PlatformIO environment | Configuration storage |
| --- | --- | --- |
| ESP32 DevKit | `esp32dev` | NVS |
| ESP32-C3-DevKitM-1 | `esp32-c3-devkitm-1` | NVS |
| ESP32-WROOM-32U | `esp32-wroom-32u` | NVS |
| ESP8266 ESP-12E (4 MB) | `esp12e` | LittleFS |
| ESP32-S3 N16R8 | `esp32-s3-n16r8` | NVS |

Build from the repository root with PlatformIO Core 6.1.19. The [library lock](libraries.ini) pins the SDK and mining libraries. To build with both libraries as sibling checkouts, use the local configuration.

~~~sh
pio run -e esp12e
pio run -c platformio-local.ini -e esp12e
~~~

[Installation, updates and artifacts](docs/flash.md) · [USB Serial API](docs/serial-api.md) · [Mining modes and recovery](docs/sessions.md) · [Architecture](docs/architecture.md) · [Verification](docs/validation.md)
