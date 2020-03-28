#include <tests/include.h>

int test_main()
{
#define M(n) MACROUTILS_CONCAT3(DPIN, LED ## n ## _CAT_DPIN, _GPIO_PORT) ,
static GPIO_Port_TypeDef led_cat_ports[] = {
    LED_FOR_EACH(M)
};
#undef M

#define M(n) MACROUTILS_CONCAT3(DPIN, LED ## n ## _CAT_DPIN, _GPIO_PIN) ,
static const uint8_t led_cat_pins[] = {
    LED_FOR_EACH(M)
};
#undef M

    for (;;) {
        for (int i = 0; i < sizeof(led_cat_ports)/sizeof(led_cat_ports[0]); ++i) {
            GPIO_Port_TypeDef port1 = led_cat_ports[i];
            int pin1 = led_cat_pins[i];

            for (int j = 0; j < sizeof(led_cat_pins)/sizeof(led_cat_pins[0]); ++j) {
                if (i == j)
                    continue;

                GPIO_Port_TypeDef port2 = led_cat_ports[j];
                int pin2 = led_cat_pins[j];

                for (int k = 0; k < sizeof(led_cat_ports)/sizeof(led_cat_ports[0]); ++k) {
                    if (k == i || k == j)
                        continue;

                    GPIO_PinModeSet(led_cat_ports[k], led_cat_pins[k], gpioModeInput, 1);
                }

                SEGGER_RTT_printf(0, "PIN %u ; PIN %u\n", i, j);

                GPIO_PinModeSet(port1, pin1, gpioModePushPull, 1);
                GPIO_PinModeSet(port2, pin2, gpioModePushPull, 0);

                delay_ms(1000);

                GPIO_PinModeSet(port1, pin1, gpioModePushPull, 0);
                GPIO_PinModeSet(port2, pin2, gpioModePushPull, 1);

                delay_ms(1000);
            }
        }
    }

    return 0;
}