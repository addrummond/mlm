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
#include <sensor.h>
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

static uint32_t touch_counts[4];
static uint32_t touch_acmp;
static uint32_t touch_chan;
static uint32_t touch_index;
static bool touch_on = true;

void my_setup_capsense()
{
    ACMP_CapsenseInit_TypeDef capsenseInit = ACMP_CAPSENSE_INIT_DEFAULT;
    CMU_ClockEnable(cmuClock_ACMP0, true);
    CMU_ClockEnable(cmuClock_ACMP1, true);
    ACMP_CapsenseInit(ACMP0, &capsenseInit);
    ACMP_CapsenseInit(ACMP1, &capsenseInit);

    ACMP_CapsenseChannelSet(ACMP0, acmpChannel0);
    ACMP_CapsenseChannelSet(ACMP1, acmpChannel6);

    while (!(ACMP0->STATUS & ACMP_STATUS_ACMPACT) || !(ACMP1->STATUS & ACMP_STATUS_ACMPACT));

    ACMP_IntEnable(ACMP0, ACMP_IEN_EDGE);
    ACMP0->CTRL |= ACMP_CTRL_IRISE_ENABLED;

    NVIC_ClearPendingIRQ(ACMP0_IRQn);
    NVIC_EnableIRQ(ACMP0_IRQn);

    ACMP_Enable(ACMP0);
    ACMP_Enable(ACMP1);
}

void my_cycle_capsense()
{
    if (touch_acmp == 0) {
        if (touch_chan == 0) {
            ACMP_CapsenseChannelSet(ACMP0, acmpChannel1);
            touch_chan = 1;
            touch_index = 1;
        } else {
            ACMP0->CTRL &= ~ACMP_CTRL_IRISE_ENABLED;
            ACMP_IntDisable(ACMP0, ACMP_IEN_EDGE);
            ACMP_IntEnable(ACMP1, ACMP_IEN_EDGE);
            ACMP1->CTRL |= ACMP_CTRL_IRISE_ENABLED;
            ACMP_CapsenseChannelSet(ACMP1, acmpChannel6);
            touch_acmp = 1;
            touch_chan = 0;
            touch_index = 2;
        }
    } else {
        if (touch_chan == 0) {
            ACMP_CapsenseChannelSet(ACMP1, acmpChannel7);
            touch_chan = 1;
            touch_index = 3;
        } else {
            ACMP1->CTRL &= ~ACMP_CTRL_IRISE_ENABLED;
            ACMP_IntDisable(ACMP1, ACMP_IEN_EDGE);
            ACMP_IntEnable(ACMP0, ACMP_IEN_EDGE);
            ACMP0->CTRL |= ACMP_CTRL_IRISE_ENABLED;
            ACMP_CapsenseChannelSet(ACMP0, acmpChannel0);
            touch_acmp = 0;
            touch_chan = 0;
            touch_index = 0;
        }
    }
}

void my_clear_capcounts()
{
    touch_counts[0] = 0;
    touch_counts[1] = 0;
    touch_counts[2] = 0;
    touch_counts[3] = 0;
}

void ACMP0_IRQHandler(void) {
	/* Clear interrupt flag */
  	ACMP0->IFC = ACMP_IFC_EDGE;
	ACMP1->IFC = ACMP_IFC_EDGE;

    if (touch_on)
        ++touch_counts[touch_index];
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

    CMU_ClockEnable(cmuClock_CORELE, true);
    CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFXO);
    CMU_OscillatorEnable(cmuOsc_LFXO, true, true);
    CMU_ClockSelectSet(cmuClock_RTC, cmuSelect_LFXO);
    CMU_ClockEnable(cmuClock_RTC, true);

    rtt_init();
    SEGGER_RTT_printf(0, "\n\nHello RTT console; core clock freq = %u.\n", CMU_ClockFreqGet(cmuClock_CORE));

    leds_all_off();

    sensor_init();
    delay_ms(100);
    sensor_turn_on();
    delay_ms(100);
    //uint8_t status = sensor_read_reg(REG_ALS_STATUS);
    //SEGGER_RTT_printf(0, "Status %u\n", status);
    uint8_t part_id = sensor_read_reg(REG_MANUFAC_ID);
    SEGGER_RTT_printf(0, "Part id %u\n", part_id);
    sensor_write_reg(REG_ALS_CONTR, 0b1110000);
    delay_ms(100);
    //SEGGER_RTT_printf(0, "Status af %u\n", status);
    for (;;) { }

    /*sensor_turn_on();
    delay_ms(100);

    for (;;) {
        sensor_reading sr = sensor_get_reading();
        SEGGER_RTT_printf(0, "READING %u %u\n", sr.chan0, sr.chan1);
        for (;;) { }
        delay_ms(500);
    }*/


/*
    setup_utilities();
    //setup_capsense();
    my_setup_capsense();

    for (unsigned i = 0;; i++) {
        if (i % (4*6) == 0) {
            touch_on = false;
            SEGGER_RTT_printf(0, "Count %u %u %u %u\n", touch_counts[0], touch_counts[1], touch_counts[2], touch_counts[3]);
            touch_on = true;
            my_clear_capcounts();
        }

        my_cycle_capsense();

        delay(10);
    }
*/

    /*for (unsigned i = 1;; ++i) {
        leds_all_off();
        SEGGER_RTT_printf(0, "Led %u\n", i);
        led_on(i);
    
        delay_ms(300);
    }*/

    return 0;
}