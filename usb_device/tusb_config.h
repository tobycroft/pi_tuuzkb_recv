#ifndef USB_DEVICE_TUSB_CONFIG_H
#define USB_DEVICE_TUSB_CONFIG_H

#define CFG_TUSB_RHPORT0_MODE       (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)

#define CFG_TUD_HID                    3

#define CFG_TUD_CDC                     0
#define CFG_TUD_MSC                     0
#define CFG_TUD_VENDOR                  0
#define CFG_TUD_MIDI                    0
#define CFG_TUD_AUDIO                   0
#define CFG_TUD_VIDEO                   0
#define CFG_TUD_DFU_RUNTIME            0
#define CFG_TUD_DFU_MODE               0

#define CFG_TUD_HID_EP_BUFSIZE          16

#define CFG_TUD_ENDPOINT0_SIZE           64

#endif // USB_DEVICE_TUSB_CONFIG_H