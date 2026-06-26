#include "UsbDescriptors.h"
#include <cstring>
#include "hardware/flash.h"
#include "hardware/sync.h"

namespace usb_device {

namespace {

struct UsbConfig {
    std::uint32_t magic;
    std::uint16_t vid;
    std::uint16_t pid;
    std::array<char, kMaxUsbStringLen> manufacturer;
    std::array<char, kMaxUsbStringLen> product;
    std::array<char, kMaxUsbStringLen> serial;
};

static_assert(sizeof(UsbConfig) <= FLASH_PAGE_SIZE, "UsbConfig must fit in a flash page");

UsbConfig g_config = {
    .magic = kConfigMagic,
    .vid = 0xCafe,
    .pid = 0x4001,
    .manufacturer = {},
    .product = {},
    .serial = {}
};

bool g_initialized = false;

void write_default_strings() {
    std::strncpy(g_config.manufacturer.data(), "TuuZKB", kMaxUsbStringLen - 1);
    g_config.manufacturer[kMaxUsbStringLen - 1] = '\0';
    std::strncpy(g_config.product.data(), "Pi TuuZKB Recv", kMaxUsbStringLen - 1);
    g_config.product[kMaxUsbStringLen - 1] = '\0';
    std::strncpy(g_config.serial.data(), "1234567890", kMaxUsbStringLen - 1);
    g_config.serial[kMaxUsbStringLen - 1] = '\0';
}

void load_config_from_flash() {
    const std::uint8_t* flash_ptr =
        reinterpret_cast<const std::uint8_t*>(XIP_BASE + kConfigFlashOffset);
    const UsbConfig* stored = reinterpret_cast<const UsbConfig*>(flash_ptr);

    if (stored->magic == kConfigMagic) {
        g_config.magic = stored->magic;
        g_config.vid = stored->vid;
        g_config.pid = stored->pid;
        std::memcpy(g_config.manufacturer.data(), stored->manufacturer.data(), kMaxUsbStringLen);
        std::memcpy(g_config.product.data(), stored->product.data(), kMaxUsbStringLen);
        std::memcpy(g_config.serial.data(), stored->serial.data(), kMaxUsbStringLen);
        g_config.manufacturer[kMaxUsbStringLen - 1] = '\0';
        g_config.product[kMaxUsbStringLen - 1] = '\0';
        g_config.serial[kMaxUsbStringLen - 1] = '\0';
    } else {
        write_default_strings();
    }
}

bool is_config_same_in_flash() {
    const std::uint8_t* flash_ptr =
        reinterpret_cast<const std::uint8_t*>(XIP_BASE + kConfigFlashOffset);
    const UsbConfig* stored = reinterpret_cast<const UsbConfig*>(flash_ptr);

    if (stored->magic != kConfigMagic) return false;
    if (stored->vid != g_config.vid) return false;
    if (stored->pid != g_config.pid) return false;
    if (std::memcmp(stored->manufacturer.data(), g_config.manufacturer.data(), kMaxUsbStringLen) != 0) return false;
    if (std::memcmp(stored->product.data(), g_config.product.data(), kMaxUsbStringLen) != 0) return false;
    if (std::memcmp(stored->serial.data(), g_config.serial.data(), kMaxUsbStringLen) != 0) return false;
    return true;
}

void save_config_to_flash() {
    if (is_config_same_in_flash()) return;

    g_config.magic = kConfigMagic;

    std::uint8_t page_buf[FLASH_PAGE_SIZE];
    std::memset(page_buf, 0xFF, FLASH_PAGE_SIZE);
    std::memcpy(page_buf, &g_config, sizeof(UsbConfig));

    std::uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(kConfigFlashOffset, FLASH_SECTOR_SIZE);
    flash_range_program(kConfigFlashOffset, page_buf, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}

} // namespace

bool usb_descriptors_init() {
    if (g_initialized) return true;
    load_config_from_flash();
    g_initialized = true;
    return true;
}

void usb_set_vid_pid(std::uint16_t vid, std::uint16_t pid) {
    if (!g_initialized) load_config_from_flash();
    g_config.vid = vid;
    g_config.pid = pid;
    save_config_to_flash();
}

void usb_set_string(std::uint8_t type, const char* str, std::uint8_t len) {
    if (!g_initialized) load_config_from_flash();
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
    save_config_to_flash();
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