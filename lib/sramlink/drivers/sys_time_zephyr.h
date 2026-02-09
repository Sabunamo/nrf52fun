/**
 * @file sys_time_zephyr.h
 * @brief Zephyr-compatible system time abstraction for SRAMLink
 *
 * This provides a compatibility layer mapping bambam sys_time API to Zephyr kernel timing.
 * Uses k_uptime_ticks() for high-resolution timing.
 */
#pragma once

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

/** @brief System time type - 64-bit for Zephyr ticks */
typedef int64_t sys_time_t;

/** @brief Ticks per second in Zephyr (CONFIG_SYS_CLOCK_TICKS_PER_SEC) */
#define SYSTEM_TIME_TICKS_PER_SECOND CONFIG_SYS_CLOCK_TICKS_PER_SEC

/** @brief Time conversion macros */
#define SYS_SEC             (SYSTEM_TIME_TICKS_PER_SECOND)
#define SYS_TO_REAL_S(x)    ((x) / SYS_SEC)
#define SYS_TO_REAL_MS(x)   ((((uint64_t)(x)) * 1000) / SYS_SEC)
#define SYS_TO_REAL_US(x)   ((((uint64_t)(x)) * 1000000) / SYS_SEC)
#define SYS_NS(x)           ((((uint64_t)(x)) * SYS_SEC) / 1000000000)
#define SYS_US(x)           ((((uint64_t)(x)) * SYS_SEC) / 1000000)
#define SYS_MS(x)           ((((uint64_t)(x)) * SYS_SEC) / 1000)
#define SYS_S(x)            ((x) * SYS_SEC)

/**
 * @brief Get the current system time in ticks (awake time)
 * @return Current uptime in kernel ticks
 */
static inline sys_time_t sys_time_get_awake(void)
{
    return k_uptime_ticks();
}

/**
 * @brief Get the total system time in ticks
 * @return Current uptime in kernel ticks
 * @note In Zephyr, this is the same as awake time (no sleep tracking)
 */
static inline sys_time_t sys_time_get_total(void)
{
    return k_uptime_ticks();
}

/**
 * @brief Get the total system time in milliseconds
 * @return Current uptime in milliseconds
 */
static inline uint32_t sys_time_get_total_ms(void)
{
    return (uint32_t)k_uptime_get();
}

/**
 * @brief Initialize the system time module
 * @note No-op for Zephyr - kernel timing is always available
 */
static inline void sys_time_init(void)
{
    /* Zephyr kernel timing is always initialized */
}

/**
 * @brief Wake the system time module
 * @note No-op for Zephyr
 */
static inline void sys_time_wake(void)
{
    /* No-op for Zephyr */
}

/**
 * @brief Sleep the system time module
 * @note No-op for Zephyr
 */
static inline void sys_time_sleep(void)
{
    /* No-op for Zephyr */
}
