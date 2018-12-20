#
# See the following for a description of the method used to automatically
# calculate dependencies for C modules.
#
#     http://scottmcpeak.com/autodepend/autodepend.html
#

# ARM architecture object files have the extension .out.
# Build platform architecture object files have the extension .o.

INCLUDES := -I ./ -I ./efm32/emlib/inc -I ./efm32/CMSIS/Include -I ./efm32/EFM32TG/Include
ARMCC := arm-none-eabi-gcc
ARMCFLAGS := -std=c11 -g -Wall -Os -mcpu=cortex-m0 -ffunction-sections -fdata-sections -nostdlib -mthumb -DEFM32TG108F32 ${INCLUDES} -Wall $(ARMCFLAGS)
OBJS := main.out rtt.out efm32/EFM32TG/Source/system_efm32tg.c efm32/emlib/src/em_system.c

efm32/EFM32TG/Source/GCC/startup_efm32tg.o: efm32/EFM32TG/Source/GCC/startup_efm32tg.c efm32/EFM32TG/Source/GCC/startup_efm32tg.S
	$(ARMCC) $(ARMCFLAGS) -c efm32/EFM32TG/Source/GCC/startup_efm32tg.c efm32/EFM32TG/Source/GCC/startup_efm32tg.S -o efm32/EFM32TG/Source/GCC/startup_efm32tg.out

-include $(OBJS:.out=.d)

%.out: %.c
	$(ARMCC) -c $(ARMCFLAGS) $*.c -o $*.out
	$(ARMCC) -MM $(ARMCFLAGS) $*.c | sed -e s/\.o:/\.out:/ > $*.d

out.elf: efm32/EFM32TG/Source/gcc/startup_efm32tg.out  $(OBJS)
	$(ARMCC) $(ARMCFLAGS) -lnosys -Xlinker --gc-sections -Xlinker -Map efm32/efm32tg.map --specs nano.specs -T ./efm32/efm32tg.ld $(OBJS) efm32/EFM32TG/Source/GCC/startup_efm32tg.out efm32/EFM32TG/Source/system_efm32tg.out -o out.elf

.PHONY: clean
clean:
	find ./ -name '*.out' -exec rm {} \;