#include <iso.h>

bool iso_dial_pos_can_go_third_below(int pos)
{
    return (pos >= 0 && pos <= 4) || pos >= 20;
}

bool iso_dial_pos_can_go_third_above(int pos)
{
    return (pos >= 0 && pos <= 3) || pos >= 19;
}

int32_t iso_dial_pos_and_third_to_iso(int pos, int third)
{
    if (pos >= 0 && pos <= 4)
        return 3*pos + ISO_3 + third;
    if (pos >= 5 && pos <= 18)
        return pos - 5 + ISO_64;
    if (pos >= 19)
        return 3*(pos - 19) + ISO_1600 + third;
}

int iso_dial_pos_to_led_n(int pos)
{
    // TODO: No doubt possible to simplify this
    return (ISO_N_DIAL_POSITIONS - ((pos + 18) % ISO_N_DIAL_POSITIONS)) % ISO_N_DIAL_POSITIONS;
}
