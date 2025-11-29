cmake_minimum_required(VERSION 3.13)

# =============================================================================
# 0. FORCE COMPILERS (The "Nuclear" Option)
# =============================================================================
# We hardcode these to prevent VS Code from using the wrong (ARM) compiler.
set(CMAKE_C_COMPILER   "C:/riscv/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-gcc.exe")
set(CMAKE_CXX_COMPILER "C:/riscv/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-g++.exe")
set(CMAKE_ASM_COMPILER "C:/riscv/xpack-riscv-none-elf-gcc-15.2.0-1/bin/riscv-none-elf-gcc.exe")

# =============================================================================
# 1. PATH CONFIGURATION
# =============================================================================
set(PICO_SDK_PATH "C:/Users/panda/.pico-sdk/sdk/2.2.0")
set(PICO_TOOLCHAIN_PATH "C:/riscv/xpack-riscv-none-elf-gcc-15.2.0-1")
set(PICO_GCC_TRIPLE "riscv-none-elf")

# =============================================================================
# 2. HARDWARE SETUP
# =============================================================================
set(PICO_BOARD pico2)
set(PICO_PLATFORM rp2350-riscv)
# Skip building the complex tool that crashes on Windows
set(PICO_NO_PICOTOOL 1)

include(pico_sdk_import.cmake)

project(secure_iot_sandbox C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# =============================================================================
# 3. EXECUTABLE & SOURCES
# =============================================================================
# We name the output 'secure_iot'
add_executable(secure_iot
    main.c
    crypto/aes.c
)

# Tell the compiler where to find aes.h
target_include_directories(secure_iot PRIVATE crypto)

# =============================================================================
# 4. UART CONFIGURATION (Matches ESP8266)
# =============================================================================
# UART 1 | Pin 4 (TX) | Pin 5 (RX) | 9600 Baud
target_compile_definitions(secure_iot PRIVATE
  PICO_DEFAULT_UART=1
  PICO_DEFAULT_UART_TX_PIN=4
  PICO_DEFAULT_UART_RX_PIN=5
  PICO_DEFAULT_UART_BAUD_RATE=9600
)

# =============================================================================
# 5. OUTPUT SETTINGS
# =============================================================================
# Enable USB (1) for Laptop Debugging
pico_enable_stdio_usb(secure_iot 1)

# Disable UART Stdio (0) because we handle it manually in main.c
pico_enable_stdio_uart(secure_iot 0)

target_link_libraries(secure_iot
    pico_stdlib
    hardware_base
    hardware_sync
    hardware_uart
)

# Generate the raw binary files
pico_add_extra_outputs(secure_iot)