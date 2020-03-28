#include <tests/include.h>

int test_main()
{
    for (;;) {
        setup_le_capsense(LE_CAPSENSE_SENSE);
        EMU_EnterEM2(true);
        SEGGER_RTT_printf(0, "(%u) %u\n", le_center_pad_is_touched(lesense_result), lesense_result);
    }

    return 0;
}