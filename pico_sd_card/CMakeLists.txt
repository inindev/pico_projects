# pico_projects/pico_sd_card/CMakeLists.txt
#
# builds the sd card cli project for raspberry pi pico or pico 2.
# this project creates a command-line interface to interact with an sd card
# via spi, including commands like ls, cat, and led control.
# license: mit (see license file in repository root).

cmake_minimum_required(VERSION 3.13)

# set path to pico sdk (adjust if different)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# define project name and languages
project(pico_sd_card C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize pico sdk
pico_sdk_init()

# define executable and source files
add_executable(pico_sd_card
    sd_card_cli.c
    sd_card_diskio.c
    ${PICO_SDK_PATH}/lib/tinyusb/lib/fatfs/source/ff.c
    ${PICO_SDK_PATH}/lib/tinyusb/lib/fatfs/source/ffsystem.c
    ${PICO_SDK_PATH}/lib/tinyusb/lib/fatfs/source/ffunicode.c
)

# enable usb stdio, disable uart
pico_enable_stdio_usb(pico_sd_card 1)
pico_enable_stdio_uart(pico_sd_card 0)

# include fatfs headers (adjust path if fatfs is elsewhere)
target_include_directories(pico_sd_card PRIVATE
    ${PICO_SDK_PATH}/lib/tinyusb/lib/fatfs/source
)

# link pico standard library and spi hardware support
target_link_libraries(pico_sd_card
    pico_stdlib
    hardware_spi
)

# generate uf2 file for flashing
pico_add_extra_outputs(pico_sd_card)

# set pico board (pico for rp2040, pico2 for rp2350, pico_w for rp2040 with wifi)
set(PICO_BOARD pico)

# define compile-time constants
target_compile_definitions(pico_sd_card PRIVATE
    PICO_DEFAULT_LED_PIN=25  # onboard led pin (gp25 for pico and pico 2)
)

# set optimization level
target_compile_options(pico_sd_card PRIVATE -O2)
