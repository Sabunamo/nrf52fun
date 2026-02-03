/**
 * @file ft6206_touch.c
 * @brief FT6206 capacitive touch screen driver (I2C)
 *
 * I2C driver for FocalTech FT6206 on Adafruit 1947 2.8" TFT.
 * Touch coordinates are mapped from native 240x320 portrait
 * to 320x240 landscape (matching display 90 deg rotation).
 *
 * I2C0: P1.02 (SDA), P1.03 (SCL) with internal pull-ups
 * Address: 0x38, Chip ID: 0x11
 */

#include "ft6206_touch.h"
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ft6206_touch, LOG_LEVEL_INF);

/* Global touch data */
ft6206_touch_data_t ft6206_touch = {0};

/* I2C device */
static const struct device *i2c_dev = NULL;
static bool initialized = false;
static ft6206_touch_state_t prev_state = TOUCH_RELEASED;

/* ---- I2C helpers ---- */

static int ft6206_read_reg(uint8_t reg, uint8_t *val)
{
    return i2c_reg_read_byte(i2c_dev, FT6206_I2C_ADDR, reg, val);
}

static int ft6206_write_reg(uint8_t reg, uint8_t val)
{
    return i2c_reg_write_byte(i2c_dev, FT6206_I2C_ADDR, reg, val);
}

static int ft6206_read_burst(uint8_t start_reg, uint8_t *buf, uint8_t len)
{
    return i2c_burst_read(i2c_dev, FT6206_I2C_ADDR, start_reg, buf, len);
}

/* ---- Coordinate mapping ---- */

/**
 * Map native FT6206 coordinates (240x320 portrait) to display
 * coordinates (320x240 landscape, 90 deg rotation).
 *
 * Native FT6206:  X = 0..239 (width),  Y = 0..319 (height)
 * Display (rot90): X = 0..319 (width),  Y = 0..239 (height)
 *
 * Mapping for 90 deg CW rotation (with axis inversion):
 *   display_x = (319 - native_y)
 *   display_y = native_x
 */
static void ft6206_map_coordinates(uint16_t native_x, uint16_t native_y,
                                   uint16_t *screen_x, uint16_t *screen_y)
{
    *screen_x = (319 - native_y);
    *screen_y = native_x;

    /* Clamp to screen bounds */
    if (*screen_x >= FT6206_SCREEN_WIDTH) {
        *screen_x = FT6206_SCREEN_WIDTH - 1;
    }
    if (*screen_y >= FT6206_SCREEN_HEIGHT) {
        *screen_y = FT6206_SCREEN_HEIGHT - 1;
    }
}

/* ---- Public API ---- */

int ft6206_init(void)
{
    /* Get I2C0 device from devicetree */
    i2c_dev = DEVICE_DT_GET(DT_NODELABEL(i2c0));

    if (!device_is_ready(i2c_dev)) {
        LOG_ERR("I2C0 device not ready");
        return -ENODEV;
    }

    /* Verify chip ID */
    uint8_t chip_id = 0;
    int ret = ft6206_read_reg(FT6206_REG_CHIP_ID, &chip_id);
    if (ret < 0) {
        LOG_ERR("Failed to read chip ID: %d", ret);
        return ret;
    }

    if (chip_id != FT6206_CHIP_ID_EXPECTED) {
        LOG_WRN("Unexpected chip ID: 0x%02X (expected 0x%02X)",
                chip_id, FT6206_CHIP_ID_EXPECTED);
        /* Continue anyway - some FT6x06 variants report different IDs */
    }

    LOG_INF("FT6206 detected at 0x%02X, chip ID: 0x%02X", FT6206_I2C_ADDR, chip_id);

    /* Set touch detection threshold */
    ret = ft6206_write_reg(FT6206_REG_TH_GROUP, FT6206_DEFAULT_THRESHOLD);
    if (ret < 0) {
        LOG_ERR("Failed to set threshold: %d", ret);
        return ret;
    }

    /* Set active mode with interrupt polling */
    ret = ft6206_write_reg(FT6206_REG_CTRL, 0x00);
    if (ret < 0) {
        LOG_ERR("Failed to set control mode: %d", ret);
        return ret;
    }

    /* Initialize touch data */
    memset(&ft6206_touch, 0, sizeof(ft6206_touch));
    ft6206_touch.state = TOUCH_RELEASED;
    prev_state = TOUCH_RELEASED;
    initialized = true;

    ft6206_print_info();

    return 0;
}

int ft6206_read(void)
{
    if (!initialized) {
        return -ENODEV;
    }

    /*
     * Read registers 0x00 through 0x0E in one burst (15 bytes).
     * This covers: DEV_MODE, GEST_ID, TD_STATUS, P1 data (6 bytes), P2 data (6 bytes)
     */
    uint8_t buf[15];
    int ret = ft6206_read_burst(FT6206_REG_DEV_MODE, buf, sizeof(buf));
    if (ret < 0) {
        ft6206_touch.valid = false;
        return ret;
    }

    uint8_t gesture   = buf[0x01];  /* GEST_ID */
    uint8_t td_status = buf[0x02];  /* TD_STATUS - number of touch points */
    uint8_t num_points = td_status & 0x0F;

    if (num_points > FT6206_MAX_TOUCH_POINTS) {
        num_points = 0;  /* Invalid reading */
    }

    ft6206_touch.gesture = gesture;
    ft6206_touch.num_points = num_points;
    ft6206_touch.timestamp = k_uptime_get_32();

    if (num_points > 0) {
        /* Parse touch point 1 */
        uint8_t event1 = (buf[0x03] >> 6) & 0x03;
        uint16_t native_x1 = ((buf[0x03] & 0x0F) << 8) | buf[0x04];
        uint16_t native_y1 = ((buf[0x05] & 0x0F) << 8) | buf[0x06];
        uint8_t touch_id1  = (buf[0x05] >> 4) & 0x0F;
        uint8_t weight1    = buf[0x07];

        ft6206_map_coordinates(native_x1, native_y1,
                               &ft6206_touch.points[0].x,
                               &ft6206_touch.points[0].y);
        ft6206_touch.points[0].event    = event1;
        ft6206_touch.points[0].touch_id = touch_id1;
        ft6206_touch.points[0].weight   = weight1;

        if (num_points > 1) {
            /* Parse touch point 2 */
            uint8_t event2 = (buf[0x09] >> 6) & 0x03;
            uint16_t native_x2 = ((buf[0x09] & 0x0F) << 8) | buf[0x0A];
            uint16_t native_y2 = ((buf[0x0B] & 0x0F) << 8) | buf[0x0C];
            uint8_t touch_id2  = (buf[0x0B] >> 4) & 0x0F;
            uint8_t weight2    = buf[0x0D];

            ft6206_map_coordinates(native_x2, native_y2,
                                   &ft6206_touch.points[1].x,
                                   &ft6206_touch.points[1].y);
            ft6206_touch.points[1].event    = event2;
            ft6206_touch.points[1].touch_id = touch_id2;
            ft6206_touch.points[1].weight   = weight2;
        }

        ft6206_touch.valid = true;

        /* Update state machine */
        if (prev_state == TOUCH_RELEASED) {
            ft6206_touch.state = TOUCH_PRESSED;
        } else {
            ft6206_touch.state = TOUCH_HELD;
        }
        prev_state = ft6206_touch.state;

    } else {
        /* No touch */
        ft6206_touch.valid = false;
        ft6206_touch.state = TOUCH_RELEASED;
        prev_state = TOUCH_RELEASED;
    }

    return 0;
}

bool ft6206_is_touched(void)
{
    return (ft6206_touch.valid && ft6206_touch.num_points > 0 &&
            ft6206_touch.state != TOUCH_RELEASED);
}

bool ft6206_get_point(uint16_t *x, uint16_t *y)
{
    if (!ft6206_is_touched()) {
        return false;
    }

    if (x) {
        *x = ft6206_touch.points[0].x;
    }
    if (y) {
        *y = ft6206_touch.points[0].y;
    }

    return true;
}

int ft6206_set_threshold(uint8_t threshold)
{
    if (!initialized) {
        return -ENODEV;
    }

    int ret = ft6206_write_reg(FT6206_REG_TH_GROUP, threshold);
    if (ret < 0) {
        LOG_ERR("Failed to set threshold: %d", ret);
        return ret;
    }

    LOG_INF("Touch threshold set to %d", threshold);
    return 0;
}

bool ft6206_is_ready(void)
{
    return initialized;
}

void ft6206_print_info(void)
{
    if (!initialized) {
        printk("FT6206: Not initialized\n");
        return;
    }

    uint8_t chip_id = 0, fw_id = 0, focal_id = 0, release = 0;
    uint8_t lib_h = 0, lib_l = 0, threshold = 0;

    ft6206_read_reg(FT6206_REG_CHIP_ID, &chip_id);
    ft6206_read_reg(FT6206_REG_FIRMID, &fw_id);
    ft6206_read_reg(FT6206_REG_FOCALTECH_ID, &focal_id);
    ft6206_read_reg(FT6206_REG_RELEASE_CODE, &release);
    ft6206_read_reg(FT6206_REG_LIB_VER_H, &lib_h);
    ft6206_read_reg(FT6206_REG_LIB_VER_L, &lib_l);
    ft6206_read_reg(FT6206_REG_TH_GROUP, &threshold);

    printk("\n===== FT6206 Touch Controller =====\n");
    printk("Chip ID:     0x%02X\n", chip_id);
    printk("Firmware:    0x%02X\n", fw_id);
    printk("FocalTech:   0x%02X\n", focal_id);
    printk("Release:     0x%02X\n", release);
    printk("Library:     %d.%d\n", lib_h, lib_l);
    printk("Threshold:   %d\n", threshold);
    printk("I2C Addr:    0x%02X\n", FT6206_I2C_ADDR);
    printk("I2C Bus:     I2C0 (P0.26 SDA, P0.27 SCL)\n");
    printk("Screen:      %dx%d (landscape)\n", FT6206_SCREEN_WIDTH, FT6206_SCREEN_HEIGHT);
    printk("===================================\n\n");
}
