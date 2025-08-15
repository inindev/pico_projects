#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_TUH_RHPORT 0 // Target primary USB-C port for native host mode
#define CFG_TUH_MAX_SPEED OPT_MODE_DEFAULT_SPEED

#ifndef CFG_TUSB_MCU
#define CFG_TUSB_MCU OPT_MCU_RP2350
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_NONE
#endif

#ifndef CFG_TUSB_DEBUG
#define CFG_TUSB_DEBUG 0
#endif
#define CFG_TUH_ENABLED 1

#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#endif

#define CFG_TUH_ENUMERATION_BUFSIZE 256
#define CFG_TUH_HUB 2
#define CFG_TUH_CDC 0
#define CFG_TUH_HID 4
#define CFG_TUH_MSC 0
#define CFG_TUH_HID_EPIN_BUFSIZE 64
#define CFG_TUH_HID_EPOUT_BUFSIZE 64

#ifdef __cplusplus
}
#endif

#endif // _TUSB_CONFIG_H_
