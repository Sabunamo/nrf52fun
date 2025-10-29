#include "gps.h"
#include "font.h"
#include "prayerTime.h"
#include "world_cities.h"

static const struct device *gps_uart;
static char gps_buffer[GPS_BUFFER_SIZE];
static int gps_buffer_pos = 0;
struct gps_data current_gps = {0};

static void process_nmea_sentence(char *sentence);
static void process_gprmc(char *sentence);
static void process_gpzda(char *sentence);
static void process_gpgga(char *sentence);
static double nmea_to_decimal(const char *nmea_coord, char hemisphere);
static const char* get_short_day_name(const char* full_day_name);

void draw_character(const struct device *display_dev, char c, int x, int y, uint16_t color);
void draw_text(const struct device *display_dev, const char* text, int x, int y, uint16_t color);

/**
 * @brief UART interrupt callback for GPS data reception
 *
 * Processes incoming GPS NMEA data character by character, handles
 * buffer management and validates NMEA sentence format before processing.
 *
 * @param dev UART device pointer
 * @param ctx Context pointer (unused)
 */
static void uart_callback(const struct device *dev, void *ctx)
{
    uint8_t byte;

    while (uart_poll_in(dev, &byte) == 0) {
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
                    process_nmea_sentence(gps_buffer);
                }

                gps_buffer_pos = 0;
            }
        }
        // Ignore other invalid characters without resetting buffer
    }
}

/**
 * @brief Convert NMEA coordinate format to decimal degrees
 *
 * Converts GPS coordinates from NMEA format (DDMM.MMMM) to decimal degrees.
 * Example: 4807.038,N becomes 48.117300 degrees
 *
 * @param nmea_coord NMEA coordinate string (e.g., "4807.038")
 * @param hemisphere Direction character ('N', 'S', 'E', 'W')
 * @return Decimal degrees (negative for South/West)
 */
static double nmea_to_decimal(const char *nmea_coord, char hemisphere)
{
    if (!nmea_coord || strlen(nmea_coord) < 4) {
        return 0.0;
    }

    double coord = atof(nmea_coord);
    int degrees = (int)(coord / 100);           // Extract degrees part
    double minutes = coord - (degrees * 100);   // Extract minutes part
    double decimal = degrees + (minutes / 60.0); // Convert to decimal degrees

    // Apply direction (South and West are negative)
    if (hemisphere == 'S' || hemisphere == 'W') {
        decimal = -decimal;
    }

    return decimal;
}

/**
 * @brief Convert full day name to 3-character abbreviation
 *
 * @param full_day_name Full day name (e.g., "Monday")
 * @return 3-character abbreviation (e.g., "Mon") or "???" if unknown
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
    return "???";  // Fallback for unknown day
}

/**
 * @brief Process GPRMC (Recommended Minimum Course) NMEA sentence
 *
 * Extracts GPS position, time, date, and validity status from GPRMC sentences.
 * Format: $GPRMC,time,status,lat,lat_dir,lon,lon_dir,speed,course,date,mag_var,checksum
 *
 * @param sentence NMEA sentence string to parse
 */
static void process_gprmc(char *sentence)
{
    char *token;
    char *tokens[15];
    int token_count = 0;

    // Split sentence into comma-separated tokens
    token = strtok(sentence, ",");
    while (token != NULL && token_count < 15) {
        tokens[token_count++] = token;
        token = strtok(NULL, ",");
    }

    // Extract date from GPRMC sentence (token[9] = DDMMYY format)
    if (token_count > 9 && tokens[9] && strlen(tokens[9]) >= 6) {
        // Validate date format (6 digits)
        bool is_date = true;
        for (int i = 0; i < 6 && i < strlen(tokens[9]); i++) {
            if (tokens[9][i] < '0' || tokens[9][i] > '9') {
                is_date = false;
                break;
            }
        }

        if (is_date) {
            // Convert DDMMYY to DD/MM/20YY format
            snprintf(current_gps.date_str, sizeof(current_gps.date_str),
                    "%.2s/%.2s/20%.2s", tokens[9], tokens[9]+2, tokens[9]+4);
            current_gps.date_valid = true;

            // Calculate Hijri date and day of week when date is available
            int day, month, year;
            if (sscanf(current_gps.date_str, "%d/%d/%d", &day, &month, &year) == 3) {
                double julian_day = convert_Gregor_2_Julian_Day((float)day, month, year);

                // Convert Gregorian date to Hijri date using GPS date and Julian Day
                hijri_date_t hijri_date = convert_Gregor_2_Hijri_Date((float)day, month, year, julian_day);

                // Store Hijri date in GPS data structure for display
                snprintf(current_gps.hijri_date_str, sizeof(current_gps.hijri_date_str),
                        "%d/%d/%d", hijri_date.day, hijri_date.month, hijri_date.year);
                current_gps.hijri_valid = true;

                // Calculate and store day of the week
                const char* day_name = day_Of_Weak(julian_day);
                const char* short_day_name = get_short_day_name(day_name);
                strncpy(current_gps.day_of_week, short_day_name, sizeof(current_gps.day_of_week) - 1);
                current_gps.day_of_week[sizeof(current_gps.day_of_week) - 1] = '\0';
                current_gps.day_valid = true;

            }
        }
    }

    // Process position data only from valid GPS fixes (status = 'A')
    if (token_count >= 10 && tokens[2][0] == 'A') {
        // Extract coordinates
        current_gps.latitude = nmea_to_decimal(tokens[3], tokens[4][0]);
        current_gps.longitude = nmea_to_decimal(tokens[5], tokens[6][0]);
        current_gps.lat_hemisphere = tokens[4][0];
        current_gps.lon_hemisphere = tokens[6][0];

        // Extract and format GPS time (HHMMSS.SS format)
        if (strlen(tokens[1]) >= 6) {
            char time_buffer[7] = {0};
            strncpy(time_buffer, tokens[1], 6);
            time_buffer[6] = '\0';

            // Validate time format (6 digits)
            bool valid_time = true;
            for (int i = 0; i < 6; i++) {
                if (time_buffer[i] < '0' || time_buffer[i] > '9') {
                    valid_time = false;
                    break;
                }
            }

            if (valid_time) {
                // Extract UTC hours, minutes, seconds
                char hours_str[3] = {time_buffer[0], time_buffer[1], '\0'};
                char minutes_str[3] = {time_buffer[2], time_buffer[3], '\0'};
                char seconds_str[3] = {time_buffer[4], time_buffer[5], '\0'};

                current_gps.utc_hours = atoi(hours_str);
                current_gps.utc_minutes = atoi(minutes_str);
                current_gps.utc_seconds = atoi(seconds_str);

                // Format as HH:MM:SS
                snprintf(current_gps.time_str, sizeof(current_gps.time_str),
                        "%.2s:%.2s:%.2s", time_buffer, time_buffer+2, time_buffer+4);

                printk("[GPS] RAW GPS UTC Time: %02d:%02d:%02d\n",
                       current_gps.utc_hours, current_gps.utc_minutes, current_gps.utc_seconds);
            }
        }

        current_gps.valid = true;
    }
}

static void process_gpzda(char *sentence)
{
    char *token;
    char *tokens[8];
    int token_count = 0;
    
    token = strtok(sentence, ",");
    while (token != NULL && token_count < 8) {
        tokens[token_count++] = token;
        token = strtok(NULL, ",");
    }
    
    // GPZDA format: $GPZDA,time,day,month,year,local_hour,local_min,checksum
    if (token_count >= 5 && tokens[1] && tokens[2] && tokens[3] && tokens[4]) {
        // Extract date from day, month, year fields
        current_gps.utc_day = atoi(tokens[2]);
        current_gps.utc_month = atoi(tokens[3]);
        current_gps.utc_year = atoi(tokens[4]);

        snprintf(current_gps.date_str, sizeof(current_gps.date_str),
                "%s/%s/%s", tokens[2], tokens[3], tokens[4]);
        current_gps.date_valid = true;

        printk("[GPS] RAW GPS Date parsed: Day=%d, Month=%d, Year=%d\n",
               current_gps.utc_day, current_gps.utc_month, current_gps.utc_year);
        printk("[GPS] Date string: %s\n", current_gps.date_str);
    } else {
        printk("[GPS] GPZDA parsing failed: token_count=%d\n", token_count);
    }
}

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
    
    // GPGGA format: $GPGGA,time,lat,N/S,lon,E/W,quality,numSV,HDOP,alt,M,geoid,M,dgps_time,dgps_id,checksum
    // We need tokens[9] for altitude above sea level
    if (token_count >= 10 && tokens[6] && tokens[9]) {
        // Check if we have a valid fix (quality > 0)
        int quality = atoi(tokens[6]);
        if (quality > 0 && tokens[9] && strlen(tokens[9]) > 0) {
            current_gps.seeHeight = atof(tokens[9]);  // Altitude above sea level
            current_gps.seeHeight_valid = true;
            
        }
    }
}

static void process_nmea_sentence(char *sentence)
{
    if (strlen(sentence) < 6) {
        return;
    }
    
    // Process GPRMC sentences for lat, long, time & date
    if (strncmp(sentence, "$GPRMC", 6) == 0) {
        process_gprmc(sentence);
    }
    // Also process GPZDA sentences for date & time
    else if (strncmp(sentence, "$GPZDA", 6) == 0) {
        process_gpzda(sentence);
    }
    // Process GPGGA sentences for altitude
    else if (strncmp(sentence, "$GPGGA", 6) == 0) {
        process_gpgga(sentence);
    }
}

/**
 * @brief Process GPS data (compatibility function)
 *
 * This function exists for compatibility with the main application loop.
 * Since GPS data is processed automatically via UART interrupts, this
 * function is intentionally left empty in interrupt-driven mode.
 */
void gps_process_data(void)
{
    // GPS data is processed automatically via UART interrupts
    // This function is kept for compatibility with main application loop
}

/**
 * @brief Initialize GPS module and UART communication
 *
 * Sets up UART1 for GPS communication at 9600 baud with interrupt-driven
 * data reception. Configures the interrupt callback for processing NMEA data.
 *
 * @return 0 on success, -1 on failure
 */
int gps_init(void)
{
    // Get GPS UART device from device tree
    gps_uart = DEVICE_DT_GET(GPS_UART_NODE);

    if (!device_is_ready(gps_uart)) {
        printk("GPS UART device not ready\n");
        return -1;
    }

    // Configure interrupt-driven UART reception
    uart_irq_callback_set(gps_uart, uart_callback);
    uart_irq_rx_enable(gps_uart);

    printk("GPS UART initialized\n");
    return 0;
}

void gps_print_info(void)
{
    if (current_gps.valid) {
        int lat_int = (int)(fabs(current_gps.latitude) * 1000000);
        int lon_int = (int)(fabs(current_gps.longitude) * 1000000);
        
        printk("Latitude: %d.%06d%c\n", 
               lat_int / 1000000, lat_int % 1000000, current_gps.lat_hemisphere);
        printk("Longitude: %d.%06d%c\n", 
               lon_int / 1000000, lon_int % 1000000, current_gps.lon_hemisphere);
        printk("Time: %s UTC\n", current_gps.time_str);
        
        if (current_gps.seeHeight_valid) {
            // Convert to integer representation for printk (which may not support %f)
            int alt_int = (int)current_gps.seeHeight;
            int alt_frac = (int)((current_gps.seeHeight - alt_int) * 10);
            printk("Altitude: %d.%d meters above sea level\n", alt_int, alt_frac);
        }
        
        if (current_gps.date_valid) {
            printk("Date: %s\n", current_gps.date_str);
            
            // Parse GPS date and convert to Julian Day
            int day, month, year;
            if (sscanf(current_gps.date_str, "%d/%d/%d", &day, &month, &year) == 3) {
                printk("Parsed date: %d/%d/%d\n", day, month, year);
                double julian_day = convert_Gregor_2_Julian_Day((float)day, month, year);
                
                // Convert double to integer parts for printk (which may not support %f)
                int jd_int = (int)julian_day;
                int jd_frac = (int)((julian_day - jd_int) * 1000000);
                printk("Julian Day: %d.%06d\n", jd_int, jd_frac);
                
                // Convert Gregorian date to Hijri date using GPS date and Julian Day
                hijri_date_t hijri_date = convert_Gregor_2_Hijri_Date((float)day, month, year, julian_day);
                
                // Store Hijri date in GPS data structure for display
                snprintf(current_gps.hijri_date_str, sizeof(current_gps.hijri_date_str), 
                        "%d/%d/%d", hijri_date.day, hijri_date.month, hijri_date.year);
                current_gps.hijri_valid = true;
                
                // Calculate and store day of the week
                const char* day_name = day_Of_Weak(julian_day);
                const char* short_day_name = get_short_day_name(day_name);
                strncpy(current_gps.day_of_week, short_day_name, sizeof(current_gps.day_of_week) - 1);
                current_gps.day_of_week[sizeof(current_gps.day_of_week) - 1] = '\0';
                current_gps.day_valid = true;
                
                printk("Hijri Date: %d/%d/%d\n", hijri_date.day, hijri_date.month, hijri_date.year);
                printk("Day of Week: %s\n", current_gps.day_of_week);
            }
        } else {
            printk("Date: No Date\n");
        }
    } else {
        printk("GPS: No fix\n");
    }
}

void display_gps_data(const struct device *display_dev, int x, int y)
{
    static int search_dots = 0;
    
    if (current_gps.valid) {
        char lat_str[32];
        char lon_str[32];
        char time_str[20];
        char date_str[32];
        char hijri_str[32];
        char day_str[20];
        
        // Convert to integer representation to avoid float formatting issues
        int lat_int = (int)(fabs(current_gps.latitude) * 1000000);
        int lon_int = (int)(fabs(current_gps.longitude) * 1000000);
        
        snprintf(lat_str, sizeof(lat_str), "%d.%06d%c", 
                lat_int / 1000000, lat_int % 1000000, current_gps.lat_hemisphere);
        snprintf(lon_str, sizeof(lon_str), "%d.%06d%c", 
                lon_int / 1000000, lon_int % 1000000, current_gps.lon_hemisphere);
        snprintf(time_str, sizeof(time_str), "Time: %s", current_gps.time_str);
        
        if (current_gps.date_valid) {
            snprintf(date_str, sizeof(date_str), "Date: %s", current_gps.date_str);
        } else {
            strcpy(date_str, "Date: No Date");
        }
        
        if (current_gps.hijri_valid) {
            snprintf(hijri_str, sizeof(hijri_str), "Hijri Date: %s", current_gps.hijri_date_str);
        } else {
            strcpy(hijri_str, "Hijri Date: --/--/----");
        }
        
        if (current_gps.day_valid) {
            snprintf(day_str, sizeof(day_str), "%s", current_gps.day_of_week);
        } else {
            strcpy(day_str, "---");
        }
        
        draw_text(display_dev, lat_str, x, y, 0x0000);
        draw_text(display_dev, lon_str, x, y + 20, 0x0000);
        draw_text(display_dev, time_str, x, y + 40, 0x0000);
        draw_text(display_dev, date_str, x, y + 60, 0x0000);
        draw_text(display_dev, hijri_str, x, y + 80, 0x0000);
        draw_text(display_dev, day_str, x, y + 100, 0x0000);
    } else {
        // Animated searching message
        char search_msg[32];
        search_dots = (search_dots + 1) % 4;
        switch(search_dots) {
            case 0: strcpy(search_msg, "GPS: Searching   "); break;
            case 1: strcpy(search_msg, "GPS: Searching.  "); break;
            case 2: strcpy(search_msg, "GPS: Searching.. "); break;
            case 3: strcpy(search_msg, "GPS: Searching..."); break;
        }
        draw_text(display_dev, search_msg, x, y, 0x0000);
        draw_text(display_dev, "Move to window/outdoor", x, y + 20, 0x0000);
        draw_text(display_dev, "Wait for satellites", x, y + 40, 0x0000);
    }
}

const char* gps_get_today_date(void)
{
    if (current_gps.date_valid) {
        return current_gps.date_str;
    } else {
        return "No Date";
    }
}

// DST configuration - Germany/Berlin
// Winter (CET - DST inactive): UTC+1
// Summer (CEST - DST active): UTC+1 + 1 = UTC+2
static struct dst_config dst_cfg = {
    .timezone_offset = 1,       // UTC+1 (CET - Central European Time)
    .dst_enabled = true,        // DST enabled
    .dst_offset = 1,            // +1 hour when DST is active (CEST = UTC+2)
    .dst_start_month = 3,       // March (last Sunday at 02:00)
    .dst_start_day = 0,         // 0 = last Sunday (calculated dynamically)
    .dst_end_month = 10,        // October (last Sunday at 03:00)
    .dst_end_day = 0,           // 0 = last Sunday (calculated dynamically)
};

void gps_set_dst_config(const struct dst_config *config)
{
    if (config) {
        dst_cfg = *config;
        printk("DST Config: UTC%+d, DST %s (offset: %+d hours)\n",
               dst_cfg.timezone_offset,
               dst_cfg.dst_enabled ? "enabled" : "disabled",
               dst_cfg.dst_offset);
        if (dst_cfg.dst_enabled) {
            printk("DST Period: Month %d Day %d to Month %d Day %d\n",
                   dst_cfg.dst_start_month, dst_cfg.dst_start_day,
                   dst_cfg.dst_end_month, dst_cfg.dst_end_day);
        }
    }
}

/**
 * @brief Calculate day of week using Zeller's congruence
 * @param day Day of month (1-31)
 * @param month Month (1-12)
 * @param year Year (full year, e.g., 2025)
 * @return Day of week (0=Sunday, 1=Monday, ..., 6=Saturday)
 */
static int calculate_day_of_week(int day, int month, int year)
{
    // Zeller's congruence for Gregorian calendar
    if (month < 3) {
        month += 12;
        year--;
    }

    int q = day;
    int m = month;
    int k = year % 100;
    int j = year / 100;

    int h = (q + ((13 * (m + 1)) / 5) + k + (k / 4) + (j / 4) - (2 * j)) % 7;

    // Convert to 0=Sunday format
    return (h + 6) % 7;
}

/**
 * @brief Get number of days in a month
 * @param month Month (1-12)
 * @param year Year (for leap year calculation)
 * @return Number of days in the month
 */
static int days_in_month(int month, int year)
{
    if (month == 2) {
        // February - check for leap year
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
            return 29;
        }
        return 28;
    }

    // April, June, September, November have 30 days
    if (month == 4 || month == 6 || month == 9 || month == 11) {
        return 30;
    }

    // All other months have 31 days
    return 31;
}

/**
 * @brief Calculate the date of the last Sunday of a given month
 * @param month Month (1-12)
 * @param year Year (full year)
 * @return Day of month for last Sunday (1-31)
 */
static int get_last_sunday(int month, int year)
{
    int last_day = days_in_month(month, year);

    // Start from the last day and work backwards to find Sunday
    for (int day = last_day; day >= last_day - 6; day--) {
        int dow = calculate_day_of_week(day, month, year);
        if (dow == 0) {  // Sunday
            return day;
        }
    }

    // Should never reach here, but return last day as fallback
    return last_day;
}

bool gps_is_dst_active(void)
{
    if (!dst_cfg.dst_enabled || !current_gps.date_valid) {
        printk("[DST] DST disabled or no valid date\n");
        return false;
    }

    int month = current_gps.utc_month;
    int day = current_gps.utc_day;
    int year = current_gps.utc_year;

    // If utc_* fields are not populated (0), try to parse from date_str
    if (year == 0 && current_gps.date_valid && strlen(current_gps.date_str) >= 10) {
        // date_str format is "DD/MM/YYYY"
        char day_str[3], month_str[3], year_str[5];
        sscanf(current_gps.date_str, "%2s/%2s/%4s", day_str, month_str, year_str);
        day = atoi(day_str);
        month = atoi(month_str);
        year = atoi(year_str);
        printk("[DST] Parsed date from date_str: %s -> %04d-%02d-%02d\n",
               current_gps.date_str, year, month, day);
    }

    printk("[DST] Current date: %04d-%02d-%02d\n", year, month, day);

    // Calculate actual DST transition dates if using "last Sunday" rule (day = 0)
    int dst_start_day = dst_cfg.dst_start_day;
    int dst_end_day = dst_cfg.dst_end_day;

    if (dst_start_day == 0) {
        // Calculate last Sunday of start month
        dst_start_day = get_last_sunday(dst_cfg.dst_start_month, year);
        printk("[DST] Calculated DST start: Last Sunday of month %d = day %d\n",
               dst_cfg.dst_start_month, dst_start_day);
    }

    if (dst_end_day == 0) {
        // Calculate last Sunday of end month
        dst_end_day = get_last_sunday(dst_cfg.dst_end_month, year);
        printk("[DST] Calculated DST end: Last Sunday of month %d = day %d\n",
               dst_cfg.dst_end_month, dst_end_day);
    }

    printk("[DST] DST period: %04d-%02d-%02d to %04d-%02d-%02d\n",
           year, dst_cfg.dst_start_month, dst_start_day,
           year, dst_cfg.dst_end_month, dst_end_day);

    bool is_dst = false;

    // Check if current date is within DST period
    if (month > dst_cfg.dst_start_month && month < dst_cfg.dst_end_month) {
        // Definitely in DST period (between start and end months)
        is_dst = true;
        printk("[DST] In DST period (between months %d and %d)\n",
               dst_cfg.dst_start_month, dst_cfg.dst_end_month);
    } else if (month == dst_cfg.dst_start_month && day >= dst_start_day) {
        // In start month, on or after start day
        is_dst = true;
        printk("[DST] In DST period (start month, day %d >= %d)\n", day, dst_start_day);
    } else if (month == dst_cfg.dst_end_month && day < dst_end_day) {
        // In end month, before end day
        is_dst = true;
        printk("[DST] In DST period (end month, day %d < %d)\n", day, dst_end_day);
    } else {
        printk("[DST] NOT in DST period\n");
    }

    return is_dst;
}

int gps_get_current_offset(void)
{
    int offset = dst_cfg.timezone_offset;
    bool dst_active = gps_is_dst_active();

    printk("[TIME] ===== TIMEZONE OFFSET CALCULATION =====\n");
    printk("[TIME] Base timezone offset: UTC%+d\n", offset);

    if (dst_active) {
        printk("[TIME] >> DST IS ACTIVE <<\n");
        printk("[TIME] Adding DST offset: %+d hour(s)\n", dst_cfg.dst_offset);
        offset += dst_cfg.dst_offset;
        printk("[TIME] New offset after DST: UTC%+d\n", offset);
    } else {
        printk("[TIME] >> DST IS NOT ACTIVE <<\n");
        printk("[TIME] No DST offset added\n");
    }

    printk("[TIME] Final total offset: UTC%+d\n", offset);
    printk("[TIME] ==========================================\n");
    return offset;
}

int gps_calculate_timezone_from_longitude(double longitude)
{
    // Calculate timezone from longitude
    // Earth rotates 360° in 24 hours, so each timezone is ~15° wide
    // Timezone offset = longitude / 15, rounded to nearest hour

    // Use round() to properly round to nearest integer instead of truncating
    double tz_calc = longitude / 15.0;
    int timezone = (int)round(tz_calc);

    // Clamp to valid timezone range [-12, +14]
    if (timezone < -12) timezone = -12;
    if (timezone > 14) timezone = 14;

    printk("[TIMEZONE] Longitude: %.6f, Calculated: %.6f / 15 = %.6f, Rounded: UTC%+d\n",
           longitude, longitude, tz_calc, timezone);

    return timezone;
}

void gps_auto_configure_timezone(void)
{
    if (!current_gps.valid) {
        printk("[TIMEZONE] Cannot auto-configure: GPS not valid\n");
        return;
    }

    // Calculate timezone from longitude (mathematical approach)
    int calculated_tz = gps_calculate_timezone_from_longitude(current_gps.longitude);

    // Find nearest city to get the political timezone
    const city_data_t* nearest_city = find_nearest_city(current_gps.latitude, current_gps.longitude);

    int final_tz = calculated_tz;  // Default to calculated

    if (nearest_city) {
        int city_tz = nearest_city->timezone_offset;

        printk("[TIMEZONE] Nearest city: %s (%s) has timezone UTC%+d\n",
               nearest_city->city_name, nearest_city->country, city_tz);
        printk("[TIMEZONE] Calculated timezone from longitude: UTC%+d\n", calculated_tz);

        // Compare calculated vs city timezone
        if (calculated_tz == city_tz) {
            printk("[TIMEZONE] ✓ Calculated and city timezones MATCH - using UTC%+d\n", calculated_tz);
            final_tz = calculated_tz;
        } else {
            printk("[TIMEZONE] ✗ Calculated (UTC%+d) and city (UTC%+d) timezones DIFFER\n",
                   calculated_tz, city_tz);
            printk("[TIMEZONE] → Using city timezone UTC%+d (political boundary)\n", city_tz);
            final_tz = city_tz;
        }
    } else {
        printk("[TIMEZONE] No nearest city found - using calculated timezone UTC%+d\n", calculated_tz);
    }

    // Update DST configuration with final timezone
    dst_cfg.timezone_offset = final_tz;

    // Also update prayer time timezone
    prayer_set_timezone(final_tz);

    printk("[TIMEZONE] ===== FINAL TIMEZONE: UTC%+d =====\n", final_tz);
    printk("[TIMEZONE] Location: %.4f°%c, %.4f°%c\n",
           fabs(current_gps.latitude), current_gps.lat_hemisphere,
           fabs(current_gps.longitude), current_gps.lon_hemisphere);
}

int gps_get_local_time(char *local_time_str, size_t max_len)
{
    if (!current_gps.valid || !local_time_str) {
        if (local_time_str && max_len > 0) {
            snprintf(local_time_str, max_len, "--:--:--");
        }
        printk("[TIME] GPS not valid or no buffer\n");
        return 0;
    }

    printk("[TIME] ========== LOCAL TIME CALCULATION ==========\n");
    printk("[TIME] GPS UTC time: %02d:%02d:%02d\n",
           current_gps.utc_hours, current_gps.utc_minutes, current_gps.utc_seconds);

    int total_offset = gps_get_current_offset();

    // Apply offset to UTC time
    int local_hours = current_gps.utc_hours + total_offset;
    int local_minutes = current_gps.utc_minutes;
    int local_seconds = current_gps.utc_seconds;

    printk("[TIME] Before rollover: %02d:%02d:%02d (offset: %+d)\n",
           local_hours, local_minutes, local_seconds, total_offset);

    // Handle day rollover
    if (local_hours >= 24) {
        local_hours -= 24;
        printk("[TIME] Day rollover: wrapped to %02d hours\n", local_hours);
    } else if (local_hours < 0) {
        local_hours += 24;
        printk("[TIME] Day rollback: wrapped to %02d hours\n", local_hours);
    }

    snprintf(local_time_str, max_len, "%02d:%02d:%02d",
             local_hours, local_minutes, local_seconds);

    printk("[TIME] Final local time: %s (UTC%+d)\n", local_time_str, total_offset);
    printk("[TIME] ============================================\n");

    return total_offset;
}