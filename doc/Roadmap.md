# Roadmap — partforge
> Générateur de partition table ESP32-S3 from scratch

---

## Vue d'ensemble

```
PHASE 1 — Comprendre le boot ESP32-S3       ✅
        ↓
PHASE 2 — Reverse la partition table        🔲 (en cours)
        ↓
PHASE 3 — Générateur from scratch           🔲
        ↓
PHASE 4 — Booter ton code bare-metal        🔲
        ↓
PHASE 5 — Doc + Tests complets              🔲
```

---

## PHASE 1 — Comprendre le boot ESP32-S3 ✅

**Objectif** : savoir exactement ce qui se passe entre le power-on et `main()`

### Ce qu'on a compris

```
POWER ON
    ↓
ROM Bootloader (0x40000000, gravé en silicium)
    ↓ lit 0x1000 en flash → vérifie magic 0xE9
    ↓
2nd Stage Bootloader (0x1000 en flash)
    ↓ lit partition table à 0x8000
    ↓ vérifie MD5
    ↓ trouve entrée app/factory
    ↓
call_start_cpu0
    ↓
main()
```

### Fichiers produits
- `docs/phase1.md`

---

## PHASE 2 — Reverse la partition table IDF 🔲

**Objectif** : lire et comprendre chaque byte d'une vraie partition table

> ⚠️ Makefile et CI mis en place dès cette phase — actifs tout au long du projet.

### Étapes

| Étape | Description | Statut |
|---|---|---|
| 1 | Extraire la partition table de la flash | ✅ |
| 2 | Analyser byte par byte avec hexyl | ✅ |
| 3 | Écrire `ptable.h` — structures + constantes | ✅ |
| 4 | Écrire `crc32.c/h` — algo from scratch | ✅ |
| 5 | Makefile avec `make run` | ✅ |
| 6 | GitHub Actions CI | ✅ |
| 7 | Écrire `ptable_parse.c` — parser C | 🔲 |

### Découverte critique ⚠️

> IDF n'utilise **pas CRC32** pour la partition table — il utilise **MD5**.
> Découvert en lisant `gen_esp32part.py` lors de cette phase.
> Impact : voir Phase 3.

Format réel du bloc de fin (`0x60` en flash) :
```
eb eb ff ff ff ff ff ff ff ff ff ff ff ff ff ff  ← MD5_PARTITION_BEGIN (16 bytes)
f4 ad 4f 45 38 56 4b 5d 74 35 b6 2c 75 b6 95 24 ← MD5 digest (16 bytes)
```

### Fichiers produits
- `src/ptable.h`
- `src/crc32.h` / `src/crc32.c`
- `tests/test_crc32.c`
- `artifacts/ptable_ref.bin`
- `Makefile`
- `.github/workflows/ci.yml`
- `docs/phase2.md`

---

## PHASE 3 — Générateur from scratch 🔲

**Objectif** : produire un `ptable.bin` valide sans IDF

### Étapes

| Étape | Description | Statut |
|---|---|---|
| 1 | Écrire `md5.c/h` — MD5 from scratch | 🔲 |
| 2 | Écrire `ptable_gen.c` — générateur | 🔲 |
| 3 | Valider avec `tools/verify.py` vs référence IDF | 🔲 |

### Format à produire

```
N × 32 bytes          ← entrées (PartEntry packed)
16 bytes              ← MD5_PARTITION_BEGIN : eb eb + 14×ff
16 bytes              ← MD5 digest des entrées
padding 0xFF          ← jusqu'à 0xC00
```

### Fichiers à produire
- `src/md5.h` / `src/md5.c`
- `src/ptable_gen.c`
- `tools/verify.py`
- `docs/phase3.md`

---

## PHASE 4 — Booter ton code bare-metal 🔲

**Objectif** : ton code tourne sans aucune ligne d'IDF

### Étapes

| Étape | Description | Statut |
|---|---|---|
| 1 | Linker script minimal | 🔲 |
| 2 | Startup `call_start_cpu0` | 🔲 |
| 3 | Flash bootloader IDF + ta ptable + ton app | 🔲 |

### Fichiers à produire
- `src/startup.c`
- `src/link.ld`
- `docs/phase4.md`

---

## PHASE 5 — Doc + Tests complets 🔲

**Objectif** : projet prêt pour un recruteur ou une équipe

> ℹ️ Makefile et CI déjà actifs depuis la Phase 2.

### Étapes

| Étape | Description | Statut |
|---|---|---|
| 1 | Tests Unity complets | 🔲 |
| 2 | README.md final | 🔲 |
| 3 | Documentation Doxygen | 🔲 |

### Fichiers à produire
- `README.md`
- `docs/phase5.md`
