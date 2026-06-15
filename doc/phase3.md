# Phase 3 — Générateur de partition table from scratch

> **Projet** : `partforge` — Générateur de partition table ESP32-S3 from scratch
> **Objectif** : Produire un `ptable.bin` valide sans IDF, identique byte par byte à la référence
> **Statut** : ✅ Complété

---

## Table des matières

1. [Ce qu'on a produit](#1-ce-quon-a-produit)
2. [md5.c/h — algo from scratch](#2-md5ch--algo-from-scratch)
3. [test_md5.c — validation MD5](#3-test_md5c--validation-md5)
4. [ptable_gen.c — générateur](#4-ptable_genc--générateur)
5. [tools/verify.py — validation finale](#5-toolsverifypy--validation-finale)
6. [Makefile — mise à jour](#6-makefile--mise-à-jour)
7. [Découverte critique — little-endian du magic](#7-découverte-critique--little-endian-du-magic)
8. [Points critiques à retenir](#8-points-critiques-à-retenir)

---

## 1. Ce qu'on a produit

| Fichier | Rôle |
|---|---|
| `src/md5.h` | Header du module MD5 |
| `src/md5.c` | Algo MD5 RFC 1321 from scratch |
| `tests/test_md5.c` | Tests de validation MD5 |
| `src/ptable_gen.c` | Générateur — produit un ptable.bin valide |
| `tools/verify.py` | Validation ptable_gen.bin vs ptable_ref.bin |
| `Makefile` | Mise à jour — ajout de `make verify` |

---

## 2. `md5.c/h` — algo from scratch

### Pourquoi MD5 et pas CRC32

Découvert en Phase 2 : IDF utilise MD5, pas CRC32, pour le checksum de la partition table.

```python
# gen_esp32part.py (IDF)
result += MD5_PARTITION_BEGIN + hashlib.md5(result).digest()
```

### Structure de l'algo MD5 (RFC 1321)

```
Message de n bytes
    ↓
1. Padding → multiple de 512 bits
2. Init état A, B, C, D
3. 64 rounds par bloc de 512 bits
4. Digest = A||B||C||D en little-endian → 16 bytes
```

### Padding RFC 1321

```
Message original
→ ajouter byte 0x80
→ ajouter bytes 0x00 jusqu'à longueur ≡ 56 mod 64
→ ajouter longueur originale sur 8 bytes little-endian
→ résultat toujours multiple de 64 bytes
```

### Initialisation

```c
/* Valeurs initiales définies par RFC 1321 */
ctx->state[0] = 0x67452301;  /* A */
ctx->state[1] = 0xEFCDAB89;  /* B */
ctx->state[2] = 0x98BADCFE;  /* C */
ctx->state[3] = 0x10325476;  /* D */
```

### Les 4 fonctions auxiliaires

```c
#define F(b, c, d)  (((b) & (c)) | (~(b) & (d)))  /* Ronde 1 */
#define G(b, c, d)  (((b) & (d)) | ((c) & ~(d)))  /* Ronde 2 */
#define H(b, c, d)  ((b) ^ (c) ^ (d))             /* Ronde 3 */
#define I(b, c, d)  ((c) ^ ((b) | ~(d)))          /* Ronde 4 */
```

### Les 64 constantes T

```
T[i] = floor(abs(sin(i+1)) * 2^32)
Précalculées, fixes pour tout message.
```

### Piège principal — little-endian

MD5 travaille entièrement en little-endian :

```c
/* Lecture d'un mot 32 bits depuis le bloc */
static inline uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | (uint32_t)p[1] << 8
         | (uint32_t)p[2] << 16
         | (uint32_t)p[3] << 24;
}
```

### API produite

```c
void md5_init(md5_ctx_t *ctx);
void md5_update(md5_ctx_t *ctx, const void *data, size_t len);
void md5_final(md5_ctx_t *ctx, uint8_t digest[16]);
void md5_compute(const void *data, size_t len, uint8_t digest[16]);
```

`md5_update()` supporte les appels multiples (streaming) — le résultat est identique à `md5_compute()` en un seul appel.

---

## 3. `test_md5.c` — validation MD5

### Vecteurs RFC 1321

| Message | MD5 attendu |
|---|---|
| `""` | `d41d8cd98f00b204e9800998ecf8427e` |
| `"a"` | `0cc175b9c0f1b6a831c399e269772661` |
| `"abc"` | `900150983cd24fb0d6963f7d28e17f72` |
| `"message digest"` | `f96b697d7cb7938d525a2f31aaf161d0` |
| `"abcdefghijklmnopqrstuvwxyz"` | `c3fcd3d76192e4007dfb496cca67e13b` |

### Validation ptable

```
MD5(ptable_ref.bin[0:96]) = f4ad4f4538564b5d7435b62c75b69524 ✅
```

### Test streaming

```
md5_update() en 3 appels == md5_compute() en un appel ✅
```

### Résultat

```
7/7 tests passés ✅
```

---

## 4. `ptable_gen.c` — générateur

### Format produit

```
3 × 32 bytes      ← entrées PartEntry packed
16 bytes          ← MD5_PARTITION_BEGIN : eb eb + 14×ff
16 bytes          ← MD5 digest des entrées
padding 0xFF      ← jusqu'à 0xC00
```

### Table générée

```c
static const PartEntry entries[] = {
    { .magic=PTABLE_MAGIC, .type=DATA, .subtype=NVS,
      .offset=0x9000,  .size=0x6000,   .name="nvs"      },
    { .magic=PTABLE_MAGIC, .type=DATA, .subtype=PHY,
      .offset=0xF000,  .size=0x1000,   .name="phy_init" },
    { .magic=PTABLE_MAGIC, .type=APP,  .subtype=FACTORY,
      .offset=0x10000, .size=0x100000, .name="factory"  },
};
```

### Séquence de génération

```
1. Buffer 0xC00 initialisé à 0xFF
2. Écriture des entrées (3 × 32 bytes)
3. Calcul MD5 des entrées
4. Écriture MD5_PARTITION_BEGIN (eb eb + 14×ff)
5. Écriture digest MD5
6. Écriture fichier
```

### Découverte critique — voir §7

```
PTABLE_MAGIC = 0x50AA dans ptable.h
→ écrit en little-endian par x86
→ donne AA 50 en mémoire
→ correct pour la flash ESP32
```

---

## 5. `tools/verify.py` — validation finale

### Ce que vérifie le script

```
Pour chaque fichier (gen + ref) :
  ✓ Taille == 0xC00
  ✓ Entrées trouvées et parsées
  ✓ MD5_PARTITION_BEGIN = eb eb + 14×ff
  ✓ MD5 digest correct

Comparaison gen vs ref :
  ✓ Même nombre d'entrées
  ✓ Entrées identiques byte par byte
  ✓ Fichiers identiques byte par byte
```

### Piège little-endian dans le parser Python

```python
# struct.unpack("<H", b'\xAA\x50') retourne 0x50AA
# Il faut comparer avec 0x50AA, pas 0xAA50

PTABLE_MAGIC_LE = 0x50AA  # valeur lue par struct.unpack("<H", ...)
PTABLE_END_LE   = 0xEBEB  # identique dans les deux sens
```

### Résultat final

```
11/11 vérifications passées ✅
ptable_gen.bin identique byte par byte à ptable_ref.bin ✅
```

---

## 6. Makefile — mise à jour

```makefile
make test    → compile + tests CRC32 + tests MD5
make verify  → test + génère ptable_gen.bin + valide vs ptable_ref.bin
make run     → verify + flash sur ESP32
make clean   → supprime build/
```

Correction ajoutée : `-static-libgcc` — nécessaire sur Arch Linux avec GCC 16 où le symlink `libgcc_s.so` n'est pas créé automatiquement.

---

## 7. Découverte critique — little-endian du magic

C'est le piège central de cette phase, rencontré trois fois.

### Le magic en flash

```
Flash (hex brut) : AA 50
                   ↑↑ ↑↑
                   byte 0  byte 1
```

### Comment x86 lit ce magic

```
struct.unpack("<H", b'\xAA\x50') → 0x50AA
```

x86 lit en little-endian : le byte de poids faible en premier.
`0xAA` est le byte bas → `0x50AA`.

### Comment x86 écrit un uint16_t

```c
uint16_t magic = 0x50AA;
/* En mémoire (little-endian) :
   byte 0 = 0xAA (poids faible)
   byte 1 = 0x50 (poids fort)
   → AA 50 en flash ✅ */
```

### Conclusion

```
PTABLE_MAGIC = 0x50AA dans ptable.h
→ x86 écrit AA 50 en mémoire
→ identique à la flash ESP32
→ c'est la bonne valeur partout
```

Utiliser `0xAA50` directement dans le générateur aurait donné `50 AA` en mémoire → fichier incorrect.

---

## 8. Points critiques à retenir

| Point | Détail |
|---|---|
| MD5, pas CRC32 | IDF utilise MD5 pour la partition table |
| `PTABLE_MAGIC = 0x50AA` | Valeur à utiliser partout — x86 écrit `AA 50` en mémoire |
| Little-endian omniprésent | MD5, magic, offsets, taille — tout est little-endian |
| `md5_update()` streaming | Résultat identique à `md5_compute()` en un appel |
| `-static-libgcc` | Nécessaire sur Arch Linux GCC 16 |
| `make verify` | Commande principale — tests + génération + validation |
| `ptable_gen.bin` == `ptable_ref.bin` | Validé byte par byte ✅ |

---

## Prochaine étape — Phase 4

Booter du code bare-metal sans aucune ligne d'IDF :

```
1. Linker script minimal (link.ld)
2. Startup call_start_cpu0 (startup.c)
3. Flash bootloader IDF + ptable_gen.bin + app bare-metal
```
