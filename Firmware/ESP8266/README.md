# ESP8266 and ESP-01(S)

This standalone project defaults to ESP-12E with 4 MiB flash. Select `esp8266-2m` for verified 2 MiB modules, or `esp01-1m` for an ESP-01/ESP-01S with verified 1 MiB flash. Run `pio run -e <environment>` or add `-t upload` for USB-to-UART programming. The `esp01-512k` environment is a diagnostic size check and cannot fit the full firmware; it produces no supported release artifact.

All supported profiles run at 160 MHz and require 1024-byte TLS MFLN on each HTTPS RPC endpoint. Each has a distinct flash map; changing profiles does not preserve an existing configuration automatically. The 4 MiB default retains its established LittleFS region.

PlatformIO fetches the libraries' default GitHub branches for development. Run `pio pkg update` to refresh them. Release CI pins the two reviewed SHAs from the repository lock file.

[Flash maps and ESP-01 wiring limits](https://github.com/AploCoin/FirmwareEmbed/blob/main/docs/compatibility.md) · [USB updates and rollback](https://github.com/AploCoin/FirmwareEmbed/blob/main/docs/flash.md) · [USB Serial configurator](https://github.com/AploCoin/FirmwareEmbed/blob/main/docs/serial-api.md)
