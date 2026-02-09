/**
 * @file cdtimer_zephyr.c
 * @brief Countdown Timer implementation for Zephyr
 */

#include "cdtimer_zephyr.h"
#include <zephyr/kernel.h>

static sys_time_t get_current_ticks(void)
{
    return sys_time_get_total();
}

void cdtimer_init(cdtimer_t *cdtimer, uint32_t interval_ms)
{
    cdtimer_set_interval(cdtimer, interval_ms);
}

void cdtimer_restart(cdtimer_t *cdtimer)
{
    cdtimer->start_time = get_current_ticks();
}

void cdtimer_set_interval(cdtimer_t *cdtimer, uint32_t new_interval_ms)
{
    cdtimer->start_time = get_current_ticks();
    cdtimer->interval = SYS_MS(new_interval_ms);
}

bool cdtimer_completed(cdtimer_t *cdtimer)
{
    return ((get_current_ticks() - cdtimer->start_time) >= cdtimer->interval);
}

void cdtimer_force_complete(cdtimer_t *cdtimer)
{
    cdtimer->start_time = get_current_ticks() - cdtimer->interval;
}

sys_time_t cdtimer_elapsed_ticks(cdtimer_t *cdtimer)
{
    return get_current_ticks() - cdtimer->start_time;
}

uint32_t cdtimer_elapsed(cdtimer_t *cdtimer)
{
    /* Return elapsed time in milliseconds */
    uint64_t elapsed = ((uint64_t)cdtimer_elapsed_ticks(cdtimer) * 1000) /
                       (uint64_t)SYSTEM_TIME_TICKS_PER_SECOND;
    return (uint32_t)elapsed;
}

uint32_t cdtimer_remaining(cdtimer_t *cdtimer)
{
    if (cdtimer_completed(cdtimer)) {
        return 0;
    }

    /* Return remaining time in milliseconds */
    sys_time_t remaining_ticks = cdtimer->start_time + cdtimer->interval - get_current_ticks();
    return (uint32_t)((remaining_ticks * (uint64_t)1000) / SYSTEM_TIME_TICKS_PER_SECOND);
}

void cdtimer_wait(cdtimer_t *cdtimer)
{
    while (!cdtimer_completed(cdtimer)) {
        k_yield();
    }
}

void cdtimer_delay_ms(uint32_t delay_ms)
{
    k_msleep(delay_ms);
}
