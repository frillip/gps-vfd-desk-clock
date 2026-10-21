# ESP32 firmware (V2+)

_A network co-processor for a clock that would rather do its own timekeeping, thank you._

The ESP32 handles everything the dsPIC would prefer not to think about: WiFi, NTP, timezone lookups, the Telnet console and firmware updates. It talks to the PIC over UART using the SML protocol described in [common/SML_PROTOCOL.md](../common/SML_PROTOCOL.md), keeps its own NTP-disciplined time with a PPS output for the PIC, and passes on the PIC's console output.

The PIC remains in charge of UTC, local time and the display. The ESP supplies information and suggestions; the PIC decides what to do with them.

This firmware requires the V2+ dsPIC firmware in [dspic_src](../dspic_src). It no longer speaks the V2 0x83 protocol.

# Building

The firmware builds with the Arduino IDE 2 or arduino-cli, using the **esp32** core 3.3.x. It has been built and run with 3.3.9 to 3.3.12. Select *ESP32 Dev Module*, 16MB flash and the *Custom* partition scheme, which uses [partitions.csv](partitions.csv).

The libraries are git submodules in [libraries](libraries): WiFiManager, ESPTelnet, ArduinoJson and ESP32-OTA-Pull. Run `git submodule update --init`, then point the IDE's sketchbook (or `--libraries` for arduino-cli) at that folder. ezTime is no longer used.

The console and PIC link baud rates are set in [src/board.h](src/board.h). The PIC link rate must match the dsPIC's UART1 setting.

## Shared sources

The ESP compiles the same SML codec and protocol definitions as the dsPIC, so the two ends cannot quietly disagree about where a field is. Rather than keeping copies, the sketch uses symbolic links that mirror the repository layout:

| Link | Target |
| --- | --- |
| `src/common/enums.h`, `src/pic/sml/sml_proto.h`, `src/pic/sml/sml.c`, `src/pic/sml/sml.h` | `common/` |

`common/sml.h` includes `sml_proto.h` from its own folder, so `sml.c`, `sml.h` and `sml_proto.h` are linked side by side in `src/pic/sml/`. The links live under `src/` because the Arduino build only compiles source files in the sketch folder and in `src/`. Anything in another subfolder is copied into the build but never compiled, which produces some puzzling linker errors.

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
| `src/pic/` | The PIC link: SML framing, the PIC's console text, its held state, commands in both directions, pairing TIME-AUTH with the epoch edge. Also the PIC state report. `sml/` links to the shared SML codec |
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

The server is polled at startup, then after 64, 128 and 256 seconds while the crystal frequency is being learnt. After that the interval adapts between 128 seconds and `ntp.interval` (default 1024): it doubles after four corrections in a row under 500µs, and halves after one over 2ms. A crystal that is warming up, or sitting next to a radiator, gets polled more often; a settled one leaves the server in peace. Failed polls are retried after 60 seconds, doubling up to the interval.

`ntp.interval` is the longest interval rather than a fixed one. A clock that saved the old 1800-second default keeps it as its ceiling; `esp-ntp-set-interval 1024` and `esp-save` bring it in line.

The server's hostname is resolved in `loop()`, not in the NTP task. In arduino-esp32 3.3.x, `Network.hostByName()` clears lwIP's DNS cache without taking the TCPIP core lock, which cancels any lookup in progress elsewhere and trips an lwIP assertion. With the PIC already running, the timezone lookup at startup overlapped the NTP lookup, and the ESP restarted every few seconds until power cycled. Keeping every DNS lookup in `loop()` means none is ever in progress when the cache is cleared. Any future network code should follow the same rule.

## Clock model

The ESP keeps UTC as a model against its own microsecond timer: an anchor point, a frequency estimate and a slew in progress. The first NTP sample sets the clock, and a sample more than 128ms from the model steps it. Each sample is judged against the model at the moment it was taken, but the correction starts from the moment it is applied, so the clock never jumps by what the old slew would have done in the meantime.

Corrections up to 8ms are slewed in gently: spread over about two poll intervals, and never faster than 5ppm. The next sample replaces whatever is left, so each sample's noise is only half applied, and the ESP PPS moves by a few microseconds a second at most. That is slow enough for the PIC's NTP loop to follow without re-stepping, which is the point. Anything larger is slewed in at 100ppm and flagged in TIME-NTP (`CORRECTING`) while it lasts, since the PIC cannot follow it anyway and might as well know.

The frequency comes from a least-squares line through the raw offsets of the last eight samples, leaving out any whose round trip is well above the best of them: queueing adds delay in one direction, so the slow exchanges carry the most asymmetry. Two samples two minutes apart give a first estimate.

The model also reports an uncertainty bound: half the round trip, the server's own root delay and dispersion, any correction not yet slewed in, and a drift allowance that shrinks as the frequency becomes known.

In a 12-hour simulation with a crystal drifting from 4.9 to 2.9ppm, 12ms round trips, a few milliseconds of queueing and 3.5ms of path asymmetry, the ESP PPS wanders by 340–700µs (sd) around its asymmetry offset, against 550–1000µs for the previous model, and never moves faster than 7µs a second, against 100µs. The asymmetry offset itself, about 1.3ms here, is invisible to NTP and to any client of it.

## ESP PPS

The ESP PPS output on GPIO5 is a 100ms pulse whose rising edge marks the ESP's UTC second. It is generated by a 1MHz hardware timer alarm. At each falling edge the interrupt arms the next rising edge, at the time `loop()` planned from the clock model, or one second on (corrected for frequency) if `loop()` has been busy. A long HTTPS request therefore delays corrections, but does not cost an edge.

The interrupt reads the time immediately after setting the pin, so the edge latency is measured rather than assumed. On the V2 board it is around 5–7µs.

After each edge the ESP sends SML-TIME-NTP, labelling that edge with its UTC second, phase, uncertainty and the details of the last NTP update. The PIC uses this as a time source when GNSS is unavailable.

## PIC epoch

The PIC's epoch output on GPIO27 is timestamped in an interrupt and paired with the SML-TIME-AUTH message that labels it. This gives the ESP a UTC reference before NTP is available, and allows a direct comparison of the PIC's epoch against NTP in the diagnostics.

# Timezone

## tzinfo lookups

With tzinfo enabled, the ESP asks the timezone service (protocol v1) where it is and how local time works there:

```
GET https://<tzinfo.server><tzinfo.path>[?lat=...&lon=...]
```

The default is `rubidium.darksky.io/smol_clock/api/v1/tzinfo`. A path saved by the V2 firmware is migrated to the new one automatically.

tzinfo is enabled by default. Clocks with saved settings keep whatever they had; `esp-tzinfo-enable` and `esp-save` turn it on.

The position comes from the PIC's GNSS fix, rounded to `tzinfo.gnss_accuracy` decimal places using integer arithmetic before it leaves the ESP. With the setting at `-1`, or with no fix after 60 seconds, no position is sent and the server falls back to GeoIP.

The ESP looks up the timezone at startup, as soon as tzinfo is enabled, at the configured interval (or sooner if the server says so), after moving more than a whole rounding step, and on request. The 60-second wait for a fix at startup is skipped when the PIC reports no GNSS receiver at all, and when nothing is cached: a clock with no schedule would otherwise show UTC for a minute for no good reason. In that case GeoIP goes first, and the GNSS position refines it once there is a fix. Failed lookups are retried after 5 minutes, doubling up to an hour, or after 30 seconds, doubling up to 5 minutes, while there is no schedule at all. The latest result is cached in NVS and used at startup until a fresh lookup replaces it.

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

At startup the ESP sends a CMD-USER INFO request, which moves the PIC into SML mode and prompts it to print its status. STAT-NET every 10 seconds and TIME-NTP every second bring the PIC back into SML mode after it resets. The PIC then sends STAT-SYS (SML 1.4) every epoch: its firmware version, uptime and why it last reset. The ESP announces the PIC when it first hears from it, and again whenever the uptime drops.

Anything on the link outside a valid frame is treated as the PIC's console output and printed, as is STAT-DEBUG text. Both go to the USB-UART console and to Telnet.

PIC console commands are sent as CMD-USER requests, and the PIC's answer is printed on whichever console the command came from. Commands from the PIC to the ESP (codes from 0x8000, for example from the PIC's menu) are carried out and acknowledged.

The PIC's diagnostics are controlled from here too (SML 1.3):

| Command | Effect |
| --- | --- |
| `pic-show-sources [-v]` | The PIC's time sources as a table: verdict, bound, phase and when each was last good; `*` marks the one in use, `+` the one that would come next, `x` one discounted (its time beyond the accuracy limit, or inconsistent) and `?` one unusable (absent, or without time, pulse or label), as `chronyc sources` does |
| `pic-debug` | Shows which debug output is on |
| `pic-debug gnss sync` | Turns exactly that output on |
| `pic-debug-enable [names]` | Turns output on, all of it without names |
| `pic-debug-disable [names]` | Turns output off, all of it without names |

The names are `timing`, `gnss`, `sync`, `sources`, `ntp`, `minute` and `tuning`, or `all`, or a mask such as `0x1f`. The sources table also appears in the status (an empty line on the console) once the PIC sends STAT-SOURCES.

# Console output

Information is printed briefly by default, in the manner of chronyc: the ESP's NTP time and its server, a line each for the time zone, leap seconds, Wi-Fi and Telnet, then the PIC's firmware, uptime and last reset cause (STAT-SYS), its time, oscillator, sources and GNSS fix. Sources the PIC has never seen are left out of the table. `-v` after `status`, `pic-info`, `pic-show-sources`, `esp-ntp-info` or `esp-wifi-info` prints everything, as before: the counters, the sources' ranks, their own uncertainty figures, runs and edges, the epoch against NTP and the ESP's GNSS PPS, the PIC's reset bits and longest main-loop pass, and the link statistics. When the PIC's uptime drops, the console prints `PIC restarted: firmware 0.1.0, up 0 s, last reset: brown-out` (or whatever the cause was). `verbose on` makes that the default, an empty line included, until `verbose off` or a restart. A GNSS clock, for example:

```
ESP:      2.0.0 smolclock, up 00:41:07
NTP:      2026-10-21T10:53:20Z +/-13.7 ms, synced
Server:   pool.ntp.org (192.0.2.10), stratum 2, updated 00:12:40 ago
Zone:     Europe/London (GNSS), UTC+01:00, changes 2026-10-25T01:00:00Z
Leap:     TAI-UTC 37 s (GNSS)
Wi-Fi:    home, 192.168.1.20, -61 dBm
Telnet:   running, 192.168.1.5 connected

PIC:      0.1.0, up 00:40:58, last reset: power-on
UTC:      2026-10-21T10:53:20Z +/-61 ns, GNSS LOCKED
Local:    2026-10-21T11:53:20+01:00 DST
Freq:     -3.7500 ppm (TRACK)
Sync:     LOCKED on GNSS, synchronised
  Source    Verdict            Bound       Phase  Last good
* GNSS      good              61 ns      +25 ns       0s
+ HOLDOVER  good              86 ns           -      42s
  NTP       good            13.7 ms   -571.0 us       0s
GNSS:     3D OK, tAcc 12 ns
```

# Telnet

Telnet gets a copy of all console output, the PIC's included. That copy never blocks: whatever the connection cannot take at once is dropped, and a note saying how much appears in the stream when there is room again. A plain write to a stalled connection waits up to 10 seconds in the network stack, holding up `loop()` (and the PIC link, and TIME-NTP) meanwhile, and after three such failures the Telnet library used to drop the client. Replies to commands typed at Telnet are still written normally.

The status shows the ESP's uptime and why it last started. A restart through the EN pin, which is what a USB-UART's DTR and RTS lines do when a computer opens or closes the port, reports as "power-on or EN pin", so an ESP that restarted while the PIC kept counting epochs was most likely reset that way.

# Settings

Settings are stored in NVS in the same layout as the V2 firmware, so existing preferences carry over. Each field is checked on load. An invalid field reverts to its default and the rest are kept, rather than one bad value resetting everything. Changes made from the console take effect immediately and persist after `esp-save`.

The full command list is in the repository [README](../README.md#console-commands), and `help` prints it on the console. `esp-wifi-info` no longer prints the saved WiFi passphrase.
