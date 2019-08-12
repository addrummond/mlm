#include <em_msc.h>
#include <state.h>
#include <stdbool.h>
#include <stdint.h>

state g_state;

static const uint32_t LOCS_PER_PAGE = PAGE_NBYTES/STATE_NBYTES;
static const uint32_t NUM_POSSIBLE_LOCS = N_DATA_PAGES * LOCS_PER_PAGE;

__attribute__((section(".ram")))
static uint32_t *find_state()
{
    // Find the current state via a binary search.
    
    uint32_t *addr = 0;

    for (uint32_t loc = NUM_POSSIBLE_LOCS/2;;) {
        uint32_t pg = loc / LOCS_PER_PAGE;
        uint32_t pgloc = loc % LOCS_PER_PAGE;
        addr = FIRST_STATE_PAGE_ADDR + ((pg * PAGE_NBYTES) + (pgloc * STATE_NBYTES))/sizeof(uint32_t);

        // This is the current stage if
        //     (a) the first four bytes are not 0xFF, and
        //     (b) either
        //             (i)  the first four bytes of the next location are 0xFF, or
        //             (ii) the current location is the last location
        if (*addr != 0xFFFFFFFF) {
            if (loc+1 == NUM_POSSIBLE_LOCS)
                break;
            uint32_t *next_addr = addr + STATE_NBYTES/sizeof(uint32_t);
            if (*next_addr == 0xFFFFFFFF)
                break;
            loc = (loc + NUM_POSSIBLE_LOCS) / 2;
        } else {
            loc /= 2;
        }
    }
    
    return addr;
}

__attribute__((section(".ram")))
static void erase_state_pages()
{
    for (uint32_t i = 0; i < N_DATA_PAGES; ++i) {
        MSC_ErasePage(FIRST_STATE_PAGE_ADDR + (i*PAGE_NBYTES)/sizeof(uint32_t));
    }
}

__attribute__((section(".ram")))
static uint32_t *prepare_next_state_address()
{
    uint32_t *a = find_state();
    uint32_t ua = (uint32_t)a;
    uint32_t loc = ((a / PAGE_NBYTES) * LOCS_PER_PAGE) + ((a % PAGE_NBYTES) / STATE_NBYTES);

    if (a == 0 || loc + 1 == NUM_POSSIBLE_LOCS) {
        // We need to start from the beginning.
        erase_state_pages();
        return FIRST_STATE_PAGE_ADDR;
    }
    
    ++loc;
    return
        FIRST_STATE_PAGE_ADDR +
        ((loc / LOCS_PER_PAGE) * (PAGE_NBYTES / sizeof(uint32_t))) +
        ((loc % LOCS_PER_PAGE) * (STATE_NBYTES / sizeof(uint32_t)));
}

__attribute__((section(".ram")))
void write_state_to_flash()
{
    MSC_Init();
    
    uint32_t *a = prepare_next_state_address();

    // Mark reading as fresh before saving it so that it will be displayed on
    // next wakeup.
    g_state.last_reading_flags |= LAST_READING_FLAGS_FRESH;

    int off = 0;
    uint32_t ud = g_state.id;
    MSC_WriteWord(a + off, &ud, sizeof(uint32_t));
    off += sizeof(uint32_t);
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
    g_state.id = 0;
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
        g_state.id = *a;
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
