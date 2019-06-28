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
    // the device is awake, not doing anything
    MODE_AWAKE_AT_REST,
} mode;

typedef struct state {
    int32_t id; // sequential id used to determine location of state in flash storage for wear leveling purposes
    mode mode;
    sensor_reading last_reading;
    int32_t last_reading_itime;
    int32_t last_reading_gain;
} state;

extern state g_state;

// Address of the user data page in flash memory.
#define USER_DATA_PAGE_ADDR ((uint32_t *)0x0FE00000)

void write_state_to_flash();
void read_state_from_flash();

#endif