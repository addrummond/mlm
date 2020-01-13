This is a simple ambient light meter using a Liteon LTR-303ALS light sensor and an EFM32 Tiny Gecko microcontroller.
It is powered by a single CR2032 coin cell.
The firmware is written in C and the PCB is designed using KiCad.
Readings are displayed using a circle of 24 LEDs, with two additional LEDs indicating ±⅓ stop adjustments.

The hardware design and software are largely complete. (This will work if you build one.)

The bottom side of the PCB (the side with no components mounted) is intended to be used as the top panel of the case.
This works best with a standard 1.6mm PCB thickness.
The inside of the case should be lined with a reflective material (e.g. aluminum foil) to reflect light from the LEDs back up through the neighboring drill holes and surrounding copper cutouts.

By default, the device functions as a reflective light meter.
The same design could be adapted for a spot meter or incident light meter by
placing a lens or diffuser over the sensor.

The LTR-303ALS maxes out at a little below the level of bright daylight, so it would usually be best to place an ND filter of around 3 stops over the sensor.
(The LTR-303ALS is very sensititive, so you won't lose any useful low-light sensitivity by doing this.)

The LTR-303ALS integrates over a period of 50-400ms, which is far too long for effective flash metering.
A different design would therefore be required for this purpose (probably using a discrete photodiode).
