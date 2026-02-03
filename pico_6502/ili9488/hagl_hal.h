/*
 * HAGL HAL for ILI9488 on RP2350
 */

#ifndef _HAGL_HAL_H
#define _HAGL_HAL_H

#include <stdint.h>
#include <stddef.h>
#include "hagl_hal_color.h"
#include "hagl/backend.h"

#define DISPLAY_WIDTH   480
#define DISPLAY_HEIGHT  320
#define DISPLAY_DEPTH   24

/* HAL init function called by HAGL */
void hagl_hal_init(hagl_backend_t *backend);

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Fast scaled framebuffer blit for 6502 emulator
 * Blits a 32x32 4-bit framebuffer scaled by 'scale' at position (x0, y0)
 * fb: 1024 bytes (4-bit color indices 0-15)
 * palette: 16 RGB888 colors
 */
void hagl_hal_blit_fb32(int16_t x0, int16_t y0, uint8_t scale,
                        const uint8_t *fb, const uint32_t *palette);

#ifdef __cplusplus
}
#endif

#endif /* _HAGL_HAL_H */
