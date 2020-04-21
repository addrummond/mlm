#include <tests/include.h>

uint8_t cnt __attribute__((section (".persistent")));

int test_main()
{
    uint32_t reset_cause = RMU_ResetCauseGet();
    RMU_ResetCauseClear();
    leds_all_off();
    if ((reset_cause & RMU_RSTCAUSE_WDOGRST) == 0) {
        // First run.
        leds_on(0b1);
    } else {
        ++cnt;
        cnt %= 24;
        leds_on((1 << (cnt % 24)));
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

    WDOG_Init_TypeDef wInit = {
        .debugRun = true,
        .em2Run = true,
        .em3Run = true,
        .em4Block = true,
        .enable = true,
        .lock = false,
        .perSel = wdogPeriod_4k, // 4k 1kHz periods should give ~4 seconds in EM3
        .swoscBlock = false
    };

    WDOGn_Init(WDOG, &wInit);
    WDOGn_Feed(WDOG);

    EMU_EnterEM3(false);

    // EM3 will be terminated by a reset, so we'll never get here.

    return 0;
}