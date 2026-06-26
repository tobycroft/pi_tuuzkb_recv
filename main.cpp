#include <cstdint>
#include <cstddef>
#include <array>
#include <iostream>

#include "pico/stdlib.h"

#include "drivers/uart/UartDriver.h"
#include "protocol/ProtocolParser.h"
#include "usb_device/UsbHidDevice.h"

int main() {
    stdio_init_all();

    drivers::UartDriver uart;
    uart.init(921600);

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

    std::array<std::uint8_t, 64> rx_buf{};

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
