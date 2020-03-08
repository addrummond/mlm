#ifndef UNITS_H
#define UNITS_H

#include <stdint.h>
#include <sensor.h>

#define EV_BPS 10 // ev and lux values stored with this number of binary places

#define AP_INDEX_MIN  0
#define AP_INDEX_MAX  11
#define SS_INDEX_MIN  0
#define SS_INDEX_MAX  11

#define F1_AP_INDEX   0
#define F1_4_AP_INDEX 1
#define F2_AP_INDEX   2
#define F2_8_AP_INDEX 3
#define F4_AP_INDEX   4
#define F5_6_AP_INDEX 5
#define F8_AP_INDEX   6
#define F11_AP_INDEX  7
#define F16_AP_INDEX  8
#define F22_AP_INDEX  9
#define F32_AP_INDEX  10
#define F45_AP_INDEX  11

#define SS1_INDEX    0
#define SS2_INDEX    1
#define SS4_INDEX    2
#define SS8_INDEX    3
#define SS15_INDEX   4
#define SS30_INDEX   5
#define SS60_INDEX   6
#define SS125_INDEX  7
#define SS250_INDEX  8
#define SS500_INDEX  9
#define SS1000_INDEX 10
#define SS2000_INDEX 11

int32_t lux_to_reflective_ev(int32_t lux);
int32_t sensor_reading_to_lux(sensor_reading r, int32_t gain, int32_t integ_time);
void ev_to_shutter_iso100_f8(int32_t ev, int *ss_index_out, int *third_out);
void ev_iso_aperture_to_shutter(int32_t ev, int32_t iso, int32_t ap, int *ap_index_out, int *ss_index_out, int *third_out);

extern const char *ap_strings[];
extern const char *ss_strings[];

#endif