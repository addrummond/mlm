#include <stdint.h>
#include <stdbool.h>
#include <em_chip.h>
#include <em_gpio.h>
#include <em_cmu.h>
#include <em_device.h>
#include <em_i2c.h>
#include <em_emu.h>
#include <rtt.h>

int main()
{
    CHIP_Init();

    CMU_ClockEnable(cmuClock_HFPER, true);

    rtt_init();
    SEGGER_RTT_printf(0, "\n\nHello RTT console; core clock freq = %u.\n", CMU_ClockFreqGet(cmuClock_CORE));

    for (;;)
       ;

    return 0;
}