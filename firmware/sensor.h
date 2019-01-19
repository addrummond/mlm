#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>

#define REG_ALS_CONTR         0x80
#define REG_ALS_MEAS_RATE     0x85
#define REG_PART_ID           0x86
#define REG_MANUFAC_ID        0x87
#define REG_ALS_DATA_CH1_0    0x88
#define REG_ALS_DATA_CH1_1    0x89
#define REG_ALS_DATA_CH0_0    0x8A
#define REG_ALS_DATA_CH0_1    0x8B
#define REG_ALS_STATUS        0x8C
#define REG_INTERRUPT         0x8F
#define REG_ALS_THRES_UP_0    0x97
#define REG_ALS_THRES_UP_1    0x98
#define REG_ALS_THRES_LOW_0   0x99
#define REG_ALS_THRES_LOW_1   0x9A
#define REG_INTERRUPT_PERSIST 0x9E

typedef struct sensor_reading {
    uint16_t chan0;
    uint16_t chan1;
} sensor_reading;

void sensor_init();
void sensor_write_reg(uint8_t reg, uint8_t val);
void sensor_turn_on();
uint8_t sensor_read_reg(uint8_t reg);
uint16_t sensor_read_reg16(uint8_t reg);
void sensor_write_reg(uint8_t reg, uint8_t val);
sensor_reading sensor_get_reading();

#endif