# sd_card_cli - SD card CLI for RP2350-based boards
#
# Uses pico-fatfs-sd (fork of carlk3's library) for SD card access.
# Supports both SDIO (Fruit Jam) and SPI (PicoMite, Waveshare PiZero) interfaces.
#
# Board selection: cmake -DTARGET_BOARD=<board> ..
#   fruit_jam        - Adafruit Fruit Jam (RP2350B, SDIO) [default]
#   picomite         - PicoMite (RP2350A, SPI)
#   waveshare_pizero - Waveshare RP2350-PiZero (RP2350B, SPI)

cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# board selection
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
elseif(TARGET_BOARD STREQUAL "waveshare_pizero")
    set(PICO_BOARD waveshare_rp2350_pizero)
    set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_LIST_DIR}/boards/waveshare_rp2350_pizero)
    set(HW_CONFIG boards/waveshare_rp2350_pizero/hw_config.c)
else()
    message(FATAL_ERROR "Unknown TARGET_BOARD: ${TARGET_BOARD}. Use 'fruit_jam', 'picomite', or 'waveshare_pizero'.")
endif()

set(PICO_PLATFORM rp2350)

# verify PICO_SDK_PATH
if(NOT DEFINED ENV{PICO_SDK_PATH})
  message(FATAL_ERROR "PICO_SDK_PATH not defined. Set it with: export PICO_SDK_PATH=/path/to/pico-sdk")
endif()
if(NOT EXISTS $ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
  message(FATAL_ERROR "PICO_SDK_PATH invalid. Ensure it points to a valid Pico SDK directory.")
endif()

# pull in pico sdk
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

# define project name and languages
project(sd_card_cli C CXX ASM)

# initialize pico sdk
pico_sdk_init()

# carlk3 FatFS + SD library
add_subdirectory(pico-fatfs-sd/src build_fatfs)

# define executable and source files
add_executable(sd_card_cli
    sd_card_cli.c
    ${HW_CONFIG}
)

# enable uart0 only
pico_enable_stdio_usb(sd_card_cli 0)
pico_enable_stdio_uart(sd_card_cli 1)

# optimization and warnings
target_compile_options(sd_card_cli PRIVATE -O2 -Wall -Wextra)

# link libraries
target_link_libraries(sd_card_cli
    pico-fatfs-sd
    pico_stdlib
    hardware_gpio
)

# generate uf2 file for flashing
pico_add_extra_outputs(sd_card_cli)

# test suite
add_subdirectory(tests)
