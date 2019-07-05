#include <em_msc.h>
#include <state.h>
#include <stdbool.h>

state g_state;

void write_state_to_flash()
{
    MSC_Init();
    MSC_ErasePage(USER_DATA_PAGE_ADDR); // pages are 512 bytes, so one page is more than enough

    ++g_state.id;

    // Mark reading as fresh before saving it so that it will be displayed on
    // next wakeup.
    g_state.last_reading_flags |= LAST_READING_FLAGS_FRESH;

    int off = 0;
    int32_t d = g_state.id;
    MSC_WriteWord(USER_DATA_PAGE_ADDR + off, &d, sizeof(int32_t));
    off += sizeof(int32_t);
    d = g_state.mode;
    MSC_WriteWord(USER_DATA_PAGE_ADDR + off, &d, sizeof(int32_t));
    off += sizeof(int32_t);
    d = g_state.last_reading.chan0 | (g_state.last_reading.chan0 << 16);
    MSC_WriteWord(USER_DATA_PAGE_ADDR + off, &d, sizeof(int32_t));
    off += sizeof(int32_t);
    d = g_state.last_reading_itime;
    MSC_WriteWord(USER_DATA_PAGE_ADDR + off, &d, sizeof(int32_t));
    off += sizeof(int32_t);
    d = g_state.last_reading_gain;
    MSC_WriteWord(USER_DATA_PAGE_ADDR + off, &d, sizeof(int32_t));
    off += sizeof(int32_t);
    d = g_state.last_reading_ev;
    MSC_WriteWord(USER_DATA_PAGE_ADDR + off, &d, sizeof(int32_t));
    off += sizeof(int32_t);
    d = g_state.last_reading_flags;
    MSC_WriteWord(USER_DATA_PAGE_ADDR + off, &d, sizeof(int32_t));
}

void read_state_from_flash()
{
    // Note that EFM32 is little endian.

    g_state.id = *((int32_t *)USER_DATA_PAGE_ADDR);
    g_state.mode = *((mode *)(USER_DATA_PAGE_ADDR+1));
    g_state.last_reading.chan0 = *((int16_t *)(USER_DATA_PAGE_ADDR+2));
    g_state.last_reading.chan1 = *((int16_t *)(USER_DATA_PAGE_ADDR+2) + 1);
    g_state.last_reading_itime = *((int32_t *)(USER_DATA_PAGE_ADDR+3));
    g_state.last_reading_gain = *((int32_t *)(USER_DATA_PAGE_ADDR+4));
    g_state.last_reading_flags = *((int32_t *)(USER_DATA_PAGE_ADDR+5));

    if (g_state.id == 0xFFFFFFFF || g_state.id == 0) {
        // The page is erased/empty or uninitialized
        g_state.id = 1;
        g_state.last_reading.chan0 = 0;
        g_state.last_reading.chan1 = 0;
        g_state.last_reading_itime = 0;
        g_state.last_reading_gain = 0;
        g_state.last_reading_ev = 0;
    }

    // Whatever state we were in when we went to sleep, we're now in JUST_WOKEN
    g_state.mode = MODE_JUST_WOKEN;
}

bool fresh_reading_is_saved()
{
    return g_state.last_reading_itime != 0 &&
           (g_state.last_reading_flags & LAST_READING_FLAGS_FRESH != 0);
}