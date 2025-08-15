cmake_minimum_required(VERSION 3.13)

set(PICO_PLATFORM rp2350)
set(PICO_BOARD waveshare_rp2350_pizero) # Custom board name
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "PICO_SDK_PATH not defined. Set it with: export PICO_SDK_PATH=/path/to/pico-sdk")
endif()
if(NOT EXISTS $ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
    message(FATAL_ERROR "PICO_SDK_PATH invalid. Ensure it points to a valid Pico SDK directory.")
endif()

include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(pizero-usb C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

# TinyUSB source files
set(TINYUSB_SRC
    ${PICO_SDK_PATH}/lib/tinyusb/src/tusb.c
    ${PICO_SDK_PATH}/lib/tinyusb/src/common/tusb_fifo.c
    ${PICO_SDK_PATH}/lib/tinyusb/src/host/hub.c
    ${PICO_SDK_PATH}/lib/tinyusb/src/host/usbh.c
    ${PICO_SDK_PATH}/lib/tinyusb/src/class/hid/hid_host.c
    ${PICO_SDK_PATH}/lib/tinyusb/src/portable/raspberrypi/rp2040/hcd_rp2040.c
    ${PICO_SDK_PATH}/lib/tinyusb/src/portable/raspberrypi/rp2040/rp2040_usb.c
    ${PICO_SDK_PATH}/lib/tinyusb/hw/bsp/rp2040/family.c
)

add_executable(pizero-usb
    main.c
    ${TINYUSB_SRC}
)

# include directories
target_include_directories(pizero-usb PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${PICO_SDK_PATH}/lib/tinyusb/src
    ${PICO_SDK_PATH}/lib/tinyusb/hw
    ${PICO_SDK_PATH}/lib/tinyusb/hw/bsp/rp2040
)

# libraries
target_link_libraries(pizero-usb
    pico_stdlib
    pico_stdio_uart
    tinyusb_host
    tinyusb_board
)

# enable uart output, disable usb stdio
pico_enable_stdio_usb(pizero-usb 0)
pico_enable_stdio_uart(pizero-usb 1)

# generate uf2 file
pico_add_extra_outputs(pizero-usb)

# define compile options
target_compile_definitions(pizero-usb PRIVATE
    PICO_RP2350B=1
    PICO_STDIO_UART=1
)

set(CMAKE_BUILD_TYPE Release)
