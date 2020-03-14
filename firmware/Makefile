DEVICE := EFM32TG108F32
ARMCC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy

CC := gcc

EFM32 := efm32
LD := efm32/load.ld
STARTUP := $(EFM32)/EFM32TG/Source/GCC/startup_efm32tg

override ARMCFLAGS += -std=gnu99 -g -Wall -mcpu=cortex-m3 \
                      -ffunction-sections -fdata-sections -mthumb \
                      -D$(DEVICE) \
                      -I./ -I$(EFM32)/emlib/inc -I$(EFM32)/CMSIS/Include -I$(EFM32)/EFM32TG/Include \
                      -I$(EFM32)/CMSIS/Include \
                      -I$(EFM32)/emlib/inc
                      -save-temps=obj

override CFLAGS += -I./

ifdef DEBUG
    # Override seems to be necessary here too because it's used in the original
    # definition.
    override ARMCFLAGS += -DDEBUG
else
    override ARMCFLAGS += -O2
endif

ifdef NOGRACE
    override ARMCFLAGS += -DNOGRACE
endif

ifdef NO_RTT
    override ARMCFLAGS += -DNO_RTT
endif

override ARMASMFLAGS += -x assembler-with-cpp -D$(DEVICE) -Wall -Wextra -mcpu=cortex-m3 -mthumb -DDEBUG_EFM
ifdef DEBUG
    override ARMASMFLAGS += -DDEBUG_EFM
endif
override ARMASMFLAGS += $(DEPFLAGS)

OBJS := main.out \
        batsense.out \
        capsense.out \
        leds.out \
        sensor.out \
        units.out \
        util.out \
        state.out \
        time.out \
        rtc.out \
        init.out \
        tempsensor.out \
        iso.out \
        $(EFM32)/EFM32TG/Source/system_efm32tg.out \
        $(patsubst %.c,%.out,$(shell find $(EFM32)/emlib/src -name '*.c'))

ifdef DEBUG
    OBJS += rtt.out rtt/SEGGER_RTT.out rtt/SEGGER_RTT_printf.out
endif

LIBS := -Wl,--start-group -lgcc -lc_nano -lnosys -Wl,--end-group

$(STARTUP).out: $(STARTUP).S
	$(ARMCC) $(ARMASMFLAGS) -c $(STARTUP).S -o $(STARTUP).out

%.out: %.c
	$(ARMCC) -c $(ARMCFLAGS) $*.c -o $*.out
	$(ARMCC) -MM $(ARMCFLAGS) $*.c | sed -e 's|^.*:|$*.o:|' | sed -e s/\.o:/\.out:/ > $*.d

-include $(OBJS:.out=.d)

out.elf: $(LD) $(STARTUP).out $(OBJS)
	$(ARMCC) $(ARMCFLAGS) -Wl,--gc-sections -T $(LD) $(OBJS) $(STARTUP).out $(LIBS) -o out.elf && \
	$(OBJCOPY) -S -O binary out.elf out.bin

.PHONY: clean
clean:
	find ./ -name '*.out' -exec rm -f {} \;
	find ./ -name '*.o' -exec rm -f {} \;
	find ./ -name '*.d' -exec rm -f {} \;
	rm -f units_test

units_test: units.c units.h sensor.h
	$(CC) -I./ -DTEST units.c -o units_test