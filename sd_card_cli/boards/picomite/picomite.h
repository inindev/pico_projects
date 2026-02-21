/*
 * Copyright (c) 2025 UKTailwind
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

// -----------------------------------------------------
// NOTE: THIS HEADER IS ALSO INCLUDED BY ASSEMBLER SO
//       SHOULD ONLY CONSIST OF PREPROCESSOR DIRECTIVES
// -----------------------------------------------------

// Board: PicoUSBMotherboardV1.1 (SchematicV1.2)
// A Pico 2 (RP2350A) based motherboard with HDMI (DVI via HSTX),
// SD card (SPI), USB console (CH340C), USB hub (CH334F),
// I2C RTC (DS3231), PWM audio, and external I/O header.

// This header may be included by other board headers as "boards/picomite.h"

pico_board_cmake_set(PICO_PLATFORM, rp2350)

#ifndef _BOARDS_PICOMITE_H
#define _BOARDS_PICOMITE_H

// On some samples, the xosc can take longer to stabilize than is usual
#ifndef PICO_XOSC_STARTUP_DELAY_MULTIPLIER
#define PICO_XOSC_STARTUP_DELAY_MULTIPLIER 64
#endif

// For board detection
#define PICOMITE

// --- RP2350 VARIANT ---
// Pico 2 module uses RP2350A (30 GPIO)
#define PICO_RP2350A 1

// --- BOARD SPECIFIC ---

// --- HSTX / DVI (HDMI) ---
#define PICOMITE_DVI_D0P_PIN 12
#define PICOMITE_DVI_D0N_PIN 13
#define PICOMITE_DVI_CKP_PIN 14
#define PICOMITE_DVI_CKN_PIN 15
#define PICOMITE_DVI_D2P_PIN 16
#define PICOMITE_DVI_D2N_PIN 17
#define PICOMITE_DVI_D1P_PIN 18
#define PICOMITE_DVI_D1N_PIN 19

// --- PWM Audio (analog via LC filter to 3.5mm jack) ---
#define PICOMITE_AUDIO_L_PIN 10
#define PICOMITE_AUDIO_R_PIN 11

// --- USB Console (CH340C USB-UART) ---
#define PICOMITE_CONSOLE_TX_PIN 8
#define PICOMITE_CONSOLE_RX_PIN 9

// --- I2C0 (DS3231 RTC) ---
#define PICOMITE_I2C_SDA_PIN 20
#define PICOMITE_I2C_SCL_PIN 21

// --- SD Card (SPI mode, active jumper config) ---
#define PICOMITE_SD_SCK_PIN 26
#define PICOMITE_SD_MOSI_PIN 27
#define PICOMITE_SD_MISO_PIN 28
#define PICOMITE_SD_CS_PIN 22

// --- USB Hub (CH334F, 4-port) ---
// Connected via Pico 2 native USB (not directly GPIO-mapped)

// --- External I/O Header (U23 2x12 + H1 3x3) ---
// GP0, GP1, GP2, GP3, GP4, GP5, GP6, GP7, GP23 exposed
// GP26, GP27, GP28 also exposed (directly or via jumpers)

// --- Reset ---
// RUN pin connected to MAX809R supervisor + pushbutton (SW15)

// --- UART ---
// Default UART is UART1 on GP8/GP9 (USB console via CH340C)
#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 1
#endif

#ifndef PICO_DEFAULT_UART_TX_PIN
#define PICO_DEFAULT_UART_TX_PIN PICOMITE_CONSOLE_TX_PIN
#endif

#ifndef PICO_DEFAULT_UART_RX_PIN
#define PICO_DEFAULT_UART_RX_PIN PICOMITE_CONSOLE_RX_PIN
#endif

// --- I2C ---
#ifndef PICO_DEFAULT_I2C
#define PICO_DEFAULT_I2C 0
#endif

#ifndef PICO_DEFAULT_I2C_SDA_PIN
#define PICO_DEFAULT_I2C_SDA_PIN PICOMITE_I2C_SDA_PIN
#endif

#ifndef PICO_DEFAULT_I2C_SCL_PIN
#define PICO_DEFAULT_I2C_SCL_PIN PICOMITE_I2C_SCL_PIN
#endif

// --- SPI ---
// Default SPI is SPI1 for the SD card
#ifndef PICO_DEFAULT_SPI
#define PICO_DEFAULT_SPI 1
#endif

#ifndef PICO_DEFAULT_SPI_SCK_PIN
#define PICO_DEFAULT_SPI_SCK_PIN PICOMITE_SD_SCK_PIN
#endif

#ifndef PICO_DEFAULT_SPI_TX_PIN
#define PICO_DEFAULT_SPI_TX_PIN PICOMITE_SD_MOSI_PIN
#endif

#ifndef PICO_DEFAULT_SPI_RX_PIN
#define PICO_DEFAULT_SPI_RX_PIN PICOMITE_SD_MISO_PIN
#endif

// --- SD Card ---
#ifndef PICO_SD_CLK_PIN
#define PICO_SD_CLK_PIN PICOMITE_SD_SCK_PIN
#endif

#ifndef PICO_SD_CMD_PIN
#define PICO_SD_CMD_PIN PICOMITE_SD_MOSI_PIN
#endif

#ifndef PICO_SD_DAT0_PIN
#define PICO_SD_DAT0_PIN PICOMITE_SD_MISO_PIN
#endif

// --- LED ---
// No dedicated LED on the motherboard; leave undefined
// Users can define PICO_DEFAULT_LED_PIN if needed

// --- FLASH ---
// Pico 2 module has 4MB flash (W25Q032)
#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (4 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (4 * 1024 * 1024)
#endif

pico_board_cmake_set_default(PICO_RP2350_A2_SUPPORTED, 1)
#ifndef PICO_RP2350_A2_SUPPORTED
#define PICO_RP2350_A2_SUPPORTED 1
#endif

#endif
