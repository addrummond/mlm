JLink notes
-----

Start JLink GDB server as follows:

    /Applications/SEGGER/JLink/JLinkGDBServer -if SWD -Device EFM32TG108F32

Start gdb using

    arm-none-eabi-gdb

Commands for setting up semihosting are in `.gdbinit` in the `firmware` dir.

Use RTT (https://www.segger.com/jlink-rtt.html) to log debug output. See
functions defined in firmware/rrt/SEGGER_RTT.h (e.g. `SEGGER_RTT_printf`).
RTT functions are included via firmware/rtt.h which provides an rtt_init()
function that wraps RTT initialization logic and stubs for RTT functions
when DEBUG mode is off.

To view RTT output:

    /Applications/SEGGER/JLink/JLinkRTTClient

It seems to hook up with the JLink GDB server without the aid of any
parameters.


Other important points
-----

It is necessary to do a `monitor reset` and `monitor halt` before loading new
firmware, since otherwise things like counters and interrupts can get screwed
up even though most of the code appears to run normally.