# dsPIC firmware (V2+)

_The part of the clock that actually knows what time it is, and would like that noted._

The dsPIC33EP256GP504 keeps UTC, disciplines the clock's one-second epoch to the best available time source, and shows local time on the tubes. Each epoch edge is produced by the timer hardware, not by software, and everything else is arranged around it: the tubes latch on it, the GNSS receiver timestamps it, and the ESP is told which second it was.

The ESP32 supplies network time, timezone schedules, leap second data and user commands over UART, using the SML protocol described in [common/SML_PROTOCOL.md](../common/SML_PROTOCOL.md). It also carries the PIC's console output. The PIC listens, but it makes its own decisions.

This firmware replaces the V2 firmware and the MCC-generated code it was built on. It needs the V2+ ESP firmware in [esp_src](../esp_src) for SML, although it also runs alongside the V2 ESP firmware, which just passes its console output through.

# Building

The project is an MPLAB X project in [nbproject](nbproject), built with XC16 2.10 for the dsPIC33EP256GP504. No MCC is involved: the configuration bits, clock set-up and peripheral set-up are all in the source.

The SML codec and protocol definitions are shared with the ESP. They are not copied into this folder: the project includes `common/sml_proto.h`, `common/sml.c` and `common/sml.h`, along with `common/enums.h`, directly from [common](../common). The includes (`../../common/sml.h` and friends) rely on the repository layout, so keep the two folders side by side.

The ESP link runs at 921600 baud, set by `CONSOLE_BAUD_RATE` in [ui/console.h](ui/console.h), which must match the PIC link rate in the ESP's `src/board.h`. At 40MHz the dsPIC actually manages 909091 baud, 1.4% slow, which both ends tolerate. 1000000 baud would be exact on both sides, if it ever matters.

## GNSS receiver

The receiver must already be configured: provisioning has not been ported yet, so for now that means u-center. The firmware expects UBX on the module's UART1 at 115200 baud, with these messages enabled once per navigation solution:

| Message | Used for |
| --- | --- |
| NAV-STATUS, NAV-TIMEUTC | Fix, UTC, and the label for each PPS edge |
| NAV-CLOCK, NAV-POSLLH | Clock bias and drift, position (for timezone lookups) |
| NAV-TIMELS | Leap seconds, current and announced |
| NAV-EOE | End of each navigation epoch |
| TIM-TP | Quantisation error of the next PPS edge |
| TIM-TM2 | The receiver's own timestamp of the PIC's epoch edge |

MON-VER is polled at startup to identify the receiver. Anything else the receiver sends is counted by class and ID in the GNSS report, so a stray message is easy to spot. NAV-DOP can be turned off.

## Tests and formatting

Most of the firmware has host tests, built with gcc against stand-ins for the hardware:

- the fixed-point Kalman filter and servo against double precision;
- the epoch timer, scheduler and OC1 output against a cycle-level model;
- the control loop, closed through a model of the oscillator and receiver;
- source selection, holdover and leap seconds;
- the UART drivers, the UBX parser and the GNSS service against a simulated receiver;
- the tube wire format, bit for bit against the V2 encoder;
- the SML codec, including golden frames and a 20000-frame fuzz test;
- the ESP link and payloads, the time zone schedule, the calendar (every day from 1970 to 2106) and the report formatter.

```
cd tests/host && make
```

[tools/format.sh](tools/format.sh) applies the house style: clang-format with [.clang-format](.clang-format), then [tools/style.py](tools/style.py). Firmware sources use CRLF line endings, the host tests LF.

# Layout

| Folder | Contents |
| --- | --- |
| `main.c`, `scheduler.c/.h` | The task list and the cooperative scheduler that runs it |
| `platform/` | Clock and configuration bits, the pin map, millisecond helpers, UTC and calendar arithmetic |
| `drivers/` | UART1 (the ESP link), UART2 (GNSS), the tube drivers |
| `timing/` | PPS capture, the epoch timer and output, the control loop, and its console report |
| `sync/` | Time source evaluation and selection, holdover, and the UTC second of each epoch |
| `gnss/` | UBX parsing, decoding and encoding, receiver detection, epoch assembly, the GNSS state |
| `esp/` | The ESP link and the SML payloads built from the clock's state |
| `ui/` | Console, display modes and frames, time zones, the fast text formatter |
| `tests/host/` | Host tests and the stand-ins they run against |

| Pin | Function |
| --- | --- |
| RA8 | GNSS PPS, captured by IC1/IC2 |
| RB10 | Epoch output (OC1): latches the tubes, captured by IC3/IC4, timed by the receiver |
| RB2 / RB3 | UART2 RX / TX to the GNSS receiver |
| RC6 / RC7 | UART1 RX / TX to the ESP32 |
| RB15 | ESP PPS, input |
| RC8 / RC9 | SPI2 clock / data to the tube drivers |
| RB11 | Tube BLANK, PWM from OC3 for brightness |
| RB12 | Tube latch, ORed with the epoch output for immediate updates |
| RA10 | Status LED, lit for the 100ms epoch pulse |
| RA7 / RA0 | Push button / toggle switch |
| RB7 | Buzzer |
| RB8 / RB9 | I2C1, not used yet |
| RB13 | Not connected: pulsed at startup to check that the capture timebases agree |

The full pin table, with the remappable pin numbers, is at the top of [platform/board.h](platform/board.h). Code elsewhere uses the function names, never the port names.

Almost everything runs from the main loop. The scheduler there reads its time from the epoch timer, so tasks keep their place in the second:

| Task | When |
| --- | --- |
| Epoch work | 1ms after each epoch: measure, steer, label the second, send the ESP its messages, render the next second's frame, print the epoch report |
| GNSS and ESP link service | Every 10ms |
| Display load | 500ms into each second, after the latch has closed |
| Minute reports | 600ms into the second, one report per epoch at 15, 30 and 45 seconds into each minute |

Only the timing edges, the 1ms slot timer and the UARTs use interrupts. The epoch timer interrupts run at priority 4 and the captures and UARTs at priority 1, so nothing in the foreground can delay an epoch. The scheduler report shows each task's worst lateness, its longest run and its share of the time.

# Time

## The epoch

Timers 2 and 3 form a 32-bit epoch timer clocked at 40MHz. Its period match triggers OC1, which raises RB10 for 100ms: that rising edge **is** the second, with no interrupt latency anywhere in its path. The length of each second is set by loading the next period, with fractional ticks carried forward so nothing is lost to rounding.

IC1/IC2 capture the GNSS PPS and IC3/IC4 capture the epoch output on a common, free-running timebase. The difference between the two is the clock's phase error, measured to one tick (25ns).

## Control loop

Each epoch, `timing/epoch_control.c` decides the length of the next second:

| State | What happens |
| --- | --- |
| ACQUIRE | Learn the reference's frequency for a few periods, then step the epoch onto its edge in one go |
| STEP | Follow the step at the estimated frequency |
| TRACK | Steer the phase with a PI servo on top of a Kalman frequency estimate, rejecting outliers |

All the arithmetic is fixed point. A period requested after epoch N only runs from N+1, so the loop acts on the phase it predicts for N+1. The PPS edge arrives asynchronously and its capture is rounded down, so the loop aims for an average measured phase of half a tick: an even split between 0 and +1.

On the V2 board against a NEO-M9N, the measured phase sits at 0 or +1 tick in 90–98% of epochs. The receiver's own timing of the epoch edge (TIM-TM2) averages around +44ns, the fixed delay through the latch logic, with a standard deviation of 12–15ns. The 40MHz oscillator is around 1.6–1.9ppm slow, which the Kalman filter takes care of.

## Sources and holdover

`sync/` decides which source the loop may steer by, and when:

| State | Meaning |
| --- | --- |
| FREE_RUN | No source: the epoch runs on the last frequency estimate, and the time of day, if ever known, is counted on |
| ACQUIRE | A source has qualified: the loop is learning its frequency and stepping onto its edge |
| LOCKED | The loop is tracking the source |
| HOLDOVER | The source has failed: the loop holds frequency, still counted as synchronised for up to an hour |

A source qualifies after 3 consecutive good epochs, and a better-ranked one takes over after 10. GNSS is taken up when its reported time accuracy is within 10µs, and kept while it is within 100µs. Even then it is ten times better than NTP. While no source is selected, the per-epoch report says why each one is not.

Only GNSS exists as a source so far. Adding one takes an entry in the source table and an adapter that reports its edge, its UTC label and its accuracy. NTP via the ESP PPS is next.

## Time of day

Times are 32-bit Unix seconds throughout, good until 2106, with no 64-bit arithmetic. Each epoch is labelled with the UTC second it marks, from the selected source, and counted on when there is none.

Leap seconds come from the receiver's NAV-TIMELS. Failing that, they come from SML-TIME-LEAP from the ESP. An inserted second is shown as 23:59:60, whether or not a source is available at the time.

# Display and time zones

The tube driver takes a logical frame (segments, separators and dots) and turns it into the V2 wire format, so the wiring lives in one place. Each frame is rendered a second ahead, shifted into the drivers at 500ms, and latched by the next epoch pulse. The tubes therefore change exactly on the second, and the display can be no later than the clock. Brightness is a 10kHz PWM on the BLANK input.

Only the HH:MM mode has been ported so far. The colon blinks with the seconds while the clock is synchronised and stays lit while it is free running. Dashes are shown until the time of day is known.

Local time is UTC plus offsets. The clock itself never moves. The PIC keeps the latest SML-TZ-STATE schedule from the ESP: the entry in force and up to eight future transitions. It moves to each new entry at its UTC second. Two settings decide what is taken from the schedule:

| Setting | On | Off |
| --- | --- | --- |
| TZ auto | Base offset from the schedule | Manual base offset |
| DST auto | DST offset and state from the schedule | Manual DST offset and state |

Without a schedule, both fall back to the manual values. Past the end of a schedule the last entry is held and flagged as stale, rather than quietly reverting to UTC.

# ESP link

The PIC starts in text mode, sending its console output raw, which is what the V2 ESP firmware expects. On the first valid SML frame from the ESP it switches to SML mode for good. From then on the console text travels in STAT-DEBUG frames, and each epoch the PIC sends TIME-AUTH, TIME-PIC, TIME-CTRL, TIME-MEAS, TIME-GNSS and STAT-UI, about 300 bytes a second.

TIME-AUTH goes first, within a few milliseconds of the edge it labels. TIME-MEAS carries the raw measurements of each epoch, which the ESP needs for statistics and Allan deviation.

From the ESP, the PIC acts on:

- TZ-STATE;
- TIME-LEAP;
- user commands: INFO, message requests, reset, time zone and DST settings, display mode and brightness;
- STAT-NET and TIME-NTP, which it holds for the menu and for the future NTP source.

Commands it does not support yet are answered with a NACK, never ignored. The PIC can also send commands of its own to the ESP, such as resetting WiFi.

# Console

The console output goes to the ESP, which shows it on its USB-UART console and over Telnet. Each epoch prints five lines:

- the GNSS capture and frequency estimate;
- the epoch capture, phase and loop state;
- the servo terms;
- the receiver's view of the edge;
- the synchroniser's state.

Once a minute, at 15, 30 and 45 seconds past, the scheduler, GNSS and sync reports follow. The sync report also includes the display, time zone and ESP link.

XC16's printf turned out to cost about 1.5ms a call, and the five epoch lines took 67ms of every second. They are now built with the small formatter in [ui/text.c](ui/text.c), which produces identical output in about 2ms. The minute reports still use printf, so they run late in the second, clear of the epoch work and the GNSS traffic that follows each navigation solution.

# Not yet ported

The V2 firmware did rather more than this. Still to come:

- the menu, buttons, buzzer and alarm;
- the other display modes and the 12-hour format;
- the I2C sensors and automatic brightness;
- the RTC;
- settings in flash;
- GNSS provisioning;
- NTP as a time source;
- the watchdog;
- bootloader entry.

The SML messages for most of these are already defined.
