<img src="https://user-images.githubusercontent.com/120347/83353560-5c061a80-a34b-11ea-853b-c11924ef79d6.jpg" width="250px">

56x87x7.5mm

This is a simple ambient light meter using a Liteon LTR-303ALS light sensor and
an EFM32 Tiny Gecko microcontroller. It is powered by a single CR2032 coin cell.
The firmware is written in C and the PCB is designed using KiCad. Readings are
displayed using a circle of 24 LEDs, with two additional LEDs indicating ±⅓ stop
adjustments. The user interface consists of three touch sensitive pads.

The user manual is available in `manual/manual.md`.

The bottom side of the PCB (the side with no components mounted) is intended to
be used as the top panel of the case. This works best with a standard 1.6mm PCB
thickness.

The case is 3D printed. It's best to use multi-jet fusion, as PLA and ABS are
usually not very light tight. (You could use PLA/ABS and then paint the inside
of the case black.) The Fusion 360 design for the case is exported in
`case/case.f3d`. The PCB attaches using five M1.6 4mm countersunk screws. The
dimples under the LEDs should be painted white and then given a gloss varnish. A
10x10x2mm piece of transparent acrylic should be glued in place over the sensor
window. (If you don't want to add this, adjust `WINDOW_ATTENUATION_STOPS` in
`config.h`.)

The device functions as a reflective light meter. The same design could be
adapted for a spot meter or incident light meter by placing a lens or diffuser
over the sensor.

The LTR-303ALS integrates over a period of 50-400ms, which is far too long for
effective flash metering. A different design would therefore be required for
this purpose (probably using a discrete photodiode).

See `firmware/Readme.md` for notes on the dev toolchain.

Note that a recent (as of June 2020) development version of KiCad will be
required to open the board and schema files.
