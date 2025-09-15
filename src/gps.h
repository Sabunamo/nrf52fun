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

#define GPS_UART_NODE DT_NODELABEL(uart1)
#define GPS_BUFFER_SIZE 256

struct gps_data {
    double latitude;
    double longitude;
    double seeHeight;   // Altitude above sea level in meters
    char lat_hemisphere;
    char lon_hemisphere;
    char time_str[11];
    char date_str[20];  // Format: DD/MM/YYYY
    char hijri_date_str[20];  // Format: DD/MM/YYYY
    char day_of_week[12];  // Day name (e.g., "Monday")
    bool valid;
    bool date_valid;
    bool hijri_valid;
    bool day_valid;
    bool seeHeight_valid;
};

extern struct gps_data current_gps;

int gps_init(void);
void gps_process_data(void);
void gps_print_info(void);
void display_gps_data(const struct device *display_dev, int x, int y);
const char* gps_get_today_date(void);

#endif