#include "md5.h"
#include <string.h>

/*
 * md5.c — MD5 from scratch
 * RFC 1321 — The MD5 Message-Digest Algorithm
 *
 * Structure de l'algo :
 *   1. Padding du message → multiple de 512 bits
 *   2. Init état A, B, C, D
 *   3. 64 rounds par bloc de 512 bits
 *   4. Digest final = A||B||C||D en little-endian
 */

/* ------------------------------------------------------------------ */
/*  Macros utilitaires                                                  */
/* ------------------------------------------------------------------ */

/* Rotation gauche 32 bits */
#define ROTL32(x, n)  (((x) << (n)) | ((x) >> (32 - (n))))

/* Les 4 fonctions auxiliaires MD5 (une par ronde) */
#define F(b, c, d)    (((b) & (c)) | (~(b) & (d)))
#define G(b, c, d)    (((b) & (d)) | ((c) & ~(d)))
#define H(b, c, d)    ((b) ^ (c) ^ (d))
#define I(b, c, d)    ((c) ^ ((b) | ~(d)))

/*
 * Opération MD5 de base :
 *   a = b + ROTL32(a + func(b,c,d) + M[k] + T[i], s)
 */
#define MD5_STEP(func, a, b, c, d, Mk, Ti, s) \
    (a) += func((b), (c), (d)) + (Mk) + (Ti); \
    (a)  = ROTL32((a), (s));                   \
    (a) += (b);

/* ------------------------------------------------------------------ */
/*  64 constantes T[i] = floor(abs(sin(i+1)) * 2^32)                  */
/*  Précalculées, fixes pour tout message                              */
/* ------------------------------------------------------------------ */
static const uint32_t T[64] = {
    /* Ronde 1 */
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
    0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
    0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    /* Ronde 2 */
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
    0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
    0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    /* Ronde 3 */
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
    0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
    0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    /* Ronde 4 */
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
    0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
    0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};

/* Décalages s par ronde (4 × 16 valeurs) */
static const uint8_t S[64] = {
    /* Ronde 1 */ 7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
    /* Ronde 2 */ 5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
    /* Ronde 3 */ 4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
    /* Ronde 4 */ 6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,
};

/* ------------------------------------------------------------------ */
/*  Lecture little-endian 32 bits                                       */
/*  MD5 traite les mots en little-endian                               */
/* ------------------------------------------------------------------ */
static inline uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | (uint32_t)p[1] << 8
         | (uint32_t)p[2] << 16
         | (uint32_t)p[3] << 24;
}

/* ------------------------------------------------------------------ */
/*  Traitement d'un bloc de 512 bits (64 bytes)                        */
/*  C'est le cœur de MD5                                               */
/* ------------------------------------------------------------------ */
static void md5_process_block(md5_ctx_t *ctx, const uint8_t block[64])
{
    /* Décomposer le bloc en 16 mots de 32 bits (little-endian) */
    uint32_t M[16];
    for (int i = 0; i < 16; i++) {
        M[i] = le32(block + i * 4);
    }

    /* Copier l'état courant */
    uint32_t a = ctx->state[0];
    uint32_t b = ctx->state[1];
    uint32_t c = ctx->state[2];
    uint32_t d = ctx->state[3];

    /*
     * 64 rounds — 4 rondes de 16 opérations
     *
     * Ronde 1 : F(b,c,d) — index M[i], s=S[i]
     * Ronde 2 : G(b,c,d) — index M[(5i+1)%16]
     * Ronde 3 : H(b,c,d) — index M[(3i+5)%16]
     * Ronde 4 : I(b,c,d) — index M[(7i)%16]
     */
    for (int i = 0; i < 64; i++) {
        uint32_t f;
        uint32_t g;

        if (i < 16) {
            f = F(b, c, d);
            g = i;
        } else if (i < 32) {
            f = G(b, c, d);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            f = H(b, c, d);
            g = (3 * i + 5) % 16;
        } else {
            f = I(b, c, d);
            g = (7 * i) % 16;
        }

        uint32_t temp = d;
        d = c;
        c = b;
        b = b + ROTL32(a + f + M[g] + T[i], S[i]);
        a = temp;
    }

    /* Accumuler dans l'état */
    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
}

/* ------------------------------------------------------------------ */
/*  API publique                                                        */
/* ------------------------------------------------------------------ */

void md5_init(md5_ctx_t *ctx)
{
    /* Valeurs initiales définies par RFC 1321 */
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;

    ctx->count[0] = 0; /* bits bas  */
    ctx->count[1] = 0; /* bits hauts */

    memset(ctx->buffer, 0, sizeof(ctx->buffer));
}

void md5_update(md5_ctx_t *ctx, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;

    /* Position courante dans le buffer (0-63) */
    uint32_t buf_used = (ctx->count[0] >> 3) & 0x3F;

    /* Mettre à jour le compteur de bits */
    ctx->count[0] += (uint32_t)(len << 3);
    if (ctx->count[0] < (uint32_t)(len << 3)) {
        ctx->count[1]++; /* carry */
    }
    ctx->count[1] += (uint32_t)(len >> 29);

    /* Bytes disponibles dans le buffer */
    uint32_t buf_free = 64 - buf_used;

    size_t offset = 0;

    /* Si on peut remplir et dépasser le buffer courant */
    if (len >= buf_free) {
        memcpy(ctx->buffer + buf_used, p, buf_free);
        md5_process_block(ctx, ctx->buffer);
        offset += buf_free;

        /* Traiter les blocs complets restants directement */
        while (offset + 64 <= len) {
            md5_process_block(ctx, p + offset);
            offset += 64;
        }

        buf_used = 0;
    }

    /* Stocker le reste dans le buffer */
    memcpy(ctx->buffer + buf_used, p + offset, len - offset);
}

void md5_final(md5_ctx_t *ctx, uint8_t digest[16])
{
    /*
     * Padding RFC 1321 :
     *   1. Ajouter byte 0x80
     *   2. Ajouter des 0x00 jusqu'à longueur ≡ 56 mod 64
     *   3. Ajouter longueur originale en bits sur 8 bytes little-endian
     */

    /* Longueur originale en bits avant padding */
    uint8_t bits[8];
    bits[0] = (uint8_t)(ctx->count[0]);
    bits[1] = (uint8_t)(ctx->count[0] >>  8);
    bits[2] = (uint8_t)(ctx->count[0] >> 16);
    bits[3] = (uint8_t)(ctx->count[0] >> 24);
    bits[4] = (uint8_t)(ctx->count[1]);
    bits[5] = (uint8_t)(ctx->count[1] >>  8);
    bits[6] = (uint8_t)(ctx->count[1] >> 16);
    bits[7] = (uint8_t)(ctx->count[1] >> 24);

    /* Ajouter le bit 1 (= byte 0x80) */
    uint8_t pad_byte = 0x80;
    md5_update(ctx, &pad_byte, 1);

    /* Ajouter des 0x00 jusqu'à position 56 mod 64 */
    uint8_t zeros[64] = {0};
    uint32_t buf_used = (ctx->count[0] >> 3) & 0x3F;
    if (buf_used <= 56) {
        md5_update(ctx, zeros, 56 - buf_used);
    } else {
        md5_update(ctx, zeros, 64 - buf_used + 56);
    }

    /* Ajouter la longueur originale sur 64 bits little-endian */
    md5_update(ctx, bits, 8);

    /* Produire le digest — 4 × uint32_t en little-endian */
    for (int i = 0; i < 4; i++) {
        digest[i * 4 + 0] = (uint8_t)(ctx->state[i]);
        digest[i * 4 + 1] = (uint8_t)(ctx->state[i] >>  8);
        digest[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 16);
        digest[i * 4 + 3] = (uint8_t)(ctx->state[i] >> 24);
    }

    /* Effacer le contexte par sécurité */
    memset(ctx, 0, sizeof(*ctx));
}

void md5_compute(const void *data, size_t len, uint8_t digest[16])
{
    md5_ctx_t ctx;
    md5_init(&ctx);
    md5_update(&ctx, data, len);
    md5_final(&ctx, digest);
}
