#include <batsense.h>
#include <config.h>
#include <em_device.h>
#include <em_chip.h>
#include <em_cmu.h>
#include <em_emu.h>
#include <em_acmp.h>
#include <em_gpio.h>
#include <leds.h>
#include <macroutils.h>
#include <stdint.h>

static int battery_voltage_in_10th_volts_helper()
{
    // Turn on the MOSFET so that battery voltage reaches the pin.
    // We also have to temporarily lower all of the other DPINs to make
    // sure that no LEDs turn on.
#define M(n) GPIO_PinModeSet(DPIN ## n ## _GPIO_PORT, DPIN ## n ## _GPIO_PIN, gpioModePushPull, 0);
    DPIN_FOR_EACH(M)
#undef M

    // Disable GPIO as we're using ACMP on this pin.
    GPIO_PinModeSet(BATSENSE_PORT, BATSENSE_PIN, gpioModeDisabled, 0);

    // We don't have a proper ADC on this model of EFM32, but we can set the ACMP
    // reference voltage in 64ths of VREG and do a binary search. As the
    // battery voltage should be stable over a small time window, this
    // should be accurate enough.

    CMU_ClockEnable(cmuClock_ACMP1, true);

    uint32_t high = 64;
    uint32_t low = 0;
    int i;

    uint32_t avg;
    for (i = 50; i > 0 && high - low > 1; --i) { // i counter is just to ensure that loop is never infinite
        ACMP_Init_TypeDef acmp1_init = ACMP_INIT_DEFAULT;
        acmp1_init.enable = false;
        acmp1_init.vddLevel = (high + low) / 2;
        ACMP_Init(ACMP1, &acmp1_init);
        ACMP_ChannelSet(ACMP1, acmpChannelVDD, acmpChannel7);
        ACMP_Enable(ACMP1);

        // Wait for it to warm up.
        while (!(ACMP1->STATUS & _ACMP_STATUS_ACMPACT_MASK))
            ;
        
        avg = (high + low) / 2;
        int v = (ACMP1->STATUS & ACMP_STATUS_ACMPOUT) >> _ACMP_STATUS_ACMPOUT_SHIFT;

        ACMP_Disable(ACMP1);

        if (v)
            low = avg;
        else
            high = avg;
    }

    CMU_ClockEnable(cmuClock_ACMP1, false);

    // Turn off the MOSFET and set the other DPINs back to their default state.
#define M(n) GPIO_PinModeSet(DPIN ## n ## _GPIO_PORT, DPIN ## n ## _GPIO_PIN, gpioModeDisabled, 0);
    DPIN_FOR_EACH(M)
#undef M

    if (i == 0)
        return -1;
    
    return (330 * avg) / 640;
}

int battery_voltage_in_10th_volts()
{
    int v = battery_voltage_in_10th_volts_helper();
    if (v != -1)
        return v;
    
    // lie
    return 20;
}