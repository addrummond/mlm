#include <stddef.h>
#include <units.h>

static int32_t log_base2(uint32_t x)
{
    // This implementation is based on Clay. S. Turner's fast binary logarithm
    // algorithm.

    int32_t b = 1U << (EV_BPS - 1);
    int32_t y = 0;

    if (x == 0) {
        return INT32_MIN; // represents negative infinity
    }

    while (x < 1U << EV_BPS) {
        x <<= 1;
        y -= 1U << EV_BPS;
    }

    while (x >= 2U << EV_BPS) {
        x >>= 1;
        y += 1U << EV_BPS;
    }

    uint64_t z = x;

    size_t i;
    for (i = 0; i < EV_BPS; i++) {
        z = z * z >> EV_BPS;
        if (z >= 2U << EV_BPS) {
            z >>= 1;
            y += b;
        }
        b >>= 1;
    }

    return y;
}

int32_t lux_to_ev(int32_t lux)
{
    // ev = 2^lux * 2.5

    lux *= 2;
    lux /= 5;

    return log_base2(lux);
}

// Lookup table for raising numbers with EV_BPS precision to the 1.4th power.
// y = x^1.4 is quite close to being linear within the range of values we're
// interested in, so a lookup table with interpolation should be pretty
// accurate. The following Python 3 function can be used to generate the table
// values.
/*
from math import *
def calc():
    EV_BPS = 11   # <<<<<===== MUST BE CONSISTENT WITH EV_BPS IN units.h
    POW14_BPS = 7 # <<<<<===== MUST BE CONSISTENT WITH #define BELOW THIS COMMENT
    i = 0
    step = 1/(1 << POW14_BPS)
    c = 0
    print("\n    ", end='')
    while i <= 1:
        if c != 0:
            print(", ", end='')
        if c % 8 == 7:
            print("\n    ", end='')
        r = pow(i, 1.4)
        r *= (1 << EV_BPS)
        print(int(round(r)), end='')
        i += step
        c += 1
    print()
*/
#define POW14_BPS 7
static const int16_t pow14_table[] = {
    0, 2, 6, 11, 16, 22, 28,
    35, 42, 50, 58, 66, 74, 83, 92,
    102, 111, 121, 131, 142, 152, 163, 174,
    185, 197, 208, 220, 232, 244, 256, 269,
    281, 294, 307, 320, 333, 347, 360, 374,
    388, 402, 416, 430, 445, 459, 474, 489,
    504, 519, 534, 549, 565, 580, 596, 612,
    628, 644, 660, 676, 693, 709, 726, 742,
    759, 776, 793, 810, 827, 845, 862, 880,
    897, 915, 933, 951, 969, 987, 1005, 1024,
    1042, 1061, 1079, 1098, 1117, 1136, 1155, 1174,
    1193, 1212, 1231, 1251, 1270, 1290, 1310, 1329,
    1349, 1369, 1389, 1409, 1429, 1450, 1470, 1490,
    1511, 1531, 1552, 1573, 1594, 1614, 1635, 1656,
    1678, 1699, 1720, 1741, 1763, 1784, 1806, 1828,
    1849, 1871, 1893, 1915, 1937, 1959, 1981, 2003,
    2026, 2048
};

static int32_t pow14(int32_t val)
{
    int32_t i = val >> (EV_BPS - POW14_BPS);
    int32_t diff = (val << (EV_BPS - POW14_BPS)) - i;
    if (i >= sizeof(pow14_table)/sizeof(int16_t) - 1) // -1 because can't interpolate if we're at the end of the table
        return pow14_table[sizeof(pow14_table)/sizeof(int16_t) - 1];

    int32_t base = pow14_table[i];
    int32_t next = pow14_table[i+1];
    int32_t comp = (next-base) * diff / (1 << (EV_BPS - POW14_BPS));
    return base + comp;
}

int32_t sensor_reading_to_lux(sensor_reading r, int32_t gain, int32_t integ_time)
{
    // The LTR-303ALS-01 refers to a mysterious "Appendix A" for the lux
    // calculation. I haven't been able to locate this appendix. The following
    // calculation is based on the code in
    //     https://github.com/automote/LTR303/blob/628988a6e5ac1bccb1d0c0eea156f0e9dddf4d17/LTR303.cpp
    // modified to use fixed-point arithmetic.

    int32_t c0 = r.chan0;
    int32_t c1 = r.chan1;

    SEGGER_RTT_printf(0, "RC0=%u, RC1=%u\n", c0, c1);

    c0 <<= EV_BPS;
    c1 <<= EV_BPS;
    int32_t ratio = (int32_t)((((int64_t)c1) << EV_BPS) / (int64_t)c0);

    SEGGER_RTT_printf(0, "C0=%u, C1=%u RAT=%u\n", c0, c1, ratio);

    // Normalize for integration time
    c0 = (c0 * 402) / integ_time;
    c1 = (c1 * 402) / integ_time;

    SEGGER_RTT_printf(0, "AF C0=%u, C1=%u RAT=%u\n", c0, c1, ratio);

    // Normalize for gain
    c0 = (c0 * gain) / 96;
    c1 = (c1 * gain) / 96;

    int32_t lux;
    if (ratio < (1 << EV_BPS) / 2) {
        SEGGER_RTT_printf(0, "OPTION1\n");
        lux = ((c0 * 19) / 625) -
              (((((c0 * 31) / 500)) * pow14(ratio)) >> EV_BPS);
    } else if (ratio < ((1 << EV_BPS) * 61) / 100) {
        SEGGER_RTT_printf(0, "OPTION2\n");
        lux = ((c0 * 14) / 625) -
              ((c1 * 31) / 1000);
    } else if (ratio < ((1 << EV_BPS) * 4) / 5) {
        SEGGER_RTT_printf(0, "OPTION3\n");
        lux = ((c0 * 8) / 625) -
              ((c1 * 153) / 10000);
    } else if (ratio < ((1 << EV_BPS) * 13) / 10) {
        SEGGER_RTT_printf(0, "OPTION4 %u %u\n", c0, c1);
        lux = ((c0 * 73) / 50000) -
              ((c1 * 7) / 6250);
    } else {
        SEGGER_RTT_printf(0, "OPTION5 NEG NEG NEG\n");
        lux = -1;
    }

    SEGGER_RTT_printf(0, "LUX: %u\n", lux >> EV_BPS);

    return (int32_t)lux;

/*    int32_t tmpCalc, factor;
    int32_t chRatio = (1000 * r.chan1) / (r.chan0 + r.chan1);
    if (chRatio < 450) {
        tmpCalc = (r.chan0 * 17743) + (r.chan1 * 11059);
        factor = 100;
    } else if ((chRatio >= 450) && (chRatio < 680)) {
        tmpCalc = (r.chan0 * 42785) + (r.chan1 * 10696);
        factor = 80;
    } else if ((chRatio >= 680) && (chRatio < 990)) {
        tmpCalc = (r.chan0 * 5926) + (r.chan1 * 1300);
        factor = 44;
    } else {
        tmpCalc = 0;
        factor = 0;
    }

    int32_t lux;
    if (gain != 0 && integ_time != 0) {
        // Original formula:
        //lux = ((tmpCalc / (gain * integ_time)) * factor) / 100;

        lux = (((tmpCalc / (gain * integ_time)) * factor) << EV_BPS) / 100;
    } else {
        lux = 0;
    }

    return lux;*/
}