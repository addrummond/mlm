#
# See the following for a description of the method used to automatically
# calculate dependencies for C modules.
#
#     http://scottmcpeak.com/autodepend/autodepend.html
#

# ARM architecture object files have the extension .out.
# Build platform architecture object files have the extension .o.

INCLUDES := -I ./ -I ./efm32/emlib/inc -I ./efm32/EFM32TG/Include -I ./efm32/CMSIS/Include
ARMCC := arm-none-eabi-gcc
override ARMCFLAGS += -std=gnu99 -g -Wall -Os -mcpu=cortex-m3 -ffunction-sections -fdata-sections -mthumb -DEFM32TG108F32 ${INCLUDES} -Wall
OBJS := efm32/EFM32TG/Source/system_efm32tg.out \
        $(patsubst %.c,%.out,$(shell find efm32/emlib/src -name '*.c')) \
        rtt/SEGGER_RTT.out \
		rtt/SEGGER_RTT_printf.out \
        main.out \
        rtt.out

ifdef DEBUG
    # Override seems to be necessary here too because it's used in the original
    # definition.
    override ARMCFLAGS += -DDEBUG
endif

efm32/EFM32TG/Source/GCC/startup_efm32tg.out: efm32/EFM32TG/Source/GCC/startup_efm32tg.S
	$(ARMCC) -c $(ARMCFLAGS) -x assembler-with-cpp -DDEBUG_EFM efm32/EFM32TG/Source/GCC/startup_efm32tg.S -o efm32/EFM32TG/Source/GCC/startup_efm32tg.out

%.out: %.c
	$(ARMCC) -c $(ARMCFLAGS) $*.c -o $*.out
	$(ARMCC) -MM $(ARMCFLAGS) $*.c  | sed -e 's|^.*:|$*.o:|' | sed -e s/\.o:/\.out:/ > $*.d

-include $(OBJS:.out=.d)

out.elf: efm32/EFM32TG/Source/GCC/startup_efm32tg.out $(OBJS)
	$(ARMCC) $(ARMCFLAGS) -Xlinker -lnosys -Xlinker --gc-sections -Xlinker -lgcc -Xlinker -Map efm32/efm32tg.map --specs nano.specs -T efm32/load.ld efm32/EFM32TG/Source/GCC/startup_efm32tg.out $(OBJS) -o out.elf

.PHONY: clean
clean:
	find ./ -name '*.out' -exec rm {} \;
	find ./ -name '*.d' -exec rm {} \;