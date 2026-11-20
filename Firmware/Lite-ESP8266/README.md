# Lite-ESP8266

Experimental compile-time Lite profile. Both Session and Direct mining use the
same MiningEmbed engine and its authorization, nonce, proof, receipt and retry
checks. A Session key never falls back to an owner key. There is no APLO1 USB
provisioning, runtime configuration store, OTA, filesystem, or Full diagnostics.
Hardware mining, TLS heap margin, watchdog behavior and physical power cuts
remain unverified.

512 KiB ESP8266/ESP-01, DOUT, 160 MHz. The no-filesystem linker layout
permits a 499,696-byte app. The raw signing latch is the reserved EEPROM sector
`0x7b000..0x7bfff`; RF calibration/Wi-Fi sectors remain at `0x7c000..0x7ffff`.
No EEPROM library or filesystem may use that sector.

Build from this standalone folder:

```sh
pio run -e lite-esp8266-512k
python3 tests/run_latch.py
python3 tests/run_image_gate.py
```

The default `include/DeviceConfig.h` selects `DeviceConfig.example.h`. That public
starter contains no credentials and its volatile arming flag is false: it stops
before Wi-Fi or key import. Both mode constructors remain linked in this starter,
so its size represents the mining application. Copy `DeviceConfig.example.h` to
`include/DeviceConfig.private.h`, supply your own values, set `kConfigured=true`,
and select Session or Direct. The ignored private header takes precedence.
Private keys are 32 bytes, not text hex. Zero/invalid keys are rejected by
SessionKey::Import. Personal firmware binaries contain secrets; keep them private.

Use explicit HTTPS URLs with DNS hostnames and CA PEM trust for each enabled
endpoint. Fallback is optional. CA PEM buffers are immutable and flash-resident.
Wi-Fi and a valid SNTP clock are required before mining. ESP8266 requires TLS MFLN
negotiation (1024 bytes), hostname verification, and a valid chain; it never falls
back to an insecure or large-fragment connection. ESP32 keeps strict certificate,
hostname and validity-time verification. No TLS or transaction checks are removed.

Each personal build must pass both PlatformIO's linked-image check and this
folder's post-build check on the actual padded `firmware.bin`. The small ESP32
slot especially limits total configuration/CA size: a typical 1939-byte ISRG
Root X1 PEM and both modes are build-verified, but two large distinct roots may
exceed capacity. Shorten configuration or omit the optional fallback when a
build exceeds the slot; never bypass the binary gate or change the flash map.

The startup marker is `APLO-LITE-LAYOUT:` followed by the release layout string.
Public binaries are inert starters and cannot receive APLO1 commands. Initial
installation requires the matching board layout. App-only updates require an
externally verified matching existing layout and must preserve the latch. Choose the
matched layout and reconcile outstanding transactions before reinstalling.

## Durable signing latch (schema 1)

The firmware checks exact physical flash capacity and app/storage geometry before
any key import. The entire 4 KiB latch must be erased at boot. Any residue, read
failure or invalid layout locks mining; boot never auto-erases or formats it.

Before a transaction reaches the network, MiningEmbed calls the board journal
hook. The adapter writes and reads back a 96-byte latch record, then permits the
broadcast. Failure retains the engine's unresolved lock. Retries reuse the same
RAM-resident signed bytes. The firmware erases the latch only after a terminal
receipt for the exact transaction hash and original chain; unknown/null receipts,
RPC errors and nonce changes do not release it. An interrupted terminal erase is
safe because the transaction is resolved; any residue still blocks the next boot.

A reset while latched deliberately requires external reconciliation, even if no
broadcast actually occurred. The record is a public recovery hint, not a durable
raw-transaction journal: byte 0 holds little-endian `0x314c5041`, bytes 4..11 the
nonce, and bytes 12..78 the NUL-terminated transaction hash. Torn/unknown bytes
must not be interpreted as an empty or resolved record. Read the raw sector,
reconcile the signer's canonical and pending nonce/transactions on the configured
chain externally, and erase exactly the latch sector only after proving it safe.
The firmware has no serial clear, reset-configuration or bypass command.

`tests/run_latch.py` compiles the actual adapter against a NOR model and verifies
all 4096 residue positions, 97 partial-write prefixes, 4097 partial-clear prefixes,
and flash-read failure. ESP8266's absolute linker symbol is modeled in the host
test; the real target linker/capacity is independently checked by PlatformIO.
These are simulated power-loss tests, not physical-device evidence.
