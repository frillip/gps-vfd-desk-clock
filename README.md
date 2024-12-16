# VFD GPS Desk Clock

_Like [the big one](https://github.com/frillip/gpsdo), but smaller!_

# Why?

Apparently some people don't have access to a Rubidium frequency standard? Who knew? But this doesn't make them any less deserving of a precision timekeeping solution!

# What?

## Overall design

The last PCB and associated components were somewhat sprawling for a layout that favoured debugging. This design is an evolution based on things learned from the last clock, and significantly reduces the footprint by stacking boards on top of each other. The bottom board holds most of the electronics, the top board holds the tubes and the drivers, connected via a 4x2 2.54mm header. The top board is also supported by a series of 20mm M3 standoffs. The clock is designed to function with or without a GNSS module populated on the board, as this represents a significant cost, and also many people do not want to have GNSS antennas strung around their house (what?).

## Tubes

Much like [the big one](https://github.com/frillip/gpsdo), we have IV-12 VFD 7 segement tubes and IV-1 seperator tubes.

## Tube drivers

I've gone for a very similar Microchip offering in 2x HV5812WG-G, this gives us 40 HV outputs which we can control via the SPI peripheral and latch with our output compare module.

## Microcontroller

A dsPIC33EP256GP504 runs the show with 4x input capture modules, 3x output compare modules, an I2C peripheral, an SPI peripheral and 2 UARTs. This is run off a relatively cheap 40MHz osciillator, but could be easily swapped out for something fancier like an OCXO or TCXO, but for the purposes of this project we can use GPS to more accurately characterise our frequency source. The flash program memory is also used to store some user preferences.

## ESP32

An ESP32 allows us to connect to the internet to obtain time from NTP servers. It is connected to the PIC via UART for data transfer, and also has access to the PPS signals from both the PIC OC module and GNSS module (if populated). It is also connected to the I2C bus (though I2C is not enabled in software), the GNSS UART via unpopulated solder jumpers, and some other IO. There is also a PPS signal from the ESP module to the PIC for sub-second accuracy. Typical achievable accuracy for NTP mode is within ~20ms of UTC. A programming and communication interface is provided on UART0 by a CP2102N USB to UART bridge. As the ESP32 is NRND, the next revision will swap the ESP32 module for an ESP32-S3 or ESP32-C6 which supports USB directly and I can remove the CP2102N from the board.

## GNSS

Again we're using the u-blox NEO M9N. This is directly integrated onto the board with an SMT SMA connector for an antenna. Both active and passive antennas are supported. There is also a second USB port connected to the GNSS module for power/configuration of the module via u-center.

## RTC

There is code for both a DS1307 RTC or a PCF8563 RTC, which seems to be more popular these days. This is battery backed with a 1215 cell and connected via I2C to both the PIC and ESP.

## Environment sensors

A BME280 temperature/pressure/humidity sensor is on the board, but code needs development. It may be possible to more accurately estimate the crystal temperature characteristics at startup with this sensor, but for now it is mostly decorative, as there is significant heating of the board from the power supply modules that render it somewhat useless as an environment sensor!

A VEML6040 is used for ambient light sensing. The output of which goes through a very basic scaling to give a target brightness for the tubes out of 4000. The PIC then drives a third OC module using this value to get flicker-free brightness control over the tubes. The resulting PWM frequency is 10kHz, however even if you were to record the tubes with a camera capable of siignificantly more than 10,000fps, you would not see any flickering as there is a persistent glow on the VFD tubes that smooths the brightness.

## Inputs

There is a single push button and a toggle switch. The toggle switch is used to arm/disarm the alarm feature, and the push button is used to navigate the menu via a series of long/short presses. Currently it is possible to set:

 - Timezone offset
 - Daylight savings time automatic/manual
 - Daylight savings time offset
 - Alarm time
 - Alarm enabled
 - Buzzer functionality
 - 12/24 hour display format
 - Display format (YYYY/MMDD/HHMM/MMSS/SSmm/delta)
 - Reset WiFi settings
 - Reset clock settings
 - Reset all settings

The menu is implemented in such a way that changes are saved only when a long press is used to confirm the selection. If left to time out, the setting will not be saved.

## Buzzer

A self oscillating 5V buzzer beeps at semi-regular intervals. If enabled, every hour it beeps 1-12 times, corresponding to the hour, in groupds of 4. It also beeps once at 15, 30 and 45 minutes past the hour. This is also utilized in for the alarm feature.

## Power

Power is provided through either the USB-C port connected to the ESP or the USB-C connected to the GNSS module. This goes through a TPS2113A power mux chip that provides current limiting, inrush current control, and power selection/prioritization. It also prevents backfeeding from one USB source to the other if both are connected. A LM3671MFX based buck converter provides 3.3V for the GNSS module, PIC, and ESP32. Tube voltages are generated using a TPS55340 boost converter IC to give 34V for the grid voltage, and a LM2831Z based buck converter provides the lower 1.65V filament voltage. Future hardware revisions will simplify and commonise much of the power supplies to help reduce complexity and cost.

# How?

After start up the microcontroller gets the stored time from the RTC and loads it into the display driver via SPI. It then sets up the output compare (OC) module to output a 1.25us pulse every 1 second to the display driver latch input. A 1kHz scheduler loop is set up and increments the internal calendar every 1s and pushes this new data to the display driver 100ms before the OC pulse goes out. When the OC triggers, the latch pin of the display driver is driven high, and the previously pushed data appears on the outputs, and in turn the tubes. This allows for very precise updating of the display.

The microcontroller also listens for a time signal over the UART from the ESP32 module. Based on WiFi status, NTP sync status, it may choose to ignore the data being sent and rely solely on the RTC if the ESP is without a network connection for instance. 

The microcontroller then waits for GNSS (if present) to obtain a fix and begin outputting a PPS signal. The PPS signal is fed into one of the pairs of input capture (IC) modules operating in 32-bit mode. This is compared with the other IC module pair that captures the timestamp of the OC pin. Once the difference between the two has been calculated, the OC module has a one-off adjustment applied to it to bring it in sync with GNSS PPS.

As a final step, the scheduler loop is then aligned to the OC signal so that it ticks 500us ahead of the PPS.

As the clock runs, it counts the number of clock cycles between each GPS PPS event, and uses this to more accurately calculate the true frequency of the 40MHz osciillator. Once a number of 'slipped' cycles has accumulated, the microcontroller performs a calculation to determine its running frequency, updates the OC parameters to match, and resynchronises with the GPS PPS signal. The microcontroller also determines the short term frequency deviation of from UTC. If it is found to have suddenly changed (due to room temperature, or as would be the case at initial startup), the OC values are updated to reflect the new frequency, and if required, PPS signals resynchronised.

By default, the clock maintains an accuracy of within around 25us of UTC in GNSS mode.

# Serial Commands

It is possible to send commands over the ESP UART to perform some actions, or print some debug information that may or may not be useful:

| Command | Description |
| --- | --- |
| `\n` | Print standard set of debug information to the terminal |
| esp-reset | Reset the ESP |
| esp-set-interval [n] | Set NTP interval to [n] (min 300, max 43200) |
| esp-set-server [s] | Set the NTP server to [s] |
| esp-resync | Force NTP sync |
| esp-wifi-show | Print WiFi info |
| esp-wifi-connect | Force WiFi connect |
| esp-wifi-disconnect | Force WiFi disconnect |
| esp-wifi-ssid [s] | Set the saved SSID to [s] |
| esp-wifi-pass [s] | Set the saved passphrase to [s] |
| esp-wifi-dhcp [b] | Enable/disable DHCP |
| esp-wifi-ip [s] | Set static IP to [s], unless [s] is 'dhcp' or 'auto' |
| esp-wifi-mask [s] | Set ip mask to [s], only valid with static IP |
| esp-wifi-gateway [s] | Set gateway to [s], only valid with static IP |
| esp-wifi-clear | Clear saved WiFi config |
| esp-wifi-setup | Enable WiFi setup AP mode |
| --- | --- |
| esp-clear-all | Clear all settings |
| esp-save | Save settings |
| --- | --- |
| pic-info | Show info directly from PIC |
| pic-reset | Resets the PIC |
| pic-resync | Manually resync PIC to GNSS |
| pic-set-rtc [n] | Set PIC delta epoch to [n] unix epoch time |
| pic-set-tz-offset [n] | Set timezone offset to [n] in seconds, rounds to nearest 15 minutes |
| pic-set-dst-offset [n] | Set dst offset to [n] in seconds, rounds to nearest 15 minutes |
| pic-set-dst-auto [b] | Enable/disable auto dst |
| pic-set-dst-active [b] | Enable/disable dst (pic-set-dst-auto must be off) |
| pic-set-alarm-enabled [b] | Enable/disable alarm |
| pic-set-alarm [n] | Set PIC alarm to [n] seconds past midnight |
| pic-set-delta [n] | Set PIC delta epoch to [n] unix epoch time |
| pic-set-beeps [b] | Enable/disable beeping |
| pic-set-display [e] | Set pic display to [e]: 1=HHMM, 2=MMSS, 3=SSMM, 4=YYYY, 5=MMDD |
| pic-set-brightness-auto [b] | Set display brightness to auto |
| pic-set-brightness [n] | Set display brightness to n / 4000 |
| pic-show-eeprom | show settings stored in EEPROM |
| pic-show-config | show running config settings |
| pic-clear-all | Clear all settings to defaults |
| pic-save | Save settings |
| --- | --- |
| rst-all | Reset both |
| rst-pic | Same as pic-reset |
| rst-esp | Same as esp-reset |


# Improvements

We could do a lot of things to improve this, like watching the trend of slipped clock cycles to get better short term accuracy, rather than just synchronising every 2000 slipped cycles. We could also look at the SHT30 sensor to temperature compensate. However, the single biggest improvement would be to increase the quality of the crystal on the board, either to an OCXO or TCXO or similar. With some additional circuitry we could also employ a VCXO/VCTCXO or even a DCTCXO. However the 'real' limit of displayed accuracy here is the persistent afterglow of the VFD tubes, hence why [really fancy clocks use Nixie tubes](https://www.daliborfarny.com/project/calibration-display-for-nasa/).
