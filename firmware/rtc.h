#ifndef RTC_H
#define RTC_H

void RTC_IRQHandler(void);
void set_rtc_interrupt_handler(void (*callback)(void));


#endif