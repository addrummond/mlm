#include <stddef.h>
#include <units.h>
#include <rtt.h>

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
// interested in, so using a small lookup table together with linear
// interpolation is pretty accurate. The following Python 3 function can be used
// to generate the table values.
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
static const int32_t pow14_table[] = {
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
    int32_t idiff = val - (i << (EV_BPS - POW14_BPS));

    // If i is the index of the last element in the lookup table, or if i is out
    // of bounds, then we can't do interpolation. In either case, we return the
    // value of the last element in the table.
    if (i >= sizeof(pow14_table)/sizeof(int16_t) - 1)
        return pow14_table[sizeof(pow14_table)/sizeof(int32_t) - 1];

    int32_t base = pow14_table[i];
    int32_t next = pow14_table[i+1];
    int32_t extra = next - base;
    extra = (extra * idiff) >> (EV_BPS - POW14_BPS);
    return base + extra;
}

int32_t sensor_reading_to_lux(sensor_reading r, int32_t gain, int32_t integ_time)
{
    // The LTR-303ALS-01 refers to a mysterious "Appendix A" for the lux
    // calculation. I haven't been able to locate this appendix. The following
    // calculation is based on the code in
    //     https://github.com/automote/LTR303/blob/628988a6e5ac1bccb1d0c0eea156f0e9dddf4d17/LTR303.cpp
    // modified to use fixed-point arithmetic.

    int64_t c0 = r.chan0;
    int64_t c1 = r.chan1;

    if (c0 == 0)
        return 0;

    SEGGER_RTT_printf(0, "RC0=%u, RC1=%u\n", c0, c1);

    c0 <<= EV_BPS;
    c1 <<= EV_BPS;
    int32_t ratio = (int32_t)(((int64_t)c1 << EV_BPS) / (int64_t)c0);

    // Normalize for integration time
    c0 = (c0 * 402) / integ_time;
    c1 = (c1 * 402) / integ_time;

    // Normalize for gain
    c0 = (c0 * gain) / 6;
    c1 = (c1 * gain) / 6;

    int64_t lux;
    if (ratio < (1 << EV_BPS) / 2) {
        lux = ((c0 * 19) / 625) -
              (((c0 * 31 * pow14(ratio)) / 500) >> EV_BPS);
    } else if (ratio < ((1 << EV_BPS) * 61) / 100) {
        lux = ((c0 * 14) / 625) -
              ((c1 * 31) / 1000);
    } else if (ratio < ((1 << EV_BPS) * 4) / 5) {
        lux = ((c0 * 8) / 625) -
              ((c1 * 153) / 10000);
    } else if (ratio < ((1 << EV_BPS) * 13) / 10) {
        lux = ((c0 * 73) / 50000) -
              ((c1 * 7) / 6250);
    } else {
        lux = -1;
    }

    return (int32_t)lux;
}

#ifdef TEST

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static double fp_sensor_reading_to_lux(sensor_reading r, int32_t gain, int32_t integ_time)
{
    double c0, c1, itime;
    c0 = (double)r.chan0;
    c1 = (double)r.chan1;
    itime = (double)integ_time;

    double ratio = c1 / c0;

    // Normalize for integration time
    c0 *= 402.0 / itime;
    c1 *= 402.0 / itime;

    // Normalize for gain
    c0 = (c0 * gain) / 6;
    c1 = (c1 * gain) / 6;

    double lux;

    if (ratio < 0.5) {
        lux = 0.0304 * c0 - 0.062 * c0 * pow(ratio,1.4);
    } else if (ratio < 0.61) {
        lux = 0.0224 * c0 - 0.031 * c1;
    } else if (ratio < 0.80) {
        lux = 0.0128 * c0 - 0.0153 * c1;
    } else if (ratio < 1.30) {
        lux = 0.00146 * c0 - 0.00112 * c1;
    } else {
        lux = 0;
    }

    return lux;
}

static void test_pow14()
{
    for (double v = 0.01; v <= 1; v += 0.015) {
        double vf = (int32_t)(v * (1 << EV_BPS));
        int32_t r = pow14(vf);
        double r1 = ((double)r) / (1 << EV_BPS);
        double r2 = pow(v, 1.4);
        printf("pow14(%f) = approx=%f, exact=%f, diff=%f\n", v, r1, r2, fabs(r1-r));
    }
}

static void test_sensor_reading_to_lux()
{
    int32_t gain = 96;
    int32_t integ_time = 350;

    double ratios[] = { 0.1, 0.49, 0.55, 0.7, 1.0 };

    printf("gain,iteg_time,c0,c1,ratio,lux,luxf\n");

    for (int i = 0; i < sizeof(ratios)/sizeof(ratios[0]); ++i) {
        double ratio = ratios[i];
        uint16_t c0 = 0;
        for (;;) {
            double c1f = ratio * (double)c0;
            if (c1f >= (1 << 16))
                c1f = (1 << 16) - 1;
            uint16_t c1 = (double)c1f;
            sensor_reading r;
            r.chan0 = c0;
            r.chan1 = c1;
            double lux = fp_sensor_reading_to_lux(r, gain, integ_time);
            int32_t luxf = sensor_reading_to_lux(r, gain, integ_time);
            double luxfd = ((double)luxf) / (1 << EV_BPS);
            printf("%i,%i,%u,%u,%.2f,%.2f,%.2f\n", gain, integ_time, c0, c1, ratio, lux, luxfd);

            if (c0 >= (1 << 16) -16)
                break;
            c0 += 16;
        }
    }
}

int main()
{
    test_pow14();
    test_sensor_reading_to_lux();

    return 0;
}

#endif