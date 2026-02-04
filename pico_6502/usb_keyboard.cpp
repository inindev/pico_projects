//
// USB HID Keyboard Host for RP2350
//
// Copyright 2026, John Clark
//

#include "usb_keyboard.h"
#include "tusb.h"
#include "pico/stdlib.h"

// Circular buffer for keyboard input
#define KB_BUFFER_SIZE 32
static volatile uint8_t kb_buffer[KB_BUFFER_SIZE];
static volatile uint8_t kb_head = 0;
static volatile uint8_t kb_tail = 0;
static volatile uint8_t last_key = 0;

// Key repeat configuration (in milliseconds)
#define REPEAT_DELAY_MS   400   // Initial delay before repeat starts
#define REPEAT_RATE_MS    50    // Interval between repeats

// Key repeat state
static uint8_t repeat_key = 0;          // HID scan code of held key
static uint8_t repeat_char = 0;         // ASCII character to repeat
static uint32_t repeat_start_ms = 0;    // When key was first pressed
static uint32_t repeat_last_ms = 0;     // When last repeat was sent
static bool repeat_active = false;      // Has initial delay passed?

// HID keyboard scan code to ASCII lookup table (US layout)
// Index is HID scan code, value is ASCII character (lowercase)
static const uint8_t hid_to_ascii[128] = {
    0,    0,    0,    0,   'a',  'b',  'c',  'd',  // 0x00-0x07
    'e',  'f',  'g',  'h',  'i',  'j',  'k',  'l',  // 0x08-0x0F
    'm',  'n',  'o',  'p',  'q',  'r',  's',  't',  // 0x10-0x17
    'u',  'v',  'w',  'x',  'y',  'z',  '1',  '2',  // 0x18-0x1F
    '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',  // 0x20-0x27
    '\r', 0x1B, '\b', '\t', ' ',  '-',  '=',  '[',  // 0x28-0x2F (Enter, Esc, Backspace, Tab, Space, -, =, [)
    ']',  '\\', 0,    ';',  '\'', '`',  ',',  '.',  // 0x30-0x37
    '/',  0,    0,    0,    0,    0,    0,    0,    // 0x38-0x3F (/, CapsLock, F1-F5)
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x40-0x47 (F6-F12, PrintScr)
    0,    0,    0,    0,    0,    0,    0, 0x94,    // 0x48-0x4F (0x4F=Right arrow)
 0x93, 0x92, 0x91,    0,    '/',  '*',  '-',  '+',  // 0x50-0x57 (0x50=Left, 0x51=Down, 0x52=Up, then keypad)
    '\r', '1',  '2',  '3',  '4',  '5',  '6',  '7',  // 0x58-0x5F (keypad)
    '8',  '9',  '0',  '.',  0,    0,    0,    0,    // 0x60-0x67 (keypad)
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x68-0x6F
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x70-0x77
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x78-0x7F
};

// Shifted characters (with Shift held)
static const uint8_t hid_to_ascii_shift[128] = {
    0,    0,    0,    0,   'A',  'B',  'C',  'D',  // 0x00-0x07
    'E',  'F',  'G',  'H',  'I',  'J',  'K',  'L',  // 0x08-0x0F
    'M',  'N',  'O',  'P',  'Q',  'R',  'S',  'T',  // 0x10-0x17
    'U',  'V',  'W',  'X',  'Y',  'Z',  '!',  '@',  // 0x18-0x1F
    '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',  // 0x20-0x27
    '\r', 0x1B, '\b', '\t', ' ',  '_',  '+',  '{',  // 0x28-0x2F
    '}',  '|',  0,    ':',  '"',  '~',  '<',  '>',  // 0x30-0x37
    '?',  0,    0,    0,    0,    0,    0,    0,    // 0x38-0x3F
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x40-0x47
    0,    0,    0,    0,    0,    0,    0, 0x94,    // 0x48-0x4F (0x4F=Right arrow)
 0x93, 0x92, 0x91,    0,    '/',  '*',  '-',  '+',  // 0x50-0x57 (arrows + keypad)
    '\r', '1',  '2',  '3',  '4',  '5',  '6',  '7',  // 0x58-0x5F
    '8',  '9',  '0',  '.',  0,    0,    0,    0,    // 0x60-0x67
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x68-0x6F
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x70-0x77
    0,    0,    0,    0,    0,    0,    0,    0,    // 0x78-0x7F
};

// Track previously pressed keys to detect new key presses
static uint8_t prev_keys[6] = {0};

// Add character to input buffer
static void kb_buffer_put(uint8_t ch) {
    uint8_t next = (kb_head + 1) % KB_BUFFER_SIZE;
    if (next != kb_tail) {  // Buffer not full
        kb_buffer[kb_head] = ch;
        kb_head = next;
        last_key = ch;
    }
}

// Check if key code is in the array
static bool key_in_array(uint8_t key, const uint8_t* arr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (arr[i] == key) return true;
    }
    return false;
}

void usb_keyboard_init(void) {
    tusb_init();
}

void usb_keyboard_task(void) {
    tuh_task();

    // Handle key repeat
    if (repeat_char != 0) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        if (!repeat_active) {
            // Check if initial delay has passed
            if (now - repeat_start_ms >= REPEAT_DELAY_MS) {
                repeat_active = true;
                repeat_last_ms = now;
                kb_buffer_put(repeat_char);
            }
        } else {
            // Check if it's time for another repeat
            if (now - repeat_last_ms >= REPEAT_RATE_MS) {
                repeat_last_ms = now;
                kb_buffer_put(repeat_char);
            }
        }
    }
}

bool usb_keyboard_available(void) {
    return kb_head != kb_tail;
}

uint8_t usb_keyboard_getchar(void) {
    if (kb_head == kb_tail) return 0;
    uint8_t ch = kb_buffer[kb_tail];
    kb_tail = (kb_tail + 1) % KB_BUFFER_SIZE;
    return ch;
}

uint8_t usb_keyboard_peek(void) {
    return last_key;
}

void usb_keyboard_clear(void) {
    kb_head = kb_tail = 0;
    last_key = 0;
}

//--------------------------------------------------------------------
// TinyUSB Callbacks
//--------------------------------------------------------------------

// Invoked when device with HID interface is mounted
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    (void)desc_report;
    (void)desc_len;

    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    // Request to receive report - only for keyboard (protocol 1)
    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        if (!tuh_hid_receive_report(dev_addr, instance)) {
            // Failed to request report
        }
    }
}

// Invoked when device with HID interface is unmounted
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    (void)dev_addr;
    (void)instance;
    // Clear state
    for (int i = 0; i < 6; i++) prev_keys[i] = 0;
    repeat_key = 0;
    repeat_char = 0;
    repeat_active = false;
}

// Invoked when HID report is received
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD && len >= 8) {
        // Standard keyboard report format:
        // Byte 0: Modifier keys (Ctrl, Shift, Alt, GUI)
        // Byte 1: Reserved
        // Bytes 2-7: Key codes (up to 6 simultaneous keys)
        uint8_t modifier = report[0];
        const uint8_t* keys = &report[2];

        bool shift = (modifier & 0x22) != 0;  // Left or Right Shift

        // Check if the repeat key is still held
        if (repeat_key != 0 && !key_in_array(repeat_key, keys, 6)) {
            // Key was released, stop repeat
            repeat_key = 0;
            repeat_char = 0;
            repeat_active = false;
        }

        // Process each key in the report
        for (int i = 0; i < 6; i++) {
            uint8_t key = keys[i];
            if (key == 0) continue;  // No key

            // Check if this is a new key press (not in previous report)
            if (!key_in_array(key, prev_keys, 6)) {
                // Convert scan code to ASCII
                uint8_t ch = 0;
                if (key < 128) {
                    ch = shift ? hid_to_ascii_shift[key] : hid_to_ascii[key];
                }
                if (ch != 0) {
                    kb_buffer_put(ch);

                    // Start key repeat tracking for this key
                    repeat_key = key;
                    repeat_char = ch;
                    repeat_start_ms = to_ms_since_boot(get_absolute_time());
                    repeat_active = false;
                }
            }
        }

        // Save current keys for next comparison
        for (int i = 0; i < 6; i++) {
            prev_keys[i] = keys[i];
        }
    }

    // Request next report
    tuh_hid_receive_report(dev_addr, instance);
}
