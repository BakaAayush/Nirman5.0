#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/uart.h"
#include "aes.h" 

// ============================================
// HARDWARE CONFIGURATION
// ============================================
#define UART_ID       uart1
#define BAUD_RATE     9600
#define UART_TX_PIN   4
#define UART_RX_PIN   5

// ============================================
// SECURITY THRESHOLDS & CONFIG
// ============================================
#define MAX_INPUT_LENGTH 64
#define THREAT_SCORE_THRESHOLD 50  // Out of 100
#define MAX_EXECUTION_TIME_MS 100  // Timeout for sandbox execution

// ============================================
// INTER-CORE COMMUNICATION
// ============================================
volatile bool system_compromised = false;
volatile bool new_input_ready = false;
volatile char shared_input_buffer[128];
volatile int threat_score = 0;
volatile char threat_reason[128];

// ============================================
// SECURITY KEY (Machine Mode Only)
// ============================================
volatile uint8_t SECRET_KEY[16] = {
    0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
    0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
};

#define SYSCALL_ENCRYPT 1

// ============================================
// BEHAVIORAL ANALYSIS STRUCTURES
// ============================================
typedef struct {
    int control_char_count;      // \x00, \x01, etc.
    int sql_injection_score;     // SQL keywords
    int code_injection_score;    // Code patterns
    int overflow_attempt_score;  // Buffer overflow patterns
    int format_string_score;     // %n, %s patterns
    int shellcode_score;         // Binary/hex patterns
    int entropy_score;           // Randomness (encrypted/obfuscated data)
    int suspicious_length;       // Abnormally long input
} ThreatAnalysis;

// ============================================
// AUTONOMOUS THREAT DETECTION ENGINE
// ============================================

// Check for SQL injection patterns
int detect_sql_injection(const char* input) {
    const char* sql_keywords[] = {
        "SELECT", "DROP", "INSERT", "DELETE", "UPDATE", 
        "UNION", "OR 1=1", "'; --", "admin'--"
    };
    int score = 0;
    for (int i = 0; i < 9; i++) {
        if (strstr(input, sql_keywords[i]) != NULL) {
            score += 15;
        }
    }
    return score > 100 ? 100 : score;
}

// Check for code injection patterns
int detect_code_injection(const char* input) {
    const char* code_patterns[] = {
        "system(", "exec(", "eval(", "import os", 
        "__import__", "subprocess", "cmd.exe", "/bin/sh",
        "$(", "${", "`", "&&", "||", ";bash"
    };
    int score = 0;
    for (int i = 0; i < 14; i++) {
        if (strstr(input, code_patterns[i]) != NULL) {
            score += 20;
        }
    }
    return score > 100 ? 100 : score;
}

// Check for buffer overflow attempts
int detect_overflow_attempt(const char* input, int length) {
    int score = 0;
    
    // Excessive length
    if (length > MAX_INPUT_LENGTH) score += 30;
    
    // Repeated characters (NOP sleds, padding)
    int max_repeat = 0, current_repeat = 1;
    for (int i = 1; i < length; i++) {
        if (input[i] == input[i-1]) {
            current_repeat++;
            if (current_repeat > max_repeat) max_repeat = current_repeat;
        } else {
            current_repeat = 1;
        }
    }
    if (max_repeat > 10) score += 25;
    
    // Null bytes (common in exploits)
    for (int i = 0; i < length; i++) {
        if (input[i] == '\0') score += 15;
    }
    
    return score > 100 ? 100 : score;
}

// Check for format string vulnerabilities
int detect_format_string(const char* input) {
    int score = 0;
    const char* format_patterns[] = {"%n", "%s", "%x", "%p", "%d"};
    
    for (int i = 0; i < 5; i++) {
        const char* pos = input;
        while ((pos = strstr(pos, format_patterns[i])) != NULL) {
            score += 10;
            pos++;
        }
    }
    return score > 100 ? 100 : score;
}

// Detect shellcode/binary patterns
int detect_shellcode(const char* input, int length) {
    int score = 0;
    int non_printable = 0;
    int hex_sequence = 0;
    
    // Count non-printable characters
    for (int i = 0; i < length; i++) {
        if (!isprint(input[i]) && input[i] != '\n' && input[i] != '\r' && input[i] != '\t') {
            non_printable++;
        }
        // Check for hex patterns (0x, \x)
        if (input[i] == '\\' && i+1 < length && input[i+1] == 'x') {
            hex_sequence++;
        }
    }
    
    if (non_printable > length * 0.3) score += 40; // >30% non-printable
    if (hex_sequence > 3) score += 30;
    
    return score > 100 ? 100 : score;
}

// Calculate entropy (randomness - indicates obfuscation/encryption)
int calculate_entropy(const char* input, int length) {
    if (length == 0) return 0;
    
    int freq[256] = {0};
    for (int i = 0; i < length; i++) {
        freq[(unsigned char)input[i]]++;
    }
    
    double entropy = 0.0;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            double p = (double)freq[i] / length;
            entropy -= p * (log(p) / log(2));
        }
    }
    
    // High entropy (>7.5) suggests encrypted/obfuscated data
    if (entropy > 7.5) return 30;
    if (entropy > 7.0) return 15;
    return 0;
}

// MASTER THREAT ANALYSIS FUNCTION
ThreatAnalysis analyze_threat(const char* input) {
    ThreatAnalysis analysis = {0};
    int length = strlen(input);
    
    // Run all detection algorithms
    analysis.sql_injection_score = detect_sql_injection(input);
    analysis.code_injection_score = detect_code_injection(input);
    analysis.overflow_attempt_score = detect_overflow_attempt(input, length);
    analysis.format_string_score = detect_format_string(input);
    analysis.shellcode_score = detect_shellcode(input, length);
    analysis.entropy_score = calculate_entropy(input, length);
    analysis.suspicious_length = (length > MAX_INPUT_LENGTH) ? 20 : 0;
    
    // Count control characters
    for (int i = 0; i < length; i++) {
        if (iscntrl(input[i]) && input[i] != '\n' && input[i] != '\r' && input[i] != '\t') {
            analysis.control_char_count++;
        }
    }
    
    return analysis;
}

// Calculate final threat score (0-100)
int calculate_threat_score(ThreatAnalysis analysis) {
    int total = analysis.sql_injection_score +
                analysis.code_injection_score +
                analysis.overflow_attempt_score +
                analysis.format_string_score +
                analysis.shellcode_score +
                analysis.entropy_score +
                analysis.suspicious_length +
                (analysis.control_char_count * 5);
    
    return total > 100 ? 100 : total;
}

// Generate human-readable threat report
void generate_threat_report(ThreatAnalysis analysis, char* output) {
    output[0] = '\0';
    
    if (analysis.sql_injection_score > 0) 
        strcat(output, "SQL_INJ ");
    if (analysis.code_injection_score > 0) 
        strcat(output, "CODE_INJ ");
    if (analysis.overflow_attempt_score > 0) 
        strcat(output, "OVERFLOW ");
    if (analysis.format_string_score > 0) 
        strcat(output, "FMT_STR ");
    if (analysis.shellcode_score > 0) 
        strcat(output, "SHELLCODE ");
    if (analysis.entropy_score > 0) 
        strcat(output, "HIGH_ENTROPY ");
    if (analysis.suspicious_length > 0) 
        strcat(output, "LONG_INPUT ");
    
    if (output[0] == '\0') strcpy(output, "CLEAN");
}

// ============================================
// KERNEL FUNCTIONS
// ============================================

void kernel_crypto_engine(uint8_t *data, int len) {
    struct AES_ctx ctx;
    AES_init_ctx(&ctx, (const uint8_t*)SECRET_KEY);
    for (int i = 0; i < len; i += 16) {
        AES_ECB_encrypt(&ctx, data + i);
    }
}

void __attribute__((interrupt("machine"))) machine_exception_handler() {
    uint32_t cause, epc;
    asm volatile("csrr %0, mcause" : "=r"(cause));
    asm volatile("csrr %0, mepc" : "=r"(epc));

    if (cause == 8) { 
        register long a0 asm("a0");
        register long a1 asm("a1");
        if (a0 == SYSCALL_ENCRYPT) kernel_crypto_engine((uint8_t*) a1, 16);
        asm volatile("csrw mepc, %0" :: "r"(epc + 4));
    } 
    else if (cause == 5 || cause == 7) { 
        system_compromised = true;
        strcpy((char*)threat_reason, "RUNTIME_VIOLATION");
        while(1) tight_loop_contents();
    }
    else { while(1); }
}

void switch_to_user_mode(void (*entry_point)()) {
    uint32_t mstatus;
    asm volatile("csrr %0, mstatus" : "=r"(mstatus));
    mstatus &= ~(3 << 11);
    asm volatile("csrw mstatus, %0" :: "r"(mstatus));
    asm volatile("csrw mepc, %0" :: "r"(entry_point));
    asm volatile("csrw mtvec, %0" :: "r"(machine_exception_handler));
    asm volatile("mret");
}

// ============================================
// CORE 1: SANDBOX WITH ANALYSIS
// ============================================

void syscall_encrypt_data(uint8_t* data) {
    register long a0 asm("a0") = SYSCALL_ENCRYPT;
    register long a1 asm("a1") = (long)data;
    asm volatile("ecall" : "+r"(a0) : "r"(a1) : "memory");
}

void sandbox_main() {
    uint8_t data_block[17];
    char hex_output[256];

    while(1) {
        if (new_input_ready) {
            // Debug: Echo received input
            printf("[SANDBOX] Processing: %s\n", (char*)shared_input_buffer);
            
            // AUTONOMOUS THREAT ANALYSIS
            ThreatAnalysis analysis = analyze_threat((char*)shared_input_buffer);
            threat_score = calculate_threat_score(analysis);
            
            char report[128];
            generate_threat_report(analysis, report);
            strcpy((char*)threat_reason, report);
            
            printf("[SANDBOX] Threat Score: %d | Reason: %s\n", threat_score, report);
            
            // DECISION: Is this input safe?
            if (threat_score >= THREAT_SCORE_THRESHOLD) {
                // MALICIOUS! Trigger security response
                sprintf(hex_output, "THREAT_DETECTED|SCORE:%d|REASON:%s\r\n", 
                        threat_score, report);
                uart_puts(UART_ID, hex_output);
                printf("[SANDBOX] BLOCKED - Sent alert to ESP\n");
                
                // Optionally trigger trap for demonstration
                // volatile uint8_t stolen = SECRET_KEY[0]; 
            } else {
                // SAFE! Proceed with encryption
                memset(data_block, ' ', 16);
                int len = strlen((char*)shared_input_buffer);
                if (len > 16) len = 16;
                memcpy(data_block, (char*)shared_input_buffer, len);
                data_block[16] = 0;

                syscall_encrypt_data(data_block);

                sprintf(hex_output, "DATA: ");
                for(int i=0; i<16; i++) 
                    sprintf(hex_output + strlen(hex_output), "%02X ", data_block[i]);
                sprintf(hex_output + strlen(hex_output), "|THREAT:%d|%s\r\n", 
                        threat_score, report);
                
                uart_puts(UART_ID, hex_output);
                printf("[SANDBOX] SAFE - Encrypted and sent\n");
            }
            
            new_input_ready = false;
        }
    }
}

void core1_entry() {
    switch_to_user_mode(sandbox_main);
}

// ============================================
// CORE 0: MONITOR
// ============================================
int main() {
    stdio_init_all(); // USB Debugging
    sleep_ms(2000); // Wait for USB to initialize
    
    printf("\n\n========================================\n");
    printf("[CORE 0] Autonomous Security System Starting...\n");
    printf("========================================\n");
    
    // Setup Hardware UART
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
    uart_set_format(UART_ID, 8, 1, UART_PARITY_NONE);
    
    printf("[CORE 0] UART initialized on GP%d(TX) and GP%d(RX) @ %d baud\n", 
           UART_TX_PIN, UART_RX_PIN, BAUD_RATE);

    // Launch Core 1 (The Sandbox)
    multicore_launch_core1(core1_entry);
    printf("[CORE 0] Core 1 (Sandbox) launched\n");
    
    // Send test message to ESP8266
    uart_puts(UART_ID, "SYSTEM_READY\r\n");
    printf("[CORE 0] Sent SYSTEM_READY to ESP8266\n");

    char rx_buf[128];
    int rx_pos = 0;

    printf("[CORE 0] Waiting for input from ESP8266...\n\n");

    while(1) {
        // --- 1. MONITOR CORE 1 STATUS ---
        if (system_compromised) {
            printf("[CORE 0] !!!!! ALARM! Malicious Code Detected in Sandbox!\n");
            uart_puts(UART_ID, "SECURITY ALERT: RUNTIME MEMORY VIOLATION\r\n");
            while(1) { sleep_ms(1000); }
        }

        // --- 2. LISTEN TO DASHBOARD (ESP8266) ---
        while (uart_is_readable(UART_ID)) {
            char c = uart_getc(UART_ID);
            if (c == '\n' || c == '\r') {
                rx_buf[rx_pos] = 0;
                if (rx_pos > 0) {
                    printf("[CORE 0] Received from ESP: '%s'\n", rx_buf);
                    
                    // Pass the input to Core 1 to process
                    strcpy((char*)shared_input_buffer, rx_buf);
                    new_input_ready = true;
                    
                    // Wait a moment for Core 1 to process
                    sleep_ms(10);
                }
                rx_pos = 0;
            } else {
                if (rx_pos < 127) rx_buf[rx_pos++] = c;
            }
        }
        
        sleep_ms(10); // Small delay to prevent CPU spinning
    }
}