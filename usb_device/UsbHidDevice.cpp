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

void UsbHidDevice::sendMediaReport(const protocol::MediaReport& report) {
    if (!initialized_ || !tud_mounted()) return;

    if (!tud_hid_n_ready(2)) return;

    current_media_ = report;

    uint16_t usage = static_cast<uint16_t>(report.byte1) | (static_cast<uint16_t>(report.byte2) << 8);
    tud_hid_n_report(2, 3, &usage, sizeof(usage));
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
