#ifndef UNITS_H
#define UNITS_H

#include <stdint.h>
#include <sensor.h>

#define EV_BPS 10 // ev and lux values stored with this number of binary places

#define F8_AP_INDEX   6
#define ISO_100_INDEX 6
#define AP_INDEX_MIN  0
#define AP_INDEX_MAX  11
#define SS_INDEX_MIN  0
#define SS_INDEX_MAX  11

#define GAIN_2X_INTEG_250_MAX_LUX  (32000 << EV_BPS)
#define GAIN_4X_INTEG_250_MAX_LUX  (16000 << EV_BPS)
#define GAIN_8X_INTEG_250_MAX_LUX  (8000 << EV_BPS)
#define GAIN_48X_INTEG_250_MAX_LUX (1300 << EV_BPS)
#define GAIN_96X_INTEG_250_MAX_LUX (600 << EV_BPS)

int32_t lux_to_ev(int32_t lux);
int32_t sensor_reading_to_lux(sensor_reading r, int32_t gain, int32_t integ_time);
void ev_to_shutter_iso100_f8(int32_t ev, int *ss_index_out, int *third_out);
void ev_iso_aperture_to_shutter(int32_t ev, int32_t iso, int32_t ap, int *ap_index_out, int *ss_index_out, int *third_out);

#endif