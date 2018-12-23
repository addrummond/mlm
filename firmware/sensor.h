#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>

typedef struct sensor_reading {
    uint16_t chan0;
    uint16_t chan1;
} sensor_reading;

void sensor_init();
void sensor_write_reg(uint8_t reg, uint8_t val);
void sensor_turn_on();
uint8_t sensor_read_reg(uint8_t reg);
void sensor_write_reg(uint8_t reg, uint8_t val);
sensor_reading sensor_get_reading();

#endif