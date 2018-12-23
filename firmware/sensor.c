#include <em_gpio.h>
#include <em_i2c.h>
#include <sensor.h>

#define I2C_PORT    gpioPortE
#define I2C_SDA_PIN 12
#define I2C_SCL_PIN 13

#define SENSOR_I2C_ADDR (0x29 << 1)

void sensor_init()
{
    GPIO_PinModeSet(I2C_PORT, I2C_SCL_PIN, gpioModeWiredAndFilter, 1); // configure SCL pin as open drain output
    GPIO_PinModeSet(I2C_PORT, I2C_SDA_PIN, gpioModeWiredAndFilter, 1); // configure SDA pin as open drain output  

    I2C0->ROUTE = I2C_ROUTE_SDAPEN | I2C_ROUTE_SCLPEN | (1 << _I2C_ROUTE_LOCATION_SHIFT);
    I2C0->CTRL = I2C_CTRL_AUTOACK | I2C_CTRL_AUTOSN;

    for (unsigned i = 0; i < 9; i++)
    {
        /*
         * TBD: Seems to be clocking at appr 80kHz-120kHz depending on compiler
         * optimization when running at 14MHz. A bit high for standard mode devices,
         * but DVK only has fast mode devices. Need however to add some time
         * measurement in order to not be dependable on frequency and code executed.
        */
        GPIO_PinModeSet(I2C_PORT, I2C_SCL_PIN, gpioModeWiredAndFilter, 0);
        GPIO_PinModeSet(I2C_PORT, I2C_SCL_PIN, gpioModeWiredAndFilter, 1);
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
}