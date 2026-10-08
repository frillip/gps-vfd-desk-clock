# VFD GPS Desk Clock

_A small desk clock with an unreasonable interest in what time it is._

<img width="1600" height="551" alt="image" src="https://github.com/user-attachments/assets/8335cbd0-ec97-4ee4-bd6c-85db791557a4" />

# Why?

Apparently some people don't have access to a Rubidium frequency standard? Who knew? But this doesn't make them any less deserving of a precision timekeeping solution!

# What?

## Overall design

The last PCB and associated components were somewhat sprawling for a layout that favoured debugging. This design is an evolution based on things learned from the last clock, and significantly reduces the footprint by stacking boards on top of each other. The bottom board holds most of the electronics, the top board holds the tubes and the drivers, connected via a 4x2 2.54mm header. The top board is also supported by a series of 20mm M3 standoffs. The clock is designed to function with or without a GNSS module populated on the board, as this represents a significant cost, and also many people do not want to have GNSS antennas strung around their house (what?).

## Tubes

There are 4x socketed IV-12 VFD 7-segment tubes for the digits and 2x IV-1 separator tubes for the separator colon and status information.

## Tube drivers

2x Microchip HV5812WG-G drivers provide 40 HV outputs, controlled via the SPI peripheral. The blank input is PWM driven to give brightness control, and the data is latched onto the output using both a GPIO and OC module output via a discrete OR gate.

## Microcontroller

A dsPIC33EP256GP504 runs the show with 4x input capture modules, 3x output compare modules, an I2C peripheral, an SPI peripheral and 2 UARTs. This runs off a relatively cheap 40MHz oscillator. It could be swapped out for something fancier like an OCXO or TCXO, but for the purposes of this project GNSS can more accurately characterise the existing frequency source. The flash program memory is also used to store some user preferences.

## ESP32

An ESP32 connects to the internet to obtain time from an NTP server. It is connected to the PIC via UART for data transfer, and also has access to the PPS signals from both the PIC OC module and GNSS module (if populated). It is also connected to the I2C bus (though I2C is not enabled in software), the GNSS UART via unpopulated solder jumpers, and some other IO. There is also a PPS signal from the ESP module to the PIC for sub-second accuracy. Typical achievable accuracy for NTP mode is within ~20ms of UTC. A programming and communication interface is provided on UART0 by a CP2102N USB to UART bridge.

## GNSS

GNSS duties are handled by the u-blox NEO-M9N module. This is directly integrated onto the board with an SMT SMA connector for an antenna. Both active and passive antennas are supported. There is also a second USB port connected to the GNSS module for power/configuration of the module via u-center.

## RTC

There is code for both a DS1307 RTC and a PCF8563 RTC, which seems to be more popular these days. The RTC is battery backed with a CR1220 cell and connected via I2C to both the PIC and ESP. It remembers the time while the clock is unplugged and provides somewhere sensible to start before GNSS or NTP becomes available.

## Environment sensors

A BME280 temperature/pressure/humidity sensor is on the board, but code needs development. It may be possible to more accurately estimate the crystal temperature characteristics at startup with this sensor, but for now it is mostly decorative, as there is significant heating of the board from the power supply modules that render it somewhat useless as an environment sensor!

A VEML6040 is used for ambient light sensing. Its output goes through a very basic scaling to give a target brightness for the tubes out of 4000. The PIC then drives a third OC module using this value to get flicker-free brightness control over the tubes. The resulting PWM frequency is 10kHz, with the persistent glow of the VFD phosphor helping to smooth the brightness. This is useful for brightness control, and rather less useful when trying to show exactly when a digit changed.

## Inputs

There is a single push button and a toggle switch. The toggle switch is used to arm/disarm the alarm feature, and the push button is used to navigate the menu via a series of long/short presses. Currently it is possible to set:

- Timezone offset
- Daylight saving time automatic/manual
- Daylight saving time offset
- Alarm time
- Alarm enabled
- Buzzer functionality
- 12/24 hour display format
- Display format (YYYY/MMDD/HHMM/MMSS/SSmm/delta)
- Reset WiFi settings
- Reset clock settings
- Reset all settings
- 
The menu is implemented in such a way that changes are saved only when a long press is used to confirm the selection. If left to time out, the setting will not be saved.

## Buzzer

A self-oscillating 5V buzzer beeps at semi-regular intervals. If enabled, every hour it beeps 1-12 times, corresponding to the hour, in groups of 4. It also beeps once at 15, 30 and 45 minutes past the hour. It also provides the alarm.

## Power

Power is provided through either the USB-C port connected to the ESP or the USB-C connected to the GNSS module. This goes through a TPS2113A power mux chip that provides current limiting, inrush current control, and power selection/prioritization. It also prevents backfeeding from one USB source to the other if both are connected. An LM3671MFX based buck converter provides 3.3V for the GNSS module, PIC, and ESP32. Tube voltages are generated using a TPS55340 boost converter IC to give 34V for the grid voltage, and an LM2831Z based buck converter provides the lower 1.65V filament voltage. Future hardware revisions will simplify and commonise much of the power supplies to help reduce complexity and cost.

# How?

After startup, the microcontroller gets the stored time from the RTC and loads it into the display driver via SPI. It then sets up the output compare (OC) module to output a 1.25µs pulse every 1 second to the display driver latch input. A 1kHz scheduler loop is set up and increments the internal calendar every 1s and pushes this new data to the display driver 100ms before the OC pulse goes out. When the OC triggers, the latch pin of the display driver is driven high, and the previously pushed data appears on the outputs, and in turn the tubes. This allows for very precise updating of the display.

The microcontroller also listens for a time signal over the UART from the ESP32 module. Based on WiFi and NTP sync status, it may choose to ignore the data being sent. The RTC provides the startup time if a better source is unavailable; the local oscillator then keeps the clock running. The ESP supplies NTP time and a timing pulse, while the dsPIC handles the timekeeping and display.

The microcontroller then waits for GNSS (if present) to obtain valid time and a usable PPS signal. The GNSS time messages identify which second it is, PPS marks where the second begins. The PPS signal is fed into one of the pairs of input capture (IC) modules operating in 32-bit mode. This is compared with the other IC module pair that captures the timestamp of the OC pin. Once the difference between the two has been calculated, the OC module has a one-off adjustment applied to it to bring it in sync with GNSS PPS.

As a final step, the scheduler loop is then aligned to the OC signal so that it ticks 500µs ahead of the PPS.

As the clock runs, it counts the number of clock cycles between each GNSS PPS event, and uses this to more accurately calculate the true frequency of the 40MHz oscillator. Once a number of 'slipped' cycles has accumulated, the microcontroller performs a calculation to determine its running frequency, updates the OC parameters to match, and resynchronises with the GNSS PPS signal. The microcontroller also determines the short-term frequency deviation relative to the GNSS reference. If it is found to have suddenly changed (due to room temperature, or as would be the case at initial startup), the OC values are updated to reflect the new frequency, and if required, PPS signals resynchronised.

By default, the clock maintains timing within around 25µs of UTC in GNSS mode.

# ESP32 network features
The ESP does more than fetch the time. It also provides a network console, pulls its own firmware updates, and can ask a remote timezone service (also in the repository) for information on current timezone offset and any DST adjustments.

## Telnet console

The same console commands can be issued through the ESP USB-UART connection or over Telnet. This saves having to find a USB cable just to see what the clock is doing. Telnet is disabled by default and uses port 23 unless changed with `esp-telnet-port`. Connect to the clock's WiFi IP address on the configured port. `quit` or `exit` closes the session.

There are two separate controls: whether Telnet should start on boot, and whether the listener is running now. `esp-telnet-enable` enables startup and starts the listener when changing from disabled to enabled. `esp-telnet-disable` disables startup but leaves a running listener alone. `esp-telnet-start` and `esp-telnet-stop` control the listener without changing the startup preference. Stopping it leaves an existing session open, so exit that session as well if required. Save the startup preference and port with `esp-save`.

The current console has no login and uses plain Telnet, so anyone with access to it can issue the commands. `esp-wifi-info` also prints the saved WiFi passphrase. This is a convenience console for a trusted local network, with USB-UART available when Telnet is disabled. Some form of authentication and general hardening is on the long list of things to be added eventually.

## OTA firmware updates

The [ESP updater](esp_src/updater_pull.cpp) can pull firmware over WiFi using an HTTPS update manifest. The hostname and path of the manifest are configurable. An optional configuration string selects an update profile, for example `development`, with `default` clearing that selection.

`esp-update-check` checks what is available without installing anything. `esp-update-pull` installs an applicable update but leaves the ESP running until `esp-reset` completes the change. `esp-update-force` bypasses the normal version comparison, permits downgrades and requests an immediate reboot after installation. Useful when testing, though somewhat more committed than merely checking.

Automatic updates are disabled by default. When enabled, the ESP checks during a chosen local hour, defaulting to 04:00, and installs and reboots if an update is available. The scheduler checks the update window every 30 minutes, so this is an hour-long window rather than a promise to update at exactly 04:00:00. Save changes with `esp-save`.

This updater only replaces the ESP firmware. A PIC bootloader and firmware update path are still on the pending features list.

## Remote timezone information

_Currently this feature is in development, and requires additional ESP and PIC development work to implement fully._

The optional [tzinfo client](esp_src/remote_tzinfo.cpp) asks a separate Python service for the timezone, base UTC offset, DST state and offset, and the next DST transition as a UTC epoch. The ESP passes this information to the dsPIC over UART, where it is available to the clock's automatic timezone/DST handling. NTP still supplies UTC time, whilst tzinfo supplies the rules for displaying local time.

The client is disabled by default. It uses a daily refresh interval. The hostname, path and interval are configurable, so the service can also be self-hosted. `esp-tzinfo-check` fetches and prints the response without making it available for application to the PIC. `esp-tzinfo-update` fetches it and marks a successful response available.

## Location privacy

A timezone lookup does not need to know which corner of the desk the clock is on. `esp-tzinfo-set-acc` controls how many decimal places of latitude and longitude are sent, rounding the coordinates before the request leaves the ESP. The default is one decimal place. Increasing this may help near a timezone boundary, but also sends a more precise location.

| Setting | Coordinate grid spacing in latitude | Effect |
| --- | --- | --- |
| `-1` | No GNSS location supplied | Sends zero coordinates and marks GNSS detection/fix invalid |
| `0` | About 111km | Whole-degree coordinates |
| `1` | About 11.1km | Default precision |
| `2` | About 1.11km | Two decimal places |
| `3` | About 111m | Three decimal places |
| `4` | About 11.1m | Four decimal places |
| `5` | About 1.11m | Five decimal places |
| `6` | About 0.111m | Maximum configured precision |

These are grid spacings, not claims about GNSS accuracy. Longitude spacing also depends on latitude. Setting `-1` suppresses GNSS location sharing with this service and leaves IP-based GeoIP lookup as an option on the server. It does not disable local GNSS timekeeping. The server still sees the public IP address of requests, and rounding coordinates does not make them anonymous.

`esp-tzinfo-disable` turns off periodic timezone requests and forwarding of remote timezone information. An explicit `esp-tzinfo-check` or `esp-tzinfo-update` still makes a request even while the feature is disabled. Automatic OTA requests are controlled separately with `esp-update-auto-disable`, and NTP has its own server setting. Save preferences with `esp-save`.

# Console commands

Commands can be issued via the ESP USB-UART console at 115200 baud or through Telnet when running. One command per line. Pressing return on an empty line prints the available diagnostics, which may or may not be useful. `help` prints the built-in command list.

Arguments below use `[n]` for a number, `[s]` for text, `[b]` for a boolean and `[e]` for a display selection. Commands marked **Not implemented** are recognised by the parser but currently only print that message. Their presence in `help` does not make them work, unfortunately.

Changes to ESP preferences require `esp-save` to persist. PIC settings use `pic-save`. The commands that clear WiFi or all ESP settings also restart the ESP as described below.

## Diagnostics and resets

| Command | Description |
| --- | --- |
| Enter on an empty line | Print available time, PPS, NTP, GNSS, tzinfo, RTC, sensor and sync-state diagnostics |
| `help` | Print the complete built-in help text, per-command help is not implemented |
| `esp-reset` | Restart the ESP, also completes an update installed with `esp-update-pull` |
| `pic-info` | Request diagnostics directly from the PIC |
| `pic-reset` | Restart the PIC |
| `rst-all` | Request a PIC reset and restart the ESP |
| `rst-pic` | Same as `pic-reset` |
| `rst-esp` | Same as `esp-reset` |
| `quit` / `exit` | Disconnect the current Telnet session, Telnet only |

## NTP

| Command | Description |
| --- | --- |
| `esp-ntp-info` | Print NTP time, sync status, server, interval and last sync time |
| `esp-ntp-set-interval [n]` | Set NTP resync interval in seconds, valid saved-preference range 300–86400, default 1800 |
| `esp-ntp-set-server [s]` | Test and select an NTP server |
| `esp-ntp-resync` | Force an NTP query and realign ESP timing |

## WiFi

| Command | Description |
| --- | --- |
| `esp-wifi-info` | Print connection status, SSID, saved passphrase and IP address |
| `esp-wifi-scan` | Scan nearby networks and print signal strength and security type |
| `esp-wifi-connect` | Request reconnection to WiFi |
| `esp-wifi-disconnect` | Disconnect WiFi, the normal reconnect task can reconnect later |
| `esp-wifi-ssid [s]` | **Not implemented:** set the saved SSID |
| `esp-wifi-pass [s]` | **Not implemented:** set the saved passphrase |
| `esp-wifi-dhcp [b]` | **Not implemented:** enable/disable DHCP |
| `esp-wifi-ip [s]` | **Not implemented:** set a static IP or select `dhcp` / `auto` |
| `esp-wifi-mask [s]` | **Not implemented:** set the static-IP subnet mask |
| `esp-wifi-gateway [s]` | **Not implemented:** set the static-IP gateway |
| `esp-wifi-clear` | Clear saved WiFi credentials and restart the ESP |
| `esp-wifi-setup` | Currently the same as `esp-wifi-clear`: clear credentials and restart into the WiFiManager setup flow |

## Telnet

| Command | Description |
| --- | --- |
| `esp-telnet-enable` | Enable startup on boot, also start the listener when changing from disabled to enabled |
| `esp-telnet-disable` | Disable startup on boot, does not stop a running listener |
| `esp-telnet-start` | Start the listener now without changing the startup preference |
| `esp-telnet-stop` | Stop the listener without changing the startup preference, an existing session remains open |
| `esp-telnet-port [n]` | Set the port, 1–65535, changing it stops and restarts the listener |

## OTA updates

| Command | Description |
| --- | --- |
| `esp-update-check` | Check the manifest and report the available firmware version without installing |
| `esp-update-pull` | Install an applicable update without rebooting, use `esp-reset` to finish |
| `esp-update-force` | Force installation regardless of the running version, allowing downgrades, and request reboot |
| `esp-update-set-server [s]` | Set the update-manifest hostname |
| `esp-update-set-path [s]` | Set the update-manifest path |
| `esp-update-set-config [s]` | Select an update profile, e.g. `development`, `default` clears it |
| `esp-update-auto-enable` | Enable automatic checking, installation and reboot in the configured local hour |
| `esp-update-auto-disable` | Disable automatic updates, manual commands remain available |
| `esp-update-auto-hour [n]` | Set the automatic-update local hour, 0–23 |

## Timezone service and location privacy

| Command | Description |
| --- | --- |
| `esp-tzinfo-enable` | Enable periodic tzinfo requests and forwarding of remote timezone information to the PIC |
| `esp-tzinfo-disable` | Disable periodic requests and forwarding, manual requests remain available |
| `esp-tzinfo-check` | Fetch and print timezone information, then leave it unavailable for application to the PIC |
| `esp-tzinfo-update` | Fetch timezone information and mark a successful response available |
| `esp-tzinfo-set-server [s]` | Set the tzinfo service hostname |
| `esp-tzinfo-set-path [s]` | Set the tzinfo request path |
| `esp-tzinfo-set-interval [n]` | Set the refresh interval in seconds, 900–604800, default 86400 |
| `esp-tzinfo-set-acc [n]` | Set transmitted GNSS coordinate precision, 0–6 decimal places, `-1` suppresses GNSS location data |

## ESP preferences

| Command | Description |
| --- | --- |
| `esp-config-show` | Print running ESP preferences, including NTP, Telnet, tzinfo and updater settings |
| `esp-config-clear` | Reset running ESP preferences to defaults, use `esp-save` to persist |
| `esp-clear-all` | Reset and save ESP preferences, clear WiFi credentials and restart the ESP |
| `esp-save` | Save ESP preferences |

## PIC timekeeping, display and preferences

| Command | Description |
| --- | --- |
| `pic-resync` | Request PIC resynchronisation with GNSS PPS or NTP when available |
| `pic-set-rtc [n]` | Write the RTC time from a Unix epoch timestamp |
| `pic-set-tz-offset [n]` | Set the timezone offset in seconds, rounded to the nearest 15 minutes |
| `pic-set-dst-offset [n]` | Set the DST offset in seconds, rounded to the nearest 15 minutes |
| `pic-set-dst-auto [b]` | **Not implemented:** enable/disable automatic DST |
| `pic-set-dst-active [b]` | **Not implemented:** manually enable/disable DST |
| `pic-set-fosc-freq [n]` | Set the PIC oscillator-frequency value in Hz, subject to the PIC's permitted range |
| `pic-set-alarm-enabled [b]` | **Not implemented:** enable/disable the alarm |
| `pic-set-alarm [n]` | **Not implemented:** set the alarm in seconds past midnight |
| `pic-set-delta [n]` | Set the delta-display reference epoch to a Unix epoch timestamp |
| `pic-set-beeps [b]` | **Not implemented:** enable/disable beeping |
| `pic-set-display [e]` | **Not implemented:** select display mode, advertised values are 1=HHMM, 2=MMSS, 3=SSMM, 4=YYYY, 5=MMDD |
| `pic-set-brightness-auto` | Restore automatic brightness, no argument required |
| `pic-set-brightness [n]` | Set manual brightness on the 0–4000 scale |
| `pic-eeprom-show` | Print saved PIC settings |
| `pic-config-show` | Print running PIC settings |
| `pic-config-clear` | Reset running PIC settings to defaults, use `pic-save` to persist |
| `pic-save` | Save PIC settings |
| `pic-bootloader-enter` | Request PIC bootloader entry and report the handshake result, PIC bootloader still in development |
| `pic-bootloader-exit` | Request exit from the PIC bootloader and report the result |

# Software V2+ and new timing architecture

The hardware can stay as it is for this work. Most of the changes are about how the dsPIC keeps time, disciplines the oscillator, and decides when to put the next number on the tubes. The existing V2 approach works, but counting slipped cycles and periodically pulling things back into line leaves room for improvement. Naturally, this means finding a more involved way of doing it.

These changes are currently in development.

## One master epoch

The central change is a master epoch owned by the dsPIC, using Timer2/3 as the underlying tick counter. Calendar time, sub-second time and display scheduling are derived from that common timebase. The generated PPS becomes an output of the timekeeping model, rather than the thing around which the rest of the clock has to organise itself.

GNSS PPS is measured against the epoch by comparing captured reference and output edges. The generated PPS is captured too, so the clock can check when its output actually happened. A calibrated phase offset accounts for the intended relationship between the reference edge and the output edge. Knowing what time it is and knowing when a pin changed are related problems, but they still need checking separately.

### Silicon errata and free-running captures

It would be convenient to synchronise all the peripheral counters and have them agree about where zero is. Unfortunately, the silicon has some qualifications. The two capture faults observed on the dsPIC33EP256GP504 are described more clearly in [Microchip's GM3XX/6XX/7XX-family errata, DS80000577Q](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU16/ProductDocuments/Errata/dsPIC33EPXXXGM3XX_6XX_7XX-Family-Silicon-Errata-and-Data-Sheet-Clarification-DS80000577.pdf). The capture blocks are shared between these families, and the GM descriptions match the behaviour seen here, the document's formal device/revision tables remain specific to the GM family.

| Issue | Detailed behaviour | Handling in V2+ |
| --- | --- | --- |
| 5 — Synchronous cascaded Input Capture | With `IC32 = 1`, `ICTRIG = 0` and a non-zero `SYNCSEL`, a synchronisation event resets only the odd, lower-word timer. The even, upper-word timer does not reset, leaving the combined 32-bit count inconsistent. | Establish the capture-counter alignment at startup, then run the IC pairs freely rather than depend on recurring synchronous resets. |
| 31 — Cascaded Input Capture rollover | When the lower word rolls from `0xFFFF` to `0x0000`, the upper word increments one timer cycle late. A capture on that boundary can contain the new lower word with the old upper word. | Retain the rollover correction when constructing a timestamp whose captured lower word is exactly `0x0000`, before calculating periods or phase differences. |

Microchip lists no workaround for either capture issue. The clock handles them in its timing architecture and timestamp processing. Free-running counters avoid the reset fault, but do not remove the rollover fault: at that boundary, the stale upper word makes the assembled timestamp 65536 ticks low. The V2+ correction accounts for the missing carry before that value reaches the frequency estimator or phase controller. Otherwise, a perfectly ordinary PPS edge can look like a rather exciting timing excursion.

The V2+ arrangement keeps Timer2/3 in charge of the epoch, with the Timer3 interrupt re-arming OC1 for the next epoch pulse. IC1/2 captures GNSS PPS and IC3/4 captures the OC1 output, using free-running 32-bit capture counters. These are the IC modules' own counters, not direct snapshots of Timer2/3. After accounting for counter alignment and the calibrated phase offset, corrected, wrap-safe timestamp differences provide the signed phase error used to discipline the master epoch. The captures measure timing, they do not independently advance the clock.

During development, starting Timer2/3 could clear the cascaded capture counter, but subsequent 32-bit periods did not provide the recurring reset required. A reset that works once is not particularly useful for a clock. The free-running arrangement avoids relying on that behaviour, while preserving the separate rollover correction.

The same GM document also describes the one-shot OC interrupt hazard in issue 24: with a timer timebase and prescaler other than 1:1, `OCxIF` can precede the pin transition. Its documented workaround is to wait at least twice the prescaler factor in CPU cycles before clearing `OCM`. This is another reason to verify the generated edge by capture instead of assuming the interrupt flag means the pin has already changed.

## Persuading the oscillator more gently

The work under test replaces the accumulated-slip correction scheme with PI discipline and Kalman-assisted estimation of phase and frequency. The aim is to follow the oscillator's drift without treating every noisy measurement as a reason to move the clock.

Acquisition, tracking and holdover are handled explicitly. Getting the clock aligned at startup, maintaining that alignment, and carrying on when the reference disappears need different behaviour. Holdover still depends on the local oscillator; no amount of software will turn it into a Rubidium standard.

## Less work in interrupts

Input-capture interrupts record the timing measurements, with the heavier processing moved out of the interrupt handlers. Display data is prepared ahead of its intended epoch, then committed at the corresponding boundary. This keeps the timekeeping, measurement and display work tied to the same model without asking an interrupt to do everything at once.

## Timezones and daylight saving

The ESP already has the remote timezone client described above. The V2+ work develops the dsPIC handling around the UTC master epoch, so supplied offsets and upcoming transitions fit the same timekeeping model. Fixed offsets and a simple automatic-DST setting are manageable until the clock encounters somewhere with more interesting rules.

The newer Python timezone-service work keeps the base timezone offset and DST offset distinct, supplies upcoming UTC transition epochs, and preserves the request time as an epoch timestamp. The current ESP client described above consumes the current offsets, DST state and next transition, using the richer service response forms part of the V2+ integration work.

The intended split remains that the ESP handles timezone lookup and fetching the rules, while the dsPIC applies the supplied offsets and transitions against its UTC master epoch. Local display time is derived from UTC, changing the timezone should not move the underlying clock. This also avoids teaching the dsPIC the entire world's timezone legislation, which seems an unreasonable thing to ask of a desk clock.

# Future hardware improvements

A better oscillator is still the most direct hardware improvement: an OCXO or TCXO gives the software a better starting point, especially during warm-up or loss of the reference. With some additional circuitry, a VCXO/VCTCXO or even a DCTCXO could also be used.

Temperature compensation is another possibility, although the BME280 first needs to become more than a decorative witness to the power supplies warming up. Its position and relationship to the oscillator temperature matter more than simply having a temperature reading available.

Using a dual core dsPIC33CH device, with the secondary core as a dedicated 'timing domain', while the main core handles the tedious tasks such as user input and UART communication, is another avenue that is being actively explored. The increased clock speed from 40MIPS to 100MIPS will also reduce quantisation uncertainty.

<img width="1329" height="573" alt="image" src="https://github.com/user-attachments/assets/7ac8c3ad-8a94-4397-b105-5bd9c55a9ebd" />

The V2+ work above is where the software timing improvements are currently being tested. However, the 'real' limit of displayed accuracy here is still the persistent afterglow of the VFD tubes, hence why [really fancy clocks use Nixie tubes](https://www.daliborfarny.com/project/calibration-display-for-nasa/).
