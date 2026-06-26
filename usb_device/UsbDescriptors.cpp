#include "UsbDescriptors.h"
#include <cstring>

namespace usb_device {

namespace {

struct UsbConfig {
    std::uint16_t vid;
    std::uint16_t pid;
    std::array<char, kMaxUsbStringLen> manufacturer;
    std::array<char, kMaxUsbStringLen> product;
    std::array<char, kMaxUsbStringLen> serial;
};

UsbConfig g_config = {
    .vid = 0xCafe,
    .pid = 0x4001,
    .manufacturer = {},
    .product = {},
    .serial = {}
};

bool g_initialized = false;

void init_default_strings() {
    if (g_initialized) return;
    std::strncpy(g_config.manufacturer.data(), "TuuZKB", kMaxUsbStringLen - 1);
    g_config.manufacturer[kMaxUsbStringLen - 1] = '\0';
    std::strncpy(g_config.product.data(), "Pi TuuZKB Recv", kMaxUsbStringLen - 1);
    g_config.product[kMaxUsbStringLen - 1] = '\0';
    std::strncpy(g_config.serial.data(), "1234567890", kMaxUsbStringLen - 1);
    g_config.serial[kMaxUsbStringLen - 1] = '\0';
    g_initialized = true;
}

} // namespace

bool usb_descriptors_init() {
    init_default_strings();
    return true;
}

void usb_set_vid_pid(std::uint16_t vid, std::uint16_t pid) {
    init_default_strings();
    g_config.vid = vid;
    g_config.pid = pid;
}

void usb_set_string(std::uint8_t type, const char* str, std::uint8_t len) {
    init_default_strings();
    if (str == nullptr || len == 0) return;

    char* dest = nullptr;
    switch (type) {
        case kStrTypeManufacturer:
            dest = g_config.manufacturer.data();
            break;
        case kStrTypeProduct:
            dest = g_config.product.data();
            break;
        case kStrTypeSerial:
            dest = g_config.serial.data();
            break;
        default:
            return;
    }

    std::size_t copy_len = len;
    if (copy_len > kMaxUsbStringLen - 1) {
        copy_len = kMaxUsbStringLen - 1;
    }
    std::memcpy(dest, str, copy_len);
    dest[copy_len] = '\0';
}

} // namespace usb_device

static tusb_desc_device_t desc_device = {
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
    desc_device.idVendor = usb_device::g_config.vid;
    desc_device.idProduct = usb_device::g_config.pid;
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

static uint16_t _desc_str[64];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void) langid;

    uint8_t chr_count;

    if ( index == 0) {
        _desc_str[1] = 0x0409;
        chr_count = 1;
    } else {
        const char* str = nullptr;
        switch (index) {
            case 1:
                str = usb_device::g_config.manufacturer.data();
                break;
            case 2:
                str = usb_device::g_config.product.data();
                break;
            case 3:
                str = usb_device::g_config.serial.data();
                break;
            case 4:
                str = "Keyboard";
                break;
            case 5:
                str = "Mouse";
                break;
            case 6:
                str = "Media Keys";
                break;
            default:
                return NULL;
        }

        chr_count = strlen(str);
        if ( chr_count > 31 ) chr_count = 31;

        for(uint8_t i=0; i<chr_count; i++) {
            _desc_str[1+i] = str[i];
        }
    }

    _desc_str[0] = (TUSB_DESC_STRING << 8 ) | (2*chr_count + 2);

    return _desc_str;
}
