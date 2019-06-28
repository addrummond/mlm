#include <stdint.h>
#include <efm32tg232f8.h>
#include <util.h>

// sign_of and iabs are useful for printing signed numbers to the RTT console
// (since RTT itself doesn't handle them).

const char *sign_of(int32_t n)
{
    if (n >= 0)
        return "+";
    return "-";
}

uint32_t iabs(int32_t n)
{
    if (n >= 0)
        return (uint32_t)n;
    return (~0U - (uint32_t)n) + 1;
}