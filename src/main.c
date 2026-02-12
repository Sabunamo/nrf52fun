/* Firmware Version */
#define FW_VERSION "2026.02.12"
#define FW_NAME   "nrf52fun Prayer Clock"

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
#include "bme280_sensor.h"
#include "pmodals_sensor.h"
#include "sd_card.h"
#ifdef CONFIG_FT6206_TOUCH
#include "ft6206_touch.h"
#endif
#ifdef CONFIG_SRAMLINK
#include "sramlink/sramlink_app.h"
#endif

/* LED for SRAMLink pairing confirmation */
#ifdef CONFIG_SRAMLINK
#define PAIRING_LED_NODE DT_ALIAS(led0)
#if DT_NODE_EXISTS(PAIRING_LED_NODE)
static const struct gpio_dt_spec pairing_led = GPIO_DT_SPEC_GET(PAIRING_LED_NODE, gpios);
#endif
#endif /* CONFIG_SRAMLINK */

// External prayer time function
extern double convert_Gregor_2_Julian_Day(float d, int m, int y);

#define RESET_PIN     10   // P1.10 (RST pin)
#define RELAY_PIN     7    // P0.07 (Relay signal via BC547)

// Variables for prayer calculations
double Lng = 0.0, Lat = 0.0, D = 0.0;

// SD card availability (set once at init, read by athan thread)
static bool sd_card_available = false;

// Flag to pause main loop heavy work during athan playback
static volatile bool athan_playing = false;

// Athan playback thread (non-blocking)
static struct k_event athan_event;
#define ATHAN_TRIGGER_BIT BIT(0)

static void athan_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    while (1) {
        k_event_wait(&athan_event, ATHAN_TRIGGER_BIT, true, K_FOREVER);

        printk("Athan thread: Starting playback...\n");
        athan_playing = true;
        if (sd_card_available) {
            int ret = sd_card_play_wav_file("SD:/athan.wav", 62500);
            if (ret != 0) {
                printk("Athan thread: WAV failed (%d), using built-in tones\n", ret);
                speaker_play_athan();
            }
        } else {
            speaker_play_athan();
        }
        athan_playing = false;

        Pray_Athan();
        printk("Athan thread: Complete.\n");
    }
}

#define ATHAN_THREAD_STACK_SIZE 2048
K_THREAD_DEFINE(athan_tid, ATHAN_THREAD_STACK_SIZE,
                athan_thread_entry, NULL, NULL, NULL,
                K_PRIO_PREEMPT(7), 0, 0);

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
    printk("=== %s - FW %s ===\n", FW_NAME, FW_VERSION);

    // Initialize athan event
    k_event_init(&athan_event);

    // Configure relay on P0.26
    const struct device *gpio0_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (!device_is_ready(gpio0_dev)) {
        printk("GPIO0 device not ready\n");
        return;
    }
    gpio_pin_configure(gpio0_dev, RELAY_PIN, GPIO_OUTPUT_INACTIVE);
    bool relay_on = false;

    // Configure Button 1 (SW0) for relay toggle
    static const struct gpio_dt_spec relay_button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
    if (!gpio_is_ready_dt(&relay_button)) {
        printk("Button GPIO not ready\n");
    } else {
        gpio_pin_configure_dt(&relay_button, GPIO_INPUT);
    }
    bool relay_btn_last = false;

    printk("Relay initialized on P0.26 (toggle with Button 1)\n");

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

    // Initialize BME280 sensor
    printk("Initializing BME280 sensor...\n");
    int bme_ret = bme280_sensor_init();
    if (bme_ret != 0) {
        printk("BME280 initialization failed: %d (sensor may not be connected)\n", bme_ret);
    } else {
        printk("BME280 sensor initialized successfully\n");
    }

    // Initialize PmodALS ambient light sensor
    // TEMPORARILY DISABLED for debugging
    // printk("Initializing PmodALS sensor...\n");
    // int als_ret = pmodals_init();
    // if (als_ret != 0) {
    //     printk("PmodALS initialization failed: %d (sensor may not be connected)\n", als_ret);
    // } else {
    //     printk("PmodALS sensor initialized successfully\n");
    // }

    // Initialize SD Card
    printk("Initializing SD Card on SPI4 (CS: P1.06)...\n");
    k_msleep(5);
    printk("Note: This may take 5-10 seconds if no card is present\n");
    k_msleep(5);
    sd_card_set_display_device(display_dev);  // Set display for BMP images
    int sd_ret = sd_card_init();
    k_msleep(5);
    printk(">>> SD Card init returned: %d <<<\n", sd_ret);
    if (sd_ret != 0) {
        printk("SD Card initialization FAILED: error %d\n", sd_ret);
        printk("Possible reasons:\n");
        printk("  - No SD card inserted\n");
        printk("  - Card not formatted (needs FAT/FAT32)\n");
        printk("  - Bad connection on SPI bus\n");
        printk("  - CS pin (P1.06) not connected\n");
        printk("Continuing without SD card support...\n");
    } else {
        printk("SD Card initialized successfully!\n");

        // Get and display SD card size
        uint32_t block_count = 0, block_size = 0;
        if (sd_card_get_size(&block_count, &block_size) == 0) {
            uint64_t total_bytes = (uint64_t)block_count * block_size;
            uint32_t total_mb = (uint32_t)(total_bytes / (1024 * 1024));
            printk("SD Card Size: %u MB (%u blocks x %u bytes)\n",
                   total_mb, block_count, block_size);
        }

        // Display woof.bmp image from SD card
        printk("Displaying woof.bmp from SD card...\n");
        int bmp_ret = sd_card_display_bmp_file("SD:/woof.bmp");
        if (bmp_ret == 0) {
            printk("BMP image displayed successfully!\n");
            printk("Image will be shown for 3 seconds...\n");
            k_msleep(3000);  // Show image for 3 seconds
        } else {
            printk("Failed to display woof.bmp: error %d\n", bmp_ret);
            printk("Make sure woof.bmp exists in root directory of SD card\n");
        }
    }

#ifdef CONFIG_FT6206_TOUCH
    // Initialize FT6206 touch screen
    printk(">>> FT6206 INIT START <<<\n");
    k_msleep(5);
    int touch_ret = ft6206_init();
    if (touch_ret != 0) {
        printk("FT6206 initialization failed: %d (touch may not be connected)\n", touch_ret);
    } else {
        printk("FT6206 touch screen initialized successfully\n");
    }
#endif

#ifdef CONFIG_SRAMLINK
    // Initialize SRAMLink protocol
    printk(">>> SRAMLINK INIT START <<<\n");
    k_msleep(10);
    sramlink_app_config_t sl_config = {
        .radio_channel = CONFIG_SRAMLINK_RADIO_CHANNEL,
        .tx_power_dbm = CONFIG_SRAMLINK_TX_POWER,
        .enable_rx = true,
        .continuous_rx = true,
        .device_type = 0,
        .button_cb = NULL,
        .status_cb = NULL,
    };
    int sl_ret = sramlink_app_init(&sl_config);
    if (sl_ret != 0) {
        printk("SRAMLink initialization failed: %d\n", sl_ret);
    } else {
        sramlink_app_start();
        printk("SRAMLink started on channel %d, device_id=0x%08X\n",
               sramlink_app_get_channel(), sramlink_app_get_device_id());
    }

#if DT_NODE_EXISTS(PAIRING_LED_NODE)
    /* Configure LED for pairing feedback */
    if (gpio_is_ready_dt(&pairing_led)) {
        gpio_pin_configure_dt(&pairing_led, GPIO_OUTPUT_INACTIVE);
    }
#endif

    /* Shared key used — devices communicate immediately */
#endif

    // Allow GPS to start receiving data
    k_msleep(200);

    // Initialize with default prayer times (new order with SHURUQ)
    prayer_time_t current_prayers[PRAYER_COUNT] = {
        {"Fajr", "05:30", false},
        {"Shuruq", "06:45", false},
        {"Dhuhr", "12:15", false},
        {"Asr", "15:45", true},
        {"Maghrib", "18:20", false},
        {"Isha", "20:00", false}
    };

    // Set initial HMI data with dynamic next prayer detection
    // Create default prayer structure for initial calculation
    prayer_myFloats_t default_prayers = {
        .fajjir = 5.5,    // 05:30
        .sunRise = 6.75,  // 06:45
        .Dhuhur = 12.25,  // 12:15
        .Assr = 15.75,    // 15:45
        .Maghreb = 18.33, // 18:20
        .Ishaa = 20.0     // 20:00
    };
    int next_prayer = get_next_prayer_index("--:--", &default_prayers);
    hmi_set_prayer_times(current_prayers, next_prayer);
    hmi_set_countdown("Calculating...");
    hmi_set_city("GPS Location...");

    // Set default weather display (no temperature sensor)
    char temp_str[20];
    snprintf(temp_str, sizeof(temp_str), "--°C");
    hmi_set_weather(temp_str);

    hmi_set_current_time("--:--");
    hmi_set_brightness(75);

    // Force initial HMI display setup
    printk("Performing initial HMI display setup...\n");
    hmi_force_full_update(display_dev);

    // Allow initial display to complete
    k_msleep(300);

    printk("Setup complete. Starting HMI display loop...\n");

    bool prayer_times_calculated = false;
    sd_card_available = (sd_ret == 0);  // Track if SD card is working

    // Backlight test variables
    uint32_t last_backlight_test = 0;
    const uint32_t backlight_interval = 30 * 1000; // 30 seconds in milliseconds

    // BME280 sensor reading variables
    uint32_t last_sensor_read = 0;
    const uint32_t sensor_interval = 5 * 1000; // Read sensor every 5 seconds

    // PmodALS sensor reading variables
    uint32_t last_als_read = 0;
    const uint32_t als_interval = 2 * 1000; // Read ambient light every 2 seconds

#ifdef CONFIG_FT6206_TOUCH
    // Touch screen variables
    uint32_t last_touch_process = 0;
    const uint32_t touch_debounce = 500; // 500ms debounce between screen toggles
#endif

    // Keep running and update display
    while (1) {
        // Pause entire main loop while athan is playing to avoid audio glitches
        if (athan_playing) {
            k_msleep(100);
            continue;
        }

        // Process GPS data using polling
        gps_process_data();

#ifdef CONFIG_SRAMLINK
        // Process SRAMLink messages
        sramlink_app_process();

        // Check if pairing just completed — blink LED 3x
        if (sramlink_app_pairing_just_completed()) {
            printk(">>> PAIRING SUCCESS — blinking LED <<<\n");
#if DT_NODE_EXISTS(PAIRING_LED_NODE)
            for (int i = 0; i < 3; i++) {
                gpio_pin_set_dt(&pairing_led, 1);
                k_msleep(200);
                gpio_pin_set_dt(&pairing_led, 0);
                k_msleep(200);
            }
#endif
        }

        // Send periodic test message for pairing discovery
        static uint32_t last_sramlink_tx = 0;
        uint32_t sl_now = k_uptime_get_32();
        if (sl_now - last_sramlink_tx >= 5000) {  // Every 5 seconds
            last_sramlink_tx = sl_now;
            // Send a status report to trigger implicit pairing on receivers
            int tx_ret = sramlink_app_send_status_report(1, 2, 3, 3300, 0);
            printk("SRAMLink TX status_report: %s (paired=%d)\n",
                   tx_ret == 0 ? "OK" : "FAIL",
                   sramlink_app_get_paired_count());
        }
#endif

#ifdef CONFIG_FT6206_TOUCH
        // Process touch screen input
        if (ft6206_is_ready()) {
            ft6206_read();
            if (ft6206_is_touched() && ft6206_touch.state == TOUCH_PRESSED) {
                uint16_t tx, ty;
                ft6206_get_point(&tx, &ty);
                uint32_t now_touch = k_uptime_get_32();

                // Debounce: only process if enough time has passed
                if (now_touch - last_touch_process >= touch_debounce) {
                    last_touch_process = now_touch;

                    if (hmi_get_screen_mode() == SCREEN_HOME) {
                        // On home screen: check if info icon was touched
                        if (hmi_check_info_icon_touch(tx, ty)) {
                            printk("Touch: Info icon pressed at (%d, %d)\n", tx, ty);

                            // Update info data with latest values before showing
                            bme280_data_t sensor_snap;
                            bool sensor_ok = (bme280_sensor_get_data(&sensor_snap) == 0);
                            hmi_update_info_data(
                                current_gps.latitude, current_gps.longitude,
                                current_gps.lat_hemisphere, current_gps.lon_hemisphere,
                                current_gps.seeHeight, current_gps.seeHeight_valid,
                                sensor_ok ? sensor_snap.temperature : 0,
                                sensor_ok ? sensor_snap.pressure : 0,
                                sensor_ok ? sensor_snap.humidity : 0,
                                sensor_ok);

                            hmi_set_screen_mode(SCREEN_INFO, display_dev);
                        }
                    } else {
                        // On info screen: any touch goes back to home
                        printk("Touch: Returning to home screen from (%d, %d)\n", tx, ty);
                        hmi_set_screen_mode(SCREEN_HOME, display_dev);
                    }
                }
            }
        }
#endif

        // Read PmodALS ambient light sensor periodically for auto-brightness
        // TEMPORARILY DISABLED for debugging
        // uint32_t als_time = k_uptime_get_32();
        // if (pmodals_is_ready() && (als_time - last_als_read >= als_interval)) {
        //     pmodals_data_t als_data;
        //     int als_read_ret = pmodals_read(&als_data);

        //     if (als_read_ret == 0 && als_data.valid) {
        //         // Update display brightness automatically
        //         hmi_set_brightness(als_data.brightness_pct);

        //         printk("PmodALS: Raw=%d, Lux=%d, Auto-Brightness=%d%%\n",
        //                als_data.raw_value, als_data.lux, als_data.brightness_pct);
        //     }

        //     last_als_read = als_time;
        // }

        // Read BME280 sensor periodically
        uint32_t sensor_time = k_uptime_get_32();
        if (bme280_sensor_is_ready() && (sensor_time - last_sensor_read >= sensor_interval)) {
            bme280_data_t sensor_data;
            int read_ret = bme280_sensor_read(&sensor_data);

            if (read_ret == 0 && sensor_data.valid) {
                // Update temperature display
                char temp_display[20];
                snprintf(temp_display, sizeof(temp_display), "%.1f°C", (double)sensor_data.temperature);
                hmi_set_weather(temp_display);

                // Update info screen data with latest sensor + GPS values
                if (hmi_get_screen_mode() == SCREEN_INFO) {
                    hmi_update_info_data(
                        current_gps.latitude, current_gps.longitude,
                        current_gps.lat_hemisphere, current_gps.lon_hemisphere,
                        current_gps.seeHeight, current_gps.seeHeight_valid,
                        sensor_data.temperature, sensor_data.pressure,
                        sensor_data.humidity, true);
                }

                printk("BME280: %.1f°C, %.1f%%, %.1fhPa\n",
                       (double)sensor_data.temperature, (double)sensor_data.humidity, (double)sensor_data.pressure);
            }

            last_sensor_read = sensor_time;
        }

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

                // Check if current time matches any prayer time
                for (int i = 0; i < PRAYER_COUNT; i++) {
                    if (strcmp(current_time_hhmm, current_prayers[i].time) == 0) {
                        // Check if we haven't already triggered for this prayer time
                        if (strcmp(last_prayer_triggered, current_prayers[i].time) != 0) {
                            printk("PRAYER TIME REACHED: %s at %s\n", current_prayers[i].name, current_prayers[i].time);
                            strcpy(last_prayer_triggered, current_prayers[i].time);

                            // Relay ON at Maghrib, OFF at Isha
                            if (i == 4) { // Maghrib
                                gpio_pin_set(gpio0_dev, RELAY_PIN, 1);
                                relay_on = true;
                                printk("Relay ON at Maghrib\n");
                            } else if (i == 5) { // Isha
                                gpio_pin_set(gpio0_dev, RELAY_PIN, 0);
                                relay_on = false;
                                printk("Relay OFF at Isha\n");
                            }

                            // Trigger athan in background thread (skip Shuruq - no athan)
                            if (i != 1) {
                                printk("Triggering athan thread for %s prayer...\n", current_prayers[i].name);
                                k_event_post(&athan_event, ATHAN_TRIGGER_BIT);
                            }

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
#ifdef CONFIG_FT6206_TOUCH
            printk("FT6206 Touch: %s\n", ft6206_is_ready() ? "READY" : "NOT INITIALIZED");
#endif
#ifdef CONFIG_SRAMLINK
            printk("SRAMLink: paired=%d, device_id=0x%08X\n",
                   sramlink_app_get_paired_count(), sramlink_app_get_device_id());
#endif

            // Print raw GPS NMEA data for debugging
            gps_print_raw_data();

            last_backlight_test = current_time;
        }

        // Check Button 1 for relay toggle
        bool relay_btn_now = gpio_pin_get_dt(&relay_button);
        if (relay_btn_now && !relay_btn_last) {
            relay_on = !relay_on;
            gpio_pin_set(gpio0_dev, RELAY_PIN, (int)relay_on);
            printk("Relay %s\n", relay_on ? "ON" : "OFF");
        }
        relay_btn_last = relay_btn_now;

        // Update display with selective updates
        hmi_update_display(display_dev);

        // 500ms delay for smooth time updates
        k_msleep(500);
    }
}