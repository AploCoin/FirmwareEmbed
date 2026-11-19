# ESP32-C3 DevKitM-1

The same image supports the DevKitM-1 Micro-USB bridge (UART0) and the chip's native USB Serial/JTAG port on boards that expose it. Open the selected port at 115200 baud and send `APLO1 1 STATUS`. Native USB takes priority when both are connected. [Espressif DevKitM-1 guide](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c3/esp32-c3-devkitm-1/user_guide.html)

Build from the repository root with `pio run -e esp32-c3-devkitm-1`, or open this directory in PlatformIO. Keep this directory inside the FirmwareEmbed checkout.

[Installation and upgrade](../../docs/flash.md) · [USB Serial API](../../docs/serial-api.md)
