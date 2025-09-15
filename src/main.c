#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "font.h"
#include "gps.h"
#include "prayer_hmi.h"
#include "prayerTime.h"
#include "world_cities.h"

#define RESET_PIN  10   // P1.10 (RST pin)

// Variables for prayer calculations
double Lng = 0.0, Lat = 0.0, D = 0.0;

void draw_character(const struct device *display_dev, char c, int x, int y, uint16_t color) {
    const uint8_t* char_pattern = font_space; // default to space
    
    // Select character pattern
    switch(c) {
        case 'I': char_pattern = font_I; break;
        case 'n': char_pattern = font_n; break;
        case ' ': char_pattern = font_space; break;
        case 't': char_pattern = font_t; break;
        case 'h': char_pattern = font_h; break;
        case 'e': char_pattern = font_e; break;
        case 'm': char_pattern = font_m; break;
        case 'a': char_pattern = font_a; break;
        case 'o': char_pattern = font_o; break;
        case 'f': char_pattern = font_f; break;
        case 'A': char_pattern = font_A; break;
        case 'l': char_pattern = font_l; break;
        case 'G': char_pattern = font_G; break;
        case 'P': char_pattern = font_P; break;
        case 'S': char_pattern = font_S; break;
        case ':': char_pattern = font_colon; break;
        case 'N': char_pattern = font_N; break;
        case 's': char_pattern = font_s; break;
        case 'i': char_pattern = font_i; break;
        case 'g': char_pattern = font_g; break;
        case 'r': char_pattern = font_r; break;
        case 'T': char_pattern = font_T; break;
        case 'L': char_pattern = font_L; break;
        case 'D': char_pattern = font_D; break;
        case '.': char_pattern = font_period; break;
        case '/': char_pattern = font_slash; break;
        case '0': char_pattern = font_0; break;
        case '1': char_pattern = font_1; break;
        case '2': char_pattern = font_2; break;
        case '3': char_pattern = font_3; break;
        case '4': char_pattern = font_4; break;
        case '5': char_pattern = font_5; break;
        case '6': char_pattern = font_6; break;
        case '7': char_pattern = font_7; break;
        case '8': char_pattern = font_8; break;
        case '9': char_pattern = font_9; break;
        default: char_pattern = font_space; break;
    }
    
    // Draw 16x8 character (16 rows, 8 columns) - font might be 16 pixels tall
    for (int row = 0; row < 16; row++) {
        uint8_t pattern = char_pattern[row];
        for (int col = 0; col < 8; col++) {
            if (pattern & (0x80 >> col)) {  // Check if bit is set
                uint16_t pixel = color;
                struct display_buffer_descriptor pixel_desc = {
                    .width = 1,
                    .height = 1,
                    .pitch = 1,
                    .buf_size = sizeof(pixel),
                };
                
                display_write(display_dev, x + col, y + row, &pixel_desc, &pixel);
            }
        }
    }
}

void draw_text(const struct device *display_dev, const char* text, int x, int y, uint16_t color) {
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        draw_character(display_dev, text[i], x + (i * 9), y, color); // 9 pixels spacing
    }
}

// Helper function to convert decimal hours to HH:MM format
void decimal_to_time_string(double decimal_hours, char* time_str, size_t max_len) {
    // Ensure positive value and within 24 hours
    while (decimal_hours < 0) decimal_hours += 24;
    while (decimal_hours >= 24) decimal_hours -= 24;
    
    int hours = (int)decimal_hours;
    int minutes = (int)((decimal_hours - hours) * 60);
    
    snprintf(time_str, max_len, "%02d:%02d", hours, minutes);
}

void main(void)
{
    printk("Starting display text test...\n");

    // Configure and reset the display first
    const struct device *reset_dev = DEVICE_DT_GET(DT_NODELABEL(gpio1));
    if (!device_is_ready(reset_dev)) {
        printk("GPIO device not ready\n");
        return;
    }

    // Configure RESET pin
    gpio_pin_configure(reset_dev, RESET_PIN, GPIO_OUTPUT_ACTIVE | GPIO_OUTPUT_INIT_HIGH);
    
    // Perform reset sequence
    printk("Resetting display...\n");
    gpio_pin_set(reset_dev, RESET_PIN, 0); // Assert reset (active low)
    k_msleep(10);
    gpio_pin_set(reset_dev, RESET_PIN, 1); // Release reset
    k_msleep(150); // Wait for display to initialize

    // Get the ILI9341 display device from devicetree
    const struct device *display_dev = DEVICE_DT_GET(DT_INST(0, ilitek_ili9341));
    if (!device_is_ready(display_dev)) {
        printk("Display device not ready\n");
        return;
    }

    printk("Display device is ready\n");

    // Turn on the display (disable blanking)
    int ret = display_blanking_off(display_dev);
    if (ret) {
        printk("display_blanking_off failed: %d\n", ret);
        return;
    }
    printk("Display blanking disabled\n");

    // Initialize Prayer HMI
    printk("Initializing Prayer HMI...\n");
    hmi_init();
    
    // Initialize GPS
    printk("Initializing GPS...\n");
    int gps_ret = gps_init();
    if (gps_ret != 0) {
        printk("GPS initialization failed: %d\n", gps_ret);
    }
    
    // Initialize with default prayer times (new order with SHURUQ)
    prayer_time_t current_prayers[PRAYER_COUNT] = {
        {"Fajr", "05:30", false},
        {"Shuruq", "06:45", false},
        {"Dhuhr", "12:15", false},
        {"Asr", "15:45", true},
        {"Maghrib", "18:20", false},
        {"Isha", "20:00", false}
    };
    
    hmi_set_prayer_times(current_prayers, PRAYER_ASR);
    hmi_set_countdown("Calculating...");
    hmi_set_city("GPS Location...");
    hmi_set_weather("28oC");
    hmi_set_current_time("--:--");
    hmi_set_brightness(75);
    
    printk("Setup complete. Starting HMI display loop...\n");
    
    bool prayer_times_calculated = false;
    
    // Keep running and update display
    while (1) {
        // Process GPS data using polling
        gps_process_data();
        
        // Update HMI with GPS data if available
        extern struct gps_data current_gps;
        static bool dates_updated = false;
        static char last_date[20] = {0};  // Track date changes for daily refresh
        
        if (current_gps.date_valid) {
            // Check if date changed (new day started) - trigger daily refresh
            if (strlen(last_date) > 0 && strcmp(last_date, current_gps.date_str) != 0) {
                printk("NEW DAY DETECTED! Date changed from '%s' to '%s'\n", last_date, current_gps.date_str);
                printk("Performing daily screen refresh and prayer time recalculation...\n");
                
                // Reset flags to trigger fresh calculations
                dates_updated = false;
                prayer_times_calculated = false;
                
                // Force complete screen refresh for new day
                hmi_clear_screen(display_dev);
                k_msleep(100);  // Brief pause for complete clear
                
                // Reset display elements for new day
                hmi_set_city("GPS Location...");
                hmi_set_countdown("Calculating...");
                
                printk("Daily refresh completed - ready for new day!\n");
            }
            
            // Update dates if needed
            if (!dates_updated) {
                hmi_set_dates(current_gps.date_str, 
                             current_gps.hijri_valid ? current_gps.hijri_date_str : "--/--/----",
                             current_gps.day_valid ? current_gps.day_of_week : "---");
                // Force full update for dates (one-time per day)
                printk("About to force full update after date update...\n");
                printk("Current time before date update: '%s'\n", current_gps.time_str);
                hmi_force_full_update(display_dev);
                dates_updated = true;
                
                // Store current date for daily change detection
                strcpy(last_date, current_gps.date_str);
                printk("Date update completed for: %s\n", last_date);
            }
        }
        
        if (current_gps.valid) {
            // Use GPS time as base, but update every second locally
            static uint32_t last_gps_update = 0;
            static char local_time[12] = {0};
            static int last_seconds = -1;
            
            uint32_t now = k_uptime_get_32();
            
            // Update local time from GPS initially or every 60 seconds for sync
            if (last_gps_update == 0 || (now - last_gps_update) > 60000) {
                // Add 2 hours timezone offset to GPS UTC time
                if (strlen(current_gps.time_str) >= 8) {
                    int utc_hours = (current_gps.time_str[0] - '0') * 10 + (current_gps.time_str[1] - '0');
                    int minutes = (current_gps.time_str[3] - '0') * 10 + (current_gps.time_str[4] - '0');
                    int seconds = (current_gps.time_str[6] - '0') * 10 + (current_gps.time_str[7] - '0');
                    
                    // Add 2 hours for local time (UTC+2)
                    int local_hours = utc_hours + 2;
                    if (local_hours >= 24) {
                        local_hours -= 24;  // Handle day rollover
                    }
                    
                    // Format as local time
                    snprintf(local_time, sizeof(local_time), "%02d:%02d:%02d", local_hours, minutes, seconds);
                    last_seconds = seconds;
                    
                    printk("GPS UTC: %s -> Local UTC+2: %s\n", current_gps.time_str, local_time);
                } else {
                    strcpy(local_time, current_gps.time_str);
                }
                
                last_gps_update = now;
            }
            
            // Update seconds every 1000ms
            static uint32_t last_second_update = 0;
            if ((now - last_second_update) >= 1000) {
                last_seconds++;
                if (last_seconds >= 60) {
                    // Handle minute rollover - resync with GPS and add timezone offset
                    if (strlen(current_gps.time_str) >= 8) {
                        int utc_hours = (current_gps.time_str[0] - '0') * 10 + (current_gps.time_str[1] - '0');
                        int minutes = (current_gps.time_str[3] - '0') * 10 + (current_gps.time_str[4] - '0');
                        int seconds = (current_gps.time_str[6] - '0') * 10 + (current_gps.time_str[7] - '0');
                        
                        // Add 2 hours for local time
                        int local_hours = utc_hours + 2;
                        if (local_hours >= 24) {
                            local_hours -= 24;
                        }
                        
                        snprintf(local_time, sizeof(local_time), "%02d:%02d:%02d", local_hours, minutes, seconds);
                        last_seconds = seconds;
                    }
                } else {
                    // Update just the seconds part
                    local_time[6] = '0' + (last_seconds / 10);
                    local_time[7] = '0' + (last_seconds % 10);
                }
                last_second_update = now;
            }
            
            hmi_set_current_time(local_time);
            
            // Calculate prayer times when GPS is available and we haven't calculated yet
            if (!prayer_times_calculated && current_gps.date_valid) {
                printk("Calculating prayer times with GPS coordinates...\n");
                
                // Set GPS coordinates for prayer calculations
                Lat = current_gps.latitude;
                Lng = current_gps.longitude;
                
                // Find nearest city to GPS coordinates and update HMI
                const city_data_t* nearest_city = find_nearest_city(current_gps.latitude, current_gps.longitude);
                if (nearest_city) {
                    printk("Nearest city found: %s (%s)\n", nearest_city->city_name, nearest_city->country);
                    hmi_set_city(nearest_city->city_name);
                } else {
                    printk("No city found, using coordinates\n");
                    char coord_str[20];
                    snprintf(coord_str, sizeof(coord_str), "%.2f,%.2f", current_gps.latitude, current_gps.longitude);
                    hmi_set_city(coord_str);
                }
                
                // Calculate prayer times
                prayer_myFloats_t prayers = prayerStruct();
                
                // Convert decimal hours to time strings and update display (new order with SHURUQ)
                char time_str[6];
                
                decimal_to_time_string(prayers.fajjir, time_str, sizeof(time_str));
                strcpy(current_prayers[0].time, time_str);   // Fajr
                
                decimal_to_time_string(prayers.sunRise, time_str, sizeof(time_str));
                strcpy(current_prayers[1].time, time_str);   // Shuruq (Sunrise)
                
                decimal_to_time_string(prayers.Dhuhur, time_str, sizeof(time_str));
                strcpy(current_prayers[2].time, time_str);   // Dhuhr
                
                decimal_to_time_string(prayers.Assr, time_str, sizeof(time_str));
                strcpy(current_prayers[3].time, time_str);   // Asr
                
                decimal_to_time_string(prayers.Maghreb, time_str, sizeof(time_str));
                strcpy(current_prayers[4].time, time_str);   // Maghrib
                
                decimal_to_time_string(prayers.Ishaa, time_str, sizeof(time_str));
                strcpy(current_prayers[5].time, time_str);   // Isha
                
                // Update HMI with calculated prayer times
                hmi_set_prayer_times(current_prayers, PRAYER_ASR);
                hmi_set_countdown("");
                
                // Force full update for prayer times (one-time)
                printk("About to force full update after prayer calculation...\n");
                printk("Current time before full update: '%s'\n", current_gps.time_str);
                hmi_force_full_update(display_dev);
                
                prayer_times_calculated = true;
                printk("Prayer times calculated and updated!\n");
                printk("Current time after full update: '%s'\n", current_gps.time_str);
            }
        }
        
        // Update display with selective updates
        hmi_update_display(display_dev);
        
        // 500ms delay for smooth time updates
        k_msleep(500);
    }
}