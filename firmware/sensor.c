#include <em_cmu.h>
#include <em_gpio.h>
#include <em_i2c.h>
#include <rtt.h>
#include <sensor.h>
#include <time.h>
#include <units.h>
#include <util.h>

#define SENSOR_I2C_PORT       gpioPortE
#define SENSOR_I2C_SDA_PIN    12
#define SENSOR_I2C_SCL_PIN    13

#define SENSOR_I2C_ADDR       (0x29 << 1)

// https://github.com/alibaba/AliOS-Things/blob/8cae6d447d331989ede14d28c0ec189f2aa2b3c7/device/sensor/drv/drv_als_liteon_ltr303.c

void sensor_init()
{
    SEGGER_RTT_printf(0, "Starting light sensor initialization..\n");

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

    SEGGER_RTT_printf(0, "Light sensor initialization complete.\n");
}

__attribute__((unused)) static void print_stat(int status)
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

void sensor_write_reg(uint16_t addr, uint8_t reg, uint8_t val)
{
    uint8_t wbuf[] = { reg, val };
    I2C_TransferSeq_TypeDef i2c_transfer = {
        .addr = addr,
        .flags = I2C_FLAG_WRITE,
        .buf[0].data = wbuf,
        .buf[0].len = 2
    };
    int status = I2C_TransferInit(I2C0, &i2c_transfer);
    while (status != i2cTransferDone) {
        status = I2C_Transfer(I2C0);
    }
}

uint8_t sensor_read_reg(uint16_t addr, uint8_t reg)
{
    return (uint8_t)(sensor_read_reg16(addr, reg) & 0xFF);
}

uint16_t sensor_read_reg16(uint16_t addr, uint8_t reg)
{
    uint8_t wbuf[1];
    wbuf[0] = reg;
    uint8_t rbuf[2];
    I2C_TransferSeq_TypeDef i2c_transfer = {
        .addr = addr,
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
    r.chan1 = sensor_read_reg16(SENSOR_I2C_ADDR, REG_ALS_DATA_CH1_0);
    r.chan0 = sensor_read_reg16(SENSOR_I2C_ADDR, REG_ALS_DATA_CH0_0);

    return r;
}

bool sensor_has_valid_data()
{
    uint8_t status = sensor_read_reg(SENSOR_I2C_ADDR, REG_ALS_STATUS);
    return !(status & 0b10000000);
}

static inline int32_t subtract_margin(int32_t max_lux)
{
    return (int32_t)((int64_t)max_lux * 85LL / 100LL);
}

// def output_tables():
//     def f(c0,c1,a,b,gain,itime):
//         return (a * c0 + b * c1) / gain / itime
//     print("static const int LUX_VAL_DOWN_SHIFT = 5;")
//     rati = 1
//     for rat in [(1.7743, 1.1059, 0.44), (4.2785, -1.9548, 0.45), (0.5926, 0.1185, 0.84)]:
//         print("static const uint16_t RAT_%i_MAX_LUX_VALS[8][6] = {" % rati)
//         for itime in reversed([50.0, 100.0, 150.0, 200.0, 250.0, 300.0, 350.0, 400.0]):
//             print("    {", end='')
//             for gain in reversed([1.0, 2.0, 4.0, 8.0, 48.0, 96.0]):
//                 c0, c1 = 0.0, 0.0
//                 if rat[2] < 0.5:
//                     c0 = 65535.0
//                     c1 = -((65535.0 * rat[2])/(rat[2] - 1.0))
//                 else:
//                     c1 = 65535.0
//                     c0 = c1 * (1.0/rat[2] - 1.0)
//                 maxlux = f(c0, c1, rat[0], rat[1], gain, itime/100.0)
//                 print("%i," % (int(round(maxlux)) >> 5), end='')
//             print("},")
//         rati += 1
//         print("};")
static const int LUX_VAL_DOWN_SHIFT = 5;
static const uint16_t RAT_1_MAX_LUX_VALS[8][6] = {
    {14,28,169,338,676,1353,},
    {16,32,193,386,773,1546,},
    {18,37,225,451,902,1804,},
    {22,45,270,541,1082,2165,},
    {28,56,338,676,1353,2706,},
    {37,75,451,902,1804,3608,},
    {56,112,676,1353,2706,5413,},
    {112,225,1353,2706,5413,10826,},
};
static const uint16_t RAT_2_MAX_LUX_VALS[8][6] = {
    {14,28,171,342,685,1371,},
    {16,32,195,391,783,1567,},
    {19,38,228,457,914,1828,},
    {22,45,274,548,1097,2194,},
    {28,57,342,685,1371,2743,},
    {38,76,457,914,1828,3657,},
    {57,114,685,1371,2743,5486,},
    {114,228,1371,2743,5486,10973,},
};
static const uint16_t RAT_3_MAX_LUX_VALS[8][6] = {
    {1,2,14,29,59,118,},
    {1,2,16,33,67,135,},
    {1,3,19,39,78,157,},
    {1,3,23,47,94,189,},
    {2,4,29,59,118,236,},
    {3,6,39,78,157,315,},
    {4,9,59,118,236,473,},
    {9,19,118,236,473,947,},
};

static void get_mode(sensor_reading r, int32_t *itime, int *itime_key, int32_t *gain, int *gain_key)
{
    int32_t lux50 = sensor_reading_to_lux(r, 1, 50);
    SEGGER_RTT_printf(0, "LUX50 = %u/%u = %u (c0=%u,c1=%u)\n", lux50, (1<<EV_BPS), lux50/(1<<EV_BPS), r.chan0, r.chan1);
    int32_t rat = ((int32_t)r.chan1 * 100) / ((int32_t)r.chan0 + (int32_t)r.chan1);

    const uint16_t *tab;
    if (rat < 45)
        tab = RAT_1_MAX_LUX_VALS;
    else if (rat < 64)
        tab = RAT_2_MAX_LUX_VALS;
    else
        tab = RAT_3_MAX_LUX_VALS;
    
    // use longest integration time possible.
    // could use binary search here, but performance of linear search is fine
    int i, j;
    for (i = 0; i < 8; ++i) {
        for (j = 0; j < 6; ++j) {
            uint32_t maxlux = tab[i*8+j] << LUX_VAL_DOWN_SHIFT << EV_BPS;
            if (lux50 < maxlux / 2)
                goto breakout;
        }
    }

breakout:
    switch (i) {
        case 7:
            *itime = 50, *itime_key = ITIME_50;
            break;
        case 6:
            *itime = 100, *itime_key = ITIME_100;
            break;
        case 5:
            *itime = 150, *itime_key = ITIME_150;
            break;
        case 4:
            *itime = 200, *itime_key = ITIME_200;
            break;
        case 3:
            *itime = 250, *itime_key = ITIME_250;
            break;
        case 2:
            *itime = 300, *itime_key = ITIME_300;
            break;
        case 1:
            *itime = 350, *itime_key = ITIME_350;
            break;
        case 0:
            *itime = 400, *itime_key = ITIME_400;
            break;
        default:
            *itime = 50, *itime_key = ITIME_50;
            break;
    }

    switch (j) {
        case 5:
            *gain = 1, *gain_key = GAIN_1X;
            break;
        case 4:
            *gain = 2, *gain_key = GAIN_2X;
            break;
        case 3:
            *gain = 4, *gain_key = GAIN_4X;
            break;
        case 2:
            *gain = 8, *gain_key = GAIN_8X;
            break;
        case 1:
            *gain = 48, *gain_key = GAIN_48X;
            break;
        case 0:
            *gain = 96, *gain_key = GAIN_96X;
            break;
        default:
            *gain = 1, *gain_key = GAIN_1X;
            break;
    }
}

sensor_reading sensor_get_reading_auto(delay_func delayf, int32_t *gain, int32_t *itime)
{
    // We first do a quick reading at 50ms integ time to get an idea
    // of the light level, and then follow up with a reading at 250ms
    // integ time with the appropriate gain setting, or at very low
    // light levels, 400ms at 48X gain. The latter should be a little
    // less noisy than 250ms at 96X gain, and has an almost identical
    // range.
    //
    // As far as I can tell at the moment, the entire sensitivity
    // range of the sensor can be covered using a 250ms integ time and
    // adjusting the gain. Setting the gain to a shorter period takes
    // the max calculated lux values above the maximum specified value
    // on the datasheet (64k lux), so I assume that any such results
    // would be bogus.

    sensor_standby();
    uint8_t measrate = sensor_read_reg(SENSOR_I2C_ADDR, REG_ALS_MEAS_RATE);
    sensor_write_reg(SENSOR_I2C_ADDR, REG_ALS_MEAS_RATE, (measrate & ~ITIME_MASK) | ITIME_50);
    sensor_turn_on(GAIN_1X);
    delayf(65); // don't poll the sensor until it's likely to be ready (saves i2c current)
    sensor_wait_till_ready(delayf);
    sensor_reading r = sensor_get_reading();

    int itime_key, gain_key;
    get_mode(r, itime, &itime_key, gain, &gain_key);

    sensor_standby();
    sensor_turn_on(gain_key);
    sensor_write_reg(SENSOR_I2C_ADDR, REG_ALS_MEAS_RATE, (measrate & ~ITIME_MASK) | itime_key);
    delayf((*itime) * 6 / 5);

    sensor_wait_till_ready(delayf);
    return sensor_get_reading();
}

void sensor_turn_on(uint8_t gain)
{
    gain &= 0b011100;
    sensor_write_reg(SENSOR_I2C_ADDR, REG_ALS_CONTR, 1 | gain);
}

void sensor_standby()
{
    sensor_write_reg(SENSOR_I2C_ADDR, REG_ALS_CONTR, 0);
}

void sensor_wait_till_ready(delay_func delayf)
{
    uint8_t status;
    do {
        status = sensor_read_reg(SENSOR_I2C_ADDR, REG_ALS_STATUS);
        delayf(10); // save some current (less i2c communication)
    } while (!(status & 0b100));
}

