#ifndef TEMPSENSOR_H
#define TEMPSENSOR_H


#include <stdint.h>

void tempsensor_init(void);
int32_t tempsensor_get_reading();


#endif