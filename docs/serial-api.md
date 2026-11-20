# USB provisioning API v1

The integration API is in MiningEmbed under `src/provisioning`.
`begin()` mounts storage without destructive recovery; `locked()` distinguishes storage
failure from an empty, usable device. `ready()` requires enrolled, enabled, validated
configuration. `config()` and its CA buffers remain immutable until destruction.
Call `SerialConfig::poll(Stream&, state)` on every loop, including Wi-Fi/time waits.
When `provisioningActive()` is true, suspend mining and release parsed TLS trust
allocations before the next poll. This reduces ESP8266 peak heap during PEM upload
and flash serialization. All methods run on the same loop thread; there is no
concurrent RPC/serial mutation model.
After `restartRequested()` becomes true, stop all mining and restart at a short loop
boundary; do not construct new Web3 objects or broadcast another transaction first.
`writePending(ptr, &store)` commits before returning true; `nullptr` clears the journal.
`pending()` is the recovered journal. Journal writes do not request restart.

Config fields: `mode` (Session/Direct), `ssid`, `password`, `primaryUrl`, `fallbackUrl`,
`primaryCa`, `fallbackCa`, `owner`, `signer`, `gaploBudget`, `chainId` (defaults to 28282),
`expiry` (block), `key[32]`, `keyPresent`, `enabled`, `ntpPrimary`, `ntpFallback`,
`hashesPerStep` and `intervalMs`. `operatorVerifier[32]` holds a domain-separated
SHA-256 verifier of a 32-byte random Operator token. No legacy compile-time settings
or RAM-only secrets are imported automatically.

## Host configurator

`scripts/configure.py` uses the same APLO1 commands on every board. Install
`pyserial`, then run the script with the board's USB port:

~~~sh
python3 -m pip install pyserial
python3 scripts/configure.py --port /dev/ttyUSB0 status
python3 scripts/configure.py --port /dev/ttyUSB0 enroll
python3 scripts/configure.py --port /dev/ttyUSB0 apply --config board.json \
  --primary-ca rpc-root.pem --ask-wifi-password --ask-key
python3 scripts/configure.py --port /dev/ttyUSB0 diag
~~~

The JSON object maps the public field names in the table below to text or
integers. Use JSON `true` for `enabled` to activate valid settings. Provide
the session owner, signer, budget and expiry when setting `mode` to `session`.
Set `mode` to `direct` to use the key's address directly. The configurator
prompts for the operator token, Wi-Fi password and private key with hidden input.
Use `--fallback-ca` for a second endpoint. An empty PEM file clears that endpoint's
stored CA when the remaining settings permit it. For an authorized reset, run
`reset --journal NONE` or pass the exact verified pending hash after reconciliation.

## Transport and authentication

Use a local USB Serial connection at 115200 baud, 8N1, with terminal echo disabled.
ESP32-C3 accepts native USB Serial/JTAG and on-board USB-to-UART. Native USB takes
priority when both are connected. Switching ports clears the previous authentication,
partial request and staged configuration.
This API accepts no network-origin provisioning. It provides no USB encryption:
the connected host and anyone who can observe the USB/UART link must be trusted.
Never put tokens or key material in shell history or enable serial request logging.

Each ASCII request is exactly `APLO1 <id> <COMMAND> [arguments]\n`; CRLF is accepted.
`id` is canonical unsigned decimal, 0..4294967295, and is a correlation value,
not a replay nonce. Responses begin `APLO1 <id> OK` or `APLO1 <id> ERR <fixed-code>`.
No response contains an input token, password, private key, PEM, RPC URL/path,
signed raw transaction, request line, or raw RPC error. Arguments and command
names are never echoed. Unrecognized runtime states become `other`.

Lines have at most 512 bytes before LF (including an optional CR). Non-ASCII input,
embedded controls, repeated/trailing spaces, extra arguments and overlong lines
are rejected. A partial line idle for 2 seconds is wiped; input is discarded until
the next LF, so its suffix cannot become a command. Each `poll()` consumes at most
256 input bytes. A host must wait for responses instead of flooding the TX buffer.

On an unowned device, during the first 120 seconds after boot:

```text
APLO1 1 ENROLL <64-hex-character-random-operator-token> I_HAVE_PHYSICAL_USB_CONTROL
```

Create the 32-byte token using a host CSPRNG. All-zero is rejected. Physical access
to this local connection is the first-owner enrollment authority; there is no
default token. Enrollment commits only the verifier and requests restart. There
is no token recovery/readback. Existing enrollment cannot be replaced with ENROLL.
After the window closes it stays closed across `millis()` wrap; power-cycle an
unowned device to enroll. A successful logical reset permits new enrollment after
restart.

After enrollment/restart:

```text
APLO1 2 AUTH <operator-token>
APLO1 3 BEGIN
APLO1 4 SET <field> <hex-encoded-value>
APLO1 5 COMMIT
```

Successful AUTH grants this serial session 120 seconds, without extending it on
commands. A failed AUTH clears staging and imposes a 5-second retry delay.
The token verifier is SHA-256(`FirmwareEmbed/Operator/v1` || decoded token), checked
with constant-time byte comparison. A random token is required; this is not a
password-hardening KDF. AUTH and ENROLL take the token directly as 64 hex characters.

`BEGIN` copies the active configuration into RAM. A second BEGIN discards the old
staging transaction. `ABORT` discards it; `LOGOUT` discards it and authentication.
Staging expires after 60 seconds without an authenticated staging command.
Restart, firmware update or disconnect followed by timeout loses staging, leaving
the previous committed configuration intact. COMMIT validates the complete
candidate, atomically commits it, replies `OK restart`, and requests restart.
No new configuration is applied to live borrowed CA buffers.

`SET` values are hex-encoded bytes, or `-` for empty. NUL is rejected. Its effective
wire limit is **224 decoded bytes per field**; the storage codec also rejects
oversized fields from trusted internal callers. Clients must use the wire caps
below rather than assuming a 512-byte stored URL can be transmitted by SET.

| Field | Value before hex encoding | Wire cap / accepted range |
| --- | --- | --- |
| `mode` | `session` or `direct` | 7 bytes |
| `ssid` | Wi-Fi SSID | 32 bytes, nonempty when enabled |
| `password` | Wi-Fi password, `-` for open network | 64 bytes |
| `primary_url`, `fallback_url` | HTTPS URL | 224 bytes each; empty fallback disables it |
| `ntp_primary`, `ntp_fallback` | DNS hostname or IPv4 address | 224 bytes; fallback may be empty |
| `owner`, `signer` | `0x` + 40 address hex digits | 42 bytes |
| `chain_id` | positive decimal signing chain ID | up to uint256, default `28282` |
| `gaplo_budget` | decimal integer base units | 78 bytes; finite positive for Session |
| `expiry` | decimal block height | uint64; positive for Session |
| `hashes_per_step` | decimal attempt count | 1..2000 |
| `interval_ms` | decimal interval | 100..60000 |
| `enabled` | `0` or `1` | 1 byte |
| `key` | 64 hex characters of the private scalar | 64 bytes; clear with `-` |
| `operator_token` | new 64-hex-character random token | 64 bytes; verifier only is persisted |

For `key` and `operator_token`, SET hex-encodes the **ASCII hex text**, so those
wire values contain 128 hex characters. AUTH/ENROLL do not use this extra encoding.
For example, `SET enabled 31` stages `enabled=1`; `SET mode 73657373696f6e`
stages Session mode. Private scalar validity and signer identity use SessionKey
Import/Address. Session authorization metadata uses `parseMiningSession`; Direct
derives the address from the key. Any supplied owner or signer must match it.
Enabled configurations require a valid
primary URL and CA, Wi-Fi SSID, imported key and Operator enrollment. A configured
fallback independently requires its own CA. No trust is inherited across endpoints.
Disabled configurations may be incomplete, but supplied URLs, PEM and keys must
still be syntactically valid. Provisioning never verifies live chain authorization,
balances, NTP reachability or Wi-Fi credentials; the runtime must do those checks.

## Chunked certificate upload

Each endpoint accepts up to **4096 decoded PEM bytes**, independently:

```text
APLO1 10 CA_BEGIN primary 4096
APLO1 11 CA_CHUNK 0 <hex-of-first-up-to-192-PEM-bytes>
APLO1 12 CA_CHUNK 192 <hex-of-next-up-to-192-PEM-bytes>
APLO1 13 CA_END
```

Replace `primary` with `fallback` for the other endpoint. The declared size must
equal the final decoded size, including PEM line feeds. A bundle of roots can be
uploaded within this bound. `CA_BEGIN <endpoint> 0` followed by CA_END clears that
endpoint's CA. `CA_END` without CA_BEGIN is rejected.

Chunks append at the exact next offset. An identical already-appended chunk is
accepted again, allowing safe response-loss retry. Out-of-order, conflicting,
overlapping beyond the current end, NUL-containing or oversized chunks are rejected.
Only one upload buffer exists. CA_BEGIN replaces an unfinished upload; ABORT,
LOGOUT and staging expiry wipe it. COMMIT refuses an unfinished upload, even a
zero-length one. CA_END calls `Web3::isValidCertificate()` before replacing the
staged field. COMMIT revalidates both endpoint certificates. PEM is never returned.

Only the active snapshot, one staged Config and one PEM upload are retained;
serialization reserves its exact bounded size. Boot transfers decoded strings
with moves. ESP8266 readback compares 128-byte chunks instead of allocating a
second record; journal writes skip reparsing immutable configuration and cap the
signed transaction at MiningEmbed's 514 hex characters. Actual free heap and TLS
behavior on physical boards remain to be measured.

## Status, pending recovery and reset

`STATUS` needs no authentication. Its complete allowlist is:

```text
APLO1 20 OK status locked=0 enrolled=1 ready=1 pending=0 restart=0 mode=session state=mining journal=NONE
```

The flags are booleans. `mode` is `session`, `direct` or `unset`. `journal` is `NONE` or the public, validated transaction
hash; no raw signed bytes are exposed. State tokens accepted from runtime are
`storage_locked`, `unconfigured`, `paused`, `wifi`, `clock`, `mining`, `pending`,
`journal_error`, `provisioning`, `rpc_unavailable`, `session_invalid`,
`session_expired`, `budget_exceeded`, `insufficient_funds`, `not_staked`, `cooldown`,
`recovery_required`, `sign_failed`, `random_unavailable`, `setup`, `time`, `ready`,
`disabled`, `error`, `storage`, `connecting` and `offline`; all others become `other`.
When `locked=1`, the journal could not be trusted. Treat `journal=NONE` in
that condition as unavailable evidence for an erase or restore.

Send `APLO1 21 DIAG` to sample board diagnostics before provisioning or while mining.
It is available without authentication and returns numeric fields.

```text
APLO1 21 OK diag reset_reason=3 uptime_ms=1024 cpu_mhz=160 flash_bytes=4194304 psram_bytes=0 heap_free=28256 heap_largest=28040
```

`reset_reason` is the Arduino core's platform-specific code. On ESP8266, 1 is a
hardware watchdog reset and 3 is a software watchdog reset. `uptime_ms` is the
32-bit uptime counter. The heap values report the free bytes and largest free
allocation block at the time of the request. A board without PSRAM reports zero.

## Read-only queries

The new queries keep the APLO1 envelope and use fixed-order `key=value` fields.
The configurator exposes them as `info`, `config`, `mining-stats`, `wallet-info`
and `network-status`.

| Command | Reply label | Fields |
| --- | --- | --- |
| `GET_INFO` | `info` | `version`, `board`, `layout`, actual `flash_bytes`, `profile_flash_bytes`, `storage_offset`, `storage_size`, `storage_schema` |
| `GET_CONFIG` | `config` | `mode`, `enabled`, `chain_id`, `expiry`, `gaplo_budget`, `ssid_set`, `primary_set`, `fallback_set`, `primary_ca_set`, `fallback_ca_set`, `scheduler` |
| `GET_WALLET_INFO` | `wallet` | `address` (owner), `signer`. A missing address is `-` |
| `GET_NETWORK_STATUS` | `network` | `wifi` (inactive/connecting/connected), `clock_ready`, `rpc` (idle/busy/primary/fallback/unavailable) |
| `GET_MINING_STATS` | `stats` | `hashes`, `hashes_per_second`, `active_hashes_per_second`, `batch`, `confirmed`, `reverted` |

For example, run `python3 scripts/configure.py --port /dev/ttyUSB0 info`.
Numbers are decimal. A budget without a numeric value appears as `-`. The
configuration query returns presence flags for Wi-Fi and RPC settings. It exposes
no Wi-Fi credential, endpoint URL, certificate, Operator verifier, key or raw
transaction. The wallet query returns the public owner and signer addresses only.

Mining counters are per boot. `hashes` counts actual candidate hashes, excluding
checks for signing or validation. `hashes_per_second` uses wall time, including
network waits, cooldown and journal recovery. `active_hashes_per_second` measures
the most recent hash slice. `batch` is the next adaptive slice size.
`confirmed` and `reverted` count journal resolutions. Legacy `hashes_per_step`
seeds the adaptive scheduler and `interval_ms` limits the error/cooldown poll rate.
Hashing itself has no fixed interval. The scheduler targets about 6 ms per slice.
Read-only requests can run while reading an RPC response. Mutating commands wait
for the mining step to return. Core TCP/TLS handshakes remain synchronous and can
delay Serial responses.

## Recover a lost Operator token

Recovery requires a fresh signature by the configured **owner**. In Direct mode it
is the address derived from the imported key. In Session mode an authorized session
key alone cannot rotate the Operator token. Use a separate signer holding the owner
key. An enrolled board without a configured owner cannot prove ownership using
this API.

1. Generate a new random nonzero 32-byte token on the trusted USB host. Compute
   `SHA256("FirmwareEmbed/Operator/v1" || token_bytes)`.
2. Send `APLO1 <id> RECOVERY_CHALLENGE <verifier-hex>`. The board returns
   `OK recovery_challenge digest=<32-byte-hex> owner=<0x-address>`.
3. Verify the displayed owner and sign the exact digest with secp256k1. The
   compact signature is `r[32] || s[32] || recovery_id[1]`, with low S and
   recovery ID 0 or 1. Use raw digest signing without an Ethereum message prefix.
4. Send `APLO1 <id> RECOVER <signature-hex>` over the same console within
   120 seconds. The challenge permits one attempt and is bound to the candidate
   verifier, wallet and current configuration generation. The CLI subcommand
   `recover` prompts for the new token and signature with hidden input.

Successful recovery rotates only the Operator verifier and restarts. Configuration,
mining key and any pending transaction survive. It never writes or discards the
journal. If no owner key is available, reconcile the journal on the correct chain
before any external flash restore or erase. For a board that never received a
wallet, confirm the journal is empty before starting a new installation.

While a transaction is pending, authenticated BEGIN/SET/CA/COMMIT may repair Wi-Fi,
CA roots, NTP, polling settings or the Operator verifier. The separate journal stays unchanged. COMMIT rejects any change to mode,
key/presence, owner, signer, chain, expiry, GAPLO budget, enabled flag or either RPC
URL. This allows certificate/network repair without losing transaction evidence.
The original pending record is recovered after restart. Serial cannot clear a
journal except through the explicit reset below; normal resolution is MiningEmbed's
authenticated-chain reconciliation followed by the journal callback with nullptr.

After AUTH, a reset requires evidence matching STATUS's exact public hash (or NONE)
and explicit acknowledgement that the operator reconciled it externally:

```text
APLO1 30 RESET <journal-hash-or-NONE> ERASE_LOCAL_ONLY_I_HAVE_RECONCILED_CHAIN
```

Check receipt and signer nonce on the correct chain before acknowledging a pending
reset. This is an operator assertion, not proof checked through an untrusted RPC.
Reset writes a newer blank tombstone and requests restart. It never erases the old
slot first, never revokes a session on-chain, and never cancels a signed transaction.
The old or new complete state survives interruption of the tombstone commit.
This is **logical reset**, not secure erase: older flash pages/slots may retain
credentials and signed bytes. The implementation stores configuration and private
key in plaintext flash; it does not enable flash encryption, secure boot, hardware
attestation or protection against a host/attacker who can dump or rewrite flash.
The snapshot SHA-256 detects corruption; it is not an authenticity signature.

Errors are fixed tokens: `framing`, `unavailable`, `enrollment`, `auth`, `memory`,
`transaction`, `value`, `chunk`, `certificate`, `upload`, `commit`, `reset` and
`command` and `recovery`. A rejected value is not echoed. After an ambiguous COMMIT/reset response
loss, restart/reconnect and inspect STATUS before retrying; IDs do not deduplicate
all commands. Only identical CA chunks have explicit retry semantics.

## Persistent format and interruption policy

Config schema **2** uses the existing `APFC` slots. A separate version-1
`APFJ` journal has independent slots and generation counters. Both records use
a 52-byte little-endian envelope with magic, uint16 schema, uint16 reserved,
uint64 generation, uint32 payload length and a 32-byte SHA-256 checksum.
The checksum covers the first 20 bytes of the header and the entire payload.
Config stores the settings and the journal-generation floor for an authorized
factory reset. A journal record stores a pending flag and, when pending, the
nonce, hash, exact signed bytes, proof and maximum fee. Signed bytes must hash
to the saved hash. A journal blob is at most 768 bytes. It is written only when
a new transaction is staged or an outstanding one is cleared. Repeats and status
polling write nothing.

ESP32 keeps the `aplo_cfg` NVS partition and `device` namespace. It uses
`slot0`/`slot1` and `journal0`/`journal1` blob keys. ESP8266 uses
`/aplo0.bin`, `/aplo1.bin`, `/journal0.bin` and `/journal1.bin` in
LittleFS. LittleFS replaces one slot at a time by flushing a temporary file and
renaming it. Both backends verify readback before acknowledging a write.
On ESP8266 the firmware can format only an entirely erased, linker-verified
LittleFS region. An unmountable nonblank region locks the store.

At boot, the firmware loads the most recent complete record of each slot pair.
Missing or corrupted committed records, unknown schemas and nonconsecutive
generations lock the store. Uncommitted temporary files are ignored. Use the
USB Update procedure with a compatible layout and storage schema. Preserve
the transaction journal during firmware writes.
