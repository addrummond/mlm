#include <capsense.h>
#include <em_acmp.h>
#include <em_cmu.h>

uint32_t touch_counts[4];
uint32_t touch_acmp;
uint32_t touch_chan;
uint32_t touch_index;

bool touch_on;

// TODO
// We may want to determine these values at startup rather than
// hard-coding them.
// These values assume a 10ms count window for each pad.
static uint32_t notouch_touch_counts[] = { 1030, 836, 951, 959 };

// Capsense pins are PC0 (S1), PC1 (S3), PC14 (S2), PC15 (S4)
void setup_capsense()
{
    ACMP_CapsenseInit_TypeDef capsenseInit = ACMP_CAPSENSE_INIT_DEFAULT;
    CMU_ClockEnable(cmuClock_ACMP0, true);
    CMU_ClockEnable(cmuClock_ACMP1, true);
    ACMP_CapsenseInit(ACMP0, &capsenseInit);
    ACMP_CapsenseInit(ACMP1, &capsenseInit);

    ACMP_CapsenseChannelSet(ACMP0, acmpChannel0);
    ACMP_CapsenseChannelSet(ACMP1, acmpChannel6);

    while (!(ACMP0->STATUS & ACMP_STATUS_ACMPACT) || !(ACMP1->STATUS & ACMP_STATUS_ACMPACT))
        ;

    ACMP_IntEnable(ACMP0, ACMP_IEN_EDGE);
    ACMP0->CTRL |= ACMP_CTRL_IRISE_ENABLED;

    NVIC_ClearPendingIRQ(ACMP0_IRQn);
    NVIC_EnableIRQ(ACMP0_IRQn);

    ACMP_Enable(ACMP0);
    ACMP_Enable(ACMP1);

    touch_on = true;
}

void disable_capsense()
{
    // TODO
}

void cycle_capsense()
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

void clear_capcounts()
{
    touch_counts[0] = 0;
    touch_counts[1] = 0;
    touch_counts[2] = 0;
    touch_counts[3] = 0;
}

static const uint32_t NOTOUCH_THRESHOLD = 333;
static const int BIAS = 7;

int touch_position_100()
{
    bool notouch = true;
    for (int i = 0; i < 4; ++i) {
        if (touch_counts[i] < NOTOUCH_THRESHOLD)
            notouch = false;
    }

    if (notouch)
        return NO_TOUCH_DETECTED;
    
    // The channels in left-to-right order
    int32_t ltr_chans[] = { touch_counts[0], touch_counts[2], touch_counts[1], touch_counts[3] };

    int32_t min = ltr_chans[0];
    int32_t mini = 0;
    for (int i = 1; i < 4; ++i) {
        if (ltr_chans[i] < min) {
            min = ltr_chans[i];
            mini = i;
        }
    }

    switch (mini) {
        case 0: { 
            return (int)((66 * ltr_chans[0]) / (ltr_chans[0] + ltr_chans[1])) - 100;
        };
        case 1: {
            if (ltr_chans[0] < ltr_chans[2])
                return (int)((66 * ltr_chans[0]) / (ltr_chans[0] + ltr_chans[1])) - 100;
            else
                return (int)(66 + (66 * ltr_chans[1]) / (ltr_chans[1] + ltr_chans[2])) - 100;
        };
        case 2: {
            if (ltr_chans[1] < ltr_chans[3])
                return (int)(66 + ((66 * ltr_chans[1]) / (ltr_chans[1] + ltr_chans[2]))) - 100;
            else
                return (int)(132 + ((66 * ltr_chans[2]) / (ltr_chans[2] + ltr_chans[3]))) - 100;
        };
        case 3: {
            return (int)(132 + ((66 * ltr_chans[2]) / (ltr_chans[2] + ltr_chans[3]))) - 100;
        };
    }
}

void ACMP0_IRQHandler(void) {
	// Clear interrupt flag
  	ACMP0->IFC = ACMP_IFC_EDGE;
	ACMP1->IFC = ACMP_IFC_EDGE;

    if (touch_on) {
        ++touch_counts[touch_index];
    }
}