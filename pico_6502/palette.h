//
//  Palette definitions for 6502 Emulator
//
//  Programs can define their own palette by setting PROGRAM_PALETTE
//  before including their program header, otherwise c64_palette is used.
//

#ifndef PALETTE_H
#define PALETTE_H

#include <stdint.h>


// Apple IIe palette (RGB888)
static const uint32_t a2e_palette[16] = {
    0x000000,  // 0: Black
    0xDD0033,  // 1: Red
    0x000099,  // 2: Dark Blue
    0xDD22DD,  // 3: Purple
    0x007722,  // 4: Dark Green
    0x555555,  // 5: Grey
    0x2222FF,  // 6: Blue
    0x66AAFF,  // 7: Light Blue
    0x885500,  // 8: Brown
    0xFF6600,  // 9: Orange
    0xAAAAAA,  // A: Light Grey
    0xFF9988,  // B: Pink
    0x11DD00,  // C: Green
    0xFFFF00,  // D: Yellow
    0x44FF99,  // E: Aqua
    0xFFFFFF   // F: White
};

// C64 palette (RGB888)
static const uint32_t c64_palette[16] = {
    0x000000,  // 0: Black
    0xFFFFFF,  // 1: White
    0x880000,  // 2: Red
    0xAAFFEE,  // 3: Cyan
    0xCC44CC,  // 4: Purple
    0x00CC55,  // 5: Green
    0x0000AA,  // 6: Blue
    0xEEEE77,  // 7: Yellow
    0xDD8855,  // 8: Orange
    0x664400,  // 9: Brown
    0xFF7777,  // A: Light Red
    0x333333,  // B: Dark Grey
    0x777777,  // C: Grey
    0xAAFF66,  // D: Light Green
    0x0088FF,  // E: Light Blue
    0xBBBBBB   // F: Light Grey
};

// C64 alternate palette (RGB888)
static const uint32_t c64b_palette[16] = {
    0x000000,  // 0: Black
    0xFFFFFF,  // 1: White
    0x880000,  // 2: Red
    0x66BBBB,  // 3: Cyan
    0x994499,  // 4: Purple
    0x559944,  // 5: Green
    0x0000AA,  // 6: Blue
    0xDDDD66,  // 7: Yellow
    0xDD7733,  // 8: Orange
    0x664400,  // 9: Brown
    0xDD5555,  // A: Light Red
    0x333333,  // B: Dark Grey
    0x777777,  // C: Grey
    0x55CC55,  // D: Light Green
    0x4477EE,  // E: Light Blue
    0xBBBBBB   // F: Light Grey
};

// CGA palette (RGB888)
static const uint32_t cga_palette[16] = {
    0x000000,  // 0: Black
    0x0000AA,  // 1: Blue
    0x00AA00,  // 2: Green
    0x00AAAA,  // 3: Cyan
    0xAA0000,  // 4: Red
    0xAA00AA,  // 5: Magenta
    0xAA5500,  // 6: Brown
    0xAAAAAA,  // 7: Light Grey
    0x555555,  // 8: Dark Grey
    0x5555FF,  // 9: Light Blue
    0x55FF55,  // A: Light Green
    0x55FFFF,  // B: Light Cyan
    0xFF5555,  // C: Light Red
    0xFF55FF,  // D: Light Magenta
    0xFFFF55,  // E: Yellow
    0xFFFFFF   // F: White
};

// Windows 16-color palette (RGB888)
static const uint32_t win_palette[16] = {
    0x000000,  // 0: Black
    0x880000,  // 1: Maroon
    0x008800,  // 2: Green
    0x888800,  // 3: Olive
    0x000088,  // 4: Navy
    0x880088,  // 5: Purple
    0x008888,  // 6: Teal
    0xCCCCCC,  // 7: Silver
    0x888888,  // 8: Grey
    0xFF0000,  // 9: Red
    0x00FF00,  // A: Lime
    0xFFFF00,  // B: Yellow
    0x0000FF,  // C: Blue
    0xFF00FF,  // D: Fuchsia
    0x00FFFF,  // E: Aqua
    0xFFFFFF   // F: White
};

// Monochrome grayscale palette (RGB888)
static const uint32_t mono_palette[16] = {
    0x000000,  // 0
    0x111111,  // 1
    0x222222,  // 2
    0x333333,  // 3
    0x444444,  // 4
    0x555555,  // 5
    0x666666,  // 6
    0x777777,  // 7
    0x888888,  // 8
    0x999999,  // 9
    0xAAAAAA,  // A
    0xBBBBBB,  // B
    0xCCCCCC,  // C
    0xDDDDDD,  // D
    0xEEEEEE,  // E
    0xFFFFFF   // F
};

// Phosphor green CRT palette (RGB888)
static const uint32_t green_palette[16] = {
    0x000000,  // 0
    0x001100,  // 1
    0x002200,  // 2
    0x003300,  // 3
    0x004400,  // 4
    0x005500,  // 5
    0x006600,  // 6
    0x007700,  // 7
    0x008800,  // 8
    0x009900,  // 9
    0x00AA00,  // A
    0x00BB00,  // B
    0x00CC00,  // C
    0x00DD00,  // D
    0x00EE00,  // E
    0x00FF00   // F
};

// Amber CRT palette (RGB888)
static const uint32_t amber_palette[16] = {
    0x000000,  // 0
    0x111000,  // 1
    0x221100,  // 2
    0x332200,  // 3
    0x443300,  // 4
    0x554400,  // 5
    0x665500,  // 6
    0x776600,  // 7
    0x887700,  // 8
    0x998800,  // 9
    0xAA9900,  // A
    0xBBAA00,  // B
    0xCCBB00,  // C
    0xDDCC00,  // D
    0xEEDD00,  // E
    0xFFEE00   // F
};

#endif // PALETTE_H
