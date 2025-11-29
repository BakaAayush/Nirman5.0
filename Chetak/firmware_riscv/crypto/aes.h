#ifndef _AES_H_
#define _AES_H_

#include <stdint.h>
#include <stddef.h>

// AES128 Encryption Context
struct AES_ctx
{
    uint8_t RoundKey[176];
};

// Function Prototypes
void AES_init_ctx(struct AES_ctx* ctx, const uint8_t* key);
void AES_ECB_encrypt(struct AES_ctx* ctx, uint8_t* buf);
void AES_ECB_decrypt(struct AES_ctx* ctx, uint8_t* buf);

#endif // _AES_H_