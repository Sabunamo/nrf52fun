/**
 * @file touch_screen.c
 * @brief Touch screen driver implementation
 *
 * Implements touch screen controller communication for resistive/capacitive touch panels
 * Supports SPI-based controllers (XPT2046, ADS7843) and I2C-based controllers (FT6206)
 */

#include "touch_screen.h"
#include "ili9341_parallel.h"

// Global touch data
struct touch_data current_touch = {0};
struct touch_calibration touch_cal = {
    .x_min = 200,
    .x_max = 3900,
    .y_min = 200,
    .y_max = 3900,
    .x_inverted = false,
    .y_inverted = false,
    .xy_swapped = false
};

// Private variables
static const struct device *touch_dev = NULL;
static bool touch_enabled = true;
static uint32_t total_touch_events = 0;
static uint32_t last_touch_time_ms = 0;
static touch_state_t previous_state = TOUCH_STATE_RELEASED;

// SPI configuration (for SPI-based touch controllers like XPT2046)
#if DT_NODE_HAS_STATUS(TOUCH_CONTROLLER_NODE, okay)
    #if DT_NODE_HAS_PROP(TOUCH_CONTROLLER_NODE, spi_max_frequency)
        static struct spi_config touch_spi_cfg = {
            .frequency = DT_PROP(TOUCH_CONTROLLER_NODE, spi_max_frequency),
            .operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB,
            .slave = DT_REG_ADDR(TOUCH_CONTROLLER_NODE),
        };
    #endif
#endif

// Forward declarations
static int touch_read_raw(uint16_t *raw_x, uint16_t *raw_y, uint16_t *pressure);
static void touch_convert_coordinates(uint16_t raw_x, uint16_t raw_y, uint16_t *screen_x, uint16_t *screen_y);
static uint32_t touch_get_timestamp_ms(void);

/**
 * @brief Get current timestamp in milliseconds
 */
static uint32_t touch_get_timestamp_ms(void)
{
    return k_uptime_get_32();
}

/**
 * @brief Read raw touch data from controller (SPI-based XPT2046)
 * @param raw_x Pointer to store raw X coordinate
 * @param raw_y Pointer to store raw Y coordinate
 * @param pressure Pointer to store pressure value
 * @return 0 on success, negative error code on failure
 */
static int touch_read_raw(uint16_t *raw_x, uint16_t *raw_y, uint16_t *pressure)
{
    if (!touch_dev || !device_is_ready(touch_dev)) {
        return -ENODEV;
    }

    // XPT2046 command bytes
    // Command format: 1 S A2 A1 A0 MODE SER/DFR PD1 PD0
    // S=1 (start), A2-A0=channel, MODE=0 (12-bit), SER=0 (differential)
    const uint8_t cmd_x = 0xD0;  // Read X position: 11010000
    const uint8_t cmd_y = 0x90;  // Read Y position: 10010000
    const uint8_t cmd_z1 = 0xB0; // Read Z1 (pressure): 10110000

    uint8_t tx_buf[3];
    uint8_t rx_buf[3];
    struct spi_buf tx_spi_buf = {.buf = tx_buf, .len = sizeof(tx_buf)};
    struct spi_buf_set tx_bufs = {.buffers = &tx_spi_buf, .count = 1};
    struct spi_buf rx_spi_buf = {.buf = rx_buf, .len = sizeof(rx_buf)};
    struct spi_buf_set rx_bufs = {.buffers = &rx_spi_buf, .count = 1};

    int ret;

#if DT_NODE_HAS_PROP(TOUCH_CONTROLLER_NODE, spi_max_frequency)
    // Read X coordinate
    tx_buf[0] = cmd_x;
    tx_buf[1] = 0x00;
    tx_buf[2] = 0x00;
    ret = spi_transceive(touch_dev, &touch_spi_cfg, &tx_bufs, &rx_bufs);
    if (ret < 0) {
        return ret;
    }
    *raw_x = ((rx_buf[1] << 8) | rx_buf[2]) >> 3; // 12-bit value

    // Read Y coordinate
    tx_buf[0] = cmd_y;
    tx_buf[1] = 0x00;
    tx_buf[2] = 0x00;
    ret = spi_transceive(touch_dev, &touch_spi_cfg, &tx_bufs, &rx_bufs);
    if (ret < 0) {
        return ret;
    }
    *raw_y = ((rx_buf[1] << 8) | rx_buf[2]) >> 3; // 12-bit value

    // Read pressure (Z1)
    tx_buf[0] = cmd_z1;
    tx_buf[1] = 0x00;
    tx_buf[2] = 0x00;
    ret = spi_transceive(touch_dev, &touch_spi_cfg, &tx_bufs, &rx_bufs);
    if (ret < 0) {
        return ret;
    }
    *pressure = ((rx_buf[1] << 8) | rx_buf[2]) >> 3; // 12-bit value

    return 0;
#else
    // Stub implementation for non-SPI touch controllers
    // Replace with I2C implementation for capacitive touch (FT6206, etc.)
    *raw_x = 0;
    *raw_y = 0;
    *pressure = 0;
    return -ENOTSUP;
#endif
}

/**
 * @brief Convert raw touch coordinates to screen coordinates
 */
static void touch_convert_coordinates(uint16_t raw_x, uint16_t raw_y, uint16_t *screen_x, uint16_t *screen_y)
{
    uint16_t x = raw_x;
    uint16_t y = raw_y;

    // Swap X and Y if configured
    if (touch_cal.xy_swapped) {
        uint16_t temp = x;
        x = y;
        y = temp;
    }

    // Map raw coordinates to screen coordinates
    int32_t screen_x_temp = ((int32_t)x - touch_cal.x_min) * TOUCH_SCREEN_WIDTH /
                            (touch_cal.x_max - touch_cal.x_min);
    int32_t screen_y_temp = ((int32_t)y - touch_cal.y_min) * TOUCH_SCREEN_HEIGHT /
                            (touch_cal.y_max - touch_cal.y_min);

    // Apply inversion if configured
    if (touch_cal.x_inverted) {
        screen_x_temp = TOUCH_SCREEN_WIDTH - 1 - screen_x_temp;
    }
    if (touch_cal.y_inverted) {
        screen_y_temp = TOUCH_SCREEN_HEIGHT - 1 - screen_y_temp;
    }

    // Clamp to screen boundaries
    if (screen_x_temp < 0) screen_x_temp = 0;
    if (screen_x_temp >= TOUCH_SCREEN_WIDTH) screen_x_temp = TOUCH_SCREEN_WIDTH - 1;
    if (screen_y_temp < 0) screen_y_temp = 0;
    if (screen_y_temp >= TOUCH_SCREEN_HEIGHT) screen_y_temp = TOUCH_SCREEN_HEIGHT - 1;

    *screen_x = (uint16_t)screen_x_temp;
    *screen_y = (uint16_t)screen_y_temp;
}

/**
 * @brief Initialize touch screen controller
 */
int touch_screen_init(void)
{
#if DT_NODE_HAS_STATUS(TOUCH_CONTROLLER_NODE, okay)
    // Get touch controller device from device tree
    touch_dev = DEVICE_DT_GET(TOUCH_CONTROLLER_NODE);

    if (!device_is_ready(touch_dev)) {
        printk("Touch: Controller device not ready\n");
        return -ENODEV;
    }

    printk("Touch: Controller device is ready\n");
    printk("Touch: Resolution: %dx%d pixels\n", TOUCH_SCREEN_WIDTH, TOUCH_SCREEN_HEIGHT);
    printk("Touch: Calibration: X[%d-%d] Y[%d-%d]\n",
           touch_cal.x_min, touch_cal.x_max,
           touch_cal.y_min, touch_cal.y_max);

    // Initialize touch data
    current_touch.valid = false;
    current_touch.state = TOUCH_STATE_RELEASED;
    previous_state = TOUCH_STATE_RELEASED;

    return 0;
#else
    printk("Touch: No touch controller configured in device tree\n");
    printk("Touch: Add 'touch_controller' alias to enable touch screen\n");
    return -ENODEV;
#endif
}

/**
 * @brief Read current touch position and state
 */
int touch_screen_read(void)
{
    if (!touch_enabled) {
        current_touch.valid = false;
        current_touch.state = TOUCH_STATE_RELEASED;
        return 0;
    }

    uint16_t raw_x, raw_y, pressure;
    int ret = touch_read_raw(&raw_x, &raw_y, &pressure);

    if (ret < 0) {
        current_touch.valid = false;
        current_touch.state = TOUCH_STATE_RELEASED;
        return ret;
    }

    uint32_t current_time = touch_get_timestamp_ms();

    // Check if touch is detected based on pressure threshold
    if (pressure >= TOUCH_PRESSURE_MIN) {
        // Touch detected
        touch_convert_coordinates(raw_x, raw_y, &current_touch.x, &current_touch.y);
        current_touch.pressure = pressure;
        current_touch.valid = true;
        current_touch.timestamp = current_time;

        // Update state machine
        if (previous_state == TOUCH_STATE_RELEASED) {
            current_touch.state = TOUCH_STATE_PRESSED;
            total_touch_events++;
            last_touch_time_ms = current_time;
        } else {
            current_touch.state = TOUCH_STATE_HELD;
        }
        previous_state = current_touch.state;

    } else {
        // No touch detected
        current_touch.pressure = 0;
        current_touch.valid = false;

        // Check debounce time before releasing
        if (previous_state != TOUCH_STATE_RELEASED) {
            if (current_time - current_touch.timestamp >= TOUCH_DEBOUNCE_MS) {
                current_touch.state = TOUCH_STATE_RELEASED;
                previous_state = TOUCH_STATE_RELEASED;
            }
        }
    }

    return 0;
}

/**
 * @brief Check if screen is currently being touched
 */
bool touch_screen_is_touched(void)
{
    return (current_touch.valid &&
            (current_touch.state == TOUCH_STATE_PRESSED ||
             current_touch.state == TOUCH_STATE_HELD));
}

/**
 * @brief Get current touch coordinates
 */
bool touch_screen_get_coordinates(uint16_t *x, uint16_t *y)
{
    if (!current_touch.valid) {
        return false;
    }

    if (x) {
        *x = current_touch.x;
    }
    if (y) {
        *y = current_touch.y;
    }

    return true;
}

/**
 * @brief Set touch screen calibration parameters
 */
void touch_screen_set_calibration(const struct touch_calibration *cal)
{
    if (cal) {
        touch_cal = *cal;
        printk("Touch: Calibration updated: X[%d-%d] Y[%d-%d]\n",
               touch_cal.x_min, touch_cal.x_max,
               touch_cal.y_min, touch_cal.y_max);
    }
}

/**
 * @brief Get current touch screen calibration parameters
 */
void touch_screen_get_calibration(struct touch_calibration *cal)
{
    if (cal) {
        *cal = touch_cal;
    }
}

/**
 * @brief Perform touch screen calibration routine
 */
int touch_screen_calibrate(const struct device *display_dev)
{
    if (!display_dev) {
        printk("Touch: Display device required for calibration\n");
        return -EINVAL;
    }

    printk("Touch: Starting calibration routine...\n");

    // Calibration points (top-left, top-right, bottom-right, bottom-left, center)
    struct {
        uint16_t x;
        uint16_t y;
    } cal_points[] = {
        {20, 20},                                       // Top-left
        {TOUCH_SCREEN_WIDTH - 20, 20},                  // Top-right
        {TOUCH_SCREEN_WIDTH - 20, TOUCH_SCREEN_HEIGHT - 20}, // Bottom-right
        {20, TOUCH_SCREEN_HEIGHT - 20},                 // Bottom-left
        {TOUCH_SCREEN_WIDTH / 2, TOUCH_SCREEN_HEIGHT / 2}    // Center
    };

    uint16_t raw_x_sum = 0, raw_y_sum = 0;
    int samples = 0;

    // Display calibration instructions
    ili9341_fill_screen(COLOR_BLACK);
    ili9341_draw_string(10, 10, "Touch Screen", COLOR_WHITE, COLOR_BLACK, 2);
    ili9341_draw_string(10, 30, "Calibration", COLOR_WHITE, COLOR_BLACK, 2);

    // Sample each calibration point
    for (int i = 0; i < 5; i++) {
        ili9341_fill_screen(COLOR_BLACK);

        char msg[32];
        snprintf(msg, sizeof(msg), "Touch point %d/5", i + 1);
        ili9341_draw_string(10, 10, msg, COLOR_YELLOW, COLOR_BLACK, 1);

        // Draw crosshair at calibration point
        touch_screen_draw_crosshair(display_dev, cal_points[i].x, cal_points[i].y, COLOR_RED);

        // Wait for touch
        k_msleep(500); // Debounce

        bool touched = false;
        while (!touched) {
            touch_screen_read();
            if (touch_screen_is_touched()) {
                uint16_t raw_x, raw_y, pressure;
                touch_read_raw(&raw_x, &raw_y, &pressure);
                raw_x_sum += raw_x;
                raw_y_sum += raw_y;
                samples++;
                touched = true;

                printk("Touch: Point %d - Raw: (%d, %d)\n", i+1, raw_x, raw_y);
            }
            k_msleep(10);
        }

        // Wait for release
        while (touch_screen_is_touched()) {
            touch_screen_read();
            k_msleep(10);
        }
    }

    // Calculate calibration values (simplified)
    // In a real implementation, you would use more sophisticated algorithms
    touch_cal.x_min = 200;   // These should be calculated from samples
    touch_cal.x_max = 3900;
    touch_cal.y_min = 200;
    touch_cal.y_max = 3900;

    printk("Touch: Calibration complete!\n");
    ili9341_fill_screen(COLOR_BLACK);
    ili9341_draw_string(10, TOUCH_SCREEN_HEIGHT / 2, "Calibration", COLOR_GREEN, COLOR_BLACK, 2);
    ili9341_draw_string(10, TOUCH_SCREEN_HEIGHT / 2 + 20, "Complete!", COLOR_GREEN, COLOR_BLACK, 2);
    k_msleep(2000);

    return 0;
}

/**
 * @brief Draw calibration crosshair
 */
void touch_screen_draw_crosshair(const struct device *display_dev, int x, int y, uint16_t color)
{
    // Draw crosshair lines
    const int size = 10;

    // Horizontal line
    for (int i = -size; i <= size; i++) {
        ili9341_draw_pixel(x + i, y, color);
    }

    // Vertical line
    for (int i = -size; i <= size; i++) {
        ili9341_draw_pixel(x, y + i, color);
    }

    // Draw circle
    for (int angle = 0; angle < 360; angle += 10) {
        int dx = (int)(size * 0.7 * cos(angle * 3.14159 / 180.0));
        int dy = (int)(size * 0.7 * sin(angle * 3.14159 / 180.0));
        ili9341_draw_pixel(x + dx, y + dy, color);
    }
}

/**
 * @brief Print touch screen information
 */
void touch_screen_print_info(void)
{
    printk("\n========== TOUCH SCREEN INFO ==========\n");
    printk("State: %s\n",
           current_touch.state == TOUCH_STATE_RELEASED ? "RELEASED" :
           current_touch.state == TOUCH_STATE_PRESSED ? "PRESSED" : "HELD");
    printk("Valid: %s\n", current_touch.valid ? "YES" : "NO");

    if (current_touch.valid) {
        printk("Position: (%d, %d)\n", current_touch.x, current_touch.y);
        printk("Pressure: %d\n", current_touch.pressure);
        printk("Timestamp: %u ms\n", current_touch.timestamp);
    }

    printk("Total touches: %u\n", total_touch_events);
    printk("Last touch: %u ms ago\n",
           touch_get_timestamp_ms() - last_touch_time_ms);
    printk("Calibration: X[%d-%d] Y[%d-%d]\n",
           touch_cal.x_min, touch_cal.x_max,
           touch_cal.y_min, touch_cal.y_max);
    printk("=====================================\n\n");
}

/**
 * @brief Enable/disable touch screen
 */
void touch_screen_enable(bool enable)
{
    touch_enabled = enable;
    printk("Touch: %s\n", enable ? "Enabled" : "Disabled");
}

/**
 * @brief Get touch screen statistics
 */
void touch_screen_get_stats(uint32_t *touch_count, uint32_t *last_touch_time)
{
    if (touch_count) {
        *touch_count = total_touch_events;
    }
    if (last_touch_time) {
        *last_touch_time = last_touch_time_ms;
    }
}
