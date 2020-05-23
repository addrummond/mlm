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
#include <units.h>
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
static const int32_t DUTY_CYCLE_MIN = 10;
static const int32_t DUTY_CYCLE_MAX = 50;

static const int CYCLES_PER_THROB_STEP = RTC_RAW_FREQ / 60;
static const int CYCLES_PER_FLASH = RTC_RAW_FREQ / 15;

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
    if (ev < 0)
        return DUTY_CYCLE_MIN;

    // Go to full brightness by EV 10 @ ISO 100.
    int32_t dc = ev * (COUNT/10);
    int32_t dcr = dc & ((1 << EV_BPS)-1);
    dc >>= EV_BPS;
    if (dcr >= (1 << EV_BPS)/2)
        ++dc;

    if (dc < DUTY_CYCLE_MIN)
        dc = DUTY_CYCLE_MIN;
    if (dc > DUTY_CYCLE_MAX)
        dc = DUTY_CYCLE_MAX;

    return 0;
    return COUNT - (uint32_t)dc;
}

static uint32_t get_duty_cycle()
{
    return duty_cycle_for_ev(g_state.led_brightness_ev_ref);
}


static void led_on_with_dc(unsigned n, uint32_t duty_cycle)
{
    GPIO_Mode_TypeDef pushPull = (g_state.bat_known_healthy ? gpioModePushPullDrive : gpioModePushPull);

    GPIO_Port_TypeDef cat_port = led_cat_ports[n];
    int cat_pin = led_cat_pins[n];
    GPIO_Port_TypeDef an_port = led_an_ports[n];
    int an_pin = led_an_pins[n];
    TIMER_TypeDef *cat_timer = led_cat_timer[n];
    int cat_chan = led_cat_chan[n];
    uint32_t cat_route = led_cat_route[n];
    uint32_t cat_location = led_cat_location[n];
    GPIO_PinModeSet(cat_port, cat_pin, pushPull, 1);
    GPIO_PinModeSet(an_port, an_pin, pushPull, 1);
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
static volatile uint32_t flash_mask;
static volatile int current_throb_index;
static volatile int current_throb_cycles;
static volatile int last_throb_cycles;
static volatile int last_flash_cycles;
static volatile bool flash_on;
static volatile uint32_t target_duty_cycle;
static volatile uint32_t current_duty_cycle;

void reset_led_state()
{
    orig_mask = 0;
    current_mask = 0;
    throb_mask = 0;
    current_mask_n = 0;
    flash_mask = 0;
    current_throb_index = 0;
    current_throb_cycles = 0;
    last_throb_cycles = 0;
    last_flash_cycles = 0;
    flash_on = false;
    target_duty_cycle = 0;
    current_duty_cycle = 0;
}

void set_led_throb_mask(uint32_t mask)
{
    throb_mask = mask;
}

void set_led_flash_mask(uint32_t mask)
{
    flash_mask = mask;
    flash_on = (mask != 0);
    last_flash_cycles = 0;
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

    if (leds_on_for_cycles - last_throb_cycles >= CYCLES_PER_THROB_STEP) {
        current_throb_index = (current_throb_index + 1) % (sizeof(throb_progression)/sizeof(throb_progression[0]));
        last_throb_cycles = leds_on_for_cycles;
    }

    if (flash_mask != 0 && leds_on_for_cycles - last_flash_cycles >= CYCLES_PER_FLASH) {
        flash_on = !flash_on;
        last_flash_cycles = leds_on_for_cycles;
    }

    int32_t dc = current_duty_cycle;
    if (dc > target_duty_cycle)
        --current_duty_cycle;
    if (throb_mask & (1 << current_mask_n)) {
        if (dc - THROB_MAG < DUTY_CYCLE_MIN)
            dc = THROB_MAG + DUTY_CYCLE_MIN;
        else if (dc + THROB_MAG > DUTY_CYCLE_MAX)
            dc = DUTY_CYCLE_MAX - THROB_MAG;
        dc += throb_progression[current_throb_index];

        if (dc < DUTY_CYCLE_MIN)
            dc = DUTY_CYCLE_MIN;
        else if (dc > DUTY_CYCLE_MAX)
            dc = DUTY_CYCLE_MAX;
    } else if ((flash_mask & (1 << current_mask_n)) && !flash_on) {
        dc = DUTY_CYCLE_MAX;
    }

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
        timerEventEveryEdge,      // Event on every capture.
        timerEdgeRising,          // Input capture edge on rising edge.
        timerPRSSELCh0,           // Not used by default, select PRS channel 0.
        timerOutputActionNone,    // No action on underflow.
        timerOutputActionNone,    // No action on overflow.
        timerOutputActionToggle,  // Action on match.
        timerCCModePWM,           // Disable compare/capture channel.
        false,                    // Disable filter.
        false,                    // Select TIMERnCCx input.
        false,                    // Clear output when counter disabled.
        false                     // Do not invert output.
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

#ifndef DEBUG
    if (g_state.bat_known_healthy) {
#define M(n) GPIO_DriveModeSet(MACROUTILS_CONCAT3(DPIN, n, _GPIO_PORT), gpioDriveModeHigh);
        DPIN_FOR_EACH(M)
#undef M
    }
#endif

    current_duty_cycle = COUNT;
    target_duty_cycle = get_duty_cycle();

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
    last_throb_cycles = 0;

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

void leds_all_off()
{
    turnoff();

    #define M(n) GPIO_DriveModeSet(MACROUTILS_CONCAT3(DPIN, n, _GPIO_PORT), gpioDriveModeStandard);
        DPIN_FOR_EACH(M)
    #undef M

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

uint32_t led_mask_for_reading(int ap_index, int ss_index, int third)
{
    if (ap_index == -1) // out of range
        return LED_OUT_OF_RANGE_MASK;

    // calculated as if leds were numbered clockwise with f8 LED as 0.
    int_fast32_t ss_led_n;
    if (ss_index >= SS_INDEX_MIN) {
        ss_led_n = (LED_1S_N + ss_index) % LED_N_IN_WHEEL;
    } else {
        ss_led_n = LED_2_N + SS_INDEX_MIN - ss_index;
    }

    int ap_led_n = (LED_F1_N + ap_index) % LED_N_IN_WHEEL;

    // convert to counterclockwise numbering
    int cc_ss_led_n = (LED_N_IN_WHEEL - ss_led_n) % LED_N_IN_WHEEL;
    int cc_ap_led_n = (LED_N_IN_WHEEL - ap_led_n) % LED_N_IN_WHEEL;

    uint32_t mask = (1 << cc_ap_led_n) | (1 << cc_ss_led_n);

    // if it's a long shutter speed, add the lights on either side.
    if (ss_index < SS_INDEX_MIN) {
        int left = (LED_N_IN_WHEEL - ss_led_n - 1) % LED_N_IN_WHEEL;
        int right = (LED_N_IN_WHEEL - ss_led_n + 1) % LED_N_IN_WHEEL;
        mask |= (1 << left) | (1 << right);
    }

    if (third == 1)
        mask |= (1 << LED_PLUS_1_3_N);
    else if (third == -1)
        mask |= (1 << LED_MINUS_1_3_N);

    return mask;
}