// pico_projects/pico_sd_card/sd_card_diskio.c
//
// implements the fatfs disk i/o layer for sd card access via spi on raspberry pi pico
// or pico 2. provides functions to initialize, read, and query the sd card, used by
// sd_card_cli.c. part of the pico projects repository.
// license: mit (see license file in repository root).

#include <stdio.h>
#include "ff.h"
#include "diskio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

// sd card spi configuration (matches sd_card_cli.c)
#define SPI_PORT spi1
#define PIN_MISO 12  // gp12 (miso)
#define PIN_CS   13  // gp13 (chip select)
#define PIN_MOSI 11  // gp11 (mosi)
#define PIN_SCK  10  // gp10 (clock)

// disk status variable
static volatile DSTATUS sd_disk_status = STA_NOINIT;

// send spi command and get response
static BYTE send_cmd(BYTE cmd, DWORD arg) {
    BYTE resp;
    BYTE buf[6];

    // prepare command packet
    buf[0] = 0x40 | cmd;                     // start bit and command index
    buf[1] = (arg >> 24) & 0xFF;             // argument bits 31-24
    buf[2] = (arg >> 16) & 0xFF;             // argument bits 23-16
    buf[3] = (arg >> 8) & 0xFF;              // argument bits 15-8
    buf[4] = arg & 0xFF;                     // argument bits 7-0
    buf[5] = cmd == 0 ? 0x95 : cmd == 8 ? 0x87 : 0xFF;  // crc for cmd0, cmd8, or dummy
    gpio_put(PIN_CS, 0);                     // select sd card
    printf("sending cmd%d: 0x%02x%02x%02x%02x%02x\n", cmd, buf[0], buf[1], buf[2], buf[3], buf[4]);
    spi_write_blocking(SPI_PORT, buf, 6);    // send command

    // wait for response (up to 2 seconds)
    uint32_t start = time_us_32();
    do {
        spi_read_blocking(SPI_PORT, 0xFF, &resp, 1);
        if ((time_us_32() - start) > 2000000) {
            gpio_put(PIN_CS, 1);
            printf("send_cmd(%d) timeout, resp: 0x%02x\n", cmd, resp);
            return 0xFF;                     // return error on timeout
        }
    } while (resp & 0x80);                   // wait until response is not busy

    printf("send_cmd(%d) response: 0x%02x\n", cmd, resp);
    return resp;
}

// initialize sd card
DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0) {
        printf("disk_initialize: invalid drive number %d\n", pdrv);
        return STA_NOINIT;                   // only drive 0 supported
    }

    // initialize spi at 400 khz for stable communication
    printf("initializing spi at 400 khz\n");
    spi_init(SPI_PORT, 400000);
    spi_set_format(SPI_PORT, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);  // 8-bit, mode 0, msb first
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);  // configure miso pin
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);   // configure clock pin
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);  // configure mosi pin
    gpio_init(PIN_CS);                           // initialize chip select
    gpio_set_dir(PIN_CS, GPIO_OUT);              // set chip select as output
    gpio_put(PIN_CS, 1);                         // set chip select high (inactive)

    // send 80 clock cycles with cs high to enter spi mode
    BYTE dummy[10] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    printf("sending 80 clock cycles\n");
    spi_write_blocking(SPI_PORT, dummy, 10);
    sleep_ms(10);                            // delay after clock cycles

    // send cmd0 (reset to idle state)
    if (send_cmd(0, 0) != 0x01) {
        gpio_put(PIN_CS, 1);
        printf("cmd0 failed\n");
        return STA_NOINIT;
    }
    sleep_ms(10);

    // send cmd8 (interface condition, check voltage, 0x1aa for 2.7-3.6v)
    BYTE resp_buf[4];
    if (send_cmd(8, 0x1AA) != 0x01) {
        gpio_put(PIN_CS, 1);
        printf("cmd8 failed\n");
        return STA_NOINIT;
    }
    spi_read_blocking(SPI_PORT, 0xFF, resp_buf, 4);  // read r7 response
    if (resp_buf[3] != 0xAA) {
        gpio_put(PIN_CS, 1);
        printf("cmd8 invalid response: 0x%02x\n", resp_buf[3]);
        return STA_NOINIT;                   // check pattern must be 0xaa
    }
    sleep_ms(10);

    // send acmd41 (initialize with hcs bit for sdhc/sdxc)
    uint32_t start = time_us_32();
    BYTE resp;
    do {
        resp = send_cmd(55, 0);              // app_cmd for acmd41
        if (resp > 0x01) {
            gpio_put(PIN_CS, 1);
            printf("cmd55 failed: 0x%02x\n", resp);
            return STA_NOINIT;
        }
        resp = send_cmd(41, 0x40000000);     // acmd41 with hcs (high capacity support)
        if ((time_us_32() - start) > 2000000) {
            gpio_put(PIN_CS, 1);
            printf("acmd41 timeout\n");
            return STA_NOINIT;
        }
        sleep_ms(10);
    } while (resp != 0x00);                  // wait for initialization complete

    // send cmd58 (read ocr to confirm addressing mode)
    if (send_cmd(58, 0) == 0x00) {
        spi_read_blocking(SPI_PORT, 0xFF, resp_buf, 4);
        printf("ocr: 0x%02x%02x%02x%02x\n", resp_buf[0], resp_buf[1], resp_buf[2], resp_buf[3]);
    } else {
        printf("cmd58 failed\n");
    }

    // set block length to 512 bytes (cmd16)
    if (send_cmd(16, 512) != 0x00) {
        gpio_put(PIN_CS, 1);
        printf("cmd16 failed\n");
        return STA_NOINIT;
    }

    // increase spi speed to 4 mhz for faster data transfer
    printf("increasing spi speed to 4 mhz\n");
    spi_init(SPI_PORT, 4000000);

    sd_disk_status &= ~STA_NOINIT;           // mark disk as initialized
    gpio_put(PIN_CS, 1);
    printf("sd card initialized successfully\n");
    return sd_disk_status;
}

// get disk status
DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;        // only drive 0 supported
    return sd_disk_status;
}

// read sector(s) from sd card
DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count) {
    if (pdrv != 0 || !count) return RES_PARERR;  // validate drive and count
    if (sd_disk_status & STA_NOINIT) return RES_NOTRDY;  // check if initialized

    BYTE cmd[6];
    for (UINT i = 0; i < count; i++) {
        // send read_single_block command (cmd17)
        cmd[0] = 0x40 | 17;
        cmd[1] = (sector >> 24) & 0xFF;
        cmd[2] = (sector >> 16) & 0xFF;
        cmd[3] = (sector >> 8) & 0xFF;
        cmd[4] = sector & 0xFF;
        cmd[5] = 0xFF;                      // dummy crc
        gpio_put(PIN_CS, 0);
        spi_write_blocking(SPI_PORT, cmd, 6);

        // wait for response (0x00 indicates success)
        BYTE resp;
        uint32_t start = time_us_32();
        do {
            spi_read_blocking(SPI_PORT, 0xFF, &resp, 1);
            if ((time_us_32() - start) > 1000000) {
                gpio_put(PIN_CS, 1);
                printf("disk_read cmd17 timeout\n");
                return RES_ERROR;
            }
        } while (resp != 0x00);

        // wait for data token (0xfe)
        start = time_us_32();
        do {
            spi_read_blocking(SPI_PORT, 0xFF, &resp, 1);
            if ((time_us_32() - start) > 1000000) {
                gpio_put(PIN_CS, 1);
                printf("disk_read data token timeout\n");
                return RES_ERROR;
            }
        } while (resp != 0xFE);

        // read 512-byte sector
        spi_read_blocking(SPI_PORT, 0xFF, buff + i * 512, 512);
        // discard 2-byte crc
        BYTE crc[2];
        spi_read_blocking(SPI_PORT, 0xFF, crc, 2);
        gpio_put(PIN_CS, 1);
        sector++;
    }
    return RES_OK;
}

// write sector(s) to sd card (stub, not implemented)
DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count) {
    return RES_WRPRT;  // return write-protected for simplicity
}

// handle miscellaneous fatfs control commands
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    if (pdrv != 0) return RES_PARERR;        // only drive 0 supported
    if (sd_disk_status & STA_NOINIT) return RES_NOTRDY;  // check if initialized

    switch (cmd) {
        case CTRL_SYNC:
            return RES_OK;                   // sync operation (no-op for sd card)
        case GET_SECTOR_COUNT:
            *(LBA_t*)buff = 0x100000;       // dummy sector count (placeholder)
            return RES_OK;
        case GET_SECTOR_SIZE:
            *(WORD*)buff = 512;             // standard sd card sector size
            return RES_OK;
        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1;              // block size (1 sector)
            return RES_OK;
        default:
            return RES_PARERR;              // unsupported command
    }
}
