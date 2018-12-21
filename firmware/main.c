#include <stdint.h>
#include <stdbool.h>
#include <em_chip.h>
#include <em_gpio.h>
#include <em_cmu.h>
#include <em_device.h>
#include <em_timer.h>
#include <rtt.h>
#include <leds.h>

int main()
{
    CHIP_Init();

    CMU_ClockEnable(cmuClock_HFPER, true);
    CMU_ClockEnable(cmuClock_GPIO, true);

    rtt_init();
    SEGGER_RTT_printf(0, "\n\nHello RTT console; core clock freq = %u.\n", CMU_ClockFreqGet(cmuClock_CORE));

    leds_all_off();
    led_on(7);

    SEGGER_RTT_printf(0, "Led on\n");

    for (;;) {

    }

    return 0;
}