#ifndef UNITS_H
#define UNITS_H

#include <stdint.h>
#include <sensor.h>

#define EV_BPS 10 // ev and lux values stored with this number of binary places

int32_t lux_to_ev(int32_t lux);
int32_t sensor_reading_to_lux(sensor_reading r, int32_t gain, int32_t integ_time);
void ev_to_shutter_iso100_f8(int32_t ev, int *ss_index_out, int *third_out);

#endif