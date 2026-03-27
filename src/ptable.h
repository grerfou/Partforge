#pragma once 
#include <stdint.h>

/*
 * ┌────────┬─────────────────────────┬─────────────────────────┬────────┬────────┐
 * │00000000│ aa 50 01 02 00 90 00 00 ┊ 00 60 00 00 6e 76 73 00 │×P••⋄×⋄⋄┊⋄`⋄⋄nvs⋄│
 * │00000010│ 00 00 00 00 00 00 00 00 ┊ 00 00 00 00 00 00 00 00 │⋄⋄⋄⋄⋄⋄⋄⋄┊⋄⋄⋄⋄⋄⋄⋄⋄│
 * │00000020│ aa 50 01 01 00 f0 00 00 ┊ 00 10 00 00 70 68 79 5f │×P••⋄×⋄⋄┊⋄•⋄⋄phy_│
 * │00000030│ 69 6e 69 74 00 00 00 00 ┊ 00 00 00 00 00 00 00 00 │init⋄⋄⋄⋄┊⋄⋄⋄⋄⋄⋄⋄⋄│
 * │00000040│ aa 50 00 00 00 00 01 00 ┊ 00 00 10 00 66 61 63 74 │×P⋄⋄⋄⋄•⋄┊⋄⋄•⋄fact│
 * │00000050│ 6f 72 79 00 00 00 00 00 ┊ 00 00 00 00 00 00 00 00 │ory⋄⋄⋄⋄⋄┊⋄⋄⋄⋄⋄⋄⋄⋄│
 * │00000060│ eb eb ff ff ff ff ff ff ┊ ff ff ff ff ff ff ff ff │××××××××┊××××××××│
 * │00000070│ f4 ad 4f 45 38 56 4b 5d ┊ 74 35 b6 2c 75 b6 95 24 │××OE8VK]┊t5×,u××$│
 * │00000080│ ff ff ff ff ff ff ff ff ┊ ff ff ff ff ff ff ff ff │××××××××┊××××××××│
 * │*       │                         ┊                         │        ┊        │
 * │00000c00│                         ┊                         │        ┊        │
 * └────────┴─────────────────────────┴─────────────────────────┴────────┴────────┘
 * 
 * aa 50              → magic
 * 01                 → type
 * 02                 → subtype
 * 00 90 00 00        → offset
 * 00 60 00 00        → size
 * 6e 76 73 00 00 00 00 00 00 00 00 00 00 00 00 00  → name[16]
 * 00 00 00 00        → flags
 *
 */

#define PTABLE_MAGIC         0x50AA // Valeur des Entrées 
#define PTABLE_END_MARKER    0xEBEB // Fin des bloc d'entrée 
#define PTABLE_ENTRY_SIZE    32     // Taille total en bytes entre les entrées
#define PTABLE_MAX_SIZE      0xC00  // 0xC00 Valeur total de Table

#define PTABLE_TYPE_APP      0x00
#define PTABLE_TYPE_DATA     0x01


#define PART_SUBTYPE_FACTORY 0x00 
#define PART_SUBTYPE_NVS     0x02 
#define PART_SUBTYPE_PHY     0x01 


typedef struct {
    uint16_t magic; // 2 bytes 
    uint8_t type; // 1 byte
    uint8_t subtype; // 1 byte
    uint32_t offset; // 4 bytes
    uint32_t size; // 4 bytes 
    char name[16]; // 1 byte x 16
    uint32_t flags; // 4 bytes
} __attribute__((packed)) PartEntry;
