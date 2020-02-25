#ifndef UNITS_H
#define UNITS_H

#include <stdint.h>
#include <sensor.h>

#define EV_BPS 10 // ev and lux values stored with this number of binary places

#define AP_INDEX_MIN  0
#define AP_INDEX_MAX  11
#define SS_INDEX_MIN  0
#define SS_INDEX_MAX  11
#define ISO_MAX       23

#define ISO_100_FULL_STOP_INDEX 6

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

#define ISO_6_INDEX    0
#define ISO_8_INDEX    1
#define ISO_10_INDEX   2
#define ISO_12_INDEX   3
#define ISO_16_INDEX   4
#define ISO_20_INDEX   5
#define ISO_25_INDEX   6
#define ISO_32_INDEX   7
#define ISO_40_INDEX   8
#define ISO_50_INDEX   9
#define ISO_64_INDEX   10
#define ISO_80_INDEX   11
#define ISO_100_INDEX  12
#define ISO_125_INDEX  13
#define ISO_160_INDEX  14
#define ISO_200_INDEX  15
#define ISO_250_INDEX  16
#define ISO_320_INDEX  17
#define ISO_400_INDEX  18
#define ISO_500_INDEX  19
#define ISO_640_INDEX  20
#define ISO_800_INDEX  21
#define ISO_1000_INDEX 22
#define ISO_1200_INDEX 23

int32_t lux_to_reflective_ev(int32_t lux);
int32_t sensor_reading_to_lux(sensor_reading r, int32_t gain, int32_t integ_time);
void ev_to_shutter_iso100_f8(int32_t ev, int *ss_index_out, int *third_out);
void ev_iso_aperture_to_shutter(int32_t ev, int32_t iso, int32_t ap, int *ap_index_out, int *ss_index_out, int *third_out);

extern const char *iso_strings[];
extern const char *ap_strings[];
extern const char *ss_strings[];

#endif