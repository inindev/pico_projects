// Generate DVI output for 640x480x4bpp using HSTX

#include <string.h>
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/vreg.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"


// clock configuration
#define clockspeed 315000
#define clockdivisor 2

// dvi serialiser configuration
#define N_TMDS_LANES 3
struct dvi_serialiser_cfg {
    uint sm_tmds[N_TMDS_LANES];
    uint pins_tmds[N_TMDS_LANES];
    uint pins_clk;
    bool invert_diffpairs;
};

// board configurations
static const struct dvi_serialiser_cfg pico_sock_cfg = {
    .sm_tmds = {0, 1, 2},
    .pins_tmds = {12, 18, 16}, // blue (d0), Red (d1), Green (d2)
    .pins_clk = 14,            // clock
    .invert_diffpairs = true
};

// select board configuration
#define DVI_CFG pico_sock_cfg

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

// framebuffer (640x480x4bpp = 153,600 bytes)
#define ALIGNED __attribute__((aligned(4)))
#define MODE3SIZE (MODE_H_ACTIVE_PIXELS * MODE_V_ACTIVE_LINES / 2)
uint8_t ALIGNED FRAMEBUFFER[MODE3SIZE];
uint16_t ALIGNED HDMIlines[2][MODE_H_ACTIVE_PIXELS] = {0};
static bool hdmi_enable = false;

// hstx command types
#define HSTX_CMD_RAW_REPEAT (0x1u << 12)
#define HSTX_CMD_TMDS (0x2u << 12)
#define HSTX_CMD_NOP (0xfu << 12)

// color map for 16 colors
const uint32_t MAP16DEF[16] = {
    0x000080, // navy blue
    0xFF0000, // full red
    0x00FF00, // full green
    0x0000FF, // full blue
    0x00FFFF, // full cyan
    0xFF00FF, // full magenta
    0xFFFF00, // full yellow
    0xFFFFFF, // white
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
static bool dma_pong = false;
volatile uint v_scanline = 2;
static bool vactive_cmdlist_posted = false;
volatile uint vblank = 0;

void __not_in_flash_func(dma_irq_handler)() {
    uint ch_num = dma_pong ? DMACH_PONG : DMACH_PING;
    dma_channel_hw_t *ch = &dma_hw->ch[ch_num];
    dma_hw->intr = 1u << ch_num;
    dma_pong = !dma_pong;
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

    hstx_ctrl_hw->expand_shift =
        2 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
        16 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
        1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
        0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        5u << HSTX_CTRL_CSR_CLKDIV_LSB |
        5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
        2u << HSTX_CTRL_CSR_SHIFT_LSB |
        HSTX_CTRL_CSR_EN_BITS;

    // assign clock and data pins for hstx using DVI_CFG
    // find minimum gpio pin to calculate base offset
    uint min_pin = DVI_CFG.pins_clk;
    for (uint lane = 0; lane < N_TMDS_LANES; ++lane) {
        if (DVI_CFG.pins_tmds[lane] < min_pin) min_pin = DVI_CFG.pins_tmds[lane];
    }

    // clock: assign to hstx bit indices based on gpio pins
    hstx_ctrl_hw->bit[DVI_CFG.pins_clk - min_pin] = HSTX_CTRL_BIT0_CLK_BITS; // positive
    gpio_set_function(DVI_CFG.pins_clk, 0); // clock positive
    if (!DVI_CFG.invert_diffpairs) {
        hstx_ctrl_hw->bit[DVI_CFG.pins_clk - min_pin + 1] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS; // negative
        gpio_set_function(DVI_CFG.pins_clk + 1, 0); // clock negative
    }

    // data lanes: assign to hstx bit indices based on gpio pins
    for (uint lane = 0; lane < N_TMDS_LANES; ++lane) {
        uint bit = DVI_CFG.pins_tmds[lane] - min_pin;
        uint32_t lane_data_sel_bits =
            (lane * 10) << HSTX_CTRL_BIT0_SEL_P_LSB |
            (lane * 10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB;
        hstx_ctrl_hw->bit[bit] = lane_data_sel_bits; // positive
        gpio_set_function(DVI_CFG.pins_tmds[lane], 0); // positive
        if (!DVI_CFG.invert_diffpairs) {
            hstx_ctrl_hw->bit[bit + 1] = lane_data_sel_bits | HSTX_CTRL_BIT0_INV_BITS; // negative
            gpio_set_function(DVI_CFG.pins_tmds[lane] + 1, 0); // negative
        }
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
    for(;;) {
        if (v_scanline != last_line) {
            last_line = v_scanline;
            load_line = v_scanline - (MODE_V_TOTAL_LINES - MODE_V_ACTIVE_LINES);
            line_to_load = last_line & 1;
            uint16_t *p = HDMIlines[line_to_load];
            if (load_line >= 0 && load_line < MODE_V_ACTIVE_LINES && hdmi_enable) {
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
    unsigned char color = c & 0x0F; // extract 4-bit color index
    unsigned char bcolor = (color << 4) | color;

    if (x1 < 0) x1 = 0;
    if (x1 >= MODE_H_ACTIVE_PIXELS) x1 = MODE_H_ACTIVE_PIXELS - 1;
    if (x2 < 0) x2 = 0;
    if (x2 >= MODE_H_ACTIVE_PIXELS) x2 = MODE_H_ACTIVE_PIXELS - 1;
    if (y1 < 0) y1 = 0;
    if (y1 >= MODE_V_ACTIVE_LINES) y1 = MODE_V_ACTIVE_LINES - 1;
    if (y2 < 0) y2 = 0;
    if (y2 >= MODE_V_ACTIVE_LINES) y2 = MODE_V_ACTIVE_LINES - 1;
    if (x2 <= x1) { t = x1; x1 = x2; x2 = t; }
    if (y2 <= y1) { t = y1; y1 = y2; y2 = t; }

    for (y = y1; y <= y2; y++) {
        x1p = x1;
        x2p = x2;
        uint8_t *p = FRAMEBUFFER + (y * (MODE_H_ACTIVE_PIXELS >> 1)) + (x1 >> 1);
        if ((x1 % 2) == 1) {
            *p &= 0x0F;
            *p |= (color << 4);
            p++;
            x1p++;
        }
        if ((x2 % 2) == 0) {
            uint8_t *q = FRAMEBUFFER + (y * (MODE_H_ACTIVE_PIXELS >> 1)) + (x2 >> 1);
            *q &= 0xF0;
            *q |= color;
            x2p--;
        }
        for (x = x1p; x < x2p; x += 2) {
            *p++ = bcolor;
        }
    }
}

int main(void) {
    // configure system voltage and clock
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    set_sys_clock_khz(clockspeed, false);
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, clockspeed * 1000, clockspeed * 1000);
    clock_configure(clk_hstx, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, clockspeed * 1000, clockspeed / clockdivisor * 1000);

    hdmi_enable = false;

    // fill framebuffer with navy blue (index 0)
    memset(FRAMEBUFFER, 0, MODE3SIZE);

    // draw 4x4 grid of 160x120 boxes, each with a unique color index (0-15)
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            DrawRectangle16(x * 160, y * 120, x * 160 + 159, y * 120 + 119, y * 4 + x);
        }
    }

    // start hdmi output
    hdmi_enable = true;
    multicore_launch_core1_with_stack(HDMICore, core1stack, 512);

    // infinite loop
    for(;;);
}
