#
# See the following for a description of the method used to automatically
# calculate dependencies for C modules.
#
#     http://scottmcpeak.com/autodepend/autodepend.html
#

# ARM architecture object files have the extension .out.
# Build platform architecture object files have the extension .o.

ARMCC := arm-none-eabi-gcc
ARMCFLAGS := -g -Wall -Os -mcpu=cortex-m0 -ffunction-sections -fdata-sections -nostdlib -mthumb -DSTM32F030 -I ./ -I ./efm32 -Wall
OBJS := main.out rtt.out \
        $(patsubst %.c,%.out,$(shell echo efm32/*.c))

# TODO FIND EFM32 EQUIV
stm/startup_stm32f030.out: stm/startup_stm32f030.s
	$(ARMCC) $(ARMCFLAGS) -c stm/startup_stm32f030.s -o stm/startup_stm32f030.out

.PHONY: prereq
prereq: bitmaps/bitmaps.c menus/menus_strings_table.c tables.c stm/startup_stm32f030.out

-include $(OBJS:.out=.d)

%.out: %.c
	$(ARMCC) -c $(ARMCFLAGS) $*.c -o $*.out
	$(ARMCC) -MM $(ARMCFLAGS) $*.c | sed -e s/\.o:/\.out:/ > $*.d

/stm/STM32F030K6_FLASH.ld: /stm/STM32F030K6_FLASH.ld

out.elf: prereq /stm/STM32F030K6_FLASH.ld stm/startup_stm32f030.out $(OBJS)
	$(ARMCC) $(ARMCFLAGS) -Wl,--gc-sections -T ./stm/STM32F030K6_FLASH.ld $(OBJS) stm/startup_stm32f030.out -o out.elf

.PHONY: clean
clean:
	rm -f bitmaps/bitmaps.c bitmaps/bitmaps.h
	rm -f menus/menus_strings_table.c menus/menus_strings_table.h
	rm -f *.d bitmaps/*.d menus/*.d
	rm -f *.out bitmaps/*.out menus/*.out
	rm -f *.o bitmaps/*.o menus/*.o

GCC := gcc
GCCFLAGS := -I./

%.o: %.c
	$(GCC) -c $(GCCFLAGS) $*.c -o $*.o
	$(GCC) -MM $(GCCFLAGS) $*.c > $*.d

bcdtest: GCCFLAGS := $(GCCFLAGS) -DTEST
bcdtest: bcd.o mymemset.o
	$(GCC) $(GCCFLAGS) mymemset.o bcd.o -o bcdtest

exposuretest: GCCFLAGS := $(GCCFLAGS) -DTEST
exposuretest: exposuretest_bcd exposure.o tables.o mymemset.o
	$(GCC) $(GCCFLAGS) bcd.o exposure.o tables.o mymemset.o -o testexposure

# Required so that we don't compile bcd with -DTEST when building exposure with -DTEST.
exposuretest_bcd: GCCFLAGS:= $(GCCFLAGS)
exposuretest_bcd: bcd.o