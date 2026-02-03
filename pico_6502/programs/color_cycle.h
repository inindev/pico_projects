//
// 6502 Program: Cycle through all 16 colors
//
// Fills the 32x32 video RAM with each color in sequence (0-15), then wraps.
//
// Entry point: 0x0600
//

#pragma once

// Program-specific overrides
#define PROGRAM_VIDEO_BASE 0x0200
#define PROGRAM_CLK_FREQ_KHZ 5000

#include <cstdint>
#include <cstddef>

// Load address for this program
inline constexpr uint16_t program_load_addr = 0x0600;

inline constexpr uint8_t program[] = {
    0xA9, 0x00,             // LDA #$00       ; color = 0
    0x85, 0x00,             // STA $00
    // fill:
    0xA5, 0x00,             // LDA $00        ; load color
    0xA2, 0x00,             // LDX #$00
    0x9D, 0x00, 0x02,       // STA $0200,X
    0xE8,                   // INX
    0xD0, 0xFA,             // BNE -6
    0x9D, 0x00, 0x03,       // STA $0300,X
    0xE8,                   // INX
    0xD0, 0xFA,             // BNE -6
    0x9D, 0x00, 0x04,       // STA $0400,X
    0xE8,                   // INX
    0xD0, 0xFA,             // BNE -6
    0x9D, 0x00, 0x05,       // STA $0500,X
    0xE8,                   // INX
    0xD0, 0xFA,             // BNE -6
    0xE6, 0x00,             // INC $00        ; color++
    0xA5, 0x00,             // LDA $00
    0x29, 0x0F,             // AND #$0F       ; wrap 0-15
    0x85, 0x00,             // STA $00
    0x4C, 0x04, 0x06        // JMP $0604      ; loop
};

inline constexpr size_t program_size = sizeof(program);
