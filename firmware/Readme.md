## Development notes

The project builds using the
[GNU ARM embedded toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads)
and GNU make.

Production build:

```sh
cd firmware
make out.elf
```

Development build:

```sh
cd firmware
DEBUG=1 make out.elf
```

[RTT](https://www.segger.com/products/debug-probes/j-link/technology/about-real-time-transfer/)
is enabled in development builds.

Other environment variables:

* `DEBUG_DEEP_SLEEP=1` Reduces the time before the device enters deep sleep mode and shows a special pattern of LEDs before it does so.

* `NOGRACE=1` Removes the grace period before initial calibration (i.e. the period where the f8 LED is turned on after
power up). This is the default behavior when `DEBUG=1`.

* `GRACE=1` Overrides the aforementioned default behavior when `DEBUG=1`.

* `NO_RTT=1` Disables RTT (already disabled unless `DEBUG=1`).

* `LEDS_WRONG_WAY_ROUND=1` Set this if you have soldered all of the LEDs the 'wrong' way round. (As every LED lead is connected to a GPIO pin, it does not really matter which way round the LEDs are as long as they have a consistent orientation.)

## Debugging using JLink

I have been using the built-in JLink debugger of the [STK3200](https://www.silabs.com/products/development-tools/mcu/32-bit/efm32-zero-gecko-starter-kit).
These instruction should work for other SiLabs EFM32 series dev kits and for a real JLink probe.
When using one of the SiLabs dev kits, you'll first have to open up Simplicity Studio
and set the debug mode of the kit to 'out'.
You can download the JLink command line tools from [Segger's website](https://www.segger.com/downloads/jlink/#J-LinkSoftwareAndDocumentationPack).

Start JLink GDB server as follows:

    JLinkGDBServer -if SWD -Device EFM32TG108F32

Start gdb using

    arm-none-eabi-gdb

Commands for setting up semihosting are in `.gdbinit` in the `firmware` dir.
They'll be run automatically if you start GDB from within this dir.

Use RTT (https://www.segger.com/jlink-rtt.html) to log debug output. See
functions defined in firmware/rrt/SEGGER_RTT.h (e.g. `SEGGER_RTT_printf`).
RTT functions are included via firmware/rtt.h which provides an rtt_init()
function that wraps RTT initialization logic and stubs for RTT functions
when DEBUG mode is off.

To view RTT output:

    JLinkRTTClient

It seems to hook up with the JLink GDB server without the aid of any
parameters.

## Notes

* An unfortunate feature of the TPS610981 boost converter is that it has a tendency to misbehave
during startup if there is an initial current spike and the power source has
a high impedance. Coin cells, especially when at less than full capacity,
have a relatively high impedance. I am still not sure exactly what happens, but the
issue appears to be the same (unsolved) one that you can read about in
[this forum post](http://e2e.ti.com/support/power-management/f/196/t/827996?TPS610981-Converter-in-latch-up-mode).
From the point of view of using the device normally, I believe that I've solved
this problem by bumping up the input capacitance slightly and making firmware
changes to reduce current spiking on startup. However, when the debugger is
connected, things are less predictable. I recommend using the 3.3V power out
of the STK3200 while programming and debugging the device. (Just attach a crocodile clip to the
coin cell retainer.) If you really want to use battery power while debugging,
it's best to power up the device first
and then connect the debugging leads. Even then, you may occasionally
find that the TPS610981 gets into a weird state. If this happens, just remove
the battery. It takes coin cells a little while to recover from being shorted,
but after a few minutes, the coin cell's open circuit voltage should be back to normal
(though the cell will no doubt be somewhat the worse for wear).

* It is necessary to do a `monitor reset` and `monitor halt` before loading new
firmware, since otherwise things like counters and interrupts can get screwed
up even though most of the code appears to run normally.
