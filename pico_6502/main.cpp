//
//  6502 Emulator on Pico2
//
//  Copyright 2018-2026, John Clark
//
//  Released under the GNU General Public License
//  https://www.gnu.org/licenses/gpl.html
//
//  Demonstrates the W65C02S emulator running on RP2350.
//

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/structs/rosc.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "w65c02s.hpp"
#include "ram.hpp"
#include "hagl.h"
#include "hagl_hal.h"
#include "palette.h"
#include "usb_keyboard.h"

#include "programs/adventure.h"
//#include "programs/alive.h"
//#include "programs/brickout.h"
//#include "programs/color_cycle.h"
//#include "programs/fire.h"
//#include "programs/plasma.h"


// ============================================================================
//  Program-configurable defaults (can be overridden by program header)
// ============================================================================
#ifndef PROGRAM_PALETTE
#define PROGRAM_PALETTE c64_palette
#endif

#ifndef PROGRAM_VIDEO_BASE
#define PROGRAM_VIDEO_BASE 0x0200
#endif

#ifndef PROGRAM_CLK_FREQ_KHZ
#define PROGRAM_CLK_FREQ_KHZ 1000  // default: 1 MHz
#endif

// ============================================================================
//  Display configuration
// ============================================================================
#define DISPLAY_WIDTH  480
#define DISPLAY_HEIGHT 320
static hagl_backend_t *display;

// 32x32 pixel framebuffer
static constexpr uint16_t VIDEO_BASE = PROGRAM_VIDEO_BASE;
static constexpr uint16_t VIDEO_WIDTH = 32;
static constexpr uint16_t VIDEO_HEIGHT = 32;
static constexpr uint16_t VIDEO_SIZE = VIDEO_WIDTH * VIDEO_HEIGHT;  // 1024 bytes
static constexpr uint16_t PIXEL_SCALE = 10;     // Each pixel = 10x10 on display
static constexpr uint16_t VIEWPORT_X = 80;      // Center 320x320 in 480x320
static constexpr uint16_t VIEWPORT_Y = 0;

// Shadow framebuffer for batched display updates (shared between cores)
static volatile uint8_t framebuffer[VIDEO_SIZE];
static volatile bool fb_dirty = false;
static volatile bool cpu_running = true;

// Emulated CPU frequency (in Hz)
// 1000 = 1 kHz, 1000000 = 1 MHz, 3000000 = 3 MHz, etc.
static constexpr uint32_t CPU_FREQ_HZ = PROGRAM_CLK_FREQ_KHZ * 1000;

// Use hooked RAM to intercept writes to I/O address
static HookedRam ram;
static W65C02S cpu;

// Read hook for page 0: keyboard input ($FF) and random byte ($FE)
// $FF: Returns next character from keyboard buffer (0 if empty)
// $FE: Returns random byte from ROSC
static uint8_t page0_read_hook(uint16_t addr) {
    if (addr == 0x00FF) {
        // Keyboard input - return next character from buffer
        return usb_keyboard_getchar();
    }
    if (addr == 0x00FE) {
        // Generate 8-bit random value from ROSC random bits
        uint8_t val = 0;
        for (int i = 0; i < 8; i++) {
            val = (val << 1) | (rosc_hw->randombit & 1);
        }
        return val;
    }
    return ram[addr];
}

// Write hook: buffer pixel writes (don't draw immediately)
static void video_write_hook(uint16_t addr, uint8_t val) {
    uint16_t offset = addr - VIDEO_BASE;
    if (offset >= VIDEO_SIZE) return;
    framebuffer[offset] = val & 0x0F;
    fb_dirty = true;
}

// Refresh display from framebuffer (fast path using direct SPI blit)
static void refresh_display() {
    // Use optimized HAL function - single window setup, streamed pixels
    // Cast away volatile for the blit function (safe: core1 is the only reader)
    hagl_hal_blit_fb32(VIEWPORT_X, VIEWPORT_Y, PIXEL_SCALE,
                       (const uint8_t*)framebuffer, PROGRAM_PALETTE);
}

// Core 1: Dedicated display refresh loop
void core1_entry() {
    while (cpu_running) {
        if (fb_dirty) {
            fb_dirty = false;
            refresh_display();
        }
        // Small yield to avoid hammering the flag
        tight_loop_contents();
    }
}

void init_display() {
    display = hagl_init();

    // Clear entire screen to black
    hagl_color_t black = hagl_color(display, 0, 0, 0);
    hagl_fill_rectangle_xyxy(display, 0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1, black);
}

int main() {
    // Overclock to 200 MHz for faster SPI (allows 50 MHz SPI clock)
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    sleep_ms(10);
    set_sys_clock_khz(200000, true);

    // Set peripheral clock to system clock (needed for fast SPI)
    clock_configure(clk_peri,
                    0,
                    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
                    200 * 1000 * 1000,
                    200 * 1000 * 1000);

    init_display();

    // Initialize USB keyboard
    usb_keyboard_init();

    // Set up RAM with hooks
    HookedRam::set_instance(&ram);
    ram.set_write_hook(VIDEO_BASE, VIDEO_BASE + VIDEO_SIZE - 1, video_write_hook);
    ram.set_read_hook(0x00, page0_read_hook);  // $FE=random, $FF=keyboard (page 0)

    // Connect CPU to RAM
    cpu.ram_read = &HookedRam::static_read;
    cpu.ram_write = &HookedRam::static_write;

    // Load program at its designated address
    ram.load(program_load_addr, program, program_size);

    // Load sine table if program requires it (e.g., plasma effect)
#ifdef PROGRAM_HAS_SINE_TABLE
    ram.load(sine_table_addr, sine_table, sizeof(sine_table));
#endif

    // Set reset vector to point to program start
    ram[0xFFFC] = program_load_addr & 0xFF;         // Low byte
    ram[0xFFFD] = (program_load_addr >> 8) & 0xFF;  // High byte

    // Reset CPU (reads reset vector into PC)
    cpu.reset();
    cpu.reg.pc = ram.read_word(0xFFFC);

    // Launch Core 1 for display refresh
    multicore_launch_core1(core1_entry);

    // Core 0: Cycle-accurate CPU emulation
    // Track total cycles executed and compare against wall-clock time
    uint64_t total_cycles = 0;
    uint64_t start_time_us = time_us_64();

    while (!cpu.halted) {
        // Execute one instruction and get cycle count
        int cycles = cpu.step();
        total_cycles += cycles;

        // Poll USB keyboard periodically (every ~1000 cycles)
        if ((total_cycles & 0x3FF) == 0) {
            usb_keyboard_task();
        }

        // Calculate when these cycles should complete at target frequency
        uint64_t target_time_us = start_time_us + (total_cycles * 1000000ULL / CPU_FREQ_HZ);

        // Wait for cycle timing (only if we're ahead)
        while (time_us_64() < target_time_us) {
            tight_loop_contents();
        }
    }

    // CPU halted (STP instruction) - signal Core 1 to stop
    cpu_running = false;
    while (true) {
        sleep_ms(1000);
    }
}
