/**
 * @file gps.h
 * @brief GPS module interface for NMEA data processing
 *
 * This module handles GPS communication via UART1 and processes NMEA sentences
 * to extract position, time, and date information for the prayer time application.
 *
 * Hardware Configuration:
 * - UART1: 9600 baud, 8N1
 * - P1.01: GPS TX (connect to GPS module RX)
 * - P1.02: GPS RX (connect to GPS module TX)
 */

#ifndef GPS_H
#define GPS_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/display.h>
#include <zephyr/sys/printk.h>
#include <zephyr/posix/time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

// Hardware configuration
#define GPS_UART_NODE DT_NODELABEL(uart1)  ///< GPS UART device tree node
#define GPS_BUFFER_SIZE 256                 ///< NMEA sentence buffer size

/**
 * @brief GPS data structure containing parsed NMEA information
 */
struct gps_data {
    double latitude;                ///< Latitude in decimal degrees (+ = North, - = South)
    double longitude;               ///< Longitude in decimal degrees (+ = East, - = West)
    double seeHeight;               ///< Altitude above sea level in meters
    char lat_hemisphere;            ///< Latitude hemisphere ('N' or 'S')
    char lon_hemisphere;            ///< Longitude hemisphere ('E' or 'W')
    char time_str[11];              ///< GPS time in HH:MM:SS format (UTC)
    char date_str[20];              ///< Date in DD/MM/YYYY format
    char hijri_date_str[20];        ///< Hijri date in DD/MM/YYYY format
    char day_of_week[12];           ///< Day name (e.g., "Monday")
    bool valid;                     ///< True when GPS has satellite fix
    bool date_valid;                ///< True when date information is available
    bool hijri_valid;               ///< True when Hijri date is calculated
    bool day_valid;                 ///< True when day of week is available
    bool seeHeight_valid;           ///< True when altitude is valid
    int utc_hours;                  ///< UTC hours (0-23)
    int utc_minutes;                ///< UTC minutes (0-59)
    int utc_seconds;                ///< UTC seconds (0-59)
    int utc_day;                    ///< UTC day (1-31)
    int utc_month;                  ///< UTC month (1-12)
    int utc_year;                   ///< UTC year (full year)
};

/**
 * @brief DST (Daylight Saving Time) configuration
 *
 * For EU DST rules (last Sunday of March to last Sunday of October):
 * - dst_start_month = 3, dst_start_day = 0
 * - dst_end_month = 10, dst_end_day = 0
 *
 * Setting day to 0 automatically calculates the last Sunday of the month.
 * Setting day to 1-31 uses that specific date.
 */
struct dst_config {
    int timezone_offset;            ///< Standard timezone offset from UTC in hours (e.g., 2 for UTC+2)
    bool dst_enabled;               ///< Enable/disable DST
    int dst_offset;                 ///< DST offset in hours (typically 1)
    int dst_start_month;            ///< Month when DST starts (1-12)
    int dst_start_day;              ///< Day when DST starts (1-31), or 0 for last Sunday of month
    int dst_end_month;              ///< Month when DST ends (1-12)
    int dst_end_day;                ///< Day when DST ends (1-31), or 0 for last Sunday of month
};

// Global GPS data instance
extern struct gps_data current_gps;

// Function declarations

/**
 * @brief Initialize GPS module and UART communication
 * @return 0 on success, -1 on failure
 */
int gps_init(void);

/**
 * @brief Process GPS data (compatibility function for interrupt-driven mode)
 */
void gps_process_data(void);

/**
 * @brief Print current GPS information to console (debug function)
 */
void gps_print_info(void);

/**
 * @brief Display GPS data on LCD screen
 * @param display_dev Display device pointer
 * @param x X position on screen
 * @param y Y position on screen
 */
void display_gps_data(const struct device *display_dev, int x, int y);

/**
 * @brief Get current date string for prayer calculations
 * @return Pointer to date string in DD/MM/YYYY format
 */
const char* gps_get_today_date(void);

/**
 * @brief Set DST configuration
 * @param config Pointer to DST configuration structure
 */
void gps_set_dst_config(const struct dst_config *config);

/**
 * @brief Check if DST is currently active
 * @return true if DST is active, false otherwise
 */
bool gps_is_dst_active(void);

/**
 * @brief Get local time with timezone and DST applied
 * @param local_time_str Output buffer for local time string (HH:MM:SS format)
 * @param max_len Maximum length of output buffer
 * @return Total offset applied (timezone + DST) in hours
 */
int gps_get_local_time(char *local_time_str, size_t max_len);

/**
 * @brief Get current timezone offset (including DST if active)
 * @return Total timezone offset in hours
 */
int gps_get_current_offset(void);

/**
 * @brief Calculate timezone offset from GPS longitude coordinate
 * @param longitude Longitude in decimal degrees
 * @return Calculated timezone offset in hours from UTC
 */
int gps_calculate_timezone_from_longitude(double longitude);

/**
 * @brief Auto-configure timezone based on current GPS coordinates
 * Updates the DST configuration with calculated timezone offset
 */
void gps_auto_configure_timezone(void);

#endif