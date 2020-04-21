#include <config.h>
#include <em_cmu.h>
#include <em_gpio.h>
#include <em_rtc.h>
#include <em_timer.h>
#include <leds.h>
#include <macroutils.h>
#include <rtc.h>
#include <rtt.h>
#include <state.h>
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

static const uint32_t COUNT = 50;
static const int32_t DUTY_CYCLE_MIN = 3;
static const int32_t DUTY_CYCLE_MAX = 90;

static const int CYCLES_PER_THROB_STEP = RTC_RAW_FREQ / 60;

// from math import *
// [round(sin(x/30*2*pi)*20) for x in range(0, 30)]
static const int8_t throb_progression[] = { 0, 2, 5, 7, 9, 10, 11, 12, 12, 11, 10, 9, 7, 5, 2, 0, -2, -5, -7, -9, -10, -11, -12, -12, -11, -10, -9, -7, -5, -2 };
static const int THROB_MAG = 20;

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

static uint32_t duty_cycle_for_ev(int32_t ev)
{
    // dc = (37/5)ev - 32

    int32_t dc = (37 * ev)/5 - 32;
    if (dc < DUTY_CYCLE_MIN)
        return DUTY_CYCLE_MIN;
    if (dc > DUTY_CYCLE_MAX)
        return DUTY_CYCLE_MAX;
    return (uint32_t)dc;
}

static uint32_t get_duty_cycle()
{
    uint32_t duty_cycle = duty_cycle_for_ev(g_state.led_brightness_ev_ref);
    if (duty_cycle < 1)
        duty_cycle = 1;
    else if (duty_cycle > DUTY_CYCLE_MAX)
        duty_cycle = DUTY_CYCLE_MAX;

    duty_cycle = DUTY_CYCLE_MAX;

    return COUNT - duty_cycle;
}


static void led_on_with_dc(unsigned n, uint32_t duty_cycle)
{
    GPIO_Port_TypeDef cat_port = led_cat_ports[n];
    int cat_pin = led_cat_pins[n];
    GPIO_Port_TypeDef an_port = led_an_ports[n];
    int an_pin = led_an_pins[n];
    TIMER_TypeDef *cat_timer = led_cat_timer[n];
    int cat_chan = led_cat_chan[n];
    uint32_t cat_route = led_cat_route[n];
    uint32_t cat_location = led_cat_location[n];
    GPIO_PinModeSet(cat_port, cat_pin, gpioModePushPull, 1);
    GPIO_PinModeSet(an_port, an_pin, gpioModePushPull, 1);
    cat_timer->ROUTE = (cat_route | cat_location);
    TIMER_CompareBufSet(cat_timer, cat_chan, duty_cycle); // duty cycle
}

static void led_off(unsigned n)
{
    GPIO_Port_TypeDef cat_port = led_cat_ports[n];
    int cat_pin = led_cat_pins[n];
    GPIO_Port_TypeDef an_port = led_an_ports[n];
    int an_pin = led_an_pins[n];
    TIMER_TypeDef *cat_timer = led_cat_timer[n];

    cat_timer->ROUTE = 0;
    GPIO_PinModeSet(cat_port, cat_pin, gpioModeInput, 1);
    GPIO_PinModeSet(an_port, an_pin, gpioModeInput, 1);
}

void led_on(unsigned n)
{
    //SEGGER_RTT_printf(0, "Turning on LED %u (= %u)\n", n, normalize_led_number(n));
    uint32_t duty_cycle = get_duty_cycle();
    led_on_with_dc(normalize_led_number(n), duty_cycle);
}

static void turnoff()
{
#define M(n) GPIO_PinModeSet(DPIN ## n ## _GPIO_PORT, DPIN ## n ## _GPIO_PIN, gpioModeInput, 0);
    DPIN_FOR_EACH(M)
#undef M

    TIMER_Enable(TIMER0, false);
    TIMER_Enable(TIMER1, false);
    CMU_ClockEnable(cmuClock_TIMER0, false);
    CMU_ClockEnable(cmuClock_TIMER1, false);
}

static volatile uint32_t orig_mask;
static volatile uint32_t current_mask;
static volatile uint32_t throb_mask;
static volatile uint32_t current_mask_n;
static volatile int current_throb_index;
static volatile int current_throb_cycles;
static volatile int next_throb_cycles;

void set_led_throb_mask(uint32_t mask)
{
    throb_mask = mask;
}

volatile uint32_t leds_on_for_cycles;

static int last_led_on;
static void led_rtc_count_callback()
{
    leds_on_for_cycles += RTC_RAW_FREQ / LED_REFRESH_RATE_HZ;

    for (;;) {
        current_mask >>= 1;
        ++current_mask_n;
        if (current_mask & 1)
            break;
        if (current_mask == 0) {
            current_mask = orig_mask;
            current_mask_n = 0;
            if (current_mask & 1)
                break;
        }
    }

    if (leds_on_for_cycles >= next_throb_cycles) {
        current_throb_index = (current_throb_index + 1) % (sizeof(throb_progression)/sizeof(throb_progression[0]));
        next_throb_cycles = leds_on_for_cycles + CYCLES_PER_THROB_STEP;
    }

    int32_t dc = get_duty_cycle();
    if (throb_mask & (1 << current_mask_n)) {
        if (dc - THROB_MAG < DUTY_CYCLE_MIN)
            dc = THROB_MAG + DUTY_CYCLE_MIN;
        else if (dc + THROB_MAG > DUTY_CYCLE_MAX)
            dc = DUTY_CYCLE_MAX - THROB_MAG;
        dc += throb_progression[current_throb_index];
    }

    if (dc < DUTY_CYCLE_MIN)
        dc = DUTY_CYCLE_MIN;
    else if (dc > DUTY_CYCLE_MAX)
        dc = DUTY_CYCLE_MAX;

    if (last_led_on != -1)
        led_off(last_led_on);

    led_on_with_dc(current_mask_n, (uint32_t)dc);

    last_led_on = (int)current_mask_n;
}

static void init_timers()
{
    static TIMER_Init_TypeDef timerInit = TIMER_INIT_DEFAULT;
    timerInit.prescale = timerPrescale256;

    CMU_ClockEnable(cmuClock_TIMER0, true);
    CMU_ClockEnable(cmuClock_TIMER1, true);
    TIMER_Init(TIMER0, &timerInit);
    TIMER_Init(TIMER1, &timerInit);
    TIMER_TopSet(TIMER0, COUNT);
    TIMER_TopSet(TIMER1, COUNT);

    static TIMER_InitCC_TypeDef timerCCInit = {
        timerEventEveryEdge,      /* Event on every capture. */
        timerEdgeRising,          /* Input capture edge on rising edge. */
        timerPRSSELCh0,           /* Not used by default, select PRS channel 0. */
        timerOutputActionNone,    /* No action on underflow. */
        timerOutputActionNone,    /* No action on overflow. */
        timerOutputActionToggle,  /* Action on match. */
        timerCCModePWM,           /* Disable compare/capture channel. */
        false,                    /* Disable filter. */
        false,                    /* Select TIMERnCCx input. */
        false,                    /* Clear output when counter disabled. */
        false                     /* Do not invert output. */
    };

    for (int i = 0; i < 3; ++i) {
        TIMER_InitCC(TIMER0, i, &timerCCInit);
        TIMER_InitCC(TIMER1, i, &timerCCInit);
        TIMER0->ROUTE = 0;
        TIMER1->ROUTE = 0;
    }
}

static bool rtc_has_been_borked_for_led_cycling;

bool rtc_borked_for_led_cycling()
{
    return rtc_has_been_borked_for_led_cycling;
}

void leds_on(uint32_t mask)
{
    static bool leds_on_for_cycles_initialized;

    RTC_Enable(false);

    mask &= (1<<LED_N)-1;

    if (mask == 0)
        return;
    
    last_led_on = -1;

    turnoff();

    set_rtc_clock_div(cmuClkDiv_1);

    orig_mask = mask;
    current_mask = mask;
    current_mask_n = 0;
    current_throb_index = 0;

    add_rtc_interrupt_handler(led_rtc_count_callback);

    RTC_Init_TypeDef init = {
        true,  // Start counting when initialization is done
        false, // Enable updating during debug halt.
        true   // Restart counting from 0 when reaching COMP0.
    };

    RTC_CompareSet(0, RTC_RAW_FREQ / LED_REFRESH_RATE_HZ);

    RTC_IntEnable(RTC_IEN_COMP0);
    NVIC_EnableIRQ(RTC_IRQn);

    if (! leds_on_for_cycles_initialized) {
        leds_on_for_cycles = 0;
        leds_on_for_cycles_initialized = true;
    }
    next_throb_cycles = CYCLES_PER_THROB_STEP;

    RTC_Init(&init);

    init_timers();

    rtc_has_been_borked_for_led_cycling = true;
}

void leds_change_mask(uint32_t mask)
{
    orig_mask = mask;
    current_mask = mask;
    current_mask_n = 0;
}

void led_fully_on(unsigned n)
{
    --n;
    n %= LED_N;
    uint8_t nn = (uint8_t)n;

    GPIO_Port_TypeDef cat_port = led_cat_ports[nn];
    int cat_pin = led_cat_pins[nn];
    GPIO_Port_TypeDef an_port = led_an_ports[nn];
    int an_pin = led_an_pins[nn];

    GPIO_PinModeSet(cat_port, cat_pin, gpioModePushPull, 0);
    GPIO_PinModeSet(an_port, an_pin, gpioModePushPull, 1);
}

void leds_all_off()
{
    turnoff();

    if (rtc_has_been_borked_for_led_cycling)
        RTC_Enable(false);

    orig_mask = 0;

    if (rtc_has_been_borked_for_led_cycling) {
        set_rtc_clock_div(MACROUTILS_CONCAT(cmuClkDiv_, RTC_CLK_DIV));
        RTC_IntDisable(RTC_IEN_COMP0);
        NVIC_DisableIRQ(RTC_IRQn);

        RTC_Init_TypeDef init = {
            false, // Start counting when initialization is done
            false, // Enable updating during debug halt.
            false  // Restart counting from 0 when reaching COMP0.
        };
        RTC_Init(&init);

        rtc_has_been_borked_for_led_cycling = false;
    }

    remove_rtc_interrupt_handler(led_rtc_count_callback);
    set_led_throb_mask(0);

    TIMER_Enable(TIMER0, false);
    TIMER_Enable(TIMER1, false);
    CMU_ClockEnable(cmuClock_TIMER0, true);
    CMU_ClockEnable(cmuClock_TIMER1, true);
}

void leds_on_for_reading(int ap_index, int ss_index, int third)
{
    // out of range case
    if (ap_index < 0 || ss_index < 0) {
        leds_on(LED_OUT_OF_RANGE_MASK);
        return;
    }

    // calculated as if leds were numbered clockwise
    unsigned ss_led_n = (LED_1S_N + ss_index) % LED_N_IN_WHEEL;
    unsigned ap_led_n = (LED_F1_N + ap_index) % LED_N_IN_WHEEL;

    // convert to counterclockwise numbering
    ss_led_n = (LED_N_IN_WHEEL - ss_led_n) % LED_N_IN_WHEEL;
    ap_led_n = (LED_N_IN_WHEEL - ap_led_n) % LED_N_IN_WHEEL;

    uint32_t mask = (1 << ap_led_n) | (1 << ss_led_n);
    if (third == 1)
        mask |= (1 << LED_PLUS_1_3_N);
    else if (third == -1)
        mask |= (1 << LED_MINUS_1_3_N);

    set_led_throb_mask(0);
    leds_on(mask);
}

void delay_ms_with_led_rtc(int ms)
{
    uint32_t start = leds_on_for_cycles;
    uint32_t target = start + ((ms * RTC_RAW_FREQ) / 1000);
    if (start < target) {
        while (leds_on_for_cycles < target)
            ;
    } else {
        while (leds_on_for_cycles > start)
            ;
        while (leds_on_for_cycles < target)
            ;
    }
}