# Phase 2 — Reverse de la partition table ESP32-S3

> **Projet** : `partforge` — Générateur de partition table ESP32-S3 from scratch  
> **Objectif** : Lire, décoder et valider une partition table réelle extraite de la flash  
> **Statut** : ✅ Complété

---

## Table des matières

1. [Ce qu'on a produit](#1-ce-quon-a-produit)
2. [Outillage](#2-outillage)
3. [ptable.h — structures et constantes](#3-ptableh--structures-et-constantes)
4. [crc32.c/h — algo from scratch](#4-crc32ch--algo-from-scratch)
5. [ptable_parse.c — parser C](#5-ptable_parsec--parser-c)
6. [Makefile](#6-makefile)
7. [CI GitHub Actions](#7-ci-github-actions)
8. [Découverte critique — MD5 et non CRC32](#8-découverte-critique--md5-et-non-crc32)
9. [Points critiques à retenir](#9-points-critiques-à-retenir)

---

## 1. Ce qu'on a produit

| Fichier | Rôle |
|---|---|
| `src/ptable.h` | Structures et constantes de la partition table |
| `src/crc32.h` | Header du module CRC32 |
| `src/crc32.c` | Algo CRC32 ISO 3309 from scratch |
| `src/ptable_parse.c` | Parser — lit et affiche une partition table binaire |
| `tests/test_crc32.c` | Tests de validation CRC32 |
| `Makefile` | Pipeline compile → tests → flash |
| `.github/workflows/ci.yml` | CI GitHub Actions |
| `artifacts/ptable_ref.bin` | Dump de référence extrait de la flash |

---

## 2. Outillage

### Extraction depuis la flash

```bash
esptool.py --port /dev/ttyACM0 read_flash 0x8000 0xC00 artifacts/ptable_ref.bin
```

### Visualisation

```bash
hexyl artifacts/ptable_ref.bin
```

`hexyl` est préféré à `xxd` — les bytes ASCII apparaissent en vert, les bytes binaires en rouge, les NULL en neutre, les `0xFF` (flash vierge) en grisé.

### Dump obtenu

```
┌────────┬─────────────────────────┬─────────────────────────┬────────┬────────┐
│00000000│ aa 50 01 02 00 90 00 00 ┊ 00 60 00 00 6e 76 73 00 │×P••⋄×⋄⋄┊⋄`⋄⋄nvs⋄│
│00000010│ 00 00 00 00 00 00 00 00 ┊ 00 00 00 00 00 00 00 00 │⋄⋄⋄⋄⋄⋄⋄⋄┊⋄⋄⋄⋄⋄⋄⋄⋄│
│00000020│ aa 50 01 01 00 f0 00 00 ┊ 00 10 00 00 70 68 79 5f │×P••⋄×⋄⋄┊⋄•⋄⋄phy_│
│00000030│ 69 6e 69 74 00 00 00 00 ┊ 00 00 00 00 00 00 00 00 │init⋄⋄⋄⋄┊⋄⋄⋄⋄⋄⋄⋄⋄│
│00000040│ aa 50 00 00 00 00 01 00 ┊ 00 00 10 00 66 61 63 74 │×P⋄⋄⋄⋄•⋄┊⋄⋄•⋄fact│
│00000050│ 6f 72 79 00 00 00 00 00 ┊ 00 00 00 00 00 00 00 00 │ory⋄⋄⋄⋄⋄┊⋄⋄⋄⋄⋄⋄⋄⋄│
│00000060│ eb eb ff ff ff ff ff ff ┊ ff ff ff ff ff ff ff ff │××××××××┊××××××××│
│00000070│ f4 ad 4f 45 38 56 4b 5d ┊ 74 35 b6 2c 75 b6 95 24 │××OE8VK]┊t5×,u××$│
│00000080│ ff ff ff ff ff ff ff ff ┊ ff ff ff ff ff ff ff ff │××××××××┊××××××××│
│*       │                         ┊                         │        ┊        │
│00000c00│                         ┊                         │        ┊        │
└────────┴─────────────────────────┴─────────────────────────┴────────┴────────┘
```

---

## 3. `ptable.h` — structures et constantes

### Format d'une entrée (32 bytes)

```
aa 50              → magic       (2 bytes, uint16_t) — lu 0x50AA en little-endian x86
01                 → type        (1 byte,  uint8_t)
02                 → subtype     (1 byte,  uint8_t)
00 90 00 00        → offset      (4 bytes, uint32_t, little-endian)
00 60 00 00        → size        (4 bytes, uint32_t, little-endian)
6e 76 73 00 ...    → name[16]    (16 bytes, char[], null-padded)
00 00 00 00        → flags       (4 bytes, uint32_t)
────────────────────────────────
total              → 32 bytes    ✅ vérifié avec sizeof()
```

### Constantes

```c
#define PTABLE_MAGIC         0x50AA  // 0xAA50 en flash, lu 0x50AA en little-endian
#define PTABLE_END_MARKER    0xEBEB  // magic du bloc MD5 (voir §8)
#define PTABLE_ENTRY_SIZE    32
#define PTABLE_MAX_SIZE      0xC00

#define PTABLE_TYPE_APP      0x00
#define PTABLE_TYPE_DATA     0x01

#define PART_SUBTYPE_FACTORY 0x00
#define PART_SUBTYPE_NVS     0x02
#define PART_SUBTYPE_PHY     0x01
```

### Structure C

```c
typedef struct {
    uint16_t magic;
    uint8_t  type;
    uint8_t  subtype;
    uint32_t offset;
    uint32_t size;
    char     name[16];
    uint32_t flags;
} __attribute__((packed)) PartEntry;
```

`__attribute__((packed))` est obligatoire — sans lui le compilateur ajoute du padding et la structure ne mappe plus les bytes de la flash.

---

## 4. `crc32.c/h` — algo from scratch

### Principe

Le CRC32 est un accumulateur — il lit chaque byte des données, met à jour une variable `uint32_t`, et retourne une empreinte de 4 bytes.

```
données → CRC lit byte par byte → uint32_t résultat
```

Il ne modifie pas les données. Si un seul byte change, le résultat change.

### Table de lookup

256 entrées précalculées — une par valeur de byte possible (`0x00` à `0xFF`). Générée une seule fois au premier appel (lazy init).

### Polynôme

```
Polynôme réfléchi CRC32 ISO 3309 : 0xEDB88320
Vecteur de validation universel   : crc32("123456789") == 0xCBF43926  ✅
```

### Validation

```bash
gcc -Wall -std=c11 -Isrc -o tests/test_crc32 tests/test_crc32.c src/crc32.c
./tests/test_crc32
# CRC32 = 0xCBF43926  ✅
```

---

## 5. `ptable_parse.c` — parser C

### Usage

```bash
./build/ptable_parse artifacts/ptable_ref.bin
```

### Sortie obtenue

```
---
name =nvs
type =1
subtype =2
offset =00009000
size =00006000
---
---
name =phy_init
type =1
subtype =1
offset =0000F000
size =00001000
---
---
name =factory
type =0
subtype =0
offset =00010000
size =00100000
---
```

### Point important — little-endian

Le magic `0xAA50` en flash est lu `0x50AA` par le CPU x86 (little-endian).  
Le define `PTABLE_MAGIC` a donc été ajusté à `0x50AA` pour que la comparaison fonctionne sur le host.  
Sur ESP32 (également little-endian), le comportement sera identique.

---

## 6. Makefile

```makefile
make all    # compile tous les binaires
make run    # compile → tests → flash sur /dev/ttyACM0
make clean  # supprime build/
```

Le port peut être surchargé :

```bash
make run PORT=/dev/ttyACM1
```

---

## 7. CI GitHub Actions

La CI se déclenche à chaque `push` et `pull_request` :

```
1. Checkout
2. Install gcc + esptool
3. make all
4. ./build/test_crc32
```

---

## 8. Découverte critique — MD5 et non CRC32

### La découverte

En cherchant à valider le checksum stocké à `0x70`, aucune variante de CRC32 ne correspondait.  
En lisant le source IDF `gen_esp32part.py` :

```python
MD5_PARTITION_BEGIN = b'\xEB\xEB' + b'\xFF' * 14

def to_binary(self):
    result = b''.join(e.to_binary() for e in self)
    if md5sum:
        result += MD5_PARTITION_BEGIN + hashlib.md5(result).digest()
```

**IDF utilise MD5, pas CRC32.**

### Format réel du bloc de fin

```
offset 0x60 : eb eb ff ff ff ff ff ff ff ff ff ff ff ff ff ff
              └──────────────────────────────────────────────┘
              MD5_PARTITION_BEGIN (16 bytes) — 2 bytes magic + 14 × 0xFF

offset 0x70 : f4 ad 4f 45 38 56 4b 5d 74 35 b6 2c 75 b6 95 24
              └──────────────────────────────────────────────┘
              MD5 digest des entrées (16 bytes)
```

### Validation

```bash
python3 -c "
import hashlib
with open('artifacts/ptable_ref.bin', 'rb') as f:
    data = f.read()
md5 = hashlib.md5(data[:96]).digest()
print('Match :', md5 == data[0x70:0x80])
"
# Match : True  ✅
```

### Impact sur la Phase 3

Le générateur `ptable_gen.c` devra produire :
1. Les entrées (N × 32 bytes)
2. `MD5_PARTITION_BEGIN` : `eb eb` + 14 × `ff`
3. MD5 digest des entrées (16 bytes)
4. Padding `0xFF` jusqu'à `0xC00`

---

## 9. Points critiques à retenir

| Point | Détail |
|---|---|
| `__attribute__((packed))` obligatoire | Sans lui `sizeof(PartEntry)` != 32 |
| Little-endian | `aa 50` en flash = `0x50AA` lu par x86 |
| `name[16]` null-padded | Pas de `\0` garanti si nom = 16 chars |
| IDF utilise MD5, pas CRC32 | Découvert dans `gen_esp32part.py` |
| `0xEBEB` = magic MD5 | Suivi de 14 × `0xFF` puis 16 bytes digest |
| MD5 calculé sur N × 32 bytes | 3 entrées × 32 = 96 bytes ici |
| CRC32 validé ✅ | Utile pour Phase 4 — image app/bootloader |
| Makefile + CI actifs | Depuis cette phase, pour tout le projet |

---

## Prochaine étape — Phase 3

Écrire `src/ptable_gen.c` qui produit un `ptable.bin` valide :
1. Écrire `src/md5.c/h` — MD5 from scratch
2. Écrire les entrées (packed structs)
3. Calculer et écrire le bloc MD5
4. Padder à `0xC00` avec `0xFF`
5. Valider avec `tools/verify.py` contre `artifacts/ptable_ref.bin`
