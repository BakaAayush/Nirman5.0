/*
 * RP2350 RISC-V Physical Memory Protection (PMP) Implementation
 * Secure IoT Sandbox with Hardware-Enforced Privilege Separation
 * 
 * Target: Raspberry Pi Pico 2 (RP2350) - Hazard3 RISC-V cores
 * Architecture: Machine Mode (M-Mode) Kernel + User Mode (U-Mode) Sandbox
 */

// Standard C library headers
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// Pico SDK headers
#include "pico/stdlib.h"
#include "pico/time.h"

// RISC-V specific headers (check availability in your SDK version)
#ifdef __riscv
    #include "hardware/riscv.h"
    #include "hardware/hazard3.h"
#endif

// ============================================================================
// MEMORY LAYOUT DEFINITIONS (Must match linker script)
// ============================================================================

// Kernel Memory (M-Mode) - Protected from U-Mode
#define KERNEL_FLASH_BASE    0x10000000
#define KERNEL_FLASH_SIZE    (64 * 1024)      // 64KB
#define KERNEL_RAM_BASE      0x20080000       // SRAM Bank 0 (non-striped)
#define KERNEL_RAM_SIZE      (4 * 1024)       // 4KB

// Sandbox Memory (U-Mode) - Restricted access
#define SANDBOX_FLASH_BASE   0x10010000       // 16KB aligned
#define SANDBOX_FLASH_SIZE   (16 * 1024)      // 16KB
#define SANDBOX_RAM_BASE     0x20082000       // SRAM Bank 1 (4KB aligned)
#define SANDBOX_RAM_SIZE     (4 * 1024)       // 4KB

// Shared IPC Buffer - Both modes can access
#define SHARED_IPC_BASE      0x20084000       // 256B aligned
#define SHARED_IPC_SIZE      256

// ============================================================================
// PMP CONFIGURATION STRUCTURES
// ============================================================================

typedef enum {
    PMP_OFF   = 0x00,  // Region disabled
    PMP_TOR   = 0x08,  // Top of Range (NOT supported on Hazard3)
    PMP_NA4   = 0x10,  // Naturally Aligned 4-byte
    PMP_NAPOT = 0x18   // Naturally Aligned Power-of-Two
} pmp_mode_t;

typedef enum {
    PMP_NONE  = 0x0,   // No permissions
    PMP_R     = 0x1,   // Read
    PMP_W     = 0x2,   // Write
    PMP_X     = 0x4,   // Execute
    PMP_RW    = 0x3,   // Read + Write
    PMP_RX    = 0x5,   // Read + Execute
    PMP_RWX   = 0x7    // All permissions
} pmp_perm_t;

#define PMP_LOCK  0x80     // Lock bit prevents modification until reset

typedef struct {
    uint32_t base_addr;    // Physical base address
    uint32_t size;         // Region size (must be power of 2 for NAPOT)
    pmp_mode_t mode;       // Addressing mode
    pmp_perm_t perm;       // Access permissions
    bool locked;           // Lock configuration
    const char* name;      // Debug identifier
} pmp_region_t;

// ============================================================================
// PMP REGION TABLE (Priority: pmp0 = Highest)
// ============================================================================

static const pmp_region_t pmp_config[] = {
    // Entry 0: DENY all access to Kernel Flash (highest priority)
    {
        .base_addr = KERNEL_FLASH_BASE,
        .size = KERNEL_FLASH_SIZE,
        .mode = PMP_NAPOT,
        .perm = PMP_NONE,
        .locked = true,
        .name = "Kernel_Flash_Deny"
    },
    
    // Entry 1: DENY all access to Kernel RAM (keys, stack)
    {
        .base_addr = KERNEL_RAM_BASE,
        .size = KERNEL_RAM_SIZE,
        .mode = PMP_NAPOT,
        .perm = PMP_NONE,
        .locked = true,
        .name = "Kernel_RAM_Deny"
    },
    
    // Entry 2: ALLOW Sandbox to execute its code (R+X, no W for W^X)
    {
        .base_addr = SANDBOX_FLASH_BASE,
        .size = SANDBOX_FLASH_SIZE,
        .mode = PMP_NAPOT,
        .perm = PMP_RX,
        .locked = false,
        .name = "Sandbox_Code"
    },
    
    // Entry 3: ALLOW Sandbox data access (R+W, no X for W^X)
    {
        .base_addr = SANDBOX_RAM_BASE,
        .size = SANDBOX_RAM_SIZE,
        .mode = PMP_NAPOT,
        .perm = PMP_RW,
        .locked = false,
        .name = "Sandbox_Data"
    },
    
    // Entry 4: Shared IPC buffer (R+W for both modes)
    {
        .base_addr = SHARED_IPC_BASE,
        .size = SHARED_IPC_SIZE,
        .mode = PMP_NAPOT,
        .perm = PMP_RW,
        .locked = false,
        .name = "Shared_IPC"
    }
};

#define PMP_NUM_REGIONS (sizeof(pmp_config) / sizeof(pmp_region_t))

// ============================================================================
// NAPOT ADDRESS ENCODING
// ============================================================================

/*
 * Calculate NAPOT-encoded address for PMP
 * Formula: pmpaddr = (base >> 2) | ((size >> 3) - 1)
 * 
 * For a region of size 2^S bytes at address A:
 * - Shift address right by 2 (34-bit addressing -> 32-bit register)
 * - OR with size mask to encode region bounds
 */
static inline uint32_t pmp_encode_napot(uint32_t base, uint32_t size) {
    // Verify power-of-2 size
    if ((size & (size - 1)) != 0) {
        panic("PMP: Size must be power of 2");
    }
    
    // Verify alignment
    if (base & (size - 1)) {
        panic("PMP: Base not aligned to size");
    }
    
    uint32_t encoded = (base >> 2) | ((size >> 3) - 1);
    return encoded;
}

/*
 * Encode NA4 address (single 4-byte word)
 */
static inline uint32_t pmp_encode_na4(uint32_t addr) {
    return (addr >> 2);
}

// ============================================================================
// PMP CSR ACCESS (RISC-V Privileged Specification)
// ============================================================================

/*
 * Write PMP configuration register (pmpcfgX)
 * Each pmpcfg holds 4 regions (8 bits per region)
 */
static inline void write_pmpcfg(int cfg_index, uint32_t value) {
    switch(cfg_index) {
        case 0: __asm__ volatile("csrw pmpcfg0, %0" :: "r"(value)); break;
        case 1: __asm__ volatile("csrw pmpcfg1, %0" :: "r"(value)); break;
        case 2: __asm__ volatile("csrw pmpcfg2, %0" :: "r"(value)); break;
        case 3: __asm__ volatile("csrw pmpcfg3, %0" :: "r"(value)); break;
        default: panic("Invalid pmpcfg index");
    }
}

/*
 * Write PMP address register (pmpaddrX)
 */
static inline void write_pmpaddr(int addr_index, uint32_t value) {
    switch(addr_index) {
        case 0:  __asm__ volatile("csrw pmpaddr0, %0"  :: "r"(value)); break;
        case 1:  __asm__ volatile("csrw pmpaddr1, %0"  :: "r"(value)); break;
        case 2:  __asm__ volatile("csrw pmpaddr2, %0"  :: "r"(value)); break;
        case 3:  __asm__ volatile("csrw pmpaddr3, %0"  :: "r"(value)); break;
        case 4:  __asm__ volatile("csrw pmpaddr4, %0"  :: "r"(value)); break;
        case 5:  __asm__ volatile("csrw pmpaddr5, %0"  :: "r"(value)); break;
        case 6:  __asm__ volatile("csrw pmpaddr6, %0"  :: "r"(value)); break;
        case 7:  __asm__ volatile("csrw pmpaddr7, %0"  :: "r"(value)); break;
        case 8:  __asm__ volatile("csrw pmpaddr8, %0"  :: "r"(value)); break;
        case 9:  __asm__ volatile("csrw pmpaddr9, %0"  :: "r"(value)); break;
        case 10: __asm__ volatile("csrw pmpaddr10, %0" :: "r"(value)); break;
        case 11: __asm__ volatile("csrw pmpaddr11, %0" :: "r"(value)); break;
        case 12: __asm__ volatile("csrw pmpaddr12, %0" :: "r"(value)); break;
        case 13: __asm__ volatile("csrw pmpaddr13, %0" :: "r"(value)); break;
        case 14: __asm__ volatile("csrw pmpaddr14, %0" :: "r"(value)); break;
        case 15: __asm__ volatile("csrw pmpaddr15, %0" :: "r"(value)); break;
        default: panic("Invalid pmpaddr index");
    }
}

// ============================================================================
// PMP INITIALIZATION
// ============================================================================

void pmp_init(void) {
    printf("\n=== PMP Initialization ===\n");
    
    // Clear all PMP configurations first
    for (int i = 0; i < 4; i++) {
        write_pmpcfg(i, 0);
    }
    
    // Configure each region
    for (size_t i = 0; i < PMP_NUM_REGIONS; i++) {
        const pmp_region_t *region = &pmp_config[i];
        
        printf("PMP[%d] %s: 0x%08X - 0x%08X (%dKB) ", 
               (int)i, region->name, 
               region->base_addr, 
               region->base_addr + region->size - 1,
               region->size / 1024);
        
        // Calculate encoded address
        uint32_t pmpaddr_value;
        if (region->mode == PMP_NAPOT) {
            pmpaddr_value = pmp_encode_napot(region->base_addr, region->size);
            printf("[NAPOT] ");
        } else if (region->mode == PMP_NA4) {
            pmpaddr_value = pmp_encode_na4(region->base_addr);
            printf("[NA4] ");
        } else {
            printf("ERROR: Invalid mode\n");
            continue;
        }
        
        // Write address register
        write_pmpaddr(i, pmpaddr_value);
        
        // Build configuration byte
        uint8_t cfg_byte = region->mode | region->perm;
        if (region->locked) {
            cfg_byte |= PMP_LOCK;
            printf("LOCKED ");
        }
        
        // Print permissions
        printf("[%c%c%c]\n",
               (region->perm & PMP_R) ? 'R' : '-',
               (region->perm & PMP_W) ? 'W' : '-',
               (region->perm & PMP_X) ? 'X' : '-');
        
        // Write configuration (4 regions per pmpcfg register)
        int cfg_reg = i / 4;
        int cfg_pos = (i % 4) * 8;
        
        uint32_t pmpcfg_val;
        switch(cfg_reg) {
            case 0: __asm__ volatile("csrr %0, pmpcfg0" : "=r"(pmpcfg_val)); break;
            case 1: __asm__ volatile("csrr %0, pmpcfg1" : "=r"(pmpcfg_val)); break;
            case 2: __asm__ volatile("csrr %0, pmpcfg2" : "=r"(pmpcfg_val)); break;
            case 3: __asm__ volatile("csrr %0, pmpcfg3" : "=r"(pmpcfg_val)); break;
        }
        
        pmpcfg_val &= ~(0xFF << cfg_pos);         // Clear existing
        pmpcfg_val |= (cfg_byte << cfg_pos);      // Set new config
        write_pmpcfg(cfg_reg, pmpcfg_val);
    }
    
    printf("=== PMP Configuration Complete ===\n\n");
}

// ============================================================================
// PRIVILEGE LEVEL MANAGEMENT
// ============================================================================

/*
 * Read current privilege level from mstatus.MPP
 */
static inline uint32_t get_privilege_level(void) {
    uint32_t mstatus;
    __asm__ volatile("csrr %0, mstatus" : "=r"(mstatus));
    return (mstatus >> 11) & 0x3;  // MPP bits [12:11]
}

/*
 * Enter User Mode (U-Mode) by performing spoofed exception return
 * Sets up mepc and mstatus, then executes mret
 */
void enter_user_mode(void (*user_function)(void)) {
    printf("Entering User Mode at 0x%08X\n", (uint32_t)user_function);
    
    // Set exception return address to user function
    __asm__ volatile("csrw mepc, %0" :: "r"(user_function));
    
    // Configure mstatus for U-Mode return
    uint32_t mstatus;
    __asm__ volatile("csrr %0, mstatus" : "=r"(mstatus));
    
    // Clear MPP (Machine Previous Privilege) to 0b00 = User Mode
    mstatus &= ~(0x3 << 11);
    
    // Set MPIE (previous interrupt enable) - optional
    mstatus |= (1 << 7);
    
    __asm__ volatile("csrw mstatus, %0" :: "r"(mstatus));
    
    // Store M-Mode stack in mscratch for trap handler recovery
    uint32_t kernel_sp;
    __asm__ volatile("mv %0, sp" : "=r"(kernel_sp));
    __asm__ volatile("csrw mscratch, %0" :: "r"(kernel_sp));
    
    // Execute mret: jumps to mepc in U-Mode
    __asm__ volatile("mret");
    
    // Never reached (now in U-Mode)
    __builtin_unreachable();
}

// ============================================================================
// TRAP HANDLER (Security Monitor)
// ============================================================================

/*
 * Machine trap handler - responds to all exceptions/interrupts in M-Mode
 * Analyzes mcause to determine action
 */
void __attribute__((interrupt)) machine_trap_handler(void) {
    uint32_t mcause, mepc, mtval;
    
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));
    __asm__ volatile("csrr %0, mepc" : "=r"(mepc));
    __asm__ volatile("csrr %0, mtval" : "=r"(mtval));
    
    printf("\n!!! TRAP OCCURRED !!!\n");
    printf("mcause: 0x%08X ", mcause);
    printf("mepc: 0x%08X ", mepc);
    printf("mtval: 0x%08X\n", mtval);
    
    // Decode mcause (bit 31 = interrupt flag, bits 30:0 = code)
    bool is_interrupt = mcause & (1U << 31);
    uint32_t cause_code = mcause & 0x7FFFFFFF;
    
    if (is_interrupt) {
        printf("Type: INTERRUPT (code %d)\n", cause_code);
        // Handle timer/external interrupts
    } else {
        printf("Type: EXCEPTION (code %d) - ", cause_code);
        
        switch(cause_code) {
            case 2:
                printf("ILLEGAL INSTRUCTION\n");
                break;
            case 5:
                printf("LOAD ACCESS FAULT - PMP VIOLATION\n");
                printf(">>> Sandbox attempted unauthorized READ from 0x%08X\n", mtval);
                break;
            case 7:
                printf("STORE ACCESS FAULT - PMP VIOLATION\n");
                printf(">>> Sandbox attempted unauthorized WRITE to 0x%08X\n", mtval);
                break;
            case 8:
                printf("ECALL from U-Mode (System Call)\n");
                // Handle syscall here
                break;
            default:
                printf("UNKNOWN EXCEPTION\n");
                break;
        }
    }
    
    // Security decision: terminate sandbox on violation
    if (cause_code == 5 || cause_code == 7) {
        printf("\n*** SECURITY VIOLATION DETECTED ***\n");
        printf("*** TERMINATING SANDBOX ***\n\n");
        
        // Don't return to faulting code - jump back to kernel
        // In production: reset sandbox or entire device
        while(1) { tight_loop_contents(); }
    }
}

// ============================================================================
// EXAMPLE SANDBOX FUNCTIONS
// ============================================================================

/*
 * Safe sandbox function - only accesses allowed memory
 */
__attribute__((section(".sandbox_section")))
void sandbox_safe_function(void) {
    printf("[Sandbox] Safe execution in U-Mode\n");
    
    // Access sandbox RAM (allowed by PMP entry 3)
    volatile uint32_t *sandbox_data = (uint32_t*)SANDBOX_RAM_BASE;
    *sandbox_data = 0xDEADBEEF;
    printf("[Sandbox] Wrote to sandbox RAM: 0x%08X\n", *sandbox_data);
    
    // Access shared IPC (allowed by PMP entry 4)
    volatile uint32_t *ipc = (uint32_t*)SHARED_IPC_BASE;
    *ipc = 0xCAFEBABE;
    printf("[Sandbox] Wrote to IPC buffer: 0x%08X\n", *ipc);
    
    printf("[Sandbox] Safe function completed\n");
}

/*
 * Malicious sandbox - attempts to access kernel memory (PMP violation)
 */
__attribute__((section(".sandbox_section")))
void sandbox_attack_kernel_ram(void) {
    printf("[Sandbox] Attempting kernel RAM attack...\n");
    
    // Try to read kernel memory (PMP entry 1 blocks this)
    volatile uint32_t *kernel_secret = (uint32_t*)KERNEL_RAM_BASE;
    
    printf("[Sandbox] Reading 0x%08X...\n", (uint32_t)kernel_secret);
    uint32_t stolen_data = *kernel_secret;  // TRAP: Load Access Fault
    
    // Never reached
    printf("[Sandbox] Stolen data: 0x%08X\n", stolen_data);
}

/*
 * Peripheral attack - tries to access hardware directly
 */
__attribute__((section(".sandbox_section")))
void sandbox_attack_peripheral(void) {
    printf("[Sandbox] Attempting peripheral attack...\n");
    
    // Try to access GPIO registers directly (not in PMP whitelist)
    volatile uint32_t *gpio_reg = (uint32_t*)0x40014000;
    
    printf("[Sandbox] Reading GPIO at 0x%08X...\n", (uint32_t)gpio_reg);
    uint32_t gpio_val = *gpio_reg;  // TRAP: Load Access Fault
    
    // Never reached
    printf("[Sandbox] GPIO value: 0x%08X\n", gpio_val);
}

// ============================================================================
// MAIN PROGRAM
// ============================================================================

int main(void) {
    stdio_init_all();
    sleep_ms(2000);  // Wait for USB serial
    
    printf("\n");
    printf("========================================\n");
    printf("RP2350 RISC-V PMP Security Demo\n");
    printf("Hardware-Enforced Privilege Separation\n");
    printf("========================================\n\n");
    
    // Verify we're running on RISC-V
    printf("Architecture: RISC-V Hazard3\n");
    printf("Current Privilege: M-Mode (Machine)\n\n");
    
    // Initialize PMP firewall
    pmp_init();
    
    // Setup trap vector
    extern void machine_trap_handler(void);
    __asm__ volatile("csrw mtvec, %0" :: "r"(&machine_trap_handler));
    printf("Trap handler installed at 0x%08X\n\n", (uint32_t)&machine_trap_handler);
    
    // Demo 1: Safe sandbox execution
    printf("===== TEST 1: Safe Sandbox =====\n");
    enter_user_mode(sandbox_safe_function);
    
    // If sandbox returned safely (rare in this model)
    printf("\n===== TEST 2: Kernel RAM Attack =====\n");
    enter_user_mode(sandbox_attack_kernel_ram);
    
    // Never reached - trap handler catches violation
    printf("\n===== TEST 3: Peripheral Attack =====\n");
    enter_user_mode(sandbox_attack_peripheral);
    
    // Infinite loop (should never get here)
    while(1) {
        tight_loop_contents();
    }
    
    return 0;
}