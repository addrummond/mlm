#include <tests/include.h>

int test_main()
{
    SEGGER_RTT_printf(0, "Startn");

    for (;;) {
        RTC_Init_TypeDef rtc_init = {
            true, // Start counting when initialization is done
            false, // Enable updating during debug halt.
            true  // Restart counting from 0 when reaching COMP0.
        };

        RTC_Init(&rtc_init);
        RTC_IntEnable(RTC_IEN_COMP0);
        NVIC_EnableIRQ(RTC_IRQn);
        RTC_CompareSet(0, RTC->CNT + RTC_RAW_FREQ*5);
        RTC_IntClear(RTC_IFC_COMP0);

        my_emu_enter_em2(true);
    }

    return 0;
}