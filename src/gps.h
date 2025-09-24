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

#endif