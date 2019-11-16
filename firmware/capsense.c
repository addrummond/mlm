#include <capsense.h>
#include <em_acmp.h>
#include <em_cmu.h>
#include <em_gpio.h>
#include <em_pcnt.h>
#include <em_prs.h>

// https://www.silabs.com/content/usergenerated/asi/cloud/attachments/siliconlabs/en/community/mcu/32-bit/knowledge-base/jcr:content/content/primary/blog/low_power_capacitive-SbaF/capsense_rtc.c

uint32_t touch_chan;

static const uint32_t NOTOUCH_THRESHOLD = 1000;

static uint32_t old_count;

// Capsense pins are PC0 (S2), PC1 (S1)
void setup_capsense()
{
    GPIO_PinModeSet(gpioPortC, 0, gpioModeInput, 0);
    GPIO_PinModeSet(gpioPortC, 1, gpioModeInput, 0);

    ACMP_CapsenseInit_TypeDef capsenseInit = ACMP_CAPSENSE_INIT_DEFAULT;
    CMU_ClockEnable(cmuClock_ACMP0, true);
    CMU_ClockEnable(cmuClock_PRS, true);
    CMU_ClockEnable(cmuClock_PCNT0, true);
    ACMP_CapsenseInit(ACMP0, &capsenseInit);

    while (!(ACMP0->STATUS & ACMP_STATUS_ACMPACT))
        ;

    PRS_SourceAsyncSignalSet(0, PRS_CH_CTRL_SOURCESEL_ACMP0, PRS_CH_CTRL_SIGSEL_ACMP0OUT);
    
    PCNT_Init_TypeDef initPCNT =
    {
        .mode        = pcntModeExtSingle,   // External, single mode.
        .counter     = 0,                   // Counter value has been initialized to 0.
        .top         = 0xFFFF,              // Counter top value.
        .negEdge     = false,               // Use positive edge.
        .countDown   = false,               // Up-counting.
        .filter      = false,               // Filter disabled.
        .hyst        = false,               // Hysteresis disabled.
        .s1CntDir    = false,               // Counter direction is given by S1.
        .cntEvent    = pcntCntEventBoth,    // Regular counter counts up on upcount events.
        .auxCntEvent = pcntCntEventNone,    // Auxiliary counter doesn't respond to events.
        .s0PRS       = pcntPRSCh0,          // PRS channel 0 selected as S0IN.
        .s1PRS       = pcntPRSCh1           // PRS channel 1 selected as S1IN.
    };

    PCNT_Init(PCNT0, &initPCNT);

    PCNT_PRSInputEnable(PCNT0, pcntPRSInputS0, true);

    // Sync up to end PCNT initialization
    while(PCNT0->SYNCBUSY)
    {
        PRS_LevelSet(PRS_SWLEVEL_CH0LEVEL, PRS_SWLEVEL_CH0LEVEL);
        PRS_LevelSet(~PRS_SWLEVEL_CH0LEVEL, PRS_SWLEVEL_CH0LEVEL);
    }

    ACMP_CapsenseChannelSet(ACMP0, acmpChannel1);
    ACMP_Enable(ACMP0);
}

void disable_capsense()
{
    ACMP0->CTRL &= ~ACMP_CTRL_IRISE_ENABLED;
    NVIC_ClearPendingIRQ(ACMP0_IRQn);
    NVIC_DisableIRQ(ACMP0_IRQn);
    CMU_ClockEnable(cmuClock_ACMP0, false);
    CMU_ClockEnable(cmuClock_PRS, false); // NEW
    CMU_ClockEnable(cmuClock_PCNT0, false); // NEW
    GPIO_PinModeSet(gpioPortC, 0, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortC, 1, gpioModeInputPull, 0);
}

void cycle_capsense()
{
    if (touch_chan == 0) {
        ACMP_CapsenseChannelSet(ACMP0, acmpChannel1);
        touch_chan = 1;
    } else {
        ACMP_CapsenseChannelSet(ACMP0, acmpChannel0);
        touch_chan = 0;
    }
}

touch_position get_touch_position(uint32_t chan0, uint32_t chan1)
{
    if (chan0 == 0 || chan1 == 0)
        return NO_TOUCH_DETECTED;

    if (chan0 > NOTOUCH_THRESHOLD && chan1 > NOTOUCH_THRESHOLD)
        return NO_TOUCH_DETECTED;
    
    if (chan0 < chan1 * 5 / 6)
        return LEFT_BUTTON;
    
    if (chan1 < chan0 * 5 / 6)
        return RIGHT_BUTTON;
    
    return NO_TOUCH_DETECTED;
}

void get_touch_count(uint32_t *chan_value, uint32_t *chan)
{
    *chan = touch_chan;
    uint32_t raw_cnt = PCNT0->CNT;

    if (old_count == 0) {
        *chan_value = raw_cnt;
    } else if (raw_cnt > old_count) {
        *chan_value = raw_cnt - old_count;
    } else {
        *chan_value = 0xFFFF - old_count + raw_cnt;
    }

    old_count = raw_cnt;
}