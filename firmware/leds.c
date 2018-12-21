#include <em_cmu.h>
#include <em_gpio.h>
#include <em_timer.h>
#include <stdint.h>
#include <leds.h>

#define mcat3_(a, b, c) a ## b ## c
#define mcat3(a, b, c) mcat3_(a, b, c)

#define M(n) mcat3(DPIN, LED ## n ## _CAT_DPIN, _GPIO_PORT) ,
static GPIO_Port_TypeDef led_cat_ports[] = {
    LED_FOR_EACH(M)
};
#undef M

#define M(n) mcat3(DPIN, LED ## n ## _AN_DPIN, _GPIO_PORT) ,
static GPIO_Port_TypeDef led_an_ports[] = {
    LED_FOR_EACH(M)
};
#undef M

#define M(n) mcat3(DPIN, LED ## n ## _CAT_DPIN, _GPIO_PIN) ,
static const uint8_t led_cat_pins[] = {
    LED_FOR_EACH(M)
};
#undef M

#define M(n) mcat3(DPIN, LED ## n ## _AN_DPIN, _GPIO_PIN) ,
static const uint8_t led_an_pins[] = {
    LED_FOR_EACH(M)
};
#undef M

#define M(n) mcat3(DPIN, LED ## n ## _CAT_DPIN, _TIMER) ,
static TIMER_TypeDef *led_cat_timer[] = {  
    LED_FOR_EACH(M)
};
#undef M

#define M(n) mcat3(DPIN, LED ## n ## _CAT_DPIN, _CHAN) ,
static const uint8_t led_cat_chan[] = {  
    LED_FOR_EACH(M)
};
#undef M

#define M(n) mcat3(DPIN, LED ## n ## _CAT_DPIN, _ROUTE) ,
static const uint32_t led_cat_route[] = {  
    LED_FOR_EACH(M)
};
#undef M

#define M(n) mcat3(DPIN, LED ## n ## _CAT_DPIN, _LOCATION) ,
static const uint8_t led_cat_location[] = {  
    LED_FOR_EACH(M)
};
#undef M

#define M(n) mcat3(DPIN, LED ## n ## _CAT_DPIN, _CLOCK) ,
static const CMU_Clock_TypeDef led_cat_clock[] = {  
    LED_FOR_EACH(M)
};
#undef M

void led_on(unsigned n)
{
    --n; // go to zero indexing
    n %= 27;
    uint8_t nn = (uint8_t)n;

    GPIO_Port_TypeDef cat_port = led_cat_ports[nn];
    int cat_pin = led_cat_pins[nn];
    int an_port = led_an_ports[nn];
    int an_pin = led_an_pins[nn];
    TIMER_TypeDef *cat_timer = led_cat_timer[nn];
    int cat_chan = led_cat_chan[nn];
    uint32_t cat_route = led_cat_route[nn];
    int cat_location = led_cat_location[nn];
    CMU_Clock_TypeDef cat_clock = led_cat_clock[nn];

    CMU_ClockEnable(cat_clock, true);

    GPIO_PinModeSet(cat_port, cat_pin, gpioModePushPull, 0);
    GPIO_PinModeSet(an_port, an_pin, gpioModePushPull, 0);
    TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;
    timerCCInit.mode = timerCCModePWM;
    timerCCInit.cmoa = timerOutputActionToggle;
    TIMER_InitCC(cat_timer, cat_chan, &timerCCInit);
    cat_timer->ROUTE |= (cat_route | cat_location);
    TIMER_TopSet(cat_timer, 100);
    TIMER_CompareBufSet(cat_timer, cat_chan, 1); // duty cycle
    TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
    timerInit.prescale = timerPrescale256;
    TIMER_Init(cat_timer, &timerInit);
}

void leds_all_off()
{
    CMU_ClockEnable(cmuClock_TIMER0, false);
    CMU_ClockEnable(cmuClock_TIMER1, false);

#define M(n) GPIO_PinModeSet(DPIN ## n ## _GPIO_PORT, DPIN ## n ## _GPIO_PIN, gpioModeInput, 0);
    DPIN_FOR_EACH(M)
#undef M
}