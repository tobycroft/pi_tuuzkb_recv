#ifndef USB_DEVICE_USB_DESCRIPTORS_H
#define USB_DEVICE_USB_DESCRIPTORS_H

#if __cplusplus < 201703L
#error "UsbDescriptors requires C++17 or later"
#endif

#include <cstdint>
#include <array>
#include "tusb.h"

namespace usb_device {

constexpr std::uint8_t kHidItfNumKeyboard = 0;
constexpr std::uint8_t kHidItfNumMouse    = 1;
constexpr std::uint8_t kHidItfNumMedia    = 2;

constexpr std::uint8_t kReportIdKeyboard = 1;
constexpr std::uint8_t kReportIdMouse    = 2;
constexpr std::uint8_t kReportIdMedia    = 3;

constexpr std::size_t kMaxUsbStringLen = 64;

constexpr std::uint8_t kStrTypeManufacturer = 0x00;
constexpr std::uint8_t kStrTypeProduct      = 0x01;
constexpr std::uint8_t kStrTypeSerial       = 0x02;

constexpr std::uint32_t kConfigFlashOffset = 0x1FF000;
constexpr std::uint32_t kConfigMagic       = 0x57AB191B;

bool usb_descriptors_init();

void usb_set_vid_pid(std::uint16_t vid, std::uint16_t pid);

void usb_set_string(std::uint8_t type, const char* str, std::uint8_t len);

std::uint16_t usb_get_vid();
std::uint16_t usb_get_pid();
const char* usb_get_manufacturer();
const char* usb_get_product();
const char* usb_get_serial();

} // namespace usb_device

#endif // USB_DEVICE_USB_DESCRIPTORS_H