//
// Copyright (c) 2025 John Clark <inindev@gmail.com>
//

#include <stdio.h>
#include <tusb.h>
#include <class/hid/hid.h>
#include "keyboard.h"

bool keyboard_connected = false;
static hid_keyboard_report_t prev_report = {0};
static bool caps_lock = false;    // track caps lock state
static bool num_lock = false;     // track num lock state
static bool scroll_lock = false;  // track scroll lock state


// check if a keycode is in the report
bool has_keycode(hid_keyboard_report_t *report, uint8_t code) {
    for (int i = 0; i < 6; i++) {
        if (report->keycode[i] == code) return true;
    }
    return false;
}

// map keycode to character using HID_KEYCODE_TO_ASCII
static char keycode_to_char(uint8_t keycode, bool shift) {
    static const char keycode_to_ascii[][2] = {
        HID_KEYCODE_TO_ASCII
    };
    if (keycode >= sizeof(keycode_to_ascii) / sizeof(keycode_to_ascii[0])) {
        return '\0'; // out of range
    }
    // letters (a-z) respect caps lock
    if (keycode >= HID_KEY_A && keycode <= HID_KEY_Z) {
        bool upper = (shift != caps_lock); // xor: upper if shift or caps lock (but not both)
        return keycode_to_ascii[keycode][upper ? 1 : 0];
    }
    // other keys use shift state directly
    return keycode_to_ascii[keycode][shift ? 1 : 0];
}

// get modifier names as string
static void print_modifiers(uint8_t modifier) {
    bool first = true;
    if (modifier & KEYBOARD_MODIFIER_LEFTCTRL) {
        printf("LeftCtrl");
        first = false;
    }
    if (modifier & KEYBOARD_MODIFIER_RIGHTCTRL) {
        printf("%sRightCtrl", first ? "" : "+");
        first = false;
    }
    if (modifier & KEYBOARD_MODIFIER_LEFTSHIFT) {
        printf("%sLeftShift", first ? "" : "+");
        first = false;
    }
    if (modifier & KEYBOARD_MODIFIER_RIGHTSHIFT) {
        printf("%sRightShift", first ? "" : "+");
        first = false;
    }
    if (modifier & KEYBOARD_MODIFIER_LEFTALT) {
        printf("%sLeftAlt", first ? "" : "+");
        first = false;
    }
    if (modifier & KEYBOARD_MODIFIER_RIGHTALT) {
        printf("%sRightAlt", first ? "" : "+");
        first = false;
    }
    if (modifier & KEYBOARD_MODIFIER_LEFTGUI) {
        printf("%sLeftGUI", first ? "" : "+");
        first = false;
    }
    if (modifier & KEYBOARD_MODIFIER_RIGHTGUI) {
        printf("%sRightGUI", first ? "" : "+");
    }
}

// callback: hid device mounted
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len) {
    if (tuh_hid_interface_protocol(dev_addr, instance) == HID_ITF_PROTOCOL_KEYBOARD) {
        keyboard_connected = true;
        caps_lock = false; // reset caps lock state
        num_lock = false; // reset num lock state
        scroll_lock = false; // reset scroll lock state
        // send initial led report
        uint8_t led_report = 0;
        tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT, &led_report, sizeof(led_report));
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
        bool shift = cur->modifier & (KEYBOARD_MODIFIER_LEFTSHIFT | KEYBOARD_MODIFIER_RIGHTSHIFT);
        for (int i = 0; i < 6; i++) {
            uint8_t key = cur->keycode[i];
            if (key && !has_keycode(&prev_report, key)) {

                if (key == HID_KEY_CAPS_LOCK) {
                    caps_lock = !caps_lock; // toggle caps lock state
                    // send led report to toggle caps lock led
                    uint8_t led_report = (caps_lock ? KEYBOARD_LED_CAPSLOCK : 0) |
                                         (num_lock ? KEYBOARD_LED_NUMLOCK : 0) |
                                         (scroll_lock ? KEYBOARD_LED_SCROLLLOCK : 0);
                    tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT, &led_report, sizeof(led_report));
                    printf("key code %u (Caps Lock, %s)\n", key, caps_lock ? "on" : "off");
                }

                else if (key == HID_KEY_NUM_LOCK) {
                    num_lock = !num_lock; // toggle num lock state
                    // send led report to toggle num lock led
                    uint8_t led_report = (caps_lock ? KEYBOARD_LED_CAPSLOCK : 0) |
                                         (num_lock ? KEYBOARD_LED_NUMLOCK : 0) |
                                         (scroll_lock ? KEYBOARD_LED_SCROLLLOCK : 0);
                    tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT, &led_report, sizeof(led_report));
                    printf("key code %u (Num Lock, %s)\n", key, num_lock ? "on" : "off");
                }

                else if (key == HID_KEY_SCROLL_LOCK) {
                    scroll_lock = !scroll_lock; // toggle scroll lock state
                    // send led report to toggle scroll lock led
                    uint8_t led_report = (caps_lock ? KEYBOARD_LED_CAPSLOCK : 0) |
                                         (num_lock ? KEYBOARD_LED_NUMLOCK : 0) |
                                         (scroll_lock ? KEYBOARD_LED_SCROLLLOCK : 0);
                    tuh_hid_set_report(dev_addr, instance, 0, HID_REPORT_TYPE_OUTPUT, &led_report, sizeof(led_report));
                    printf("key code %u (Scroll Lock, %s)\n", key, scroll_lock ? "on" : "off");
                }

                else if (key >= HID_KEY_F1 && key <= HID_KEY_F12) {
                    printf("key code %u (F%d)", key, key - HID_KEY_F1 + 1);
                    if (cur->modifier) {
                        printf(" modifiers: ");
                        print_modifiers(cur->modifier);
                    }
                    printf("\n");
                }

                else if (key >= HID_KEY_F13 && key <= HID_KEY_F24) {
                    printf("key code %u (F%d)", key, key - HID_KEY_F13 + 13);
                    if (cur->modifier) {
                        printf(" modifiers: ");
                        print_modifiers(cur->modifier);
                    }
                    printf("\n");
                }

                else if (key == HID_KEY_DELETE) {
                    printf("key code %u (Delete)", key, key - HID_KEY_F1 + 1);
                    if (cur->modifier) {
                        printf(" modifiers: ");
                        print_modifiers(cur->modifier);
                    }
                    printf("\n");
                }

                else if (key >= HID_KEY_ARROW_RIGHT && key <= HID_KEY_ARROW_UP) {
                    const char *names[] = {"Right", "Left", "Down", "Up"};
                    printf("key code %u (%s)", key, names[key - HID_KEY_ARROW_RIGHT]);
                    if (cur->modifier) {
                        printf(" modifiers: ");
                        print_modifiers(cur->modifier);
                    }
                    printf("\n");
                }

                else {
                    printf("key code %u", key);
                    char ch = keycode_to_char(key, shift);
                    if (ch) {
                        if (ch == '\r') {
                            printf(" (Enter)");
                        } else if (ch == '\t') {
                            printf(" (Tab)");
                        } else if (ch == '\b') {
                            printf(" (Backspace)");
                        } else if (ch == ' ') {
                            printf(" (Space)");
                        } else if (ch == '\e') {
                            printf(" (Esc)");
                        } else {
                            printf(" ('%c')", ch);
                        }
                    }
                    if (cur->modifier) {
                        printf(" modifiers: ");
                        print_modifiers(cur->modifier);
                    }
                    printf("\n");
                }
            }
        }

        prev_report = *cur; // update previous state
        tuh_hid_receive_report(dev_addr, instance); // request next report
    }
}
