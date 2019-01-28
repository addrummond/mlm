#ifndef RTT_H
#define RTT_H

#ifdef DEBUG
#include <rtt/SEGGER_RTT.h>
#include <stdint.h>
extern uint8_t rtt_up_buf[];
void rtt_init();
#else
#define rtt_init(...)
#define SEGGER_RTT_printf(...)
#define SEGGER_RTT_Write(...)
#define SEGGER_RTT_WriteString(...)
#endif

#endif
