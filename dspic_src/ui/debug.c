#include "debug.h"

static uint16_t mask = DEBUG_DEFAULT;

void debug_init(void)
{
    mask = DEBUG_DEFAULT;
}


bool debug_enabled(uint16_t subsystems)
{
    return (mask & subsystems) != 0U;
}


uint16_t debug_mask(void)
{
    return mask;
}


void debug_set(uint16_t new_mask)
{
    mask = new_mask & DEBUG_ALL;
}
