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

int popcount(uint32_t x)
{
    int count;
    for (count=0; x; count++)
        x &= x - 1;
    return count;
}