# Firmware architecture

Each directory under `Firmware/` is one complete board project. It owns its
PlatformIO settings, boot and console setup, network and clock code, flash
driver and partition table where applicable. Add a board by creating a new
directory with its own `platformio.ini`, source and `[release]` flash metadata.
GitHub Actions discovers the project and builds a copy of it in isolation.

- **AploEmbed** provides strict HTTPS RPC, endpoint-specific CA trust and exact-width
  transaction signing. On ESP8266 it reuses the BearSSL client, checks 1024-byte
  MFLN and streams large receipts.
- **MiningEmbed** provides both modes, the transaction journal and recovery.
  Its portable provisioning module implements APLO1, validates versioned snapshots
  and uses the board's flash backend. It has no board selection or partition table.
- **The board project** chooses the physical console, mounts NVS or LittleFS and
  starts Wi-Fi and SNTP. It applies committed settings at restart and keeps the
  active CA buffers alive. The adaptive scheduler gives hashing short slices and services Serial between them.
  The HTTPS reader services public queries cooperatively. Core handshakes can delay
  a response.

The USB protocol and `scripts/configure.py` are the same for all boards. The
physical USB host is trusted during provisioning. Stock images store keys in
onboard flash in plaintext, including the full account key in Direct mode.
