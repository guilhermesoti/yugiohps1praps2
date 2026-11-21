#ifndef MEMORIES_PC_GUEST_SHA256_H
#define MEMORIES_PC_GUEST_SHA256_H
#include <stddef.h>
#include <stdint.h>

/* SHA-256 of `length` bytes (FIPS 180-4). No allocation and no library
 * calls, so it can run anywhere, including on guest memory. */
void Sha256(const void *data, size_t length, uint8_t digest[32]);

/* The same, in pieces: Sha256Begin, Sha256Add as many times as needed,
 * Sha256End. Gives what Sha256 gives for the pieces put together. */
typedef struct {
    uint32_t h[8];
    uint8_t buffer[64];
    size_t used;
    uint64_t length;
} Sha256Ctx;
void Sha256Begin(Sha256Ctx *ctx);
void Sha256Add(Sha256Ctx *ctx, const void *data, size_t length);
void Sha256End(Sha256Ctx *ctx, uint8_t digest[32]);

#endif
