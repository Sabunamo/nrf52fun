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
// Board-specific UART selection
#if DT_NODE_EXISTS(DT_ALIAS(gps_uart))
    #define GPS_UART_NODE DT_ALIAS(gps_uart)  ///< GPS UART from device tree alias
#elif DT_NODE_HAS_STATUS(DT_NODELABEL(uart1), okay)
    #define GPS_UART_NODE DT_NODELABEL(uart1)  ///< GPS on UART1 (nrf5340dk)
#else
    #define GPS_UART_NODE DT_NODELABEL(uart0)  ///< GPS on UART0 (nrf52dk, default)
#endif
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
 * @brief Get local time with automatic DST adjustment (CET/CEST)
 * Automatically detects DST based on European rules:
 * - DST starts: Last Sunday of March at 2:00 AM (UTC+2)
 * - DST ends: Last Sunday of October at 3:00 AM (UTC+1)
 * @param local_time Output buffer for local time
 * @param max_len Size of output buffer (must be at least 11 bytes)
 * @return Timezone offset applied (1 for CET winter, 2 for CEST summer, 0 if invalid)
 */
int gps_get_local_time(char *local_time, size_t max_len);

/**
 * @brief Auto-configure timezone based on GPS coordinates
 * Uses GPS longitude to calculate timezone offset and updates prayer time settings
 */
void gps_auto_configure_timezone(void);


/**
 * @brief Print raw NMEA sentences received from GPS
 * Displays the last 5 NMEA sentences received for debugging
 */
void gps_print_raw_data(void);

/**
 * @brief Get GPS statistics
 * @param bytes_rx Pointer to store total bytes received (can be NULL)
 * @param sentences_parsed Pointer to store total sentences parsed (can be NULL)
 */
void gps_get_stats(uint32_t *bytes_rx, uint32_t *sentences_parsed);

#endif // GPS_NEO7M_H
