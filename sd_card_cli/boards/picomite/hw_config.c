/*
 * hw_config.c
 * for PicoMite (RP2350A) SPI interface
 *
 * PicoMite SD card pinout (active jumper config):
 *   SCK  = GPIO 26  (SPI1)
 *   MOSI = GPIO 27  (SPI1)
 *   MISO = GPIO 28  (SPI1)
 *   CS   = GPIO 22
 */

#include "hw_config.h"

static spi_t spi = {
    .hw_inst = spi1,
    .sck_gpio = 26,
    .mosi_gpio = 27,
    .miso_gpio = 28,
    .baud_rate = 12500 * 1000,  /* 12.5 MHz */
};

static sd_spi_if_t spi_if = {
    .spi = &spi,
    .ss_gpio = 22,
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
