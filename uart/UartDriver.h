#ifndef UART_UARTDRIVER_H
#define UART_UARTDRIVER_H

#if __cplusplus < 201703L
#error "UartDriver requires C++17 or later"
#endif

#include <cstdint>
#include <cstddef>

namespace uart {

class UartDriver {
public:
    UartDriver();
    ~UartDriver();

    UartDriver(const UartDriver&) = delete;
    UartDriver& operator=(const UartDriver&) = delete;

    void init(std::uint32_t baud_rate);

    std::size_t read(std::uint8_t* buf, std::size_t len);

    bool isReadable() const;

    void write(const std::uint8_t* data, std::size_t len);

private:
    bool initialized_;

    static constexpr std::uint8_t kTXPin = 0;
    static constexpr std::uint8_t kRXPin = 1;
};

} // namespace uart

#endif // UART_UARTDRIVER_H