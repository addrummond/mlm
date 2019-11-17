#include <capsense.h>
#include <em_acmp.h>
#include <em_cmu.h>
#include <em_gpio.h>
#include <em_lesense.h>
#include <em_pcnt.h>
#include <em_prs.h>
#include <rtt.h>
#include <stdbool.h>

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
    CMU_ClockEnable(cmuClock_CORELE, true);
    CMU_ClockEnable(cmuClock_ACMP0, true);
    CMU_ClockEnable(cmuClock_PRS, true);
    CMU_ClockEnable(cmuClock_PCNT0, true);
    ACMP_CapsenseInit(ACMP0, &capsenseInit);

    while (!(ACMP0->STATUS & ACMP_STATUS_ACMPACT))
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
}

void disable_capsense()
{
    ACMP0->CTRL &= ~ACMP_CTRL_IRISE_ENABLED;
    NVIC_ClearPendingIRQ(ACMP0_IRQn);
    NVIC_DisableIRQ(ACMP0_IRQn);
    CMU_ClockEnable(cmuClock_CORELE, false);
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

#define LESENSE_ACMP_VDD_SCALE    0x37U

#define LESENSE_CAPSENSE_CH_CONF_SLEEP                                                                   \
  {                                                                                                      \
    true,                     /* Enable scan channel. */                                                 \
    true,                     /* Enable the assigned pin on scan channel. */                             \
    true,                     /* Enable interrupts on channel. */                                        \
    lesenseChPinExDis,        /* GPIO pin is disabled during the excitation period. */                   \
    lesenseChPinIdleDis,      /* GPIO pin is disabled during the idle period. */                         \
    false,                    /* Don't use alternate excitation pins for excitation. */                  \
    false,                    /* Disabled to shift results from this channel to the decoder register. */ \
    false,                    /* Disabled to invert the scan result bit. */                              \
    true,                     /* Enabled to store counter value in the result buffer. */                 \
    lesenseClkLF,             /* Use the LF clock for excitation timing. */                              \
    lesenseClkLF,             /* Use the LF clock for sample timing. */                                  \
    0x00U,                    /* Excitation time is set to 0 excitation clock cycles. */                 \
    0x05U,                    /* Sample delay is set to 1(+1) sample clock cycles. */                    \
    0x01U,                    /* Measure delay is set to 0 excitation clock cycles.*/                    \
    LESENSE_ACMP_VDD_SCALE,   /* ACMP threshold has been set to LESENSE_ACMP_VDD_SCALE. */               \
    lesenseSampleModeCounter, /* Counter will be used in comparison. */                                  \
    lesenseSetIntLevel,       /* Interrupt is generated if the sensor triggers. */                       \
    0x0EU,                    /* Counter threshold has been set to 0x0E. */                              \
    lesenseCompModeLess       /* Compare mode has been set to trigger interrupt on "less". */            \
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
    CMU_ClockEnable(cmuClock_CORELE, true);
    CMU_ClockEnable(cmuClock_LESENSE, true);

    CMU_ClockDivSet(cmuClock_LESENSE, cmuClkDiv_1);

    GPIO_DriveModeSet(gpioPortC, gpioDriveModeStandard);
    GPIO_PinModeSet(gpioPortC, 0, gpioModeDisabled, 0);
    GPIO_PinModeSet(gpioPortC, 1, gpioModeDisabled, 0);

    //ACMP_CapsenseInit_TypeDef initACMP = ACMP_CAPSENSE_INIT_DEFAULT;
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
    LESENSE_ScanFreqSet(0U, 4U); // 4 Hz
    LESENSE_ClkDivSet(lesenseClkLF, lesenseClkDiv_1);
    LESENSE_ChannelConfig(&chanConfig, 0);
    LESENSE_ChannelConfig(&chanConfig, 1);
    LESENSE_IntDisable(LESENSE_IEN_SCANCOMPLETE);

    NVIC_EnableIRQ(LESENSE_IRQn);

    LESENSE_ScanStart();

    if (init) {
        SEGGER_RTT_printf(0, "WAITING!\n");
        while (!(LESENSE->STATUS & LESENSE_STATUS_BUFHALFFULL)) ;
        SEGGER_RTT_printf(0, "DONE!\n");

        for (uint32_t i = 0U; i < 2; i++)
        {
            uint32_t v = LESENSE_ScanResultDataBufferGet(i);
            SEGGER_RTT_printf(0, "V: %u\n", v);
        }

        LESENSE_ChannelThresSet(0/*pin 0 */, LESENSE_ACMP_VDD_SCALE, 15/* TODO TODO CONST VAL*/);
        LESENSE_ChannelThresSet(1/*pin 0 */, LESENSE_ACMP_VDD_SCALE, 15/* TODO TODO CONST VAL*/);
    }
}

void LESENSE_IRQHandler(void)
{

    SEGGER_RTT_printf(0, "INT\n");
    if (LESENSE_IF_CH0 & LESENSE_IntGetEnabled()) {
        LESENSE_IntClear(LESENSE_IF_CH0);

        /* Clear flags. */
        uint32_t count = LESENSE_ScanResultDataGet();
        uint32_t chan0 = LESENSE_ScanResultDataBufferGet(0);

        SEGGER_RTT_printf(0, "Interrupt %u (chan 0 = %u)\n", count, chan0);
    }
    if (LESENSE_IF_CH1 & LESENSE_IntGetEnabled()) {
        LESENSE_IntClear(LESENSE_IF_CH1);

        /* Clear flags. */
        uint32_t count = LESENSE_ScanResultDataGet();
        uint32_t chan1 = LESENSE_ScanResultDataBufferGet(1);

        SEGGER_RTT_printf(0, "Interrupt %u (chan 0 = %u)\n", count, chan1);
    }
}

void disable_le_capsense()
{
    CMU_ClockEnable(cmuClock_ACMP0, false);
    CMU_ClockEnable(cmuClock_CORELE, false);
    CMU_ClockEnable(cmuClock_LESENSE, false);

    CMU_ClockDivSet(cmuClock_LESENSE, cmuClkDiv_1);

    GPIO_PinModeSet(gpioPortC, 0, gpioModeInputPull, 0);
    GPIO_PinModeSet(gpioPortC, 1, gpioModeInputPull, 0);
}