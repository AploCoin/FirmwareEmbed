# ESP32-WROOM-32U mining firmware

This directory is a complete PlatformIO project. Copy it anywhere, open it in PlatformIO and edit its own `src/`, `include/` and `platformio.ini`.

Uses the hardware-verified configuration: 4 MB DIO flash at 40 MHz, CPU at 240 MHz and CP2102/UART0 at 115200 baud. Connect the external Wi-Fi antenna.

For a LAN SNTP server on a custom UDP port, configure `APLO_LOCAL_NTP_HOST` with the IPv4 address and `APLO_LOCAL_NTP_PORT` with the port. Without a local host override, the project uses the configured NTP servers.

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
pio run
pio run -t upload
pio device monitor -b 115200
~~~

PlatformIO fetches the pinned [AploEmbed](https://github.com/AploCoin/AploEmbed) and [MiningEmbed](https://github.com/AploCoin/MiningEmbed) commits from GitHub. The Serial prompt accepts only the restricted session key. Keep the owner key on the signing device.

If the pinned esptool reports a missing intelhex module, use pio system info to find the PlatformIO Python executable and install intelhex==2.3.0 in that environment.
