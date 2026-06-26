#include <cstdint>
#include <cstddef>
#include <array>

#include "pico/time.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"

#include "drivers/uart/UartDriver.h"
#include "protocol/ProtocolParser.h"
#include "usb_device/UsbHidDevice.h"
#include "usb_device/UsbDescriptors.h"

int main() {

    constexpr std::uint8_t kGreenLedPin = 25;
    constexpr std::int64_t kLedBlinkDurationUs = 80000;

    gpio_init(kGreenLedPin);
    gpio_set_dir(kGreenLedPin, GPIO_OUT);
    gpio_put(kGreenLedPin, 0);

    constexpr std::uint8_t kBootBtnPin = 22;
    constexpr std::int64_t kBootBtnHoldUs = 200000;

    gpio_init(kBootBtnPin);
    gpio_set_dir(kBootBtnPin, GPIO_IN);
    gpio_pull_up(kBootBtnPin);

    absolute_time_t last_key_activity{};
    absolute_time_t btn_press_time{};
    bool btn_was_pressed = false;

    drivers::UartDriver uart;
    uart.init(300000);

    usb_device::usb_descriptors_init();

    usb_device::UsbHidDevice hid_device;
    hid_device.init();

    protocol::ProtocolParser parser;

    parser.setKbCallback([&](const protocol::KeyboardReport& report) {
        hid_device.sendKeyboardReport(report);
    });

    parser.setKbSingleKeyCallback([&](const protocol::KbSingleKeyEvent& evt) {
        last_key_activity = get_absolute_time();
        hid_device.handleSingleKey(evt);
    });

    parser.setMediaCallback([&](const protocol::MediaReport& report) {
        hid_device.sendMediaReport(report);
    });

    parser.setMouseCallback([&](const protocol::MouseReport& report) {
        hid_device.sendMouseReport(report);
    });

    parser.setMouseMoveCallback([&](const protocol::MouseMoveEvent& evt) {
        hid_device.handleMouseMove(evt);
    });

    parser.setMouseWheelCallback([&](const protocol::MouseWheelEvent& evt) {
        hid_device.handleMouseWheel(evt);
    });

    parser.setParaCfgCallback([&](const protocol::ParaCfgData& cfg) {
        usb_device::usb_set_vid_pid(cfg.vid, cfg.pid);
    });

    parser.setUsbStringCallback([&](const protocol::UsbStringData& str) {
        usb_device::usb_set_string(str.type, str.str.data(), str.len);
    });

    constexpr std::array<std::uint8_t, 3> kErrorReport = {0x57, 0xAB, 0x2E};
    parser.setChecksumErrorCallback([&]() {
        uart.write(kErrorReport.data(), kErrorReport.size());
    });

    std::array<std::uint8_t, 128> rx_buf{};

    constexpr std::array<std::uint8_t, 3> kHeartbeat = {0x57, 0xAB, 0x98};
    constexpr std::int64_t kHeartbeatIntervalUs = 5000000;

    absolute_time_t last_heartbeat = get_absolute_time();

    while (true) {
        hid_device.task();

        absolute_time_t now = get_absolute_time();

        bool btn_pressed = !gpio_get(kBootBtnPin);

        if (btn_pressed) {
            if (!btn_was_pressed) {
                btn_press_time = now;
                btn_was_pressed = true;
            } else if (absolute_time_diff_us(btn_press_time, now) >= kBootBtnHoldUs) {
                gpio_put(kGreenLedPin, 0);
                reset_usb_boot(0, 0);
            }
        } else {
            btn_was_pressed = false;
        }

        bool mounted = hid_device.isMounted();
        bool key_active = absolute_time_diff_us(last_key_activity, now) < kLedBlinkDurationUs;

        if (!mounted) {
            gpio_put(kGreenLedPin, 1);
        } else {
            gpio_put(kGreenLedPin, key_active);
        }

        if (absolute_time_diff_us(last_heartbeat, now) >= kHeartbeatIntervalUs) {
            uart.write(kHeartbeat.data(), kHeartbeat.size());
            last_heartbeat = now;
        }

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