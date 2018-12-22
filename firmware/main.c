#include <stdint.h>
#include <stdbool.h>
#include <em_acmp.h>
#include <em_chip.h>
#include <em_cmu.h>
#include <em_device.h>
#include <em_emu.h>
#include <em_gpio.h>
#include <em_prs.h>
#include <em_timer.h>
#include <rtt.h>
#include <leds.h>
#include <utilities.h>

#define RTC_FREQ 32768

void delay_ms(int ms)
{
  uint32_t endValue = ms * RTC_FREQ / 1000;
  RTC->CNT = 0;
  
  RTC->CTRL |= RTC_CTRL_EN;
  
  while ( RTC->CNT < endValue );
  
  RTC->CTRL &= ~RTC_CTRL_EN;
}

void my_setup_capsense()
{
    ACMP_CapsenseInit_TypeDef capsenseInit = ACMP_CAPSENSE_INIT_DEFAULT;
    CMU_ClockEnable(cmuClock_ACMP0, true);
    CMU_ClockEnable(cmuClock_ACMP1, true);
    ACMP_CapsenseInit(ACMP0, &capsenseInit);
    ACMP_CapsenseInit(ACMP1, &capsenseInit);

    ACMP_CapsenseChannelSet(ACMP0, acmpChannel0);
    ACMP_CapsenseChannelSet(ACMP1, acmpChannel0);

    while (!(ACMP0->STATUS & ACMP_STATUS_ACMPACT) || !(ACMP1->STATUS & ACMP_STATUS_ACMPACT));

    ACMP_IntEnable(ACMP0, ACMP_IEN_EDGE);
    ACMP0->CTRL = ACMP0->CTRL | ACMP_CTRL_IRISE_ENABLED;

    //ACMP_IntEnable(ACMP1, ACMP_IEN_EDGE);
    //ACMP1->CTRL = ACMP1->CTRL | ACMP_CTRL_IRISE_ENABLED;

    NVIC_ClearPendingIRQ(ACMP0_IRQn);
    NVIC_EnableIRQ(ACMP0_IRQn);
}

static uint32_t touch_count0;
static uint32_t touch_count1;

void ACMP0_IRQHandler(void) {
	/* Clear interrupt flag */
	ACMP0->IFC = ACMP_IFC_EDGE;

    ++touch_count0;
}

#define ACMP_PERIOD_MS  100

void setup_capsense()
{
      /* Use the default STK capacative sensing setup */
      ACMP_CapsenseInit_TypeDef capsenseInit = ACMP_CAPSENSE_INIT_DEFAULT;
 
      CMU_ClockEnable(cmuClock_HFPER, true);
      CMU_ClockEnable(cmuClock_ACMP0, true);
 
      /* Set up ACMP1 in capsense mode */
      ACMP_CapsenseInit(ACMP0, &capsenseInit);
 
      // This is all that is needed to setup PC8, or the left-most slider
      // i.e. no GPIO routes or GPIO clocks need to be configured
      ACMP_CapsenseChannelSet(ACMP0, acmpChannel0);
 
      // Enable the ACMP1 interrupt
      ACMP_IntEnable(ACMP0, ACMP_IEN_EDGE);
      ACMP0->CTRL = ACMP0->CTRL | ACMP_CTRL_IRISE_ENABLED;
 
      // Wait until ACMP warms up
      while (!(ACMP0->STATUS & ACMP_STATUS_ACMPACT)) ;
 
      CMU_ClockEnable(cmuClock_PRS, true);
      CMU_ClockEnable(cmuClock_TIMER1, true);
 
      // Use TIMER1 to count ACMP events (rising edges)
      // It will be clocked by the capture/compare feature
      TIMER_Init_TypeDef timer_settings = TIMER_INIT_DEFAULT;
      timer_settings.clkSel = timerClkSelCC1;
      timer_settings.prescale = timerPrescale1024;
      TIMER_Init(TIMER1, &timer_settings);
      TIMER1->TOP  = 0xFFFF;
 
      // Set up TIMER1's capture/compare feature, to act as the source clock
      TIMER_InitCC_TypeDef timer_cc_settings = TIMER_INITCC_DEFAULT;
      timer_cc_settings.mode = timerCCModeCapture;
      timer_cc_settings.prsInput = true;
      timer_cc_settings.prsSel = timerPRSSELCh0;
      timer_cc_settings.eventCtrl = timerEventRising;
      timer_cc_settings.edge = timerEdgeBoth;
      TIMER_InitCC(TIMER1, 1, &timer_cc_settings);
 
      // Set up PRS so that TIMER1 CC1 can observe the event produced by ACMP0
      PRS_SourceSignalSet(0, PRS_CH_CTRL_SOURCESEL_ACMP0, PRS_CH_CTRL_SIGSEL_ACMP0OUT, prsEdgePos);
 
}

int main()
{
    CHIP_Init();

    CMU_ClockEnable(cmuClock_HFPER, true);
    CMU_ClockEnable(cmuClock_GPIO, true);

    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFRCO);
    CMU_ClockEnable(cmuClock_RTC, true);

    rtt_init();
    SEGGER_RTT_printf(0, "\n\nHello RTT console; core clock freq = %u.\n", CMU_ClockFreqGet(cmuClock_CORE));

    setup_utilities();
    //setup_capsense();
    my_setup_capsense();

    for (;;) {
        SEGGER_RTT_printf(0, "Count %u %u\n", touch_count0, touch_count1);

        // Clear the count
        touch_count0 = 0;
        touch_count1 = 0;

        delay(50);
    }

    /*for (unsigned i = 1;; ++i) {
        leds_all_off();
        SEGGER_RTT_printf(0, "Led %u\n", i);
        led_on(i);
    
        delay_ms(300);
    }*/

    return 0;
}