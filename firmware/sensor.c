#include <em_cmu.h>
#include <em_gpio.h>
#include <em_i2c.h>
#include <rtt.h>
#include <sensor.h>

#define SENSOR_I2C_PORT       gpioPortE
#define SENSOR_I2C_SDA_PIN    12
#define SENSOR_I2C_SCL_PIN    13

#define SENSOR_I2C_ADDR       (0x29 << 1)

#define SENSOR_INT_PORT       gpioPortF
#define SENSOR_INT_PIN        1

void sensor_init()
{
    SEGGER_RTT_printf(0, "Starting sensor initialization..\n");

    CMU_ClockEnable(cmuClock_I2C0, true);

    // This is now the SWDIO pin, gotta be careful how we configure it while
    // programmer is connected.
//    GPIO_PinModeSet(SENSOR_INT_PORT, SENSOR_INT_PIN, gpioModeWiredAndPullUp, 1);

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

    I2C_Init_TypeDef i2c_init = /*I2C_INIT_DEFAULT;*/ {
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
        .buf[0].len = sizeof(wbuf)/sizeof(wbuf[0]),
    };
    SEGGER_RTT_printf(0, "Starting transfer..\n");
    int status = I2C_TransferInit(I2C0, &i2c_transfer);
    print_stat(status);
    while (status == i2cTransferInProgress) {
        status = I2C_Transfer(I2C0);
    }
    print_stat(status);
    SEGGER_RTT_printf(0, "Ending transfer..\n");
    print_stat(status);
}

uint8_t sensor_read_reg(uint8_t reg)
{
    uint8_t wbuf[1];
    wbuf[0] = reg;
    uint8_t rbuf[1];
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
    print_stat(status);
    return rbuf[0];
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
    uint8_t stat;
    while (! ((stat = sensor_read_reg(REG_ALS_STATUS)) & 0b100)) {
        //SEGGER_RTT_printf(0, "Status %u\n", stat);
    }

    sensor_reading r;
    r.chan1 = sensor_read_reg16(REG_ALS_DATA_CH1_0);
    r.chan0 = sensor_read_reg16(REG_ALS_DATA_CH0_0);

    return r;
}

void sensor_turn_on()
{
    sensor_write_reg(REG_ALS_CONTR, 0b00000011);
}