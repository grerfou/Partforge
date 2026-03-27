#include <stdio.h>
#include <string.h>
#include "crc32.h"

static void testCrc(){
    const char *data = "123456789";
    uint32_t result = crc32_compute(data, strlen(data));
    printf("CRC32 = 0x%08X\n", result);
    if (result == 0xCBF43926)
        printf("OK\n");
    else
        printf("ERREUR — attendu 0xCBF43926\n");

}

static void validation(){

    FILE *f = fopen("artifacts/ptable_ref.bin", "rb");
    if (f == NULL) {
        printf("Erreur ouverture fichier\n");
    }

    uint8_t buf[96];
    fread(buf, 1, 96, f);
    fclose(f);

    uint32_t result = crc32_compute(buf, 96);

    printf("CRC32 ptable = 0x%08X\n", result);
    if (result == 0x454FADF4)
        printf("OK\n");
    else
        printf("ERREUR — attendu 0x454FADF4\n");
}

int main(void){

    testCrc();
    validation();

}

