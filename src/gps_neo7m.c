/**
 * @file gps_neo7m.c
 * @brief NEO-7M GPS module driver implementation
 *
 * Implements NMEA sentence parsing for NEO-7M GPS module
 * Processes GPRMC, GPGGA, GPGSA, GPGSV sentences
 */

#include "gps_neo7m.h"
#include "prayerTime.h"
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

                // Old algorithm (for comparison)
                hijri_date_t hijri_old = convert_Gregor_2_Hijri_Date((float)day, month, year, julian_day);

                // New Tabular algorithm (more accurate)
                hijri_date_t hijri_new = convert_JD_to_Hijri_Tabular(julian_day);

                // Use the new tabular result for display
                snprintf(current_gps.hijri_date_str, sizeof(current_gps.hijri_date_str),
                        "%d/%d/%d", hijri_new.day, hijri_new.month, hijri_new.year);
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

// ============================================================================
// DST (Daylight Saving Time) support
// ============================================================================

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
    return (h + 6) % 7;  // 0=Sunday
}

/**
 * @brief Find last Sunday of a given month/year
 */
static int find_last_sunday(int month, int year)
{
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
        days_in_month[1] = 29;
    }
    int last_day = days_in_month[month - 1];
    for (int day = last_day; day >= 1; day--) {
        if (calculate_day_of_week(day, month, year) == 0) {
            return day;
        }
    }
    return last_day;
}

/**
 * @brief Find the Nth Sunday of a given month/year (for US DST rules)
 * @param n Which Sunday (1=first, 2=second, etc.)
 */
static int find_nth_sunday(int month, int year, int n)
{
    int count = 0;
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
        days_in_month[1] = 29;
    }
    for (int day = 1; day <= days_in_month[month - 1]; day++) {
        if (calculate_day_of_week(day, month, year) == 0) {
            count++;
            if (count == n) return day;
        }
    }
    return 1;  // Fallback
}

// DST rule types
#define DST_NONE 0
#define DST_EU   1  // Last Sun March 1:00 UTC → Last Sun October 1:00 UTC
#define DST_US   2  // 2nd Sun March ~7:00 UTC → 1st Sun November ~6:00 UTC

// Use city database value instead of table override
#define TZ_USE_CITY -99

/**
 * @brief Country timezone & DST lookup table
 *
 * Provides the CORRECT standard timezone for each country (overriding
 * potentially wrong values in the city database) and the DST rule.
 * Countries not in this table use the city database timezone as-is (no DST).
 *
 * For multi-timezone countries (US, CA), std_tz = TZ_USE_CITY so the
 * per-city timezone_offset is used instead.
 */
typedef struct {
    char cc[3];       // ISO 3166-1 alpha-2 country code
    int8_t std_tz;    // Standard timezone (UTC offset), or TZ_USE_CITY
    uint8_t dst_rule; // DST_NONE, DST_EU, or DST_US
} country_tz_t;

static const country_tz_t country_tz_table[] = {
    // === EU DST countries ===
    // WET zone (UTC+0 standard, UTC+1 summer)
    {"GB",  0, DST_EU}, {"PT",  0, DST_EU},
    // CET zone (UTC+1 standard, UTC+2 summer)
    {"FR",  1, DST_EU}, {"DE",  1, DST_EU}, {"IT",  1, DST_EU},
    {"ES",  1, DST_EU}, {"NL",  1, DST_EU}, {"BE",  1, DST_EU},
    {"CH",  1, DST_EU}, {"AT",  1, DST_EU}, {"CZ",  1, DST_EU},
    {"SK",  1, DST_EU}, {"HU",  1, DST_EU}, {"PL",  1, DST_EU},
    {"RS",  1, DST_EU}, {"HR",  1, DST_EU}, {"BA",  1, DST_EU},
    {"SI",  1, DST_EU}, {"MK",  1, DST_EU}, {"ME",  1, DST_EU},
    {"AL",  1, DST_EU}, {"XK",  1, DST_EU}, {"MT",  1, DST_EU},
    {"NO",  1, DST_EU}, {"SE",  1, DST_EU}, {"DK",  1, DST_EU},
    // EET zone (UTC+2 standard, UTC+3 summer)
    {"FI",  2, DST_EU}, {"RO",  2, DST_EU}, {"BG",  2, DST_EU},
    {"GR",  2, DST_EU}, {"CY",  2, DST_EU}, {"LT",  2, DST_EU},
    {"LV",  2, DST_EU}, {"EE",  2, DST_EU}, {"UA",  2, DST_EU},
    {"MD",  2, DST_EU},
    // === US/Canada DST (multi-timezone, use city values) ===
    {"US", TZ_USE_CITY, DST_US}, {"CA", TZ_USE_CITY, DST_US},
    // === Non-DST timezone corrections (city DB has wrong values) ===
    {"TR",  3, DST_NONE},  // Turkey: permanent UTC+3 (city DB has +4)
    {"EG",  2, DST_NONE},  // Egypt: permanent UTC+2 (city DB has +3)
};

#define COUNTRY_TZ_TABLE_SIZE (sizeof(country_tz_table) / sizeof(country_tz_table[0]))

/**
 * @brief Look up country in the timezone/DST table
 * @return Pointer to entry, or NULL if country not found
 */
static const country_tz_t* find_country_tz(const char* country)
{
    for (int i = 0; i < (int)COUNTRY_TZ_TABLE_SIZE; i++) {
        if (strcmp(country, country_tz_table[i].cc) == 0) {
            return &country_tz_table[i];
        }
    }
    return NULL;
}

/**
 * @brief Check if DST is currently active based on rule, date, and UTC hour
 * @return 1 if DST is active (add +1 hour), 0 otherwise
 */
static int is_dst_active(int dst_rule, int day, int month, int year, int utc_hour)
{
    if (dst_rule == DST_EU) {
        // EU: Last Sunday of March at 1:00 UTC → Last Sunday of October at 1:00 UTC
        if (month < 3 || month > 10) return 0;
        if (month > 3 && month < 10) return 1;
        if (month == 3) {
            int last_sun = find_last_sunday(3, year);
            if (day > last_sun) return 1;
            if (day == last_sun && utc_hour >= 1) return 1;
            return 0;
        }
        if (month == 10) {
            int last_sun = find_last_sunday(10, year);
            if (day < last_sun) return 1;
            if (day == last_sun && utc_hour < 1) return 1;
            return 0;
        }
    }

    if (dst_rule == DST_US) {
        // US: 2nd Sunday of March at 2:00 local → 1st Sunday of November at 2:00 local
        // Approximate using UTC (covers EST to PST transition times)
        if (month < 3 || month > 11) return 0;
        if (month > 3 && month < 11) return 1;
        if (month == 3) {
            int second_sun = find_nth_sunday(3, year, 2);
            if (day > second_sun) return 1;
            if (day == second_sun && utc_hour >= 7) return 1;
            return 0;
        }
        if (month == 11) {
            int first_sun = find_nth_sunday(11, year, 1);
            if (day < first_sun) return 1;
            if (day == first_sun && utc_hour < 6) return 1;
            return 0;
        }
    }

    return 0;
}

/**
 * @brief Get effective timezone for a city, applying DST and corrections
 * @param city_tz The timezone_offset stored in the city database
 * @param country The country code of the nearest city
 * @param day, month, year, utc_hour GPS date/time in UTC
 * @param out_dst_active Set to 1 if DST is active, 0 otherwise (can be NULL)
 * @return Effective timezone offset (standard + DST if applicable)
 */
static int get_effective_timezone(int city_tz, const char* country,
                                  int day, int month, int year, int utc_hour,
                                  int* out_dst_active)
{
    if (out_dst_active) *out_dst_active = 0;

    const country_tz_t* entry = find_country_tz(country);
    if (!entry) {
        // Country not in table — no DST, use city value as-is
        return city_tz;
    }

    // Get standard timezone: from table override or city database
    int standard_tz = (entry->std_tz == TZ_USE_CITY) ? city_tz : entry->std_tz;

    // Check DST
    int dst = is_dst_active(entry->dst_rule, day, month, year, utc_hour);
    if (out_dst_active) *out_dst_active = dst;

    return standard_tz + dst;
}

/**
 * @brief Get local time using prayer-configured timezone
 * @param local_time Output buffer for local time (must be at least 11 bytes)
 * @param max_len Size of output buffer
 * @return Timezone offset applied (from prayer_get_timezone, 0 if invalid)
 */
int gps_get_local_time(char *local_time, size_t max_len)
{
    if (!current_gps.time_str[0] || !local_time || max_len < 11) {
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

    // Use the same timezone as prayer calculations (set by gps_auto_configure_timezone)
    // This replaces the old hardcoded CET/CEST logic so display time matches prayer times
    int offset = prayer_get_timezone();

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

        printk("NEO-7M: Nearest city: %s (%s), city DB timezone: UTC%+d\n",
               nearest_city->city_name, nearest_city->country, city_tz);
        printk("NEO-7M: Calculated timezone from longitude: UTC%+d\n", calculated_tz);

        // Apply country-based timezone correction and DST via lookup table
        // This dynamically overrides wrong city DB values and adds DST
        int dst_active = 0;
        if (current_gps.date_valid) {
            int day, month, year, utc_hour = 0;
            if (sscanf(current_gps.date_str, "%d/%d/%d", &day, &month, &year) == 3) {
                sscanf(current_gps.time_str, "%d", &utc_hour);
                final_tz = get_effective_timezone(city_tz, nearest_city->country,
                                                  day, month, year, utc_hour,
                                                  &dst_active);
            } else {
                final_tz = city_tz;  // Can't parse date, use city value
            }
        } else {
            // No date yet — use city value without DST
            final_tz = get_effective_timezone(city_tz, nearest_city->country,
                                              1, 1, 2000, 0, NULL);
        }

        printk("NEO-7M: Effective timezone: UTC%+d (DST: %s)\n",
               final_tz, dst_active ? "ACTIVE +1h" : "inactive");
    } else {
        printk("NEO-7M: No nearest city found - using longitude-based UTC%+d\n", calculated_tz);
    }

    printk("NEO-7M: Final timezone: UTC%+d\n", final_tz);

    // Update prayer time timezone
    prayer_set_timezone(final_tz);
}
