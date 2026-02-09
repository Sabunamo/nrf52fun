/**
 * @file cdtimer_zephyr.h
 * @brief Countdown Timer for Zephyr - SRAMLink compatible
 *
 * Provides a convenient interface to track timing intervals.
 * Port of bambam cdtimer to Zephyr kernel timing.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "sys_time_zephyr.h"

/**
 * @brief Structure for maintaining countdown timer state
 */
typedef struct {
    sys_time_t start_time;  /**< Timestamp when cdtimer was last restarted */
    sys_time_t interval;    /**< Time interval in ticks */
} cdtimer_t;

/**
 * @brief Maximum milliseconds representable in the timer
 */
#define CDTIMER_MAX_MS  (INT32_MAX / (SYSTEM_TIME_TICKS_PER_SECOND / 1000))

/**
 * @brief Initialize a cdtimer_t struct with interval_ms milliseconds
 * @param cdtimer Pointer to the timer structure
 * @param interval_ms Interval in milliseconds
 */
void cdtimer_init(cdtimer_t *cdtimer, uint32_t interval_ms);

/**
 * @brief Set the interval of cdtimer to a new value
 * @param cdtimer Pointer to the timer structure
 * @param interval_ms New interval in milliseconds
 */
void cdtimer_set_interval(cdtimer_t *cdtimer, uint32_t interval_ms);

/**
 * @brief Restart the countdown interval
 * @param cdtimer Pointer to the timer structure
 */
void cdtimer_restart(cdtimer_t *cdtimer);

/**
 * @brief Check if the interval has completed
 * @param cdtimer Pointer to the timer structure
 * @return true if interval has expired, false otherwise
 */
bool cdtimer_completed(cdtimer_t *cdtimer);

/**
 * @brief Force a timer to be completed
 * @param cdtimer Pointer to the timer structure
 */
void cdtimer_force_complete(cdtimer_t *cdtimer);

/**
 * @brief Get elapsed time in milliseconds since timer was restarted
 * @param cdtimer Pointer to the timer structure
 * @return Elapsed time in milliseconds
 */
uint32_t cdtimer_elapsed(cdtimer_t *cdtimer);

/**
 * @brief Get elapsed time in ticks since timer was restarted
 * @param cdtimer Pointer to the timer structure
 * @return Elapsed time in system ticks
 */
sys_time_t cdtimer_elapsed_ticks(cdtimer_t *cdtimer);

/**
 * @brief Get remaining time in milliseconds
 * @param cdtimer Pointer to the timer structure
 * @return Remaining time in milliseconds, 0 if completed
 */
uint32_t cdtimer_remaining(cdtimer_t *cdtimer);

/**
 * @brief Block until timer completes
 * @param cdtimer Pointer to the timer structure
 * @note Uses k_yield() to allow other threads to run
 */
void cdtimer_wait(cdtimer_t *cdtimer);

/**
 * @brief Delay for specified milliseconds
 * @param delay_ms Delay duration in milliseconds
 */
void cdtimer_delay_ms(uint32_t delay_ms);
