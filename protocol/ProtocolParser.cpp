#include "ProtocolParser.h"
#include <cstring>

namespace protocol {

ProtocolParser::ProtocolParser()
    : state_(State::WaitHdr1)
    , addr_code_(0)
    , cmd_code_(0)
    , data_len_(0)
    , data_recv_(0)
    , checksum_acc_(0)
    , kb_cb_(nullptr)
    , media_cb_(nullptr)
    , mouse_cb_(nullptr)
    , para_cfg_cb_(nullptr)
    , usb_str_cb_(nullptr) {
    frame_buf_.fill(0);
}

void ProtocolParser::setKbCallback(KbCallback cb) {
    kb_cb_ = std::move(cb);
}

void ProtocolParser::setMediaCallback(MediaCallback cb) {
    media_cb_ = std::move(cb);
}

void ProtocolParser::setMouseCallback(MouseCallback cb) {
    mouse_cb_ = std::move(cb);
}

void ProtocolParser::setParaCfgCallback(ParaCfgCallback cb) {
    para_cfg_cb_ = std::move(cb);
}

void ProtocolParser::setUsbStringCallback(UsbStringCallback cb) {
    usb_str_cb_ = std::move(cb);
}

void ProtocolParser::reset() {
    state_ = State::WaitHdr1;
    data_recv_ = 0;
    checksum_acc_ = 0;
}

std::uint8_t ProtocolParser::computeChecksum(std::uint8_t hdr1, std::uint8_t hdr2,
                                              std::uint8_t addr, std::uint8_t cmd,
                                              std::uint8_t len,
                                              const std::uint8_t* data) {
    std::uint16_t sum = hdr1 + hdr2 + addr + cmd + len;
    for (std::uint8_t i = 0; i < len; ++i) {
        sum += data[i];
    }
    return static_cast<std::uint8_t>(sum & 0xFF);
}

void ProtocolParser::feed(const std::uint8_t* data, std::size_t len) {
    if (data == nullptr || len == 0) return;

    for (std::size_t i = 0; i < len; ++i) {
        std::uint8_t byte = data[i];

        switch (state_) {
            case State::WaitHdr1:
                if (byte == kFrameHdr1) {
                    checksum_acc_ = byte;
                    state_ = State::WaitHdr2;
                }
                break;

            case State::WaitHdr2:
                if (byte == kFrameHdr2) {
                    checksum_acc_ += byte;
                    state_ = State::WaitAddr;
                } else {
                    reset();
                }
                break;

            case State::WaitAddr:
                addr_code_ = byte;
                checksum_acc_ += byte;
                state_ = State::WaitCmd;
                break;

            case State::WaitCmd:
                cmd_code_ = byte;
                checksum_acc_ += byte;
                state_ = State::WaitLen;
                break;

            case State::WaitLen:
                data_len_ = byte;
                checksum_acc_ += byte;
                data_recv_ = 0;
                if (data_len_ == 0) {
                    state_ = State::WaitChecksum;
                } else if (data_len_ > kMaxFrameSize) {
                    reset();
                } else {
                    state_ = State::WaitData;
                }
                break;

            case State::WaitData:
                frame_buf_[data_recv_++] = byte;
                checksum_acc_ += byte;
                if (data_recv_ >= data_len_) {
                    state_ = State::WaitChecksum;
                }
                break;

            case State::WaitChecksum: {
                std::uint8_t expected = static_cast<std::uint8_t>(checksum_acc_ & 0xFF);
                if (byte == expected) {
                    processFrame();
                }
                reset();
                break;
            }
        }
    }
}

void ProtocolParser::processFrame() {
    switch (cmd_code_) {
        case kCmdSendKbGeneralData: {
            if (data_len_ >= 8) {
                KeyboardReport report{};
                report.modifiers = frame_buf_[0];
                report.reserved  = frame_buf_[1];
                for (int i = 0; i < 6; ++i) {
                    report.keys[i] = frame_buf_[2 + i];
                }
                if (kb_cb_) kb_cb_(report);
            }
            break;
        }

        case kCmdSendKbMediaData: {
            if (data_len_ >= 2) {
                MediaReport report{};
                report.byte1 = frame_buf_[0];
                report.byte2 = frame_buf_[1];
                if (media_cb_) media_cb_(report);
            }
            break;
        }

        case kCmdSendMsRelData: {
            if (data_len_ >= 5) {
                MouseReport report{};
                report.reserved = frame_buf_[0];
                report.buttons  = frame_buf_[1];
                report.x        = static_cast<std::int8_t>(frame_buf_[2]);
                report.y        = static_cast<std::int8_t>(frame_buf_[3]);
                report.wheel    = static_cast<std::int8_t>(frame_buf_[4]);
                if (mouse_cb_) mouse_cb_(report);
            }
            break;
        }

        case kCmdSetParaCfg: {
            if (data_len_ >= 4) {
                ParaCfgData cfg{};
                cfg.vid = (static_cast<std::uint16_t>(frame_buf_[0]) << 8) | frame_buf_[1];
                cfg.pid = (static_cast<std::uint16_t>(frame_buf_[2]) << 8) | frame_buf_[3];
                if (para_cfg_cb_) para_cfg_cb_(cfg);
            }
            break;
        }

        case kCmdSetUsbString: {
            if (data_len_ >= 2) {
                UsbStringData str{};
                str.type = frame_buf_[0];
                str.len = frame_buf_[1];
                std::size_t copy_len = str.len;
                if (copy_len > kMaxUsbStringLen - 1) {
                    copy_len = kMaxUsbStringLen - 1;
                }
                for (std::size_t i = 0; i < copy_len; ++i) {
                    str.str[i] = static_cast<char>(frame_buf_[2 + i]);
                }
                str.str[copy_len] = '\0';
                if (usb_str_cb_) usb_str_cb_(str);
            }
            break;
        }

        default:
            break;
    }
}

} // namespace protocol
