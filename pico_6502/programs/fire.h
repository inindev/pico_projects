//
// 6502 Program: Fire Effect
//
// Classic demoscene fire using cellular automata.
// Bottom row seeded with random hot values, each pixel above
// averages neighbors below with cooling for upward flame effect.
//
// Entry point: 0x0600
//

#pragma once

#include <cstdint>
#include <cstddef>

// Program-specific overrides
#define PROGRAM_VIDEO_BASE 0x0200
#define PROGRAM_CLK_FREQ_KHZ 2000

// Fire palette - black through red/orange/yellow to white
static const uint32_t fire_palette[16] = {
    0x000000,  // 0: Black
    0x1F0000,  // 1: Very dark red
    0x3F0000,  // 2: Dark red
    0x5F0000,  // 3: Dark red
    0x7F0000,  // 4: Red
    0x9F0000,  // 5: Red
    0xBF1F00,  // 6: Red-orange
    0xDF3F00,  // 7: Orange
    0xFF5F00,  // 8: Orange
    0xFF7F00,  // 9: Orange-yellow
    0xFF9F00,  // A: Yellow-orange
    0xFFBF00,  // B: Yellow
    0xFFDF00,  // C: Yellow
    0xFFEF00,  // D: Bright yellow
    0xFFFF7F,  // E: Yellow-white
    0xFFFFFF   // F: White
};
#define PROGRAM_PALETTE fire_palette

// Load address for this program
inline constexpr uint16_t program_load_addr = 0x0600;

//
// 6502 Assembly (hand-assembled):
//
// Memory map:
//   $FE       - random byte source (hardware RNG)
//   $01       - temp X coordinate
//   $02       - current Y coordinate (row being processed)
//   $03       - sum accumulator for averaging
//   $04-$05   - current row video pointer
//   $06-$07   - row below video pointer
//   $0200-$05FF - Video RAM (32x32 = 1024 bytes)
//   $0600     - Program start
//
// Algorithm:
//   1. Seed bottom row (row 31) with random hot values (12-15)
//   2. For each row from 30 down to 0:
//      For each pixel x from 0 to 31:
//        sum = below + below + below_left + below_right
//        pixel = (sum / 4) - 1  (cooling)
//        clamp to 0 if negative
//   3. Repeat forever
//
inline constexpr uint8_t program[] = {
    // === frame: Start of frame loop ===
    // Seed bottom row (row 31 at $05E0) with random hot values
    0xA9, 0xE0,             // $0600: LDA #$E0
    0x85, 0x04,             // $0602: STA $04        ; ptr_lo = $E0
    0xA9, 0x05,             // $0604: LDA #$05
    0x85, 0x05,             // $0606: STA $05        ; ptr_hi = $05 -> $05E0

    0xA0, 0x00,             // $0608: LDY #0         ; X counter

    // seed: Seed loop for bottom row
    0xA5, 0xFE,             // $060A: LDA $FE        ; random byte
    0x09, 0x0C,             // $060C: ORA #$0C       ; bias to 12-15 (hot)
    0x29, 0x0F,             // $060E: AND #$0F       ; mask to 0-15
    0x91, 0x04,             // $0610: STA ($04),Y    ; store pixel
    0xC8,                   // $0612: INY
    0xC0, 0x20,             // $0613: CPY #32
    0xD0, 0xF3,             // $0615: BNE seed       ; loop back to $060A

    // Initialize Y = 30 (process rows 30 down to 0)
    0xA9, 0x1E,             // $0617: LDA #30
    0x85, 0x02,             // $0619: STA $02        ; Y = 30

    // === yloop: Process one row ===
    // Calculate current row pointer = $0200 + Y*32
    0xA5, 0x02,             // $061B: LDA $02        ; load Y
    0x0A,                   // $061D: ASL A          ; *2
    0x0A,                   // $061E: ASL A          ; *4
    0x0A,                   // $061F: ASL A          ; *8
    0x0A,                   // $0620: ASL A          ; *16
    0x0A,                   // $0621: ASL A          ; *32 (low byte)
    0x85, 0x04,             // $0622: STA $04
    0xA5, 0x02,             // $0624: LDA $02
    0x4A,                   // $0626: LSR A          ; Y/2
    0x4A,                   // $0627: LSR A          ; Y/4
    0x4A,                   // $0628: LSR A          ; Y/8 (high bits)
    0x18,                   // $0629: CLC
    0x69, 0x02,             // $062A: ADC #$02       ; + $02 base
    0x85, 0x05,             // $062C: STA $05        ; current row ptr hi

    // Row below pointer = current + 32
    0xA5, 0x04,             // $062E: LDA $04
    0x18,                   // $0630: CLC
    0x69, 0x20,             // $0631: ADC #$20       ; +32
    0x85, 0x06,             // $0633: STA $06
    0xA5, 0x05,             // $0635: LDA $05
    0x69, 0x00,             // $0637: ADC #0
    0x85, 0x07,             // $0639: STA $07        ; row below ptr

    0xA0, 0x00,             // $063B: LDY #0         ; X = 0

    // === xloop: Process one pixel ===
    // Sum = pixel_below * 2 (double weight center)
    0xB1, 0x06,             // $063D: LDA ($06),Y    ; below
    0x85, 0x03,             // $063F: STA $03        ; sum = below
    0x18,                   // $0641: CLC
    0x65, 0x03,             // $0642: ADC $03        ; sum += below
    0x85, 0x03,             // $0644: STA $03

    // Add below-left (or center if at left edge)
    0x98,                   // $0646: TYA            ; check X
    0xF0, 0x0C,             // $0647: BEQ skip_left  ; if X=0, skip to $0655
    0x88,                   // $0649: DEY            ; X-1
    0xB1, 0x06,             // $064A: LDA ($06),Y    ; below-left
    0xC8,                   // $064C: INY            ; restore X
    0x18,                   // $064D: CLC
    0x65, 0x03,             // $064E: ADC $03
    0x85, 0x03,             // $0650: STA $03
    0x4C, 0x5C, 0x06,       // $0652: JMP do_right   ; $065C

    // skip_left: Use center instead
    0xB1, 0x06,             // $0655: LDA ($06),Y    ; below (center)
    0x18,                   // $0657: CLC
    0x65, 0x03,             // $0658: ADC $03
    0x85, 0x03,             // $065A: STA $03

    // do_right: Add below-right (or center if at right edge)
    0xC0, 0x1F,             // $065D: CPY #31        ; check if X=31
    0xF0, 0x0C,             // $065F: BEQ skip_right ; if X=31, skip to $066D
    0xC8,                   // $0661: INY            ; X+1
    0xB1, 0x06,             // $0662: LDA ($06),Y    ; below-right
    0x88,                   // $0664: DEY            ; restore X
    0x18,                   // $0665: CLC
    0x65, 0x03,             // $0666: ADC $03
    0x85, 0x03,             // $0668: STA $03
    0x4C, 0x73, 0x06,       // $0669: JMP divide     ; $0673

    // skip_right: Use center instead
    0xB1, 0x06,             // $066D: LDA ($06),Y    ; below (center)
    0x18,                   // $066F: CLC
    0x65, 0x03,             // $0670: ADC $03
    0x85, 0x03,             // $0672: STA $03

    // divide: Divide sum by 4 and apply cooling
    0xA5, 0x03,             // $0675: LDA $03
    0x4A,                   // $0677: LSR A          ; /2
    0x4A,                   // $0678: LSR A          ; /4
    0x38,                   // $0679: SEC
    0xE9, 0x01,             // $067A: SBC #1         ; cooling
    0xB0, 0x02,             // $067C: BCS no_clamp   ; if >= 0, skip
    0xA9, 0x00,             // $067E: LDA #0         ; clamp to 0

    // no_clamp: Store result
    0x91, 0x04,             // $0680: STA ($04),Y    ; store pixel

    // Next X
    0xC8,                   // $0682: INY
    0xC0, 0x20,             // $0683: CPY #32
    0xD0, 0xB8,             // $0685: BNE xloop      ; back to $063D

    // Next Y (decrement, process upward)
    0xC6, 0x02,             // $0687: DEC $02
    0x10, 0x92,             // $0689: BPL yloop      ; back to $061B while Y >= 0

    // Frame complete, loop forever
    0x4C, 0x00, 0x06        // $068B: JMP frame      ; $0600
};

inline constexpr size_t program_size = sizeof(program);
