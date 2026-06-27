#include <cstdint>
#include <cstddef>
#include <array>

#include "pico/time.h"
#include "pico/bootrom.h"
#include "hardware/gpio.h"

#include "uart/UartDriver.h"
#include "protocol/ProtocolParser.h"
#include "usb_device/UsbHidDevice.h"
#include "usb_device/UsbDescriptors.h"

extern "C" bool get_bootsel_button(void);

int main() {

    constexpr std::uint8_t kGreenLedPin = 25;
    constexpr std::int64_t kLedBlinkDurationUs = 80000;

    gpio_init(kGreenLedPin);
    gpio_set_dir(kGreenLedPin, GPIO_OUT);
    gpio_put(kGreenLedPin, 0);

    constexpr std::int64_t kBootBtnHoldUs = 2500000;
    constexpr std::int64_t kBootBtnPollUs = 50000;

    absolute_time_t last_key_activity{};
    absolute_time_t btn_press_time{};
    bool btn_was_pressed = false;

    uart::UartDriver uart;
    uart.init(115200);

    // ===== 自动波特率协商 =====
    // 依次尝试 115200 → 300000 → 921600
    // 每个波特率发送 0xFF 协商帧，然后监听 500ms
    // 收到完整有效的 57AB 帧（Go 端响应）即锁定
    constexpr std::array<std::uint32_t, 3> kBaudRates = {115200, 300000, 921600};
    constexpr std::int64_t kBaudTimeoutUs = 500000;
    constexpr std::array<std::uint8_t, 4> kBaudNegPkt = {
        0x57, 0xAB, protocol::kCmdBaudNegotiate,
        static_cast<std::uint8_t>((0x57 + 0xAB + protocol::kCmdBaudNegotiate) & 0xFF)
    };
    std::uint32_t locked_baud = 0;

    {
        protocol::ProtocolParser temp_parser;
        for (auto rate : kBaudRates) {
            uart.setBaudRate(rate);
            uart.flushRx();
            temp_parser.reset();

            uart.write(kBaudNegPkt.data(), kBaudNegPkt.size());

            absolute_time_t start = get_absolute_time();
            while (absolute_time_diff_us(start, get_absolute_time()) < kBaudTimeoutUs) {
                if (!uart.isReadable()) {
                    sleep_us(500);
                    continue;
                }
                std::uint8_t tmp[64];
                auto n = uart.read(tmp, sizeof(tmp));
                temp_parser.feed(tmp, n);
                if (temp_parser.hasReceivedValidFrame()) {
                    locked_baud = rate;
                    break;
                }
            }
            if (locked_baud != 0) break;
        }
    }

    if (locked_baud == 0) {
        locked_baud = 300000;
        uart.setBaudRate(locked_baud);
    }

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

    parser.setChecksumErrorCallback([&](const protocol::ChecksumErrorInfo& info) {
        std::array<std::uint8_t, 7> err_pkt{};
        err_pkt[0] = 0x57;
        err_pkt[1] = 0xAB;
        err_pkt[2] = protocol::kCmdError;
        err_pkt[3] = info.cmd;
        err_pkt[4] = info.expected_checksum;
        err_pkt[5] = info.received_checksum;
        err_pkt[6] = static_cast<std::uint8_t>(
            (0x57 + 0xAB + protocol::kCmdError + info.cmd
             + info.expected_checksum + info.received_checksum) & 0xFF);
        uart.write(err_pkt.data(), err_pkt.size());
    });

    parser.setIndexLossCallback([&](std::uint8_t lost_index) {
        std::array<std::uint8_t, 5> loss_pkt{};
        loss_pkt[0] = 0x57;
        loss_pkt[1] = 0xAB;
        loss_pkt[2] = protocol::kCmdIndexLoss;
        loss_pkt[3] = lost_index;
        loss_pkt[4] = static_cast<std::uint8_t>(
            (0x57 + 0xAB + protocol::kCmdIndexLoss + lost_index) & 0xFF);
        uart.write(loss_pkt.data(), loss_pkt.size());
    });

    std::array<std::uint8_t, 128> rx_buf{};

    absolute_time_t last_uart_rx{};
    absolute_time_t last_bootsel_poll = get_absolute_time();

    while (true) {
        hid_device.task();

        absolute_time_t now = get_absolute_time();

        if (absolute_time_diff_us(last_bootsel_poll, now) >= kBootBtnPollUs) {
            last_bootsel_poll = now;

            bool btn_pressed = !get_bootsel_button();

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
        }

        bool mounted = hid_device.isMounted();
        bool key_active = absolute_time_diff_us(last_key_activity, now) < kLedBlinkDurationUs;
        bool uart_rx_active = absolute_time_diff_us(last_uart_rx, now) < kLedBlinkDurationUs;

        if (!mounted) {
            gpio_put(kGreenLedPin, 1);
        } else {
            gpio_put(kGreenLedPin, key_active || uart_rx_active);
        }

        if (uart.isReadable()) {
            std::size_t n = uart.read(rx_buf.data(), rx_buf.size());
            if (n > 0) {
                last_uart_rx = get_absolute_time();
                parser.feed(rx_buf.data(), n);
            }
        }

        sleep_us(100);
    }

    return 0;
}