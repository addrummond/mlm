
#include <tests/include.h>

int test_main()
{
    for (;;) {
        int v = battery_voltage_in_10th_volts();
        SEGGER_RTT_printf(0, "V: %u\n", v);
        delay_ms(1000);
    }

    return 0;
}