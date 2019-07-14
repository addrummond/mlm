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
} mode;

#define LAST_READING_FLAGS_FRESH 1

typedef struct state {
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
#define STATE_NBYTES (4 /* mode */ + \
                      4 /* last_reading */ + \
                      4 /* last_reading_itime */ + \
                      4 /* last_reading_ev */ + \
                      4 /* last_reading_flags */ + \
                      4 /* last_reading_iso */ + \
                      4 /* last_reading_compensation */ \
                     )

extern state g_state;

// Address of the user data page in flash memory.
#define USER_DATA_PAGE_ADDR ((uint32_t *)0x0FE00000)

#define USER_DATA_PAGE_NBYTES 512

void write_state_to_flash(void);
void read_state_from_flash(void);
void set_state_to_default(void);
bool fresh_reading_is_saved(void);

#endif