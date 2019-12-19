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

// Capsense pins are PC14 (S3), PC0 (S2), PC1 (S1)
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
}

void disable_capsense()
{
    ACMP0->CTRL &= ~ACMP_CTRL_IRISE_ENABLED;
    ACMP1->CTRL &= ~ACMP_CTRL_IRISE_ENABLED;
    NVIC_ClearPendingIRQ(ACMP0_IRQn);
    NVIC_DisableIRQ(ACMP0_IRQn);
    CMU_ClockEnable(cmuClock_ACMP0, false);
    CMU_ClockEnable(cmuClock_ACMP1, false);
    CMU_ClockEnable(cmuClock_PRS, false); // NEW
    CMU_ClockEnable(cmuClock_PCNT0, false); // NEW
    GPIO_PinModeSet(gpioPortC, 0, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortC, 1, gpioModeInputPull, 0);
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
    uint32_t NOTOUCH_THRESHOLD0 = calibration_values[0] * 5 / 4;
    uint32_t NOTOUCH_THRESHOLD1 = calibration_values[1] * 5 / 4;
    uint32_t NOTOUCH_THRESHOLD2 = calibration_values[2] * 5 / 4;

    if (chan0 == 0 || chan1 == 0 || chan2 == 0)
        return NO_TOUCH_DETECTED;

    if (chan0 > NOTOUCH_THRESHOLD0 && chan1 > NOTOUCH_THRESHOLD1 && chan2 > NOTOUCH_THRESHOLD2)
        return NO_TOUCH_DETECTED;
    
    static const uint32_t ratnum = 15;
    static const uint32_t ratdenom = 20;

#define LT(c1, c2) \
    ((chan ## c1 * calibration_values[c2] / calibration_values[c1]) < chan ## c2 * ratnum / ratdenom)
    
    if (LT(0, 1) || LT(0, 2))
        return RIGHT_BUTTON;
    
    if (LT(1, 0) || LT(1, 2))
        return LEFT_BUTTON;

    if (LT(2, 0) || LT(2, 1))
        return CENTER_BUTTON;
    
    return NO_TOUCH_DETECTED;

#undef LT
}

void get_touch_count(uint32_t *chan_value, uint32_t *chan)
{
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
}

#define LESENSE_ACMP_VDD_SCALE    0x37U

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
    0,                                  /* Measure delay */                                                        \
    LESENSE_ACMP_VDD_SCALE,             /* ACMP threshold has been set to LESENSE_ACMP_VDD_SCALE. */               \
    LESENSE_CH_INTERACT_SAMPLE_COUNTER, /* Counter will be used in comparison. */                                  \
    LESENSE_CH_INTERACT_SETIF_NONE,     /* Interrupt is generated if the sensor triggers. */                       \
    0x0,                                /* Counter threshold has been set to 0x0. */                               \
    LESENSE_CH_EVAL_COMP_GE             /* Compare mode has been set to trigger interrupt on >= */                 \
  }

static const LESENSE_ChDesc_TypeDef chanConfig = LESENSE_CAPSENSE_CH_CONF_SLEEP;

static const LESENSE_Init_TypeDef  initLESENSE =
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
    .acmp0Mode      = lesenseACMPModeMuxThres,
    .acmp1Mode      = lesenseACMPModeDisable,
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

void setup_le_capsense()
{
    static bool init = true;

    CMU_ClockEnable(cmuClock_ACMP0, true);
    CMU_ClockEnable(cmuClock_LESENSE, true);

    CMU_ClockDivSet(cmuClock_LESENSE, cmuClkDiv_1);

    GPIO_DriveModeSet(gpioPortC, gpioDriveModeStandard);
    GPIO_PinModeSet(gpioPortC, 0, gpioModeDisabled, 0);
    GPIO_PinModeSet(gpioPortC, 1, gpioModeDisabled, 0);

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

    ACMP_GPIOSetup(ACMP0, 0, false, false);

    ACMP_CapsenseInit(ACMP0, &initACMP);

    if (init)
    {
        LESENSE_Init(&initLESENSE, true);
    }

    LESENSE_ScanStop();
    while (LESENSE_STATUS_SCANACTIVE & LESENSE_StatusGet())
        ;
    LESENSE_ResultBufferClear();
    LESENSE_ScanFreqSet(0, 0); // N/A as we're using one shot mode
    LESENSE_ClkDivSet(lesenseClkLF, lesenseClkDiv_1);
    LESENSE_ChannelConfig(&chanConfig, 0);
    LESENSE_ChannelConfig(&chanConfig, 1);
    LESENSE_IntEnable(LESENSE_IEN_SCANCOMPLETE);

    NVIC_EnableIRQ(LESENSE_IRQn);

    LESENSE_ScanModeSet(lesenseScanStartOneShot, true);
}

void LESENSE_IRQHandler(void)
{
    LESENSE_IntClear(LESENSE_IEN_SCANCOMPLETE);
    uint32_t chan0 = LESENSE_ScanResultDataBufferGet(0);
    uint32_t chan1 = LESENSE_ScanResultDataBufferGet(1);
    SEGGER_RTT_printf(0, "HERE %u %u\n", chan0, chan1);
}

void disable_le_capsense()
{
    CMU_ClockEnable(cmuClock_ACMP0, false);
    CMU_ClockEnable(cmuClock_LESENSE, false);

    CMU_ClockDivSet(cmuClock_LESENSE, cmuClkDiv_1);

    GPIO_PinModeSet(gpioPortC, 0, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortC, 1, gpioModeInputPull, 0);
}