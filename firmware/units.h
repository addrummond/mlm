#ifndef UNITS_H
#define UNITS_H

#include <stdint.h>
#include <sensor.h>

#define EV_BPS 7 // ev and lux values stored with this number of binary places

int32_t lux_to_ev(int32_t lux);

#endif