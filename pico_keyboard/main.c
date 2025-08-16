//
// Copyright (c) 2025 John Clark <inindev@gmail.com>
//

#include <pico/stdlib.h>
#include <stdio.h>
#include <tusb.h>
#include <stdbool.h>
#include <pico/time.h>
#include <hardware/uart.h>
#include <bsp/board.h>
#include "keyboard.h"


int main() {
    // initialize board hardware (usb setup via waveshare_rp2350_pizero.h)
    board_init();

    // initialize led for status indication
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, 0);

    // initialize tinyusb host on port 0
    tuh_init(0);
    printf("pizero-usb started\n");

    // main loop
    bool led_state = false;
    for(uint32_t count = 0; ; count++) {
        tuh_task(); // process usb events (detection, enumeration, reports)

        if(count % 50 == 0) {
            if (!keyboard_connected) {
                led_state = !led_state;
                gpio_put(PICO_DEFAULT_LED_PIN, led_state);
                if(led_state) {
                    printf("led on ");
                }
                else {
                    printf("-> led off %d\n", count / 100);
                }
            }
            else if (count % 1000 == 0) {
                printf("running... %d\n", count / 100);
            }
        }

        sleep_ms(10);
    }

    return 0;
}
