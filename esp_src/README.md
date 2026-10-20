# ESP32 firmware (V2+)

_A network co-processor for a clock that would rather do its own timekeeping, thank you._

The ESP32 handles everything the dsPIC would prefer not to think about: WiFi, NTP, timezone lookups, the Telnet console and firmware updates. It talks to the PIC over UART using the SML protocol described in [common/SML_PROTOCOL.md](../common/SML_PROTOCOL.md), keeps its own NTP-disciplined time with a PPS output for the PIC, and passes on the PIC's console output.

The PIC remains in charge of UTC, local time and the display. The ESP supplies information and suggestions; the PIC decides what to do with them.

This firmware requires the V2+ dsPIC firmware in [dspic_src](../dspic_src). It no longer speaks the V2 0x83 protocol, apart from the single frame used to enter the PIC bootloader.

# Building

The firmware builds with the Arduino IDE 2 or arduino-cli, using the **esp32** core 3.3.x. It has been built and run with 3.3.9 to 3.3.12. Select *ESP32 Dev Module*, 16MB flash and the *Custom* partition scheme, which uses [partitions.csv](partitions.csv).

The libraries are git submodules in [libraries](libraries): WiFiManager, ESPTelnet, ArduinoJson and ESP32-OTA-Pull. Run `git submodule update --init`, then point the IDE's sketchbook (or `--libraries` for arduino-cli) at that folder. ezTime is no longer used.

The console and PIC link baud rates are set in [src/board.h](src/board.h). The PIC link rate must match the dsPIC's UART1 setting.

## Shared sources

The ESP compiles the same SML codec and protocol definitions as the dsPIC, so the two ends cannot quietly disagree about where a field is. Rather than keeping copies, the sketch uses symbolic links that mirror the repository layout:

| Link | Target |
| --- | --- |
| `src/common/enums.h`, `src/pic/sml/sml_proto.h`, `src/pic/sml/sml.c`, `src/pic/sml/sml.h` | `common/` |

Mirroring the layout means the relative include in `dspic_src/esp/sml.h` (`..\..\..\common\sml_proto.h`) resolves in the sketch exactly as it does in the repository. The links live under `src/` because the Arduino build only compiles source files in the sketch folder and in `src/`. Anything in another subfolder is copied into the build but never compiled, which produces some puzzling linker errors.

On Windows, symbolic links need Developer Mode (or an administrator prompt) and `git config core.symlinks true` before checkout. Alternatively, `mklink` in cmd from `esp_src\src`:

```bat
mklink common\enums.h      ..\..\..\common\enums.h
mklink pic\sml\sml_proto.h ..\..\..\..\common\sml_proto.h
mklink pic\sml\sml.c       ..\..\..\..\common\sml.c
mklink pic\sml\sml.h       ..\..\..\..\common\sml.h
```

Failing that, copying the four files into place also works, provided they are copied again whenever the originals change.

## Tests and formatting

The portable parts of the firmware have host tests: the clock model, NTP packet handling, tzinfo parsing (including a byte-for-byte check of the TZ-STATE frame against the protocol document), the frame and text splitter, and argument parsing. They build with gcc and run under the address and undefined-behaviour sanitisers:

```
cd tests/host && make
```

[tools/format.sh](tools/format.sh) applies the same house style as the dsPIC sources.

# Layout

| Folder | Contents |
| --- | --- |
| `esp_src.ino` | `setup()` and `loop()` |
| `src/board.h`, `src/version.h` | Pin map and baud rates, firmware version |
| `src/common/` | Links to the shared protocol definitions |
| `src/pic/` | The PIC link: SML framing, the PIC's console text, its held state, commands in both directions, pairing TIME-AUTH with the epoch edge. Also the PIC state report and the bootloader path. `sml/` links to the PIC's codec |
| `src/timing/` | NTP client and packet handling, the clock model, ESP PPS output, PIC epoch and GNSS PPS capture |
| `src/tz/` | tzinfo lookups and cache, TZ-STATE assembly, TIME-LEAP |
| `src/net/` | WiFi, Telnet, OTA updates, STAT-NET |
| `src/console/` | Console line input and output, the command table |
| `src/settings/` | Preferences stored in NVS |
| `src/util/` | Argument parsing, time formatting |

| Pin | Function |
| --- | --- |
| GPIO13 / GPIO12 | UART2 RX / TX to the PIC |
| GPIO27 | PIC epoch output, input to the ESP |
| GPIO5 | ESP PPS output, to the PIC |
| GPIO18 | GNSS PPS, only with the solder jumpers fitted |
| GPIO23 | Status LED, lit for 100ms on each ESP PPS edge while NTP is synced |

Almost everything runs from `loop()`. The exceptions are the NTP exchanges, which run in a task of their own, and the timing edges, which are handled in interrupts. Blocking work such as an HTTPS request holds up `loop()` but nothing else: the ESP PPS carries on in its interrupt, and the 4KB receive buffer on the PIC link holds several seconds of traffic.

# Time

## NTP

ezTime has been replaced by a small NTP client. Each poll sends four requests two seconds apart and keeps the exchange with the shortest round trip, since the fastest exchange has the least queueing and therefore the least asymmetry. The send and receive timestamps are taken immediately around the socket calls in the client task, so loop latency never reaches the measurement.

The server is polled at startup, then after 64, 128 and 256 seconds while the crystal frequency is being learnt, then at the configured interval (default 1800 seconds). Failed polls are retried after 60 seconds, doubling up to the interval.

The server's hostname is resolved in `loop()`, not in the NTP task. In arduino-esp32 3.3.x, `Network.hostByName()` clears lwIP's DNS cache without taking the TCPIP core lock, which cancels any lookup in progress elsewhere and trips an lwIP assertion. With the PIC already running, the timezone lookup at startup overlapped the NTP lookup, and the ESP restarted every few seconds until power cycled. Keeping every DNS lookup in `loop()` means none is ever in progress when the cache is cleared. Any future network code should follow the same rule.

## Clock model

The ESP keeps UTC as a model against its own microsecond timer: an anchor point, a frequency estimate and a slew in progress. The first NTP sample sets the clock. A sample more than 128ms from the model steps it. Anything smaller is slewed in at 100ppm, so the PPS output never jumps. The frequency is estimated from raw offsets at least 60 seconds apart.

The model also reports an uncertainty bound: half the round trip, the server's own root delay and dispersion, any correction not yet slewed in, and a drift allowance that shrinks as the frequency becomes known.

In simulation, with a crystal running 20ppm fast, 300µs of NTP noise and 30-minute polls, the frequency estimate settles within 0.001ppm, the error stays under 1ms after the first hour, and it never exceeds the reported uncertainty.

## ESP PPS

The ESP PPS output on GPIO5 is a 100ms pulse whose rising edge marks the ESP's UTC second. It is generated by a 1MHz hardware timer alarm. At each falling edge the interrupt arms the next rising edge, at the time `loop()` planned from the clock model, or one second on (corrected for frequency) if `loop()` has been busy. A long HTTPS request therefore delays corrections, but does not cost an edge.

The interrupt reads the time immediately after setting the pin, so the edge latency is measured rather than assumed. On the V2 board it is around 5–7µs.

After each edge the ESP sends SML-TIME-NTP, labelling that edge with its UTC second, phase, uncertainty and the details of the last NTP update. The PIC can use this as a time source when GNSS is unavailable.

## PIC epoch

The PIC's epoch output on GPIO27 is timestamped in an interrupt and paired with the SML-TIME-AUTH message that labels it. This gives the ESP a UTC reference before NTP is available, and allows a direct comparison of the PIC's epoch against NTP in the diagnostics.

# Timezone

## tzinfo lookups

With tzinfo enabled, the ESP asks the timezone service (protocol v1) where it is and how local time works there:

```
GET https://<tzinfo.server><tzinfo.path>[?lat=...&lon=...]
```

The default is `rubidium.darksky.io/smol_clock/api/v1/tzinfo`. A path saved by the V2 firmware is migrated to the new one automatically.

The position comes from the PIC's GNSS fix, rounded to `tzinfo.gnss_accuracy` decimal places using integer arithmetic before it leaves the ESP. With the setting at `-1`, or with no fix after 60 seconds, no position is sent and the server falls back to GeoIP.

The ESP looks up the timezone at startup, at the configured interval (or sooner if the server says so), after moving more than a whole rounding step, and on request. Failed lookups are retried after 5 minutes, doubling up to an hour. The latest result is cached in NVS and used at startup until a fresh lookup replaces it.

## Schedule horizon

The v1 response gives the current state and up to eight future transitions, but no horizon. The ESP infers one:

| Transitions received | Horizon |
| --- | --- |
| More than 8 | The ninth transition, exactly |
| Exactly 8 | One second after the last, since the server may have stopped there |
| Fewer than 8 | Open-ended, as the server's five-year window held no more |

## Sending to the PIC

The schedule goes to the PIC as SML-TZ-STATE when it changes, whenever the PIC reports that it holds no schedule (after a PIC reset), and every 10 minutes. Transitions already in force are folded into the current entry, so the PIC always receives as many future transitions as possible. Nothing is sent while tzinfo is disabled, and the PIC uses its manual offsets.

SML-TIME-LEAP is built from the PIC's own GNSS leap-second data and sent back to it, so the information survives the receiver losing its fix. The v1 service has no leap-second data of its own.

# PIC link

At startup the ESP sends a CMD-USER INFO request, which moves the PIC into SML mode and prompts it to print its status. STAT-NET every 10 seconds and TIME-NTP every second bring the PIC back into SML mode after it resets.

Anything on the link outside a valid frame is treated as the PIC's console output and printed, as is STAT-DEBUG text. Both go to the USB-UART console and to Telnet.

PIC console commands are sent as CMD-USER requests, and the PIC's answer is printed on whichever console the command came from. Commands from the PIC to the ESP (codes from 0x8000, for example from the PIC's menu) are carried out and acknowledged.

While the PIC bootloader is active the ESP neither reads from nor writes to the link. Entry still relies on the V2 PIC firmware; the V2+ firmware has no bootloader entry yet.

# Settings

Settings are stored in NVS in the same layout as the V2 firmware, so existing preferences carry over. Each field is checked on load. An invalid field reverts to its default and the rest are kept, rather than one bad value resetting everything. Changes made from the console take effect immediately and persist after `esp-save`.

The full command list is in the repository [README](../README.md#console-commands), and `help` prints it on the console. `esp-wifi-info` no longer prints the saved WiFi passphrase.
