# Full and Lite compatibility

Each folder is a standalone PlatformIO project. All rows refer to the pinned
frameworks and library commits in the release. Device profiles in the release
are marked experimental for community hardware testing.

| Edition | Environment | Required flash | Application limit | Preserved storage |
| --- | --- | ---: | ---: | --- |
| Full | esp32dev | 4 MiB | 2,621,440 B | NVS at 0x290000 |
| Full | esp32dev-2m | 2 MiB | 1,572,864 B | NVS at 0x190000 |
| Full | esp32-c3-devkitm-1 | 4 MiB | 2,621,440 B | NVS at 0x290000 |
| Full | esp32-wroom-32u | 4 MiB | 2,621,440 B | NVS at 0x290000 |
| Full | esp32-s3-n16r8 | 16 MiB; 8 MiB PSRAM | 13,107,200 B | NVS at 0xc90000 |
| Full | esp12e | 4 MiB | 1,044,464 B | LittleFS at 0x300000 |
| Full | esp8266-2m | 2 MiB | 1,044,464 B | LittleFS at 0x1e0000 |
| Full | esp01-1m | 1 MiB | 892,912 B | LittleFS at 0xdb000 |
| Lite | lite-esp8266-512k | 512 KiB | 499,696 B linker capacity | 4 KiB guard at 0x7b000 |
| Lite | lite-esp32-1m | 1 MiB | 978,944 B factory slot | 4 KiB guard at 0xff000 |

The Full capacity diagnostics are the 512 KiB ESP8266 with a 64 KiB
LittleFS slice and the 1 MiB ESP32 with a 64 KiB NVS slice. Their capacities
are 434,160 and 851,968 bytes. CI requires size-related failure on both and
lists them as exclusions without downloadable images. Lite has separate
layouts and its own binary-size gate.

## ESP8266 flash, static RAM and heap

Espressif describes SRAM and ROM inside the ESP8266EX and external SPI flash
for the program. Different flash capacities on the same ESP8266EX change
image and storage capacity. They leave the SoC memory architecture unchanged.
The datasheet describes under 50 kB of user-accessible Heap + Data for its
connected station-mode SDK configuration. That figure applies to the stated SDK
conditions. [Espressif ESP8266EX datasheet §3.1.2–3.1.3](https://www.espressif.com/sites/default/files/documentation/0a-esp8266ex_datasheet_en.pdf)

PlatformIO reports static RAM against its linker capacity of 81,920 bytes
for these ESP8266 environments. CI applies a stricter 43,008-byte budget to
the Full profiles and checks the configured budget of each Lite profile.
Free heap and its largest contiguous block during Wi-Fi, X.509 parsing and
TLS are runtime measurements. Release compilation does not establish those
values for a module. ESP8266 RPC endpoints must negotiate 1024-byte MFLN.

Check the installed SPI flash size on ESP-01/ESP-01S rather than inferring it
from the model name. Full uses the 1 MiB ESP-01 profile. Lite targets the
512 KiB map with the SDK sectors and the signing guard reserved. Full and Lite
use different maps on smaller devices. Select the same layout when updating.

[Build checks and hardware evidence](validation.md) · [USB Install and Update](flash.md)
