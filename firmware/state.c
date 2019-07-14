#include <em_msc.h>
#include <state.h>
#include <stdbool.h>
#include <stdint.h>

state g_state;

__attribute__((section(".ram")))
static uint32_t *find_state_in_user_page() {
    // Page is 512 bytes and state is ~28 bytes, so a linear search has
    // acceptable performance.
    uint32_t *addr;
    for (addr = USER_DATA_PAGE_ADDR; addr < USER_DATA_PAGE_ADDR + (USER_DATA_PAGE_NBYTES/sizeof(uint32_t)); addr += STATE_NBYTES/sizeof(uint32_t)) {
        if (*addr != 0xFFFFFFFF)
            break;
    }

    if (addr >= USER_DATA_PAGE_ADDR + (USER_DATA_PAGE_NBYTES/sizeof(uint32_t)))
        return 0;

    return addr;
}

__attribute__((section(".ram")))
static uint32_t *next_address_of_state()
{
    uint32_t *a = find_state_in_user_page();
    if (a == 0)
        return USER_DATA_PAGE_ADDR;
    if (a + (STATE_NBYTES/sizeof(uint32_t)) > a + (USER_DATA_PAGE_NBYTES/sizeof(uint32_t)))
        return USER_DATA_PAGE_ADDR;
    return a + (USER_DATA_PAGE_NBYTES/sizeof(uint32_t));
}

__attribute__((section(".ram")))
void write_state_to_flash()
{
    uint32_t *a = next_address_of_state();

    MSC_Init();
    MSC_ErasePage(USER_DATA_PAGE_ADDR); // pages are 512 bytes, so one page is more than enough

    // Mark reading as fresh before saving it so that it will be displayed on
    // next wakeup.
    g_state.last_reading_flags |= LAST_READING_FLAGS_FRESH;

    int off = 0;
    int32_t d = g_state.mode;
    MSC_WriteWord(a + off, &d, sizeof(int32_t));
    off += sizeof(int32_t);
    d = g_state.last_reading.chan0 | (g_state.last_reading.chan0 << 16);
    MSC_WriteWord(a + off, &d, sizeof(int32_t));
    off += sizeof(int32_t);
    d = g_state.last_reading_itime;
    MSC_WriteWord(a + off, &d, sizeof(int32_t));
    off += sizeof(int32_t);
    d = g_state.last_reading_gain;
    MSC_WriteWord(a + off, &d, sizeof(int32_t));
    off += sizeof(int32_t);
    d = g_state.last_reading_ev;
    MSC_WriteWord(a + off, &d, sizeof(int32_t));
    off += sizeof(int32_t);
    d = g_state.last_reading_flags;
    MSC_WriteWord(a + off, &d, sizeof(int32_t));
    off += sizeof(int32_t);
    d = g_state.iso;
    MSC_WriteWord(a + off, &d, sizeof(int32_t));
    off += sizeof(int32_t);
    d = g_state.compensation;
    MSC_WriteWord(a + off, &d, sizeof(int32_t));
}

__attribute__((section(".ram")))
void set_state_to_default()
{
    g_state.last_reading.chan0 = 0;
    g_state.last_reading.chan1 = 0;
    g_state.last_reading_itime = 0;
    g_state.last_reading_gain = 0;
    g_state.last_reading_ev = 0;
    g_state.iso = 12; // ISO 100
    g_state.compensation = 0;
}

void read_state_from_flash()
{
    uint32_t *a = find_state_in_user_page();
    if (a == 0) {
        set_state_to_default();
    } else {
        // Note that EFM32 is little endian.
        g_state.mode = *((mode *)(a+1));
        g_state.last_reading.chan0 = *((int16_t *)(a+2));
        g_state.last_reading.chan1 = *(((int16_t *)(a+2)) + 1);
        g_state.last_reading_itime = *((int32_t *)(a+3));
        g_state.last_reading_gain = *((int32_t *)(a+4));
        g_state.last_reading_flags = *((int32_t *)(a+5));
    }

    // Whatever state we were in when we went to sleep, we're now in JUST_WOKEN
    g_state.mode = MODE_JUST_WOKEN;
}

bool fresh_reading_is_saved()
{
    return g_state.last_reading_itime != 0 &&
           (g_state.last_reading_flags & LAST_READING_FLAGS_FRESH != 0);
}