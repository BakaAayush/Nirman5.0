#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "aes.h" 

// ============================================
// HARDWARE CONFIGURATION (Must match ESP8266)
// ============================================
#define UART_ID       uart1    // UART Instance 1
#define BAUD_RATE     9600     // 9600 Baud
#define UART_TX_PIN   4        // GP4 (Physical Pin 6) -> Connect to ESP8266 RX
#define UART_RX_PIN   5        // GP5 (Physical Pin 7) -> Connect to ESP8266 TX

// ============================================
// SECURITY CONFIGURATION
// ============================================
// AES-128 Key (Hidden in Kernel Memory - Machine Mode Only)
volatile uint8_t SECRET_KEY[16] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

#define SYSCALL_ENCRYPT 1

// ============================================
// KERNEL FUNCTIONS (Machine Mode - Trusted)
// ============================================

// The actual encryption logic (Protected)
void kernel_crypto_engine(uint8_t *data, int len) {
    struct AES_ctx ctx;
    AES_init_ctx(&ctx, (const uint8_t*)SECRET_KEY);
    // Encrypt in 16-byte blocks
    for (int i = 0; i < len; i += 16) {
        AES_ECB_encrypt(&ctx, data + i);
    }
}

// Trap Handler: Intercepts Sandbox requests and Security Violations
void __attribute__((interrupt("machine"))) machine_exception_handler() {
    uint32_t cause, epc;
    asm volatile("csrr %0, mcause" : "=r"(cause));
    asm volatile("csrr %0, mepc" : "=r"(epc));

    // Case 1: Valid System Call (ECALL from User Mode)
    // mcause 8 = Environment call from U-mode
    if (cause == 8) {
        register long a0 asm("a0");
        register long a1 asm("a1");
        
        if (a0 == SYSCALL_ENCRYPT) {
            // Kernel performs encryption on behalf of the user
            // In a real system, we would check memory bounds here first!
            kernel_crypto_engine((uint8_t*) a1, 16);
        }
        
        // Resume execution at the next instruction
        asm volatile("csrw mepc, %0" :: "r"(epc + 4));
    } 
    // Case 2: Security Violation (Illegal Memory Access)
    // mcause 5 = Load Access Fault, 7 = Store Access Fault
    else if (cause == 5 || cause == 7) {
        printf("\n[KERNEL] !!! SECURITY ALERT !!! Illegal Access at %lx\n", epc);
        
        // Broadcast Alert to ESP8266 immediately so dashboard turns RED
        uart_puts(UART_ID, "SECURITY ALERT: SANDBOX COMPROMISED\n");
        
        // Lock system to prevent data leakage
        while(1) {
            tight_loop_contents();
        }
    }
    else {
        // Unhandled exception, hang
        printf("[KERNEL] Unhandled Trap: %ld\n", cause);
        while(1);
    }
}

// Helper to switch CPU from Machine Mode -> User Mode
void switch_to_user_mode(void (*entry_point)()) {
    uint32_t mstatus;
    asm volatile("csrr %0, mstatus" : "=r"(mstatus));
    
    // Clear MPP bits (11 and 12) to set previous mode to User Mode (00)
    mstatus &= ~(3 << 11); 
    asm volatile("csrw mstatus, %0" :: "r"(mstatus));
    
    // Set the return address to the Sandbox Main function
    asm volatile("csrw mepc, %0" :: "r"(entry_point));
    
    // --- INITIALIZE UART BEFORE JUMPING ---
    // We do this in Kernel mode to ensure hardware is ready
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);

    printf("[KERNEL] Dropping privileges. Jumping to Sandbox...\n");
    
    // Execute MRET (Return from Machine Mode) -> Enters User Mode
    asm volatile("mret");
}

void sandbox_main(); // Forward declaration

int main() {
    // Initialize USB for Laptop Debugging
    stdio_init_all(); 
    sleep_ms(2000);   
    
    printf("--- RISC-V SECURE IOT BOOTING ---\n");

    // Install the Trap Handler
    asm volatile("csrw mtvec, %0" :: "r"(machine_exception_handler));
    
    // Hand over control to the untrusted application
    switch_to_user_mode(sandbox_main);
    
    while(1); // Should never reach here
}

// ============================================
// SANDBOX APPLICATION (User Mode - Untrusted)
// ============================================

// Wrapper to call the Kernel
void syscall_encrypt_data(uint8_t* data) {
    register long a0 asm("a0") = SYSCALL_ENCRYPT;
    register long a1 asm("a1") = (long)data;
    asm volatile("ecall" : "+r"(a0) : "r"(a1) : "memory");
}

void sandbox_main() {
    // Data buffer (16 bytes for AES + null terminator)
    uint8_t data_packet[17] = "Temp: 24.5C     "; 
    int counter = 0;
    char hex_output[64]; 

    while(1) {
        printf("\n--- [SANDBOX] Cycle %d ---\n", counter++);
        
        // 1. Generate Fake Sensor Data
        sprintf((char*)data_packet, "Temp: %d.0C     ", 20 + (counter % 10));
        
        // 2. Ask Kernel to Encrypt
        syscall_encrypt_data(data_packet);
        
        // 3. Format as HEX string for ESP8266 Dashboard
        // Result looks like: "DATA: A4 B5 12 ..."
        sprintf(hex_output, "DATA: ");
        for(int i=0; i<16; i++) {
            sprintf(hex_output + strlen(hex_output), "%02X ", data_packet[i]);
        }
        strcat(hex_output, "\n"); // Important: ESP reads until newline

        // 4. Send to ESP8266 (Wire) AND Laptop (USB)
        printf("[SANDBOX] Sending: %s", hex_output); 
        uart_puts(UART_ID, hex_output);

        // 5. ATTACK SIMULATION (Triggered on Cycle 3)
        if (counter == 3) {
            printf("[SANDBOX] >:D LAUNCHING ATTACK ON KERNEL...\n");
            sleep_ms(500); // Drama pause
            
            // Try to read Kernel Key directly (forbidden!)
            // Because we are in User Mode, PMP checks will fail this load instruction.
            volatile uint8_t stolen = SECRET_KEY[0]; 
            
            // If PMP works, the CPU traps BEFORE executing this line.
            printf("If you see this, the attack succeeded (FAIL!)\n");
        }

        sleep_ms(2000); // Send data every 2 seconds
    }
}