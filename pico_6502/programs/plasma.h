//
// 6502 Program: Plasma Effect
//
// Classic demoscene plasma using overlapping sine waves.
// 32x32 display with smooth color cycling.
//
// Entry point: 0x0600
// Sine table: 0x0700-0x07FF (256 bytes)
//

#pragma once

#include <cstdint>
#include <cstddef>

// Program-specific overrides
#define PROGRAM_VIDEO_BASE 0x0200
#define PROGRAM_CLK_FREQ_KHZ 50000

// Custom plasma palette - smooth rainbow gradient for best effect
static const uint32_t plasma_palette[16] = {
    0x000040,  // 0: Deep blue
    0x0000C0,  // 1: Blue
    0x0040FF,  // 2: Light blue
    0x00C0FF,  // 3: Cyan
    0x00FFC0,  // 4: Cyan-green
    0x00FF40,  // 5: Green
    0x40FF00,  // 6: Yellow-green
    0xC0FF00,  // 7: Yellow
    0xFFC000,  // 8: Orange
    0xFF4000,  // 9: Red-orange
    0xFF0040,  // A: Red
    0xFF00C0,  // B: Magenta
    0xC000FF,  // C: Purple
    0x4000FF,  // D: Violet
    0x2000A0,  // E: Deep violet
    0x100060   // F: Dark purple
};
#define PROGRAM_PALETTE plasma_palette

// Load address for this program
inline constexpr uint16_t program_load_addr = 0x0600;

// Pre-computed sine lookup table (256 entries, values 0-85)
// Formula: round(42.5 + 42.5 * sin(i * 2 * PI / 256))
// This allows 3 waves to sum to 0-255, then >> 4 gives 0-15
inline constexpr uint8_t sine_table[256] = {
    42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
    58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 69, 70, 71, 72,
    73, 73, 74, 75, 75, 76, 77, 77, 78, 78, 79, 79, 80, 80, 81, 81,
    81, 82, 82, 82, 83, 83, 83, 83, 84, 84, 84, 84, 84, 84, 84, 85,
    85, 85, 84, 84, 84, 84, 84, 84, 84, 83, 83, 83, 83, 82, 82, 82,
    81, 81, 81, 80, 80, 79, 79, 78, 78, 77, 77, 76, 75, 75, 74, 73,
    73, 72, 71, 70, 69, 69, 68, 67, 66, 65, 64, 63, 62, 61, 60, 59,
    58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43,
    42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27,
    26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 15, 14, 13, 12,
    11, 11, 10,  9,  9,  8,  7,  7,  6,  6,  5,  5,  4,  4,  3,  3,
     3,  2,  2,  2,  1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,  2,  2,  2,  3,
     3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  9,  9, 10, 11, 11,
    12, 13, 14, 15, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
    27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42
};

// Sine table load address
inline constexpr uint16_t sine_table_addr = 0x0700;
#define PROGRAM_HAS_SINE_TABLE  // Signal to main.cpp to load the sine table

//
// 6502 Assembly (hand-assembled):
//
// Memory map:
//   $00       - animation time counter
//   $01       - temp accumulator for plasma sum
//   $02       - current X coordinate (0-31)
//   $03       - current Y coordinate (0-31)
//   $04-$05   - video RAM pointer (lo/hi)
//   $0200-$05FF - Video RAM (32x32 = 1024 bytes)
//   $0600     - Program start
//   $0700-$07FF - Sine lookup table (256 bytes)
//
// Algorithm:
//   For each pixel (x, y):
//     wave1 = sin[x*4 + time]
//     wave2 = sin[y*4 + time*2]
//     wave3 = sin[(x + y)*2 + time]
//     color = (wave1 + wave2 + wave3) >> 4
//   time++
//
inline constexpr uint8_t program[] = {
    // 0600: Initialize time to 0
    0xA9, 0x00,             // LDA #$00
    0x85, 0x00,             // STA $00        ; time = 0

    // 0604: frame - start of frame loop
    // Set video pointer to $0200
    0xA9, 0x00,             // LDA #$00
    0x85, 0x04,             // STA $04        ; video_ptr_lo = $00
    0xA9, 0x02,             // LDA #$02
    0x85, 0x05,             // STA $05        ; video_ptr_hi = $02

    // Initialize Y = 0
    0xA9, 0x00,             // LDA #$00
    0x85, 0x03,             // STA $03        ; Y = 0

    // 0610: yloop - Y coordinate loop
    // Initialize X = 0
    0xA9, 0x00,             // LDA #$00
    0x85, 0x02,             // STA $02        ; X = 0

    // 0614: xloop - X coordinate loop
    // Wave 1: sin[X*4 + time]
    0xA5, 0x02,             // LDA $02        ; load X
    0x0A,                   // ASL A          ; X * 2
    0x0A,                   // ASL A          ; X * 4
    0x18,                   // CLC
    0x65, 0x00,             // ADC $00        ; + time
    0xAA,                   // TAX
    0xBD, 0x00, 0x07,       // LDA $0700,X    ; sin[X*4 + time]
    0x85, 0x01,             // STA $01        ; accumulator = wave1

    // Wave 2: sin[Y*4 + time*2]
    0xA5, 0x00,             // LDA $00        ; load time
    0x0A,                   // ASL A          ; time * 2
    0x85, 0x06,             // STA $06        ; temp store time*2
    0xA5, 0x03,             // LDA $03        ; load Y
    0x0A,                   // ASL A          ; Y * 2
    0x0A,                   // ASL A          ; Y * 4
    0x18,                   // CLC
    0x65, 0x06,             // ADC $06        ; + time*2
    0xAA,                   // TAX
    0xBD, 0x00, 0x07,       // LDA $0700,X    ; sin[Y*4 + time*2]
    0x18,                   // CLC
    0x65, 0x01,             // ADC $01        ; + accumulator
    0x85, 0x01,             // STA $01        ; accumulator = wave1 + wave2

    // Wave 3: sin[(X + Y)*2 + time]
    0xA5, 0x02,             // LDA $02        ; load X
    0x18,                   // CLC
    0x65, 0x03,             // ADC $03        ; + Y
    0x0A,                   // ASL A          ; (X + Y) * 2
    0x18,                   // CLC
    0x65, 0x00,             // ADC $00        ; + time
    0xAA,                   // TAX
    0xBD, 0x00, 0x07,       // LDA $0700,X    ; sin[(X+Y)*2 + time]
    0x18,                   // CLC
    0x65, 0x01,             // ADC $01        ; + accumulator

    // Scale from 0-255 to 0-15 (divide by 16)
    0x4A,                   // LSR A
    0x4A,                   // LSR A
    0x4A,                   // LSR A
    0x4A,                   // LSR A

    // Store pixel to video RAM
    0xA0, 0x00,             // LDY #$00
    0x91, 0x04,             // STA ($04),Y    ; store pixel

    // Increment video pointer
    0xE6, 0x04,             // INC $04        ; video_ptr_lo++
    0xD0, 0x02,             // BNE +2         ; skip if no carry
    0xE6, 0x05,             // INC $05        ; video_ptr_hi++

    // X++, loop while X < 32
    0xE6, 0x02,             // INC $02        ; X++
    0xA5, 0x02,             // LDA $02
    0xC9, 0x20,             // CMP #$20       ; X < 32?
    0xD0, 0xB8,             // BNE xloop      ; branch to $0614 (-72)

    // Y++, loop while Y < 32
    0xE6, 0x03,             // INC $03        ; Y++
    0xA5, 0x03,             // LDA $03
    0xC9, 0x20,             // CMP #$20       ; Y < 32?
    0xD0, 0xAC,             // BNE yloop      ; branch to $0610 (-84)

    // Increment time and loop forever
    0xE6, 0x00,             // INC $00        ; time++
    0x4C, 0x04, 0x06        // JMP $0604      ; next frame
};

inline constexpr size_t program_size = sizeof(program);
