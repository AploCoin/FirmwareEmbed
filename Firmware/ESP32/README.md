# ESP32 DevKit

Use the USB-to-UART port at 115200 baud.

Copy this directory on its own and open it in PlatformIO. Run `pio run` to compile
or `pio run -t upload` to flash the selected device. PlatformIO fetches the libraries' default
GitHub branches. Run `pio pkg update` to refresh dependencies in an existing project.

[Installation and upgrade](https://github.com/AploCoin/FirmwareEmbed/blob/main/docs/flash.md) · [USB Serial configurator](https://github.com/AploCoin/FirmwareEmbed/blob/main/docs/serial-api.md)

The default `esp32dev` environment requires 4 MiB flash. `pio run -e esp32dev-2m` selects a 2 MiB factory layout with a different configuration offset. Both use one factory application and USB updates; no OTA slots are reserved. The 1 MiB board uses the separate [Lite project](../Lite-ESP32). Use a matching board profile and storage map for Update.
