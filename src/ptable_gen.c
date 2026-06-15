#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "ptable.h"
#include "md5.h"

/*
 * ptable_gen.c — Générateur de partition table ESP32-S3 from scratch
 *
 * Produit un ptable.bin valide sans IDF :
 *   N × 32 bytes          ← entrées (PartEntry packed)
 *   16 bytes              ← MD5_PARTITION_BEGIN : eb eb + 14×ff
 *   16 bytes              ← MD5 digest des entrées
 *   padding 0xFF          ← jusqu'à 0xC00
 *
 *     Le magic 0xAA50 est écrit tel quel en flash.
 *     PTABLE_MAGIC = 0x50AA est la valeur lue par x86 (little-endian).
 *     Le générateur écrit pour la flash → on utilise 0xAA50 directement.
 */

/* Magic tel qu'il doit apparaître en flash */
//#define PTABLE_MAGIC_FLASH  0xAA50
#define PTABLE_MAGIC_FLASH  PTABLE_MAGIC

/* ------------------------------------------------------------------ */
/*  Table à générer — même que ptable_ref.bin                          */
/* ------------------------------------------------------------------ */
static const PartEntry entries[] = {
    {
        .magic   = PTABLE_MAGIC_FLASH,
        .type    = PTABLE_TYPE_DATA,
        .subtype = PART_SUBTYPE_NVS,
        .offset  = 0x00009000,
        .size    = 0x00006000,
        .name    = "nvs",
        .flags   = 0,
    },
    {
        .magic   = PTABLE_MAGIC_FLASH,
        .type    = PTABLE_TYPE_DATA,
        .subtype = PART_SUBTYPE_PHY,
        .offset  = 0x0000F000,
        .size    = 0x00001000,
        .name    = "phy_init",
        .flags   = 0,
    },
    {
        .magic   = PTABLE_MAGIC_FLASH,
        .type    = PTABLE_TYPE_APP,
        .subtype = PART_SUBTYPE_FACTORY,
        .offset  = 0x00010000,
        .size    = 0x00100000,
        .name    = "factory",
        .flags   = 0,
    },
};

#define N_ENTRIES  (sizeof(entries) / sizeof(entries[0]))

/* ------------------------------------------------------------------ */
/*  main                                                                */
/* ------------------------------------------------------------------ */
int main(int argc, char *argv[])
{
    const char *outfile = (argc >= 2) ? argv[1] : "artifacts/ptable_gen.bin";

    /* Buffer de sortie — 0xC00 bytes initialisés à 0xFF */
    uint8_t buf[PTABLE_MAX_SIZE];
    memset(buf, 0xFF, sizeof(buf));

    /* 1. Écrire les entrées */
    size_t entries_size = N_ENTRIES * PTABLE_ENTRY_SIZE;
    for (size_t i = 0; i < N_ENTRIES; i++) {
        memcpy(buf + i * PTABLE_ENTRY_SIZE, &entries[i], PTABLE_ENTRY_SIZE);
    }
    printf("Entrées écrites : %zu × %d = %zu bytes\n",
           N_ENTRIES, PTABLE_ENTRY_SIZE, entries_size);

    /* 2. Calculer le MD5 des entrées */
    uint8_t digest[16];
    md5_compute(buf, entries_size, digest);
    printf("MD5 calculé     : ");
    for (int i = 0; i < 16; i++) printf("%02x", digest[i]);
    printf("\n");

    /* 3. Écrire MD5_PARTITION_BEGIN : eb eb + 14×ff + digest */
    uint8_t md5_begin[16];
    md5_begin[0] = 0xEB;
    md5_begin[1] = 0xEB;
    memset(md5_begin + 2, 0xFF, 14);
    memcpy(buf + entries_size,      md5_begin, 16);
    memcpy(buf + entries_size + 16, digest,    16);
    printf("MD5_BEGIN écrit à offset 0x%03zX\n", entries_size);
    printf("MD5 digest écrit à offset 0x%03zX\n", entries_size + 16);

    /* 4. Écrire le fichier */
    FILE *f = fopen(outfile, "wb");
    if (f == NULL) { perror("fopen"); return 1; }
    size_t written = fwrite(buf, 1, PTABLE_MAX_SIZE, f);
    fclose(f);
    if (written != PTABLE_MAX_SIZE) {
        fprintf(stderr, "Erreur écriture : %zu/%d bytes\n", written, PTABLE_MAX_SIZE);
        return 1;
    }
    printf("Fichier généré  : %s (%d bytes)\n", outfile, PTABLE_MAX_SIZE);
    return 0;
}
