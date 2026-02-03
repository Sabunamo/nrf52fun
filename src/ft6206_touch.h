/**
 * @file ft6206_touch.h
 * @brief FT6206 capacitive touch screen driver (I2C)
 *
 * Driver for FocalTech FT6206 capacitive touch controller on the
 * Adafruit 1947 2.8" TFT display shield.
 *
 * Hardware:
 * - Interface: I2C0 @ 0x38
 * - Pins: P1.02 (SDA), P1.03 (SCL) with internal pull-ups
 * - Chip ID: 0x11
 * - Supports up to 2 simultaneous touch points
 * - Native resolution: 240x320, mapped to 320x240 (90 deg landscape)
 */

#ifndef FT6206_TOUCH_H
#define FT6206_TOUCH_H

#include <zephyr/kernel.h>
#include <stdbool.h>
#include <stdint.h>

/* FT6206 I2C address */
#define FT6206_I2C_ADDR             0x38

/* FT6206 register map */
#define FT6206_REG_DEV_MODE         0x00
#define FT6206_REG_GEST_ID          0x01
#define FT6206_REG_TD_STATUS        0x02
#define FT6206_REG_P1_XH            0x03
#define FT6206_REG_P1_XL            0x04
#define FT6206_REG_P1_YH            0x05
#define FT6206_REG_P1_YL            0x06
#define FT6206_REG_P1_WEIGHT        0x07
#define FT6206_REG_P1_MISC          0x08
#define FT6206_REG_P2_XH            0x09
#define FT6206_REG_P2_XL            0x0A
#define FT6206_REG_P2_YH            0x0B
#define FT6206_REG_P2_YL            0x0C
#define FT6206_REG_TH_GROUP         0x80
#define FT6206_REG_TH_DIFF          0x85
#define FT6206_REG_CTRL             0x86
#define FT6206_REG_TIMEENTERMONITOR 0x87
#define FT6206_REG_PERIODACTIVE     0x88
#define FT6206_REG_PERIODMONITOR    0x89
#define FT6206_REG_LIB_VER_H        0xA1
#define FT6206_REG_LIB_VER_L        0xA2
#define FT6206_REG_CHIP_ID          0xA3
#define FT6206_REG_G_MODE           0xA4
#define FT6206_REG_FIRMID           0xA7
#define FT6206_REG_FOCALTECH_ID     0xA8
#define FT6206_REG_RELEASE_CODE     0xA9

/* Expected chip ID */
#define FT6206_CHIP_ID_EXPECTED     0x11

/* Touch event flags (bits [7:6] of P1_XH) */
#define FT6206_EVENT_PUT_DOWN       0x00
#define FT6206_EVENT_PUT_UP         0x01
#define FT6206_EVENT_CONTACT        0x02

/* Gesture IDs */
#define FT6206_GEST_NONE            0x00
#define FT6206_GEST_MOVE_UP         0x10
#define FT6206_GEST_MOVE_LEFT       0x14
#define FT6206_GEST_MOVE_DOWN       0x18
#define FT6206_GEST_MOVE_RIGHT      0x1C
#define FT6206_GEST_ZOOM_IN         0x48
#define FT6206_GEST_ZOOM_OUT        0x49

/* Default touch threshold (0-255, lower = more sensitive) */
#define FT6206_DEFAULT_THRESHOLD    128

/* Screen dimensions (landscape after 90 deg rotation) */
#define FT6206_SCREEN_WIDTH         320
#define FT6206_SCREEN_HEIGHT        240

/* Maximum touch points */
#define FT6206_MAX_TOUCH_POINTS     2

/**
 * @brief Touch point state
 */
typedef enum {
    TOUCH_RELEASED = 0,
    TOUCH_PRESSED,
    TOUCH_HELD
} ft6206_touch_state_t;

/**
 * @brief Single touch point data
 */
typedef struct {
    uint16_t x;             // Screen X coordinate (0 to 319, landscape)
    uint16_t y;             // Screen Y coordinate (0 to 239, landscape)
    uint8_t  event;         // Event flag (put_down, put_up, contact)
    uint8_t  touch_id;      // Touch point ID (0 or 1)
    uint8_t  weight;        // Touch weight / pressure
} ft6206_point_t;

/**
 * @brief Touch screen data structure
 */
typedef struct {
    ft6206_point_t points[FT6206_MAX_TOUCH_POINTS];
    uint8_t  num_points;    // Number of active touch points (0-2)
    uint8_t  gesture;       // Current gesture ID
    ft6206_touch_state_t state;  // Overall touch state
    bool     valid;         // Data validity flag
    uint32_t timestamp;     // Timestamp of last read (ms)
} ft6206_touch_data_t;

/* Global touch data */
extern ft6206_touch_data_t ft6206_touch;

/**
 * @brief Initialize FT6206 touch controller
 *
 * Verifies chip ID, configures touch threshold, and sets active mode.
 *
 * @return 0 on success, negative error code on failure
 */
int ft6206_init(void);

/**
 * @brief Read touch data from FT6206
 *
 * Reads all active touch points and gesture data.
 * Updates the global ft6206_touch structure.
 *
 * @return 0 on success, negative error code on failure
 */
int ft6206_read(void);

/**
 * @brief Check if screen is currently being touched
 * @return true if at least one touch point is active
 */
bool ft6206_is_touched(void);

/**
 * @brief Get first touch point coordinates (landscape)
 * @param x Pointer to store X coordinate (can be NULL)
 * @param y Pointer to store Y coordinate (can be NULL)
 * @return true if touch is active, false otherwise
 */
bool ft6206_get_point(uint16_t *x, uint16_t *y);

/**
 * @brief Set touch detection threshold
 * @param threshold Threshold value (0-255, lower = more sensitive)
 * @return 0 on success, negative error code on failure
 */
int ft6206_set_threshold(uint8_t threshold);

/**
 * @brief Check if FT6206 is initialized and ready
 * @return true if ready, false otherwise
 */
bool ft6206_is_ready(void);

/**
 * @brief Print FT6206 device info (chip ID, firmware, library version)
 */
void ft6206_print_info(void);

#endif // FT6206_TOUCH_H
