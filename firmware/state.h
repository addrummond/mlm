#ifndef STATE_H
#define STATE_H

#include <stdbool.h>
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
    // setting the iso (or exposure if exposure priority)
    MODE_SETTING_ISO,
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
    int32_t last_ap;
    int32_t iso_dial_pos;
    int32_t iso_third; // 0, -1 or 1
    int32_t compensation; // in units of 1/3 stop
    int32_t led_brightness_ev_ref;
    int32_t deep_sleep_counter;
    bool watchdog_wakeup;
    bool bat_known_healthy;
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
                      4 /* led_brightness_ev_ref */ \
                     )

extern state g_state;

void set_state_to_default(mode initial_mode);
bool fresh_reading_is_saved(void);

#endif
