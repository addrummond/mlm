#ifndef LEDS_HH
#define LEDS_HH

#include <stdbool.h>
#include <stdint.h>

#define LED_N           26
#define LED_N_IN_WHEEL  24

#define DPIN1_GPIO_PORT gpioPortB
#define DPIN1_GPIO_PIN  11
#define DPIN1_TIMER     TIMER1
#define DPIN1_CHAN      2
#define DPIN1_ROUTE     TIMER_ROUTE_CC2PEN
#define DPIN1_LOCATION  TIMER_ROUTE_LOCATION_LOC3

#define DPIN2_GPIO_PORT gpioPortD
#define DPIN2_GPIO_PIN  6
#define DPIN2_TIMER     TIMER1
#define DPIN2_CHAN      0
#define DPIN2_ROUTE     TIMER_ROUTE_CC0PEN
#define DPIN2_LOCATION  TIMER_ROUTE_LOCATION_LOC4

#define DPIN3_GPIO_PORT gpioPortD
#define DPIN3_GPIO_PIN  7
#define DPIN3_TIMER     TIMER1
#define DPIN3_CHAN      1
#define DPIN3_ROUTE     TIMER_ROUTE_CC1PEN
#define DPIN3_LOCATION  TIMER_ROUTE_LOCATION_LOC4

#define DPIN4_GPIO_PORT gpioPortB
#define DPIN4_GPIO_PIN  7
#define DPIN4_TIMER     TIMER1
#define DPIN4_CHAN      0
#define DPIN4_ROUTE     TIMER_ROUTE_CC0PEN
#define DPIN4_LOCATION  TIMER_ROUTE_LOCATION_LOC3

#define DPIN5_GPIO_PORT gpioPortB
#define DPIN5_GPIO_PIN  8
#define DPIN5_TIMER     TIMER1
#define DPIN5_CHAN      1
#define DPIN5_ROUTE     TIMER_ROUTE_CC1PEN
#define DPIN5_LOCATION  TIMER_ROUTE_LOCATION_LOC3

#define DPIN6_GPIO_PORT gpioPortA
#define DPIN6_GPIO_PIN  0
#define DPIN6_TIMER     TIMER0
#define DPIN6_CHAN      0
#define DPIN6_ROUTE     TIMER_ROUTE_CC0PEN
#define DPIN6_LOCATION  TIMER_ROUTE_LOCATION_LOC0

#ifndef LEDS_WRONG_WAY_ROUND
#define LED1_CAT_DPIN   1
#define LED2_CAT_DPIN   2
#define LED3_CAT_DPIN   3
#define LED4_CAT_DPIN   4
#define LED5_CAT_DPIN   5
#define LED6_CAT_DPIN   1
#define LED7_CAT_DPIN   2
#define LED8_CAT_DPIN   3
#define LED9_CAT_DPIN   4
#define LED10_CAT_DPIN  6
#define LED11_CAT_DPIN  1
#define LED12_CAT_DPIN  2
#define LED13_CAT_DPIN  3
#define LED14_CAT_DPIN  5
#define LED15_CAT_DPIN  6
#define LED16_CAT_DPIN  1
#define LED17_CAT_DPIN  2
#define LED18_CAT_DPIN  4
#define LED19_CAT_DPIN  5
#define LED20_CAT_DPIN  6
#define LED21_CAT_DPIN  1
#define LED22_CAT_DPIN  3
#define LED23_CAT_DPIN  4
#define LED24_CAT_DPIN  5
#define LED25_CAT_DPIN  5
#define LED26_CAT_DPIN  6
#define LED1_AN_DPIN    6
#define LED2_AN_DPIN    6
#define LED3_AN_DPIN    6
#define LED4_AN_DPIN    6
#define LED5_AN_DPIN    6
#define LED6_AN_DPIN    5
#define LED7_AN_DPIN    5
#define LED8_AN_DPIN    5
#define LED9_AN_DPIN    5
#define LED10_AN_DPIN   5
#define LED11_AN_DPIN   4
#define LED12_AN_DPIN   4
#define LED13_AN_DPIN   4
#define LED14_AN_DPIN   4
#define LED15_AN_DPIN   4
#define LED16_AN_DPIN   3
#define LED17_AN_DPIN   3
#define LED18_AN_DPIN   3
#define LED19_AN_DPIN   3
#define LED20_AN_DPIN   3
#define LED21_AN_DPIN   2
#define LED22_AN_DPIN   2
#define LED23_AN_DPIN   2
#define LED24_AN_DPIN   2
#define LED25_AN_DPIN   1
#define LED26_AN_DPIN   1
#else
#define LED1_AN_DPIN   1
#define LED2_AN_DPIN   2
#define LED3_AN_DPIN   3
#define LED4_AN_DPIN   4
#define LED5_AN_DPIN   5
#define LED6_AN_DPIN   1
#define LED7_AN_DPIN   2
#define LED8_AN_DPIN   3
#define LED9_AN_DPIN   4
#define LED10_AN_DPIN  6
#define LED11_AN_DPIN  1
#define LED12_AN_DPIN  2
#define LED13_AN_DPIN  3
#define LED14_AN_DPIN  5
#define LED15_AN_DPIN  6
#define LED16_AN_DPIN  1
#define LED17_AN_DPIN  2
#define LED18_AN_DPIN  4
#define LED19_AN_DPIN  5
#define LED20_AN_DPIN  6
#define LED21_AN_DPIN  1
#define LED22_AN_DPIN  3
#define LED23_AN_DPIN  4
#define LED24_AN_DPIN  5
#define LED25_AN_DPIN  5
#define LED26_AN_DPIN  6
#define LED1_CAT_DPIN    6
#define LED2_CAT_DPIN    6
#define LED3_CAT_DPIN    6
#define LED4_CAT_DPIN    6
#define LED5_CAT_DPIN    6
#define LED6_CAT_DPIN    5
#define LED7_CAT_DPIN    5
#define LED8_CAT_DPIN    5
#define LED9_CAT_DPIN    5
#define LED10_CAT_DPIN   5
#define LED11_CAT_DPIN   4
#define LED12_CAT_DPIN   4
#define LED13_CAT_DPIN   4
#define LED14_CAT_DPIN   4
#define LED15_CAT_DPIN   4
#define LED16_CAT_DPIN   3
#define LED17_CAT_DPIN   3
#define LED18_CAT_DPIN   3
#define LED19_CAT_DPIN   3
#define LED20_CAT_DPIN   3
#define LED21_CAT_DPIN   2
#define LED22_CAT_DPIN   2
#define LED23_CAT_DPIN   2
#define LED24_CAT_DPIN   2
#define LED25_CAT_DPIN   1
#define LED26_CAT_DPIN   1
#endif

#define LED_FOR_EACH(m) m(1) m(2) m(3) m(4) m(5) m(6) m(7) m(8) m(9) m(10) m(11) m(12) m(13) m(14) m(15) m(16) m(17) m(18) m(19) m(20) m(21) m(22) m(23) m(24) m(25) m(26)

#define DPIN_FOR_EACH(m) m(1) m(2) m(3) m(4) m(5) m(6)

#define LED_ISO6_N      18
#define LED_F8_N        0
#define LED_F11_N       1
#define LED_F16_N       2
#define LED_F22_N       3
#define LED_F32_N       4
#define LED_F45_N       5
#define LED_1S_N        6
#define LED_2_N         7
#define LED_4_N         8
#define LED_8_N         9
#define LED_15_N        10
#define LED_30_N        11
#define LED_60_N        12
#define LED_125_N       13
#define LED_250_N       14
#define LED_500_N       15
#define LED_1000_N      16
#define LED_2000_N      17
#define LED_F1_N        18
#define LED_F1_4_N      19
#define LED_F2_N        20
#define LED_F2_8_N      21
#define LED_F4_N        22
#define LED_F5_6_N      23
#define LED_MINUS_1_3_N 24
#define LED_PLUS_1_3_N  25

#define LED_OUT_OF_RANGE_MASK 0b100000000000000000000011

void reset_led_state();
void led_on(unsigned n);
void leds_on(uint32_t mask);
void leds_change_mask(uint32_t mask);
void led_fully_on(unsigned n);
void leds_all_off();
uint32_t led_mask_for_reading(int ap_index, int ss_index, int third);
void set_led_throb_mask(uint32_t mask);
void set_led_flash_mask(uint32_t mask);
bool rtc_borked_for_led_cycling(void);

extern volatile uint32_t leds_on_for_cycles;

#endif