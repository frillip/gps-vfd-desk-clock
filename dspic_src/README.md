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
- source selection, holdover and leap seconds, and the choice between GNSS, NTP and holdover, closed through a model of both PPS inputs;
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
| `drivers/` | UART1 (the ESP link), UART2 (GNSS), the tube drivers, I2C1 and the PCF8563 RTC |
| `timing/` | PPS capture, the epoch timer and output, the control loop, and its console report |
| `sync/` | Time source evaluation and selection, holdover, and the UTC second of each epoch |
| `gnss/` | UBX parsing, decoding and encoding, receiver detection, epoch assembly, the GNSS state |
| `esp/` | The ESP link and the SML payloads built from the clock's state |
| `ui/` | Console, display modes and frames, time zones, the fast text formatter |
| `tests/host/` | Host tests and the stand-ins they run against |

| Pin | Function |
| --- | --- |
| RA8 | GNSS PPS: watched always, captured by IC1/IC2 unless NTP is selected |
| RB10 | Epoch output (OC1): latches the tubes, captured by IC3/IC4, timed by the receiver |
| RB2 / RB3 | UART2 RX / TX to the GNSS receiver |
| RC6 / RC7 | UART1 RX / TX to the ESP32 |
| RB15 | ESP PPS: watched always, captured by IC1/IC2 while NTP is selected |
| RC8 / RC9 | SPI2 clock / data to the tube drivers |
| RB11 | Tube BLANK, PWM from OC3 for brightness |
| RB12 | Tube latch, ORed with the epoch output for immediate updates |
| RA10 | Status LED, lit for the 100ms epoch pulse |
| RA7 / RA0 | Push button / toggle switch |
| RB7 | Buzzer |
| RB8 / RB9 | I2C1 on its alternate pins ASCL1/ASDA1 (ALTI2C1 = ON): the PCF8563 RTC (and, to come, the light sensor) |
| RB13 | Not connected: pulsed at startup to check that the capture timebases agree |

The full pin table, with the remappable pin numbers, is at the top of [platform/board.h](platform/board.h). Code elsewhere uses the function names, never the port names.

Almost everything runs from the main loop. The scheduler there reads its time from the epoch timer, so tasks keep their place in the second:

| Task | When |
| --- | --- |
| Epoch work | 1ms after each epoch: measure, steer, label the second, send the ESP its messages, render the next second's frame, print the epoch report |
| GNSS and ESP link service | Every 10ms |
| Display load | 500ms into each second, after the latch has closed |
| Minute reports | 600ms into the second, one report per epoch at 15, 30 and 45 seconds into each minute |

Only the timing edges, the 1ms slot timer and the UARTs use interrupts. The epoch timer interrupts run at priority 4, the PPS watch at 2, and the captures and UARTs at 1, so nothing in the foreground can delay an epoch. The scheduler report shows each task's worst lateness, its longest run and its share of the time.

# Time

## The epoch

Timers 2 and 3 form a 32-bit epoch timer clocked at 40MHz. Its period match triggers OC1, which raises RB10 for 100ms: that rising edge **is** the second, with no interrupt latency anywhere in its path. The length of each second is set by loading the next period, with fractional ticks carried forward so nothing is lost to rounding.

IC1/IC2 capture the reference PPS and IC3/IC4 capture the epoch output on a common, free-running timebase. The difference between the two is the clock's phase error, measured to one tick (25ns). At start-up Timer1 resets all four capture timers on the same cycle, and one strobe edge on RB13, captured by both pairs, proves it. The check is retried from scratch up to 8 times. If the pairs never agree but keep disagreeing by the same tick or two, that offset is taken off every reference capture instead. The reference is the GNSS PPS, or the ESP's PPS while NTP is the selected source: the capture is only ever rerouted when a source is selected.

Both PPS pins are also watched by interrupt-on-change, whichever one is captured. Each rising edge is numbered and timestamped from the same timebase, to within a microsecond or so of interrupt latency. That is enough to show a source's pulses are arriving and where they are, and it is what the edges are labelled by, so GNSS labelling carries on while the capture follows the ESP. Steering still needs the capture.

## Control loop

Each epoch, `timing/epoch_control.c` decides the length of the next second:

| State | What happens |
| --- | --- |
| ACQUIRE | Learn the reference's frequency for a few periods, then step the epoch onto its edge in one go |
| STEP | Follow the step at the estimated frequency |
| TRACK | Steer the phase with a PI servo on top of a Kalman frequency estimate, rejecting outliers |

All the arithmetic is fixed point. A period requested after epoch N only runs from N+1, so the loop acts on the phase it predicts for N+1, at its best estimate of the true second (the Kalman estimate less the servo integral). The PPS edge arrives asynchronously and its capture is rounded down, so the loop aims for an average measured phase of half a tick: an even split between 0 and +1.

How hard the loop follows its reference depends on the source, by profile:

| Profile | Used for | Servo | Gate | Kalman |
| --- | --- | --- | --- | --- |
| precise | GNSS | Kp 1/8, Ki 1/256: time constant ~8s, up to 0.4ppm of slew | 400ns | Learns the oscillator's frequency |
| gentle | NTP | Kp 1/64, Ki 1/16384: time constant ~2 minutes, up to 10ppm of slew | 300µs | Learns nothing: the ESP slews its own clock |

Both are critically damped. The NTP profile averages out the microseconds of jitter on the ESP's PPS, and corrects the ESP's edge by the offset the ESP reports for it in TIME-NTP. A clock that has never had GNSS has no frequency estimate, and the servo alone would take minutes to find the oscillator's error, a few hundred microseconds out of phase meanwhile. So while acquiring NTP it measures the ESP's average period over 16 seconds once, and starts the Kalman filter from that. Two things in TIME-NTP help further. The ESP's own estimate of each edge arrives after the PIC has measured it, and wanders by microseconds, by tens just after a correction, so the edge is corrected by its running average. And while the ESP steps or slews its clock fast (TIME-NTP `CORRECTING`), the phase jump is the ESP's time moving, so the servo works it off with its integral held rather than taking it for a frequency error. Routine corrections, slewed in at 5ppm or less, are simply followed. In the model, a 150µs correction then settles in about 3 minutes with 1.6µs of overshoot, instead of 8 minutes with 21µs.

On the V2 board against a NEO-M9N, the measured phase sits at 0 or +1 tick in 90–98% of epochs. The receiver's own timing of the epoch edge (TIM-TM2) averages around +44ns, the fixed delay through the latch logic, with a standard deviation of 12–15ns. The 40MHz oscillator is around 1.6–1.9ppm slow, which the Kalman filter takes care of.

## Sources and holdover

`sync/` decides which source the loop may steer by, and when:

| State | Meaning |
| --- | --- |
| FREE_RUN | No source: the epoch runs on the last frequency estimate, and the time of day, if known, is counted on |
| ACQUIRE | A source has been chosen: the loop is learning its frequency and stepping onto its edge, or slewing onto it if it is already close |
| LOCKED | The loop is tracking the source |
| HOLDOVER | The source has failed: the loop holds frequency, still counted as synchronised while its uncertainty is within 100ms |

The sources are GNSS and NTP, and holdover on our own oscillator is treated as a third. Each carries a ± bound on its time error, and the choice is made on those bounds alone:

| Candidate | Bound |
| --- | --- |
| GNSS | 3 × tAcc + 25ns |
| NTP | The ESP's own bound + 20µs for the jitter of its PPS |
| Holdover | u0 + f·t + ½·D·t², from the bound when the loop last steered (u0), the frequency error that source leaves (f: 40ns/s after GNSS, 1µs/s after NTP), and the oscillator's drift rate (D: 5ppt/s) |

A source becomes a candidate after 3 consecutive good epochs. A candidate takes over from the one in use when its bound is less than half of it for 10 epochs in a row. Holdover takes over at once when the source in use fails, so when GNSS is lost the clock holds over, and only moves to NTP once NTP is twice as good. With the holdover model measured on the bench, that is after about 30 minutes with NTP at ±20µs, about 9 hours with NTP at ±2ms, and about a day at the ±11ms an ESP reports over Wi-Fi. GNSS is taken up when its reported time accuracy is within 10µs, and kept while it is within 100µs. NTP is taken up while the ESP's bound is within 50ms, and kept while it is within 100ms: the ESP's bound covers the whole Wi-Fi round trip, so ~10ms is normal.

Those limits decide what competes, not whether the clock runs. In free run, with nothing within its limit, a source that meets every other requirement (present, time valid, its pulses arriving, labelled, consistent) for 3 epochs is taken as a fallback, however coarse: a receiver just after its first fix, or an ESP that only knows its time to tens of milliseconds. It is steered by and kept while it meets those requirements, and anything twice as good displaces it. Until the bound in use is within 100ms, the clock does not call itself synchronised. The time of day does not wait for any of this: it is set from the best labelled source the first epoch there is one.

Selecting a source routes the capture to it if need be, then restarts the loop with that source's profile. If the loop was tracking and the new source's edge is already within half the profile's gate of the epoch, the loop slews onto it without a step. A handover from GNSS holdover to NTP usually goes that way; one from NTP back to GNSS steps.

The GNSS holdover model comes from a night of virtual holdover trials on a V2 board (`sync/sync_tuning.c`, see [Console](#console)). Against GNSS, the held frequency drifted by 0.8µs at most after a minute, 23µs after ten, 50µs after an hour and 96µs after two (RMS 0.12, 3.8, 15 and 34µs). The bound is set at about three times the worst case, to leave room for temperature: 2.4µs, 25µs, 176µs and 420µs. Holdover after NTP is dominated by the ESP's own time: its frequency wanders by about 2ppm between its 30-minute polls, so 1µs/s.

The RTC is a source of the time of day only. On V2 it has no pulse the capture can take, so it never steers and is never a candidate, but it is judged like the others with a bound of ±999ms. At start-up it is usually the first source with a time. The time of day is taken from it, and while the clock is still free running, the epoch is moved once onto the RTC's second (see [I2C and the RTC](#i2c-and-the-rtc)), so the seconds shown change when the RTC's do. GNSS or NTP then take over as usual.

Adding a source takes an entry in the source table, an adapter that reports its edge, its UTC label and its uncertainty, and a loop profile.

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

STAT-SOURCES, the synchroniser's candidates ranked, follows whenever the choice or a verdict changes, every 10 seconds, and on request; the ESP shows it with `pic-show-sources`. STAT-SYS carries the firmware version (`platform/version.h`), the uptime, why the PIC last reset (RCON, captured and cleared first thing in `system_init()`) and the longest main-loop pass. It goes in the first epoch in SML mode, so the ESP hears about a restart at once, and every 10 seconds after. The same version and reset cause are printed at start-up and in `pic-info`. TIME-AUTH goes first, within a few milliseconds of the edge it labels. TIME-MEAS carries the raw measurements of each epoch, which the ESP needs for statistics and Allan deviation.

From the ESP, the PIC acts on:

- TZ-STATE;
- TIME-LEAP;
- user commands: INFO, message requests, reset, time zone and DST settings, display mode and brightness, the sources (SHOW_SOURCES) and the debug mask (SET_DEBUG);
- TIME-NTP, which labels the ESP's PPS for the NTP source;
- STAT-NET, which it holds for the menu.

Commands it does not support yet are answered with a NACK, never ignored. The PIC can also send commands of its own to the ESP, such as resetting WiFi.

# Console

The console output goes to the ESP, which shows it on its USB-UART console and over Telnet. What is printed is set by a mask of subsystems in [ui/debug.h](ui/debug.h). `DEBUG_DEFAULT` sets it at build time, nothing by default (`-DDEBUG_DEFAULT=DEBUG_ALL` for a bench session); the ESP changes it with SML `SET_DEBUG` (`pic-debug`, `pic-debug-enable`, `pic-debug-disable`), and the PIC prints the result. It is not kept over a reset yet. State changes are always printed.

| Bit | Name | What |
| --- | --- | --- |
| 0x01 | TIMING | Per epoch: the reference capture and frequency estimate, the epoch capture, phase and loop state, the servo terms, the receiver's view of the edge |
| 0x02 | GNSS | Per epoch: the GNSS edge and its label |
| 0x04 | SYNC | Per epoch: the synchroniser's state |
| 0x08 | SOURCES | Per epoch: every candidate, best first, with its bound, verdict and phase |
| 0x10 | NTP | Per epoch: the latest TIME-NTP and the edge it labels |
| 0x20 | MINUTE | Each minute, at 15, 30 and 45 seconds past: the scheduler, GNSS and sync reports. The sync report also covers the candidates, the display, time zone and ESP link |
| 0x40 | TUNING | Each minute, at 55 seconds past: the bench statistics below |

The sources line marks the candidate in use with `*` and the second choice with `+`, and says whether each source's phase comes from its captured edge (`cap`) or its watched one (`ioc`):

```
   SOURCES: 1*GNSS 115ns good 412 cap +1 | 2+HOLD 140ns 0s | 3 NTP 2020us good 412 ioc -1203 |
```

The tuning report gives, for the minute just ended:

- the captured reference's period and the loop's phase: mean, standard deviation and extremes;
- each PPS input's period as watched, and how far its watched timestamp lags its capture (for `PPS_MONITOR_LATENCY_TICKS`);
- each source's phase against the epoch. On the GNSS clock, the NTP line is the ESP's PPS as the PIC sees it, which sets the NTP jitter allowance and `SOURCE_NTP_PPS_DELAY_NS`.

It also gives virtual holdover trials, which build up over hours. While the loop is locked, a trial starts every 10 minutes, up to 12 at once. Each takes the period the loop would hold if its source failed at that moment, then adds up how far that period drifts from the reference over the next two hours. Nothing is actually held over. The error at 10s, 1, 5, 10, 30, 60 and 120 minutes gives f and D for the holdover model.

XC16's printf turned out to cost about 1.5ms a call, and the epoch lines took 67ms of every second. They are now built with the small formatter in [ui/text.c](ui/text.c), which produces identical output in about 2ms. The minute reports still use printf, so they run late in the second, clear of the epoch work and the GNSS traffic that follows each navigation solution.

# I2C and the RTC

I2C1 runs at 400kHz, as on V2, but nothing waits for it. A driver queues a transfer (bytes to write, then optionally a repeated start and bytes to read), and the master interrupt carries it through a byte at a time. The driver checks the result later from the main loop. A transfer that has not finished within 20ms is abandoned. After that, or after a bus collision, the bus is recovered: SCL is clocked by hand until any slave that was part-way through a byte releases SDA, and a stop is sent. The same recovery runs at start-up, because a PIC reset in the middle of a read leaves the RTC holding SDA low.

The PCF8563 driver is V2's, ported onto this. It reads the control registers and the time in one transfer and reports whether the time can be trusted: VL (the oscillator lost power) and STOP must be clear and the date must be real. A write clears STOP and VL and is read back to confirm it. The RTC is read and printed at start-up, then read every minute (or tried every 10s while it does not answer), and shown in `pic-info` and the minute report. The RTC holds UTC, never local time, as on V2; time zones and DST are applied only on the display. Unlike V2, the century bit is honoured (1 for 2100 onwards), and the registers are converted with the clock's own UTC calendar rather than `mktime()`.

The RTC holds the time to the second, and has no pulse on V2, so `sync/source_rtc.c` finds where its second falls by polling. It reads the chip back to back, a few hundred microseconds a read, each stamped on the capture timebase as its I2C start completes, until the seconds count goes up. The increment lies between the last read before it and the first after, which places it to well within a millisecond. From there the second is counted on, and found again every 6 hours. This happens at start-up, after `pic-set-rtc`, and whenever the second found is more than 7 hours old.

Once the clock is synchronised, the RTC is checked after a minute, then every 6 hours, and a minute after any correction of the clock's time. If its second is more than 10ms from ours, or its time cannot be trusted (VL or STOP set), it is set, aligned to the epoch:

1. The time is loaded with STOP set, which holds the chip's prescaler in reset.
2. STOP is released so that the chip's first increment lands on one of our epochs. The data sheets for this prescaler give that first increment 0.507813 to 0.507935s after the release. The release is timed against the capture timebase, with the main loop spinning for the last 2ms.
3. The RTC's second is found again, which confirms the time and measures where the increment landed.

That error corrects the next release, so a part whose start delay differs from the data sheet's (a clone, say) is learnt on the second attempt. An attempt that misses its moment, because the main loop was busy or the bus was in use, is tried again the next second. After three, the time is written plainly, to the nearest second, so the RTC is never left stopped. Nothing is written unless the clock is synchronised, and never next to a leap second, which the RTC cannot hold.

The RTC's bound stays ±999ms. The aligned write makes it much better than that, but the time of the last setting has to survive a power cycle before that can be counted on, and that waits for the settings store. `pic-info` and the minute report show where the RTC's second was last found, its offset from the clock, the last setting and the learnt correction.

# Not yet ported

The V2 firmware did rather more than this. Still to come:

- the menu, buttons, buzzer and alarm;
- the other display modes and the 12-hour format;
- the light sensor and automatic brightness;
- the RTC's bound from the time of its last setting (it stays ±999ms until that time can be kept over a power cycle), and SML-TIME-RTC;
- settings in flash;
- GNSS provisioning;
- keeping the debug mask over a reset;
- the watchdog;
- bootloader entry.

The SML messages for most of these are already defined.
