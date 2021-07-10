# Mimimalist Light Meter

<img src="https://user-images.githubusercontent.com/120347/86258444-bf85a100-bbb2-11ea-91e5-7cb454e6880c.jpg" width="250px">  <img src="https://user-images.githubusercontent.com/120347/86258443-beed0a80-bbb2-11ea-8225-8dd4185c6486.jpg" width="250px">  <img src="https://user-images.githubusercontent.com/120347/86258426-ba285680-bbb2-11ea-8737-4603d1c740bd.jpg" width="250px">

## User manual

The user manual is [here](https://github.com/addrummond/mlm/blob/master/manual/manual.md).

## Overview

This is a simple ambient light meter using a Liteon LTR-303ALS light sensor and
an EFM32 Tiny Gecko ARM microcontroller. It is powered by a single CR2032 coin cell.
The firmware is written in C and the PCB is designed using KiCad. Readings are
displayed using a circle of 24 LEDs, with two additional LEDs indicating ±⅓ stop
adjustments. The user interface consists of three touch sensitive pads.

The goal of the project is to make available a simple, compact and inexpensive
light meter suitable for analogue photography. 

Dimensions: 87×56x7.5mm

The bottom side of the PCB (the side with no components mounted) is intended to
be used as the top panel of the case. This works best with a standard 1.6mm PCB
thickness.

The case is designed for 3D printing. It's best to use multi-jet fusion, as PLA
and ABS are usually not very light tight. (You could use PLA/ABS and then paint
the inside of the case black.) The STL file and Fusion 360 design for the case
are in the `case` directory. The PCB attaches to the case using five M1.6 4mm
countersunk screws. The dimples under the LEDs should be painted white and then
given a gloss varnish. A 10×10×2mm piece of transparent acrylic should be glued
in place over the sensor window. (If you don't add this, adjust
`WINDOW_ATTENUATION_STOPS` in `config.h`.)

The device functions as a reflective light meter. The same design could be
adapted for a spot meter or incident light meter by placing a lens or diffuser
over the sensor.

The LTR-303ALS integrates over a period of 50-400ms, which is far too long for
effective flash metering. A different design would therefore be required for
this purpose (probably using a discrete photodiode).

## Build notes

See `firmware/Readme.md` for notes on the dev toolchain.

Note that a recent (as of June 2020) development version of KiCad will be
required to open the board and schema files. Version 5.99.0-3928-g3c8396cd0 is
know to work. Each component in the schematic has a `MAN` (manufacturer) and
`MPN` (manufacturer part number) property.

I recommend using the [interactive BOM](https://github.com/openscopeproject/InteractiveHtmlBom) plugin for KiCad to aid manual placement of components.

## Known issue

* LEDs a little dim in bright daylight.

I anticipate fixing this in future revisions.
