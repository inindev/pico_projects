# SD Card CLI for RP2350-Based Boards

An interactive SD card CLI built on a fork of carlk3's
[no-OS-FatFS-SD-SDIO-SPI-RPi-Pico](https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)
library. The fork is patched for RP2350B extended GPIO support (pins 32-47),
upgraded to FatFs R0.16, and supports multiple boards via SDIO or SPI.

**Supported boards:**

| Board | Chip | SD Interface |
|-------|------|-------------|
| Adafruit Fruit Jam | RP2350B | SDIO (4-bit) |
| PicoMite | RP2350A | SPI |

## Table of Contents

1. [Fork Overview](#1-fork-overview)
2. [Library Structure](#2-library-structure)
3. [Background: The RP2350B GPIO Problem](#3-background-the-rp2350b-gpio-problem)
4. [Required Patches (5 fixes)](#4-required-patches-5-fixes)
5. [Hardware Configuration (hw_config.c)](#5-hardware-configuration-hw_configc)
6. [FatFs Configuration (ffconf.h)](#6-fatfs-configuration-ffconfh)
7. [CMake Integration](#7-cmake-integration)
8. [API Reference: carlk3 vs fatfs-pico-sdio](#8-api-reference-carlk3-vs-fatfs-pico-sdio)
9. [PIO Resource Usage](#9-pio-resource-usage)
10. [Gotchas and Lessons Learned](#10-gotchas-and-lessons-learned)
11. [Building and Flashing](#11-building-and-flashing)

---

## 1. Fork Overview

This is a trimmed fork of carlk3's library. Changes from upstream:

- **Removed:** C++ wrappers (FatFsSd, FsLib, iostream), PlatformIO support,
  ZuluSCSI/mbed-os/SdFat stubs, all examples
- **Patched:** 5 fixes for RP2350B extended GPIO support (GPIOs 32-47)
- **Upgraded:** FatFs R0.15 → R0.16, relocated to `lib/fatfs/`
- **Multi-board:** Supports SDIO (Fruit Jam) and SPI (PicoMite) via
  board-specific `hw_config.c` files under `boards/`

All patches are backward-compatible with RP2040.

---

## 2. Library Structure

```
pico-fatfs-sd/
├── LICENSE                              # Apache 2.0 (carlk3)
├── README.md
├── lib/
│   └── fatfs/                           # FatFs R0.16 (elm-chan)
│       ├── LICENSE.txt                  # BSD-style
│       ├── documents/                   # elm-chan docs
│       └── source/
│           ├── ff.c, ff.h               # FatFs core
│           ├── ffconf.h.stock           # stock config (renamed, not used)
│           ├── diskio.c, diskio.h       # disk I/O interface
│           ├── ffsystem.c, ffunicode.c
└── src/
    ├── CMakeLists.txt                   # build target definition
    ├── include/                         # library headers + config overrides
    │   ├── ffconf.h                     # ** customized FatFs config **
    │   ├── hw_config.h                  # user must implement these functions
    │   ├── f_util.h, crash.h, crc.h
    │   ├── delays.h, my_debug.h, my_rtc.h
    │   ├── sd_timeouts.h, util.h
    ├── sd_driver/                       # SD card drivers
    │   ├── sd_card.c, sd_card.h         # top-level sd_card_t API
    │   ├── sd_card_constants.h, sd_regs.h
    │   ├── sd_timeouts.c
    │   ├── dma_interrupts.c, dma_interrupts.h
    │   ├── SDIO/
    │   │   ├── rp2040_sdio.c            # PIO SDIO driver (patched)
    │   │   ├── rp2040_sdio.h
    │   │   ├── rp2040_sdio.pio          # PIO assembly source
    │   │   ├── sd_card_sdio.c           # SDIO card init/read/write (patched)
    │   │   └── SdioCard.h               # SDIO function declarations
    │   └── SPI/
    │       ├── my_spi.c                 # SPI bus driver
    │       ├── sd_card_spi.c            # SPI card init/read/write
    │       └── sd_spi.c                 # low-level SPI commands
    └── src/                             # glue + utilities
        ├── glue.c                       # FatFs ↔ sd_card_t bridge
        ├── f_util.c                     # FRESULT_str()
        ├── crash.c, crc.c
        └── my_debug.c, my_rtc.c, util.c
```

**Key design:** The customized `src/include/ffconf.h` overrides the stock FatFs
config. The stock `ffconf.h` in `lib/fatfs/source/` is renamed to
`ffconf.h.stock` so the compiler finds the custom one first via the include path.

---

## 3. Background: The RP2350B GPIO Problem

The Adafruit Fruit Jam uses the RP2350B, which has 48 GPIOs (0-47). Its SD card
slot is wired to extended GPIOs:

| Signal | GPIO |
|--------|------|
| CD     | 33   |
| CLK    | 34   |
| CMD    | 35   |
| D0     | 36   |
| D1     | 37   |
| D2     | 38   |
| D3     | 39   |

The RP2350's PIO blocks each have a 32-pin window. By default this window covers
GPIOs 0-31, which means GPIOs 34-39 are invisible to PIO. The Pico SDK provides
`pio_set_gpio_base(pio, 16)` to shift the window to GPIOs 16-47.

The upstream carlk3 library was written for the RP2040 (GPIOs 0-29) and does not
account for this. Five bugs must be patched.

---

## 4. Required Patches (5 fixes)

All patches are in two files under `src/sd_driver/SDIO/`.

### Patch 1: Set PIO GPIO base (`rp2040_sdio.c`, ~line 784)

**Problem:** PIO defaults to GPIO window 0-31. Pins 34-39 are unreachable.

**Fix:** Before `pio_clear_instruction_memory()`, add:

```c
/* RP2350B: If SDIO pins are above GPIO 31, shift the PIO GPIO window so
   that the 32-pin PIO address space covers the actual pins.  gpio_base=16
   maps PIO pins 0-31 -> GPIOs 16-47. */
uint gpio_base = (SDIO_D0 >= 32) ? 16 : 0;
pio_set_gpio_base(SDIO_PIO, gpio_base);
```

**Why 16?** The PIO window is 32 pins wide. Setting base=16 covers GPIOs 16-47,
which includes both the SDIO pins (34-39) and leaves room for lower pins if needed.
The SDK only allows base values that are multiples of 16.

### Patch 2: Fix `input_sync_bypass` overflow (`rp2040_sdio.c`, ~line 827)

**Problem:** The original code does `1 << SDIO_CLK` where SDIO_CLK=34. This
overflows a 32-bit register (undefined behavior in C).

**Original:**
```c
SDIO_PIO->input_sync_bypass |= (1 << SDIO_CLK) | (1 << SDIO_CMD) | ...
```

**Fix:** Subtract `gpio_base` to get PIO-relative pin numbers:

```c
{
    uint gb = pio_get_gpio_base(SDIO_PIO);
    SDIO_PIO->input_sync_bypass |=
        (1u << (SDIO_CLK - gb)) | (1u << (SDIO_CMD - gb)) |
        (1u << (SDIO_D0 - gb))  | (1u << (SDIO_D1 - gb))  |
        (1u << (SDIO_D2 - gb))  | (1u << (SDIO_D3 - gb));
}
```

**Why:** The `input_sync_bypass` register is indexed by PIO-relative pin number
(0-31), not absolute GPIO number. With gpio_base=16, GPIO 34 is PIO pin 18.

### Patch 3: Fix CLK pin calculation (`sd_card_sdio.c`, ~line 648)

**Problem:** The CLK pin is at a fixed PIO offset from D0. The original code
computes `CLK = (D0 + 30) % 32`. With D0=36: `(36+30) % 32 = 2` (GPIO 2, wrong).

**Original:**
```c
sd_card_p->sdio_if_p->CLK_gpio =
    (sd_card_p->sdio_if_p->D0_gpio + SDIO_CLK_PIN_D0_OFFSET) % 32;
```

**Fix:** Do modular arithmetic in PIO-pin space:

```c
uint gpio_base = (sd_card_p->sdio_if_p->D0_gpio >= 32) ? 16 : 0;
sd_card_p->sdio_if_p->CLK_gpio = gpio_base +
    ((sd_card_p->sdio_if_p->D0_gpio - gpio_base + SDIO_CLK_PIN_D0_OFFSET) % 32);
```

**Math:** `SDIO_CLK_PIN_D0_OFFSET` = 30 (defined in `rp2040_sdio.pio`), which is
effectively -2 mod 32. With gpio_base=16, D0=36:
`16 + ((36-16+30) % 32)` = `16 + (50 % 32)` = `16 + 18` = **34** (correct).

### Patch 4: Fix `sd_sdio_isBusy()` (`sd_card_sdio.c`, ~line 231)

**Problem:** Uses `sio_hw->gpio_in` which is a 32-bit register covering GPIOs 0-31
only. `1 << 36` is undefined behavior.

**Original:**
```c
return (sio_hw->gpio_in & (1 << sd_card_p->sdio_if_p->D0_gpio)) == 0;
```

**Fix:** Use the SDK's `gpio_get()` which handles extended GPIOs:

```c
return !gpio_get(sd_card_p->sdio_if_p->D0_gpio);
```

### Patch 5: Fix hard-coded `GPIO_FUNC_PIO1` (`sd_card_sdio.c`, ~line 554)

**Problem:** `sd_sdio_init()` always passes `GPIO_FUNC_PIO1` to `gpio_conf()`,
which breaks if PIO0 is used instead.

**Original:**
```c
gpio_conf(sd_card_p->sdio_if_p->CLK_gpio, GPIO_FUNC_PIO1, ...);
```

**Fix:** Derive the function from which PIO block is configured:

```c
gpio_function_t fn = (pio1 == sd_card_p->sdio_if_p->SDIO_PIO)
                         ? GPIO_FUNC_PIO1 : GPIO_FUNC_PIO0;
gpio_conf(sd_card_p->sdio_if_p->CLK_gpio, fn, ...);
gpio_conf(sd_card_p->sdio_if_p->CMD_gpio, fn, ...);
gpio_conf(sd_card_p->sdio_if_p->D0_gpio,  fn, ...);
// ... D1, D2, D3 likewise
```

---

## 5. Hardware Configuration (hw_config.c)

The library requires a user-provided `hw_config.c` that implements two functions:
`sd_get_num()` and `sd_get_by_num()`. These tell the library how many SD cards
exist and how each is wired.

```c
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
```

### Key configuration notes

- **Only set CMD_gpio and D0_gpio.** CLK, D1, D2, D3 are auto-calculated by
  `sd_sdio_ctor()` based on PIO pin offset constraints. Setting them to non-zero
  triggers an assertion failure.

- **Card detect polarity:** On the Fruit Jam, the CD pin (GPIO 33) reads **1**
  when a card is present (active-high). Set `card_detected_true = 1`. Getting this
  wrong causes "no SD card" errors even with a card inserted.

- **Baud rate:** The RP2350 defaults to `clk_sys` = 150 MHz. Dividing by 6 gives
  25 MHz, which is the SD card standard speed. The library accepts the raw baud
  rate value, not a divisor.

- **PIO selection:** We use `pio1` here. If your application uses PIO1 for
  something else, you can use `pio0` instead (Patch 5 ensures this works).

---

## 6. FatFs Configuration (ffconf.h)

FatFs R0.16 is in `lib/fatfs/source/`. Its stock `ffconf.h` has been renamed to
`ffconf.h.stock` so the compiler doesn't find it. Instead, the customized config
at `src/include/ffconf.h` is used (found first via the include path order).

### Key customizations (differing from R0.16 stock defaults)

| Setting | Value | Stock | Why |
|---------|-------|-------|-----|
| `FF_USE_FIND` | 1 | 0 | Enable `f_findfirst()`/`f_findnext()` |
| `FF_USE_MKFS` | 1 | 0 | Enable `f_mkfs()` for formatting |
| `FF_USE_FASTSEEK` | 1 | 0 | Fast seek support |
| `FF_USE_EXPAND` | 1 | 0 | Enable `f_expand()` |
| `FF_USE_STRFUNC` | 1 | 0 | Enable `f_gets()`/`f_printf()` etc. |
| `FF_PRINT_LLI` | 1 | 0 | `f_printf()` long long support |
| `FF_PRINT_FLOAT` | 1 | 0 | `f_printf()` float support |
| `FF_STRF_ENCODE` | 3 | 0 | String I/O as UTF-8 |
| `FF_CODE_PAGE` | 437 | 932 | U.S. code page (not Japanese) |
| `FF_USE_LFN` | 3 | 0 | Long file names, heap-allocated buffer |
| `FF_LFN_UNICODE` | 2 | 0 | UTF-8 API encoding |
| `FF_FS_RPATH` | 2 | 0 | Relative paths + `f_getcwd()` |
| `FF_VOLUMES` | 4 | 1 | Support up to 4 volumes |
| `FF_LBA64` | 1 | 0 | 64-bit LBA for large cards |
| `FF_FS_EXFAT` | 1 | 0 | exFAT support |
| `FF_FS_LOCK` | 16 | 0 | File lock for 16 simultaneous opens |
| `FF_USE_CHMOD` | 0 | 0 | Disabled (same as stock) |

### Upgrading FatFs

To upgrade FatFs in the future:
1. Replace the contents of `lib/fatfs/source/` with the new release
2. Rename the new stock `ffconf.h` to `ffconf.h.stock`
3. Update `FFCONF_DEF` in `src/include/ffconf.h` to match the new `FF_DEFINED`
   value in `ff.h`
4. Add any new config options from the stock `ffconf.h` to the custom one

---

## 7. CMake Integration

### Root CMakeLists.txt

The root `CMakeLists.txt` uses `TARGET_BOARD` to select between boards. Each
board sets `PICO_BOARD` and points to the correct `hw_config.c`:

```cmake
cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Board selection: cmake -DTARGET_BOARD=<board> ..
#   fruit_jam  - Adafruit Fruit Jam (RP2350B, SDIO) [default]
#   picomite   - PicoMite (RP2350A, SPI)
if(NOT DEFINED TARGET_BOARD)
    set(TARGET_BOARD "fruit_jam")
endif()

if(TARGET_BOARD STREQUAL "fruit_jam")
    set(PICO_BOARD adafruit_fruit_jam)
    set(HW_CONFIG boards/fruit_jam/hw_config.c)
elseif(TARGET_BOARD STREQUAL "picomite")
    set(PICO_BOARD picomite)
    set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards/picomite)
    set(HW_CONFIG boards/picomite/hw_config.c)
endif()

set(PICO_PLATFORM rp2350)

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
project(sd_card_cli C CXX ASM)
pico_sdk_init()

add_subdirectory(pico-fatfs-sd/src build_fatfs)

add_executable(sd_card_cli
    sd_card_cli.c
    ${HW_CONFIG}
)

pico_enable_stdio_usb(sd_card_cli 0)
pico_enable_stdio_uart(sd_card_cli 1)

target_link_libraries(sd_card_cli
    pico-fatfs-sd
    pico_stdlib
    hardware_gpio
)

pico_add_extra_outputs(sd_card_cli)
```

### Key CMake points

- **Board selection:** Pass `-DTARGET_BOARD=fruit_jam` or
  `-DTARGET_BOARD=picomite` at configure time. Defaults to `fruit_jam`.

- **Per-board hw_config:** Each board has its own `hw_config.c` under
  `boards/<board>/`. The `HW_CONFIG` variable selects the correct one.

- **`add_subdirectory` path:** Point to `pico-fatfs-sd/src` (not the repo
  root). The second argument `build_fatfs` is the binary directory.

- **Link target name:** `pico-fatfs-sd` (the library's CMake target). This
  pulls in all include paths automatically.

- **No extra include directories needed.** The library target exports its own
  include paths via `target_link_libraries`.

- **Include path order matters.** The library's CMakeLists.txt lists `include`
  before `../lib/fatfs/source` so the custom `ffconf.h` is found before the
  stock one (which is renamed anyway as a safety measure).

---

## 8. API Reference: carlk3 vs fatfs-pico-sdio

If migrating from another library (e.g., inindev's fatfs-pico-sdio), here's the
mapping:

### Initialization

| fatfs-pico-sdio | carlk3 |
|-----------------|--------|
| `sd_init_4pins()` | Automatic. `f_mount(&fs, "0:", 1)` triggers `disk_initialize()` which calls `sd_card_p->init()` |
| `sd_deinit()` | `sd_card_p->deinit(sd_card_p)` where `sd_card_p = sd_get_by_num(0)` |

### Card queries

| fatfs-pico-sdio | carlk3 |
|-----------------|--------|
| `sd_is_initialized()` | `!(sd_card_p->state.m_Status & STA_NOINIT)` |
| `sd_get_sector_count()` | `sd_card_p->get_num_sectors(sd_card_p)` |
| `sd_get_csd_raw(buf)` | `memcpy(buf, sd_card_p->state.CSD, 16)` (available after mount) |
| `sd_get_bus_width()` | No equivalent (always 4-bit in SDIO mode) |

### FatFs layer

The FatFs API (`f_mount`, `f_open`, `f_read`, `f_write`, `f_close`, `f_mkdir`,
`f_unlink`, `f_stat`, `f_opendir`, `f_readdir`, `f_getfree`, `disk_ioctl`) is
**unchanged** between libraries. This fork uses FatFs R0.16.

### Headers

```c
#include "sd_card.h"     // carlk3 sd card driver
#include "hw_config.h"   // sd_get_by_num(), sd_get_num()
#include "f_util.h"      // FRESULT_str() - human-readable error strings
#include "ff.h"          // FatFs API
#include "diskio.h"      // disk_ioctl() etc.
```

### Important: f_chmod is disabled

The custom `ffconf.h` sets `FF_USE_CHMOD 0`, which means `f_chmod()` is not
available. If your code uses `f_chmod()` (e.g., to clear read-only before delete),
remove those calls or set `FF_USE_CHMOD` to 1 in `src/include/ffconf.h`.

---

## 9. PIO Resource Usage

The SDIO driver uses:

| Resource | Usage |
|----------|-------|
| PIO block | 1 (PIO1 in our config) |
| State machines | 2 of 4 (SM0: CMD+CLK, SM1: DATA) |
| DMA channels | 2 (allocated at runtime) |
| DMA IRQ | 1 (DMA_IRQ_1 in our config) |
| PIO instruction memory | Fully used (3 programs loaded after `pio_clear_instruction_memory`) |

**Sharing considerations:** The driver calls `pio_clear_instruction_memory()` on
init, so no other PIO programs can coexist on the same PIO block. 2 of 4 state
machines remain free, but there's no instruction memory left for them. Use the
other PIO block (PIO0) for any additional PIO peripherals.

---

## 10. Gotchas and Lessons Learned

1. **Card detect polarity varies by board.** The Fruit Jam's CD pin reads HIGH
   when a card is present. Other boards may be active-low. Always verify with
   `gpio_get(33)` before configuring `card_detected_true`.

2. **CLK/D1-D3 must be zero in hw_config.** The library auto-calculates them from
   D0. Non-zero values trigger `myASSERT()` failures.

3. **The library initializes the card lazily.** There is no explicit init call.
   The first `f_mount(&fs, "0:", 1)` (with the force-mount flag = 1) triggers
   `disk_initialize()` -> `sd_card_p->init()`. This means CSD/CID data and sector
   count are only available **after** a successful mount.

4. **Reinit after deinit:** Call `sd_card_p->deinit(sd_card_p)`, then
   `f_mount(&fs, "0:", 1)`. The mount triggers reinitialization automatically.

5. **The `SDIO_CLK_PIN_D0_OFFSET` constant (30)** is defined in `rp2040_sdio.pio`
   and represents -2 in mod-32 arithmetic. This is a hardware constraint of the
   PIO program: CLK must be 2 pins below D0 in the PIO pin space.

6. **RP2040 compatibility:** All patches use conditional checks
   (`D0_gpio >= 32 ? 16 : 0`), so patched code still works on RP2040 boards
   where GPIOs are 0-29. The gpio_base stays 0 and behavior is unchanged.

7. **ffconf.h override mechanism:** The stock `ffconf.h` in `lib/fatfs/source/`
   must be renamed (to `ffconf.h.stock`) because `ff.h` does
   `#include "ffconf.h"` and the compiler checks the same directory first. The
   custom config in `src/include/ffconf.h` is found via the include path.

8. **`dma_interrupts.c` needs `<hardware/dma.h>`** explicitly. Upstream got it
   transitively through SPI headers; after stripping SPI, the direct include is
   required.

---

## 11. Building and Flashing

The project supports multiple board targets via the `TARGET_BOARD` CMake variable.
Each board gets its own build directory to allow side-by-side builds.

```bash
# Set SDK path
export PICO_SDK_PATH=/path/to/pico-sdk

# Fruit Jam (default)
cmake -B build-fruitjam -DTARGET_BOARD=fruit_jam .
make -C build-fruitjam -j$(sysctl -n hw.ncpu)

# PicoMite
cmake -B build-picomite -DTARGET_BOARD=picomite .
make -C build-picomite -j$(sysctl -n hw.ncpu)

# Outputs (per build directory):
#   build-<board>/sd_card_cli.uf2      - Interactive CLI
#   build-<board>/tests/sd_card_tests.uf2  - Automated test suite
```

**Flash:** Hold BOOTSEL, plug in USB, copy the `.uf2` file to the RPI-RP2 drive.

**Serial console:** Connect to the Fruit Jam's UART0 (GPIO 44 TX, GPIO 45 RX) at
115200 baud. The CLI presents a `> ` prompt with commands like `mount`, `umount`,
`ls`, `cat`, `write`, `cp`, `rm`, `mkdir`, `info`, `csd`, and `format`.
