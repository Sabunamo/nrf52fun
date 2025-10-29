/*
 * ILI9341 TFT LCD Driver - Parallel 8-bit Interface
 * For MCUFriend 2.4" TFT LCD Shield on nRF52-DK
 */

#include "ili9341_parallel.h"
#include "font.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(ili9341, LOG_LEVEL_DBG);

/* GPIO pins for parallel interface */
static const struct gpio_dt_spec data_pins[] = {
    GPIO_DT_SPEC_GET(DT_NODELABEL(lcd_d0), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(lcd_d1), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(lcd_d2), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(lcd_d3), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(lcd_d4), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(lcd_d5), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(lcd_d6), gpios),
    GPIO_DT_SPEC_GET(DT_NODELABEL(lcd_d7), gpios),
};

static const struct gpio_dt_spec lcd_rst = GPIO_DT_SPEC_GET(DT_NODELABEL(lcd_rst), gpios);
static const struct gpio_dt_spec lcd_cs = GPIO_DT_SPEC_GET(DT_NODELABEL(lcd_cs), gpios);
static const struct gpio_dt_spec lcd_rs = GPIO_DT_SPEC_GET(DT_NODELABEL(lcd_rs), gpios);
static const struct gpio_dt_spec lcd_wr = GPIO_DT_SPEC_GET(DT_NODELABEL(lcd_wr), gpios);
static const struct gpio_dt_spec lcd_rd = GPIO_DT_SPEC_GET(DT_NODELABEL(lcd_rd), gpios);

static uint16_t screen_width = ILI9341_WIDTH;
static uint16_t screen_height = ILI9341_HEIGHT;

/* Low-level parallel interface functions */
static inline void write_data_bus(uint8_t data)
{
    for (int i = 0; i < 8; i++) {
        gpio_pin_set_dt(&data_pins[i], (data >> i) & 0x01);
    }
}

static inline void pulse_wr(void)
{
    k_busy_wait(5);  // Setup time - ensure data is stable (increased for nRF5340)
    gpio_pin_set_dt(&lcd_wr, 0);
    k_busy_wait(10);  // WR low pulse width (increased for nRF5340)
    gpio_pin_set_dt(&lcd_wr, 1);
    k_busy_wait(5);  // Hold time after WR goes high (increased for nRF5340)
}

static void write_command(uint8_t cmd)
{
    gpio_pin_set_dt(&lcd_rs, 0);  // Command mode (must be set before CS)
    k_busy_wait(2);  // Allow RS to settle
    gpio_pin_set_dt(&lcd_cs, 0);  // CS active low
    k_busy_wait(2);  // CS setup time (increased for nRF5340)
    write_data_bus(cmd);
    pulse_wr();
    gpio_pin_set_dt(&lcd_cs, 1);  // CS inactive high
    k_busy_wait(2);  // CS hold time (increased for nRF5340)
}

static void write_data(uint8_t data)
{
    gpio_pin_set_dt(&lcd_rs, 1);  // Data mode (must be set before CS)
    k_busy_wait(2);  // Allow RS to settle
    gpio_pin_set_dt(&lcd_cs, 0);  // CS active low
    k_busy_wait(2);  // CS setup time (increased for nRF5340)
    write_data_bus(data);
    pulse_wr();
    gpio_pin_set_dt(&lcd_cs, 1);  // CS inactive high
    k_busy_wait(2);  // CS hold time (increased for nRF5340)
}

static void write_data16(uint16_t data)
{
    write_data(data >> 8);    // High byte
    write_data(data & 0xFF);  // Low byte
}

static void hardware_reset(void)
{
    // Ensure CS is high (inactive) during reset
    gpio_pin_set_dt(&lcd_cs, 1);

    // Reset sequence: HIGH -> LOW -> HIGH
    gpio_pin_set_dt(&lcd_rst, 1);
    k_msleep(10);
    gpio_pin_set_dt(&lcd_rst, 0);  // Assert reset (active low)
    k_msleep(20);
    gpio_pin_set_dt(&lcd_rst, 1);  // Release reset
    k_msleep(150);
}

static void set_address_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    write_command(ILI9341_CASET);  // Column address set
    write_data16(x0);
    write_data16(x1);

    write_command(ILI9341_PASET);  // Page address set
    write_data16(y0);
    write_data16(y1);

    write_command(ILI9341_RAMWR);  // Write to RAM
}

/* Public API functions */
int ili9341_init(void)
{
    int ret;

    LOG_INF("Initializing ILI9341 TFT LCD...");

    /* Configure data pins as outputs */
    for (int i = 0; i < 8; i++) {
        if (!device_is_ready(data_pins[i].port)) {
            LOG_ERR("Data pin %d GPIO device not ready", i);
            return -ENODEV;
        }
        ret = gpio_pin_configure_dt(&data_pins[i], GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            LOG_ERR("Failed to configure data pin %d", i);
            return ret;
        }
        LOG_DBG("Configured D%d on P%d.%02d", i, data_pins[i].port->name[4] - '0', data_pins[i].pin);
    }

    /* Configure control pins as outputs */
    if (!device_is_ready(lcd_rst.port) || !device_is_ready(lcd_cs.port) ||
        !device_is_ready(lcd_rs.port) || !device_is_ready(lcd_wr.port) ||
        !device_is_ready(lcd_rd.port)) {
        LOG_ERR("Control GPIO device not ready");
        return -ENODEV;
    }

    gpio_pin_configure_dt(&lcd_rst, GPIO_OUTPUT_INACTIVE);  // RST initially low
    gpio_pin_configure_dt(&lcd_cs, GPIO_OUTPUT_ACTIVE);     // CS initially high (inactive)
    gpio_pin_configure_dt(&lcd_rs, GPIO_OUTPUT_INACTIVE);    // RS low
    gpio_pin_configure_dt(&lcd_wr, GPIO_OUTPUT_ACTIVE);      // WR high (inactive)
    gpio_pin_configure_dt(&lcd_rd, GPIO_OUTPUT_ACTIVE);      // RD high (inactive)

    LOG_INF("Control pins configured - RST:P0.25 CS:P0.07 RS:P0.06 WR:P0.05 RD:P0.04");

    /* Hardware reset */
    LOG_INF("Performing hardware reset...");
    hardware_reset();
    LOG_INF("Hardware reset complete");

    LOG_INF("Sending initialization commands...");

    /* Software reset */
    write_command(ILI9341_SWRESET);
    k_msleep(200);

    /* Exit sleep mode first */
    write_command(ILI9341_SLPOUT);
    k_msleep(200);

    /* Power control A */
    write_command(0xCB);
    write_data(0x39);
    write_data(0x2C);
    write_data(0x00);
    write_data(0x34);
    write_data(0x02);

    /* Power control B */
    write_command(0xCF);
    write_data(0x00);
    write_data(0xC1);
    write_data(0x30);

    /* Driver timing control A */
    write_command(0xE8);
    write_data(0x85);
    write_data(0x00);
    write_data(0x78);

    /* Driver timing control B */
    write_command(0xEA);
    write_data(0x00);
    write_data(0x00);

    /* Power on sequence control */
    write_command(0xED);
    write_data(0x64);
    write_data(0x03);
    write_data(0x12);
    write_data(0x81);

    /* Pump ratio control */
    write_command(0xF7);
    write_data(0x20);

    /* Power control 1 */
    write_command(ILI9341_PWCTR1);
    write_data(0x23);

    /* Power control 2 */
    write_command(ILI9341_PWCTR2);
    write_data(0x10);

    /* VCOM control 1 */
    write_command(ILI9341_VMCTR1);
    write_data(0x3e);
    write_data(0x28);

    /* VCOM control 2 */
    write_command(ILI9341_VMCTR2);
    write_data(0x86);

    /* Memory access control - BGR color filter */
    write_command(ILI9341_MADCTL);
    write_data(0x48);  // MY=0, MX=1, MV=0, ML=0, BGR=1, MH=0

    /* Pixel format - 16 bit */
    write_command(ILI9341_PIXFMT);
    write_data(0x55);  // 16-bit color

    /* Frame rate control */
    write_command(ILI9341_FRMCTR1);
    write_data(0x00);
    write_data(0x18);

    /* Display function control */
    write_command(ILI9341_DFUNCTR);
    write_data(0x08);
    write_data(0x82);
    write_data(0x27);

    /* Enable 3G (3 gamma control) */
    write_command(0xF2);
    write_data(0x00);

    /* Positive gamma correction */
    write_command(ILI9341_GMCTRP1);
    write_data(0x0F);
    write_data(0x31);
    write_data(0x2B);
    write_data(0x0C);
    write_data(0x0E);
    write_data(0x08);
    write_data(0x4E);
    write_data(0xF1);
    write_data(0x37);
    write_data(0x07);
    write_data(0x10);
    write_data(0x03);
    write_data(0x0E);
    write_data(0x09);
    write_data(0x00);

    /* Negative gamma correction */
    write_command(ILI9341_GMCTRN1);
    write_data(0x00);
    write_data(0x0E);
    write_data(0x14);
    write_data(0x03);
    write_data(0x11);
    write_data(0x07);
    write_data(0x31);
    write_data(0xC1);
    write_data(0x48);
    write_data(0x08);
    write_data(0x0F);
    write_data(0x0C);
    write_data(0x31);
    write_data(0x36);
    write_data(0x0F);

    /* Normal display on */
    write_command(ILI9341_NORON);
    k_msleep(10);

    /* Display on */
    write_command(ILI9341_DISPON);
    k_msleep(100);

    LOG_INF("ILI9341 initialization complete");

    /* Test basic communication by filling a small area */
    LOG_INF("Testing display with small white square...");
    ili9341_fill_rect(0, 0, 50, 50, COLOR_WHITE);
    LOG_INF("Test pattern sent");

    return 0;
}

void ili9341_set_rotation(uint8_t rotation)
{
    uint8_t madctl = 0;

    rotation = rotation % 4;

    switch (rotation) {
    case 0:
        madctl = 0x48;  // MY=0, MX=1, MV=0, ML=0, BGR=1
        screen_width = ILI9341_WIDTH;
        screen_height = ILI9341_HEIGHT;
        break;
    case 1:
        madctl = 0x28;  // MY=0, MX=0, MV=1, ML=0, BGR=1
        screen_width = ILI9341_HEIGHT;
        screen_height = ILI9341_WIDTH;
        break;
    case 2:
        madctl = 0x88;  // MY=1, MX=0, MV=0, ML=0, BGR=1
        screen_width = ILI9341_WIDTH;
        screen_height = ILI9341_HEIGHT;
        break;
    case 3:
        madctl = 0xE8;  // MY=1, MX=1, MV=1, ML=0, BGR=1
        screen_width = ILI9341_HEIGHT;
        screen_height = ILI9341_WIDTH;
        break;
    }

    write_command(ILI9341_MADCTL);
    write_data(madctl);
}

void ili9341_fill_screen(uint16_t color)
{
    ili9341_fill_rect(0, 0, screen_width, screen_height, color);
}

void ili9341_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= screen_width || y >= screen_height) {
        return;
    }

    set_address_window(x, y, x, y);
    write_data16(color);
}

void ili9341_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= screen_width || y >= screen_height) {
        return;
    }

    if (x + w > screen_width) {
        w = screen_width - x;
    }
    if (y + h > screen_height) {
        h = screen_height - y;
    }

    set_address_window(x, y, x + w - 1, y + h - 1);

    gpio_pin_set_dt(&lcd_cs, 0);
    gpio_pin_set_dt(&lcd_rs, 1);  // Data mode

    for (uint32_t i = 0; i < (uint32_t)w * h; i++) {
        write_data_bus(color >> 8);
        pulse_wr();
        write_data_bus(color & 0xFF);
        pulse_wr();
    }

    gpio_pin_set_dt(&lcd_cs, 1);
}

void ili9341_draw_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    ili9341_fill_rect(x, y, w, 1, color);
}

void ili9341_draw_vline(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{
    ili9341_fill_rect(x, y, 1, h, color);
}

void ili9341_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    ili9341_draw_hline(x, y, w, color);
    ili9341_draw_hline(x, y + h - 1, w, color);
    ili9341_draw_vline(x, y, h, color);
    ili9341_draw_vline(x + w - 1, y, h, color);
}

void ili9341_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, uint8_t size)
{
    if (c < 32 || c > 126) {
        c = 32;  // Replace invalid chars with space
    }

    const uint8_t *glyph = font_8x8[c - 32];

    // Optimized version using block write
    uint16_t char_width = FONT_WIDTH * size;
    uint16_t char_height = FONT_HEIGHT * size;

    if (x + char_width > screen_width || y + char_height > screen_height) {
        return;  // Character would be off screen
    }

    set_address_window(x, y, x + char_width - 1, y + char_height - 1);

    gpio_pin_set_dt(&lcd_rs, 1);  // Data mode
    gpio_pin_set_dt(&lcd_cs, 0);  // CS active

    for (uint8_t row = 0; row < FONT_HEIGHT; row++) {
        uint8_t line = glyph[row];

        // Repeat each row 'size' times for scaling
        for (uint8_t sy = 0; sy < size; sy++) {
            uint8_t bit_line = line;

            for (uint8_t col = 0; col < FONT_WIDTH; col++) {
                uint16_t pixel_color = (bit_line & 0x01) ? color : bg;

                // Repeat each pixel 'size' times horizontally
                for (uint8_t sx = 0; sx < size; sx++) {
                    write_data_bus(pixel_color >> 8);
                    pulse_wr();
                    write_data_bus(pixel_color & 0xFF);
                    pulse_wr();
                }

                bit_line >>= 1;
            }
        }
    }

    gpio_pin_set_dt(&lcd_cs, 1);  // CS inactive
}

void ili9341_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size)
{
    uint16_t cursor_x = x;
    uint16_t cursor_y = y;
    size_t len = strlen(str);

    for (size_t i = 0; i < len; i++) {
        if (str[i] == '\n') {
            // Newline
            cursor_x = x;
            cursor_y += FONT_HEIGHT * size;
        } else if (str[i] == '\r') {
            // Carriage return
            cursor_x = x;
        } else {
            // Draw character
            ili9341_draw_char(cursor_x, cursor_y, str[i], color, bg, size);
            cursor_x += FONT_WIDTH * size;

            // Wrap to next line if needed
            if (cursor_x + FONT_WIDTH * size > screen_width) {
                cursor_x = x;
                cursor_y += FONT_HEIGHT * size;
            }
        }
    }
}
