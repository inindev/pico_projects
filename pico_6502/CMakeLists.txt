#
# Copyright (c) 2026 John Clark <inindev@gmail.com>
#
cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{PICO_SDK_PATH})
  message(FATAL_ERROR "PICO_SDK_PATH not defined. Set it with: export PICO_SDK_PATH=/path/to/pico-sdk")
endif()
if(NOT EXISTS $ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
  message(FATAL_ERROR "PICO_SDK_PATH invalid or SDK outdated. Ensure it points to a valid Pico SDK directory with external/pico_sdk_import.cmake")
endif()

set(PICO_PLATFORM rp2350)
set(PICO_BOARD waveshare_rp2350_pizero)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

# pull in pico sdk (classic import style)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_6502 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# hagl hal library
add_library(hagl_hal INTERFACE)
target_sources(hagl_hal INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/ili9488/hagl_hal.c)
target_include_directories(hagl_hal INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/ili9488)
target_link_libraries(hagl_hal INTERFACE pico_stdlib hardware_spi)

# hagl library
add_subdirectory(ili9488/hagl)

add_executable(
    pico_6502
    main.cpp
    ili9488/hagl_hal.c
)

target_link_libraries(
    pico_6502
    pico_stdlib
    hardware_pwm
    hardware_spi
    hagl
)

# create map/bin/hex/uf2 files
pico_add_extra_outputs(pico_6502)
