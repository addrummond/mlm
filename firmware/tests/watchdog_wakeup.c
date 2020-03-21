#include <tests/include.h>

int test_main()
{
    uint32_t reset_cause = RMU_ResetCauseGet();
    RMU_ResetCauseClear();
    leds_all_off();
    if ((reset_cause & RMU_RSTCAUSE_WDOGRST) == 0) {
        // First run.
        leds_on(0b1);
    } else {
        leds_on(0b1000);
    }

    //RMU->CTRL &= ~0b111;
    //RMU->CTRL |= 1; // limited watchdog reset

    uint32_t base = leds_on_for_cycles;
    while (leds_on_for_cycles < base + RTC_RAW_FREQ)
        ;

    leds_all_off();

    CMU_ClockEnable(cmuClock_CORELE, true);

    EMU_EM23Init_TypeDef dcdcInit = EMU_EM23INIT_DEFAULT;
    EMU_EM23Init(&dcdcInit);

    WDOG_Init_TypeDef wInit = WDOG_INIT_DEFAULT;
    wInit.debugRun = true; // Run in debug
    wInit.clkSel = wdogClkSelULFRCO;
    wInit.em2Run = true;
    wInit.em3Run = true;
    wInit.perSel = wdogPeriod_4k; // 4k 1kHz periods should give ~4 seconds in EM3
    wInit.enable = true;

    WDOGn_Init(WDOG, &wInit);
    WDOGn_Feed(WDOG);

    EMU_EnterEM3(false);

    // EM3 will be terminated by a reset, so we'll never get here.

    return 0;
}