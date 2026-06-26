#include "UsbDescriptors.h"

namespace usb_device {

bool usb_descriptors_init() {
    return true;
}

} // namespace usb_device

static tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0xCafe,
    .idProduct          = 0x4001,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

uint8_t const* tud_descriptor_device_cb(void) {
    return (uint8_t const*) &desc_device;
}

static uint8_t const desc_hid_report_keyboard[] = {
    TUD_HID_REPORT_DESC_KEYBOARD( HID_REPORT_ID(1) )
};

static uint8_t const desc_hid_report_mouse[] = {
    TUD_HID_REPORT_DESC_MOUSE( HID_REPORT_ID(2) )
};

static uint8_t const desc_hid_report_media[] = {
    TUD_HID_REPORT_DESC_CONSUMER( HID_REPORT_ID(3) )
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN + TUD_HID_DESC_LEN)

#define EPNUM_HID_KEYBOARD   0x81
#define EPNUM_HID_MOUSE      0x82
#define EPNUM_HID_MEDIA      0x83

static uint8_t const desc_configuration[] = {
    TUD_CONFIG_DESCRIPTOR(1, 3, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(desc_hid_report_keyboard), EPNUM_HID_KEYBOARD, 16, 10),
    TUD_HID_DESCRIPTOR(1, 5, false, sizeof(desc_hid_report_mouse),    EPNUM_HID_MOUSE,    16, 10),
    TUD_HID_DESCRIPTOR(2, 6, false, sizeof(desc_hid_report_media),    EPNUM_HID_MEDIA,    16, 10),
};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index) {
    (void) index;
    return desc_configuration;
}

uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
    switch (instance) {
        case 0:
            return desc_hid_report_keyboard;
        case 1:
            return desc_hid_report_mouse;
        case 2:
            return desc_hid_report_media;
        default:
            return nullptr;
    }
}

static char const* string_desc_arr [] = {
    (const char[]) { 0x09, 0x04 },
    "TuuZKB",
    "Pi TuuZKB Recv",
    "1234567890",
    "Keyboard",
    "Mouse",
    "Media Keys",
};

static uint16_t _desc_str[32];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;

    uint8_t chr_count;

    if ( index == 0) {
        memcpy(&_desc_str[1], string_desc_arr[0], 2);
        chr_count = 1;
    } else {
        if ( !(index < sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) ) return NULL;

        const char* str = string_desc_arr[index];

        chr_count = strlen(str);
        if ( chr_count > 31 ) chr_count = 31;

        for(uint8_t i=0; i<chr_count; i++) {
            _desc_str[1+i] = str[i];
        }
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

    return _desc_str;
}
