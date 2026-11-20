# FirmwareEmbed v1.0

AploCoin mining firmware with two modes in one image per board. Set up the device over USB Serial to use an owner-authorized Session Key or a Direct Private Key. Wi-Fi, RPC endpoints, TLS trust, settings, the provisioned key and outstanding transactions are stored in onboard flash.

| Board | PlatformIO environment | Configuration storage |
| --- | --- | --- |
| ESP32 DevKit (4 MiB / 2 MiB) | `esp32dev` / `esp32dev-2m` | NVS |
| ESP32-C3-DevKitM-1 | `esp32-c3-devkitm-1` | NVS |
| ESP32-WROOM-32U | `esp32-wroom-32u` | NVS |
| ESP8266 (4 MiB / 2 MiB / ESP-01 1 MiB) | `esp12e` / `esp8266-2m` / `esp01-1m` | LittleFS |
| ESP32-S3 N16R8 | `esp32-s3-n16r8` | NVS |

Each board folder is a complete PlatformIO project. Copy just that folder and open
it in PlatformIO, or run `pio run` from inside it. The board's `platformio.ini`
uses the latest code from the default GitHub branches of AploEmbed and MiningEmbed.
Run `pio pkg update` inside an existing project to refresh downloaded libraries.

~~~sh
pio run -d Firmware/ESP8266
git archive --format=zip -o FirmwareEmbed-ESP8266.zip HEAD:Firmware/ESP8266
~~~

Use [one host configurator](scripts/configure.py) for all boards.

[Flash profiles and hardware limits](docs/compatibility.md) · [Installation, updates and artifacts](docs/flash.md) · [USB Serial API and configurator](docs/serial-api.md) · [Mining modes and recovery](docs/sessions.md) · [Architecture](docs/architecture.md) · [Verification](docs/validation.md)
