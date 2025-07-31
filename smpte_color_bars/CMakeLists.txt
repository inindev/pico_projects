
# pico_projects/smpte_color_bars/CMakeLists.txt
#
# builds the SMPTE color bars project for raspberry pi pico or pico 2.
# this project generates SMPTE color bars on an hdmi display
# using the PicoDVI library and an adafruit hdmi sock.
# license: mit (see license file in repository root).

cmake_minimum_required(VERSION 3.13)

# set PICO_SDK_PATH from environment variable if not provided
if (NOT PICO_SDK_PATH)
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    if (NOT PICO_SDK_PATH)
        message(FATAL_ERROR "PICO_SDK_PATH not set in environment or CMake command")
    endif()
endif()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(smpte_color_bars C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# default to rp2040 if PICO_PLATFORM not specified
if (NOT PICO_PLATFORM)
    set(PICO_PLATFORM rp2040)
endif()

pico_sdk_init()

add_compile_options(-Wall)

add_executable(smpte_color_bars
    smpte_color_bars.c
    pico_dvi/libdvi/dvi.c
    pico_dvi/libdvi/dvi_serialiser.c
    pico_dvi/libdvi/dvi_timing.c
    pico_dvi/libdvi/tmds_encode.c
    pico_dvi/libdvi/tmds_encode.S
)

target_include_directories(smpte_color_bars PRIVATE
    pico_dvi/include
    pico_dvi/libdvi
    .
)

target_compile_definitions(smpte_color_bars PRIVATE
    DVI_DEFAULT_SERIAL_CONFIG=pico_sock_cfg
    DVI_N_TMDS_BUFFERS=5
)

pico_generate_pio_header(smpte_color_bars ${CMAKE_CURRENT_LIST_DIR}/pico_dvi/libdvi/dvi_serialiser.pio)
pico_generate_pio_header(smpte_color_bars ${CMAKE_CURRENT_LIST_DIR}/pico_dvi/libdvi/tmds_encode_1bpp.pio)

target_link_libraries(smpte_color_bars PRIVATE
    pico_stdlib
    pico_multicore
    hardware_dma
    hardware_interp
    hardware_pio
    hardware_pwm
    pico_util
)

pico_add_extra_outputs(smpte_color_bars)
