//
// Copyright (c) 2025 John Clark <inindev@gmail.com>
//

#ifndef _KEYBOARD_H_
#define _KEYBOARD_H_

#include <stdbool.h>
#include <pico/stdlib.h>
#include <tusb.h>

bool has_keycode(hid_keyboard_report_t *report, uint8_t code);
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *desc_report, uint16_t desc_len);
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance);
void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const *report, uint16_t len);

extern bool keyboard_connected;

#endif // _KEYBOARD_H_
