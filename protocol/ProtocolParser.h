#ifndef PROTOCOL_PROTOCOL_PARSER_H
#define PROTOCOL_PROTOCOL_PARSER_H

#if __cplusplus < 201703L
#error "ProtocolParser requires C++17 or later"
#endif

#include <cstdint>
#include <cstddef>
#include <array>
#include <functional>

namespace protocol {

constexpr std::uint8_t kFrameHdr1 = 0x57;
constexpr std::uint8_t kFrameHdr2 = 0xAB;

constexpr std::uint8_t kCmdSendKbGeneralData  = 0x12;
constexpr std::uint8_t kCmdSendKbMediaData    = 0x13;
constexpr std::uint8_t kCmdSendMsRelMoveData  = 0x15;
constexpr std::uint8_t kCmdSendMsRelWheelData = 0x16;
constexpr std::uint8_t kCmdSetParaCfg         = 0x19;
constexpr std::uint8_t kCmdSetUsbString       = 0x1B;
constexpr std::uint8_t kCmdSetPollingRate     = 0x1C;

constexpr std::uint8_t kCmdReset              = 0x0F;

constexpr std::uint8_t kCmdLedStatus          = 0x73;
constexpr std::uint8_t kCmdDeviceInfo        = 0x74;
constexpr std::uint8_t kCmdGetUsbString       = 0x75;
constexpr std::uint8_t kCmdPowerOnNotify     = 0x79;

constexpr std::uint8_t kCmdError              = 0xE2;
constexpr std::uint8_t kCmdIndexLoss          = 0xE1;
constexpr std::uint8_t kCmdBaudNegotiate      = 0xA1;

constexpr std::size_t kErrorPacketLen = 4;

constexpr std::size_t kMaxFrameSize = 256;
constexpr std::size_t kMaxUsbStringLen = 64;

constexpr std::size_t kKbGeneralDataLen = 2;
constexpr std::size_t kKbMediaDataLen   = 2;
constexpr std::size_t kMsRelMoveLen     = 2;
constexpr std::size_t kMsRelWheelLen    = 1;
constexpr std::size_t kParaCfgLen       = 4;
constexpr std::size_t kPollingRateLen   = 1;
constexpr std::size_t kUsbStringMinLen  = 2;
constexpr std::size_t kCombinedUsbStringLen = 192;
constexpr std::size_t kLedStatusLen     = 1;

constexpr std::uint8_t kIndexTolerance   = 12;
constexpr std::uint8_t kMaxPendingFrames = 12;

constexpr std::size_t kDeviceInfoPayloadLen = 201;
constexpr std::size_t kDeviceInfoFrameLen   = 205;

constexpr std::uint8_t kStrTypeManufacturer = 0x00;
constexpr std::uint8_t kStrTypeProduct      = 0x01;
constexpr std::uint8_t kStrTypeSerial       = 0x02;

struct KeyboardReport {
    std::uint8_t modifiers;
    std::uint8_t reserved;
    std::array<std::uint8_t, 6> keys;
};

struct KbSingleKeyEvent {
    std::uint8_t usage;
    bool pressed;
};

struct MediaReport {
    std::uint8_t byte1;
    std::uint8_t byte2;
};

struct MouseReport {
    std::uint8_t reserved;
    std::uint8_t buttons;
    std::int8_t  x;
    std::int8_t  y;
    std::int8_t  wheel;
};

struct MouseMoveEvent {
    std::int8_t dx;
    std::int8_t dy;
};

struct MouseWheelEvent {
    std::int8_t wheel;
};

struct ParaCfgData {
    std::uint16_t vid;
    std::uint16_t pid;
};

struct UsbStringData {
    std::uint8_t  type;
    std::uint8_t  len;
    std::array<char, kMaxUsbStringLen> str;
};

struct ChecksumErrorInfo {
    std::uint8_t cmd;
    std::uint8_t expected_checksum;
    std::uint8_t received_checksum;
};

struct LedStatusData {
    std::uint8_t led_byte;
};

struct PollingRateData {
    std::uint8_t rate_ms;
};

struct PendingFrame {
    std::uint8_t index;
    std::uint8_t cmd;
    std::array<std::uint8_t, kMaxFrameSize> data;
    std::uint8_t len;
};

class ProtocolParser {
public:
    using KbCallback        = std::function<void(const KeyboardReport&)>;
    using KbSingleKeyCallback = std::function<void(const KbSingleKeyEvent&)>;
    using MediaCallback     = std::function<void(const MediaReport&)>;
    using MouseCallback     = std::function<void(const MouseReport&)>;
    using MouseMoveCallback = std::function<void(const MouseMoveEvent&)>;
    using MouseWheelCallback = std::function<void(const MouseWheelEvent&)>;
    using ParaCfgCallback   = std::function<void(const ParaCfgData&)>;
    using UsbStringCallback = std::function<void(const UsbStringData&)>;
    using ChecksumErrorCallback = std::function<void(const ChecksumErrorInfo&)>;
    using IndexLossCallback    = std::function<void(std::uint8_t lost_index)>;
    using LedStatusCallback    = std::function<void(const LedStatusData&)>;
    using GetUsbStringCallback = std::function<void()>;
    using ResetCallback         = std::function<void()>;
    using PollingRateCallback   = std::function<void(const PollingRateData&)>;

    ProtocolParser();
    ~ProtocolParser() = default;

    ProtocolParser(const ProtocolParser&) = delete;
    ProtocolParser& operator=(const ProtocolParser&) = delete;

    void feed(const std::uint8_t* data, std::size_t len);

    void setKbCallback(KbCallback cb);
    void setKbSingleKeyCallback(KbSingleKeyCallback cb);
    void setMediaCallback(MediaCallback cb);
    void setMouseCallback(MouseCallback cb);
    void setMouseMoveCallback(MouseMoveCallback cb);
    void setMouseWheelCallback(MouseWheelCallback cb);
    void setParaCfgCallback(ParaCfgCallback cb);
    void setUsbStringCallback(UsbStringCallback cb);
    void setChecksumErrorCallback(ChecksumErrorCallback cb);
    void setIndexLossCallback(IndexLossCallback cb);
    void setLedStatusCallback(LedStatusCallback cb);
    void setGetUsbStringCallback(GetUsbStringCallback cb);
    void setResetCallback(ResetCallback cb);
    void setPollingRateCallback(PollingRateCallback cb);

    bool hasReceivedValidFrame() const { return frame_received_; }
    void clearFrameReceivedFlag() { frame_received_ = false; }

    bool pollPendingFrame();

    void reset();

private:
    enum class State {
        WaitHdr1,
        WaitHdr2,
        WaitCmd,
        WaitData,
        WaitIndex,
        WaitChecksum
    };

    State state_;
    std::uint8_t cmd_code_;
    std::uint16_t expected_data_len_;
    std::uint8_t data_recv_;
    std::uint8_t pkt_index_;
    std::uint8_t checksum_acc_;
    bool has_index_;

    std::array<std::uint8_t, kMaxFrameSize> frame_buf_;

    KbCallback        kb_cb_;
    KbSingleKeyCallback kb_single_cb_;
    MediaCallback     media_cb_;
    MouseCallback     mouse_cb_;
    MouseMoveCallback mouse_move_cb_;
    MouseWheelCallback mouse_wheel_cb_;
    ParaCfgCallback   para_cfg_cb_;
    UsbStringCallback usb_str_cb_;
    ChecksumErrorCallback checksum_err_cb_;
    IndexLossCallback    idx_loss_cb_;
    LedStatusCallback led_status_cb_;
    GetUsbStringCallback get_usb_string_cb_;
    ResetCallback reset_cb_;
    PollingRateCallback polling_rate_cb_;

    void dispatchCommand(std::uint8_t cmd, const std::uint8_t* data, std::uint8_t len);
    void handleIndexedFrame(std::uint8_t index, std::uint8_t cmd,
                            const std::uint8_t* data, std::uint8_t len);
    void flushPendingFrames();
    void addPendingFrame(std::uint8_t index, std::uint8_t cmd,
                         const std::uint8_t* data, std::uint8_t len);
    bool hasPendingIndex(std::uint8_t idx) const;
    static bool cmdHasIndex(std::uint8_t cmd);
    void addRecentIndex(std::uint8_t idx);
    bool isRecentIndex(std::uint8_t idx) const;

    std::uint8_t last_idx_;
    bool idx_initialized_;

    std::array<PendingFrame, kMaxPendingFrames> pending_queue_;
    std::uint8_t pending_count_;

    static constexpr std::uint8_t kRecentIdxSize = 8;
    std::array<std::uint8_t, kRecentIdxSize> recent_idxs_;
    std::uint8_t recent_idx_pos_;
    std::uint8_t recent_idx_count_;

    bool frame_received_ = false;
};

} // namespace protocol

#endif // PROTOCOL_PROTOCOL_PARSER_H