#include <em_cmu.h>
#include <em_gpio.h>
#include <em_i2c.h>
#include <stdint.h>
#include <tempsensor.h>

#define TEMPSENSOR_I2C_PORT    gpioPortE
#define TEMPSENSOR_I2C_SDA_PIN 12
#define TEMPSENSOR_I2C_SCL_PIN 13
#define TEMPSENSOR_I2C_ADDR    (0b1001000 << 1)

void tempsensor_init()
{
    SEGGER_RTT_printf(0, "Starting temperature sensor initialization..\n");

    CMU_ClockEnable(cmuClock_I2C0, true);

    GPIO_PinModeSet(TEMPSENSOR_I2C_PORT, TEMPSENSOR_I2C_SCL_PIN, gpioModeWiredAndFilter, 1); // configure SCL pin as open drain output
    GPIO_PinModeSet(TEMPSENSOR_I2C_PORT, TEMPSENSOR_I2C_SDA_PIN, gpioModeWiredAndFilter, 1); // configure SDA pin as open drain output

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
        GPIO_PinModeSet(TEMPSENSOR_I2C_PORT, TEMPSENSOR_I2C_SCL_PIN, gpioModeWiredAndFilter, 0);
        GPIO_PinModeSet(TEMPSENSOR_I2C_PORT, TEMPSENSOR_I2C_SCL_PIN, gpioModeWiredAndFilter, 1);
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

    SEGGER_RTT_printf(0, "Temperature sensor initialization complete.\n");
}

static uint16_t read_reg16(uint16_t addr)
{
    uint8_t rbuf[2];
    I2C_TransferSeq_TypeDef i2c_transfer = {
        .addr = addr,
        .flags = I2C_FLAG_READ,
        .buf[0].data = rbuf,
        .buf[0].len = sizeof(rbuf)/sizeof(rbuf[0])
    };
    int status = I2C_TransferInit(I2C0, &i2c_transfer);
    while (status == i2cTransferInProgress)
        status = I2C_Transfer(I2C0);
    return ((uint16_t)(rbuf[0]) << 8) | ((uint16_t)rbuf[1]);
}

int32_t tempsensor_get_reading(delay_func delayf)
{
    // Default precision is 9 bits (0.5C). That's fine for us,
    // so we don't change it.

    int16_t reading = 0;
    int tries = 0;
    do {
        delayf(25);
        reading = read_reg16(TEMPSENSOR_I2C_ADDR);
    } while (reading == 0 && ++tries < 11);

    int32_t reading32 = (int32_t)reading;

    return reading;
}