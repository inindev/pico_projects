/*
 * HAGL HAL implementation for ILI9488 on RP2350
 */

#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hagl_hal.h"
#include "hagl.h"
#include "hagl/bitmap.h"

/* Pin definitions */
#define SPI_INST   spi1
#define PIN_MOSI   11  // header pin 19
#define PIN_SCK    10  // header pin 23
#define PIN_CS     13  // header pin 33
#define PIN_DC     14  // header pin 7
#define PIN_RST    15  // header pin 29
#define PIN_BL     12  // header pin 21

/* Low-level SPI functions */
static void ili9488_send_cmd(uint8_t cmd) {
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 0);
    spi_write_blocking(SPI_INST, &cmd, 1);
    gpio_put(PIN_CS, 1);
}

static void ili9488_send_data(uint8_t *data, size_t len) {
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 1);
    spi_write_blocking(SPI_INST, data, len);
    gpio_put(PIN_CS, 1);
}

static void set_addr_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
    uint8_t data[4];

    ili9488_send_cmd(0x2A);
    data[0] = (x1 >> 8); data[1] = x1; data[2] = (x2 >> 8); data[3] = x2;
    ili9488_send_data(data, 4);

    ili9488_send_cmd(0x2B);
    data[0] = (y1 >> 8); data[1] = y1; data[2] = (y2 >> 8); data[3] = y2;
    ili9488_send_data(data, 4);

    ili9488_send_cmd(0x2C);
}

/* HAL function: put a single pixel */
static void hal_put_pixel(void *self, int16_t x, int16_t y, hagl_color_t color) {
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;

    set_addr_window(x, y, x, y);

    uint8_t rgb[3] = {
        (color >> 16) & 0xFF,  /* R */
        (color >> 8) & 0xFF,   /* G */
        color & 0xFF           /* B */
    };
    ili9488_send_data(rgb, 3);
}

/* HAL function: convert RGB to color */
static hagl_color_t hal_color(void *self, uint8_t r, uint8_t g, uint8_t b) {
    return ((hagl_color_t)(((uint32_t)r << 16) | ((uint32_t)g << 8) | b));
}

/* HAL function: draw horizontal line (optimized) */
static void hal_hline(void *self, int16_t x, int16_t y, uint16_t width, hagl_color_t color) {
    if (y < 0 || y >= DISPLAY_HEIGHT) return;
    if (x < 0) { width += x; x = 0; }
    if (x + width > DISPLAY_WIDTH) width = DISPLAY_WIDTH - x;
    if (width <= 0) return;

    set_addr_window(x, y, x + width - 1, y);

    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 1);
    for (uint16_t i = 0; i < width; i++) {
        spi_write_blocking(SPI_INST, &r, 1);
        spi_write_blocking(SPI_INST, &g, 1);
        spi_write_blocking(SPI_INST, &b, 1);
    }
    gpio_put(PIN_CS, 1);
}

/* HAL function: draw vertical line (optimized) */
static void hal_vline(void *self, int16_t x, int16_t y, uint16_t height, hagl_color_t color) {
    if (x < 0 || x >= DISPLAY_WIDTH) return;
    if (y < 0) { height += y; y = 0; }
    if (y + height > DISPLAY_HEIGHT) height = DISPLAY_HEIGHT - y;
    if (height <= 0) return;

    set_addr_window(x, y, x, y + height - 1);

    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 1);
    for (uint16_t i = 0; i < height; i++) {
        spi_write_blocking(SPI_INST, &r, 1);
        spi_write_blocking(SPI_INST, &g, 1);
        spi_write_blocking(SPI_INST, &b, 1);
    }
    gpio_put(PIN_CS, 1);
}

/* HAL function: blit with transparency (skip black pixels) */
static void hal_blit(void *self, int16_t x0, int16_t y0, hagl_bitmap_t *src) {
    hagl_color_t *ptr = (hagl_color_t *)src->buffer;

    for (uint16_t y = 0; y < src->height; y++) {
        for (uint16_t x = 0; x < src->width; x++) {
            hagl_color_t color = *(ptr++);
            if (color != 0) {
                hal_put_pixel(self, x0 + x, y0 + y, color);
            }
        }
    }
}

/* HAL function: flush (no-op for direct drawing) */
static size_t hal_flush(void *self) {
    return 0;
}

/*
 * Fast scaled framebuffer blit for 6502 emulator
 * Blits a 32x32 4-bit framebuffer scaled to 320x320 at (x0, y0)
 * palette is 16 RGB888 colors, fb is 1024 bytes (4-bit indices)
 */

// Line buffer for batched SPI writes (32 pixels * 10 scale * 3 bytes = 960 bytes)
static uint8_t line_buf[32 * 10 * 3];

void hagl_hal_blit_fb32(int16_t x0, int16_t y0, uint8_t scale,
                        const uint8_t *fb, const uint32_t *palette) {
    uint16_t fb_width = 32;
    uint16_t fb_height = 32;
    uint16_t scaled_w = fb_width * scale;
    uint16_t scaled_h = fb_height * scale;
    uint16_t line_bytes = scaled_w * 3;

    // Set address window once for entire blit area
    set_addr_window(x0, y0, x0 + scaled_w - 1, y0 + scaled_h - 1);

    // Stream all pixels - ILI9488 auto-increments address
    gpio_put(PIN_CS, 0);
    gpio_put(PIN_DC, 1);

    for (uint16_t y = 0; y < fb_height; y++) {
        // Build one scaled scanline into buffer
        uint8_t *p = line_buf;
        for (uint16_t x = 0; x < fb_width; x++) {
            uint8_t color_idx = fb[y * fb_width + x] & 0x0F;
            uint32_t rgb = palette[color_idx];
            uint8_t r = (rgb >> 16) & 0xFF;
            uint8_t g = (rgb >> 8) & 0xFF;
            uint8_t b = rgb & 0xFF;

            // Repeat pixel 'scale' times horizontally
            for (uint8_t sx = 0; sx < scale; sx++) {
                *p++ = r;
                *p++ = g;
                *p++ = b;
            }
        }

        // Send the same scanline 'scale' times (vertical scaling)
        for (uint8_t sy = 0; sy < scale; sy++) {
            spi_write_blocking(SPI_INST, line_buf, line_bytes);
        }
    }

    gpio_put(PIN_CS, 1);
}

/* HAL function: close */
static void hal_close(void *self) {
    /* Nothing to clean up */
}

/* Initialize display hardware */
static void init_display_hw(void) {
    spi_init(SPI_INST, 65 * 1000 * 1000);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);

    uint pins[] = {PIN_CS, PIN_DC, PIN_RST, PIN_BL};
    for (int i = 0; i < 4; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
    }

    gpio_put(PIN_BL, 1);
    gpio_put(PIN_CS, 1);

    gpio_put(PIN_RST, 0); sleep_ms(50);
    gpio_put(PIN_RST, 1); sleep_ms(50);

    ili9488_send_cmd(0x01); sleep_ms(120);

    uint8_t pix_fmt = 0x66;
    ili9488_send_cmd(0x3A); ili9488_send_data(&pix_fmt, 1);

    uint8_t madctl = 0x28;
    ili9488_send_cmd(0x36); ili9488_send_data(&madctl, 1);

    ili9488_send_cmd(0x11); sleep_ms(120);
    ili9488_send_cmd(0x29);
}

/* HAL init - called by HAGL's hagl_init() */
void hagl_hal_init(hagl_backend_t *backend) {
    init_display_hw();

    backend->width = DISPLAY_WIDTH;
    backend->height = DISPLAY_HEIGHT;
    backend->depth = DISPLAY_DEPTH;
    backend->put_pixel = hal_put_pixel;
    backend->color = hal_color;
    backend->hline = hal_hline;
    backend->vline = hal_vline;
    backend->blit = hal_blit;
    backend->flush = hal_flush;
    backend->close = hal_close;
}
