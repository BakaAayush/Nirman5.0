#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/structs/systick.h"

// --- INCLUDE YOUR CRYPTO LIBRARY ---
// Make sure you have added 'crypto' to your CMakeLists.txt include path
#include "aes.h" 

// --- MEMORY MAP & KEYS ---
// In a real device, this key is burned into OTP (One-Time Programmable) memory.
// It is ONLY accessible by the Kernel (Machine Mode).
// AES-128 Key (16 bytes)
volatile uint8_t SECRET_KEY[16] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

// --- SYSCALL NUMBERS ---
#define SYSCALL_ENCRYPT 1
#define SYSCALL_EXIT    99

// --- PROTOTYPES ---
void sandbox_main();
void kernel_trap_handler();

// ============================================================================
// PART 1: THE KERNEL (TRUSTED ZONE)
// ============================================================================

// The Secure Crypto Engine (Now using Real AES-128)
void kernel_crypto_engine(uint8_t *data, int len) {
    // 1. Initialize the AES Context with our secret key
    struct AES_ctx ctx;
    AES_init_ctx(&ctx, (const uint8_t*)SECRET_KEY);

    // 2. Encrypt the data
    // AES-ECB encrypts in 16-byte blocks.
    // For this demo, we assume the sandbox sends data in multiples of 16 bytes.
    for (int i = 0; i < len; i += 16) {
        AES_ECB_encrypt(&ctx, data + i);
    }   
}

// The Trap Handler: Catches Syscalls (ECALL) and Security Violations
void __attribute__((interrupt("machine"))) machine_exception_handler() {
    uint32_t cause;
    uint32_t epc;
    
    // Read RISC-V CSRs (Control Status Registers)
    asm volatile("csrr %0, mcause" : "=r"(cause));
    asm volatile("csrr %0, mepc" : "=r"(epc));

    // --- CASE A: SYSTEM CALL (Valid Request) ---
    // Cause 8 = Environment call from User mode
    if (cause == 8) {
        // Read arguments from registers a0 (ID) and a1 (Data Pointer)
        register long a0 asm("a0");
        register long a1 asm("a1");
        register long syscall_id = a0;

        if (syscall_id == SYSCALL_ENCRYPT) {
            uint8_t* buffer = (uint8_t*) a1;
            int len = 16; // Fixed 16-byte block for this demo
            
            // EXECUTE SECURE FUNCTION
            kernel_crypto_engine(buffer, len);
            
            printf("[KERNEL] AES Engine: Data Encrypted successfully.\n");
        }
        else if (syscall_id == SYSCALL_EXIT) {
            printf("[KERNEL] Sandbox requested exit. Halting.\n");
            while(1);
        }

        // Advance Program Counter (PC) to next instruction so we don't loop forever
        asm volatile("csrw mepc, %0" :: "r"(epc + 4));
        
    } 
    // --- CASE B: SECURITY VIOLATION (Attack Attempt) ---
    // Cause 5 = Load Access Fault (Reading forbidden memory)
    // Cause 7 = Store Access Fault (Writing forbidden memory)
    else if (cause == 5 || cause == 7) {
        printf("\n[!!! SECURITY ALERT !!!] ILLEGAL MEMORY ACCESS DETECTED!\n");
        printf("[KERNEL] Source: Sandbox (User Mode)\n");
        printf("[KERNEL] Faulting Address: PC=%lx\n", epc);
        printf("[KERNEL] Action: BLOCKING ACCESS & REBOOTING SANDBOX\n");
        
        // In a real system, we would log this to flash and restart the thread.
        // For demo, we hang here to show the alert.
        while(1) tight_loop_contents(); 
    }
    else {
        printf("[KERNEL] Unhandled Exception. Cause: %ld\n", cause);
        while(1);
    }
}

// Helper to switch CPU from Machine Mode (Privileged) to User Mode (Restricted)
void switch_to_user_mode(void (*entry_point)()) {
    // 1. Set MSTATUS: Clear MPP bits to 00 (User Mode)
    uint32_t mstatus;
    asm volatile("csrr %0, mstatus" : "=r"(mstatus));
    mstatus &= ~(3 << 11); 
    asm volatile("csrw mstatus, %0" :: "r"(mstatus));

    // 2. Set MEPC: The address to jump to (Sandbox function)
    asm volatile("csrw mepc, %0" :: "r"(entry_point));

    // 3. Setup PMP (Simplified for Demo)
    // In a full implementation, you would configure PMPCFG/PMPADDR registers here
    // to physically lock the SECRET_KEY memory range.

    printf("[KERNEL] Privilege Drop: Switching to User Mode...\n");
    // 4. Execute MRET (Return from Machine Mode -> User Mode)
    asm volatile("mret");
}

int main() {
    stdio_init_all();
    sleep_ms(3000); // Wait for Serial Monitor
    
    printf("\n==========================================\n");
    printf("   SECURE RISC-V IOT SYSTEM (AES-128)     \n");
    printf("==========================================\n");

    // Install the Trap Handler
    asm volatile("csrw mtvec, %0" :: "r"(machine_exception_handler));

    // Handover control to the Untrusted Sandbox
    switch_to_user_mode(sandbox_main);

    // Should never reach here
    while(1);
}

// ============================================================================
// PART 2: THE SANDBOX (UNTRUSTED USER MODE)
// ============================================================================

// Wrapper function to call the Kernel
void syscall_encrypt_data(uint8_t* data) {
    register long a0 asm("a0") = SYSCALL_ENCRYPT;
    register long a1 asm("a1") = (long)data;
    asm volatile("ecall" : "+r"(a0) : "r"(a1) : "memory");
}

void sandbox_main() {
    // Buffer must be 16 bytes for AES-128 (padded with spaces)
    uint8_t data_packet[17] = "Temp: 24.5C     "; 
    int counter = 0;

    while(1) {
        printf("\n--- [SANDBOX] Cycle %d ---\n", counter++);
        
        // 1. Prepare Data
        sprintf((char*)data_packet, "Temp: %d.0C     ", 20 + (counter % 10));
        printf("[SANDBOX] Collecting Sensor Data: %s\n", data_packet);

        // 2. Send to Kernel for Encryption
        printf("[SANDBOX] Invoking Syscall for AES Encryption...\n");
        syscall_encrypt_data(data_packet);
        
        // 3. Print Encrypted Result (Hex Dump)
        printf("[SANDBOX] Received Encrypted Blob: ");
        for(int i=0; i<16; i++) {
            printf("%02X ", data_packet[i]);
        }
        printf("\n");

        // 4. ATTACK SIMULATION (Triggered on Cycle 3)
        if (counter == 3) {
            printf("\n[SANDBOX] >:D ATTACK SEQUENCE INITIATED...\n");
            printf("[SANDBOX] Attempting to read Kernel Key directly...\n");
            
            // This is a direct memory access to the SECRET_KEY variable.
            // If PMP is working, this instruction will TRIGGER A TRAP (Cause 5).
            volatile uint8_t stolen_byte = SECRET_KEY[0]; 
            
            // If we reach here, PMP failed!
            printf("[SANDBOX] SUCCESS! I stole a byte: %02X\n", stolen_byte);
        }

        // Delay
        for(volatile int i=0; i<5000000; i++); 
    }
}