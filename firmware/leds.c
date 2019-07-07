#include <config.h>
#include <em_cmu.h>
#include <em_gpio.h>
#include <em_rtc.h>
#include <em_timer.h>
#include <leds.h>
#include <macroutils.h>
#include <rtc.h>
#include <stdint.h>
#include <time.h>
#include <util.h>

#define M(n) MACROUTILS_CONCAT3(DPIN, LED ## n ## _CAT_DPIN, _GPIO_PORT) ,
static GPIO_Port_TypeDef led_cat_ports[] = {
    LED_FOR_EACH(M)
};
#undef M

#define M(n) MACROUTILS_CONCAT3(DPIN, LED ## n ## _AN_DPIN, _GPIO_PORT) ,
static GPIO_Port_TypeDef led_an_ports[] = {
    LED_FOR_EACH(M)
};
#undef M

#define M(n) MACROUTILS_CONCAT3(DPIN, LED ## n ## _CAT_DPIN, _GPIO_PIN) ,
static const uint8_t led_cat_pins[] = {
    LED_FOR_EACH(M)
};
#undef M

#define M(n) MACROUTILS_CONCAT3(DPIN, LED ## n ## _AN_DPIN, _GPIO_PIN) ,
static const uint8_t led_an_pins[] = {
    LED_FOR_EACH(M)
};
#undef M

#define M(n) MACROUTILS_CONCAT3(DPIN, LED ## n ## _CAT_DPIN, _TIMER) ,
static TIMER_TypeDef *led_cat_timer[] = {
    LED_FOR_EACH(M)
};
#undef M

#define M(n) MACROUTILS_CONCAT3(DPIN, LED ## n ## _CAT_DPIN, _CHAN) ,
static const uint8_t led_cat_chan[] = {
    LED_FOR_EACH(M)
};
#undef M

#define M(n) MACROUTILS_CONCAT3(DPIN, LED ## n ## _CAT_DPIN, _ROUTE) ,
static const uint32_t led_cat_route[] = {
    LED_FOR_EACH(M)
};
#undef M

#define M(n) MACROUTILS_CONCAT3(DPIN, LED ## n ## _CAT_DPIN, _LOCATION) ,
static const uint32_t led_cat_location[] = {
    LED_FOR_EACH(M)
};
#undef M

#define M(n) MACROUTILS_CONCAT3(DPIN, LED ## n ## _CAT_DPIN, _CLOCK) ,
static const CMU_Clock_TypeDef led_cat_clock[] = {
    LED_FOR_EACH(M)
};
#undef M

static const uint32_t COUNT = 50;
static const uint32_t ONE_LED_DUTY_CYCLE = 1;

static unsigned normalize_led_number(unsigned n)
{
    // Make sure n is positive first, as result of % with negative operand is
    // implementation-defined. Ok to use a loop, as this should never be called
    // with a very large negative value.
    while (n < 0)
        n += LED_N;

    n %= LED_N;

    return n;
}

static void led_on_with_dc(unsigned n, uint32_t duty_cycle)
{
    GPIO_Port_TypeDef cat_port = led_cat_ports[n];
    int cat_pin = led_cat_pins[n];
    int an_port = led_an_ports[n];
    int an_pin = led_an_pins[n];
    TIMER_TypeDef *cat_timer = led_cat_timer[n];
    int cat_chan = led_cat_chan[n];
    uint32_t cat_route = led_cat_route[n];
    uint32_t cat_location = led_cat_location[n];
    CMU_Clock_TypeDef cat_clock = led_cat_clock[n];

    GPIO_PinModeSet(cat_port, cat_pin, gpioModePushPull, 1);
    GPIO_PinModeSet(an_port, an_pin, gpioModePushPull, 1);
    CMU_ClockEnable(cat_clock, true);
    TIMER_InitCC_TypeDef timerCCInit = TIMER_INITCC_DEFAULT;
    timerCCInit.mode = timerCCModePWM;
    timerCCInit.cmoa = timerOutputActionToggle;
    TIMER_InitCC(cat_timer, cat_chan, &timerCCInit);
    cat_timer->ROUTE = (cat_route | cat_location);
    TIMER_TopSet(cat_timer, COUNT);
    TIMER_CompareBufSet(cat_timer, cat_chan, duty_cycle); // duty cycle
    TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
    timerInit.prescale = timerPrescale256;
    TIMER_Init(cat_timer, &timerInit);
}

static void led_off(unsigned n)
{
    n = normalize_led_number(n);

    GPIO_Port_TypeDef cat_port = led_cat_ports[n];
    int cat_pin = led_cat_pins[n];
    int an_port = led_an_ports[n];
    int an_pin = led_an_pins[n];
    CMU_Clock_TypeDef cat_clock = led_cat_clock[n];

    GPIO_PinModeSet(cat_port, cat_pin, gpioModeInput, 0);
    GPIO_PinModeSet(an_port, an_pin, gpioModeInput, 0);

    CMU_ClockEnable(cat_clock, false);
}

void led_on(unsigned n)
{
    led_on_with_dc(normalize_led_number(n), COUNT - ONE_LED_DUTY_CYCLE);
}

static void turnoff()
{
    CMU_ClockEnable(cmuClock_TIMER0, false);
    CMU_ClockEnable(cmuClock_TIMER1, false);

#define M(n) GPIO_PinModeSet(DPIN ## n ## _GPIO_PORT, DPIN ## n ## _GPIO_PIN, gpioModeInput, 0);
    DPIN_FOR_EACH(M)
#undef M
}

static uint32_t orig_mask;
static uint32_t current_mask;
static uint32_t current_mask_n;
static uint32_t led_duty_cycle;

static void led_rtc_count_callback()
{
    static unsigned last_on;
    //SEGGER_RTT_printf(0, "Here!\n");

    // Find first set bit.
    for (;;) {
        for (; current_mask != 0 && (current_mask & 1) == 0; current_mask >>= 1, ++current_mask_n)
            ;

        if (current_mask != 0)
            break;

        current_mask = orig_mask;
        current_mask_n = 0;
    }

    led_off(last_on);

    //SEGGER_RTT_printf(0, "Turn on %u\n", current_mask_n);
    led_on_with_dc(current_mask_n, led_duty_cycle);

    last_on = current_mask_n;

    current_mask >>= 1;
    ++current_mask_n;
}

void leds_on(uint32_t mask)
{
    mask &= 0b111111111111111111111111111; // there are 27 leds

    if (mask == 0)
        return;

    turnoff();

    CMU_ClockDivSet(cmuClock_RTC, cmuClkDiv_1);

    uint32_t leds_on = popcount(mask);
    led_duty_cycle = COUNT - (ONE_LED_DUTY_CYCLE * (uint32_t)leds_on);

    orig_mask = mask;
    current_mask = mask;

    set_rtc_interrupt_handler(led_rtc_count_callback);

    RTC_Init_TypeDef init = {
        true, // Start counting when initialization is done
        false, // Enable updating during debug halt.
        true  // Restart counting from 0 when reaching COMP0.
    };

    RTC_CompareSet(0, RTC_RAW_FREQ / LED_REFRESH_RATE_HZ);

    RTC_IntEnable(RTC_IEN_COMP0);
    NVIC_EnableIRQ(RTC_IRQn);

    RTC_Init(&init);
}

void led_fully_on(unsigned n)
{
    --n;
    n %= LED_N;
    uint8_t nn = (uint8_t)n;

    GPIO_Port_TypeDef cat_port = led_cat_ports[nn];
    int cat_pin = led_cat_pins[nn];
    int an_port = led_an_ports[nn];
    int an_pin = led_an_pins[nn];

    GPIO_PinModeSet(cat_port, cat_pin, gpioModePushPull, 0);
    GPIO_PinModeSet(an_port, an_pin, gpioModePushPull, 1);
}

void leds_all_off()
{
    if (orig_mask != 0) {
        RTC_Enable(false);
        orig_mask = 0;
        CMU_ClockDivSet(cmuClock_RTC, RTC_CMU_CLK_DIV);
    }

    turnoff();
}