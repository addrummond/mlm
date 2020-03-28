#ifndef RTC_H
#define RTC_H

void RTC_IRQHandler(void);
void add_rtc_interrupt_handler(void (*callback)(void));
void remove_rtc_interrupt_handler(void (*callback)(void));
void clear_rtc_interrupt_handlers(void);


#endif