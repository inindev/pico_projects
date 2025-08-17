#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "dvi/dvi.h"
#include "render.h"
#include "dhgr_patterns.h"

#define WIDTH 720
#define HEIGHT 480
#define BPP 4
#define LINE_BYTES (WIDTH * BPP / 8) // 360 bytes per line
#define FRAMEBUFFER_SIZE (WIDTH * HEIGHT * BPP / 8) // 172.8 KB

static uint8_t framebuffer[LINE_BYTES]; // Single line buffer
static struct dvi_inst dvi0;

// TMDS buffer for one line
static uint32_t tmds_buffer[WIDTH / 2]; // 4 bpp, 2 pixels per 32-bit word

void render_blue_line(uint8_t *fb, uint32_t *tmds_buf, uint y) {
    // Fill line with blue (e.g., 0x1 from dhgr_pixel_table for DBlu)
    for (int x = 0; x < LINE_BYTES; x += 2) {
        fb[x] = 0x11; // Two 4-bit pixels: DBlu (0x1)
        fb[x + 1] = 0x11;
    }
    
    // Convert to TMDS (mimic render_dhgr.c)
    for (int x = 0; x < WIDTH / 2; x++) {
        uint8_t pixel_pair = fb[x];
        tmds_buf[x] = (dhgr_pixel_table[pixel_pair & 0x0F] << 16) |
                      (dhgr_pixel_table[(pixel_pair >> 4) & 0x0F]);
    }
}

void main() {
    // Initialize DVI for 720x480
    dvi0.timing = &DVI_TIMING_720x480p_60Hz;
    dvi0.ser_cfg = DVI_DEFAULT_SERIAL_CONFIG;
    dvi_init(&dvi0, next_striped_spin_lock());
    
    // Claim DMA channel
    int dma_chan = dma_claim_unused_channel(true);
    
    // Render loop
    while (1) {
        for (uint y = 0; y < HEIGHT; y++) {
            // Render one line
            render_blue_line(framebuffer, tmds_buffer, y);
            
            // DMA to TMDS output (mimic render_dhgr.c)
            dma_channel_transfer_from_buffer_now(dma_chan, tmds_buffer, WIDTH / 2);
            
            // Wait for scanline completion
            dvi_scanline_wait_for(&dvi0);
        }
    }
}