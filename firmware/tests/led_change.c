#include <tests/include.h>

int test_main()
{
    leds_all_off();
    uint32_t v = 1;
    for (;;) {
        SEGGER_RTT_printf(0, "MASK %u\n", v);
        leds_on(v);

        uint32_t base = leds_on_for_cycles;
        while (leds_on_for_cycles < base + RTC_RAW_FREQ / 4)
            ;

        leds_all_off();

        v <<= 1;
        if (v > (1 << 25))
            v = 1;
    }

    for (;;) {
        SEGGER_RTT_printf(0, "First pattern\n");
        leds_on(0b101);
        uint32_t base_cycles = leds_on_for_cycles;
        while (leds_on_for_cycles < base_cycles + RTC_RAW_FREQ)
            ;
        SEGGER_RTT_printf(0, "Second pattern\n");
        leds_all_off();
        CMU_ClockSelectSet(cmuClock_RTC, cmuSelect_LFRCO);
        CMU_ClockDivSet(cmuClock_RTC, RTC_CMU_CLK_DIV);
        CMU_ClockEnable(cmuClock_RTC, true);
        delay_ms_cyc(1000);
        //leds_on(0b00011);
        //base_cycles = leds_on_for_cycles;
        //while (leds_on_for_cycles < base_cycles + RTC_RAW_FREQ)
        //    ;
    }

    return 0;
}
