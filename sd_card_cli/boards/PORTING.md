# Porting pico-pc to a New Board

This document describes the process used to port pico-pc from the Adafruit Fruit Jam (RP2350B) to the PicoMite (RP2350A), and serves as a guide for porting to other RP2350-based boards.

## Overview

pico-pc has four hardware-dependent subsystems. Each one needs to be adapted when changing boards:

| Subsystem | Key files | What varies between boards |
|-----------|-----------|---------------------------|
| **Board definition** | `CMakeLists.txt`, `boards/<board>.h` | GPIO assignments, flash size, default peripherals |
| **HDMI/DVI** | `src/hal/hdmi_hal.c` | HSTX differential pair pin mapping, polarity |
| **Audio** | `src/hal/audio_hal.c`, `src/hal/audio_hal.h` | Output method (PWM, I2S+codec, etc.), pins |
| **SD card** | *(currently removed)* | Interface (SPI vs SDIO), pins, library choice |

Everything else (console, display, USB keyboard, palette, fonts) is hardware-independent and needs no changes.

## Step 1: Board Header

Create `boards/<boardname>.h` following the pico-sdk board header convention. This file defines:

- GPIO pin assignments for all peripherals
- Default UART, I2C, SPI peripheral selections
- Flash size and boot stage2 configuration
- Board detection macro

Example structure (from `boards/picomite.h`):

```c
pico_board_cmake_set(PICO_PLATFORM, rp2350)

#ifndef _BOARDS_PICOMITE_H
#define _BOARDS_PICOMITE_H

// Board detection
#define PICOMITE

// Pin definitions
#define PICOMITE_DVI_D0P_PIN 12
#define PICOMITE_DVI_CKP_PIN 14
// ... etc

// Pico SDK defaults (UART, I2C, SPI, flash)
#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 1
#endif
// ... etc

#endif
```

**Important**: The header is also included by assembler, so it must contain only preprocessor directives.

## Step 2: CMakeLists.txt

Change the board selection and header search path:

```cmake
set(PICO_BOARD <boardname>)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards)
```

The SDK will look for `<boardname>.h` in the `boards/` directory.

Update HAL_SOURCES to add or remove board-specific source files (e.g., remove `fruit_jam.cpp` if the board has no buttons/NeoPixels, remove `hw_config.c` if no SD card).

Update link libraries to match the hardware used:
- **PWM audio**: needs `hardware_pwm`, does not need `hardware_pio` or `hardware_i2c`
- **I2S audio**: needs `hardware_pio` and `hardware_i2c` (for codec)
- **SD card (SPI)**: needs `hardware_spi` and an SPI-capable FatFS library
- **SD card (SDIO)**: needs `hardware_pio` and the `pico-fatfs-sd` library

## Step 3: HDMI (hdmi_hal.c)

HSTX on RP2350 always uses GPIO 12-19 (four differential pairs), but boards differ in which pair carries the clock vs. data lanes.

Edit the `dvi_serialiser_cfg` struct:

```c
static const struct dvi_serialiser_cfg picomite_cfg = {
    .sm_tmds = {0, 1, 2},
    .pins_tmds = {D0_P_PIN, D1_P_PIN, D2_P_PIN},  // TMDS data lane base pins
    .pins_clk = CLK_P_PIN,                          // TMDS clock base pin
    .invert_diffpairs = true,                        // may need adjustment per PCB
};

#define DVI_CFG picomite_cfg
```

**How to determine pin mapping**: The board schematic shows which GPIO pairs connect to which HDMI/DVI lanes. TMDS lane 0 = blue, lane 1 = green, lane 2 = red. The P pin is always the even GPIO in a pair.

**invert_diffpairs**: When `true`, only the P pin is configured and the HSTX hardware handles the N pin complement. This is the common case. If the display shows no signal or garbled colors, try flipping this value.

Remove any board-specific `#include` (e.g., `boards/adafruit_fruit_jam.h`) and replace with your board header if you reference its pin defines.

## Step 4: Audio (audio_hal.c)

This is the subsystem most likely to differ between boards. The public API in `audio_hal.h` stays consistent:

```c
void audio_hal_init(uint32_t sample_rate);
void audio_hal_start(void);
void audio_hal_stop(void);
void audio_hal_set_callback(audio_fill_callback_t callback);
void audio_hal_set_volume(uint8_t level);
void audio_hal_mute(bool mute);
void audio_hal_beep(void);
```

### PWM audio (PicoMite approach)

Used when the board has an LC filter on PWM pins going to a 3.5mm jack.

- Configure two GPIO pins as PWM (left + right channel on the same slice)
- PWM wrap = `sys_clock / sample_rate` (gives ~12-bit resolution at 270 MHz / 44.1 kHz)
- DMA feeds 32-bit values to the PWM CC register (channel A = low 16 bits, channel B = high 16 bits)
- Volume control via software amplitude scaling before PWM conversion
- No PIO or I2C needed

### I2S + codec audio (Fruit Jam approach)

Used when the board has an I2S DAC (e.g., TLV320DAC3100).

- PIO runs an I2S program (DIN, BCLK, WS pins)
- PWM generates MCLK for the codec
- I2C configures the codec (PLL, clock dividers, output routing, volume)
- DMA feeds 32-bit stereo samples (16-bit left + 16-bit right) to PIO TX FIFO
- Volume control via codec registers
- Requires `hardware_pio`, `hardware_i2c`, and the I2S PIO program

### Adding a new audio backend

1. Copy the existing `audio_hal.c` as a starting point
2. Replace the init/start/stop internals with your hardware setup
3. Keep the DMA double-buffer pattern (ping-pong with chain-to)
4. Keep the `audio_fill_callback_t` interface — the rest of the system uses it
5. Use DMA_IRQ_2 for audio (DMA_IRQ_0 is used by HDMI)
6. Add or remove functions from `audio_hal.h` as needed (e.g., headphone detect only makes sense with a codec)

## Step 5: SD Card

The SD card subsystem is currently removed from the PicoMite build. When adding it back:

### SDIO (Fruit Jam approach)
- Uses the `pico-fatfs-sd` library (SDIO only, trimmed fork)
- Requires consecutive GPIO pins for D0-D3, with CLK at a fixed offset from D0
- Uses one PIO instance and DMA_IRQ_1
- Configure in `hw_config.c` with `sd_sdio_if_t` struct

### SPI (PicoMite approach — not yet implemented)
- Requires an SPI-capable FatFS library (e.g., the upstream [no-OS-FatFS-SD-SDIO-SPI-RPi-Pico](https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico))
- Uses hardware SPI with CS, SCK, MOSI, MISO pins
- Configure in `hw_config.c` with `sd_spi_if_t` struct

In either case, update `main.cpp` to include the FatFS headers and add the mount/detect code back.

## Step 6: Board-Specific Peripherals

If your board has peripherals not present on other boards (buttons, LEDs, NeoPixels, etc.):

1. Create a board peripheral HAL (e.g., `src/hal/<board>.cpp`)
2. Add it to HAL_SOURCES in CMakeLists.txt
3. Call its init/task functions from `main.cpp`
4. Add any needed PIO programs and link libraries

## Step 7: main.cpp

Update main.cpp to match the board's capabilities:
- Add/remove `#include` directives for board-specific HALs
- Add/remove init calls and main loop tasks
- Add/remove SD card mount code

## Quick Checklist

- [ ] Board header in `boards/<boardname>.h`
- [ ] `PICO_BOARD` and `PICO_BOARD_HEADER_DIRS` set in CMakeLists.txt
- [ ] HAL_SOURCES updated (add/remove board-specific files)
- [ ] Link libraries updated (add/remove hardware modules)
- [ ] PIO header generation updated (add/remove .pio files)
- [ ] `hdmi_hal.c` DVI pin config matches schematic
- [ ] `audio_hal.c` matches board's audio output hardware
- [ ] SD card library and config match board's SD interface
- [ ] `main.cpp` init sequence matches available peripherals
- [ ] Clean build with zero warnings
