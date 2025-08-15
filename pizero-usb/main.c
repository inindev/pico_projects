//
// Copyright (C) 2025, John Clark <inindev@gmail.com>
//

#include <pico/stdlib.h>
#include <stdio.h>
#include <tusb.h>
#include <stdbool.h>
#include "pico/time.h"
#include "class/hid/hid.h"
#include "hardware/uart.h"
#include "bsp/board.h" // waveshare_rp2350_pizero.h

#define LED_PIN 25
#define UART_TX_PIN 4
#define UART_RX_PIN 5

bool keyboard_connected = false;
static hid_keyboard_report_t prev_report = {0};


// redirect printf to uart1
int __io_putchar(int ch) {
    uart_putc(uart1, ch);
    return ch;
}

// check if a keycode is in the report
bool has_keycode(hid_keyboard_report_t *report, uint8_t code) {
    for (int i = 0; i < 6; i++) {
        if (report->keycode[i] == code) return true;
    }
    return false;
}

// callback: hid device mounted
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len) {
    if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD) {
        keyboard_connected = true;
        gpio_put(LED_PIN, 1);  // turn led on
        printf("keyboard connected (addr: %d, instance: %d)\n", dev_addr, instance);
    }
    tuh_hid_receive_report(dev_addr, instance); // Request report data
}

// callback: hid device unmounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD) {
        keyboard_connected = false;
        gpio_put(LED_PIN, 0);  // turn led off
        printf("keyboard disconnected\n");
    }
}

// callback: hid report received
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len) {
    if (keyboard_connected && tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD) {
        hid_keyboard_report_t *cur = (hid_keyboard_report_t *)report;
        for (int i = 0; i < 6; i++) {
            uint8_t key = cur->keycode[i];
            if (key && !has_keycode(&prev_report, key)) {
                printf("key code %u pressed\n", key);
            }
        }
        prev_report = *cur; // update previous state
        tuh_hid_receive_report(dev_addr, instance); // request next report
    }
}

int main() {
    // initialize uart1 for serial output
    stdio_uart_init_full(uart1, 115200, UART_TX_PIN, UART_RX_PIN);
    setvbuf(stdout, NULL, _IONBF, 0); // ensure unbuffered output

    // initialize board hardware (usb setup via waveshare_rp2350_pizero.h)
    board_init();

    // initialize LED for status indication
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 0);

    // initialize TinyUSB host on port 0
    tuh_init(0);
    printf("pizero-usb started\n");

    // main loop
    bool led_state = false;
    for(uint32_t count = 0; ; count++) {
        tuh_task(); // process usb events (detection, enumeration, reports)

        if(count % 50 == 0) {
            if (!keyboard_connected) {
                led_state = !led_state;
                gpio_put(LED_PIN, led_state);
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
