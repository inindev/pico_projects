//
// USB HID Keyboard Host for RP2350
//
// Copyright 2026, John Clark
//

#ifndef _USB_KEYBOARD_H_
#define _USB_KEYBOARD_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize USB host for keyboard input
void usb_keyboard_init(void);

// Poll USB host - must be called regularly from main loop
void usb_keyboard_task(void);

// Check if a character is available in the input buffer
bool usb_keyboard_available(void);

// Get the next character from the input buffer (0 if empty)
uint8_t usb_keyboard_getchar(void);

// Peek at the last key pressed without consuming it
uint8_t usb_keyboard_peek(void);

// Clear the keyboard input buffer
void usb_keyboard_clear(void);

#ifdef __cplusplus
}
#endif

#endif // _USB_KEYBOARD_H_
