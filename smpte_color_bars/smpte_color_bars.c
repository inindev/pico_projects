// pico_projects/smpte_color_bars/smpte_color_bars.c
//
// generates smpte color bars on an hdmi display using the picodvi library and an adafruit
// hdmi sock for raspberry pi pico (rp2040). renders a test pattern with three rows of color
// bars using dual-core processing for efficient scanline generation.
// part of the pico projects repository.
// license: mit (see license file in repository root).

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/gpio.h"
#include "hardware/vreg.h"
#include "pico/sem.h"

#include "dvi.h"
#include "dvi_serialiser.h"
#include "tmds_encode.h"
#include "common_dvi_pin_configs.h"

// pick one:
#define MODE_640x480_60Hz
// #define MODE_800x480_60Hz
// #define MODE_800x600_60Hz
// #define MODE_960x540p_60Hz
// #define MODE_1280x720_30Hz

#if defined(MODE_640x480_60Hz)
#define FRAME_WIDTH 320
#define FRAME_HEIGHT 240
#define VREG_VSEL VREG_VOLTAGE_1_20
#define DVI_TIMING dvi_timing_640x480p_60hz

#elif defined(MODE_800x480_60Hz)
#define FRAME_WIDTH 400
#define FRAME_HEIGHT 240
#define VREG_VSEL VREG_VOLTAGE_1_20
#define DVI_TIMING dvi_timing_800x480p_60hz

#elif defined(MODE_800x600_60Hz)
#define FRAME_WIDTH 400
#define FRAME_HEIGHT 300
#define VREG_VSEL VREG_VOLTAGE_1_30
#define DVI_TIMING dvi_timing_800x600p_60hz

#elif defined(MODE_960x540p_60Hz)
#define FRAME_WIDTH 480
#define FRAME_HEIGHT 270
#define VREG_VSEL VREG_VOLTAGE_1_25
#define DVI_TIMING dvi_timing_960x540p_60hz

#elif defined(MODE_1280x720_30Hz)
#define FRAME_WIDTH 640
#define FRAME_HEIGHT 360
#define VREG_VSEL VREG_VOLTAGE_1_25
#define DVI_TIMING dvi_timing_1280x720p_30hz

#else
#error "Select a video mode!"
#endif

#define LED_PIN 21

// color definitions (rgb565 format)
#define COLOR_YELLOW        0xffe0  // ffff00
#define COLOR_CYAN          0x07ff  // 00ffff
#define COLOR_GREEN         0x07e0  // 00ff00
#define COLOR_MAGENTA       0xf81f  // ff00ff
#define COLOR_RED           0xf800  // ff0000
#define COLOR_BLUE          0x001f  // 0000ff
#define COLOR_DARK_TEAL     0x09eb  // 073e59
#define COLOR_DARK_PURPLE   0x380e  // 3a0073
#define COLOR_LIGHT_GRAY    0xce59  // cccccc
#define COLOR_MEDIUM_GRAY   0x2925  // 262626
#define COLOR_DARK_GRAY     0x10a2  // 131313
#define COLOR_BLACK         0x0000  // 000000
#define COLOR_WHITE         0xffff  // ffffff

// height macros
#define TOP_HEIGHT ((FRAME_HEIGHT * 2) / 3)
#define BOTTOM_HEIGHT (FRAME_HEIGHT / 4)
#define MIDDLE_HEIGHT (FRAME_HEIGHT - TOP_HEIGHT - BOTTOM_HEIGHT)
#define MIDDLE_START TOP_HEIGHT
#define BOTTOM_START (TOP_HEIGHT + MIDDLE_HEIGHT)

// render SD ECR-1-1978 color bars with three rows
void render_scanline(uint16_t *pixbuf, uint y) {
    if (y < MIDDLE_START) {
        // top row: 7 bars
        for (uint x = 0; x < FRAME_WIDTH; x++) {
            if (x < (FRAME_WIDTH / 7)) {
                pixbuf[x] = COLOR_LIGHT_GRAY;
            } else if (x < ((FRAME_WIDTH * 2) / 7)) {
                pixbuf[x] = COLOR_YELLOW;
            } else if (x < ((FRAME_WIDTH * 3) / 7)) {
                pixbuf[x] = COLOR_CYAN;
            } else if (x < ((FRAME_WIDTH * 4) / 7)) {
                pixbuf[x] = COLOR_GREEN;
            } else if (x < ((FRAME_WIDTH * 5) / 7)) {
                pixbuf[x] = COLOR_MAGENTA;
            } else if (x < ((FRAME_WIDTH * 6) / 7)) {
                pixbuf[x] = COLOR_RED;
            } else {
                pixbuf[x] = COLOR_BLUE;
            }
        }
    } else if (y < BOTTOM_START) {
        // middle row (skinny): 7 bars
        for (uint x = 0; x < FRAME_WIDTH; x++) {
            if (x < (FRAME_WIDTH / 7)) {
                pixbuf[x] = COLOR_BLUE;
            } else if (x < ((FRAME_WIDTH * 2) / 7)) {
                pixbuf[x] = COLOR_DARK_GRAY;
            } else if (x < ((FRAME_WIDTH * 3) / 7)) {
                pixbuf[x] = COLOR_MAGENTA;
            } else if (x < ((FRAME_WIDTH * 4) / 7)) {
                pixbuf[x] = COLOR_DARK_GRAY;
            } else if (x < ((FRAME_WIDTH * 5) / 7)) {
                pixbuf[x] = COLOR_CYAN;
            } else if (x < ((FRAME_WIDTH * 6) / 7)) {
                pixbuf[x] = COLOR_DARK_GRAY;
            } else {
                pixbuf[x] = COLOR_LIGHT_GRAY;
            }
        }
    } else {
        // bottom row: 8 bars with varying widths
        for (uint x = 0; x < FRAME_WIDTH; x++) {
            if (x < ((FRAME_WIDTH * 5) / 28)) {
                pixbuf[x] = COLOR_DARK_TEAL;
            } else if (x < ((FRAME_WIDTH * 10) / 28)) {
                pixbuf[x] = COLOR_WHITE;
            } else if (x < ((FRAME_WIDTH * 15) / 28)) {
                pixbuf[x] = COLOR_DARK_PURPLE;
            } else if (x < ((FRAME_WIDTH * 20) / 28)) {
                pixbuf[x] = COLOR_DARK_GRAY;
            } else if (x < ((FRAME_WIDTH * 16) / 21)) {
                pixbuf[x] = COLOR_BLACK;
            } else if (x < ((FRAME_WIDTH * 17) / 21)) {
                pixbuf[x] = COLOR_DARK_GRAY;
            } else if (x < ((FRAME_WIDTH * 18) / 21)) {
                pixbuf[x] = COLOR_MEDIUM_GRAY;
            } else {
                pixbuf[x] = COLOR_DARK_GRAY;
            }
        }
    }
}

// dvi setup & launch
struct dvi_inst dvi0;

uint16_t __scratch_y("render") __attribute__((aligned(4))) core0_scanbuf[FRAME_WIDTH];
uint16_t __scratch_x("render") __attribute__((aligned(4))) core1_scanbuf[FRAME_WIDTH];

void encode_scanline(uint16_t *pixbuf, uint32_t *tmdsbuf) {
    uint pixwidth = dvi0.timing->h_active_pixels;
    uint words_per_channel = pixwidth / DVI_SYMBOLS_PER_WORD;
    tmds_encode_data_channel_16bpp((uint32_t*)pixbuf, tmdsbuf + 0 * words_per_channel, pixwidth / 2, DVI_16BPP_BLUE_MSB, DVI_16BPP_BLUE_LSB);
    tmds_encode_data_channel_16bpp((uint32_t*)pixbuf, tmdsbuf + 1 * words_per_channel, pixwidth / 2, DVI_16BPP_GREEN_MSB, DVI_16BPP_GREEN_LSB);
    tmds_encode_data_channel_16bpp((uint32_t*)pixbuf, tmdsbuf + 2 * words_per_channel, pixwidth / 2, DVI_16BPP_RED_MSB, DVI_16BPP_RED_LSB);
}

void core1_main() {
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    while (queue_is_empty(&dvi0.q_tmds_valid))
        __wfe();
    dvi_start(&dvi0);
    while (1) {
        for (uint y = 1; y < FRAME_HEIGHT; y += 2) {
            render_scanline(core1_scanbuf, y);
            uint32_t *tmdsbuf = (uint32_t*)multicore_fifo_pop_blocking();
            encode_scanline(core1_scanbuf, tmdsbuf);
            multicore_fifo_push_blocking((uintptr_t)tmdsbuf);
        }
    }
}

int main() {
    vreg_set_voltage(VREG_VSEL);
    sleep_ms(10);
    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);

    setup_default_uart();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    printf("Configuring DVI\n");

    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    printf("Core 1 start\n");
    multicore_launch_core1(core1_main);

    printf("Start rendering\n");
    static uint heartbeat = 0;
    while (1) {
        for (uint y = 0; y < FRAME_HEIGHT; y += 2) {
            uint32_t *tmds0, *tmds1;
            queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmds0);
            queue_remove_blocking_u32(&dvi0.q_tmds_free, &tmds1);
            multicore_fifo_push_blocking((uintptr_t)tmds1);
            render_scanline(core0_scanbuf, y);
            encode_scanline(core0_scanbuf, tmds0);
            queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmds0);
            tmds1 = (uint32_t*)multicore_fifo_pop_blocking();
            queue_add_blocking_u32(&dvi0.q_tmds_valid, &tmds1);
        }
        if (++heartbeat >= 30) {
            heartbeat = 0;
            gpio_xor_mask(1u << LED_PIN);
        }
    }

    __builtin_unreachable();
}
