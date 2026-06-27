#ifndef USB_DEVICE_USB_HID_DEVICE_H
#define USB_DEVICE_USB_HID_DEVICE_H

#if __cplusplus < 201703L
#error "UsbHidDevice requires C++17 or later"
#endif

#include <cstdint>
#include <array>
#include "../protocol/ProtocolParser.h"

namespace usb_device {

class UsbHidDevice {
public:
    UsbHidDevice();
    ~UsbHidDevice() = default;

    UsbHidDevice(const UsbHidDevice&) = delete;
    UsbHidDevice& operator=(const UsbHidDevice&) = delete;

    void init();

    void task();

    void sendKeyboardReport(const protocol::KeyboardReport& report);
    void sendMouseReport(const protocol::MouseReport& report);
    void sendMediaReport(const protocol::MediaReport& report);

    void handleMouseMove(const protocol::MouseMoveEvent& evt);
    void handleMouseWheel(const protocol::MouseWheelEvent& evt);

    void handleSingleKey(const protocol::KbSingleKeyEvent& evt);

    bool isMounted() const;

private:
    void flushKeyboardReport();
    void flushMouseReport();

    bool initialized_;

    protocol::KeyboardReport current_kb_;
    protocol::MouseReport    current_mouse_;
    protocol::MediaReport    current_media_;

    bool kb_dirty_;
    bool mouse_dirty_;
};

} // namespace usb_device

#endif // USB_DEVICE_USB_HID_DEVICE_H