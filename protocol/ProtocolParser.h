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

constexpr std::uint8_t kCmdSendKbGeneralData = 0x12;
constexpr std::uint8_t kCmdSendKbMediaData   = 0x13;
constexpr std::uint8_t kCmdSendMsRelData     = 0x15;
constexpr std::uint8_t kCmdSetParaCfg        = 0x19;
constexpr std::uint8_t kCmdSetUsbString      = 0x1B;

constexpr std::size_t kMaxFrameSize = 128;
constexpr std::size_t kHeaderSize   = 5;

constexpr std::size_t kMaxUsbStringLen = 64;

struct KeyboardReport {
    std::uint8_t modifiers;
    std::uint8_t reserved;
    std::array<std::uint8_t, 6> keys;
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

struct ParaCfgData {
    std::uint16_t vid;
    std::uint16_t pid;
};

struct UsbStringData {
    std::uint8_t  type;
    std::uint8_t  len;
    std::array<char, kMaxUsbStringLen> str;
};

class ProtocolParser {
public:
    using KbCallback        = std::function<void(const KeyboardReport&)>;
    using MediaCallback     = std::function<void(const MediaReport&)>;
    using MouseCallback     = std::function<void(const MouseReport&)>;
    using ParaCfgCallback   = std::function<void(const ParaCfgData&)>;
    using UsbStringCallback = std::function<void(const UsbStringData&)>;

    ProtocolParser();
    ~ProtocolParser() = default;

    ProtocolParser(const ProtocolParser&) = delete;
    ProtocolParser& operator=(const ProtocolParser&) = delete;

    void feed(const std::uint8_t* data, std::size_t len);

    void setKbCallback(KbCallback cb);
    void setMediaCallback(MediaCallback cb);
    void setMouseCallback(MouseCallback cb);
    void setParaCfgCallback(ParaCfgCallback cb);
    void setUsbStringCallback(UsbStringCallback cb);

private:
    enum class State {
        WaitHdr1,
        WaitHdr2,
        WaitAddr,
        WaitCmd,
        WaitLen,
        WaitData,
        WaitChecksum
    };

    State state_;
    std::uint8_t addr_code_;
    std::uint8_t cmd_code_;
    std::uint8_t data_len_;
    std::uint8_t data_recv_;
    std::uint8_t checksum_acc_;

    std::array<std::uint8_t, kMaxFrameSize> frame_buf_;

    KbCallback        kb_cb_;
    MediaCallback     media_cb_;
    MouseCallback     mouse_cb_;
    ParaCfgCallback   para_cfg_cb_;
    UsbStringCallback usb_str_cb_;

    void reset();
    void processFrame();
    static std::uint8_t computeChecksum(std::uint8_t hdr1, std::uint8_t hdr2,
                                        std::uint8_t addr, std::uint8_t cmd,
                                        std::uint8_t len,
                                        const std::uint8_t* data);
};

} // namespace protocol

#endif // PROTOCOL_PROTOCOL_PARSER_H
