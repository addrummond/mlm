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

#define GAIN_MASK (0b111 << 2)
#define GAIN_1X   0             // 1 lux to 64k lux
#define GAIN_2X   (1 << 2)      // 0.5 lux to 32k lux
#define GAIN_4X   (0b10 << 2)   // 0.25 lux to 16k lux
#define GAIN_8X   (0b11 << 2)   // 0.125 lux to 8k lux
#define GAIN_48X  (0b110 << 2)  // 0.02 lux to 1.3k lux
#define GAIN_96X  (0b111 << 2)  // 0.01 lux to 600 lux

#define ITIME_MASK (0b111 << 3)
#define ITIME_100  0
#define ITIME_50   (0b001 << 3)
#define ITIME_200  (0b010 << 3)
#define ITIME_400  (0b011 << 3)
#define ITIME_150  (0b100 << 3)
#define ITIME_250  (0b101 << 3)
#define ITIME_300  (0b110 << 3)
#define ITIME_350  (0b111 << 3)

typedef struct sensor_reading {
    uint16_t chan0;
    uint16_t chan1;
} sensor_reading;

void sensor_init();
void sensor_write_reg(uint8_t reg, uint8_t val);
void sensor_turn_on(uint8_t gain);
void sensor_standby(void);
uint8_t sensor_read_reg(uint8_t reg);
uint16_t sensor_read_reg16(uint8_t reg);
void sensor_write_reg(uint8_t reg, uint8_t val);
sensor_reading sensor_get_reading();
sensor_reading sensor_get_reading_auto(int32_t *gain, int32_t *itime);
void sensor_wait_till_ready(void);

#endif