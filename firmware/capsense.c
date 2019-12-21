#include <capsense.h>
#include <em_acmp.h>
#include <em_cmu.h>
#include <em_gpio.h>
#include <em_lesense.h>
#include <em_wdog.h>
#include <em_pcnt.h>
#include <em_prs.h>
#include <em_rtc.h>
#include <rtt.h>
#include <stdbool.h>
#include <time.h>

// https://www.silabs.com/content/usergenerated/asi/cloud/attachments/siliconlabs/en/community/mcu/32-bit/knowledge-base/jcr:content/content/primary/blog/low_power_capacitive-SbaF/capsense_rtc.c

static uint32_t touch_chan;
static uint32_t touch_acmp;

static uint32_t old_count;

static uint32_t calibration_values[3];
static uint32_t le_calibration_values[3];

static const PCNT_Init_TypeDef initPCNT =
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

void setup_capsense()
{
    GPIO_PinModeSet(gpioPortC, 0, gpioModeInput, 0);
    GPIO_PinModeSet(gpioPortC, 1, gpioModeInput, 0);
    GPIO_PinModeSet(gpioPortC, 14, gpioModeInput, 0);

    ACMP_CapsenseInit_TypeDef capsenseInit = ACMP_CAPSENSE_INIT_DEFAULT;
    CMU_ClockEnable(cmuClock_ACMP0, true);
    CMU_ClockEnable(cmuClock_ACMP1, true);
    CMU_ClockEnable(cmuClock_PRS, true);
    CMU_ClockEnable(cmuClock_PCNT0, true);
    ACMP_CapsenseInit(ACMP0, &capsenseInit);
    ACMP_CapsenseInit(ACMP1, &capsenseInit);

    while (!(ACMP0->STATUS & ACMP_STATUS_ACMPACT) || !(ACMP1->STATUS & ACMP_STATUS_ACMPACT))
        ;

    PRS_SourceAsyncSignalSet(0, PRS_CH_CTRL_SOURCESEL_ACMP0, PRS_CH_CTRL_SIGSEL_ACMP0OUT);

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
    ACMP_Enable(ACMP1);

    touch_acmp = 0;
    touch_chan = 0;
    old_count = 0;
}

void disable_capsense()
{
    ACMP0->CTRL &= ~ACMP_CTRL_IRISE_ENABLED;
    ACMP1->CTRL &= ~ACMP_CTRL_IRISE_ENABLED;
    ACMP_Disable(ACMP0);
    ACMP_Disable(ACMP1);
    NVIC_ClearPendingIRQ(ACMP0_IRQn);
    NVIC_DisableIRQ(ACMP0_IRQn);
    CMU_ClockEnable(cmuClock_ACMP0, false);
    CMU_ClockEnable(cmuClock_ACMP1, false);
    CMU_ClockEnable(cmuClock_PRS, false); // NEW
    CMU_ClockEnable(cmuClock_PCNT0, false); // NEW
    GPIO_PinModeSet(gpioPortC, 0, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortC, 1, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortC, 14, gpioModeInputPull, 0);
}

void cycle_capsense()
{
    if (touch_acmp == 0) {
        if (touch_chan == 0) {
            ACMP_CapsenseChannelSet(ACMP0, acmpChannel1);
            touch_chan = 1;
        } else {
            ACMP0->CTRL &= ~ACMP_CTRL_IRISE_ENABLED;
            ACMP1->CTRL |= ACMP_CTRL_IRISE_ENABLED;
            ACMP_CapsenseChannelSet(ACMP1, acmpChannel6);
            PRS_SourceAsyncSignalSet(0, PRS_CH_CTRL_SOURCESEL_ACMP1, PRS_CH_CTRL_SIGSEL_ACMP1OUT);
            touch_acmp = 1;
            touch_chan = 0;
        }
    } else {
        ACMP1->CTRL &= ~ACMP_CTRL_IRISE_ENABLED;
        ACMP0->CTRL |= ACMP_CTRL_IRISE_ENABLED;
        ACMP_CapsenseChannelSet(ACMP0, acmpChannel0);
        PRS_SourceAsyncSignalSet(0, PRS_CH_CTRL_SOURCESEL_ACMP0, PRS_CH_CTRL_SIGSEL_ACMP0OUT);

        touch_acmp = 0;
        touch_chan = 0;
    }
}

void calibrate_capsense()
{
    uint32_t count, chan;
    get_touch_count(&count, &chan);
    delay_ms(PAD_COUNT_MS);

    uint32_t chans_done = 0;
    do {
        get_touch_count(&count, &chan);
        if (! (chans_done & (1 << chan))) {
            calibration_values[chan] = count;
            chans_done |= (1 << chan);
        }
        
        cycle_capsense();
        get_touch_count(&count, &chan);

        delay_ms(PAD_COUNT_MS);
    } while (chans_done != 0b111);

    chans_done = 0;

    RTC_Enable(false);
    CMU_ClockDivSet(cmuClock_RTC, cmuClkDiv_1);
    RTC_Enable(true);

    cycle_capsense();
    get_touch_count(&count, &chan);
    uint32_t start = RTC->CNT;
    uint32_t end = RTC->CNT + LE_PAD_CLOCK_COUNT;
    while (RTC->CNT < end)
        ;

    do {
        get_touch_count(&count, &chan);
        if (! (chans_done & (1 << chan))) {
            le_calibration_values[chan] = count;
            chans_done |= (1 << chan);
        }

        cycle_capsense();
        get_touch_count(&count, &chan);

        start = RTC->CNT;
        end = start + LE_PAD_CLOCK_COUNT;
        while (RTC->CNT < end)
            ;
    } while (chans_done != 0b111);

    RTC_Enable(false);
    CMU_ClockDivSet(cmuClock_RTC, RTC_CMU_CLK_DIV);
    RTC_Enable(true);

    SEGGER_RTT_printf(0, "Capsense calibration values: %u %u %u (le: %u %u %u)\n", calibration_values[0], calibration_values[1], calibration_values[2], le_calibration_values[0], le_calibration_values[1], le_calibration_values[2]);
}

touch_position get_touch_position(uint32_t chan0, uint32_t chan1, uint32_t chan2)
{
    uint32_t NOTOUCH_THRESHOLD0 = calibration_values[0] * 2 / 3;
    uint32_t NOTOUCH_THRESHOLD1 = calibration_values[1] * 2 / 3;
    uint32_t NOTOUCH_THRESHOLD2 = calibration_values[2] * 2 / 3;

    if (chan0 < NOTOUCH_THRESHOLD0)
        return RIGHT_BUTTON;
    if (chan1 < NOTOUCH_THRESHOLD1)
        return LEFT_BUTTON;
    if (chan2 < NOTOUCH_THRESHOLD2)
        return CENTER_BUTTON;

    return NO_TOUCH_DETECTED;
}

bool center_pad_is_touched(uint32_t chan2)
{
    return chan2 != 0 && chan2 < calibration_values[2] * 2 / 3;
}

uint32_t get_touch_count(uint32_t *chan_value, uint32_t *chan)
{
    if (chan != 0)
        *chan = (touch_acmp << 1) | touch_chan;

    uint32_t raw_cnt = PCNT0->CNT;

    if (old_count == 0) {
        *chan_value = raw_cnt;
    } else if (raw_cnt > old_count) {
        *chan_value = raw_cnt - old_count;
    } else {
        *chan_value = 0xFFFF - old_count + raw_cnt;
    }

    old_count = raw_cnt;
    return raw_cnt;
}

#define LESENSE_ACMP_VDD_SCALE 0x37U

#define LESENSE_CAPSENSE_CH_CONF_SLEEP                                                                             \
  {                                                                                                                \
    true,                               /* Enable scan channel. */                                                 \
    true,                               /* Enable the assigned pin on scan channel. */                             \
    true,                               /* Enable interrupts on channel. */                                        \
    lesenseChPinExDis,                  /* GPIO pin is disabled during the excitation period. */                   \
    lesenseChPinIdleDis,                /* GPIO pin is disabled during the idle period. */                         \
    false,                              /* Don't use alternate excitation pins for excitation. */                  \
    false,                              /* Disabled to shift results from this channel to the decoder register. */ \
    false,                              /* Disabled to invert the scan result bit. */                              \
    true,                               /* Enabled to store counter value in the result buffer. */                 \
    lesenseClkLF,                       /* Use the LF clock for excitation timing. */                              \
    lesenseClkLF,                       /* Use the LF clock for sample timing. */                                  \
    0,                                  /* Excitation time is set to 0 excitation clock cycles. */                 \
    LE_PAD_CLOCK_COUNT,                 /* Sample delay */                                                         \
    1,                                  /* Measure delay */                                                        \
    LESENSE_ACMP_VDD_SCALE,             /* ACMP threshold has been set to LESENSE_ACMP_VDD_SCALE. */               \
    lesenseSampleModeCounter,           /* Counter will be used in comparison. */                                  \
    lesenseSetIntLevel,                 /* Interrupt is generated if the sensor triggers. */                       \
    0x00,                               /* Counter threshold has been set to 0x0. */                               \
    lesenseCompModeLess                 /* Compare mode has been set to trigger interrupt on >= */                 \
  }

#define LESENSE_CAPSENSE_CH_CONF_SENSE                                                                   \
  {                                                                                                      \
    true,                     /* Enable scan channel. */                                                 \
    true,                     /* Enable the assigned pin on scan channel. */                             \
    false,                    /* Disable interrupts on channel. */                                       \
    lesenseChPinExDis,        /* GPIO pin is disabled during the excitation period. */                   \
    lesenseChPinIdleDis,      /* GPIO pin is disabled during the idle period. */                         \
    false,                    /* Don't use alternate excitation pins for excitation. */                  \
    false,                    /* Disabled to shift results from this channel to the decoder register. */ \
    false,                    /* Disabled to invert the scan result bit. */                              \
    true,                     /* Enabled to store counter value in the result buffer. */                 \
    lesenseClkLF,             /* Use the LF clock for excitation timing. */                              \
    lesenseClkLF,             /* Use the LF clock for sample timing. */                                  \
    0x000,                    /* Excitation time is set to ___ excitation clock cycles. */               \
    LE_PAD_CLOCK_COUNT,       /* Sample delay is set to ___ sample clock cycles. */                      \
    0x0,                      /* Measure delay is set to ___ excitation clock cycles.*/                  \
    LESENSE_ACMP_VDD_SCALE,   /* ACMP threshold has been set to LESENSE_ACMP_VDD_SCALE. */               \
    lesenseSampleModeCounter, /* ACMP will be used in comparison. */                                     \
    lesenseSetIntLevel,       /* Interrupt is generated if the sensor triggers. */                       \
    0x0,                      /* Counter threshold has been set to 0x00. */                              \
    lesenseCompModeLess       /* Compare mode has been set to trigger interrupt on "less". */            \
  }

static const LESENSE_ChDesc_TypeDef chanConfigSense = LESENSE_CAPSENSE_CH_CONF_SENSE;
static const LESENSE_ChDesc_TypeDef chanConfigSleep = LESENSE_CAPSENSE_CH_CONF_SLEEP;

static const LESENSE_Init_TypeDef initLESENSE =
{
    .coreCtrl         =
    {
    .scanStart    = lesenseScanStartPeriodic,
    .prsSel       = lesensePRSCh0,
    .scanConfSel  = lesenseScanConfDirMap,
    .invACMP0     = false,
    .invACMP1     = false,
    .dualSample   = false,
    .storeScanRes = false,
    .bufOverWr    = true,
    .bufTrigLevel = lesenseBufTrigHalf,
    .wakeupOnDMA  = lesenseDMAWakeUpDisable,
    .biasMode     = lesenseBiasModeDutyCycle,
    .debugRun     = false
    },

    .timeCtrl         =
    {
    .startDelay     =          0U
    },

    .perCtrl          =
    {
    .dacCh0Data     = lesenseDACIfData,
    .dacCh0ConvMode = lesenseDACConvModeDisable,
    .dacCh0OutMode  = lesenseDACOutModeDisable,
    .dacCh1Data     = lesenseDACIfData,
    .dacCh1ConvMode = lesenseDACConvModeDisable,
    .dacCh1OutMode  = lesenseDACOutModeDisable,
    .dacPresc       =                        0U,
    .dacRef         = lesenseDACRefBandGap,
    .acmp0Mode      = lesenseACMPModeDisable,
    .acmp1Mode      = lesenseACMPModeMuxThres,
    .warmupMode     = lesenseWarmupModeNormal
    },

    .decCtrl          =
    {
    .decInput  = lesenseDecInputSensorSt,
    .chkState  = false,
    .intMap    = true,
    .hystPRS0  = false,
    .hystPRS1  = false,
    .hystPRS2  = false,
    .hystIRQ   = false,
    .prsCount  = true,
    .prsChSel0 = lesensePRSCh0,
    .prsChSel1 = lesensePRSCh1,
    .prsChSel2 = lesensePRSCh2,
    .prsChSel3 = lesensePRSCh3
    }
};

static le_capsense_mode le_mode;

void setup_le_capsense(le_capsense_mode mode)
{
    le_mode = mode;

    CMU_ClockEnable(cmuClock_ACMP1, true);
    CMU_ClockEnable(cmuClock_LESENSE, true);

    CMU_ClockDivSet(cmuClock_LESENSE, cmuClkDiv_1);

    GPIO_DriveModeSet(gpioPortC, gpioDriveModeStandard);
    GPIO_PinModeSet(gpioPortC, 14, gpioModeDisabled, 0);

    static const ACMP_CapsenseInit_TypeDef initACMP = {
        .fullBias                 = false,
        .halfBias                 = false,
        .biasProg                 = 0x7,
        .warmTime                 = acmpWarmTime512,
        .hysteresisLevel          = acmpHysteresisLevel7,
        .resistor                 = acmpResistor0,
        .lowPowerReferenceEnabled = false,
        .vddLevel                 = 0x3D,
        .enable                   = false
    };

    ACMP_GPIOSetup(ACMP1, 0, false, false);

    ACMP_CapsenseInit(ACMP1, &initACMP);

    LESENSE_Init(&initLESENSE, true);

    LESENSE_ScanStop();
    while (LESENSE_STATUS_SCANACTIVE & LESENSE_StatusGet())
        ;
    LESENSE_ResultBufferClear();
    LESENSE_ScanFreqSet(mode == LE_CAPSENSE_SENSE ? 0 : 8, 0);
    LESENSE_ClkDivSet(lesenseClkLF, lesenseClkDiv_1);

    if (mode == LE_CAPSENSE_SENSE) {
        LESENSE_ChannelConfig(&chanConfigSense, 14);
        LESENSE_IntEnable(LESENSE_IEN_SCANCOMPLETE);
    } else {
        LESENSE_ChannelConfig(&chanConfigSleep, 14);
        LESENSE_IntDisable(LESENSE_IEN_SCANCOMPLETE);
    }

    NVIC_EnableIRQ(LESENSE_IRQn);

    LESENSE_ScanModeSet(mode == LE_CAPSENSE_SLEEP ? lesenseScanStartPeriodic : lesenseScanStartOneShot, true);

    if (mode == LE_CAPSENSE_SLEEP) {
        while (!(LESENSE->STATUS & LESENSE_STATUS_BUFHALFFULL))
            ;
        LESENSE_ChannelThresSet(14, LESENSE_ACMP_VDD_SCALE, le_calibration_values[2]);
    }
}

void LESENSE_IRQHandler(void)
{
    LESENSE_IntClear(LESENSE_IEN_SCANCOMPLETE);
    LESENSE_IntClear(LESENSE_IF_CH14);

    // Stop additional interrupts screwing things up by setting threshold to zero.
    if (le_mode == LE_CAPSENSE_SLEEP)
        LESENSE_ChannelThresSet(14, LESENSE_ACMP_VDD_SCALE, 0);
}

void disable_le_capsense()
{
    if (le_mode == LE_CAPSENSE_SLEEP)
        LESENSE_ChannelThresSet(14, LESENSE_ACMP_VDD_SCALE, 0);

    LESENSE_ScanStop();

    while (LESENSE_STATUS_SCANACTIVE & LESENSE_StatusGet())
        ;

    ACMP_Disable(ACMP1);

    CMU_ClockEnable(cmuClock_ACMP1, false);
    CMU_ClockEnable(cmuClock_LESENSE, false);

    CMU_ClockDivSet(cmuClock_LESENSE, cmuClkDiv_1);

    GPIO_PinModeSet(gpioPortC, 14, gpioModeInputPull, 0);

    LESENSE_Reset();

    NVIC_DisableIRQ(LESENSE_IRQn);
}

// To be called after LESENSE wakeup to detect press kind
press get_center_pad_press()
{
    // We should define a setup function that just turns on ACM1,
    // but when I tried that I got weird issues. This works ok.
    // The extra power consumption of having both comparators on
    // shouldn't be significant, as we're doing all this in EM1.
    setup_capsense();
    cycle_capsense();
    cycle_capsense();

    const int long_press_ticks = LONG_PRESS_MS * RTC_FREQ / 1000;
    const int touch_count_ticks = PAD_COUNT_MS * RTC_FREQ / 1000;

    int next_touch_count = touch_count_ticks;
    press p;

    RTC->CNT = 0;
    RTC->CTRL |= RTC_CTRL_EN;

    uint32_t count;
    get_touch_count(&count, 0);

    int miss_count = 0;
    for (;;) {
        while (RTC->CNT < next_touch_count)
            ;
        
        next_touch_count = RTC->CNT + touch_count_ticks;
        
        get_touch_count(&count, 0);
        //SEGGER_RTT_printf(0, "COUNT %u\n", count);
        //next_touch_count = RTC->CNT + touch_count_ticks;
        //continue;

        if (! center_pad_is_touched(count)) {
            ++miss_count;
            if (miss_count > 1) {
                p = PRESS_TAP;
                break;
            }
        }

        if (RTC->CNT >= long_press_ticks) {
            p = PRESS_HOLD;
            break;
        }
    }

    RTC->CTRL &= ~RTC_CTRL_EN;
    disable_capsense();
    return p;
}
