#include <stdbool.h>
#include <stddef.h>
#include <units.h>
#include <rtt.h>

#ifdef TEST
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#endif

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

// I finally located the mysterious "Appendix A" referred to on the datasheet here:
//
//     https://github.com/automote/LTR303/issues/2
//
// The lux formula given is as follows. Note that unit for ALS_INT is in
// 1/10ths of a second.
//
// RATIO = CH1/(CH0+CH1)
// IF (RATIO < 0.45)
//   ALS_LUX = (1.7743 * CH0 + 1.1059 * CH1) / ALS_GAIN / ALS_INT
// ELSEIF (RATIO < 0.64 && RATIO >= 0.45)
//   ALS_LUX = (4.2785 * CH0 – 1.9548 * CH1) / ALS_GAIN / ALS_INT
// ELSEIF (RATIO < 0.85 && RATIO >= 0.64)
//   ALS_LUX = (0.5926 * CH0 + 0.1185 * CH1) / ALS_GAIN / ALS_INT
// ELSE
//   ALS_LUX = 0
// END
//
//
int32_t sensor_reading_to_lux(sensor_reading r, int32_t als_gain_val, int32_t als_integ_time_val)
{
    int32_t rat = ((int32_t)r.chan1 * 100) / ((int32_t)r.chan0 + (int32_t)r.chan1);
    int64_t ch0 = r.chan0;
    int64_t ch1 = r.chan1;

#define TOFP(x) ((int64_t)(((double)(1 << EV_BPS)) * (x) + 0.5))
    static const int64_t c1a = TOFP(1.7743);
    static const int64_t c1b = TOFP(1.1059);
    static const int64_t c2a = TOFP(4.2785);
    static const int64_t c2b = TOFP(1.9548);
    static const int64_t c3a = TOFP(0.5926);
    static const int64_t c3b = TOFP(0.1185);
#undef TOFP

    int64_t tmp;
    if (rat < 45)
        tmp = (c1a * ch0 + c1b * ch1);
    else if (rat < 64)
        tmp = (c2a * ch0 - c2b * ch1);
    else if (rat < 85)
        tmp = (c3a * ch0 + c3b * ch1);
    else
        tmp = 0;
    
    if (tmp < 0)
        tmp = 0;
    
    // We multiply tmp by 100 because als_integ_time_val is in ms, whereas ALS_INT in
    // the formula above is in 1/10th seconds.
    return (int32_t)((tmp * 100) / (int64_t)als_gain_val / (int64_t)(als_integ_time_val));
}

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
    int32_t frac = ev & ((1 << EV_BPS)-1);
    int ss_index = whole - 6;
    int third = 0;

    if (frac > (1 << EV_BPS)/3) {
        third = 1;
    } else if (frac > (2 << EV_BPS)/3) {
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

#ifdef TEST
    fprintf(stderr, "EV %i -> ss %i at f8 with %i/3\n", ev, ss_index, third);
    fprintf(stderr, "SS iso comp %i\n", fsiso - ISO_100_INDEX);
    fprintf(stderr, "SS ap comp %i\n", ap - F8_AP_INDEX);
#endif

    // Adjust shutter speed to compensate for ISO difference.
    ss_index += fsiso - ISO_100_INDEX/3;

    // Adjust shutter speed to get desired aperture.
    ss_index -= ap - F8_AP_INDEX;

    // Make adjustments if the shutter speed is out of range.
    if (ss_index < SS_INDEX_MIN) {
        ap -= SS_INDEX_MIN - ss_index;
        ss_index = SS_INDEX_MIN;
        if (ap < AP_INDEX_MIN) {
            // We can't display this exposure at the given ISO.
            goto error_set;
        } else {
            goto default_set;
        }
    } else if (ss_index > SS_INDEX_MAX) {
        ap += ss_index - SS_INDEX_MAX;
        ss_index = SS_INDEX_MAX;
        if (ap > AP_INDEX_MAX) {
            // We can't display this exposure at the given ISO.
            goto error_set;
        } else {
            goto default_set;
        }
    } else {
        goto default_set;
    }

error_set:
    *ap_index_out = -1;
    *ss_index_out = -1;
    *third_out = -1;
    return;

default_set:
    *ap_index_out = ap;
    *ss_index_out = ss_index;
    *third_out = third;
}

const char *iso_strings[] = {
    "6",
    "8",
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
    "1250"
};

const char *ss_strings[] = {
    "1S",
    "2",
    "4",
    "8",
    "15",
    "30",
    "60",
    "125",
    "250",
    "500",
    "1000",
    "2000"
};

const char *ap_strings[] = {
    "1",
    "1.4",
    "2",
    "2.8",
    "4",
    "5.6",
    "8",
    "11",
    "16",
    "22",
    "32",
    "45"
};

#ifdef TEST

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

static bool plot_channels_to_lux()
{
    FILE *fp = fopen("testoutputs/channels_to_lux.csv", "w");

    static const int32_t gains[] = { 1, 2, 4, 8, 48, 96 };
    static const int32_t integ_times[] = { 50, 100, 150, 200, 250, 300, 350, 400 };

    fprintf(fp, "gain,integ_time,chan1,chan2,lux_i,lux_fp\n");

    for (int gi = 0; gi < sizeof(gains)/sizeof(gains[0]); ++gi) {
        int32_t gain = gains[gi];

        for (int iti = 0; iti < sizeof(integ_times)/sizeof(integ_times[0]); ++iti) {
            int32_t integ_time = integ_times[iti];

            for (int chan1 = 512; chan1 < (1 << 16)-512; chan1 += 512) {
                for (int chan2 = 512; chan2 < (1 << 16)-512; chan2 += 512) {
                    int32_t lux = sensor_reading_to_lux((sensor_reading){ chan1, chan2 }, gain, integ_time);
                    fprintf(fp, "%i,%i,%i,%i,%i,%f\n", gain, integ_time, chan1, chan2, lux, ((double)lux)/(double)(1 << EV_BPS));
                }
            }
        }
    }

    return true;
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
        { 12 << EV_BPS, ISO_100_INDEX * 3, 5/*f5.6*/, 5/*f5.6*/, 7/*125*/, 0  },
        { 13 << EV_BPS, (ISO_100_INDEX-1) * 3, 5/*f5.6*/, 5/*f5.6*/, 7/*125*/, 0  },
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
    bool channels_to_lux_passed = plot_channels_to_lux();
    bool ev_iso_aperture_to_shutter_passed = test_ev_iso_aperture_to_shutter();

    printf("\n");
    printf("test_lux_to_ev.....................%s\n", passed(lux_to_ev_passed));
    printf("plot_channels_to_lux...............%s\n", passed(channels_to_lux_passed));
    printf("test_ev_iso_aperture_to_shutter....%s\n", passed(ev_iso_aperture_to_shutter_passed));

    return 0;
}

#endif