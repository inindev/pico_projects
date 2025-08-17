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

# libdvi
add_subdirectory(libdvi)

add_executable(text_display
    text_display.c
    dhgr_patterns.h
)

# include directories
target_include_directories(text_display PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/libdvi
)

# link libraries
target_link_libraries(text_display
    pico_stdlib
    pico_multicore
    hardware_dma
    libdvi  # PicoDVI library
)

# define dvi-specific compile options
target_compile_definitions(text_display PRIVATE
    DVI_DEFAULT_SERIAL_CONFIG=pico2_dvi_config
    PICO_SCANVIDEO_COLOR_PIN_COUNT=4  # 4 bpp
    PICO_SCANVIDEO_PIXEL_WIDTH=720
    PICO_SCANVIDEO_PIXEL_HEIGHT=480
)

# enable usb output, disable uart
pico_enable_stdio_usb(text_display 1)
pico_enable_stdio_uart(text_display 0)

# generate extra outputs
pico_add_extra_outputs(text_display)
