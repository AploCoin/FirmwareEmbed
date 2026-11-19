# ESP32-S3 N16R8 mining firmware

This directory is a complete PlatformIO project. Copy it anywhere, open it in PlatformIO and edit its own `src/`, `include/` and `platformio.ini`.

The environment `esp32-s3-n16r8` targets an ESP32-S3 with 16 MB quad flash and 8 MB octal PSRAM at 3.3 V (module order code N16R8). It starts from the PlatformIO `esp32-s3-devkitc-1` profile and overrides that profile's 8 MB flash size. The console is UART0 at 115200 baud. Native USB CDC is off, so a USB-UART bridge such as CH343 is the serial port. Wi-Fi sleep is disabled in this project's board runtime.

The radio is 2.4 GHz only. ESP32-S3-WROOM-1-N16R8 uses the PCB antenna on the module; do not attach a second antenna. ESP32-S3-WROOM-1U-N16R8 needs a 2.4 GHz antenna on the IPEX connector before the radio starts. Power the board from a USB port that can supply at least 500 mA. Wi-Fi transmit current peaks at a few hundred milliamps on the 3.3 V rail.

## Set up

1. Install PlatformIO and Git. Open this folder directly.
2. Copy `include/DeviceConfig.example.h` to `include/DeviceConfig.h`. Fill in Wi-Fi, HTTPS RPC and the public metadata from your confirmed session. Follow [session setup](docs/sessions.md) to create or renew it.
3. For manual CA setup, replace the `APLO_RPC_PRIMARY_CA` definition with your public root PEM. Set `APLO_RPC_FALLBACK_CA` too if you configure a second RPC.

   ~~~cpp
   static const char rpcCa[] PROGMEM = R"PEM(
   Paste the public PEM root certificate here.
   )PEM";
   #define APLO_RPC_PRIMARY_CA rpcCa
   ~~~

   For a versioned CA set, you can generate `AploTrustProfile.h` with the [optional AploEmbed tool](https://github.com/AploCoin/AploEmbed/blob/master/docs/tls-trust.md#offline-builds-and-ca-updates) and place it in `include/`. Its URLs and CA values override the manual RPC fields.

## Build and upload

~~~sh
pio run -e esp32-s3-n16r8
pio run -e esp32-s3-n16r8 -t upload --upload-port /dev/ttyACM0
pio device monitor -b 115200 --port /dev/ttyACM0
~~~

Upload rewrites the bootloader, partition table and application. It does not erase the whole flash and it does not write eFuse. PlatformIO fetches the pinned [AploEmbed](https://github.com/AploCoin/AploEmbed) and [MiningEmbed](https://github.com/AploCoin/MiningEmbed) commits from GitHub. The Serial prompt accepts only the restricted session key. Keep the owner key on the signing device.

If the pinned esptool reports a missing intelhex module, use pio system info to find the PlatformIO Python executable and install intelhex==2.3.0 in that environment.
