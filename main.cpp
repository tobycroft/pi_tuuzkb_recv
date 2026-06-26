#include <cstdint>
#include <cstddef>
#include <array>
#include <iostream>

#include "pico/stdlib.h"

#include "drivers/uart/UartDriver.h"
#include "protocol/ProtocolParser.h"
#include "usb_device/UsbHidDevice.h"
#include "usb_device/UsbDescriptors.h"

int main() {
    stdio_init_all();

    drivers::UartDriver uart;
    uart.init(921600);

    usb_device::usb_descriptors_init();

    usb_device::UsbHidDevice hid_device;
    hid_device.init();

    protocol::ProtocolParser parser;

    parser.setKbCallback([&](const protocol::KeyboardReport& report) {
        hid_device.sendKeyboardReport(report);
    });

    parser.setMediaCallback([&](const protocol::MediaReport& report) {
        hid_device.sendMediaReport(report);
    });

    parser.setMouseCallback([&](const protocol::MouseReport& report) {
        hid_device.sendMouseReport(report);
    });

    parser.setParaCfgCallback([&](const protocol::ParaCfgData& cfg) {
        usb_device::usb_set_vid_pid(cfg.vid, cfg.pid);
    });

    parser.setUsbStringCallback([&](const protocol::UsbStringData& str) {
        usb_device::usb_set_string(str.type, str.str.data(), str.len);
    });

    std::array<std::uint8_t, 128> rx_buf{};

    while (true) {
        hid_device.task();

        if (uart.isReadable()) {
            std::size_t n = uart.read(rx_buf.data(), rx_buf.size());
            if (n > 0) {
                parser.feed(rx_buf.data(), n);
            }
        }

        sleep_us(100);
    }

    return 0;
}
