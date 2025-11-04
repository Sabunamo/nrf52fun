#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "font.h"
#ifdef USE_NEO6M_GPS
    #include "gps_neo6m.h"
#else
    #include "gps_neo7m.h"
#endif
#include "ili9341_tft.h"
#include "prayerTime.h"
#include "world_cities.h"
#include "speaker.h"

// External prayer time function
extern double convert_Gregor_2_Julian_Day(float d, int m, int y);

#define RESET_PIN     10   // P1.10 (RST pin)

// Variables for prayer calculations
double Lng = 0.0, Lat = 0.0, D = 0.0;

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

    // Backlight control (handled by display driver)
    printk("Backlight controlled by display driver\n");

    // Initialize GPS
    printk("Initializing GPS...\n");
    int gps_ret = gps_init();
    if (gps_ret != 0) {
        printk("GPS initialization failed: %d\n", gps_ret);
    }

    // Initialize speaker for Athan
    printk("Initializing Speaker...\n");
    int speaker_ret = speaker_init();
    if (speaker_ret != 0) {
        printk("Speaker initialization failed: %d\n", speaker_ret);
    } else {
        printk("Speaker initialized successfully\n");
    }

    // Allow GPS to start receiving data
    k_msleep(200);

    // Initialize with default prayer times (new order with SHURUQ)
    // Keep this for Athan triggering, but don't display until GPS is valid
    prayer_time_t current_prayers[PRAYER_COUNT] = {
        {"Fajr", "05:30", false},
        {"Shuruq", "06:45", false},
        {"Dhuhr", "12:15", false},
        {"Asr", "15:45", true},
        {"Maghrib", "18:20", false},
        {"Isha", "20:00", false}
    };

    // Don't set placeholder values - they will show on GPS waiting screen
    // Only set brightness (non-text element)
    hmi_set_brightness(75);

    // Force initial HMI display setup
    printk("Performing initial HMI display setup...\n");
    hmi_force_full_update(display_dev);

    // Allow initial display to complete
    k_msleep(300);

    printk("Setup complete. Starting HMI display loop...\n");

    bool prayer_times_calculated = false;

    // Backlight test variables
    uint32_t last_backlight_test = 0;
    const uint32_t backlight_interval = 30 * 1000; // 30 seconds in milliseconds

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
                // Use GPS DST function to get local time with automatic DST
                int offset = gps_get_local_time(local_time, sizeof(local_time));

                if (strlen(local_time) >= 8) {
                    last_seconds = (local_time[6] - '0') * 10 + (local_time[7] - '0');
                    printk("GPS UTC: %s -> Local (UTC%+d): %s\n",
                           current_gps.time_str, offset, local_time);
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
                    // Handle minute rollover - resync with GPS using DST-aware function
                    gps_get_local_time(local_time, sizeof(local_time));
                    if (strlen(local_time) >= 8) {
                        last_seconds = (local_time[6] - '0') * 10 + (local_time[7] - '0');
                    }
                } else {
                    // Update just the seconds part
                    local_time[6] = '0' + (last_seconds / 10);
                    local_time[7] = '0' + (last_seconds % 10);
                }
                last_second_update = now;
            }

            hmi_set_current_time(local_time);

            // Update next prayer highlight continuously when prayer times are calculated
            if (prayer_times_calculated) {
                static prayer_myFloats_t current_prayer_floats;

                // Update the prayer_myFloats_t structure with current prayer times
                int fajr_h = (current_prayers[0].time[0] - '0') * 10 + (current_prayers[0].time[1] - '0');
                int fajr_m = (current_prayers[0].time[3] - '0') * 10 + (current_prayers[0].time[4] - '0');
                current_prayer_floats.fajjir = fajr_h + (fajr_m / 60.0);

                int shuruq_h = (current_prayers[1].time[0] - '0') * 10 + (current_prayers[1].time[1] - '0');
                int shuruq_m = (current_prayers[1].time[3] - '0') * 10 + (current_prayers[1].time[4] - '0');
                current_prayer_floats.sunRise = shuruq_h + (shuruq_m / 60.0);

                int dhuhr_h = (current_prayers[2].time[0] - '0') * 10 + (current_prayers[2].time[1] - '0');
                int dhuhr_m = (current_prayers[2].time[3] - '0') * 10 + (current_prayers[2].time[4] - '0');
                current_prayer_floats.Dhuhur = dhuhr_h + (dhuhr_m / 60.0);

                int asr_h = (current_prayers[3].time[0] - '0') * 10 + (current_prayers[3].time[1] - '0');
                int asr_m = (current_prayers[3].time[3] - '0') * 10 + (current_prayers[3].time[4] - '0');
                current_prayer_floats.Assr = asr_h + (asr_m / 60.0);

                int maghrib_h = (current_prayers[4].time[0] - '0') * 10 + (current_prayers[4].time[1] - '0');
                int maghrib_m = (current_prayers[4].time[3] - '0') * 10 + (current_prayers[4].time[4] - '0');
                current_prayer_floats.Maghreb = maghrib_h + (maghrib_m / 60.0);

                int isha_h = (current_prayers[5].time[0] - '0') * 10 + (current_prayers[5].time[1] - '0');
                int isha_m = (current_prayers[5].time[3] - '0') * 10 + (current_prayers[5].time[4] - '0');
                current_prayer_floats.Ishaa = isha_h + (isha_m / 60.0);

                // Get next prayer index based on current time
                int next_prayer = get_next_prayer_index(local_time, &current_prayer_floats);

                // Update highlight if next prayer changed
                static int last_next_prayer = -1;
                if (next_prayer != last_next_prayer) {
                    hmi_set_prayer_times(current_prayers, next_prayer);
                    hmi_force_full_update(display_dev);
                    last_next_prayer = next_prayer;
                    printk("Next prayer updated to index: %d (%s)\n", next_prayer, current_prayers[next_prayer].name);
                }
            }

            // Check for prayer time and trigger LED (only for 5 main prayers, excluding Shuruq)
            if (prayer_times_calculated && strlen(local_time) >= 5) {
                static char last_prayer_triggered[6] = {0};
                char current_time_hhmm[6];
                snprintf(current_time_hhmm, sizeof(current_time_hhmm), "%c%c:%c%c",
                         local_time[0], local_time[1], local_time[3], local_time[4]);

                // Check if current time matches any of the 5 main prayer times (excluding Shuruq at index 1)
                for (int i = 0; i < PRAYER_COUNT; i++) {
                    if (i == 1) continue; // Skip Shuruq (index 1)

                    if (strcmp(current_time_hhmm, current_prayers[i].time) == 0) {
                        // Check if we haven't already triggered for this prayer time
                        if (strcmp(last_prayer_triggered, current_prayers[i].time) != 0) {
                            printk("PRAYER TIME REACHED: %s at %s\n", current_prayers[i].name, current_prayers[i].time);
                            strcpy(last_prayer_triggered, current_prayers[i].time);

                            // Play Athan through speaker
                            printk("Playing Athan for %s prayer...\n", current_prayers[i].name);
                            speaker_play_athan();

                            // Also trigger LED blinking
                            Pray_Athan();
                        }
                        break;
                    }
                }
            }

            // Calculate prayer times when GPS is available and we haven't calculated yet
            if (!prayer_times_calculated && current_gps.date_valid) {
                printk("Calculating prayer times with GPS coordinates...\n");

                // Set GPS coordinates for prayer calculations
                Lat = current_gps.latitude;
                Lng = current_gps.longitude;

                // Auto-configure timezone based on GPS coordinates
                gps_auto_configure_timezone();

                // Parse GPS date (format: DD/MM/YYYY) and set current Julian Day
                int day, month, year;
                if (sscanf(current_gps.date_str, "%d/%d/%d", &day, &month, &year) == 3) {
                    // Set global day variable for prayer calculations
                    D = (double)day;
                    // Convert to Julian Day for prayer time calculations
                    convert_Gregor_2_Julian_Day((float)day, month, year);
                }

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

                // Update HMI with calculated prayer times using dynamic next prayer detection
                int next_prayer = get_next_prayer_index(local_time, &prayers);
                hmi_set_prayer_times(current_prayers, next_prayer);
                hmi_set_countdown("");

                // Force full update for prayer times (one-time)
                hmi_force_full_update(display_dev);

                prayer_times_calculated = true;
            }
        }

        // Periodic status update (every 30 seconds)
        uint32_t current_time = k_uptime_get_32();
        if (current_time - last_backlight_test >= backlight_interval) {
            printk("=== Status Update (every 30 seconds) ===\n");
            printk("GPS Valid: %s\n", current_gps.valid ? "YES" : "NO");
            printk("Prayer Times Calculated: %s\n", prayer_times_calculated ? "YES" : "NO");
            printk("Display Working: YES\n");

            last_backlight_test = current_time;
        }

        // Update display with selective updates
        hmi_update_display(display_dev);

        // 500ms delay for smooth time updates
        k_msleep(500);
    }
}