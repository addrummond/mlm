#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>

void sensor_init();
void sensor_write_reg(uint8_t reg, uint8_t val);
void sensor_turn_on();
uint8_t sensor_read_reg(uint8_t reg);
void sensor_write_reg(uint8_t reg, uint8_t val);
uint16_t sensor_get_reading();

#endif