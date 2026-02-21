# Test suite for SD card driver (SDIO or SPI)
# Builds a standalone firmware that runs on the RP2350 and exercises
# the SD card driver and FatFs integration.

add_executable(sd_card_tests
    sd_card_tests.c
    ${CMAKE_SOURCE_DIR}/${HW_CONFIG}
)

pico_enable_stdio_usb(sd_card_tests 0)
pico_enable_stdio_uart(sd_card_tests 1)

target_compile_options(sd_card_tests PRIVATE -O2 -Wall -Wextra)

target_link_libraries(sd_card_tests
    pico-fatfs-sd
    pico_stdlib
    hardware_gpio
)

pico_add_extra_outputs(sd_card_tests)
