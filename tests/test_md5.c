#include <stdio.h>
#include <string.h>
#include "md5.h"

/*
 * test_md5.c — Validation MD5 from scratch
 * Vecteurs de référence : RFC 1321 + ptable_ref.bin
 */

/* ------------------------------------------------------------------ */
/*  Utilitaire : compare digest et affiche résultat                    */
/* ------------------------------------------------------------------ */
static int check(const char *label,
                 const uint8_t got[16],
                 const uint8_t expected[16])
{
    printf("%s\n", label);
    printf("  Obtenu  : ");
    for (int i = 0; i < 16; i++) printf("%02x", got[i]);
    printf("\n");
    printf("  Attendu : ");
    for (int i = 0; i < 16; i++) printf("%02x", expected[i]);
    printf("\n");

    if (memcmp(got, expected, 16) == 0) {
        printf("  OK\n\n");
        return 1;
    } else {
        printf("  ERREUR\n\n");
        return 0;
    }
}

/* ------------------------------------------------------------------ */
/*  Vecteurs RFC 1321                                                   */
/* ------------------------------------------------------------------ */
static int test_rfc1321(void)
{
    int passed = 0;
    uint8_t digest[16];

    /* MD5("") = d41d8cd98f00b204e9800998ecf8427e */
    md5_compute("", 0, digest);
    passed += check("MD5(\"\")",
        digest,
        (const uint8_t *)"\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04"
                         "\xe9\x80\x09\x98\xec\xf8\x42\x7e");

    /* MD5("a") = 0cc175b9c0f1b6a831c399e269772661 */
    md5_compute("a", 1, digest);
    passed += check("MD5(\"a\")",
        digest,
        (const uint8_t *)"\x0c\xc1\x75\xb9\xc0\xf1\xb6\xa8"
                         "\x31\xc3\x99\xe2\x69\x77\x26\x61");

    /* MD5("abc") = 900150983cd24fb0d6963f7d28e17f72 */
    md5_compute("abc", 3, digest);
    passed += check("MD5(\"abc\")",
        digest,
        (const uint8_t *)"\x90\x01\x50\x98\x3c\xd2\x4f\xb0"
                         "\xd6\x96\x3f\x7d\x28\xe1\x7f\x72");

    /* MD5("message digest") = f96b697d7cb7938d525a2f31aaf161d0 */
    md5_compute("message digest", 14, digest);
    passed += check("MD5(\"message digest\")",
        digest,
        (const uint8_t *)"\xf9\x6b\x69\x7d\x7c\xb7\x93\x8d"
                         "\x52\x5a\x2f\x31\xaa\xf1\x61\xd0");

    /* MD5("abcdefghijklmnopqrstuvwxyz") = c3fcd3d76192e4007dfb496cca67e13b */
    md5_compute("abcdefghijklmnopqrstuvwxyz", 26, digest);
    passed += check("MD5(\"abcdefghijklmnopqrstuvwxyz\")",
        digest,
        (const uint8_t *)"\xc3\xfc\xd3\xd7\x61\x92\xe4\x00"
                         "\x7d\xfb\x49\x6c\xca\x67\xe1\x3b");

    return passed;
}

/* ------------------------------------------------------------------ */
/*  Validation contre ptable_ref.bin                                   */
/*  MD5 des 96 premiers bytes = f4ad4f4538564b5d7435b62c75b69524      */
/* ------------------------------------------------------------------ */
static int test_ptable(void)
{
    uint8_t digest[16];
    uint8_t buf[96];

    FILE *f = fopen("artifacts/ptable_ref.bin", "rb");
    if (f == NULL) {
        printf("test_ptable : artifacts/ptable_ref.bin non trouvé — SKIP\n\n");
        return 0;
    }

    size_t n = fread(buf, 1, 96, f);
    fclose(f);

    if (n != 96) {
        printf("test_ptable : lecture incomplète (%zu bytes) — ERREUR\n\n", n);
        return 0;
    }

    md5_compute(buf, 96, digest);
    return check("MD5(ptable_ref.bin[0:96])",
        digest,
        (const uint8_t *)"\xf4\xad\x4f\x45\x38\x56\x4b\x5d"
                         "\x74\x35\xb6\x2c\x75\xb6\x95\x24");
}

/* ------------------------------------------------------------------ */
/*  Test md5_update() en plusieurs appels                              */
/*  Vérifie que le résultat est identique à md5_compute() en un appel */
/* ------------------------------------------------------------------ */
static int test_streaming(void)
{
    uint8_t digest_oneshot[16];
    uint8_t digest_stream[16];
    md5_ctx_t ctx;

    const char *msg = "abcdefghijklmnopqrstuvwxyz";
    size_t len = 26;

    /* One-shot */
    md5_compute(msg, len, digest_oneshot);

    /* Streaming — 3 appels */
    md5_init(&ctx);
    md5_update(&ctx, msg,      8);   /* "abcdefgh" */
    md5_update(&ctx, msg + 8,  9);   /* "ijklmnopq" */
    md5_update(&ctx, msg + 17, 9);   /* "rstuvwxyz" */
    md5_final(&ctx, digest_stream);

    return check("MD5 streaming == one-shot",
        digest_stream,
        digest_oneshot);
}

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    printf("══════════════════════════════════════\n");
    printf("  TESTS MD5\n");
    printf("══════════════════════════════════════\n\n");

    int passed = 0;
    int total  = 0;

    /* RFC 1321 — 5 vecteurs */
    int rfc = test_rfc1321();
    passed += rfc;
    total  += 5;

    /* ptable_ref.bin */
    int ptable = test_ptable();
    passed += ptable;
    total  += 1;

    /* Streaming */
    int stream = test_streaming();
    passed += stream;
    total  += 1;

    printf("══════════════════════════════════════\n");
    printf("  %d/%d tests passés\n", passed, total);
    printf("══════════════════════════════════════\n");

    return (passed == total) ? 0 : 1;
}
