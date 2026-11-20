# USB Install and Update

`manifest.json` schema 3 provides `install` and `update` parts per profile.
`edition` and binary names identify Full and Lite. Each profile records its layout,
flash bounds, preserved storage, hardware test status and asset hashes. A
diagnostic profile in `excludedProfiles` has no installable assets. The firmware
uses USB only. ESP32 boots one factory application, and ESP8266 writes its
application at address zero.

Install esptool 4.x and pySerial into the host environment.

```sh
python3 -m pip install 'esptool>=4,<5' pyserial
```

## Install on a blank device

Inspect the write plan with the release manifest and the exact board profile.
Pass `--execute` once you have verified the plan. The service probes the physical
chip and flash size and writes only the listed parts. It never runs a whole-chip
erase. On a board with other firmware, preserve its data and reconcile any
outstanding transaction before writing a new layout.

```sh
python3 scripts/flash.py release/manifest.json --profile esp32dev \
  --port /dev/ttyUSB0 --mode install
```

A Full profile stores configuration in NVS or LittleFS. A Lite profile stores
only the transaction guard in its reserved flash sector. For Lite, pass
`--lite-app` with the image built from your private header. A stock Lite asset is
an unconfigured template. See [Lite setup](lite.md).

## Full Update

Capture fresh `GET_INFO` from the connected device. Update verifies that the
profile, physical flash, storage schema, and layout match. On ESP32, the
flasher checks the physical factory partition table before writing. It writes a
single application file and checks that sector erase ranges stay outside the
persistent configuration and journal.

```sh
python3 scripts/configure.py --port /dev/ttyUSB0 info > device-info.txt
python3 scripts/flash.py release/manifest.json --profile esp32dev \
  --port /dev/ttyUSB0 --mode update --device-info device-info.txt
```

An earlier release can be restored with `update` if it uses the same board and
layout and declares support for the current storage schema. Retain both the
manifest and matching binaries. Keep the current journal during any update or
rollback.

## Lite Update

Keep the exact previous personal binary in private storage. Supply it and the
new personal build to the updater. Both binaries must carry the same layout
marker. Before any write, the service reads the current application and
compares it with the previous binary. ESP32 also checks the physical partition
table. The one application write leaves the persistent journal intact.

```sh
python3 scripts/flash.py release/manifest.json --profile lite-esp8266-512k \
  --port /dev/ttyUSB0 --mode update --installed-app previous-private.bin \
  --lite-app new-private.bin
```

Close other serial programs before using the USB service. With interrupted
application writes, use USB download mode to repair the application and keep
the original storage region. The manifest, sources and SHA-256 checksums belong
to the same release.
