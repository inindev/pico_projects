//
// Copyright (c) 2025 John Clark <inindev@gmail.com>
//

#include <stdio.h>
#include <stdbool.h>
#include <tusb.h>
#include <class/hid/hid.h>
#include "keyboard.h"

bool keyboard_connected = false;
static hid_keyboard_report_t prev_report = {0};

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
        gpio_put(LED_PIN, 1); // turn led on
        printf("keyboard connected (addr: %d, instance: %d)\n", dev_addr, instance);
    }
    tuh_hid_receive_report(dev_addr, instance); // request report data
}

// callback: hid device unmounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD) {
        keyboard_connected = false;
        gpio_put(LED_PIN, 0); // turn led off
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
