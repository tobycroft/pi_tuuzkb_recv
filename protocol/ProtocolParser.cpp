#include "ProtocolParser.h"
#include <cstring>

namespace protocol {

ProtocolParser::ProtocolParser()
    : state_(State::WaitHdr1)
    , pkt_index_(0)
    , cmd_code_(0)
    , data_len_(0)
    , data_recv_(0)
    , checksum_acc_(0)
    , expected_index_(0)
    , has_cached_(false)
    , cached_index_(0)
    , cached_cmd_(0)
    , cached_data_len_(0)
    , kb_cb_(nullptr)
    , kb_single_cb_(nullptr)
    , media_cb_(nullptr)
    , mouse_cb_(nullptr)
    , mouse_move_cb_(nullptr)
    , mouse_wheel_cb_(nullptr)
    , para_cfg_cb_(nullptr)
    , usb_str_cb_(nullptr) {
    frame_buf_.fill(0);
    cached_data_.fill(0);
}

void ProtocolParser::setKbCallback(KbCallback cb) {
    kb_cb_ = std::move(cb);
}

void ProtocolParser::setKbSingleKeyCallback(KbSingleKeyCallback cb) {
    kb_single_cb_ = std::move(cb);
}

void ProtocolParser::setMediaCallback(MediaCallback cb) {
    media_cb_ = std::move(cb);
}

void ProtocolParser::setMouseCallback(MouseCallback cb) {
    mouse_cb_ = std::move(cb);
}

void ProtocolParser::setMouseMoveCallback(MouseMoveCallback cb) {
    mouse_move_cb_ = std::move(cb);
}

void ProtocolParser::setMouseWheelCallback(MouseWheelCallback cb) {
    mouse_wheel_cb_ = std::move(cb);
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
                                              std::uint8_t index, std::uint8_t cmd,
                                              std::uint8_t len,
                                              const std::uint8_t* data) {
    std::uint16_t sum = hdr1 + hdr2 + index + cmd + len;
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
                pkt_index_ = byte;
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
                    handleOrderedFrame(pkt_index_, cmd_code_, frame_buf_.data(), data_len_);
                }
                reset();
                break;
            }
        }
    }
}

void ProtocolParser::handleOrderedFrame(std::uint8_t index, std::uint8_t cmd,
                                        const std::uint8_t* data, std::uint8_t len) {
    if (index < expected_index_) {
        return;
    }

    if (index == expected_index_) {
        dispatchCommand(cmd, data, len);
        expected_index_++;
        if (has_cached_ && cached_index_ == expected_index_) {
            dispatchCommand(cached_cmd_, cached_data_.data(), cached_data_len_);
            expected_index_++;
            has_cached_ = false;
        }
        return;
    }

    if (index == expected_index_ + 1) {
        if (!has_cached_) {
            cached_index_ = index;
            cached_cmd_ = cmd;
            cached_data_len_ = len;
            std::memcpy(cached_data_.data(), data, len);
            has_cached_ = true;
        }
        return;
    }

    if (index == expected_index_ + 2 && has_cached_) {
        dispatchCommand(cached_cmd_, cached_data_.data(), cached_data_len_);
        dispatchCommand(cmd, data, len);
        expected_index_ = static_cast<std::uint8_t>(index + 1);
        has_cached_ = false;
        return;
    }

    dispatchCommand(cmd, data, len);
    expected_index_ = static_cast<std::uint8_t>(index + 1);
    has_cached_ = false;
}

void ProtocolParser::dispatchCommand(std::uint8_t cmd, const std::uint8_t* data, std::uint8_t len) {
    switch (cmd) {
        case kCmdSendKbGeneralData: {
            if (len >= 2) {
                KbSingleKeyEvent evt{};
                evt.usage = data[0];
                evt.pressed = (data[1] != 0);
                if (kb_single_cb_) kb_single_cb_(evt);
            }
            break;
        }

        case kCmdSendKbMediaData: {
            if (len >= 2) {
                MediaReport report{};
                report.byte1 = data[0];
                report.byte2 = data[1];
                if (media_cb_) media_cb_(report);
            }
            break;
        }

        case kCmdSendMsRelData: {
            if (len >= 2) {
                MouseMoveEvent evt{};
                evt.dx = static_cast<std::int8_t>(data[0]);
                evt.dy = static_cast<std::int8_t>(data[1]);
                if (mouse_move_cb_) mouse_move_cb_(evt);
            } else if (len >= 1) {
                MouseWheelEvent evt{};
                evt.wheel = static_cast<std::int8_t>(data[0]);
                if (mouse_wheel_cb_) mouse_wheel_cb_(evt);
            }
            break;
        }

        case kCmdSetParaCfg: {
            if (len >= 4) {
                ParaCfgData cfg{};
                cfg.vid = (static_cast<std::uint16_t>(data[0]) << 8) | data[1];
                cfg.pid = (static_cast<std::uint16_t>(data[2]) << 8) | data[3];
                if (para_cfg_cb_) para_cfg_cb_(cfg);
            }
            break;
        }

        case kCmdSetUsbString: {
            if (len >= 2) {
                UsbStringData str{};
                str.type = data[0];
                str.len = data[1];
                std::size_t copy_len = str.len;
                if (copy_len > kMaxUsbStringLen - 1) {
                    copy_len = kMaxUsbStringLen - 1;
                }
                for (std::size_t i = 0; i < copy_len; ++i) {
                    str.str[i] = static_cast<char>(data[2 + i]);
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

void ProtocolParser::processFrame() {
}

} // namespace protocol
