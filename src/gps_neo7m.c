/**
 * @file gps_neo7m.c
 * @brief NEO-7M GPS module driver implementation
 *
 * Implements NMEA sentence parsing for NEO-7M GPS module
 * Processes GPRMC, GPGGA, GPGSA, GPGSV sentences
 */

#include "gps_neo7m.h"
#include "font.h"
#include "prayerTime.h"
#include "ili9341_parallel.h"
#include "world_cities.h"

static const struct device *gps_uart;
static char gps_buffer[GPS_BUFFER_SIZE];
static int gps_buffer_pos = 0;
struct gps_data current_gps = {0};

// Debug: Store last few NMEA sentences for display
#define DEBUG_NMEA_COUNT 5
static char debug_nmea[DEBUG_NMEA_COUNT][80];
static int debug_nmea_index = 0;
static uint32_t total_bytes_received = 0;
static uint32_t total_sentences_parsed = 0;
static uint32_t poll_in_success = 0;

// Forward declarations
static void process_nmea_sentence(char *sentence);
static void process_gprmc(char *sentence);
static void process_gpgga(char *sentence);
static void process_gpgsa(char *sentence);
static double nmea_to_decimal(const char *nmea_coord, char hemisphere);
static const char* get_short_day_name(const char* full_day_name);

#define GPS_THREAD_STACK_SIZE 2048
#define GPS_THREAD_PRIORITY 5

/**
 * @brief GPS polling thread for UART RX
 */
static void gps_poll_thread(void *p1, void *p2, void *p3)
{
    uint8_t byte;
    uint32_t poll_count = 0;

    printk("NEO-7M: GPS polling thread started - waiting for UART init\n");

    // Wait for UART to be initialized
    while (!gps_uart || !device_is_ready(gps_uart)) {
        k_msleep(100);
    }

    printk("NEO-7M: UART ready, starting to poll\n");

    uint32_t last_stats_print = 0;

    while (1) {
        poll_count++;

        // Poll for incoming data - keep polling while data available
        int poll_ret;
        while ((poll_ret = uart_poll_in(gps_uart, &byte)) == 0) {
            poll_in_success++;
            total_bytes_received++;

            // Process only printable ASCII characters for NMEA data
            if (byte >= 32 && byte <= 126) {
                if (gps_buffer_pos < GPS_BUFFER_SIZE - 1) {
                    gps_buffer[gps_buffer_pos] = byte;
                    gps_buffer_pos++;
                } else {
                    // Buffer overflow protection - reset to prevent corruption
                    gps_buffer_pos = 0;
                }
            } else if (byte == '\r') {
                // Ignore carriage return characters
                continue;
            } else if (byte == '\n') {
                // End of NMEA sentence - process if valid
                if (gps_buffer_pos > 0) {
                    gps_buffer[gps_buffer_pos] = '\0';

                    // Validate NMEA sentence format (must start with $ and have minimum length)
                    if (gps_buffer[0] == '$' && gps_buffer_pos > 6) {
                        // Store for debug display
                        strncpy(debug_nmea[debug_nmea_index], gps_buffer, 79);
                        debug_nmea[debug_nmea_index][79] = '\0';
                        debug_nmea_index = (debug_nmea_index + 1) % DEBUG_NMEA_COUNT;

                        // Disable NMEA spam to prevent RTT buffer overflow
                        // printk("NEO-7M NMEA: %s\n", gps_buffer);
                        process_nmea_sentence(gps_buffer);
                    }

                    gps_buffer_pos = 0;
                }
            }
        }

        // Print statistics every 5 seconds
        if (poll_count - last_stats_print >= 5000) {  // 5000 * 1ms = 5 seconds
            printk("NEO-7M Stats: %u bytes, %u sentences\n",
                   total_bytes_received, total_sentences_parsed);
            last_stats_print = poll_count;
        }

        // Poll at ~1000Hz (1ms interval)
        k_msleep(1);
    }
}

K_THREAD_DEFINE(gps_poll_tid, GPS_THREAD_STACK_SIZE,
                gps_poll_thread, NULL, NULL, NULL,
                GPS_THREAD_PRIORITY, 0, 0);

/**
 * @brief Convert NMEA coordinate format to decimal degrees
 */
static double nmea_to_decimal(const char *nmea_coord, char hemisphere)
{
    if (!nmea_coord || strlen(nmea_coord) < 4) {
        return 0.0;
    }

    double coord = atof(nmea_coord);
    int degrees = (int)(coord / 100);
    double minutes = coord - (degrees * 100);
    double decimal = degrees + (minutes / 60.0);

    // Apply direction (South and West are negative)
    if (hemisphere == 'S' || hemisphere == 'W') {
        decimal = -decimal;
    }

    return decimal;
}

/**
 * @brief Convert full day name to 3-character abbreviation
 */
static const char* get_short_day_name(const char* full_day_name)
{
    if (strcmp(full_day_name, "Sunday") == 0) return "Sun";
    if (strcmp(full_day_name, "Monday") == 0) return "Mon";
    if (strcmp(full_day_name, "Tuesday") == 0) return "Tue";
    if (strcmp(full_day_name, "Wednesday") == 0) return "Wed";
    if (strcmp(full_day_name, "Thursday") == 0) return "Thu";
    if (strcmp(full_day_name, "Friday") == 0) return "Fri";
    if (strcmp(full_day_name, "Saturday") == 0) return "Sat";
    return "???";
}

/**
 * @brief Process GPRMC (Recommended Minimum Course) NMEA sentence
 * Format: $GPRMC,time,status,lat,lat_dir,lon,lon_dir,speed,course,date,mag_var,checksum
 */
static void process_gprmc(char *sentence)
{
    char *token;
    char *tokens[15];
    int token_count = 0;

    token = strtok(sentence, ",");
    while (token != NULL && token_count < 15) {
        tokens[token_count++] = token;
        token = strtok(NULL, ",");
    }

    // Extract date (token[9] = DDMMYY format)
    if (token_count > 9 && tokens[9] && strlen(tokens[9]) >= 6) {
        bool is_date = true;
        for (int i = 0; i < 6 && i < strlen(tokens[9]); i++) {
            if (tokens[9][i] < '0' || tokens[9][i] > '9') {
                is_date = false;
                break;
            }
        }

        if (is_date) {
            snprintf(current_gps.date_str, sizeof(current_gps.date_str),
                    "%.2s/%.2s/20%.2s", tokens[9], tokens[9]+2, tokens[9]+4);
            current_gps.date_valid = true;

            // Calculate Hijri date and day of week
            int day, month, year;
            if (sscanf(current_gps.date_str, "%d/%d/%d", &day, &month, &year) == 3) {
                double julian_day = convert_Gregor_2_Julian_Day((float)day, month, year);
                hijri_date_t hijri_date = convert_Gregor_2_Hijri_Date((float)day, month, year, julian_day);

                snprintf(current_gps.hijri_date_str, sizeof(current_gps.hijri_date_str),
                        "%d/%d/%d", hijri_date.day, hijri_date.month, hijri_date.year);
                current_gps.hijri_valid = true;

                const char* day_name = day_Of_Weak(julian_day);
                const char* short_day_name = get_short_day_name(day_name);
                strncpy(current_gps.day_of_week, short_day_name, sizeof(current_gps.day_of_week) - 1);
                current_gps.day_of_week[sizeof(current_gps.day_of_week) - 1] = '\0';
                current_gps.day_valid = true;
            }
        }
    }

    // Extract time (token[1] = HHMMSS.SSS format)
    if (token_count > 1 && tokens[1] && strlen(tokens[1]) >= 6) {
        bool is_time = true;
        for (int i = 0; i < 6 && i < strlen(tokens[1]); i++) {
            if (tokens[1][i] < '0' || tokens[1][i] > '9') {
                is_time = false;
                break;
            }
        }

        if (is_time) {
            snprintf(current_gps.time_str, sizeof(current_gps.time_str),
                    "%.2s:%.2s:%.2s", tokens[1], tokens[1]+2, tokens[1]+4);
        }
    }

    // Process position data only from valid GPS fixes (status = 'A')
    if (token_count >= 10 && tokens[2][0] == 'A') {
        current_gps.latitude = nmea_to_decimal(tokens[3], tokens[4][0]);
        current_gps.longitude = nmea_to_decimal(tokens[5], tokens[6][0]);
        current_gps.lat_hemisphere = tokens[4][0];
        current_gps.lon_hemisphere = tokens[6][0];
        current_gps.valid = true;
    }
}

/**
 * @brief Process GPGGA (Global Positioning System Fix Data) NMEA sentence
 * Format: $GPGGA,time,lat,N/S,lon,E/W,quality,numSV,HDOP,alt,M,geoid,M,dgps_time,dgps_id,checksum
 */
static void process_gpgga(char *sentence)
{
    char *token;
    char *tokens[15];
    int token_count = 0;

    token = strtok(sentence, ",");
    while (token != NULL && token_count < 15) {
        tokens[token_count++] = token;
        token = strtok(NULL, ",");
    }

    if (token_count >= 10) {
        // Extract altitude (token[9])
        if (tokens[9] && strlen(tokens[9]) > 0) {
            int quality = atoi(tokens[6]);
            if (quality > 0) {
                current_gps.seeHeight = atof(tokens[9]);
                current_gps.seeHeight_valid = true;
            }
        }
    }
}

/**
 * @brief Process GPGSA (GPS DOP and Active Satellites) NMEA sentence
 * Format: $GPGSA,mode,fix_type,sat1,...,sat12,PDOP,HDOP,VDOP,checksum
 */
static void process_gpgsa(char *sentence)
{
    char *token;
    char *tokens[20];
    int token_count = 0;

    token = strtok(sentence, ",");
    while (token != NULL && token_count < 20) {
        tokens[token_count++] = token;
        token = strtok(NULL, ",");
    }

    // Token[2] contains fix type: 1=no fix, 2=2D fix, 3=3D fix
    if (token_count > 2 && tokens[2] && strlen(tokens[2]) > 0) {
        int fix_type = atoi(tokens[2]);
        // Update validity based on fix type
        if (fix_type >= 2) {
            // 2D or 3D fix available
        } else {
            // No fix
            current_gps.valid = false;
        }
    }
}

/**
 * @brief Process NMEA sentence dispatcher
 */
static void process_nmea_sentence(char *sentence)
{
    if (strlen(sentence) < 6) {
        return;
    }

    total_sentences_parsed++;

    // Process GPRMC sentences for lat, long, time, date, speed, course
    if (strncmp(sentence, "$GPRMC", 6) == 0 || strncmp(sentence, "$GNRMC", 6) == 0) {
        process_gprmc(sentence);
    }
    // Process GPGGA sentences for altitude, fix quality, satellites, HDOP
    else if (strncmp(sentence, "$GPGGA", 6) == 0 || strncmp(sentence, "$GNGGA", 6) == 0) {
        process_gpgga(sentence);
    }
    // Process GPGSA sentences for fix type and DOP
    else if (strncmp(sentence, "$GPGSA", 6) == 0 || strncmp(sentence, "$GNGSA", 6) == 0) {
        process_gpgsa(sentence);
    }
}

/**
 * @brief Initialize NEO-7M GPS module
 */
int gps_init(void)
{
    // Get GPS UART device from device tree
    gps_uart = DEVICE_DT_GET(GPS_UART_NODE);

    if (!device_is_ready(gps_uart)) {
        printk("NEO-7M: UART device not ready\n");
        return -1;
    }

    printk("NEO-7M: UART device is ready\n");
    printk("NEO-7M: Using POLLING mode at 9600 baud\n");
    printk("NEO-7M: GPS polling thread running in background\n");
    printk("NEO-7M: Wiring: GPS_TX->P0.08, GPS_RX->P0.06, VCC->3.3V/5V, GND->GND\n");

    // Send a test message on UART TX
    const char test_msg[] = "nRF52 NEO-7M Init\r\n";
    for (int i = 0; i < sizeof(test_msg) - 1; i++) {
        uart_poll_out(gps_uart, test_msg[i]);
    }
    printk("NEO-7M: Test message sent on UART TX\n");

    return 0;
}

/**
 * @brief Process GPS data (compatibility function)
 */
void gps_process_data(void)
{
    // Data is processed automatically via polling thread
}

/**
 * @brief Print GPS info to console
 */
void gps_print_info(void)
{
    if (current_gps.valid) {
        int lat_int = (int)(fabs(current_gps.latitude) * 1000000);
        int lon_int = (int)(fabs(current_gps.longitude) * 1000000);

        printk("NEO-7M: Lat: %d.%06d%c, Lon: %d.%06d%c\n",
               lat_int / 1000000, lat_int % 1000000, current_gps.lat_hemisphere,
               lon_int / 1000000, lon_int % 1000000, current_gps.lon_hemisphere);
        printk("NEO-7M: Time: %s UTC, Date: %s\n",
               current_gps.time_str, current_gps.date_str);

        if (current_gps.seeHeight_valid) {
            int alt_int = (int)current_gps.seeHeight;
            printk("NEO-7M: Altitude: %d m\n", alt_int);
        }
    } else {
        printk("NEO-7M: No fix\n");
    }
}

/**
 * @brief Display GPS data on LCD
 */
void display_gps_data(const struct device *display_dev, int x, int y)
{
    static int search_dots = 0;
    char status_str[40];

    // Always show byte counter and stats at top
    snprintf(status_str, sizeof(status_str), "RX:%u S:%u",
             total_bytes_received, total_sentences_parsed);
    ili9341_draw_string(x, y, status_str, COLOR_YELLOW, COLOR_BLACK, 1);

    // Show last received NMEA sentence for debugging
    if (debug_nmea_index > 0) {
        int last_idx = (debug_nmea_index - 1 + DEBUG_NMEA_COUNT) % DEBUG_NMEA_COUNT;
        if (debug_nmea[last_idx][0] != '\0') {
            char truncated[31];
            strncpy(truncated, debug_nmea[last_idx], 30);
            truncated[30] = '\0';
            ili9341_draw_string(x, y + 12, truncated, COLOR_GREEN, COLOR_BLACK, 1);
        }
    }

    if (current_gps.valid) {
        // GPS has fix - show data
        char time_str[32];
        char lat_str[32];
        char lon_str[32];
        char alt_str[32];
        char date_str[32];

        int lat_int = (int)(fabs(current_gps.latitude) * 1000000);
        int lon_int = (int)(fabs(current_gps.longitude) * 1000000);

        snprintf(time_str, sizeof(time_str), "%s UTC", current_gps.time_str);
        snprintf(lat_str, sizeof(lat_str), "%d.%06d%c",
                lat_int / 1000000, lat_int % 1000000, current_gps.lat_hemisphere);
        snprintf(lon_str, sizeof(lon_str), "%d.%06d%c",
                lon_int / 1000000, lon_int % 1000000, current_gps.lon_hemisphere);

        if (current_gps.date_valid) {
            snprintf(date_str, sizeof(date_str), "%s", current_gps.date_str);
        } else {
            strcpy(date_str, "---");
        }

        if (current_gps.seeHeight_valid) {
            int alt_int = (int)current_gps.seeHeight;
            snprintf(alt_str, sizeof(alt_str), "%dm", alt_int);
        } else {
            strcpy(alt_str, "---");
        }

        int line_y = y + 30;

        // Time
        ili9341_draw_string(x, line_y, "Time:", COLOR_CYAN, COLOR_BLACK, 1);
        ili9341_draw_string(x + 50, line_y, time_str, COLOR_WHITE, COLOR_BLACK, 2);
        line_y += 20;

        // Latitude
        ili9341_draw_string(x, line_y, "Lat:", COLOR_CYAN, COLOR_BLACK, 1);
        ili9341_draw_string(x + 50, line_y, lat_str, COLOR_WHITE, COLOR_BLACK, 2);
        line_y += 20;

        // Longitude
        ili9341_draw_string(x, line_y, "Long:", COLOR_CYAN, COLOR_BLACK, 1);
        ili9341_draw_string(x + 50, line_y, lon_str, COLOR_WHITE, COLOR_BLACK, 2);
        line_y += 20;

        // Altitude
        ili9341_draw_string(x, line_y, "Alt:", COLOR_CYAN, COLOR_BLACK, 1);
        ili9341_draw_string(x + 50, line_y, alt_str, COLOR_MAGENTA, COLOR_BLACK, 2);
        line_y += 20;

        // Date
        ili9341_draw_string(x, line_y, "Date:", COLOR_CYAN, COLOR_BLACK, 1);
        ili9341_draw_string(x + 50, line_y, date_str, COLOR_GREEN, COLOR_BLACK, 2);

    } else {
        // Searching for fix
        char search_msg[32];
        search_dots = (search_dots + 1) % 4;
        switch(search_dots) {
            case 0: strcpy(search_msg, "Searching   "); break;
            case 1: strcpy(search_msg, "Searching.  "); break;
            case 2: strcpy(search_msg, "Searching.. "); break;
            case 3: strcpy(search_msg, "Searching..."); break;
        }

        int line_y = y + 30;

        ili9341_draw_string(x, line_y, "No satellite fix", COLOR_YELLOW, COLOR_BLACK, 2);
        line_y += 20;
        ili9341_draw_string(x, line_y, search_msg, COLOR_CYAN, COLOR_BLACK, 2);
        line_y += 25;

        // Show time even without fix
        if (current_gps.time_str[0] != '\0') {
            char time_display[32];
            snprintf(time_display, sizeof(time_display), "Time: %s UTC", current_gps.time_str);
            ili9341_draw_string(x, line_y, time_display, COLOR_WHITE, COLOR_BLACK, 1);
            line_y += 12;
        }

        // Show date if available
        if (current_gps.date_valid && current_gps.date_str[0] != '\0') {
            char date_display[32];
            snprintf(date_display, sizeof(date_display), "Date: %s", current_gps.date_str);
            ili9341_draw_string(x, line_y, date_display, COLOR_GREEN, COLOR_BLACK, 1);
            line_y += 12;
        }

        line_y += 10;
        ili9341_draw_string(x, line_y, "Move to window", COLOR_RED, COLOR_BLACK, 1);
        line_y += 12;
        ili9341_draw_string(x, line_y, "for satellite lock", COLOR_RED, COLOR_BLACK, 1);
    }
}

/**
 * @brief Get current date string
 */
const char* gps_get_today_date(void)
{
    if (current_gps.date_valid) {
        return current_gps.date_str;
    } else {
        return "No Date";
    }
}

/**
 * @brief Send test data for loopback testing
 */
void gps_send_test_data(int count)
{
    if (!gps_uart) {
        return;
    }

    char test_msg[64];
    snprintf(test_msg, sizeof(test_msg), "$GPTEST,%d,NEO7M,OK*FF\r\n", count);

    for (int i = 0; test_msg[i] != '\0'; i++) {
        uart_poll_out(gps_uart, test_msg[i]);
    }

    printk("NEO-7M: UART TX test #%d sent\n", count);
}

/**
 * @brief Get GPS statistics
 */
void gps_get_stats(uint32_t *bytes_rx, uint32_t *sentences_parsed)
{
    if (bytes_rx) {
        *bytes_rx = total_bytes_received;
    }
    if (sentences_parsed) {
        *sentences_parsed = total_sentences_parsed;
    }
}

/**
 * @brief Print raw NMEA sentences for debugging
 */
void gps_print_raw_data(void)
{
    printk("\n========== RAW GPS NMEA DATA ==========\n");
    printk("Total bytes: %u, Total sentences: %u\n", total_bytes_received, total_sentences_parsed);
    printk("Last %d NMEA sentences received:\n", DEBUG_NMEA_COUNT);

    // Print the last DEBUG_NMEA_COUNT sentences in order
    for (int i = 0; i < DEBUG_NMEA_COUNT; i++) {
        int idx = (debug_nmea_index + i) % DEBUG_NMEA_COUNT;
        if (debug_nmea[idx][0] != '\0') {
            printk("  [%d] %s\n", i+1, debug_nmea[idx]);
        }
    }

    printk("GPS Valid: %s\n", current_gps.valid ? "YES" : "NO");
    if (current_gps.valid) {
        printk("Position: %.6f%c, %.6f%c\n",
               fabs(current_gps.latitude), current_gps.lat_hemisphere,
               fabs(current_gps.longitude), current_gps.lon_hemisphere);
        printk("Time: %s UTC, Date: %s\n", current_gps.time_str, current_gps.date_str);
    }
    printk("=======================================\n\n");
}

/**
 * @brief Calculate day of week (0=Sunday, 1=Monday, ..., 6=Saturday)
 * Uses Zeller's congruence algorithm
 */
static int calculate_day_of_week(int day, int month, int year)
{
    if (month < 3) {
        month += 12;
        year--;
    }
    int q = day;
    int m = month;
    int k = year % 100;
    int j = year / 100;
    int h = (q + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    // Convert to 0=Sunday format
    return (h + 6) % 7;
}

/**
 * @brief Find last Sunday of a given month/year
 * @return Day of month (1-31) of the last Sunday
 */
static int find_last_sunday(int month, int year)
{
    // Days in each month
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // Check for leap year
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
        days_in_month[1] = 29;
    }

    int last_day = days_in_month[month - 1];

    // Find last Sunday by checking backwards from end of month
    for (int day = last_day; day >= 1; day--) {
        if (calculate_day_of_week(day, month, year) == 0) {  // 0 = Sunday
            return day;
        }
    }
    return last_day;  // Fallback (shouldn't happen)
}

/**
 * @brief Check if date is in DST period (European rules)
 * DST starts: Last Sunday of March at 2:00 AM
 * DST ends: Last Sunday of October at 3:00 AM
 * @return true if in DST period, false otherwise
 */
static bool is_dst_active(int day, int month, int year, int hour)
{
    // DST not active from November to February
    if (month < 3 || month > 10) {
        return false;
    }

    // Definitely active from April to September
    if (month > 3 && month < 10) {
        return true;
    }

    // March: Check if after last Sunday at 2:00 AM
    if (month == 3) {
        int last_sunday = find_last_sunday(3, year);
        if (day > last_sunday) {
            return true;
        } else if (day == last_sunday && hour >= 2) {
            return true;
        }
        return false;
    }

    // October: Check if before last Sunday at 3:00 AM
    if (month == 10) {
        int last_sunday = find_last_sunday(10, year);
        if (day < last_sunday) {
            return true;
        } else if (day == last_sunday && hour < 3) {
            return true;
        }
        return false;
    }

    return false;
}

/**
 * @brief Get local time with automatic DST adjustment (CET/CEST)
 * @param local_time Output buffer for local time (must be at least 11 bytes)
 * @param max_len Size of output buffer
 * @return Timezone offset applied (1 for CET, 2 for CEST, 0 if invalid)
 */
int gps_get_local_time(char *local_time, size_t max_len)
{
    if (!current_gps.time_str[0] || !current_gps.date_valid || !local_time || max_len < 11) {
        if (local_time && max_len > 0) {
            snprintf(local_time, max_len, "--:--:--");
        }
        return 0;
    }

    int hours, minutes, seconds;
    if (sscanf(current_gps.time_str, "%d:%d:%d", &hours, &minutes, &seconds) != 3) {
        snprintf(local_time, max_len, "--:--:--");
        return 0;
    }

    int day, month, year;
    if (sscanf(current_gps.date_str, "%d/%d/%d", &day, &month, &year) != 3) {
        snprintf(local_time, max_len, "--:--:--");
        return 0;
    }

    // Determine timezone offset (CET = UTC+1, CEST = UTC+2)
    int offset = is_dst_active(day, month, year, hours) ? 2 : 1;

    // Apply offset
    hours += offset;

    // Handle day wraparound
    if (hours >= 24) {
        hours -= 24;
    } else if (hours < 0) {
        hours += 24;
    }

    snprintf(local_time, max_len, "%02d:%02d:%02d", hours, minutes, seconds);
    return offset;
}

/**
 * @brief Auto-configure timezone based on GPS coordinates
 * Uses nearest city timezone data with fallback to longitude calculation
 */
void gps_auto_configure_timezone(void)
{
    if (!current_gps.valid) {
        printk("NEO-7M: Cannot auto-configure timezone - GPS not valid\n");
        return;
    }

    // Calculate timezone from longitude (15 degrees per hour) as fallback
    double tz_calc = current_gps.longitude / 15.0;
    int calculated_tz = (int)(tz_calc >= 0 ? tz_calc + 0.5 : tz_calc - 0.5);

    // Clamp to valid range
    if (calculated_tz < -12) calculated_tz = -12;
    if (calculated_tz > 14) calculated_tz = 14;

    // Find nearest city to get the political timezone
    const city_data_t* nearest_city = find_nearest_city(current_gps.latitude, current_gps.longitude);

    int final_tz = calculated_tz;  // Default to calculated

    if (nearest_city) {
        int city_tz = nearest_city->timezone_offset;

        printk("NEO-7M: Nearest city: %s (%s) has timezone UTC%+d\n",
               nearest_city->city_name, nearest_city->country, city_tz);
        printk("NEO-7M: Calculated timezone from longitude: UTC%+d\n", calculated_tz);

        // Compare calculated vs city timezone
        if (calculated_tz == city_tz) {
            printk("NEO-7M: Calculated and city timezones MATCH - using UTC%+d\n", calculated_tz);
            final_tz = calculated_tz;
        } else {
            printk("NEO-7M: Calculated (UTC%+d) and city (UTC%+d) timezones DIFFER\n",
                   calculated_tz, city_tz);
            printk("NEO-7M: Using city timezone UTC%+d (political boundary)\n", city_tz);
            final_tz = city_tz;
        }
    } else {
        printk("NEO-7M: No nearest city found - using calculated timezone UTC%+d\n", calculated_tz);
    }

    printk("NEO-7M: Longitude: %.4f, Final timezone: UTC%+d\n",
           current_gps.longitude, final_tz);

    // Update prayer time timezone
    prayer_set_timezone(final_tz);

    printk("NEO-7M: Timezone configured to UTC%+d\n", final_tz);
}
