#include <iso.h>

bool iso_dial_pos_can_go_third_below(int pos)
{
    return pos == 0 || pos > 21;
}

bool iso_dial_pos_can_go_third_above(int pos)
{
    return pos > 20;
}

int32_t iso_dial_pos_and_third_to_iso(int pos, int third)
{
    if (pos <= 20)
        return pos + ISO_12;
    return ISO_1600 + 3*(pos - 21) + third;
}

int iso_dial_pos_to_led_n(int pos)
{
    // TODO: No doubt possible to simplify this
    return (ISO_N_DIAL_POSITIONS - ((pos + 18) % ISO_N_DIAL_POSITIONS)) % ISO_N_DIAL_POSITIONS;
}
