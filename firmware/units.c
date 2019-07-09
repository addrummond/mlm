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

typedef struct eviats_test {
    int32_t ev;
    int32_t iso;
    int32_t ap;
    int32_t ap_out;
    int32_t ss_out;
    int32_t third_out;
} eviats_test;

static bool test_ev_iso_aperture_to_shutter()
{
    bool passed = true;

    static const eviats_test tests[] = {
        { 12 << EV_BPS, ISO_100_INDEX * 3, 5/*f5.6*/, 7/*120*/, 0  },
    };

    for (int i = 0; i < sizeof(tests)/sizeof(tests[0]); ++i) {
        int ap, ss, third;
        ev_iso_aperture_to_shutter(tests[i].ev, tests[i].iso, tests[i].ap, &ap, &ss, &third);
        if (ap != tests[i].ap_out || ss != tests[i].ss_out || third != tests[i].third_out) {
            passed = false;
            fprintf(stderr, "Expected %i %i %i got %i %i %i\n", tests[i].ap_out, tests[i].ss_out, tests[i].third_out, ap, ss, third);
        }
    }

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
    bool lux_to_ev_passed = test_lux_to_ev();
    bool ev_iso_aperture_to_shutter_passed = test_ev_iso_aperture_to_shutter();

    printf("\n");
    printf("test_lux_to_ev.....................%s\n", passed(lux_to_ev_passed));
    printf("test_ev_iso_aperture_to_shutter....%s\n", passed(ev_iso_aperture_to_shutter_passed));

    return 0;
}

#endif