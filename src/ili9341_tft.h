#ifndef PRAYER_HMI_H
#define PRAYER_HMI_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <stdbool.h>
#include "prayerTime.h"

// Display specifications for 320x240 RGB565
#define DISPLAY_WIDTH       320
#define DISPLAY_HEIGHT      240
#define CHAR_WIDTH          8
#define CHAR_HEIGHT         16

// RGB565 Color definitions
#define COLOR_BLACK         0x0000
#define COLOR_WHITE         0xFFFF
#define COLOR_RED           0xF800
#define COLOR_GREEN         0x07E0
#define COLOR_BLUE          0x001F
#define COLOR_YELLOW        0xFFE0
#define COLOR_CYAN          0x07FF
#define COLOR_MAGENTA       0xF81F
#define COLOR_GRAY          0x8410
#define COLOR_DARK_GRAY     0x4208
#define COLOR_LIGHT_GRAY    0xC618
#define COLOR_ORANGE        0xFD20

// Layout dimensions
#define TOP_BAR_HEIGHT      30
#define BOTTOM_BAR_HEIGHT   30
#define MIDDLE_HEIGHT       (DISPLAY_HEIGHT - TOP_BAR_HEIGHT - BOTTOM_BAR_HEIGHT)

// Top bar positions
#define CITY_X              5
#define CITY_Y              5
#define GREG_DATE_X         (DISPLAY_WIDTH / 2 - 40)
#define GREG_DATE_Y         5
#define HIJRI_DATE_X        (DISPLAY_WIDTH - 85)
#define HIJRI_DATE_Y        5

// Middle section - Prayer times (centered with equal margins)
#define PRAYER_START_Y      (TOP_BAR_HEIGHT + 10)
#define PRAYER_HEIGHT       25                          // Height for 16x16 font (16px tall + spacing)
#define PRAYER_MARGIN       45                          // Equal margin from both sides
#define PRAYER_NAME_X       PRAYER_MARGIN               // Left-aligned prayer names
#define PRAYER_TIME_X       (DISPLAY_WIDTH - PRAYER_MARGIN - 85)  // Right-aligned times (85px for "HH:MM" in 16x16)
#define COUNTDOWN_Y         (PRAYER_START_Y + 6 * PRAYER_HEIGHT + 10)

// Bottom bar positions
#define WEATHER_X           5
#define WEATHER_Y           (DISPLAY_HEIGHT - BOTTOM_BAR_HEIGHT + 5)
#define CLOCK_X             (DISPLAY_WIDTH / 2 - 30)
#define CLOCK_Y             (DISPLAY_HEIGHT - BOTTOM_BAR_HEIGHT + 5)
#define TIME_DISPLAY_WIDTH  72    // 8 chars * 9 pixels per char
#define TIME_DISPLAY_HEIGHT 16    // Font height
#define SETTINGS_X          (DISPLAY_WIDTH - 60)
#define SETTINGS_Y          (DISPLAY_HEIGHT - BOTTOM_BAR_HEIGHT + 5)
#define BRIGHTNESS_X        (DISPLAY_WIDTH - 30)
#define BRIGHTNESS_Y        (DISPLAY_HEIGHT - BOTTOM_BAR_HEIGHT + 5)

// Prayer names
typedef enum {
    PRAYER_FAJR = 0,
    PRAYER_SHURUQ = 1,
    PRAYER_DHUHR = 2,
    PRAYER_ASR = 3,
    PRAYER_MAGHRIB = 4,
    PRAYER_ISHA = 5,
    PRAYER_COUNT = 6
} prayer_index_t;

// Prayer time structure
typedef struct {
    char name[10];      // Prayer name
    char time[8];       // HH:MM format
    bool is_next;       // Is this the next prayer?
} prayer_time_t;

// HMI display data structure
typedef struct {
    // Location and date info
    char city[20];
    char gregorian_date[12];
    char hijri_date[12];
    char day_of_week[4];

    // Prayer times
    prayer_time_t prayers[PRAYER_COUNT];
    int next_prayer_index;
    char countdown_text[25];

    // Bottom bar info
    char weather_temp[8];
    char current_time[12];  // "HH:MM:SS" format + extra space
    uint8_t brightness_level;

    // Status flags
    bool gps_valid;
    bool prayer_times_valid;
    bool weather_valid;

    // Update flags to prevent unnecessary redraws
    bool needs_full_update;
    bool needs_time_update;
    bool screen_initialized;
} hmi_display_data_t;

// Function declarations
void hmi_init(void);
void hmi_update_display(const struct device *display_dev);
void hmi_force_full_update(const struct device *display_dev);
void hmi_set_city(const char* city);
void hmi_set_dates(const char* greg_date, const char* hijri_date, const char* day);
void hmi_set_prayer_times(const prayer_time_t* prayer_times, int next_prayer);
void hmi_set_countdown(const char* countdown);
void hmi_set_weather(const char* temperature);
void hmi_set_current_time(const char* time);
void hmi_set_brightness(uint8_t level);


// Display section functions
void hmi_draw_top_bar(const struct device *display_dev);
void hmi_draw_prayer_times(const struct device *display_dev);
void hmi_draw_bottom_bar(const struct device *display_dev);
void hmi_clear_screen(const struct device *display_dev);

// Utility functions
void hmi_draw_text_centered(const struct device *display_dev, const char* text,
                           int center_x, int y, uint16_t color);
void hmi_draw_rectangle(const struct device *display_dev, int x, int y,
                       int width, int height, uint16_t color);

// Image display functions
int hmi_display_bmp_image(const struct device *display_dev, const char* filename);

// ========================================================================
// Compatible API with ili9341_parallel.h (for unified main.c)
// ========================================================================

/**
 * @brief Initialize ILI9341 display (compatible wrapper)
 * @return 0 on success, negative error code on failure
 */
int ili9341_init(void);

/**
 * @brief Fill entire screen with a color (compatible wrapper)
 * @param color RGB565 color value
 */
void ili9341_fill_screen(uint16_t color);

/**
 * @brief Set display rotation (compatible wrapper)
 * @param rotation 0=portrait, 1=landscape, 2=portrait180, 3=landscape180
 */
void ili9341_set_rotation(uint8_t rotation);

/**
 * @brief Draw a string on the display (compatible wrapper)
 * @param x X coordinate
 * @param y Y coordinate
 * @param str String to display
 * @param fg_color Foreground color (RGB565)
 * @param bg_color Background color (RGB565)
 * @param size Font size multiplier (1=8x16, 2=16x32, etc.)
 */
void ili9341_draw_string(int x, int y, const char *str, uint16_t fg_color, uint16_t bg_color, int size);

/**
 * @brief Draw a horizontal line (compatible wrapper)
 * @param x Starting X coordinate
 * @param y Y coordinate
 * @param w Width in pixels
 * @param color Line color (RGB565)
 */
void ili9341_draw_hline(int x, int y, int w, uint16_t color);

#endif // PRAYER_HMI_H