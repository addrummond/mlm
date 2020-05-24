#include <dwt.h>
#include <em_cmu.h>
#include <em_dbg.h>
#include <em_emu.h>
#include <init.h>
#include <myemu.h>

static void before_sleep()
{
    *DWT_CTRL &= ~1U;
    if (! DBG_Connected()) {
        CoreDebug->DHCSR &= ~CoreDebug_DHCSR_C_DEBUGEN_Msk;
        CoreDebug->DEMCR &= ~CoreDebug_DEMCR_TRCENA_Msk;
    }
    gpio_pins_to_initial_states(false);
}

static void on_awake()
{
    // So that we can use CYCCNT.
    CoreDebug->DHCSR |= CoreDebug_DHCSR_C_DEBUGEN_Msk;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
}

void my_emu_enter_em1()
{
    EMU_EnterEM1();
}

void my_emu_enter_em2(bool b)
{
    before_sleep();
    EMU_EnterEM2(b);
    on_awake();
}

void my_emu_enter_em3(bool b)
{
    before_sleep();
    EMU_EnterEM3(b);
    // EM3 ends following reset, so we never get here.
}