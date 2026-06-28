#include "ProtocolParser.h"
#include <cstring>

namespace protocol {

ProtocolParser::ProtocolParser()
    : state_(State::WaitHdr1)
    , cmd_code_(0)
    , expected_data_len_(0)
    , data_recv_(0)
    , pkt_index_(0)
    , checksum_acc_(0)
    , has_index_(false)
    , kb_cb_(nullptr)
    , kb_single_cb_(nullptr)
    , media_cb_(nullptr)
    , mouse_cb_(nullptr)
    , mouse_move_cb_(nullptr)
    , mouse_wheel_cb_(nullptr)
    , para_cfg_cb_(nullptr)
    , usb_str_cb_(nullptr)
    , checksum_err_cb_(nullptr)
    , idx_loss_cb_(nullptr)
    , led_status_cb_(nullptr)
    , get_usb_string_cb_(nullptr)
    , reset_cb_(nullptr)
    , last_idx_(0)
    , idx_initialized_(false)
    , pending_count_(0)
    , recent_idx_pos_(0)
    , recent_idx_count_(0) {
    frame_buf_.fill(0);
    for (auto& p : pending_queue_) {
        p.data.fill(0);
    }
    recent_idxs_.fill(0);
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

void ProtocolParser::setChecksumErrorCallback(ChecksumErrorCallback cb) {
    checksum_err_cb_ = std::move(cb);
}

void ProtocolParser::setIndexLossCallback(IndexLossCallback cb) {
    idx_loss_cb_ = std::move(cb);
}

void ProtocolParser::setLedStatusCallback(LedStatusCallback cb) {
    led_status_cb_ = std::move(cb);
}

void ProtocolParser::setGetUsbStringCallback(GetUsbStringCallback cb) {
    get_usb_string_cb_ = std::move(cb);
}

void ProtocolParser::setResetCallback(ResetCallback cb) {
    reset_cb_ = std::move(cb);
}

void ProtocolParser::reset() {
    state_ = State::WaitHdr1;
    data_recv_ = 0;
    checksum_acc_ = 0;
    has_index_ = false;
    expected_data_len_ = 0;
    frame_received_ = false;
}

bool ProtocolParser::cmdHasIndex(std::uint8_t cmd) {
    return cmd == kCmdSendKbGeneralData
        || cmd == kCmdSendKbMediaData
        || cmd == kCmdSendMsRelMoveData
        || cmd == kCmdSendMsRelWheelData;
}

static std::uint8_t getFixedDataLen(std::uint8_t cmd) {
    switch (cmd) {
        case kCmdSendKbGeneralData:  return kKbGeneralDataLen;
        case kCmdSendKbMediaData:    return kKbMediaDataLen;
        case kCmdSendMsRelMoveData:  return kMsRelMoveLen;
        case kCmdSendMsRelWheelData: return kMsRelWheelLen;
        case kCmdSetParaCfg:         return kParaCfgLen;
        case kCmdSetUsbString:       return kCombinedUsbStringLen;
        case kCmdLedStatus:          return kLedStatusLen;
        default:                     return 0;
    }
}

static bool isVariableLenCmd(std::uint8_t cmd) {
    (void)cmd;
    return false;
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
                    state_ = State::WaitCmd;
                } else {
                    reset();
                }
                break;

            case State::WaitCmd:
                cmd_code_ = byte;
                checksum_acc_ += byte;
                has_index_ = cmdHasIndex(cmd_code_);
                data_recv_ = 0;

                if (isVariableLenCmd(cmd_code_)) {
                    expected_data_len_ = kMaxFrameSize;
                } else {
                    expected_data_len_ = getFixedDataLen(cmd_code_);
                    if (expected_data_len_ == 0) {
                        if (cmd_code_ == kCmdBaudNegotiate || cmd_code_ == kCmdGetUsbString || cmd_code_ == kCmdReset) {
                            // 无数据负载帧，直接进入校验
                        } else {
                            reset();
                            break;
                        }
                    }
                }

                if (expected_data_len_ > kMaxFrameSize) {
                    reset();
                    break;
                }

                if (expected_data_len_ == 0) {
                    if (has_index_) {
                        state_ = State::WaitIndex;
                    } else {
                        state_ = State::WaitChecksum;
                    }
                } else {
                    state_ = State::WaitData;
                }
                break;

            case State::WaitData:
                frame_buf_[data_recv_++] = byte;
                checksum_acc_ += byte;

                if (isVariableLenCmd(cmd_code_)) {
                    if (data_recv_ >= 2) {
                        std::uint8_t str_len = frame_buf_[1];
                        std::uint8_t total_len = 2 + str_len;
                        if (total_len > kMaxFrameSize) {
                            reset();
                            break;
                        }
                        expected_data_len_ = total_len;
                    }
                }

                if (data_recv_ >= expected_data_len_) {
                    if (has_index_) {
                        state_ = State::WaitIndex;
                    } else {
                        state_ = State::WaitChecksum;
                    }
                }
                break;

            case State::WaitIndex:
                pkt_index_ = byte;
                checksum_acc_ += byte;
                state_ = State::WaitChecksum;
                break;

            case State::WaitChecksum: {
                std::uint8_t expected = static_cast<std::uint8_t>(checksum_acc_ & 0xFF);
                bool checksum_ok = (byte == expected);
                if (checksum_ok) {
                    if (has_index_) {
                        handleIndexedFrame(pkt_index_, cmd_code_, frame_buf_.data(), data_recv_);
                    } else {
                        dispatchCommand(cmd_code_, frame_buf_.data(), data_recv_);
                    }
                } else {
                    if (checksum_err_cb_) {
                        ChecksumErrorInfo info{};
                        info.cmd = cmd_code_;
                        info.expected_checksum = expected;
                        info.received_checksum = byte;
                        checksum_err_cb_(info);
                    }
                }
                reset();
                if (checksum_ok) {
                    frame_received_ = true;
                }
                break;
            }
        }
    }
}

void ProtocolParser::flushPendingFrames() {
    while (pending_count_ > 0) {
        if (pending_queue_[0].index == static_cast<std::uint8_t>(last_idx_ + 1)) {
            dispatchCommand(pending_queue_[0].cmd,
                           pending_queue_[0].data.data(),
                           pending_queue_[0].len);
            addRecentIndex(pending_queue_[0].index);
            last_idx_ = pending_queue_[0].index;
            for (std::uint8_t i = 0; i < pending_count_ - 1; ++i) {
                pending_queue_[i] = pending_queue_[i + 1];
            }
            --pending_count_;
        } else {
            break;
        }
    }
}

void ProtocolParser::addPendingFrame(std::uint8_t index, std::uint8_t cmd,
                                     const std::uint8_t* data, std::uint8_t len) {
    for (std::uint8_t i = 0; i < pending_count_; ++i) {
        if (pending_queue_[i].index == index) {
            return;
        }
    }

    if (pending_count_ >= kMaxPendingFrames) {
        flushPendingFrames();
        if (idx_loss_cb_) {
            idx_loss_cb_(static_cast<std::uint8_t>(last_idx_ + 1));
        }
        dispatchCommand(cmd, data, len);
        addRecentIndex(index);
        last_idx_ = index;
        return;
    }

    std::uint8_t pos = pending_count_;
    for (std::uint8_t i = 0; i < pending_count_; ++i) {
        if (pending_queue_[i].index > index) {
            pos = i;
            break;
        }
    }

    for (std::uint8_t i = pending_count_; i > pos; --i) {
        pending_queue_[i] = pending_queue_[i - 1];
    }

    pending_queue_[pos].index = index;
    pending_queue_[pos].cmd = cmd;
    pending_queue_[pos].len = len;
    for (std::uint8_t i = 0; i < len; ++i) {
        pending_queue_[pos].data[i] = data[i];
    }
    ++pending_count_;
}

bool ProtocolParser::hasPendingIndex(std::uint8_t idx) const {
    for (std::uint8_t i = 0; i < pending_count_; ++i) {
        if (pending_queue_[i].index == idx) {
            return true;
        }
    }
    return false;
}

bool ProtocolParser::isRecentIndex(std::uint8_t idx) const {
    for (std::uint8_t i = 0; i < recent_idx_count_; ++i) {
        if (recent_idxs_[i] == idx) return true;
    }
    return false;
}

void ProtocolParser::addRecentIndex(std::uint8_t idx) {
    recent_idxs_[recent_idx_pos_] = idx;
    recent_idx_pos_ = static_cast<std::uint8_t>((recent_idx_pos_ + 1) % kRecentIdxSize);
    if (recent_idx_count_ < kRecentIdxSize) {
        ++recent_idx_count_;
    }
}

void ProtocolParser::handleIndexedFrame(std::uint8_t index, std::uint8_t cmd,
                                        const std::uint8_t* data, std::uint8_t len) {
    if (!idx_initialized_) {
        dispatchCommand(cmd, data, len);
        last_idx_ = index;
        idx_initialized_ = true;
        addRecentIndex(index);
        return;
    }

    int diff_signed = static_cast<int>(index) - static_cast<int>(last_idx_);
    if (diff_signed > 128) {
        diff_signed -= 256;
    } else if (diff_signed < -128) {
        diff_signed += 256;
    }

    if (diff_signed > kIndexTolerance || diff_signed < -kIndexTolerance) {
        pending_count_ = 0;
        if (idx_loss_cb_) {
            idx_loss_cb_(static_cast<std::uint8_t>(last_idx_ + 1));
        }
        dispatchCommand(cmd, data, len);
        addRecentIndex(index);
        last_idx_ = index;
        return;
    }

    std::uint8_t diff = index - last_idx_;

    if (diff == 0) {
        return;
    }

    if (diff > 128) {
        return;
    }

    if (isRecentIndex(index)) {
        return;
    }

    if (hasPendingIndex(index)) {
        return;
    }

    if (diff == 1) {
        dispatchCommand(cmd, data, len);
        addRecentIndex(index);
        last_idx_ = index;
        flushPendingFrames();
        return;
    }

    addPendingFrame(index, cmd, data, len);
}

void ProtocolParser::dispatchCommand(std::uint8_t cmd, const std::uint8_t* data, std::uint8_t len) {
    switch (cmd) {
        case kCmdSendKbGeneralData: {
            if (len >= kKbGeneralDataLen) {
                KbSingleKeyEvent evt{};
                evt.usage = data[0];
                evt.pressed = (data[1] != 0);
                if (kb_single_cb_) {
                    kb_single_cb_(evt);
                }
            }
            break;
        }

        case kCmdSendKbMediaData: {
            if (len >= kKbMediaDataLen) {
                MediaReport report{};
                report.byte1 = data[0];
                report.byte2 = data[1];
                if (media_cb_) media_cb_(report);
            }
            break;
        }

        case kCmdSendMsRelMoveData: {
            if (len >= kMsRelMoveLen) {
                MouseMoveEvent evt{};
                evt.dx = static_cast<std::int8_t>(data[0]);
                evt.dy = static_cast<std::int8_t>(data[1]);
                if (mouse_move_cb_) mouse_move_cb_(evt);
            }
            break;
        }

        case kCmdSendMsRelWheelData: {
            if (len >= kMsRelWheelLen) {
                MouseWheelEvent evt{};
                evt.wheel = static_cast<std::int8_t>(data[0]);
                if (mouse_wheel_cb_) mouse_wheel_cb_(evt);
            }
            break;
        }

        case kCmdSetParaCfg: {
            if (len >= kParaCfgLen) {
                ParaCfgData cfg{};
                cfg.vid = (static_cast<std::uint16_t>(data[0]) << 8) | data[1];
                cfg.pid = (static_cast<std::uint16_t>(data[2]) << 8) | data[3];
                if (para_cfg_cb_) para_cfg_cb_(cfg);
            }
            break;
        }

        case kCmdSetUsbString: {
            if (len >= kCombinedUsbStringLen) {
                if (usb_str_cb_) {
                    UsbStringData manuf{};
                    manuf.type = kStrTypeManufacturer;
                    manuf.len = kMaxUsbStringLen - 1;
                    std::memcpy(manuf.str.data(), data, kMaxUsbStringLen);
                    manuf.str[kMaxUsbStringLen - 1] = '\0';
                    usb_str_cb_(manuf);

                    UsbStringData prod{};
                    prod.type = kStrTypeProduct;
                    prod.len = kMaxUsbStringLen - 1;
                    std::memcpy(prod.str.data(), data + kMaxUsbStringLen, kMaxUsbStringLen);
                    prod.str[kMaxUsbStringLen - 1] = '\0';
                    usb_str_cb_(prod);

                    UsbStringData serial{};
                    serial.type = kStrTypeSerial;
                    serial.len = kMaxUsbStringLen - 1;
                    std::memcpy(serial.str.data(), data + kMaxUsbStringLen * 2, kMaxUsbStringLen);
                    serial.str[kMaxUsbStringLen - 1] = '\0';
                    usb_str_cb_(serial);
                }
            }
            break;
        }

        case kCmdLedStatus: {
            if (len >= kLedStatusLen) {
                LedStatusData led{};
                led.led_byte = data[0];
                if (led_status_cb_) led_status_cb_(led);
            }
            break;
        }

        case kCmdGetUsbString: {
            if (get_usb_string_cb_) get_usb_string_cb_();
            break;
        }

        case kCmdReset: {
            if (reset_cb_) reset_cb_();
            break;
        }

        default:
            break;
    }
}

} // namespace protocol