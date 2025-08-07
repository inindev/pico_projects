# pico_projects/text_display/CMakeLists.txt
#
# this example requires an external digital video connector connected to
# gpios 12 through 19 (hstx-capable gpios) with appropriate
# current-limiting resistors, e.g., 270 ohms. the pinout matches the
# pico dvi sock board: https://github.com/wren6991/pico-dvi-sock

cmake_minimum_required(VERSION 3.13)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_PLATFORM rp2350)
set(PICO_BOARD pico2)

# verify PICO_SDK_PATH
if(NOT DEFINED ENV{PICO_SDK_PATH})
  message(FATAL_ERROR "PICO_SDK_PATH not defined. Set it with: export PICO_SDK_PATH=/path/to/pico-sdk")
endif()
if(NOT EXISTS $ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
  message(FATAL_ERROR "PICO_SDK_PATH invalid. Ensure it points to a valid Pico SDK directory.")
endif()

# pull in pico sdk
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(text_display C CXX ASM)
pico_sdk_init()

add_executable(text_display
    text_display.c
)

# include directories
target_include_directories(text_display PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/images
    ${PICO_SDK_PATH}/src/rp2350/hardware_structs/include
    ${PICO_SDK_PATH}/src/rp2350/hardware_regs/include
)

# define boot stage
pico_define_boot_stage2(slower_boot2 ${PICO_DEFAULT_BOOT_STAGE2_FILE})
target_compile_definitions(slower_boot2 PRIVATE PICO_FLASH_SPI_CLKDIV=4)
pico_set_boot_stage2(text_display slower_boot2)

# enable usb output, disable uart
pico_enable_stdio_usb(text_display 1)
pico_enable_stdio_uart(text_display 0)

# set printf implementation
pico_set_printf_implementation(text_display compiler)

# compile options
target_compile_options(text_display PRIVATE
    -DPICOMITE
    -DNDEBUG
    -Drp2350
    -DPICO_XOSC_STARTUP_DELAY_MULTIPLIER=64
    -DPICO_FLASH_SIZE_BYTES=4194304
    -O3
    -Wall
)

# link libraries
target_link_libraries(text_display
    pico_stdlib
    pico_multicore
    hardware_dma
    pico_sync
)

# generate extra outputs
pico_add_extra_outputs(text_display)
