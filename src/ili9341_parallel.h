/*
 * ILI9341 TFT LCD Driver - Parallel 8-bit Interface
 * For MCUFriend 2.4" TFT LCD Shield on nRF52-DK
 */

#ifndef ILI9341_PARALLEL_H
#define ILI9341_PARALLEL_H

#include <zephyr/kernel.h>
#include <stdint.h>

/* ILI9341 Commands */
#define ILI9341_NOP         0x00
#define ILI9341_SWRESET     0x01
#define ILI9341_RDDID       0x04
#define ILI9341_RDDST       0x09
#define ILI9341_SLPIN       0x10
#define ILI9341_SLPOUT      0x11
#define ILI9341_PTLON       0x12
#define ILI9341_NORON       0x13
#define ILI9341_INVOFF      0x20
#define ILI9341_INVON       0x21
#define ILI9341_DISPOFF     0x28
#define ILI9341_DISPON      0x29
#define ILI9341_CASET       0x2A
#define ILI9341_PASET       0x2B
#define ILI9341_RAMWR       0x2C
#define ILI9341_RAMRD       0x2E
#define ILI9341_MADCTL      0x36
#define ILI9341_PIXFMT      0x3A
#define ILI9341_FRMCTR1     0xB1
#define ILI9341_FRMCTR2     0xB2
#define ILI9341_FRMCTR3     0xB3
#define ILI9341_INVCTR      0xB4
#define ILI9341_DFUNCTR     0xB6
#define ILI9341_PWCTR1      0xC0
#define ILI9341_PWCTR2      0xC1
#define ILI9341_PWCTR3      0xC2
#define ILI9341_PWCTR4      0xC3
#define ILI9341_PWCTR5      0xC4
#define ILI9341_VMCTR1      0xC5
#define ILI9341_VMCTR2      0xC7
#define ILI9341_GMCTRP1     0xE0
#define ILI9341_GMCTRN1     0xE1

/* Screen dimensions */
#define ILI9341_WIDTH       240
#define ILI9341_HEIGHT      320

/* Color definitions (RGB565) */
#define COLOR_BLACK         0x0000
#define COLOR_WHITE         0xFFFF
#define COLOR_RED           0xF800
#define COLOR_GREEN         0x07E0
#define COLOR_BLUE          0x001F
#define COLOR_YELLOW        0xFFE0
#define COLOR_CYAN          0x07FF
#define COLOR_MAGENTA       0xF81F

/* Function prototypes */
int ili9341_init(void);
void ili9341_set_rotation(uint8_t rotation);
void ili9341_fill_screen(uint16_t color);
void ili9341_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
void ili9341_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ili9341_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ili9341_draw_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void ili9341_draw_vline(uint16_t x, uint16_t y, uint16_t h, uint16_t color);
void ili9341_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, uint8_t size);
void ili9341_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size);

#endif /* ILI9341_PARALLEL_H */
