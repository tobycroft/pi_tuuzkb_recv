#include <cstdint>
#include <cstddef>
#include <cstring>
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
    // 循环尝试 115200 → 300000 → 921600，每 0.5 秒切换一次
    // 每个波特率发送 0xA1 协商帧，收到完整有效的 57AB 帧即锁定
    constexpr std::array<std::uint32_t, 3> kBaudRates = {115200, 300000, 921600};
    constexpr std::int64_t kBaudTimeoutUs = 500000;
    constexpr std::array<std::uint8_t, 4> kBaudNegPkt = {
        0x57, 0xAB, protocol::kCmdBaudNegotiate,
        static_cast<std::uint8_t>((0x57 + 0xAB + protocol::kCmdBaudNegotiate) & 0xFF)
    };
    std::uint32_t locked_baud = 0;

    {
        protocol::ProtocolParser temp_parser;
        std::size_t rate_idx = 0;
        absolute_time_t bootsel_last_poll = get_absolute_time();

        while (locked_baud == 0) {
            auto rate = kBaudRates[rate_idx];
            uart.setBaudRate(rate);
            uart.flushRx();
            temp_parser.reset();

            uart.write(kBaudNegPkt.data(), kBaudNegPkt.size());

            absolute_time_t start = get_absolute_time();
            while (absolute_time_diff_us(start, get_absolute_time()) < kBaudTimeoutUs) {
                absolute_time_t now = get_absolute_time();

                if (absolute_time_diff_us(bootsel_last_poll, now) >= kBootBtnPollUs) {
                    bootsel_last_poll = now;
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
            rate_idx = (rate_idx + 1) % kBaudRates.size();
        }
    }

    usb_device::usb_descriptors_init();

    usb_device::UsbHidDevice hid_device;
    hid_device.init();

    auto send_device_info = [&]() {
        std::array<std::uint8_t, protocol::kDeviceInfoFrameLen> dev_info_pkt{};
        dev_info_pkt[0] = 0x57;
        dev_info_pkt[1] = 0xAB;
        dev_info_pkt[2] = protocol::kCmdDeviceInfo;

        std::uint16_t vid = usb_device::usb_get_vid();
        std::uint16_t pid = usb_device::usb_get_pid();
        dev_info_pkt[3] = static_cast<std::uint8_t>((vid >> 8) & 0xFF);
        dev_info_pkt[4] = static_cast<std::uint8_t>(vid & 0xFF);
        dev_info_pkt[5] = static_cast<std::uint8_t>((pid >> 8) & 0xFF);
        dev_info_pkt[6] = static_cast<std::uint8_t>(pid & 0xFF);

        dev_info_pkt[7]  = static_cast<std::uint8_t>(locked_baud & 0xFF);
        dev_info_pkt[8]  = static_cast<std::uint8_t>((locked_baud >> 8) & 0xFF);
        dev_info_pkt[9]  = static_cast<std::uint8_t>((locked_baud >> 16) & 0xFF);
        dev_info_pkt[10] = static_cast<std::uint8_t>((locked_baud >> 24) & 0xFF);

        dev_info_pkt[11] = hid_device.isMounted() ? static_cast<std::uint8_t>(1) : static_cast<std::uint8_t>(0);

        const char* mfgr = usb_device::usb_get_manufacturer();
        const char* prod = usb_device::usb_get_product();
        const char* serial = usb_device::usb_get_serial();
        std::size_t offset = 12;
        std::size_t len = std::strlen(mfgr);
        if (len > usb_device::kMaxUsbStringLen) len = usb_device::kMaxUsbStringLen;
        std::memcpy(&dev_info_pkt[offset], mfgr, len);
        offset += usb_device::kMaxUsbStringLen;
        len = std::strlen(prod);
        if (len > usb_device::kMaxUsbStringLen) len = usb_device::kMaxUsbStringLen;
        std::memcpy(&dev_info_pkt[offset], prod, len);
        offset += usb_device::kMaxUsbStringLen;
        len = std::strlen(serial);
        if (len > usb_device::kMaxUsbStringLen) len = usb_device::kMaxUsbStringLen;
        std::memcpy(&dev_info_pkt[offset], serial, len);

        std::uint8_t sum = 0;
        for (std::size_t i = 0; i < protocol::kDeviceInfoFrameLen - 1; ++i) {
            sum += dev_info_pkt[i];
        }
        dev_info_pkt[protocol::kDeviceInfoFrameLen - 1] = sum;
        uart.write(dev_info_pkt.data(), dev_info_pkt.size());
    };

    send_device_info();

    auto send_device_info_75 = [&]() {
        std::array<std::uint8_t, protocol::kDeviceInfoFrameLen> pkt{};
        pkt[0] = 0x57;
        pkt[1] = 0xAB;
        pkt[2] = protocol::kCmdGetUsbString;

        std::uint16_t vid = usb_device::usb_get_vid();
        std::uint16_t pid = usb_device::usb_get_pid();
        pkt[3] = static_cast<std::uint8_t>((vid >> 8) & 0xFF);
        pkt[4] = static_cast<std::uint8_t>(vid & 0xFF);
        pkt[5] = static_cast<std::uint8_t>((pid >> 8) & 0xFF);
        pkt[6] = static_cast<std::uint8_t>(pid & 0xFF);

        pkt[7]  = static_cast<std::uint8_t>(locked_baud & 0xFF);
        pkt[8]  = static_cast<std::uint8_t>((locked_baud >> 8) & 0xFF);
        pkt[9]  = static_cast<std::uint8_t>((locked_baud >> 16) & 0xFF);
        pkt[10] = static_cast<std::uint8_t>((locked_baud >> 24) & 0xFF);

        pkt[11] = hid_device.isMounted() ? static_cast<std::uint8_t>(1) : static_cast<std::uint8_t>(0);

        const char* mfgr = usb_device::usb_get_manufacturer();
        const char* prod = usb_device::usb_get_product();
        const char* serial = usb_device::usb_get_serial();
        std::size_t offset = 12;
        std::size_t len = std::strlen(mfgr);
        if (len > usb_device::kMaxUsbStringLen) len = usb_device::kMaxUsbStringLen;
        std::memcpy(&pkt[offset], mfgr, len);
        offset += usb_device::kMaxUsbStringLen;
        len = std::strlen(prod);
        if (len > usb_device::kMaxUsbStringLen) len = usb_device::kMaxUsbStringLen;
        std::memcpy(&pkt[offset], prod, len);
        offset += usb_device::kMaxUsbStringLen;
        len = std::strlen(serial);
        if (len > usb_device::kMaxUsbStringLen) len = usb_device::kMaxUsbStringLen;
        std::memcpy(&pkt[offset], serial, len);

        std::uint8_t sum = 0;
        for (std::size_t i = 0; i < protocol::kDeviceInfoFrameLen - 1; ++i) {
            sum += pkt[i];
        }
        pkt[protocol::kDeviceInfoFrameLen - 1] = sum;
        uart.write(pkt.data(), pkt.size());
    };

    usb_device::UsbHidDevice::setLedCallback([&](std::uint8_t led_byte) {
        std::array<std::uint8_t, 5> led_pkt{};
        led_pkt[0] = 0x57;
        led_pkt[1] = 0xAB;
        led_pkt[2] = protocol::kCmdLedStatus;
        led_pkt[3] = led_byte;
        led_pkt[4] = static_cast<std::uint8_t>(
            (0x57 + 0xAB + protocol::kCmdLedStatus + led_byte) & 0xFF);
        uart.write(led_pkt.data(), led_pkt.size());
    });

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
        send_device_info();
    });

    parser.setUsbStringCallback([&](const protocol::UsbStringData& str) {
        usb_device::usb_set_string(str.type, str.str.data(), str.len);
        send_device_info();
    });

    parser.setGetUsbStringCallback([&]() {
        send_device_info_75();
    });

    parser.setResetCallback([&]() {
        gpio_put(kGreenLedPin, 0);
        reset_usb_boot(0, 0);
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

        parser.pollPendingFrame();

        sleep_us(100);
    }

    return 0;
}