#ifndef STATE_H
#define STATE_H

#include <stdint.h>
#include <sensor.h>

// sleep state doesn't need to be encoded here, as we know we're not sleeping
// any time our code is running.
typedef enum mode {
    MODE_JUST_WOKEN,
    // the button is being held down, waiting for reading to complete
    MODE_DOING_READING,
    // the device is awake, not doing anything.
    MODE_AWAKE_AT_REST,
    // a reading is being shown and can be manipulated using the slider
    MODE_DISPLAY_READING,
    // the device is snoozing after a reading display has timed out
    MODE_SNOOZE
} mode;

#define LAST_READING_FLAGS_FRESH 1

typedef struct state {
    uint32_t id; // used for wear leveling calculations
    mode mode;
    sensor_reading last_reading;
    int32_t last_reading_itime;
    int32_t last_reading_gain;
    int32_t last_reading_ev;
    int32_t last_reading_flags;
    int32_t iso; // 0 = ISO 6, 1 = ISO 8, and so on (increments of 1/3 stop).
    int32_t compensation; // in units of 1/3 stop
} state;

// Don't trust sizeof here because of possible padding.
#define STATE_NBYTES (4 /* id */ + \
                      4 /* mode */ + \
                      4 /* last_reading */ + \
                      4 /* last_reading_itime */ + \
                      4 /* last_reading_ev */ + \
                      4 /* last_reading_flags */ + \
                      4 /* last_reading_iso */ + \
                      4 /* compensation */ \
                     )

extern state g_state;

// Address of the user data page in flash memory.
#define USER_DATA_PAGE_ADDR ((uint32_t *)0x0FE00000)

#define PAGE_NBYTES 512

// There's only one page reserved for user data, but as we're not using anywhere
// near all of the 32kb of flash available for code, we can afford to use a few
// extra pages to help with wear leveling.
#define N_CHEEKY_PAGES 4

#define N_DATA_PAGES (N_CHEEKY_PAGES+1)

#define FIRST_STATE_PAGE_ADDR (USER_DATA_PAGE_ADDR - N_CHEEKY_PAGES*(PAGE_NBYTES/sizeof(uint32_t)))

// *Note on erase cycles*
//
// The EFM32 part we're using claims a minimum of 10K erase cycles for the
// flash. We erase all data pages every
//     (N_DATA_PAGES * (PAGE_NBYTES / STATE_NBYTES)) = 80
// times the state is written. Thus, we should be able to write the state
// at least 800,000 times before the flash starts to fail. This should be
// more than enough. Supposing that the state is saved 100 times a day
// every day, that's ~22 years of life.

void write_state_to_flash(void);
void read_state_from_flash(void);
void set_state_to_default(void);
bool fresh_reading_is_saved(void);
void erase_state_pages(void);

#endif
