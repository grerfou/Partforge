#!/usr/bin/env python3
"""
verify.py — Validation de ptable_gen.bin contre ptable_ref.bin
partforge — Générateur de partition table ESP32-S3 from scratch

Usage :
    python3 tools/verify.py
    python3 tools/verify.py <gen.bin> <ref.bin>
"""

import sys
import hashlib
import struct

# ------------------------------------------------------------------ #
#  Chemins par défaut                                                  #
# ------------------------------------------------------------------ #
GEN_BIN = "artifacts/ptable_gen.bin"
REF_BIN = "artifacts/ptable_ref.bin"

PTABLE_MAX_SIZE   = 0xC00
PTABLE_ENTRY_SIZE = 32

#
# ⚠️  Little-endian :
#     En flash        → 0xAA 0x50 (magic écrit tel quel)
#     Lu par x86 LE   → 0x50AA
#     struct.unpack LE → lit 0x50AA
#
PTABLE_MAGIC_LE   = 0x50AA  # valeur lue par struct.unpack("<H", ...)
PTABLE_END_LE     = 0xEBEB  # identique dans les deux sens  # idem

MD5_BEGIN_MAGIC   = b'\xeb\xeb'

# ------------------------------------------------------------------ #
#  Utilitaires                                                         #
# ------------------------------------------------------------------ #
OK   = "\033[92mOK\033[0m"
FAIL = "\033[91mERREUR\033[0m"

def check(label, result):
    status = OK if result else FAIL
    print(f"  {label:<50} {status}")
    return result

# ------------------------------------------------------------------ #
#  Lecture                                                             #
# ------------------------------------------------------------------ #
def read_file(path):
    try:
        with open(path, "rb") as f:
            return f.read()
    except FileNotFoundError:
        print(f"FATAL : fichier non trouvé → {path}")
        sys.exit(1)

# ------------------------------------------------------------------ #
#  Parsing                                                             #
# ------------------------------------------------------------------ #
def parse_entries(data):
    """
    Lit les entrées jusqu'au marqueur de fin 0xEBEB.
    struct.unpack("<H") lit en little-endian → compare avec PTABLE_MAGIC_LE.
    """
    entries = []
    offset = 0
    while offset + PTABLE_ENTRY_SIZE <= len(data):
        magic = struct.unpack_from("<H", data, offset)[0]
        if magic == PTABLE_END_LE:
            break
        if magic == PTABLE_MAGIC_LE:
            entry = {
                "magic":   magic,
                "type":    data[offset + 2],
                "subtype": data[offset + 3],
                "offset":  struct.unpack_from("<I", data, offset + 4)[0],
                "size":    struct.unpack_from("<I", data, offset + 8)[0],
                "name":    data[offset + 12:offset + 28].rstrip(b"\x00").decode("ascii"),
                "flags":   struct.unpack_from("<I", data, offset + 28)[0],
            }
            entries.append(entry)
        offset += PTABLE_ENTRY_SIZE
    return entries, offset

def extract_md5(data, entries_end):
    begin  = data[entries_end      : entries_end + 16]
    digest = data[entries_end + 16 : entries_end + 32]
    return begin, digest

# ------------------------------------------------------------------ #
#  Vérifications                                                       #
# ------------------------------------------------------------------ #
def verify_file(label, data):
    print(f"\n── {label} ({len(data)} bytes) ──")
    passed = 0
    total  = 0

    total += 1
    passed += check(f"Taille == 0x{PTABLE_MAX_SIZE:03X}", len(data) == PTABLE_MAX_SIZE)

    entries, entries_end = parse_entries(data)
    total += 1
    passed += check(f"Entrées trouvées : {len(entries)}", len(entries) > 0)

    for e in entries:
        print(f"     [{e['name']:12}] type=0x{e['type']:02X} "
              f"sub=0x{e['subtype']:02X} "
              f"offset=0x{e['offset']:08X} "
              f"size=0x{e['size']:08X}")

    md5_begin, md5_stored = extract_md5(data, entries_end)
    total += 1
    passed += check("MD5_PARTITION_BEGIN = eb eb + 14×ff",
                    md5_begin[:2] == MD5_BEGIN_MAGIC and
                    md5_begin[2:] == b'\xff' * 14)

    md5_computed = hashlib.md5(data[:entries_end]).digest()
    total += 1
    passed += check(f"MD5 correct : {md5_computed.hex()}",
                    md5_computed == md5_stored)

    if md5_computed != md5_stored:
        print(f"     Stocké   : {md5_stored.hex()}")
        print(f"     Calculé  : {md5_computed.hex()}")

    return passed, total

def verify_identical(gen, ref):
    print("\n── Comparaison gen vs ref ──")
    passed = 0
    total  = 0

    gen_entries, gen_end = parse_entries(gen)
    ref_entries, ref_end = parse_entries(ref)

    total += 1
    passed += check("Même nombre d'entrées",
                    len(gen_entries) == len(ref_entries))

    total += 1
    passed += check("Entrées identiques byte par byte",
                    gen[:gen_end] == ref[:ref_end])

    total += 1
    identical = (gen == ref)
    passed += check("Fichiers identiques byte par byte", identical)

    if not identical:
        for i in range(min(len(gen), len(ref))):
            if gen[i] != ref[i]:
                print(f"     Première diff offset 0x{i:03X} : "
                      f"gen=0x{gen[i]:02X} ref=0x{ref[i]:02X}")
                break

    return passed, total

# ------------------------------------------------------------------ #
#  main                                                                #
# ------------------------------------------------------------------ #
def main():
    gen_path = sys.argv[1] if len(sys.argv) >= 2 else GEN_BIN
    ref_path = sys.argv[2] if len(sys.argv) >= 3 else REF_BIN

    print("══════════════════════════════════════════════════")
    print("  verify.py — partforge partition table validator")
    print("══════════════════════════════════════════════════")
    print(f"  GEN : {gen_path}")
    print(f"  REF : {ref_path}")

    gen = read_file(gen_path)
    ref = read_file(ref_path)

    total_passed = 0
    total_tests  = 0

    p, t = verify_file("ptable_gen.bin", gen)
    total_passed += p; total_tests += t

    p, t = verify_file("ptable_ref.bin", ref)
    total_passed += p; total_tests += t

    p, t = verify_identical(gen, ref)
    total_passed += p; total_tests += t

    print(f"\n══════════════════════════════════════════════════")
    print(f"  {total_passed}/{total_tests} vérifications passées")
    print(f"══════════════════════════════════════════════════")

    sys.exit(0 if total_passed == total_tests else 1)

if __name__ == "__main__":
    main()
