#include <stdint.h>
#include <stdbool.h>
#include <em_chip.h>
#include <em_gpio.h>
#include <em_cmu.h>
#include <em_device.h>
#include <em_timer.h>
#include <rtt.h>

int main()
{
    CHIP_Init();

    CMU_ClockEnable(cmuClock_HFPER, true);
    CMU_ClockEnable(cmuClock_GPIO, true);
    CMU_ClockEnable(cmuClock_TIMER1, true);

    rtt_init();
    SEGGER_RTT_printf(0, "\n\nHello RTT console; core clock freq = %u.\n", CMU_ClockFreqGet(cmuClock_CORE));

    /////

    // Ground PD6 and set pwm output on PD7.
    GPIO_PinModeSet(gpioPortD, 6, gpioModePushPull, 0);
    GPIO_PinModeSet(gpioPortD, 7, gpioModePushPull, 0);
    TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;
    timerCCInit.mode = timerCCModePWM;
    timerCCInit.cmoa = timerOutputActionToggle;
    TIMER_InitCC(TIMER1, 0, &timerCCInit);
    TIMER1->ROUTE |= (TIMER_ROUTE_CC0PEN | TIMER_ROUTE_LOCATION_LOC4);
    TIMER_TopSet(TIMER1, 100);
    TIMER_CompareBufSet(TIMER1, 0, 1); // duty cycle
    TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
    timerInit.prescale = timerPrescale256;
    TIMER_Init(TIMER1, &timerInit);

    SEGGER_RTT_printf(0, "Timer set\n", CMU_ClockFreqGet(cmuClock_CORE));

//    GPIO_PinModeSet(gpioPortD, 7, gpioModePushPull, 1);

    /////


    unsigned b = 0;
    for (;;b++) {
        unsigned cycle_delay = 25000     * 14 - 28;
        TIMER_CounterSet(TIMER0, 0);
        TIMER0->CMD = TIMER_CMD_START;
        while(TIMER0->CNT < cycle_delay)
            ;
        TIMER_CompareBufSet(TIMER1, 0, b % 25); // duty cycle
        TIMER0->CMD = TIMER_CMD_STOP;
    }

    return 0;
}