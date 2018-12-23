#include <em_gpio.h>
#include <em_i2c.h>
#include <rtt.h>
#include <sensor.h>

#define SENSOR_I2C_PORT      gpioPortE
#define SENSOR_I2C_SDA_PIN   12
#define SENSOR_I2C_SCL_PIN   13

#define SENSOR_I2C_ADDR      (0x29 << 1)

#define SENSOR_INT_PORT       gpioPortF
#define SENSOR_INT_PIN        21

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

void sensor_init()
{
    SEGGER_RTT_printf(0, "Starting sensor initialization..\n");

    GPIO_PinModeSet(SENSOR_I2C_PORT, SENSOR_I2C_SCL_PIN, gpioModeWiredAndFilter, 1); // configure SCL pin as open drain output
    GPIO_PinModeSet(SENSOR_I2C_PORT, SENSOR_I2C_SDA_PIN, gpioModeWiredAndFilter, 1); // configure SDA pin as open drain output  

    I2C0->ROUTE = I2C_ROUTE_SDAPEN | I2C_ROUTE_SCLPEN | (6 << _I2C_ROUTE_LOCATION_SHIFT);
    I2C0->CTRL = I2C_CTRL_AUTOACK | I2C_CTRL_AUTOSN;

    for (unsigned i = 0; i < 9; i++)
    {
        // Alex: This is a comment from some code I copied from somewhere ages
        // ago.
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
}

void sensor_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t wbuf[] = { reg, val };
    I2C_TransferSeq_TypeDef i2c_transfer = {
        .addr = SENSOR_I2C_ADDR,
        .flags = I2C_FLAG_WRITE_WRITE,
        .buf[0].data = wbuf,
        .buf[0].len = sizeof(wbuf)/sizeof(wbuf[0])
    };
    SEGGER_RTT_printf(0, "Starting transfer..\n");
    int status = I2C_TransferInit(I2C0, &i2c_transfer);
    while (status == i2cTransferInProgress)
        status = I2C_Transfer(I2C0);
    SEGGER_RTT_printf(0, "Ending transfer..\n");
    print_stat(status);
}

void sensor_turn_on()
{
    sensor_write_reg(REG_ALS_CONTR, 0b00000001);
}