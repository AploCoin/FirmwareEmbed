# Release validation

The v0.1.0 release workflow runs the SDK, TLS, mining, provisioning, USB
flasher and host configurator tests. It exercises both Lite journal adapters
with torn writes and erases. It compiles each published profile in an isolated
folder using the locked AploEmbed and MiningEmbed commits, then checks linker
Flash/static RAM and the sector-rounded size of the actual binary. The manifest
reports per-profile resources. The source archive contains the three exact Git
trees used by CI, and SHA256SUMS covers the archive and firmware assets.

For each Lite profile, CI also compiles both wallet modes with a valid public
root CA and synthetic configuration. The manifest's `configuredBuilds` field
reports padded binary bytes and static RAM separately. Only the inactive
template enters the release asset directory.

TLS regression fixtures cover delayed connections, bounded response writes and
reads, partial responses, deadlines, and failover. ESP32 Full pumps limited
read-only APLO1 replies on the caller during the TLS worker's wait. ESP8266 Full
uses a recurrent callback at the Arduino core's cooperative yields. Mutating
Serial operations wait for the RPC step to finish. The fixtures test console
backpressure and deferred mutations. The actual delay and available heap on
devices vary with the board, certificate chain and network.

## Hardware evidence

The earlier WROOM-32U bench run exercised native AploNode mining in both
modes, config-preserving USB update, in-flight transaction recovery and TLS
fallback on its then-current image. It used a separate Docker test chain.
Hardware evidence for v0.1.0 requires flashing this release's exact profile.
The manifest marks the current profiles experimental for community testing.

Useful physical checks are sustained mining in both modes, USB responses
during connection faults, minimum free heap and largest block during TLS,
and reset and power interruption while a journal entry is outstanding.
On Lite, verify the signing lock after a power cut and keep the sector intact
until you reconcile the transaction. On C3, test both USB Serial/JTAG and
the UART bridge. On ESP8266, verify negotiated MFLN with the actual RPC.
