//
// 6502 Program: Brickout
//
// A brick breaker game with auto-reset when all bricks are destroyed.
// Uses $04 as brick counter (120 bricks = 4 columns x 30 rows).
//
// Entry point: 0x0600
//

#pragma once

// Program-specific overrides
#define PROGRAM_VIDEO_BASE 0x0200
#define PROGRAM_CLK_FREQ_KHZ 50

#include <cstdint>
#include <cstddef>

// Load address for this program
inline constexpr uint16_t program_load_addr = 0x0600;

inline constexpr uint8_t program[] = {
    // 0600: Init ball direction
    0xa9, 0xfe, 0x85, 0x02,             // LDA #$FE, STA $02 (X dir)
    0xa9, 0xee, 0x85, 0x03,             // LDA #$EE, STA $03 (Y dir)
    // 0608: draw_init - video pointer
    0xa9, 0x00, 0x85, 0x00,             // LDA #$00, STA $00
    0xa9, 0x02, 0x85, 0x01,             // LDA #$02, STA $01
    0xa2, 0x20,                         // LDX #$20 (32 rows)
    // 0612: draw_row
    0xa9, 0x02,                         // LDA #$02 (wall color)
    0x9d, 0xff, 0x01,                   // STA $01FF,X (top)
    0x9d, 0xdf, 0x05,                   // STA $05DF,X (bottom)
    0xa0, 0x00, 0x91, 0x00,             // LDY #$00, STA ($00),Y (left)
    0xa0, 0x1f, 0x91, 0x00,             // LDY #$1F, STA ($00),Y (right)
    // Skip bricks on row 0 (X=32) and row 31 (X=1) to preserve borders
    0xe0, 0x01, 0xf0, 0x19,             // CPX #$01, BEQ +25 (skip_bricks)
    0xe0, 0x20, 0xf0, 0x15,             // CPX #$20, BEQ +21 (skip_bricks)
    // Draw 4 bricks (colors 3,4,5,6) at columns 23-26
    0xa9, 0x03, 0xa0, 0x17, 0x91, 0x00, // LDA #$03, LDY #$17, STA ($00),Y
    0xa9, 0x04, 0xc8, 0x91, 0x00,       // LDA #$04, INY, STA ($00),Y
    0xa9, 0x05, 0xc8, 0x91, 0x00,       // LDA #$05, INY, STA ($00),Y
    0xa9, 0x06, 0xc8, 0x91, 0x00,       // LDA #$06, INY, STA ($00),Y
    // 063F: skip_bricks - advance to next row
    0x18, 0xa5, 0x00, 0x69, 0x20,       // CLC, LDA $00, ADC #$20
    0x85, 0x00, 0xa5, 0x01, 0x69, 0x00, // STA $00, LDA $01, ADC #$00
    0x85, 0x01, 0xca, 0xd0, 0xc3,       // STA $01, DEX, BNE -61 (draw_row)
    // 064F: Init brick counter (120 = 4 cols x 30 rows)
    0xa9, 0x78, 0x85, 0x04,             // LDA #$78, STA $04
    // 0653: Init ball position
    0xa6, 0x02, 0xa4, 0x03,             // LDX $02, LDY $03
    0xa9, 0x44, 0x85, 0x00,             // LDA #$44, STA $00
    0xa9, 0x02, 0x85, 0x01,             // LDA #$02, STA $01
    // 065F: game_loop
    0x8a, 0x48,                         // TXA, PHA
    0xa9, 0x01, 0xa2, 0x00, 0x81, 0x00, // LDA #$01, LDX #$00, STA ($00,X) (draw ball)
    0x68, 0xaa, 0xca,                   // PLA, TAX, DEX
    0xf0, 0x4a,                         // BEQ +74 -> 06B6 (check_x)
    0x88, 0xd0, 0xfa,                   // DEY, BNE -6 (delay)
    // 066F: Check Y movement
    0x8a, 0x48,                         // TXA, PHA
    0x20, 0xe8, 0x06,                   // JSR $06E8 (clear_ball) **FIXED**
    // 0674: check_y_dir
    0xa5, 0x03, 0x29, 0x01,             // LDA $03, AND #$01
    0xd0, 0x0d,                         // BNE +13 (move_up)
    // move_down
    0x18, 0xa5, 0x00, 0x69, 0x20,       // CLC, LDA $00, ADC #$20
    0x85, 0x00, 0x90, 0x11,             // STA $00, BCC +17 (check_y_coll)
    0xe6, 0x01, 0xd0, 0x0d,             // INC $01, BNE +13 (check_y_coll)
    // 0687: move_up
    0x38, 0xa5, 0x00, 0xe9, 0x20,       // SEC, LDA $00, SBC #$20
    0x85, 0x00, 0xa5, 0x01, 0xe9, 0x00, // STA $00, LDA $01, SBC #$00
    0x85, 0x01,                         // STA $01
    // 0694: check_y_coll
    0xa2, 0x00, 0xa1, 0x00,             // LDX #$00, LDA ($00,X)
    0xd0, 0x07,                         // BNE +7 (y_collision)
    // no collision
    0xa4, 0x03, 0x68, 0xaa,             // LDY $03, PLA, TAX
    0x4c, 0x5f, 0x06,                   // JMP $065F (game_loop)
    // 06A1: y_collision
    0xc9, 0x02, 0xf0, 0x08,             // CMP #$02, BEQ +8 (y_bounce, skip brick dec)
    // brick hit
    0xa9, 0x00, 0x81, 0x00,             // LDA #$00, STA ($00,X)
    0xc6, 0x04,                         // DEC $04 (brick counter)
    0xf0, 0x38,                         // BEQ +56 -> 06E5 (reset) **FIXED**
    // 06AD: y_bounce
    0xa9, 0x01, 0x45, 0x03, 0x85, 0x03, // LDA #$01, EOR $03, STA $03
    0x4c, 0x74, 0x06,                   // JMP $0674 (check_y_dir)
    // 06B6: check_x
    0x20, 0xe8, 0x06,                   // JSR $06E8 (clear_ball) **FIXED**
    // 06B9: check_x_dir
    0xa5, 0x02, 0x29, 0x01,             // LDA $02, AND #$01
    0xd0, 0x04,                         // BNE +4 (move_left)
    // move_right
    0xe6, 0x00, 0xd0, 0x02,             // INC $00, BNE +2 (check_x_coll)
    // 06C3: move_left
    0xc6, 0x00,                         // DEC $00
    // 06C5: check_x_coll
    0xa2, 0x00, 0xa1, 0x00,             // LDX #$00, LDA ($00,X)
    0xd0, 0x05,                         // BNE +5 (x_collision)
    // no collision
    0xa6, 0x02,                         // LDX $02
    0x4c, 0x5f, 0x06,                   // JMP $065F (game_loop)
    // 06D0: x_collision
    0xc9, 0x02, 0xf0, 0x08,             // CMP #$02, BEQ +8 (x_bounce, skip brick dec)
    // brick hit
    0xa9, 0x00, 0x81, 0x00,             // LDA #$00, STA ($00,X)
    0xc6, 0x04,                         // DEC $04 (brick counter)
    0xf0, 0x09,                         // BEQ +9 -> 06E5 (reset) **FIXED**
    // 06DC: x_bounce
    0xa9, 0x01, 0x45, 0x02, 0x85, 0x02, // LDA #$01, EOR $02, STA $02
    0x4c, 0xb9, 0x06,                   // JMP $06B9 (check_x_dir)
    // 06E5: reset - redraw everything
    0x4c, 0x08, 0x06,                   // JMP $0608 (draw_init)
    // 06E8: clear_ball subroutine
    0xa9, 0x00, 0xaa, 0x81, 0x00, 0x60  // LDA #$00, TAX, STA ($00,X), RTS
};

inline constexpr size_t program_size = sizeof(program);
