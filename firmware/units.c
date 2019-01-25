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

int32_t sensor_reading_to_lux(sensor_reading r, int32_t gain, int32_t integ_time)
{
    // The LTR-303ALS-01 refers to a mysterious "Appendix A" for the lux
    // calculation. I haven't been able to locate this appendix. The following
    // calculation is based on the code in
    //     https://github.com/alibaba/AliOS-Things/blob/8cae6d447d331989ede14d28c0ec189f2aa2b3c7/device/sensor/drv/drv_als_liteon_ltr303.c

    int32_t tmpCalc, factor;
    int32_t chRatio = (1000 * r.chan1) / (r.chan0 + r.chan1);
    if (chRatio < 450)
    {
        tmpCalc = (r.chan0 * 17743) + (r.chan1 * 11059);
        factor = 100;
    }
    else if ((chRatio >= 450) && (chRatio < 680))
    {
        tmpCalc = (r.chan0 * 42785) + (r.chan1 * 10696);
        factor = 80;
    }
    else if ((chRatio >= 680) && (chRatio < 990))
    {
        tmpCalc = (r.chan0 * 5926) + (r.chan1 * 1300);
        factor = 44;
    }
    else
    {
        tmpCalc = 0;
        factor = 0;
    }

    int32_t lux;
    if (gain != 0 && integ_time != 0) {
        // Original formula:
        //   lux = ((tmpCalc / (als_gain_val * als_integ_time_val)) * factor) / 100;

        lux = (((tmpCalc / (gain * integ_time)) * factor) << EV_BPS) / 100;
    }
    else {
        lux = 0;
    }

    return lux;
}