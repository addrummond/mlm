#include <tests/include.h>

int test_main()
{
    for (;;) {
        SEGGER_RTT_printf(0, "LOOP\n");
        setup_le_capsense(LE_CAPSENSE_SLEEP);
        EMU_EnterEM2(true);
        disable_le_capsense();
        setup_capsense();
        press p = get_pad_press(CENTER_BUTTON);
        switch (p) {
            case PRESS_TAP:
                SEGGER_RTT_printf(0, "TAP!\n");
                break;
            case PRESS_HOLD:
                SEGGER_RTT_printf(0, "HOLD!\n");
                break;
            default:
                SEGGER_RTT_printf(0, "Unknown press type\n");
        }
    }

    return 0;
}