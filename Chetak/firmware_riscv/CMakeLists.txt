cmake_minimum_required(VERSION 3.13)

# =============================================================================
# 1. PATH CONFIGURATION
# =============================================================================

# The Path to your Pico SDK (Keep this matching your installation)
set(PICO_SDK_PATH "C:/Users/panda/.pico-sdk/sdk/2.2.0")

# The Path to your NEW RISC-V Compiler
# Note: We point to the folder *above* 'bin' so CMake can find the structure.
set(PICO_TOOLCHAIN_PATH "C:/riscv/xpack-riscv-none-elf-gcc-15.2.0-1")

# CRITICAL: The xPack compiler is named 'riscv-none-elf-gcc', but Pico SDK 
# defaults to looking for 'riscv32-unknown-elf'. We must tell it the correct name.
set(PICO_GCC_TRIPLE "riscv-none-elf")

# =============================================================================
# 2. BOARD & PLATFORM SETUP
# =============================================================================

# Target the Raspberry Pi Pico 2
set(PICO_BOARD pico2)

# Force the chip into RISC-V Mode (This enables the secure sandbox features)
set(PICO_PLATFORM rp2350-riscv)

# Skip building the tool that caused the 'ld returned 116' crash
set(PICO_NO_PICOTOOL 1)

# =============================================================================
# 3. PROJECT SETUP
# =============================================================================

include(pico_sdk_import.cmake)

project(secure_iot_sandbox C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# =============================================================================
# 4. EXECUTABLE & LIBRARIES
# =============================================================================

add_executable(secure_iot
    main.c
    crypto/aes.c
)

# Allow main.c to find aes.h
target_include_directories(secure_iot PRIVATE crypto)

# Enable USB Logging (so you can see "ATTACK DETECTED" messages)
pico_enable_stdio_usb(secure_iot 1)
pico_enable_stdio_uart(secure_iot 0)

target_link_libraries(secure_iot
    pico_stdlib
    hardware_base
    hardware_sync
)

# Generate the .uf2 file for flashing
pico_add_extra_outputs(secure_iot)