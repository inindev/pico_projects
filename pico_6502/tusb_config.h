//
// TinyUSB configuration for USB Host HID (keyboard)
//
// Copyright 2026, John Clark
//

#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// Common Configuration
//--------------------------------------------------------------------
#define CFG_TUSB_OS                 OPT_OS_PICO

// USB port 0 in host mode
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_HOST

//--------------------------------------------------------------------
// Host Configuration
//--------------------------------------------------------------------
#define CFG_TUH_ENABLED             1
#define CFG_TUH_RPI_PIO_USB         0  // Use native USB, not PIO USB
#define CFG_TUH_MAX_SPEED           OPT_MODE_FULL_SPEED

// Size of buffer to hold descriptors and other data used for enumeration
#define CFG_TUH_ENUMERATION_BUFSIZE 256

// Number of hub devices
#define CFG_TUH_HUB                 1

// Max number of devices
#define CFG_TUH_DEVICE_MAX          (CFG_TUH_HUB ? 4 : 1)

//--------------------------------------------------------------------
// HID Host Configuration
//--------------------------------------------------------------------
#define CFG_TUH_HID                 4  // Max number of HID interfaces
#define CFG_TUH_HID_EPIN_BUFSIZE    64
#define CFG_TUH_HID_EPOUT_BUFSIZE   64

//--------------------------------------------------------------------
// Disable Device Mode
//--------------------------------------------------------------------
#define CFG_TUD_ENABLED             0

#ifdef __cplusplus
}
#endif

#endif // _TUSB_CONFIG_H_
