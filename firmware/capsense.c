#include <capsense.h>
#include <em_acmp.h>
#include <em_cmu.h>

uint32_t touch_counts[2];
uint32_t touch_chan;
uint32_t touch_index;

bool touch_on;

static int32_t sq(int32_t v)
{
    return v*v;
}

static const uint32_t NOTOUCH_THRESHOLD = 333;

// Capsense pins are PC0 (S2), PC1 (S1)
void setup_capsense()
{
    ACMP_CapsenseInit_TypeDef capsenseInit = ACMP_CAPSENSE_INIT_DEFAULT;
    CMU_ClockEnable(cmuClock_ACMP0, true);
    ACMP_CapsenseInit(ACMP0, &capsenseInit);

    ACMP_CapsenseChannelSet(ACMP0, acmpChannel0);

    while (!(ACMP0->STATUS & ACMP_STATUS_ACMPACT))
        ;

    ACMP_IntEnable(ACMP0, ACMP_IEN_EDGE);
    ACMP0->CTRL |= ACMP_CTRL_IRISE_ENABLED;

    NVIC_ClearPendingIRQ(ACMP0_IRQn);
    NVIC_EnableIRQ(ACMP0_IRQn);

    ACMP_Enable(ACMP0);

    touch_on = true;
}

void disable_capsense()
{
    ACMP0->CTRL &= ~ACMP_CTRL_IRISE_ENABLED;
    NVIC_ClearPendingIRQ(ACMP0_IRQn);
    NVIC_DisableIRQ(ACMP0_IRQn);
    ACMP_IntDisable(ACMP0, ACMP_IEN_EDGE);
    CMU_ClockEnable(cmuClock_ACMP0, false);
    touch_on = false;
}

void cycle_capsense()
{
    if (touch_chan == 0) {
        ACMP_CapsenseChannelSet(ACMP0, acmpChannel1);
        touch_chan = 1;
        touch_index = 1;
    } else {
        ACMP_CapsenseChannelSet(ACMP0, acmpChannel0);
        touch_chan = 0;
        touch_index = 0;
    }
}

void clear_capcounts()
{
    touch_counts[0] = 0;
    touch_counts[1] = 0;
}

void ACMP0_IRQHandler(void) {
	// Clear interrupt flag
  	ACMP0->IFC = ACMP_IFC_EDGE;

    if (touch_on) {
        ++touch_counts[touch_index];
    }
}