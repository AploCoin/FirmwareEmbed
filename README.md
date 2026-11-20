# FirmwareEmbed v1.0

AploCoin mining firmware with two modes in one image per board. Set up the device over USB Serial to use an owner-authorized Session Key or a Direct Private Key. Wi-Fi, RPC endpoints, TLS trust, settings, the provisioned key and outstanding transactions are stored in onboard flash.

| Board | PlatformIO environment | Configuration storage |
| --- | --- | --- |
| ESP32 DevKit | `esp32dev` | NVS |
| ESP32-C3-DevKitM-1 | `esp32-c3-devkitm-1` | NVS |
| ESP32-WROOM-32U | `esp32-wroom-32u` | NVS |
| ESP8266 ESP-12E (4 MB) | `esp12e` | LittleFS |
| ESP32-S3 N16R8 | `esp32-s3-n16r8` | NVS |

Each board folder is a complete PlatformIO project. Copy just that folder and open
it in PlatformIO, or run `pio run` from inside it. The board's `platformio.ini`
pins the GitHub revisions of AploEmbed and MiningEmbed, which PlatformIO fetches
automatically.

~~~sh
pio run -d Firmware/ESP8266
git archive --format=zip -o FirmwareEmbed-ESP8266.zip HEAD:Firmware/ESP8266
~~~

Use [one host configurator](scripts/configure.py) for all boards.

[Installation, updates and artifacts](docs/flash.md) · [USB Serial API and configurator](docs/serial-api.md) · [Mining modes and recovery](docs/sessions.md) · [Architecture](docs/architecture.md) · [Verification](docs/validation.md)
