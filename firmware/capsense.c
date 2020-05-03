#include <capsense.h>
#include <em_acmp.h>
#include <em_cmu.h>
#include <em_emu.h>
#include <em_gpio.h>
#include <em_lesense.h>
#include <em_wdog.h>
#include <em_pcnt.h>
#include <em_prs.h>
#include <em_rtc.h>
#include <leds.h>
#include <rtc.h>
#include <rtt.h>
#include <stdbool.h>
#include <time.h>

// https://www.silabs.com/content/usergenerated/asi/cloud/attachments/siliconlabs/en/community/mcu/32-bit/knowledge-base/jcr:content/content/primary/blog/low_power_capacitive-SbaF/capsense_rtc.c

static uint32_t touch_chan;
static uint32_t touch_acmp;

static uint32_t old_count;

uint32_t calibration_values[3] __attribute__((section (".persistent")));
uint32_t le_calibration_center_pad_value __attribute__((section (".persistent")));

static const PCNT_Init_TypeDef initPCNT =
{
    .mode        = pcntModeExtSingle, // External, single mode.
    .counter     = 0,                 // Counter value has been initialized to 0.
    .top         = 0xFFFF,            // Counter top value.
    .negEdge     = false,             // Use positive edge.
    .countDown   = false,             // Up-counting.
    .filter      = false,             // Filter disabled.
    .hyst        = false,             // Hysteresis disabled.
    .s1CntDir    = false,             // Counter direction is given by S1.
    .cntEvent    = pcntCntEventBoth,  // Regular counter counts up on upcount events.
    .auxCntEvent = pcntCntEventNone,  // Auxiliary counter doesn't respond to events.
    .s0PRS       = pcntPRSCh0,        // PRS channel 0 selected as S0IN.
    .s1PRS       = pcntPRSCh1         // PRS channel 1 selected as S1IN.
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
    remove_rtc_interrupt_handler(recalibrate_le_capsense);

    ACMP0->CTRL &= ~ACMP_CTRL_IRISE_ENABLED;
    ACMP1->CTRL &= ~ACMP_CTRL_IRISE_ENABLED;
    ACMP_Disable(ACMP0);
    ACMP_Disable(ACMP1);
    NVIC_ClearPendingIRQ(ACMP0_IRQn);
    NVIC_DisableIRQ(ACMP0_IRQn);
    CMU_ClockEnable(cmuClock_ACMP0, false);
    CMU_ClockEnable(cmuClock_ACMP1, false);
    CMU_ClockEnable(cmuClock_PRS, false);
    CMU_ClockEnable(cmuClock_PCNT0, false);
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
    setup_capsense();

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
    set_rtc_clock_div(cmuClkDiv_1);
    RTC_Enable(true);

    cycle_capsense();
    get_touch_count(&count, &chan);
    uint32_t end = RTC->CNT + LE_PAD_CLOCK_COUNT;
    while (RTC->CNT < end)
        ;

    RTC_Enable(false);
    set_rtc_clock_div(RTC_CMU_CLK_DIV);
    RTC_Enable(true);

    disable_capsense();

    SEGGER_RTT_printf(0, "Capsense calibration values: %u %u %u\n", calibration_values[0], calibration_values[1], calibration_values[2]);
}

void calibrate_le_capsense()
{
    setup_le_capsense(LE_CAPSENSE_SENSE);
    EMU_EnterEM2(true);
    le_calibration_center_pad_value = lesense_result;
    disable_le_capsense();

    SEGGER_RTT_printf(0, "LE capsense calibration value: %u\n", le_calibration_center_pad_value);
}

static const uint32_t THRESHOLD_NUM = 89;
static const uint32_t THRESHOLD_DENOM = 100;

touch_position get_touch_position(uint32_t chan0, uint32_t chan1, uint32_t chan2)
{
    if (chan0 > calibration_values[0] * 2 || chan1 > calibration_values[1] * 2 || chan2 > calibration_values[2] * 2)
        return NO_TOUCH_DETECTED;

    uint32_t rat0nopress = (calibration_values[0] << 8) / (calibration_values[1] + calibration_values[2]);
    uint32_t rat1nopress = (calibration_values[1] << 8) / (calibration_values[0] + calibration_values[2]);
    uint32_t rat2nopress = (calibration_values[2] << 8) / (calibration_values[0] + calibration_values[1]);

    uint32_t rat0 = (chan0 << 8) / (chan1 + chan2);
    uint32_t rat1 = (chan1 << 8) / (chan0 + chan2);
    uint32_t rat2 = (chan2 << 8) / (chan0 + chan1);

    if (rat0 < (85 * rat0nopress) / 100 && rat1 < (85 * rat1nopress) / 100 && chan2 >= calibration_values[2] * THRESHOLD_NUM / THRESHOLD_DENOM) {
        SEGGER_RTT_printf(0, "LEFT AND RIGHT %u %u %u\n", chan0, chan1, chan2);
        return LEFT_AND_RIGHT_BUTTONS;
    } else if (rat2 < (80 * rat2nopress) / 100) {
        return CENTER_BUTTON;
    } else if (rat0 < (80 * rat0nopress) / 100) {
        return RIGHT_BUTTON;
    } else if (rat1 < (80 * rat1nopress) / 100) {
        return LEFT_BUTTON;
    }

    return NO_TOUCH_DETECTED;
}

bool le_center_pad_is_touched(uint32_t chan2)
{
    return chan2 != 0 && chan2 < le_calibration_center_pad_value * THRESHOLD_NUM / THRESHOLD_DENOM;
}

uint32_t get_touch_count(uint32_t *chan_value, uint32_t *chan)
{
    if (chan != 0)
        *chan = (touch_acmp << 1) | touch_chan;

    uint32_t raw_count = PCNT0->CNT;

    if (chan_value != 0) {
        if (raw_count >= old_count)
            *chan_value = raw_count - old_count;
        else
            *chan_value = (1 << PCNT0_CNT_SIZE) - old_count + raw_count;
    }

    old_count = raw_count;
    return raw_count;
}

static const uint16_t LESENSE_ACMP_VDD_SCALE = 0x37U;

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
    0,                        /* Excitation time is set to 0 excitation clock cycles. */                 \
    LE_PAD_CLOCK_COUNT,       /* Sample delay */                                                         \
    1,                        /* Measure delay */                                                        \
    LESENSE_ACMP_VDD_SCALE,   /* ACMP threshold has been set to LESENSE_ACMP_VDD_SCALE. */               \
    lesenseSampleModeCounter, /* Counter will be used in comparison. */                                  \
    lesenseSetIntLevel,       /* Interrupt is generated if the sensor triggers. */                       \
    0x00,                     /* Counter threshold has been set to 0x0. */                               \
    lesenseCompModeLess       /* Compare mode has been set to trigger interrupt on >= */                 \
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
    1,                        /* Measure delay is set to ___ excitation clock cycles.*/                  \
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
    .coreCtrl = {
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

    .timeCtrl = {
        .startDelay = 0U
    },

    .perCtrl = {
        .dacCh0Data     = lesenseDACIfData,
        .dacCh0ConvMode = lesenseDACConvModeDisable,
        .dacCh0OutMode  = lesenseDACOutModeDisable,
        .dacCh1Data     = lesenseDACIfData,
        .dacCh1ConvMode = lesenseDACConvModeDisable,
        .dacCh1OutMode  = lesenseDACOutModeDisable,
        .dacPresc       = 0U,
        .dacRef         = lesenseDACRefBandGap,
        .acmp0Mode      = lesenseACMPModeDisable,
        .acmp1Mode      = lesenseACMPModeMuxThres,
        .warmupMode     = lesenseWarmupModeNormal
    },

    .decCtrl = {
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

static uint32_t le_center_pad_count_to_threshold(uint32_t count)
{
    return count * 85 / 100;
}

void setup_le_capsense(le_capsense_mode mode)
{
    le_mode = mode;

    CMU_ClockEnable(cmuClock_ACMP1, true);
    CMU_ClockEnable(cmuClock_LESENSE, true);

    CMU_ClockDivSet(cmuClock_LESENSE, cmuClkDiv_1);

    GPIO_DriveModeSet(gpioPortC, gpioDriveModeStandard);
    GPIO_PinModeSet(gpioPortC, 14, gpioModeDisabled, 0);

    static const ACMP_CapsenseInit_TypeDef initACMP = {
        .fullBias                 = true,
        .halfBias                 = true,
        .biasProg                 = 0x7,
        .warmTime                 = acmpWarmTime512,
        .hysteresisLevel          = acmpHysteresisLevel5,
        .resistor                 = acmpResistor0,
        .lowPowerReferenceEnabled = false,
        .vddLevel                 = 0x30,
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
        LESENSE_ChannelThresSet(14, LESENSE_ACMP_VDD_SCALE, le_center_pad_count_to_threshold(le_calibration_center_pad_value));
    }

    if (mode != LE_CAPSENSE_SENSE) {
        // Periodically wake up to recalibrate.
        SEGGER_RTT_printf(0, "Setting recalibration wakeup timer.\n");
        static const RTC_Init_TypeDef rtc_init = {
            .enable   = true,
            .debugRun = false,
            .comp0Top = true
        };
        RTC_Init(&rtc_init);
        RTC_IntEnable(RTC_IEN_COMP0);
        NVIC_EnableIRQ(RTC_IRQn);
        set_rtc_clock_div(cmuClkDiv_32768);
        RTC_CompareSet(0, RTC->CNT + LE_CAPSENSE_CALIBRATION_INTERVAL_SECONDS);
        RTC_IntClear(RTC_IFC_COMP0);
    }
}

uint32_t lesense_result;

static volatile bool irq_handler_triggered;

bool check_lesense_irq_handler()
{
    bool v = irq_handler_triggered;
    irq_handler_triggered = false;
    return v;
}

void LESENSE_IRQHandler(void)
{
    LESENSE_IntClear(LESENSE_IEN_SCANCOMPLETE);
    LESENSE_IntClear(LESENSE_IF_CH14);

    lesense_result = LESENSE_ScanResultDataGet();

    // Stop additional interrupts screwing things up by setting threshold to zero.
    if (le_mode == LE_CAPSENSE_SLEEP)
        LESENSE_ChannelThresSet(14, LESENSE_ACMP_VDD_SCALE, 0);

    irq_handler_triggered = true;
}

static uint32_t get_max_value(volatile uint32_t* A, int N){
    int i;
    uint32_t max = 0;

    for (i = 0; i < N; i++) {
        if (max < A[i])
            max = A[i];
    }

    return max;
}

#define NUMBER_OF_LE_CALIBRATION_VALUES 5
#define NUM_LESENSE_CHANNELS            1

// Adapted from example code in AN0028.
void recalibrate_le_capsense()
{
    SEGGER_RTT_printf(0, "Recalibrating capsense.\n");

    static uint32_t channels_used_mask;
    static const int num_channels_used = 1;
    static volatile uint32_t calibration_value[NUM_LESENSE_CHANNELS][NUMBER_OF_LE_CALIBRATION_VALUES];
    static volatile uint32_t channel_max_value[NUM_LESENSE_CHANNELS];

    int i, k;
    uint16_t nominal_count;
    static uint8_t calibration_value_index = 0;

    // Wait for current scan to finish
    while (LESENSE->STATUS & LESENSE_STATUS_SCANACTIVE)
        ;

    // Get position for first channel data in count buffer from lesense write pointer
    k = ((LESENSE->PTR & _LESENSE_PTR_WR_MASK) >> _LESENSE_PTR_WR_SHIFT);

    // Handle circular buffer wraparound
    if (k >= num_channels_used)
        k = k - num_channels_used;
    else
        k = k - num_channels_used + NUM_LESENSE_CHANNELS;

    // Fill calibration values array with buffer values
    for (i = 0; i < NUM_LESENSE_CHANNELS; i++) {
        if ((channels_used_mask >> i) & 0x1)
            calibration_value[i][calibration_value_index] = LESENSE_ScanResultDataBufferGet(k++);
    }

    // Wrap around calibration_values_index
    calibration_value_index++;
    if (calibration_value_index >= NUMBER_OF_LE_CALIBRATION_VALUES)
        calibration_value_index = 0;

    // Calculate max value for each channel and set threshold
    for (i = 0; i < NUM_LESENSE_CHANNELS; i++) {
        if ((channels_used_mask >> i) & 0x1) {
            channel_max_value[i] = get_max_value(calibration_value[i], NUMBER_OF_LE_CALIBRATION_VALUES);

            nominal_count = channel_max_value[i];
            LESENSE_ChannelThresSet(14, LESENSE_ACMP_VDD_SCALE, le_center_pad_count_to_threshold(nominal_count));
        }
    }
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

static const int MAX_MISS_COUNT = 2;

press get_pad_press(touch_position touch_pos)
{
    int rtc_freq = get_rtc_freq();
    int long_press_ticks = LONG_PRESS_MS * rtc_freq / 1000;
    int touch_count_ticks = PAD_COUNT_MS * rtc_freq / 1000;

    press p;

    RTC->CTRL |= RTC_CTRL_EN;

    get_touch_count(0, 0); // clear any nonsense value

    int miss_count = 0;
    uint32_t chans[] = { 0, 0, 0 };
    int rtc_base = RTC->CNT;
    for (;;) {
        uint32_t base = RTC->CNT;
        while (RTC->CNT - base < touch_count_ticks)
            ;

        uint32_t count, chan;
        get_touch_count(&count, &chan);
        chans[chan] = count;

        if (chans[0] != 0 && chans[1] != 0 && chans[2] != 0) {
            touch_position tp = get_touch_position(chans[0], chans[1], chans[2]);
            if (tp != touch_pos) {
                ++miss_count;
                if (miss_count > MAX_MISS_COUNT) {
                    p = PRESS_TAP;
                    break;
                }
            }
        }

        if (RTC->CNT - rtc_base >= long_press_ticks) {
            p = PRESS_HOLD;
            break;
        }

        cycle_capsense();
    }

    RTC->CTRL &= ~RTC_CTRL_EN;
    disable_capsense();

    return p;
}

press get_pad_press_while_leds_on(touch_position touch_pos)
{
    const uint32_t long_press_ticks = LONG_PRESS_MS * RTC_RAW_FREQ / 1000;
    const uint32_t touch_count_ticks = PAD_COUNT_MS * RTC_RAW_FREQ / 1000;

    press p;

    get_touch_count(0, 0); // clear any nonsense value

    int miss_count = 0;
    uint32_t chans[] = { 0, 0, 0 };
    uint32_t base_touch_count = leds_on_for_cycles;
    for (;;) {
        uint32_t base = leds_on_for_cycles;
        while (leds_on_for_cycles - base < touch_count_ticks)
            ;

        uint32_t count, chan;
        get_touch_count(&count, &chan);
        chans[chan] = count;

        if (chans[0] != 0 && chans[1] != 0 && chans[2] != 0) {
            touch_position tp = get_touch_position(chans[0], chans[1], chans[2]);
            if (tp != touch_pos) {
                ++miss_count;
                if (miss_count > MAX_MISS_COUNT) {
                    p = PRESS_TAP;
                    break;
                }
            }
        }

        if (leds_on_for_cycles - base_touch_count >= long_press_ticks) {
            p = PRESS_HOLD;
            break;
        }

        cycle_capsense();
    }

    return p;
}
