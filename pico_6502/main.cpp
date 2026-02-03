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
#include "hardware/structs/rosc.h"
#include "w65c02s.hpp"
#include "ram.hpp"
#include "hagl.h"
#include "hagl_hal.h"
#include "palette.h"

//#include "programs/color_cycle.h"
//#include "programs/adventure.h"
//#include "programs/alive.h"
//#include "programs/brickout.h"
#include "programs/plasma.h"


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

// Shadow framebuffer for batched display updates
static uint8_t framebuffer[VIDEO_SIZE];
static bool fb_dirty = false;

// Emulated CPU frequency (in Hz)
// 1000 = 1 kHz, 1000000 = 1 MHz, 3000000 = 3 MHz, etc.
static constexpr uint32_t CPU_FREQ_HZ = PROGRAM_CLK_FREQ_KHZ * 1000;

// Use hooked RAM to intercept writes to I/O address
static HookedRam ram;
static W65C02S cpu;

// Read hook: return random byte when $FE is read
static uint8_t random_read_hook(uint16_t addr) {
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
    if (!fb_dirty) return;
    fb_dirty = false;

    // Use optimized HAL function - single window setup, streamed pixels
    hagl_hal_blit_fb32(VIEWPORT_X, VIEWPORT_Y, PIXEL_SCALE, framebuffer, PROGRAM_PALETTE);
}

void init_display() {
    display = hagl_init();

    // Clear entire screen to black
    hagl_color_t black = hagl_color(display, 0, 0, 0);
    hagl_fill_rectangle_xyxy(display, 0, 0, DISPLAY_WIDTH - 1, DISPLAY_HEIGHT - 1, black);
}

int main() {
    init_display();

    // Set up RAM with hooks
    HookedRam::set_instance(&ram);
    ram.set_write_hook(VIDEO_BASE, VIDEO_BASE + VIDEO_SIZE - 1, video_write_hook);
    ram.set_read_hook(0x00, random_read_hook);  // $FE returns random byte (page 0)

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

    // Cycle-accurate timing
    // Track total cycles executed and compare against wall-clock time
    uint64_t total_cycles = 0;
    uint64_t start_time_us = time_us_64();

    // Display refresh timing (target ~30 FPS = 33333 us per frame)
    constexpr uint64_t REFRESH_INTERVAL_US = 33333;
    uint64_t next_refresh_us = start_time_us + REFRESH_INTERVAL_US;

    while (!cpu.halted) {
        // Execute one instruction and get cycle count
        int cycles = cpu.step();
        total_cycles += cycles;

        // Calculate when these cycles should complete at target frequency
        uint64_t target_time_us = start_time_us + (total_cycles * 1000000ULL / CPU_FREQ_HZ);
        uint64_t now = time_us_64();

        // Refresh display at fixed interval (account for refresh time in timing)
        if (now >= next_refresh_us) {
            uint64_t before_refresh = time_us_64();
            refresh_display();
            uint64_t refresh_duration = time_us_64() - before_refresh;
            // Adjust timing to account for display refresh overhead
            start_time_us += refresh_duration;
            next_refresh_us = time_us_64() + REFRESH_INTERVAL_US;
        }

        // Wait for cycle timing (only if we're ahead)
        while (time_us_64() < target_time_us) {
            tight_loop_contents();
        }
    }

    // CPU halted (STP instruction)
    while (true) {
        sleep_ms(1000);
    }
}
