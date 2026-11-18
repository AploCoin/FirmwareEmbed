# FirmwareEmbed

Standalone AploCoin mining projects for embedded boards. Each project in [Firmware/](Firmware/) can be copied to a new directory and opened directly in PlatformIO. Its own code, settings and library lock travel with it.

| Board | Project | PlatformIO environment |
| --- | --- | --- |
| ESP8266 ESP-12E | [ESP8266](Firmware/ESP8266/) | `esp12e` |
| ESP32 DevKit | [ESP32](Firmware/ESP32/) | `esp32dev` |
| ESP32-C3 DevKitM-1 | [ESP32-C3](Firmware/ESP32-C3/) | `esp32-c3-devkitm-1` |
| ESP32-WROOM-32U | [ESP32-WROOM-32U](Firmware/ESP32-WROOM-32U/) | `esp32-wroom-32u` |

Open the chosen project's README for its dependency revisions, Wi-Fi and RPC configuration, trust profile, build and upload commands. Each project needs separate [AploEmbed](https://github.com/AploCoin/AploEmbed) and [MiningEmbed](https://github.com/AploCoin/MiningEmbed) checkouts at the commits in its lock. AploEmbed handles HTTPS, RPC and session keys. MiningEmbed handles mining. The session key is entered locally over Serial and only kept in RAM.

Edit the selected project's source or build settings to customize a device. The ESP8266 Wi-Fi and heap handling, ESP32-C3 USB and radio settings, and WROOM-32U flash and optional LAN time adapter are local to their respective folders. To add a board, copy the closest project into a new folder under `Firmware/` and adapt its own settings and runtime.

To package just one board from a Git checkout:

~~~sh
git archive --format=zip -o FirmwareEmbed-ESP8266.zip HEAD:Firmware/ESP8266
~~~

## Repository checks

For maintainers, configure the desired projects’ library paths first. From the repository root:

~~~sh
python3 tests/run.py --board ESP32
python3 scripts/build_matrix.py
~~~

The matrix runs PlatformIO separately in every configured board project and writes logs to `build-logs/`. Use the same public test fixture and trust profile when comparing memory across revisions. [Architecture](docs/architecture.md) · [Session lifecycle](docs/sessions.md)
