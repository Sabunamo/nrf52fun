#include "prayer_hmi.h"
#include "gps.h"
#include "font.h"
#include <zephyr/drivers/gpio.h>

static hmi_display_data_t hmi_data = {0};

// Backlight control variables
#define BACKLIGHT_PIN  3   // P0.03 (Backlight control pin)
static const struct device *backlight_dev = NULL;

static void hmi_draw_character(const struct device *display_dev, char c, int x, int y, uint16_t color);
static void hmi_draw_text(const struct device *display_dev, const char* text, int x, int y, uint16_t color);

static void hmi_draw_character(const struct device *display_dev, char c, int x, int y, uint16_t color) {
    const uint8_t* char_pattern = font_space; // default to space

    // Select character pattern from complete font system
    switch(c) {
        // Lowercase letters
        case 'a': char_pattern = font_a; break;
        case 'b': char_pattern = font_b; break;
        case 'c': char_pattern = font_c; break;
        case 'd': char_pattern = font_d; break;
        case 'e': char_pattern = font_e; break;
        case 'f': char_pattern = font_f; break;
        case 'g': char_pattern = font_g; break;
        case 'h': char_pattern = font_h; break;
        case 'i': char_pattern = font_i; break;
        case 'j': char_pattern = font_j; break;
        case 'k': char_pattern = font_k; break;
        case 'l': char_pattern = font_l; break;
        case 'm': char_pattern = font_m; break;
        case 'n': char_pattern = font_n; break;
        case 'o': char_pattern = font_o; break;
        case 'p': char_pattern = font_p; break;
        case 'q': char_pattern = font_q; break;
        case 'r': char_pattern = font_r; break;
        case 's': char_pattern = font_s; break;
        case 't': char_pattern = font_t; break;
        case 'u': char_pattern = font_u; break;
        case 'v': char_pattern = font_v; break;
        case 'w': char_pattern = font_w; break;
        case 'x': char_pattern = font_x; break;
        case 'y': char_pattern = font_y; break;
        case 'z': char_pattern = font_z; break;

        // Uppercase letters
        case 'A': char_pattern = font_A; break;
        case 'B': char_pattern = font_B; break;
        case 'C': char_pattern = font_C; break;
        case 'D': char_pattern = font_D; break;
        case 'E': char_pattern = font_E; break;
        case 'F': char_pattern = font_F; break;
        case 'G': char_pattern = font_G; break;
        case 'H': char_pattern = font_H; break;
        case 'I': char_pattern = font_I; break;
        case 'J': char_pattern = font_J; break;
        case 'K': char_pattern = font_K; break;
        case 'L': char_pattern = font_L; break;
        case 'M': char_pattern = font_M; break;
        case 'N': char_pattern = font_N; break;
        case 'O': char_pattern = font_O; break;
        case 'P': char_pattern = font_P; break;
        case 'Q': char_pattern = font_Q; break;
        case 'R': char_pattern = font_R; break;
        case 'S': char_pattern = font_S; break;
        case 'T': char_pattern = font_T; break;
        case 'U': char_pattern = font_U; break;
        case 'V': char_pattern = font_V; break;
        case 'W': char_pattern = font_W; break;
        case 'X': char_pattern = font_X; break;
        case 'Y': char_pattern = font_Y; break;
        case 'Z': char_pattern = font_Z; break;

        // Digits
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

        // Special characters
        case ' ': char_pattern = font_space; break;
        case ':': char_pattern = font_colon; break;
        case '.': char_pattern = font_period; break;
        case '/': char_pattern = font_slash; break;
        case '-': char_pattern = font_dash; break;
        case '%': char_pattern = font_percent; break;
        case '\xB0': char_pattern = font_degree; break; // Degree symbol (°)

        default: char_pattern = font_space; break;
    }

    // Draw 16x8 character (16 rows, 8 columns)
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

static void hmi_draw_text(const struct device *display_dev, const char* text, int x, int y, uint16_t color) {
    int len = strlen(text);
    for (int i = 0; i < len; i++) {
        char c = text[i];
        // Special handling for temperature: 'o' before 'C' becomes degree symbol
        if (c == 'o' && i + 1 < len && text[i + 1] == 'C') {
            // Use degree symbol pattern instead of 'o'
            const uint8_t* char_pattern = font_degree;
            for (int row = 0; row < 16; row++) {
                uint8_t pattern = char_pattern[row];
                for (int col = 0; col < 8; col++) {
                    if (pattern & (0x80 >> col)) {
                        uint16_t pixel = color;
                        struct display_buffer_descriptor pixel_desc = {
                            .width = 1,
                            .height = 1,
                            .pitch = 1,
                            .buf_size = sizeof(pixel),
                        };
                        display_write(display_dev, x + (i * 9) + col, y + row, &pixel_desc, &pixel);
                    }
                }
            }
        } else {
            hmi_draw_character(display_dev, c, x + (i * 9), y, color); // 9 pixels spacing
        }
    }
}

void hmi_init(void)
{
    memset(&hmi_data, 0, sizeof(hmi_display_data_t));

    strcpy(hmi_data.city, "Unknown");
    strcpy(hmi_data.gregorian_date, "--/--/----");
    strcpy(hmi_data.hijri_date, "--/--/----");
    strcpy(hmi_data.day_of_week, "---");
    strcpy(hmi_data.countdown_text, "Next prayer in --:--");
    strcpy(hmi_data.weather_temp, "--°C");
    strcpy(hmi_data.current_time, "--:--");
    hmi_data.brightness_level = 50;
    hmi_data.next_prayer_index = -1;

    // Initialize update flags
    hmi_data.needs_full_update = true;
    hmi_data.needs_time_update = false;
    hmi_data.screen_initialized = false;

    for (int i = 0; i < PRAYER_COUNT; i++) {
        strcpy(hmi_data.prayers[i].name, "-----");
        strcpy(hmi_data.prayers[i].time, "--:--");
        hmi_data.prayers[i].is_next = false;
    }

    strcpy(hmi_data.prayers[PRAYER_FAJR].name, "Fajr");
    strcpy(hmi_data.prayers[PRAYER_DHUHR].name, "Dhuhr");
    strcpy(hmi_data.prayers[PRAYER_ASR].name, "Asr");
    strcpy(hmi_data.prayers[PRAYER_MAGHRIB].name, "Maghrib");
    strcpy(hmi_data.prayers[PRAYER_ISHA].name, "Isha");
}

void hmi_clear_screen(const struct device *display_dev)
{
    static uint16_t black_line[DISPLAY_WIDTH];
    for (int i = 0; i < DISPLAY_WIDTH; i++) {
        black_line[i] = COLOR_BLACK;
    }

    struct display_buffer_descriptor line_desc = {
        .width = DISPLAY_WIDTH,
        .height = 1,
        .pitch = DISPLAY_WIDTH,
        .buf_size = sizeof(black_line),
    };

    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        display_write(display_dev, 0, y, &line_desc, black_line);
    }
}

void hmi_draw_rectangle(const struct device *display_dev, int x, int y, int width, int height, uint16_t color)
{
    static uint16_t line_buffer[DISPLAY_WIDTH];

    for (int i = 0; i < width && i < DISPLAY_WIDTH; i++) {
        line_buffer[i] = color;
    }

    struct display_buffer_descriptor line_desc = {
        .width = width,
        .height = 1,
        .pitch = width,
        .buf_size = width * sizeof(uint16_t),
    };

    for (int row = 0; row < height; row++) {
        if (y + row >= DISPLAY_HEIGHT) break;
        display_write(display_dev, x, y + row, &line_desc, line_buffer);
    }
}

void hmi_draw_text_centered(const struct device *display_dev, const char* text, int center_x, int y, uint16_t color)
{
    int text_width = strlen(text) * 9;
    int start_x = center_x - (text_width / 2);
    if (start_x < 0) start_x = 0;
    hmi_draw_text(display_dev, text, start_x, y, color);
}

void hmi_draw_top_bar(const struct device *display_dev)
{
    hmi_draw_rectangle(display_dev, 0, 0, DISPLAY_WIDTH, TOP_BAR_HEIGHT, COLOR_DARK_GRAY);

    hmi_draw_text(display_dev, hmi_data.city, CITY_X, CITY_Y, COLOR_WHITE);

    // Create combined day and gregorian date string: "Fri-12/09/2025"
    char day_date_str[32];
    if (hmi_data.day_of_week[0] != '-') {
        snprintf(day_date_str, sizeof(day_date_str), "%s-%s", hmi_data.day_of_week, hmi_data.gregorian_date);
    } else {
        strncpy(day_date_str, hmi_data.gregorian_date, sizeof(day_date_str) - 1);
        day_date_str[sizeof(day_date_str) - 1] = '\0';
    }

    hmi_draw_text_centered(display_dev, day_date_str, DISPLAY_WIDTH / 2, GREG_DATE_Y, COLOR_WHITE);

    hmi_draw_text(display_dev, hmi_data.hijri_date, HIJRI_DATE_X, HIJRI_DATE_Y, COLOR_WHITE);
}

void hmi_draw_prayer_times(const struct device *display_dev)
{
    int current_y = PRAYER_START_Y;

    for (int i = 0; i < PRAYER_COUNT; i++) {
        uint16_t bg_color = COLOR_BLACK;
        uint16_t text_color = COLOR_WHITE;

        if (hmi_data.prayers[i].is_next) {
            bg_color = COLOR_DARK_GRAY;
            text_color = COLOR_YELLOW;
        }

        // Draw centered prayer background with margins
        hmi_draw_rectangle(display_dev, PRAYER_MARGIN - 10, current_y, DISPLAY_WIDTH - 2 * (PRAYER_MARGIN - 10), PRAYER_HEIGHT, bg_color);

        hmi_draw_text(display_dev, hmi_data.prayers[i].name, PRAYER_NAME_X, current_y + 5, text_color);
        hmi_draw_text(display_dev, hmi_data.prayers[i].time, PRAYER_TIME_X, current_y + 5, text_color);

        current_y += PRAYER_HEIGHT;
    }

    if (hmi_data.countdown_text[0] != 'N' || hmi_data.countdown_text[5] != 'p') {
        hmi_draw_text_centered(display_dev, hmi_data.countdown_text, DISPLAY_WIDTH / 2, COUNTDOWN_Y, COLOR_GREEN);
    }
}

void hmi_draw_bottom_bar(const struct device *display_dev)
{
    int bottom_y = DISPLAY_HEIGHT - BOTTOM_BAR_HEIGHT;
    hmi_draw_rectangle(display_dev, 0, bottom_y, DISPLAY_WIDTH, BOTTOM_BAR_HEIGHT, COLOR_DARK_GRAY);

    if (hmi_data.weather_valid && hmi_data.weather_temp[0] != '-') {
        hmi_draw_text(display_dev, hmi_data.weather_temp, WEATHER_X, WEATHER_Y, COLOR_CYAN);
    }

    // Use fixed position for consistent time display
    hmi_draw_text(display_dev, hmi_data.current_time, CLOCK_X, CLOCK_Y, COLOR_WHITE);

    hmi_draw_text(display_dev, "SET", SETTINGS_X, SETTINGS_Y, COLOR_LIGHT_GRAY);

    char brightness_str[8];
    snprintf(brightness_str, sizeof(brightness_str), "%d%%", hmi_data.brightness_level);
    hmi_draw_text(display_dev, brightness_str, BRIGHTNESS_X, BRIGHTNESS_Y, COLOR_ORANGE);
}

static char last_time_displayed[12] = {0};

void hmi_update_display(const struct device *display_dev)
{
    // First time initialization - draw everything once
    if (!hmi_data.screen_initialized) {
        hmi_clear_screen(display_dev);
        hmi_draw_top_bar(display_dev);
        hmi_draw_prayer_times(display_dev);
        hmi_draw_bottom_bar(display_dev);
        strcpy(last_time_displayed, hmi_data.current_time);
        hmi_data.screen_initialized = true;
        return;
    }

    // Only update if time changed (ultra-fast selective update)
    if (strcmp(last_time_displayed, hmi_data.current_time) != 0) {
        // Debug: Print what we're about to display
        printk("HMI: Updating time from '%s' to '%s'\n", last_time_displayed, hmi_data.current_time);

        // Clear exact time display area for consistent positioning
        hmi_draw_rectangle(display_dev, CLOCK_X - 2, CLOCK_Y - 1, TIME_DISPLAY_WIDTH + 4, TIME_DISPLAY_HEIGHT + 2, COLOR_BLACK);

        // Small delay to ensure clear completes
        k_usleep(500);

        // Draw new time at exact position
        hmi_draw_text(display_dev, hmi_data.current_time, CLOCK_X, CLOCK_Y, COLOR_WHITE);

        // Update last displayed time
        strcpy(last_time_displayed, hmi_data.current_time);
    }

    // Reset flags
    hmi_data.needs_full_update = false;
    hmi_data.needs_time_update = false;
}

void hmi_force_full_update(const struct device *display_dev)
{
    // Force complete screen redraw (used for prayer times, dates, etc.)
    hmi_clear_screen(display_dev);
    hmi_draw_top_bar(display_dev);
    hmi_draw_prayer_times(display_dev);
    hmi_draw_bottom_bar(display_dev);

    // After full redraw, ensure time is properly displayed and tracked
    // Clear the time area specifically and redraw it cleanly
    hmi_draw_rectangle(display_dev, CLOCK_X - 2, CLOCK_Y - 1, TIME_DISPLAY_WIDTH + 4, TIME_DISPLAY_HEIGHT + 2, COLOR_BLACK);
    k_usleep(500);  // Ensure clear completes
    hmi_draw_text(display_dev, hmi_data.current_time, CLOCK_X, CLOCK_Y, COLOR_WHITE);

    // Reset time tracking to current time
    strcpy(last_time_displayed, hmi_data.current_time);
    hmi_data.screen_initialized = true;

    printk("HMI: Full update completed, time reset to: '%s'\n", hmi_data.current_time);
}

void hmi_set_city(const char* city)
{
    if (city) {
        strncpy(hmi_data.city, city, sizeof(hmi_data.city) - 1);
        hmi_data.city[sizeof(hmi_data.city) - 1] = '\0';
    }
}

void hmi_set_dates(const char* greg_date, const char* hijri_date, const char* day)
{
    if (greg_date) {
        strncpy(hmi_data.gregorian_date, greg_date, sizeof(hmi_data.gregorian_date) - 1);
        hmi_data.gregorian_date[sizeof(hmi_data.gregorian_date) - 1] = '\0';
    }
    if (hijri_date) {
        strncpy(hmi_data.hijri_date, hijri_date, sizeof(hmi_data.hijri_date) - 1);
        hmi_data.hijri_date[sizeof(hmi_data.hijri_date) - 1] = '\0';
    }
    if (day) {
        strncpy(hmi_data.day_of_week, day, sizeof(hmi_data.day_of_week) - 1);
        hmi_data.day_of_week[sizeof(hmi_data.day_of_week) - 1] = '\0';
    }
}

void hmi_set_prayer_times(const prayer_time_t* prayer_times, int next_prayer)
{
    if (prayer_times) {
        for (int i = 0; i < PRAYER_COUNT; i++) {
            hmi_data.prayers[i] = prayer_times[i];
            hmi_data.prayers[i].is_next = (i == next_prayer);
        }
        hmi_data.next_prayer_index = next_prayer;
    }
}

void hmi_set_countdown(const char* countdown)
{
    if (countdown) {
        strncpy(hmi_data.countdown_text, countdown, sizeof(hmi_data.countdown_text) - 1);
        hmi_data.countdown_text[sizeof(hmi_data.countdown_text) - 1] = '\0';
    }
}

void hmi_set_weather(const char* temperature)
{
    if (temperature) {
        strncpy(hmi_data.weather_temp, temperature, sizeof(hmi_data.weather_temp) - 1);
        hmi_data.weather_temp[sizeof(hmi_data.weather_temp) - 1] = '\0';
        hmi_data.weather_valid = true;
    }
}

void hmi_set_current_time(const char* time)
{
    if (time) {
        strncpy(hmi_data.current_time, time, sizeof(hmi_data.current_time) - 1);
        hmi_data.current_time[sizeof(hmi_data.current_time) - 1] = '\0';
    }
}

void hmi_set_brightness(uint8_t level)
{
    if (level <= 100) {
        hmi_data.brightness_level = level;
    }
}

// Backlight control functions
void hmi_backlight_init(void)
{
    backlight_dev = DEVICE_DT_GET(DT_NODELABEL(gpio0));
    if (!device_is_ready(backlight_dev)) {
        printk("Backlight GPIO device not ready\n");
        backlight_dev = NULL;
        return;
    }

    // Configure backlight pin as output and turn on by default
    gpio_pin_configure(backlight_dev, BACKLIGHT_PIN, GPIO_OUTPUT_ACTIVE | GPIO_OUTPUT_INIT_HIGH);
    gpio_pin_set(backlight_dev, BACKLIGHT_PIN, 1); // Turn on backlight
    printk("Display backlight initialized and enabled\n");
}

void hmi_set_backlight(bool on)
{
    if (backlight_dev) {
        gpio_pin_set(backlight_dev, BACKLIGHT_PIN, on ? 1 : 0);
        printk("Display backlight %s\n", on ? "enabled" : "disabled");
    }
}

void hmi_toggle_backlight(void)
{
    static bool backlight_state = true; // Default on
    backlight_state = !backlight_state;
    hmi_set_backlight(backlight_state);
}

void hmi_test_backlight(void)
{
    if (!backlight_dev) {
        printk("Backlight test failed: device not initialized\n");
        return;
    }

    printk("Starting backlight test - you should see the display backlight blink 3 times...\n");

    // Blink the backlight 3 times
    for (int i = 0; i < 3; i++) {
        printk("Backlight test %d/3: OFF\n", i + 1);
        hmi_set_backlight(false);
        k_msleep(1000); // Off for 1 second

        printk("Backlight test %d/3: ON\n", i + 1);
        hmi_set_backlight(true);
        k_msleep(1000); // On for 1 second
    }

    printk("Backlight test completed - backlight should be ON\n");
}