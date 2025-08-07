// Generate DVI output for 640x480x4bpp using HSTX

#include <stdio.h>
#include <string.h>
#include "font_8x14.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/uart.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

#define ALIGNED __attribute__((aligned(4)))

// dvi constants
#define TMDS_CTRL_00 0x354u
#define TMDS_CTRL_01 0x0abu
#define TMDS_CTRL_10 0x154u
#define TMDS_CTRL_11 0x2abu
#define SYNC_V0_H0 (TMDS_CTRL_00 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V0_H1 (TMDS_CTRL_01 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H0 (TMDS_CTRL_10 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H1 (TMDS_CTRL_11 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))

// timing parameters for 640x480
#define MODE_H_ACTIVE_PIXELS 640
#define MODE_H_FRONT_PORCH 16
#define MODE_H_SYNC_WIDTH 64
#define MODE_H_BACK_PORCH 120
#define MODE_V_ACTIVE_LINES 480
#define MODE_V_FRONT_PORCH 1
#define MODE_V_SYNC_WIDTH 3
#define MODE_V_BACK_PORCH 16
#define MODE_H_TOTAL_PIXELS (MODE_H_FRONT_PORCH + MODE_H_SYNC_WIDTH + MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS)
#define MODE_V_TOTAL_LINES (MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH + MODE_V_ACTIVE_LINES)

// clock configuration
#define clockspeed 315000
#define clockdivisor 2

// framebuffer (640x480x4bpp = 153,600 bytes)
#define MODE3SIZE (MODE_H_ACTIVE_PIXELS * MODE_V_ACTIVE_LINES / 2)
uint8_t ALIGNED FRAMEBUFFER[MODE3SIZE];
uint16_t ALIGNED HDMIlines[2][MODE_H_ACTIVE_PIXELS] = {0};
uint8_t *WriteBuf = FRAMEBUFFER;
volatile int HRes;
volatile int VRes;
volatile int HDMImode = 0;

// hstx command types
#define HSTX_CMD_RAW         (0x0u << 12)
#define HSTX_CMD_RAW_REPEAT  (0x1u << 12)
#define HSTX_CMD_TMDS        (0x2u << 12)
#define HSTX_CMD_TMDS_REPEAT (0x3u << 12)
#define HSTX_CMD_NOP         (0xfu << 12)

// screen mode
#define SCREENMODE3 28

// color map for 16 colors
const uint32_t MAP16DEF[16] = {
    0x000080, // navy blue (index 0)
    0xFF0000, // full red
    0x00FF00, // full green
    0x0000FF, // full blue
    0x00FFFF, // full cyan
    0xFF00FF, // full magenta
    0xFFFF00, // full yellow
    0xFFFFFF, // white (index 7)
    0x000000, // black
    0x7F0000, // mid red
    0x007F00, // mid green
    0x00007F, // mid blue
    0x007F7F, // mid cyan
    0x7F007F, // mid magenta
    0x7F7F00, // mid yellow
    0x7F7F7F  // gray
};
uint16_t map16[16];

// convert rgb to rgb555
uint16_t RGB555(uint32_t c) {
    return ((c & 0xf8) >> 3) | ((c & 0xf800) >> 6) | ((c & 0xf80000) >> 9);
}

// hstx command lists
static uint32_t vblank_line_vsync_off[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH, SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH, SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS), SYNC_V1_H1,
    HSTX_CMD_NOP
};
static uint32_t vblank_line_vsync_on[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH, SYNC_V0_H1,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH, SYNC_V0_H0,
    HSTX_CMD_RAW_REPEAT | (MODE_H_BACK_PORCH + MODE_H_ACTIVE_PIXELS), SYNC_V0_H1,
    HSTX_CMD_NOP
};
static uint32_t vactive_line[] = {
    HSTX_CMD_RAW_REPEAT | MODE_H_FRONT_PORCH, SYNC_V1_H1,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_H_SYNC_WIDTH, SYNC_V1_H0,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | MODE_H_BACK_PORCH, SYNC_V1_H1,
    HSTX_CMD_TMDS | MODE_H_ACTIVE_PIXELS
};

// dma logic
#define DMACH_PING 0
#define DMACH_PONG 1

// cursor position
static int cursor_x = 0;
static int cursor_y = 0;

// track which dma channel is active (ping or pong)
static bool dma_pong = false;

// track current scanline, starting at 2 (third scanline, zero-based)
volatile uint v_scanline = 2;

// flag to track if command list is posted during active period
static bool vactive_cmdlist_posted = false;
volatile uint vblank = 0;

// dma interrupt handler to manage scanline data
void __not_in_flash_func(dma_irq_handler)() {
    // determine which channel just finished
    uint ch_num = dma_pong ? DMACH_PONG : DMACH_PING;
    dma_channel_hw_t *ch = &dma_hw->ch[ch_num];
    dma_hw->intr = 1u << ch_num;
    dma_pong = !dma_pong;

    // configure next transfer based on scanline
    if (v_scanline >= MODE_V_FRONT_PORCH && v_scanline < (MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH)) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_on;
        ch->transfer_count = count_of(vblank_line_vsync_on);
        vblank = 1;
    } else if (v_scanline < MODE_V_FRONT_PORCH + MODE_V_SYNC_WIDTH + MODE_V_BACK_PORCH) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_off;
        ch->transfer_count = count_of(vblank_line_vsync_off);
        vblank = 1;
    } else if (!vactive_cmdlist_posted) {
        ch->read_addr = (uintptr_t)vactive_line;
        ch->transfer_count = count_of(vactive_line);
        vactive_cmdlist_posted = true;
        vblank = 0;
    } else {
        ch->read_addr = (uintptr_t)HDMIlines[v_scanline & 1];
        ch->transfer_count = MODE_H_ACTIVE_PIXELS / 2;
        vactive_cmdlist_posted = false;
    }

    // increment scanline if command list was posted
    if (!vactive_cmdlist_posted) {
        v_scanline = (v_scanline + 1) % MODE_V_TOTAL_LINES;
    }
}

// core1 program for hdmi output
uint32_t core1stack[128];
void __not_in_flash_func(HDMICore)(void) {
    int last_line = 2, load_line, line_to_load;

    // initialize color map
    for (int i = 0; i < 16; i++) map16[i] = RGB555(MAP16DEF[i]);

    // configure hstx tmds encoder for rgb555
    hstx_ctrl_hw->expand_tmds =
        29 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB |
        4 << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |
        2 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB |
        4 << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |
        7 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB |
        4 << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB;

    // configure pixel and control symbol shifting
    hstx_ctrl_hw->expand_shift =
        2 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
        16 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
        1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
        0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

    // configure serial output
    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        5u << HSTX_CTRL_CSR_CLKDIV_LSB |
        5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
        2u << HSTX_CTRL_CSR_SHIFT_LSB |
        HSTX_CTRL_CSR_EN_BITS;

    // assign clock and data pins for hstx
    hstx_ctrl_hw->bit[2] = HSTX_CTRL_BIT0_CLK_BITS;
    hstx_ctrl_hw->bit[3] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;
    for (uint lane = 0; lane < 3; ++lane) {
        static const int lane_to_output_bit[3] = {0, 6, 4};
        int bit = lane_to_output_bit[lane];
        uint32_t lane_data_sel_bits =
            (lane * 10) << HSTX_CTRL_BIT0_SEL_P_LSB |
            (lane * 10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB;
        hstx_ctrl_hw->bit[bit] = lane_data_sel_bits;
        hstx_ctrl_hw->bit[bit + 1] = lane_data_sel_bits | HSTX_CTRL_BIT0_INV_BITS;
    }

    // set gpio pins 12-19 for hstx
    for (int i = 12; i <= 19; ++i) {
        gpio_set_function(i, 0);
    }

    // configure dma channels
    dma_channel_config c;
    c = dma_channel_get_default_config(DMACH_PING);
    channel_config_set_chain_to(&c, DMACH_PONG);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(DMACH_PING, &c, &hstx_fifo_hw->fifo, vblank_line_vsync_off, count_of(vblank_line_vsync_off), false);
    c = dma_channel_get_default_config(DMACH_PONG);
    channel_config_set_chain_to(&c, DMACH_PING);
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(DMACH_PONG, &c, &hstx_fifo_hw->fifo, vblank_line_vsync_off, count_of(vblank_line_vsync_off), false);

    // enable dma interrupts
    dma_hw->ints0 = (1u << DMACH_PING) | (1u << DMACH_PONG);
    dma_hw->inte0 = (1u << DMACH_PING) | (1u << DMACH_PONG);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);
    bus_ctrl_hw->priority = 1;
    dma_channel_start(DMACH_PING);

    // process scanlines
    while (1) {
        if (v_scanline != last_line) {
            last_line = v_scanline;
            load_line = v_scanline - (MODE_V_TOTAL_LINES - MODE_V_ACTIVE_LINES);
            line_to_load = last_line & 1;
            uint16_t *p = HDMIlines[line_to_load];
            if (load_line >= 0 && load_line < MODE_V_ACTIVE_LINES && HDMImode == SCREENMODE3) {
                __dmb();
                for (int i = 0; i < MODE_H_ACTIVE_PIXELS / 2; i++) {
                    int d = FRAMEBUFFER[load_line * MODE_H_ACTIVE_PIXELS / 2 + i];
                    *p++ = map16[d & 0xf];
                    d >>= 4;
                    *p++ = map16[d & 0xf];
                }
            }
        }
    }
}

void DrawRectangle16(int x1, int y1, int x2, int y2, int c) {
    int x, y, x1p, x2p, t;
    unsigned char color = c & 0x0F; // Extract 4-bit color index
    unsigned char bcolor = (color << 4) | color;
    if (x1 < 0) x1 = 0;
    if (x1 >= HRes) x1 = HRes - 1;
    if (x2 < 0) x2 = 0;
    if (x2 >= HRes) x2 = HRes - 1;
    if (y1 < 0) y1 = 0;
    if (y1 >= VRes) y1 = VRes - 1;
    if (y2 < 0) y2 = 0;
    if (y2 >= VRes) y2 = VRes - 1;
    if (x2 <= x1) { t = x1; x1 = x2; x2 = t; }
    if (y2 <= y1) { t = y1; y1 = y2; y2 = t; }
    for (y = y1; y <= y2; y++) {
        x1p = x1;
        x2p = x2;
        uint8_t *p = WriteBuf + (y * (HRes >> 1)) + (x1 >> 1);
        if ((x1 % 2) == 1) {
            *p &= 0x0F;
            *p |= (color << 4);
            p++;
            x1p++;
        }
        if ((x2 % 2) == 0) {
            uint8_t *q = WriteBuf + (y * (HRes >> 1)) + (x2 >> 1);
            *q &= 0xF0;
            *q |= color;
            x2p--;
        }
        for (x = x1p; x < x2p; x += 2) {
            *p++ = bcolor;
        }
    }
}

// draw a character in 16-color mode
void DrawChar16(int x, int y, char c, int fg_color, int bg_color) {
    unsigned char *font_data = font_8x14[(unsigned char)c];
    int i, j;
    unsigned char color;

    // use 4-bit color indices directly
    unsigned char fg = fg_color & 0x0F;
    unsigned char bg = bg_color & 0x0F;

    // iterate over 14 rows of the character
    for (i = 0; i < 14; i++) {
        if (y + i < 0 || y + i >= VRes) continue; // skip if out of bounds
        uint8_t *p = WriteBuf + ((y + i) * (HRes >> 1)) + (x >> 1);
        unsigned char row = font_data[i];

        // handle partial first byte if x is odd
        if (x & 1) {
            *p &= 0x0F; // clear high nibble
            *p |= (row & 0x80 ? fg : bg) << 4; // set high nibble
            p++;
            row <<= 1;
        }

        // draw full bytes (two pixels per byte)
        for (j = x & 1; j < 8 && x + j < HRes; j += 2) {
            color = ((row & 0x80) ? fg : bg) | (((row & 0x40) ? fg : bg) << 4);
            if (x + j < HRes - 1) *p++ = color;
            row <<= 2;
        }

        // handle partial last byte if needed
        if (!(x & 1) && (x + j - 1 < HRes) && j < 8) {
            *p &= 0xF0; // clear low nibble
            *p |= (row & 0x80) ? fg : bg; // set low nibble
        }
    }
}

// draw a string in 16-color mode
void DrawString16(int x, int y, const char *str, int fg_color, int bg_color) {
    int x_pos = x;
    while (*str) {
        if (x_pos + 8 <= HRes) { // ensure character fits within horizontal resolution
            DrawChar16(x_pos, y, *str, fg_color, bg_color);
        }
        x_pos += 8; // advance by character width
        str++;
    }
}

void display_char(char c) {
    // handle special characters
    if (c == '\n' || c == '\r') {
        cursor_x = 0;
        cursor_y += 14; // move to next line
        if (cursor_y >= VRes) {
            cursor_y = 0; // wrap to top of screen
            memset(WriteBuf, 0, MODE3SIZE); // clear screen
        }
        return;
    }
    
    // display printable character
    if (cursor_x + 8 <= HRes) {
        DrawChar16(cursor_x, cursor_y, c, 7, 0); // white (7) on navy (0)
        cursor_x += 8; // advance cursor
    }
    
    // wrap to next line if end of line reached
    if (cursor_x + 8 > HRes) {
        cursor_x = 0;
        cursor_y += 14;
        if (cursor_y >= VRes) {
            cursor_y = 0; // wrap to top
            memset(WriteBuf, 0, MODE3SIZE); // clear screen
        }
    }
}

int main(void) {
    // configure system voltage and clock
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    set_sys_clock_khz(clockspeed, false);
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, clockspeed * 1000, clockspeed * 1000);
    clock_configure(clk_hstx, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, clockspeed * 1000, clockspeed / clockdivisor * 1000);

    HRes = MODE_H_ACTIVE_PIXELS;
    VRes = MODE_V_ACTIVE_LINES;
    HDMImode = 0;

    // fill framebuffer with navy blue (index 0)
    memset(FRAMEBUFFER, 0, MODE3SIZE);

    // start hdmi output
    HDMImode = SCREENMODE3;
    multicore_launch_core1_with_stack(HDMICore, core1stack, 512);

    // infinite loop
    for(;;);
}
