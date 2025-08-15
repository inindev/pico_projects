#
# Copyright (c) 2025 John Clark <inindev@gmail.com>
#
cmake_minimum_required(VERSION 3.13)

# set pico platform and board
set(PICO_PLATFORM rp2350)
set(PICO_BOARD waveshare_rp2350_pizero)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_SOURCE_DIR})

# verify pico sdk path
if(NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "PICO_SDK_PATH not defined. Set it with: export PICO_SDK_PATH=/path/to/pico-sdk")
endif()
if(NOT EXISTS $ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
    message(FATAL_ERROR "PICO_SDK_PATH invalid. Ensure it points to a valid Pico SDK directory.")
endif()

# include pico sdk
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

# project definition
project(pizero-kbd C CXX ASM)

# set c/c++ standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# initialize pico sdk
pico_sdk_init()

# define executable
add_executable(pizero-kbd
    main.c
    keyboard.c
    ${TINYUSB_SRC}
)

# define compile-time options
target_compile_definitions(pizero-kbd PRIVATE
    PICO_RP2350B=1
    LED_PIN=PICO_DEFAULT_LED_PIN
    PICO_STDIO_UART=PICO_DEFAULT_UART
    UART_TX_PIN=PICO_DEFAULT_UART_TX_PIN
    UART_RX_PIN=PICO_DEFAULT_UART_RX_PIN
    UART_BAUD_RATE=115200
    #UART_BAUD_RATE=1500000
)
if(PICO_DEFAULT_UART EQUAL 0)
    target_compile_definitions(pizero-kbd PRIVATE UART_PORT=uart0)
else()
    target_compile_definitions(pizero-kbd PRIVATE UART_PORT=uart1)
endif()

# set build type
set(CMAKE_BUILD_TYPE Release)

# tinyusb source files
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

# specify include directories
target_include_directories(pizero-kbd PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${PICO_SDK_PATH}/lib/tinyusb/src
    ${PICO_SDK_PATH}/lib/tinyusb/hw
    ${PICO_SDK_PATH}/lib/tinyusb/hw/bsp/rp2040
)

# link libraries
target_link_libraries(pizero-kbd
    pico_stdlib
    pico_stdio_uart
    tinyusb_host
    tinyusb_board
)

# configure stdio: enable uart, disable usb
pico_enable_stdio_usb(pizero-kbd 0)
pico_enable_stdio_uart(pizero-kbd 1)

# generate uf2 output file
pico_add_extra_outputs(pizero-kbd)
