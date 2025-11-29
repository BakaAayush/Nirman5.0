cmake_minimum_required(VERSION 3.13)

# 1. PATH CONFIGURATION
set(PICO_SDK_PATH "C:/Users/panda/.pico-sdk/sdk/2.2.0")
set(PICO_TOOLCHAIN_PATH "C:/riscv/xpack-riscv-none-elf-gcc-15.2.0-1")
set(PICO_GCC_TRIPLE "riscv-none-elf")

# 2. BOARD & PLATFORM
set(PICO_BOARD pico2)
set(PICO_PLATFORM rp2350-riscv)
set(PICO_NO_PICOTOOL 1)

include(pico_sdk_import.cmake)

project(secure_iot_sandbox C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(secure_iot
    main.c
    crypto/aes.c
)

target_include_directories(secure_iot PRIVATE crypto)

# --- CRITICAL CHANGE: SWITCH TO UART1 on GP4/GP5 ---
target_compile_definitions(secure_iot PRIVATE
  PICO_DEFAULT_UART=1           # Use UART Instance 1
  PICO_DEFAULT_UART_TX_PIN=4    # Transmit on Pin 4
  PICO_DEFAULT_UART_RX_PIN=5    # Receive on Pin 5
)

# Enable BOTH USB and UART so you can debug on laptop AND talk to ESP32
pico_enable_stdio_usb(secure_iot 1)
pico_enable_stdio_uart(secure_iot 1)  # <--- MUST BE 1

target_link_libraries(secure_iot
    pico_stdlib
    hardware_base
    hardware_sync
)

pico_add_extra_outputs(secure_iot)