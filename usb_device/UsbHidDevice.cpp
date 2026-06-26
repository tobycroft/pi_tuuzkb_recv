#include "UsbHidDevice.h"
#include "bsp/board.h"
#include "tusb.h"

namespace usb_device {

UsbHidDevice::UsbHidDevice()
    : initialized_(false) {
    current_kb_.modifiers = 0;
    current_kb_.reserved = 0;
    current_kb_.keys.fill(0);
    current_mouse_.reserved = 0;
    current_mouse_.buttons = 0;
    current_mouse_.x = 0;
    current_mouse_.y = 0;
    current_mouse_.wheel = 0;
    current_media_.byte1 = 0;
    current_media_.byte2 = 0;
}

void UsbHidDevice::init() {
    if (initialized_) return;
    board_init();
    tud_init(0);
    initialized_ = true;
}

void UsbHidDevice::task() {
    if (!initialized_) return;
    tud_task();
}

bool UsbHidDevice::isMounted() const {
    return tud_mounted();
}

void UsbHidDevice::sendKeyboardReport(const protocol::KeyboardReport& report) {
    if (!initialized_ || !tud_mounted()) return;

    if (!tud_hid_n_ready(0)) return;

    current_kb_ = report;

    uint8_t keycode[6];
    for (int i = 0; i < 6; ++i) {
        keycode[i] = report.keys[i];
    }

    tud_hid_n_keyboard_report(0, 1, report.modifiers, keycode);
}

void UsbHidDevice::sendMouseReport(const protocol::MouseReport& report) {
    if (!initialized_ || !tud_mounted()) return;

    if (!tud_hid_n_ready(1)) return;

    current_mouse_ = report;

    tud_hid_n_mouse_report(1, 2, report.buttons, report.x, report.y, report.wheel, 0);
}

void UsbHidDevice::handleMouseMove(const protocol::MouseMoveEvent& evt) {
    if (!initialized_ || !tud_mounted()) return;
    if (!tud_hid_n_ready(1)) return;

    current_mouse_.x = evt.dx;
    current_mouse_.y = evt.dy;

    tud_hid_n_mouse_report(1, 2, current_mouse_.buttons,
                           current_mouse_.x, current_mouse_.y,
                           current_mouse_.wheel, 0);
}

void UsbHidDevice::handleMouseWheel(const protocol::MouseWheelEvent& evt) {
    if (!initialized_ || !tud_mounted()) return;
    if (!tud_hid_n_ready(1)) return;

    current_mouse_.wheel = evt.wheel;
    current_mouse_.x = 0;
    current_mouse_.y = 0;

    tud_hid_n_mouse_report(1, 2, current_mouse_.buttons,
                           current_mouse_.x, current_mouse_.y,
                           current_mouse_.wheel, 0);
}

void UsbHidDevice::sendMediaReport(const protocol::MediaReport& report) {
    if (!initialized_ || !tud_mounted()) return;

    if (!tud_hid_n_ready(2)) return;

    current_media_ = report;

    uint16_t usage = static_cast<uint16_t>(report.byte1) | (static_cast<uint16_t>(report.byte2) << 8);
    tud_hid_n_report(2, 3, &usage, sizeof(usage));
}

static std::uint8_t modifier_usage_to_bit(std::uint8_t usage) {
    if (usage >= 0xE0 && usage <= 0xE7) {
        return static_cast<std::uint8_t>(1 << (usage - 0xE0));
    }
    return 0;
}

void UsbHidDevice::handleSingleKey(const protocol::KbSingleKeyEvent& evt) {
    if (!initialized_ || !tud_mounted()) return;
    if (!tud_hid_n_ready(0)) return;

    std::uint8_t mod_bit = modifier_usage_to_bit(evt.usage);
    if (mod_bit != 0) {
        if (evt.pressed) {
            current_kb_.modifiers |= mod_bit;
        } else {
            current_kb_.modifiers &= static_cast<std::uint8_t>(~mod_bit);
        }
    } else {
        if (evt.pressed) {
            bool already = false;
            for (auto& k : current_kb_.keys) {
                if (k == evt.usage) {
                    already = true;
                    break;
                }
            }
            if (!already) {
                bool inserted = false;
                for (auto& k : current_kb_.keys) {
                    if (k == 0) {
                        k = evt.usage;
                        inserted = true;
                        break;
                    }
                }
                if (!inserted) {
                    current_kb_.keys[5] = evt.usage;
                }
            }
        } else {
            for (auto& k : current_kb_.keys) {
                if (k == evt.usage) {
                    k = 0;
                }
            }
        }
    }

    uint8_t keycode[6];
    for (int i = 0; i < 6; ++i) {
        keycode[i] = current_kb_.keys[i];
    }
    tud_hid_n_keyboard_report(0, 1, current_kb_.modifiers, keycode);
}

} // namespace usb_device

extern "C" {

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;
    return 0;
}

void tud_mount_cb(void) {
}

void tud_umount_cb(void) {
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void) remote_wakeup_en;
}

void tud_resume_cb(void) {
}

} // extern "C"
