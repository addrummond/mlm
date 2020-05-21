#ifndef ISO_H
#define ISO_H

#include <stdbool.h>
#include <stdint.h>

// The lowest ISO setting available is ISO 10 (which is 1/3rd stop below
// ISO 12, the lowest ISO number on the inner dial). However, for our
// units, we starts from 0 = ISO 0.8 (the lowest ISO number).

#define ISO_0_PT_8           0
#define ISO_3                6
#define ISO_12               12 // just a coincidence that ISO 12 == 12
#define ISO_64               19
#define ISO_100              21
#define ISO_1600             33
#define ISO_MIN              (ISO_12 - 1)
#define ISO_MAX              (ISO_12 + (7*3) + (2*3) + 1)
#define ISO_100_DIAL_POS     7
#define ISO_N_DIAL_POSITIONS 24

int32_t iso_dial_pos_and_third_to_iso(int pos, int third);
bool iso_dial_pos_can_go_third_below(int pos);
bool iso_dial_pos_can_go_third_above(int pos);
int iso_dial_pos_to_led_n(int pos);


#endif