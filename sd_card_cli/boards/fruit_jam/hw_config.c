/*
 * hw_config.c
 * for Adafruit Fruit Jam (RP2350B) SDIO interface
 *
 * Fruit Jam SDIO pinout:
 *   CLK  = GPIO 34  (auto-calculated from D0)
 *   CMD  = GPIO 35
 *   D0   = GPIO 36
 *   D1   = GPIO 37
 *   D2   = GPIO 38
 *   D3   = GPIO 39
 *   CD   = GPIO 33  (active-high card detect)
 */

#include "hw_config.h"

static sd_sdio_if_t sdio_if = {
    .CMD_gpio = 35,
    .D0_gpio = 36,
    /* CLK, D1-D3 are auto-calculated by sd_sdio_ctor() from D0 */
    .SDIO_PIO = pio1,
    .DMA_IRQ_num = DMA_IRQ_1,
    .baud_rate = 150 * 1000 * 1000 / 6,  /* 25 MHz (RP2350 default clk_sys=150MHz) */
};

static sd_card_t sd_card = {
    .type = SD_IF_SDIO,
    .sdio_if_p = &sdio_if,
    .use_card_detect = true,
    .card_detect_gpio = 33,
    .card_detected_true = 1,    /* active-high: GPIO reads 1 when card is present */
    .card_detect_use_pull = true,
    .card_detect_pull_hi = true, /* enable internal pull-up */
};

size_t sd_get_num(void) {
    return 1;
}

sd_card_t *sd_get_by_num(size_t num) {
    return (num == 0) ? &sd_card : NULL;
}
