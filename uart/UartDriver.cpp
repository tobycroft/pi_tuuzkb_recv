#include "UartDriver.h"

#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"

namespace uart {

UartDriver::UartDriver() : initialized_(false) {}

UartDriver::~UartDriver() {
    if (initialized_) {
        uart_deinit(uart0);
    }
}

void UartDriver::init(std::uint32_t baud_rate) {
    if (initialized_) return;

    uart_init(uart0, baud_rate);

    gpio_set_function(kTXPin, GPIO_FUNC_UART);
    gpio_set_function(kRXPin, GPIO_FUNC_UART);

    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart0, true);

    initialized_ = true;
}

std::size_t UartDriver::read(std::uint8_t* buf, std::size_t len) {
    if (!initialized_ || buf == nullptr || len == 0) return 0;

    std::size_t count = 0;
    while (count < len && uart_is_readable(uart0)) {
        buf[count++] = uart_getc(uart0);
    }
    return count;
}

bool UartDriver::isReadable() const {
    if (!initialized_) return false;
    return uart_is_readable(uart0);
}

void UartDriver::write(const std::uint8_t* data, std::size_t len) {
    if (!initialized_ || data == nullptr || len == 0) return;
    uart_write_blocking(uart0, data, len);
}

} // namespace uart