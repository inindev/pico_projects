# pico_projects/pico_hdmi/CMakeLists.txt
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

project(pico_hdmi C CXX ASM)
pico_sdk_init()

add_executable(pico_hdmi
    pico_hdmi.c
)

# include directories
target_include_directories(pico_hdmi PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/images
    ${PICO_SDK_PATH}/src/rp2350/hardware_structs/include
    ${PICO_SDK_PATH}/src/rp2350/hardware_regs/include
)

# enable usb output, disable uart
pico_enable_stdio_usb(pico_hdmi 1)
pico_enable_stdio_uart(pico_hdmi 0)

# set printf implementation
pico_set_printf_implementation(pico_hdmi compiler)

# compile options
target_compile_options(pico_hdmi PRIVATE
    -O3
    -Wall
)

# link libraries
target_link_libraries(pico_hdmi
    pico_stdlib
    pico_multicore
    hardware_dma
    pico_sync
)

# generate extra outputs
pico_add_extra_outputs(pico_hdmi)
