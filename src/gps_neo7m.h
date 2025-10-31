/**
 * @file gps_neo7m.h
 * @brief NEO-7M GPS module driver for NMEA data processing
 *
 * This module handles NEO-7M GPS communication via UART and processes NMEA sentences
 * to extract position, time, and date information.
 *
 * Hardware: NEO-7M WPI430 Velleman GPS Module
 * - Default baud rate: 9600 bps
 * - Output: NMEA 0183 protocol
 * - Update rate: 1Hz (configurable up to 10Hz)
 *
 * UART Configuration:
 * - UART0: 9600 baud, 8N1
 * - P0.08: NEO-7M TX (connect to GPS module TX)
 * - P0.06: NEO-7M RX (connect to GPS module RX)
 *
 * COMPATIBLE INTERFACE: Uses same function/variable names as gps.h
 */

#ifndef GPS_NEO7M_H
#define GPS_NEO7M_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/printk.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

// Hardware configuration
#define GPS_UART_NODE DT_NODELABEL(uart0)  ///< GPS UART device tree node (using UART0 for GPS)
#define GPS_BUFFER_SIZE 256                 ///< NMEA sentence buffer size

/**
 * @brief GPS data structure containing parsed NMEA information
 * EXACTLY matches struct gps_data from gps.h for compatibility
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

// Function declarations - EXACTLY matches gps.h interface

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
 * @brief Send test data on UART for loopback testing
 * @param count Test message counter
 */
void gps_send_test_data(int count);

#endif // GPS_NEO7M_H
