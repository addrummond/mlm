#include <stdbool.h>
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

// For small lux values we convert to EV using a lookup table.
//
// Python3 code to calculate the table:
/*
from math import *
def calc():
    EV_BPS = 10 # <<<<<===== MUST BE CONSISTENT WITH EV_BPS IN units.h
    FRAC = 1/256
    lux = FRAC
    i = 0
    print("    %i," % -(5 << EV_BPS))
    print("    ", end="")
    while lux <= 0.5:
        if i != 0:
            print(", ", end="")
        if i != 0 and i % 8 == 0:
            print("\n    ", end="")
        print(round(log2(lux * 0.4) * (1 << EV_BPS)), end='')
        lux += FRAC
        i += 1
    print()
*/
static const int16_t lux_to_ev_lookup[] = {
    -5120, // 0 lux -> -5EV
    -9546, -8522, -7923, -7498, -7168, -6899, -6671, -6474,
    -6300, -6144, -6003, -5875, -5756, -5647, -5545, -5450,
    -5360, -5276, -5196, -5120, -5048, -4979, -4914, -4851,
    -4790, -4732, -4677, -4623, -4571, -4521, -4473, -4426,
    -4380, -4336, -4293, -4252, -4211, -4172, -4133, -4096,
    -4060, -4024, -3989, -3955, -3922, -3890, -3858, -3827,
    -3796, -3766, -3737, -3708, -3680, -3653, -3626, -3599,
    -3573, -3547, -3522, -3497, -3473, -3449, -3425, -3402,
    -3379, -3356, -3334, -3312, -3291, -3269, -3248, -3228,
    -3207, -3187, -3167, -3148, -3128, -3109, -3091, -3072,
    -3054, -3036, -3018, -3000, -2982, -2965, -2948, -2931,
    -2915, -2898, -2882, -2866, -2850, -2834, -2818, -2803,
    -2787, -2772, -2757, -2742, -2728, -2713, -2699, -2684,
    -2670, -2656, -2642, -2629, -2615, -2602, -2588, -2575,
    -2562, -2549, -2536, -2523, -2510, -2498, -2485, -2473,
    -2461, -2449, -2437, -2425, -2413, -2401, -2389, -2378
};

int32_t lux_to_ev(int32_t lux)
{
    // lux = 2^ev * 2.5

    if (lux == 0)
        return -(5 << EV_BPS);

    // For lux values of 0.5 and below, the calculation is a little inaccurate,
    // so we use a lookup table instead.
    if (lux <= (1 << (EV_BPS-1))) {
        int32_t luxdiv16 = lux >> (EV_BPS - 8);
        if (luxdiv16 >= sizeof(lux_to_ev_lookup)/sizeof(lux_to_ev_lookup[0]))
            return lux_to_ev_lookup[sizeof(lux_to_ev_lookup)/sizeof(lux_to_ev_lookup[0]) - 1];
        int32_t under = lux_to_ev_lookup[luxdiv16];
        int32_t over = lux_to_ev_lookup[luxdiv16+1] + (luxdiv16 >> (EV_BPS-8)); // second term empirically determined
        int32_t diff = over - under;
        int32_t extra = lux - (luxdiv16 << (EV_BPS - 8));
        diff *= extra;
        diff >>= EV_BPS - 8;
        return under + diff;
    }

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
    EV_BPS = 10   # <<<<<===== MUST BE CONSISTENT WITH EV_BPS IN units.h
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
    0, 1, 3, 5, 8, 11, 14,
    18, 21, 25, 29, 33, 37, 42, 46,
    51, 56, 61, 66, 71, 76, 82, 87,
    93, 98, 104, 110, 116, 122, 128, 134,
    141, 147, 154, 160, 167, 173, 180, 187,
    194, 201, 208, 215, 222, 230, 237, 244,
    252, 259, 267, 275, 282, 290, 298, 306,
    314, 322, 330, 338, 346, 355, 363, 371,
    380, 388, 397, 405, 414, 422, 431, 440,
    449, 458, 467, 475, 484, 494, 503, 512,
    521, 530, 540, 549, 558, 568, 577, 587,
    596, 606, 616, 625, 635, 645, 655, 665,
    675, 685, 695, 705, 715, 725, 735, 745,
    755, 766, 776, 786, 797, 807, 818, 828,
    839, 849, 860, 871, 881, 892, 903, 914,
    925, 936, 946, 957, 968, 979, 991, 1002,
    1013, 1024
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

    c0 <<= EV_BPS;
    c1 <<= EV_BPS;
    int32_t ratio = (int32_t)(((int64_t)c1 << EV_BPS) / (int64_t)c0);

    // Normalize for gain
    // We know from the code linked above that the channel values should be
    // multiplied by 16 prior to the calculations if gain is set to 1X and the
    // integration time is 400.0 ms.
    // We infer from the datasheet for a different part (!), the LTR303ALS,
    // that as the gain is increased above 1, the result should be divded by
    // the gain and by (400/integration time).
    c0 *= 16;
    c1 *= 16;

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
        return -1;
    }

    // compensate for gain and integration time.
    lux = (lux * (400 << EV_BPS)) / (integ_time * gain);
    // this is now at 2*EV_BPS precision
    int64_t round = lux & ((1 << EV_BPS)-1);
    lux >>= EV_BPS;
    if (round >= (1 << EV_BPS)/2)
        ++lux;

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

    c0 *= 16;
    c1 *= 16;

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

    lux = ((lux * 400) / ((double)integ_time) * (double)gain);

    return lux;
}

static bool test_log_base2()
{
    return false;
}

static double fp_lux_to_ev(double lux)
{
    return log2(lux * (2.0/5.0));
}

static bool test_lux_to_ev()
{
    bool passed = true;

    FILE *fp = fopen("testoutputs/lux_to_ev.csv", "w");
    fprintf(fp, "lux,ev_fix,ev_fp,diff\n");

    for (double lux = 0.1; lux < 656000; lux *= 1.2) {
        int32_t ilux = (int32_t)(lux * (1 << EV_BPS));
        double r1 = fp_lux_to_ev(lux);
        int32_t r2 = lux_to_ev(ilux);
        double r2f = ((double)r2) / (1 << EV_BPS);
        double diff = fabs(r1-r2f);
        if (diff > 0.01)
            passed = false;
        fprintf(fp, "%f,%f,%f,%f\n", lux, r2f, r1, diff);
    }

    return passed;
}

static bool test_pow14()
{
    bool passed = true;

    FILE *fp = fopen("testoutputs/pow14.csv", "w");

    for (double v = 0.01; v <= 1; v += 0.015) {
        double vf = (int32_t)(v * (1 << EV_BPS));
        int32_t r = pow14(vf);
        double r1 = ((double)r) / (1 << EV_BPS);
        double r2 = pow(v, 1.4);
        double diff = fabs(r1-r2);

        if (diff > 0.0021)
            passed = false;

        fprintf(fp, "pow14(%f) = approx=%f, exact=%f, diff=%f\n", v, r1, r2, diff);
    }

    fclose(fp);

    return passed;
}

static bool test_sensor_reading_to_lux()
{
    bool passed = true;

    int32_t integ_time = 350;

    double ratios[] = { 0.1, 0.49, 0.55, 0.7, 1.0 };
    int32_t gains[] = { 1, 2, 4, 8, 48, 96 };

    FILE *fp = fopen("testoutputs/luxcalc.csv", "w");

    fprintf(fp, "gain,iteg_time,c0,c1,ratio,lux,luxf\n");

    for (int i = 0; i < sizeof(gains)/sizeof(gains[0]); ++i) {
        int32_t gain = gains[i];

        for (int j = 0; j < sizeof(ratios)/sizeof(ratios[0]); ++j) {
            double ratio = ratios[j];

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
                fprintf(fp, "%i,%i,%u,%u,%.2f,%.2f,%.2f\n", gain, integ_time, c0, c1, ratio, lux, luxfd);
                if (fabs(lux-luxfd) > 0.001)
                    passed = false;

                if (c0 >= (1 << 16) -16)
                    break;
                c0 += 16;
            }
        }
    }

    fclose(fp);

    return passed;
}

static const char *passed(bool p)
{
    if (p)
        return "PASSED";
    return "FAILED";
}

int main()
{
    bool pow14_passed = test_pow14();
    bool lux_to_ev_passed = test_lux_to_ev();
    bool sensor_reading_to_lux_passed = test_sensor_reading_to_lux();

    printf("\n");
    printf("pow14 test...................%s\n", passed(pow14_passed));
    printf("lux_to_ev_test...............%s\n", passed(lux_to_ev_passed));
    printf("sensor_reading_to_lux test...%s\n", passed(sensor_reading_to_lux_passed));

    return 0;
}

#endif