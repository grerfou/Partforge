# Phase 1 — Comprendre le boot ESP32-S3

> **Projet** : `partforge` — Générateur de partition table ESP32-S3 from scratch  
> **Objectif de la phase** : Comprendre exactement ce qui se passe entre le power-on et `main()`  
> **Statut** : ✅ Complété

---

## Table des matières

1. [Environnement de travail](#1-environnement-de-travail)
2. [La séquence de boot complète](#2-la-séquence-de-boot-complète)
3. [La SPI Flash — organisation mémoire](#3-la-spi-flash--organisation-mémoire)
4. [Lecture de la partition table depuis la flash](#4-lecture-de-la-partition-table-depuis-la-flash)
5. [Décodage byte par byte](#5-décodage-byte-par-byte)
6. [Layout flash constaté](#6-layout-flash-constaté)
7. [Points critiques à retenir](#7-points-critiques-à-retenir)

---

## 1. Environnement de travail

| Élément | Valeur |
|---|---|
| OS | Arch Linux |
| ESP-IDF | Installé et fonctionnel (`idf.py`) |
| esptool | v4.11.0 |
| Hardware | ESP32-S3 (QFN56) rev v0.1, PSRAM 8MB, Crystal 40MHz |
| Port série | `/dev/ttyACM0` (USB natif, pas de convertisseur UART) |
| Groupe utilisateur | `uucp` (Arch) |

> **Note** : L'ESP32-S3 utilise l'USB natif du chip (USB-Serial/JTAG intégré).  
> Il apparaît donc sur `ttyACM0` et non `ttyUSB0` comme les modules avec convertisseur CP210x/CH340.

### Vérification de la carte

```bash
esptool.py --port /dev/ttyACM0 chip_id
```

Sortie attendue :
```
Chip is ESP32-S3 (QFN56) (revision v0.1)
Features: WiFi, BLE, Embedded PSRAM 8MB (AP_1v8)
Crystal is 40MHz
USB mode: USB-Serial/JTAG
MAC: 68:b6:b3:3c:ef:64
```

---

## 2. La séquence de boot complète

```
POWER ON
    │
    ▼
ROM Bootloader  (adresse CPU : 0x40000000)
    │  ► Gravé en silicium par Espressif — non modifiable
    │  ► Initialise le CPU Xtensa LX7 (registres, caches)
    │  ► Lit les GPIO de strapping (pin BOOT)
    │       BOOT = 1 → mode normal (boot depuis SPI flash)
    │       BOOT = 0 → mode download (UART, pour flasher)
    │  ► Initialise le contrôleur SPI flash
    │  ► Lit 4 bytes à l'adresse flash 0x1000
    │  ► Vérifie le magic byte : 0xE9
    │  ► Charge le 2nd stage bootloader en IRAM
    │  ► Saute dessus
    │
    ▼
2nd Stage Bootloader  (flash : 0x1000 → chargé en IRAM)
    │  ► Programme C compilé par IDF, flashé par l'utilisateur
    │  ► Initialise l'horloge CPU (PLL, fréquence)
    │  ► Reconfigure la flash en mode rapide (QIO, fréquence SPI)
    │  ► Lit la partition table à l'adresse flash 0x8000
    │  ► Vérifie le CRC32 de la partition table
    │  ► Trouve l'entrée de type app/factory
    │  ► Charge l'application en mémoire (IRAM/DRAM)
    │  ► Vérifie optionnellement le hash SHA256 de l'app
    │  ► Saute vers l'entry point de l'application
    │
    ▼
call_start_cpu0  (entry point de ton application)
    │  ► Initialise la section .bss à zéro
    │  ► Copie la section .data depuis la flash vers la RAM
    │  ► Appelle main()
    │
    ▼
main()
```

### Sources IDF de référence

| Fichier | Rôle |
|---|---|
| `components/bootloader/subproject/main/bootloader_start.c` | Code du 2nd stage bootloader |
| `components/partition_table/` | Gestion de la partition table |

---

## 3. La SPI Flash — organisation mémoire

La flash SPI est un composant **externe** au chip, connecté via le bus SPI.  
Elle est accessible de deux façons :

| Mode | Mécanisme | Utilisé par |
|---|---|---|
| **Direct SPI** | Lecture byte par byte via le bus SPI | ROM bootloader au démarrage |
| **Memory-mapped** | Mappée dans l'espace d'adressage CPU | Application après init cache |

**Mapping mémoire (lecture) :**
```
Adresse CPU 0x3C000000 + offset  ←→  offset en flash
```

**Layout flash standard IDF :**

```
Adresse flash   Taille      Contenu
0x0000          (réservé)   Non utilisé
0x1000          variable    2nd stage bootloader  ← ROM lit ici (magic 0xE9)
0x8000          0xC00       Partition table       ← 2nd stage lit ici
0x9000          variable    nvs  (ou autre data)
...             ...         ...
0x10000         variable    Application factory   ← ton main()
```

---

## 4. Lecture de la partition table depuis la flash

### Commande

```bash
esptool.py --port /dev/ttyACM0 read_flash 0x8000 0xC00 ptable_ref.bin
```

| Paramètre | Valeur | Signification |
|---|---|---|
| `0x8000` | adresse de départ | Adresse fixe de la partition table |
| `0xC00` | taille | 3072 bytes — taille max d'une partition table IDF |
| `ptable_ref.bin` | fichier de sortie | Dump binaire brut |

### Inspection brute

```bash
xxd ptable_ref.bin | head -40
```

Sortie obtenue :
```
00000000: aa50 0102 0090 0000 0060 0000 6e76 7300  .P.......`..nvs.
00000010: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000020: aa50 0101 00f0 0000 0010 0000 7068 795f  .P..........phy_
00000030: 696e 6974 0000 0000 0000 0000 0000 0000  init............
00000040: aa50 0000 0000 0100 0000 1000 6661 6374  .P..........fact
00000050: 6f72 7900 0000 0000 0000 0000 0000 0000  ory.............
00000060: ebeb ffff ffff ffff ffff ffff ffff ffff  ................
00000070: f4ad 4f45 3856 4b5d 7435 b62c 75b6 9524  ..OE8VK]t5.,u..$
00000080: ffff ffff ffff ffff ffff ffff ffff ffff  ................
```

---

## 5. Décodage byte par byte

### Format d'une entrée (32 bytes, `__attribute__((packed))`)

```c
typedef struct {
    uint16_t magic;    // 0x00 — toujours 0xAA50
    uint8_t  type;     // 0x02 — 0x00=app, 0x01=data
    uint8_t  subtype;  // 0x03 — dépend du type
    uint32_t offset;   // 0x04 — adresse en flash (little-endian)
    uint32_t size;     // 0x08 — taille en bytes (little-endian)
    char     name[16]; // 0x0C — nom ASCII, null-padded
    uint32_t flags;    // 0x1C — flags (0 par défaut)
} PartEntry;           // total : 32 bytes
```

---

### Entrée 1 — `nvs` (offset 0x000000)

```
aa50 0102 0090 0000 0060 0000 6e76 7300 00...
```

| Offset | Bytes bruts | Valeur décodée | Signification |
|---|---|---|---|
| 0x00 | `aa 50` | 0xAA50 | ✅ Magic valide |
| 0x02 | `01` | 0x01 | Type = **data** |
| 0x03 | `02` | 0x02 | Subtype = **nvs** |
| 0x04 | `00 90 00 00` | 0x00009000 | Offset flash = **0x9000** |
| 0x08 | `00 60 00 00` | 0x00006000 | Taille = **0x6000** (24 Ko) |
| 0x0C | `6e 76 73 00...` | `"nvs\0..."` | Nom = **"nvs"** |
| 0x1C | `00 00 00 00` | 0x00000000 | Flags = 0 |

---

### Entrée 2 — `phy_init` (offset 0x000020)

```
aa50 0101 00f0 0000 0010 0000 7068 795f 696e6974 00...
```

| Offset | Bytes bruts | Valeur décodée | Signification |
|---|---|---|---|
| 0x00 | `aa 50` | 0xAA50 | ✅ Magic valide |
| 0x02 | `01` | 0x01 | Type = **data** |
| 0x03 | `01` | 0x01 | Subtype = **phy_init** |
| 0x04 | `00 f0 00 00` | 0x0000F000 | Offset flash = **0xF000** |
| 0x08 | `00 10 00 00` | 0x00001000 | Taille = **0x1000** (4 Ko) |
| 0x0C | `70 68 79 5f 69 6e 69 74...` | `"phy_init\0..."` | Nom = **"phy_init"** |
| 0x1C | `00 00 00 00` | 0x00000000 | Flags = 0 |

---

### Entrée 3 — `factory` (offset 0x000040)

```
aa50 0000 0000 0100 0000 1000 6661 6374 6f72 7900 00...
```

| Offset | Bytes bruts | Valeur décodée | Signification |
|---|---|---|---|
| 0x00 | `aa 50` | 0xAA50 | ✅ Magic valide |
| 0x02 | `00` | 0x00 | Type = **app** |
| 0x03 | `00` | 0x00 | Subtype = **factory** |
| 0x04 | `00 00 01 00` | 0x00010000 | Offset flash = **0x10000** |
| 0x08 | `00 00 10 00` | 0x00100000 | Taille = **0x100000** (1 Mo) |
| 0x0C | `66 61 63 74 6f 72 79...` | `"factory\0..."` | Nom = **"factory"** |
| 0x1C | `00 00 00 00` | 0x00000000 | Flags = 0 |

---

### Marqueur de fin (offset 0x000060)

```
ebeb ffff ffff ffff...
```

`0xEBEB` = marqueur de fin de table. Le 2nd stage bootloader arrête de parser les entrées ici.  
Tout ce qui suit est `0xFF` = flash vierge (état effacé).

---

### CRC32 (offset 0x000070)

```
f4ad 4f45 3856 4b5d 7435 b62c 75b6 9524
```

C'est le **checksum CRC32** calculé sur l'ensemble des entrées de la table.  
Le 2nd stage bootloader le vérifie au démarrage. S'il ne correspond pas → **boot refusé**.

> ⚠️ **Point critique Phase 3** : le CRC32 devra être calculé et écrit correctement  
> par le générateur `ptable_gen.c`. C'est la condition sine qua non pour booter.

---

## 6. Layout flash constaté

```
Adresse flash   Partition   Type         Taille
─────────────────────────────────────────────────
0x1000          bootloader  (2nd stage)  (non lu)
0x8000          ptable      (ce fichier) 0xC00
0x9000          nvs         data/nvs     0x6000   (24 Ko)
0xF000          phy_init    data/phy     0x1000   (4 Ko)
0x10000         factory     app/factory  0x100000 (1 Mo)
0x110000 →      (vierge)    0xFF...
```

---

## 7. Points critiques à retenir

| Point | Détail |
|---|---|
| Le ROM bootloader est immuable | Il lit toujours à `0x1000`. Câblé en silicium. |
| Le magic de chaque entrée est `0xAA50` | Toute entrée sans ce magic est ignorée. |
| Les valeurs sont en little-endian | `00 90 00 00` = `0x00009000`, pas `0x00009000` inversé. |
| Le nom est 16 bytes ASCII null-padded | Pas de terminaison garantie si le nom fait exactement 16 chars. |
| Le marqueur de fin est `0xEBEB` | Pas une entrée valide, juste un stop-marker. |
| Le CRC32 est obligatoire | Sans CRC32 valide, le 2nd stage refuse de booter. |
| Les offsets doivent être alignés sur `0x1000` | Contrainte du MMU et du cache flash. |
| `ttyACM0` et non `ttyUSB0` | Spécifique à l'USB natif de l'ESP32-S3. |

---

## Prochaine étape — Phase 2

Écrire le parser C (`src/ptable_parse.c`) qui :
1. Lit `ptable_ref.bin`
2. Valide le magic de chaque entrée
3. Affiche les champs décodés
4. Vérifie le CRC32

Ce parser sera la base de validation du générateur développé en Phase 3.
