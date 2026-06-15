// src/md5.h
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t state[4];    // A, B, C, D
    uint32_t count[2];    // longueur en bits
    uint8_t  buffer[64];  // bloc courant
} md5_ctx_t;

void md5_init(md5_ctx_t *ctx);
void md5_update(md5_ctx_t *ctx, const void *data, size_t len);
void md5_final(md5_ctx_t *ctx, uint8_t digest[16]);

// Fonction tout-en-un
void md5_compute(const void *data, size_t len, uint8_t digest[16]);
