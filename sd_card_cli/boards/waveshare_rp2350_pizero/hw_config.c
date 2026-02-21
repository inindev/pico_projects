/*
 * hw_config.c
 * for Waveshare RP2350-PiZero (RP2350B) SPI interface
 *
 * Waveshare RP2350-PiZero SD card pinout:
 *   SCK  = GPIO 30  (SPI1)
 *   MOSI = GPIO 31  (SPI1)
 *   MISO = GPIO 40  (SPI1)
 *   CS   = GPIO 43
 */

#include "hw_config.h"

static spi_t spi = {
    .hw_inst = spi1,
    .sck_gpio = 30,
    .mosi_gpio = 31,
    .miso_gpio = 40,
    .baud_rate = 12500 * 1000,  /* 12.5 MHz */
};

static sd_spi_if_t spi_if = {
    .spi = &spi,
    .ss_gpio = 43,
};

static sd_card_t sd_card = {
    .type = SD_IF_SPI,
    .spi_if_p = &spi_if,
    .use_card_detect = false,
};

size_t sd_get_num(void) {
    return 1;
}

sd_card_t *sd_get_by_num(size_t num) {
    return (num == 0) ? &sd_card : NULL;
}
