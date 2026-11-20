# Lite setup and transaction recovery

Lite projects are standalone at [Lite-ESP8266](../Firmware/Lite-ESP8266) and
[Lite-ESP32](../Firmware/Lite-ESP32). Select the project matching the flash size
reported by the chip. The public binary is an inactive template. Each project
uses `include/DeviceConfig.h`, which loads the ignored
`include/DeviceConfig.private.h` when it exists.

1. Copy `include/DeviceConfig.example.h` to `include/DeviceConfig.private.h`.
   Set the Wi-Fi credentials, HTTPS RPC URL, trusted CA PEM, NTP names and
   positive chain ID. Use a DNS name covered by the certificate. ESP8266 RPC
   must negotiate 1024-byte MFLN. Set a fallback URL only with its own CA.
2. For Direct Private Key, set `kMode = Mode::Direct` and import the 32-byte
   key of the funded, staked owner. For Session Key, set `Mode::Session`, the
   approved owner and signer addresses, the finite GAPLO allowance and the
   absolute expiry block. Import the session signer key. Follow the
   [session registration guide](sessions.md).
3. Set `kConfigured = true`. Build with `pio run` from the selected folder.
   The build checks the actual application binary boundary. The 1 MiB ESP32
   has a tight budget. Use a compact configuration, starting with one trusted
   root PEM and short RPC URLs. A larger CA bundle or second root can exceed
   the slot. Keep certificate verification enabled.
4. Keep the private header and each personal binary in private storage.
   They contain the key and Wi-Fi password. Use [Install or Update](flash.md)
   with the matching release manifest.

Lite reserves one 4 KiB signing-guard sector at `0x7b000` on ESP8266 and
`0xff000` on ESP32. It verifies a record write before any broadcast. A live
retry uses the original signed bytes under the usual chain, receipt, nonce and
authorization checks. A terminal receipt for the exact hash permits erasing
the guard. On startup, any non-erased byte or flash read failure blocks signing.
The ESP8266 profile reserves the sector without LittleFS.

After an interrupted transaction, keep the guard and the previous personal
binary until the transaction is reconciled. Read the named sector through USB
and inspect it off-device. A complete record starts with ASCII `APL1`, followed
by the little-endian 64-bit transaction nonce and the ASCII `0x…` hash. Verify
the receipt by hash and reconcile latest/pending nonce and signer on the
correct chain. A missing receipt alone cannot prove that the old transaction
will never execute. If the write was torn or the outcome remains uncertain,
leave the guard set. Erase only that guard sector after a conclusive
reconciliation, then reboot.

Lite compiles without APLO1 and reads its settings at boot. The hardware status
in the release manifest is **experimental / community testing**.
