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

int32_t sensor_reading_to_lux(sensor_reading r, int32_t gain, int32_t integ_time)
{
    // The LTR-303ALS-01 datasheet refers to a mysterious "Appendix A" for the
    // lux calculation. I haven't been able to locate this appendix. The
    // following calculation has been derived from various scraps of information
    // online.

    // Doing this with int64s just to be on the safe side. This could probably
    // be rewritten to use int32s with some more careful thought about maximum
    // values.

    int64_t c0 = r.chan0;
    int64_t c1 = r.chan1;

    if (c0 == 0)
        return 0;

    c0 <<= EV_BPS;
    c1 <<= EV_BPS;
    int32_t ratio = (int32_t)(((int64_t)c1 << EV_BPS) / (int64_t)c0);

    int64_t lux;
    int64_t g64 = gain;
    if (ratio < (45 << EV_BPS) / 100) {
        lux = ((c0 * 19240) + (c1 * 18119)) / g64 / (1 << 14);
    } else if (ratio < (64 << EV_BPS) / 100) {
        lux = ((c0 * 70099) + (c1 * 32027)) / g64 / (1 << 14);
    } else if (ratio < (85 << EV_BPS) / 100) {
        lux = ((c0 * 9709) + (c1 * 1942)) / g64 / (1 << 14);
    } else {
        return -1;
    }

    // compensate for integration time.
    lux = ((lux * integ_time) / 100);

    return (int32_t)lux;
}

static const int32_t F8_AP_INDEX = 6;
static const int32_t ISO_100_INDEX = 4;
static const int32_t AP_INDEX_MIN = 0;
static const int32_t AP_INDEX_MAX = 12;
static const int32_t SS_INDEX_MIN = 0;
static const int32_t SS_INDEX_MAX = 12;

// Assume that we have an infinite sequence of lights indicating shutter speeds.
// The light at index 0 indicates 1S. The light at index n+1 indicates the
// shutter speed at one stop above the light at index n. Given an EV@100 value,
// this function calculates the index (possibly negative) of the required
// shutter speed for an aperture of f8 and ISO of 100.
void ev_to_shutter_iso100_f8(int32_t ev, int *ss_index_out, int *third_out)
{
    // 1 second at f8 at ISO 100 is EV 6. Thus, for every stop our ev value
    // is above EV 6, we need to add one to our shutter speed index.

    int32_t whole = ev >> EV_BPS;
    int32_t frac = ev & (1 << (EV_BPS-1));
    int ss_index = whole - 6;
    int third = 0;

    if (frac > (1 << EV_BPS)/3) {
        third = 1;
    } else if (frac <= (2 << EV_BPS)/3) {
        ++ss_index;
        third = -1;
    }

    if (ss_index_out != NULL)
        *ss_index_out = ss_index;
    if (third_out != NULL)
        *third_out = third;
}

void ev_iso_aperture_to_shutter(int32_t ev, int32_t iso, int32_t ap, int *ap_index_out, int *ss_index_out, int *third_out)
{
    int fsiso = iso / 3;
    int r = iso % 3;
    int ss_index, third;

    // For calculation purposes it's convenient to have the ISO setting on a
    // full stop as well as the aperture and the shutter speed. Thus, if the
    // ISO not on a full stop boundary, we adjust the ev value to compensate.
    ev_to_shutter_iso100_f8(ev + ((1<<EV_BPS)*r)/3, &ss_index, &third);

    // Adjust shutter speed to compensate for ISO difference.
    ss_index += fsiso - ISO_100_INDEX;

    // Adjust shutter speed to get desired aperture.
    ss_index += ap - F8_AP_INDEX;

    // Make adjustments if the shutter speed is out of range.
    if (ss_index < SS_INDEX_MIN) {
        ap -= SS_INDEX_MIN - ss_index;
        if (ap < AP_INDEX_MIN) {
            // We can't display this exposure at the given ISO.
            *ap_index_out = -1;
            *ss_index_out = -1;
            *third_out = -1;
        } else {
            *ap_index_out = ap;
            *ss_index_out = ss_index;
            *third_out = third;
        }
    } else if (ss_index > SS_INDEX_MAX) {
        ap += ss_index - SS_INDEX_MAX;
        if (ap > AP_INDEX_MAX) {
            // We can't display this exposure at the given ISO.
            *ap_index_out = -1;
            *ss_index_out = -1;
            *third_out = -1;
        } else {
            *ap_index_out = ap;
            *ss_index_out = ss_index;
            *third_out = third;
        }
    }
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

    lux = (lux * 400) / ((double)integ_time * (double)gain);

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

static bool test_sensor_reading_to_lux_problem_cases()
{
    // problem:
    // GAIN 4 ITIME 100 c0=2176 c1=2807
    // READING 2176 2807 lux=543/1024 (0) ev=4294965003/1024 (4294967293)
    sensor_reading r;
    r.chan0 = 2176;
    r.chan1 = 2807;
    double lux = fp_sensor_reading_to_lux(r, 4, 100);
    int32_t luxf = sensor_reading_to_lux(r, 4, 100);
    double luxfd = ((double)luxf) / (1 << EV_BPS);
    printf("gain=4,itime=100,c0=2176,c1=2807: fix=%.2f, fp=%.2f\n", luxfd, lux);
    return false;
}

static bool test_sensor_reading_to_lux()
{
    bool passed = true;

    int32_t integ_times[] = { 50, 100, 150, 200, 250, 300, 350, 400 };
    double ratios[] = { 0.1, 0.25, 0.5, 1.0 };
    int32_t gains[] = { 1, 2, 4, 8, 48, 96 };

    FILE *fp = fopen("testoutputs/luxcalc.csv", "w");

    fprintf(fp, "gain,integ_time,c0,c1,ratio,lux,luxf\n");

    int line = 1;
    for (int k = 0; k < sizeof(integ_times)/sizeof(integ_times[0]); ++k) {
        int32_t integ_time = integ_times[k];

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
                    double absdiff = fabs(lux-luxfd);
                    if (absdiff / lux >= 0.01) {
                        fprintf(stderr, "BAD at line %i: %.3f %.3f (absdiff=%.3f, reldiff=%.3f)\n", line, lux, luxfd, absdiff, absdiff / lux);
                        passed = false;
                    }

                    if (c0 >= (1 << 16) -16)
                        break;
                    c0 += 16;
                    ++line;
                }
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
    bool lux_problem_cases_passed = test_sensor_reading_to_lux_problem_cases();

    printf("\n");
    printf("pow14 test...................%s\n", passed(pow14_passed));
    printf("lux_to_ev_test...............%s\n", passed(lux_to_ev_passed));
    printf("sensor_reading_to_lux test...%s\n", passed(sensor_reading_to_lux_passed));
    printf("lux problem cases test.......%s\n", passed(lux_problem_cases_passed));

    return 0;
}

#endif