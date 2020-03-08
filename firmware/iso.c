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

const char *iso_to_string(int32_t iso)
{
    static const char *iso_strings[] = {
        "10",
        "12",
        "16",
        "20",
        "25",
        "32",
        "40",
        "50",
        "64",
        "80",
        "100",
        "125",
        "160",
        "200",
        "250",
        "320",
        "400",
        "500",
        "640",
        "800",
        "1000",
        "1250",
        "1600",
        "2000",
        "2500",
        "3200",
        "4000",
        "5000",
        "6400",
        "8000"
    };

    if (iso < ISO_12 - 1)
        return "[?ISO-]";
    if (iso - ISO_12 + 1 >= sizeof(iso_strings)/sizeof(iso_strings[0]))
        return "[?ISO+]";
    
    return iso_strings[iso - ISO_12 + 1];
}