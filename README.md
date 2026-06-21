# Partforge

> ESP32-S3 partition table generator — from scratch, without IDF.

## What is this?

Partforge is a low-level C project that reverse engineers the ESP32-S3 
partition table binary format and generates a valid `ptable.bin` 
without any dependency on ESP-IDF.

Built as a deep-dive into ESP32 boot internals: ROM bootloader, 
2nd stage bootloader, flash memory layout, and the IDF partition 
table format.

## Status

🔧 Work in progress

| Phase | Description | Status |
|-------|-------------|--------|
| 1 | Understand ESP32-S3 boot sequence | ✅ Done |
| 2 | Reverse engineer partition table binary format | ✅ Done |
| 3 | Generate valid ptable.bin from scratch | ✅ Done |
| 4 | Boot bare-metal code without IDF | 🔲 In progress |
| 5 | Full documentation and tests | 🔲 Planned |

## Key findings

- IDF uses **MD5**, not CRC32, to checksum the partition table
- Discovered by reading `gen_esp32part.py` from Espressif source
- Magic byte `0xAA50` requires careful little-endian handling on x86
- Generated `ptable.bin` validated byte-for-byte against a real 
  flash dump

## What's implemented

- `crc32.c` — CRC32 ISO 3309 from scratch, lookup table, 
  validated against universal test vector
- `md5.c` — MD5 RFC 1321 from scratch, 7/7 test vectors passing
- `ptable_parse.c` — binary parser for real flash dumps
- `ptable_gen.c` — generates a valid `ptable.bin` identical 
  byte-for-byte to IDF output
- `tools/verify.py` — validates generated binary against 
  reference flash dump

## Build

```bash
make all      # compile everything
make test     # run CRC32 + MD5 tests
make verify   # generate + validate against reference
make clean
```

## CI

GitHub Actions — builds and runs tests on every push.

## Stack

- Language : C11
- Tools : GCC · CMake · Make · GitHub Actions · esptool · Python
- Target : ESP32-S3 (Xtensa LX7, ESP-IDF v5.2.3)
