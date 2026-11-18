# ESP32-WROOM-32U mining firmware

This directory is a complete PlatformIO project. Copy it anywhere, open it in PlatformIO and edit its own `src/`, `include/` and `platformio.ini`.

Uses the hardware-verified configuration: 4 MB DIO flash at 40 MHz, CPU at 240 MHz and CP2102/UART0 at 115200 baud. Connect the external Wi-Fi antenna.

For a LAN SNTP server on a custom UDP port, configure `APLO_LOCAL_NTP_HOST` with the IPv4 address and `APLO_LOCAL_NTP_PORT` with the port. Without a local host override, the project uses the configured NTP servers.

## Set up

1. Install PlatformIO, Python 3, Git and the OpenSSL CLI.
2. Clone [AploEmbed](https://github.com/AploCoin/AploEmbed) and [MiningEmbed](https://github.com/AploCoin/MiningEmbed) and check out the revisions in [dependencies.lock.json](dependencies.lock.json). From this board directory run:

   ~~~sh
   python3 scripts/configure_dependencies.py --aploembed /path/to/AploEmbed --miningembed /path/to/MiningEmbed
   cp include/DeviceConfig.example.h include/DeviceConfig.h
   ~~~

3. Set Wi-Fi and the public authorization fields in `include/DeviceConfig.h`. See [session setup](docs/sessions.md) for registration and renewal. The device takes only the restricted session key over local Serial after connecting. Keep the owner key on the owner's machine.
4. Set an HTTPS trust profile. For a local gateway, supply the public root PEM and a LAN hostname found in the server certificate:

   ~~~sh
   python3 /path/to/AploEmbed/tools/trust/profile.py configure --kind local \
     --primary https://rpc.lan:19443 --ca /path/to/local-root.pem \
     --out trust-profile.local.json
   export APLO_TRUST_PROFILE="$PWD/trust-profile.local.json"
   ~~~

   For a public RPC with a compatible chain, set its URL and the chosen versioned CA set in [trust-profile.example.json](trust-profile.example.json), copy it to `trust-profile.local.json` and use the same environment variable. For your own RPC select a public set or pass its public root PEM. Use `--fallback` with `--fallback-ca` or `--fallback-public-set` to pin trust separately for a second address. See `docs/tls-trust.md` in the pinned AploEmbed checkout for profile options and CA rotation.

## Build and flash

~~~sh
pio run
pio run -t upload
pio device monitor -b 115200
~~~

If the pinned esptool reports a missing `intelhex` module, run `pio system info` to find the PlatformIO Python executable and install `intelhex==2.3.0` into that environment.

Select the actual upload port when multiple serial devices are attached. The environment in this folder is `esp32-wroom-32u`. A successful build uses this board's code and the two pinned libraries. Missing credentials or authorization stop startup. Missing trust stops HTTPS before a connection. Profile files and local configuration are ignored by Git.
