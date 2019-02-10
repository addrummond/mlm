#include <em_cmu.h>
#include <em_gpio.h>
#include <em_i2c.h>
#include <rtt.h>
#include <sensor.h>
#include <util.h>

#define SENSOR_I2C_PORT       gpioPortE
#define SENSOR_I2C_SDA_PIN    12
#define SENSOR_I2C_SCL_PIN    13

#define SENSOR_I2C_ADDR       (0x29 << 1)

#define SENSOR_INT_PORT       gpioPortF
#define SENSOR_INT_PIN        1

// https://github.com/alibaba/AliOS-Things/blob/8cae6d447d331989ede14d28c0ec189f2aa2b3c7/device/sensor/drv/drv_als_liteon_ltr303.c

void sensor_init()
{
    SEGGER_RTT_printf(0, "Starting sensor initialization..\n");

    CMU_ClockEnable(cmuClock_I2C0, true);

    GPIO_PinModeSet(SENSOR_I2C_PORT, SENSOR_I2C_SCL_PIN, gpioModeWiredAndFilter, 1); // configure SCL pin as open drain output
    GPIO_PinModeSet(SENSOR_I2C_PORT, SENSOR_I2C_SDA_PIN, gpioModeWiredAndFilter, 1); // configure SDA pin as open drain output

    I2C0->ROUTE = I2C_ROUTE_SDAPEN | I2C_ROUTE_SCLPEN | (6 << _I2C_ROUTE_LOCATION_SHIFT);
    I2C0->CTRL = I2C_CTRL_AUTOACK | I2C_CTRL_AUTOSN;

    /* In some situations (after a reset during an I2C transfer), the slave */
    /* device may be left in an unknown state. Send 9 clock pulses just in case. */
    for (unsigned i = 0; i < 9; i++)
    {
        /*
         * TBD: Seems to be clocking at appr 80kHz-120kHz depending on compiler
         * optimization when running at 14MHz. A bit high for standard mode devices,
         * but DVK only has fast mode devices. Need however to add some time
         * measurement in order to not be dependable on frequency and code executed.
        */
        GPIO_PinModeSet(SENSOR_I2C_PORT, SENSOR_I2C_SCL_PIN, gpioModeWiredAndFilter, 0);
        GPIO_PinModeSet(SENSOR_I2C_PORT, SENSOR_I2C_SCL_PIN, gpioModeWiredAndFilter, 1);
    }

    I2C_Init_TypeDef i2c_init = {
        .enable = true,
        .master = true,
        .refFreq = 0,
        .freq = I2C_FREQ_STANDARD_MAX,
        .clhr = i2cClockHLRAsymetric
    };

    I2C_Init(I2C0, &i2c_init);
    NVIC_ClearPendingIRQ(I2C0_IRQn);
    NVIC_DisableIRQ(I2C0_IRQn);

    SEGGER_RTT_printf(0, "Sensor initialization complete.\n");
}

static void print_stat(int status)
{
    if (status == 0)
        SEGGER_RTT_printf(0, "S: I2C done.\n");
    else if (status == 1)
        SEGGER_RTT_printf(0, "S: I2C in progress\n");
    else if (status == -1)
        SEGGER_RTT_printf(0, "S: NACK\n");
    else if (status == -2)
        SEGGER_RTT_printf(0, "S: BUS ERR\n");
    else if (status == -3)
        SEGGER_RTT_printf(0, "S: ARB LOST\n");
    else if (status == -4)
        SEGGER_RTT_printf(0, "S: USAGE FAULT\n");
    else if (status == -5)
        SEGGER_RTT_printf(0, "S: SW FAULT\n");
    else
        SEGGER_RTT_printf(0, "S: UNKNOWN %u\n", status);
}

void sensor_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t wbuf[] = { reg, val };
    I2C_TransferSeq_TypeDef i2c_transfer = {
        .addr = SENSOR_I2C_ADDR,
        .flags = I2C_FLAG_WRITE,
        .buf[0].data = wbuf,
        .buf[0].len = 2
    };
    int status = I2C_TransferInit(I2C0, &i2c_transfer);
    while (status != i2cTransferDone) {
        status = I2C_Transfer(I2C0);
    }
}

uint8_t sensor_read_reg(uint8_t reg)
{
    return (uint8_t)(sensor_read_reg16(reg) & 0xFF);
}

uint16_t sensor_read_reg16(uint8_t reg)
{
    uint8_t wbuf[1];
    wbuf[0] = reg;
    uint8_t rbuf[2];
    I2C_TransferSeq_TypeDef i2c_transfer = {
        .addr = SENSOR_I2C_ADDR,
        .flags = I2C_FLAG_WRITE_READ,
        .buf[0].data = wbuf,
        .buf[0].len = sizeof(wbuf)/sizeof(wbuf[0]),
        .buf[1].data = rbuf,
        .buf[1].len = sizeof(rbuf)/sizeof(rbuf[0])
    };
    int status = I2C_TransferInit(I2C0, &i2c_transfer);
    while (status == i2cTransferInProgress)
        status = I2C_Transfer(I2C0);
    return (uint16_t)rbuf[0] | ((uint16_t)rbuf[1] << 8);
}

sensor_reading sensor_get_reading()
{
    sensor_reading r;
    // The datasheet says that chan1 is to be read before chan0. This is
    // important for ensuring that the various status flags are correctly
    // reset following a read.
    r.chan1 = sensor_read_reg16(REG_ALS_DATA_CH1_0);
    r.chan0 = sensor_read_reg16(REG_ALS_DATA_CH0_0);

    return r;
}

bool sensor_has_valid_data()
{
    uint8_t status = sensor_read_reg(REG_ALS_STATUS);
    return !(status & 0b10000000);
}

sensor_reading sensor_get_reading_auto(int32_t *gain, int32_t *itime)
{
    // We try:
    //
    //     100ms integration 1X gain.
    //     100ms integration 4X gain.
    //     100ms integration at 48X gain.
    //     400ms at 48X gain.
    //     400ms at 96X gain.
    //
    // Currently, this sequence is determined by inspection of the graphs
    // produced by testoutput/plot_luxcalc.R.

    uint8_t measrate = sensor_read_reg(REG_ALS_MEAS_RATE);

    sensor_reading r;
    for (int i = 0; i < 4; ++i) { // four retries if we get an extreme reading
        sensor_write_reg(REG_ALS_MEAS_RATE, (measrate & ~ITIME_MASK) | ITIME_100);
        delay_ms(110); // don't poll the sensor until it's likely to be ready (saves i2c current)
        sensor_wait_till_ready();
        r = sensor_get_reading();

        //SEGGER_RTT_printf(0, "Initial reading %u %u\n", r.chan0, r.chan1);

        if (r.chan0 < 12000) {
            //SEGGER_RTT_printf(0, "DOWN 1\n");
            sensor_write_reg(REG_ALS_MEAS_RATE, (measrate & ~ITIME_MASK) | ITIME_100);
            sensor_turn_on(GAIN_4X);
            delay_ms(110);
            sensor_wait_till_ready();
            r = sensor_get_reading();
            if (r.chan0 < 2000) {
                //SEGGER_RTT_printf(0, "DOWN 2\n");
                sensor_write_reg(REG_ALS_MEAS_RATE, (measrate & ~ITIME_MASK) | ITIME_100);
                sensor_turn_on(GAIN_48X);
                delay_ms(110);
                sensor_wait_till_ready();
                r = sensor_get_reading();
                if (r.chan0 < 10000) {
                    //SEGGER_RTT_printf(0, "DOWN 3\n");
                    sensor_write_reg(REG_ALS_MEAS_RATE, (measrate & ~ITIME_MASK) | ITIME_400);
                    delay_ms(420);
                    sensor_wait_till_ready();
                    r = sensor_get_reading();
                    if (r.chan0 < 20000) {
                        //SEGGER_RTT_printf(0, "DOWN 4\n");
                        sensor_write_reg(REG_ALS_MEAS_RATE, (measrate & ~ITIME_MASK) | ITIME_400);
                        sensor_turn_on(GAIN_96X);
                        delay_ms(420);
                        sensor_wait_till_ready();
                        r = sensor_get_reading();
                        *gain = 96;
                        *itime = 400;
                    } else {
                        *gain = 48;
                        *itime = 400;
                    }
                } else {
                    *gain = 48;
                    *itime = 100;
                }
            } else {
                *gain = 4;
                *itime = 100;
            }
        } else {
            *gain = 1;
            *itime = 100;
        }

        if (r.chan0 > 100 && r.chan0 < 65436)
            break;
    }

    return r;
}

void sensor_turn_on(uint8_t gain)
{
    gain &= 0b011100;
    sensor_write_reg(REG_ALS_CONTR, 1 | gain);
}

void sensor_turn_off()
{
    sensor_write_reg(REG_ALS_CONTR, 0);
}

void sensor_wait_till_ready(void)
{
    uint8_t status;
    do {
        status = sensor_read_reg(REG_ALS_STATUS);
        delay_ms(50); // save some current (less i2c communication)
    } while ((status & 0b10000000) || !(status & 0b100));
}