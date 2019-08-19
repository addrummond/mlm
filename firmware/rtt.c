#include <stdint.h>
#include <rtt.h>

#if defined DEBUG && !defined NO_RTT

uint8_t rtt_up_buf[128];

void rtt_init()
{
    SEGGER_RTT_Init();
    SEGGER_RTT_ConfigUpBuffer(0, "D", rtt_up_buf, sizeof(rtt_up_buf), SEGGER_RTT_MODE_BLOCK_IF_FIFO_FULL);
}

#endif