#include <tests/include.h>

int test_main()
{
    // Turn on the LDO to power up the sensor.
    GPIO_PinModeSet(REGMODE_PORT, REGMODE_PIN, gpioModePushPull, 1);
    SEGGER_RTT_printf(0, "LDO turned on\n");
    delay_ms(100); // make sure LDO has time to start up and sensor has time to
                   // power up
    tempsensor_init();
    delay_ms(100);

    for (;;) {
        int32_t reading = tempsensor_get_reading();
        SEGGER_RTT_printf(0, "TEMP READING %s%u, %s%uC\n", sign_of(reading), iabs(reading), sign_of(reading), iabs(reading >> 8));
        delay_ms(500);
    }

    return 0;
}