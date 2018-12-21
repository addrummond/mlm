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
    //CMU_ClockEnable(cmuClock_TIMER1, true);

    rtt_init();
    SEGGER_RTT_printf(0, "\n\nHello RTT console; core clock freq = %u.\n", CMU_ClockFreqGet(cmuClock_CORE));

    /////

    // Ground PD6 and set pwm output on PD7.
    GPIO_PinModeSet(gpioPortD, 6, gpioModePushPull, 1);
    GPIO_PinModeSet(gpioPortD, 7, gpioModePushPull, 0);
    /*TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;
    timerCCInit.mode = timerCCModePWM;
    timerCCInit.cmoa = timerOutputActionToggle;
    TIMER_InitCC(TIMER1, 1, &timerCCInit);
    TIMER1->ROUTE |= (TIMER_ROUTE_CC1PEN | TIMER_ROUTE_LOCATION_LOC4);
    TIMER_TopSet(TIMER1, 100);
    TIMER_CompareBufSet(TIMER1, 1, 20); // duty cycle
    TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
    timerInit.prescale = timerPrescale256;
    TIMER_Init(TIMER1, &timerInit);*/


//    GPIO_PinModeSet(gpioPortD, 7, gpioModePushPull, 1);

    /////


    for (;;)
       ;

    return 0;
}