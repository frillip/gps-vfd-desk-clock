# VFD GPS Desk Clock

_Like [the big one](https://github.com/frillip/gpsdo), but smaller!_

# Why?

Apparently some people don't have access to a Rubidium frequency standard? Who knew? But this doesn't make them any less deserving of a precision timekeeping solution!

# What?

## Overall design

The last PCB and associated components were somewhat sprawling for a layout that favoured debugging. This design is an evolution based on things learned from the last clock, and significantly reduces the footprint by stacking boards on top of each other. The bottom board holds most of the electronics, the top board holds the tubes and the drivers, connected via a 4x2 2.54mm header. The top board is also supported by a series of 20mm M3 standoffs.

## Tubes

Much like [the big one](https://github.com/frillip/gpsdo), we have IV-12 VFD 7 segement tubes and IV-1 seperator tubes.

## Tube drivers

I've gone for a very similar Microchip offering in 2x HV5812WG-G, this gives us 40 HV outputs which we can control via the SPI peripheral and latch with our output compare module.

## Microcontroller

A dsPIC33EP256GP504 runs the show with 4x input capture modules, 2x output compare modules, an i2c peripheral, an SPI peripheral and 2 UARTs. This is run off a relatively cheap 10MHz crystal, but could be easily swapped out for something fancier like an OCXO or TCXO, but for the purposes of this project we use GPS to more accurately characterise our frequency source.

## GPS

Again we're using the u-blox NEO M9N. The plan is to integrate this directly onto the board, rather than use the (excellent and highly recommended) Sparkfun breakout.

## RTC

There is code for both a DS1307 RTC or a PCF8563 RTC, which seems to be more popular these days. This is battery backed with a 1215 cell.

## Environment sensors

A SHT30 temperature/pressure/humidity sensor is on the board, but code needs development. It may be possible to more accurately estimate the crystal temperature characteristics at startup with this sensor, but for now it is mostly decorative.

A BH1730 is available for ambient light sensing, but again, needs development.

## Inputs

There is a single push button and a toggle switch. I've yet to code any sort of UI, and am not sure I even need one, so for now these are just for debugging purposes.

## Buzzer

A self oscillating 5V buzzer beeps at semi-regular intervals. This could be potentially used for an alarm feature in the future.

## Power

Currently using the same STUSB4500 to supply 20V to several DCDC modules, but as we do not need the full 100W available from a USB-PD source, the plan is to use a plain 5V USB-C supply with a LDO to 3.3V for the microcontroller + GPS, a buck to 1.5V for the tube filaments, and a boost to 35V for the tube grid and segment voltages.

# How?

After start up the microcontroller gets the stored time from the RTC and loads it into the display driver via SPI. It then sets up the output compare (OC) module to output a 1.25us pulse every 1 second to the display driver latch input. A 1kHz scheduler loop is set up and increments the internal calendar every 1s and pushes this new data to the display driver 100ms before the OC pulse goes out. When the OC triggers, the latch pin of the display driver is driven high, and the previously pushed data appears on the outputs, and in turn the tubes. This allows for very precise updating of the display.

The microcontroller then waits for GPS to obtain a fix and begin outputting a PPS signal. The PPS signal is fed into one of the pairs of input capture (IC) modules operating in 32-bit mode. This is compared with the other IC module pair that captures the timestamp of the OC pin. Once the difference between the two has been calculated, the OC module has a one-off adjustment applied to it to bring it in sync with the GPS PPS pin.

As a final step, the scheduler loop is then aligned to the OC signal so that it ticks 500us ahead of the PPS.

As the clock runs, it counts the number of clock cycles between each GPS PPS event, and uses this to more accurately calculate the true frequency of the 10MHz crystal. Once a number of 'slipped' cycles has accumulated, the microcontroller performs a calculation to determine its running frequency, updates the OC parameters to match, and resynchronises with the GPS PPS signal.

By default, the clock maintains an accuracy of within around 25us of UTC.

# Improvements

We could do a lot of things to improve this, like watching the trend of slipped clock cycles to get better short term accuracy, rather than just synchronising every 2000 slipped cycles. We could also look at the SHT30 sensor to temperature compensate. However, the single biggest improvement would be to increase the quality of the crystal on the board, either to an OCXO or TCXO or similar. With some additional circuitry we could also employ a VCXO/VCTCXO or even a DCTCXO. However the 'real' limit of displayed accuracy here is the persistent afterglow of the VFD tubes, hence why [really fancy clocks use Nixie tubes](https://www.daliborfarny.com/project/calibration-display-for-nasa/).
