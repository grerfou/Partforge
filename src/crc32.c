#include "crc32.h"
#include <stddef.h>

static uint32_t crc32_table[256]; // Tableau de 256 entré de 32 bits
static int table_ready = 0; 


static void crc32_build_table(void){

    for (uint32_t i = 0; i < 256; i++){

        uint32_t crc = i;

        for (int bit = 0; bit < 8; bit++){

            if (crc & 1){
                crc = crc >> 1;
                crc = crc ^ 0xEDB88320;
            } else {
                crc = crc >> 1;
            }
        }

       crc32_table[i] = crc;
    }
}

uint32_t crc32_compute(const void *data, size_t len){
    
    if (!table_ready){
        crc32_build_table();
        table_ready = 1;
    }

    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;

    while (len--){
        crc = (crc >> 8) ^ crc32_table[(crc ^ *p++) & 0xFF];

    }

    return crc ^ 0xFFFFFFFF;

}
