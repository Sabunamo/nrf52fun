/**
 * @file touch_screen.h
 * @brief Touch screen driver for resistive/capacitive touch controllers
 *
 * This module handles touch screen communication and processes touch events
 * to provide touch coordinates and state information.
 *
 * Hardware: Touch screen controller (XPT2046, FT6206, or compatible)
 * - Interface: SPI or I2C (configurable via device tree)
 * - Resolution: Matches display resolution
 * - Touch detection with debouncing
 *
 * Configuration:
 * - Device tree alias: touch_controller
 * - Calibration parameters for accurate coordinate mapping
 * - Debounce time for stable touch detection
 *
 * COMPATIBLE INTERFACE: Can be used with ILI9341 display driver
 */

#ifndef TOUCH_SCREEN_H
#define TOUCH_SCREEN_H

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <stdbool.h>

// Hardware configuration
#if DT_NODE_EXISTS(DT_ALIAS(touch_controller))
    #define TOUCH_CONTROLLER_NODE DT_ALIAS(touch_controller)
#else
    #define TOUCH_CONTROLLER_NODE DT_INVALID_NODE
#endif

// Touch screen resolution (adjust to match your display)
#define TOUCH_SCREEN_WIDTH  320     ///< Touch screen width in pixels
#define TOUCH_SCREEN_HEIGHT 240     ///< Touch screen height in pixels

// Touch detection thresholds
#define TOUCH_PRESSURE_MIN  100     ///< Minimum pressure to register touch
#define TOUCH_DEBOUNCE_MS   50      ///< Debounce time in milliseconds

/**
 * @brief Touch state enumeration
 */
typedef enum {
    TOUCH_STATE_RELEASED = 0,   ///< Touch is not detected
    TOUCH_STATE_PRESSED,        ///< Touch is currently active
    TOUCH_STATE_HELD            ///< Touch is being held
} touch_state_t;

/**
 * @brief Touch data structure containing position and state
 */
struct touch_data {
    uint16_t x;                 ///< X coordinate in pixels (0 to TOUCH_SCREEN_WIDTH-1)
    uint16_t y;                 ///< Y coordinate in pixels (0 to TOUCH_SCREEN_HEIGHT-1)
    uint16_t pressure;          ///< Touch pressure value (0 = no touch)
    touch_state_t state;        ///< Current touch state
    bool valid;                 ///< True when touch data is valid
    uint32_t timestamp;         ///< Timestamp of last touch event (ms)
};

/**
 * @brief Touch calibration data structure
 */
struct touch_calibration {
    int16_t x_min;              ///< Minimum raw X value
    int16_t x_max;              ///< Maximum raw X value
    int16_t y_min;              ///< Minimum raw Y value
    int16_t y_max;              ///< Maximum raw Y value
    bool x_inverted;            ///< True if X axis is inverted
    bool y_inverted;            ///< True if Y axis is inverted
    bool xy_swapped;            ///< True if X and Y axes are swapped
};

// Global touch data instance
extern struct touch_data current_touch;
extern struct touch_calibration touch_cal;

// Function declarations

/**
 * @brief Initialize touch screen controller
 * @return 0 on success, negative error code on failure
 */
int touch_screen_init(void);

/**
 * @brief Read current touch position and state
 * Updates the global current_touch structure with latest touch data
 * @return 0 on success, negative error code on failure
 */
int touch_screen_read(void);

/**
 * @brief Check if screen is currently being touched
 * @return true if touch is detected, false otherwise
 */
bool touch_screen_is_touched(void);

/**
 * @brief Get current touch coordinates
 * @param x Pointer to store X coordinate (can be NULL)
 * @param y Pointer to store Y coordinate (can be NULL)
 * @return true if touch is valid, false otherwise
 */
bool touch_screen_get_coordinates(uint16_t *x, uint16_t *y);

/**
 * @brief Set touch screen calibration parameters
 * @param cal Pointer to calibration data structure
 */
void touch_screen_set_calibration(const struct touch_calibration *cal);

/**
 * @brief Get current touch screen calibration parameters
 * @param cal Pointer to store calibration data
 */
void touch_screen_get_calibration(struct touch_calibration *cal);

/**
 * @brief Perform touch screen calibration routine
 * Guides user through calibration points and calculates calibration values
 * @param display_dev Display device pointer for showing calibration UI
 * @return 0 on success, negative error code on failure
 */
int touch_screen_calibrate(const struct device *display_dev);

/**
 * @brief Print touch screen information to console (debug function)
 */
void touch_screen_print_info(void);

/**
 * @brief Display touch crosshair on screen
 * @param display_dev Display device pointer
 * @param x X position of crosshair
 * @param y Y position of crosshair
 * @param color Color of crosshair
 */
void touch_screen_draw_crosshair(const struct device *display_dev, int x, int y, uint16_t color);

/**
 * @brief Enable/disable touch screen
 * @param enable true to enable, false to disable
 */
void touch_screen_enable(bool enable);

/**
 * @brief Get touch screen statistics
 * @param touch_count Pointer to store total touch events (can be NULL)
 * @param last_touch_time Pointer to store last touch timestamp (can be NULL)
 */
void touch_screen_get_stats(uint32_t *touch_count, uint32_t *last_touch_time);

#endif // TOUCH_SCREEN_H
