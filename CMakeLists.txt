# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pi_tuuzkb_recv C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# ====== Pi TuuZKB Recv - USB HID Device Firmware ======
#
# 分层架构：
#   /uart               - UART 驱动层
#   /protocol           - 协议解析层
#   /usb_device         - USB HID 设备层 (TinyUSB Device)
#   main.cpp            - 主程序入口

add_executable(pi_tuuzkb_recv
    main.cpp
    uart/UartDriver.cpp
    protocol/ProtocolParser.cpp
    usb_device/UsbDescriptors.cpp
    usb_device/UsbHidDevice.cpp
)

pico_set_program_name(pi_tuuzkb_recv "pi_tuuzkb_recv")
pico_set_program_version(pi_tuuzkb_recv "0.1")

pico_enable_stdio_uart(pi_tuuzkb_recv 0)
pico_enable_stdio_usb(pi_tuuzkb_recv 0)

target_include_directories(pi_tuuzkb_recv PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/uart
    ${CMAKE_CURRENT_LIST_DIR}/protocol
    ${CMAKE_CURRENT_LIST_DIR}/usb_device
)

target_link_libraries(pi_tuuzkb_recv
    pico_stdlib
    hardware_uart
    hardware_gpio
    hardware_flash
    hardware_sync
    tinyusb_device
    tinyusb_board
)

pico_add_extra_outputs(pi_tuuzkb_recv)