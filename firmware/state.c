#include <em_msc.h>
#include <iso.h>
#include <rtt.h>
#include <state.h>
#include <stdbool.h>
#include <stdint.h>
#include <units.h>

state g_state;

void set_state_to_default()
{
    g_state.mode = MODE_JUST_WOKEN;
    g_state.id = 0;
    g_state.last_reading.chan0 = 0;
    g_state.last_reading.chan1 = 0;
    g_state.last_reading_itime = 0;
    g_state.last_reading_gain = 0;
    g_state.last_reading_ev = 0;
    g_state.led_brightness_ev_ref = 5;
    g_state.iso_dial_pos = ISO_100_DIAL_POS;
    g_state.iso_third = 0;
    g_state.compensation = 0;
}

bool fresh_reading_is_saved()
{
    return g_state.last_reading_itime != 0 &&
           ((g_state.last_reading_flags & LAST_READING_FLAGS_FRESH) != 0);
}
