target remote localhost:2331
monitor semihosting enable
set mem inaccessible-by-default off
monitor semihosting breakOnError 1
monitor semihosting IOclient 3
