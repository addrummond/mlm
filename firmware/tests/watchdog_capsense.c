#include <tests/include.h>

int test_main()
{
    uint32_t reset_cause = RMU_ResetCauseGet();
    RMU_ResetCauseClear();

    //RMU->CTRL &= ~0b111;
    //RMU->CTRL |= 1; // limited watchdog reset

    leds_all_off();
    leds_on(0b1000000);
    uint32_t base = leds_on_for_cycles;
    while (leds_on_for_cycles < base + RTC_RAW_FREQ/8)
        WDOGn_Feed(WDOG);
    leds_all_off();

    setup_le_capsense(LE_CAPSENSE_SENSE);
    //EMY_EnterEM2(true);
    WDOGn_Feed(WDOG);
    my_emu_enter_em2();

    SEGGER_RTT_printf(0, "RES %u\n", lesense_result);
    if (le_center_pad_is_touched(lesense_result)) {
        leds_all_off();
        leds_on(0b100000000000000000000000);
        uint32_t base = leds_on_for_cycles;
        while (leds_on_for_cycles < base + RTC_RAW_FREQ)
            WDOGn_Feed(WDOG);
        leds_all_off();
    }

    CMU_ClockEnable(cmuClock_CORELE, true);

    EMU_EM23Init_TypeDef dcdcInit = EMU_EM23INIT_DEFAULT;
    EMU_EM23Init(&dcdcInit);

    WDOG_Init_TypeDef wInit = WDOG_INIT_DEFAULT;
    wInit.debugRun = true; // Run in debug
    wInit.clkSel = wdogClkSelULFRCO;
    wInit.em2Run = true;
    wInit.em3Run = true;
    wInit.perSel = wdogPeriod_4k; // 1k 1kHz periods should give ~1 seconds in EM3
    wInit.enable = true;

    WDOGn_Init(WDOG, &wInit);
    WDOGn_Feed(WDOG);

    SEGGER_RTT_printf(0, "SLEEP %u\n", lesense_result);

    my_emu_enter_em1();

    // EM3 will be terminated by a reset, so we'll never get here.

    return 0;
}