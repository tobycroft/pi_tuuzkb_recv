#ifndef USB_DEVICE_USB_DESCRIPTORS_H
#define USB_DEVICE_USB_DESCRIPTORS_H

#if __cplusplus < 201703L
#error "UsbDescriptors requires C++17 or later"
#endif

#include <cstdint>
#include "tusb.h"

namespace usb_device {

constexpr std::uint8_t kHidItfNumKeyboard = 0;
constexpr std::uint8_t kHidItfNumMouse    = 1;

constexpr std::uint8_t kReportIdKeyboard = 1;
constexpr std::uint8_t kReportIdMouse    = 2;
constexpr std::uint8_t kReportIdMedia    = 3;

bool usb_descriptors_init();

} // namespace usb_device

#endif // USB_DEVICE_USB_DESCRIPTORS_H
