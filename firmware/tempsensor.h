#ifndef TEMPSENSOR_H
#define TEMPSENSOR_H


#include <stdint.h>

typedef void (*delay_func)(int ms);

void tempsensor_init(void);
int32_t tempsensor_get_reading(delay_func delay);


#endif