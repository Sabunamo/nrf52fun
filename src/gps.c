#include "gps.h"
#include "font.h"
#include "prayerTime.h"

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
                // Format as HH:MM:SS
                snprintf(current_gps.time_str, sizeof(current_gps.time_str),
                        "%.2s:%.2s:%.2s", time_buffer, time_buffer+2, time_buffer+4);
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
        snprintf(current_gps.date_str, sizeof(current_gps.date_str), 
                "%s/%s/%s", tokens[2], tokens[3], tokens[4]);
        current_gps.date_valid = true;
        
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