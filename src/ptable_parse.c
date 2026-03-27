#include <stdio.h>
#include "ptable.h"

int main(int argc, char *argv[])
{
    if (argc < 2){
        printf("Usage: %s <ptable.bin>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (f == NULL){
        printf("Erreur ouverture fichier\n");
        return 1;
    }
    
    PartEntry entry;

    while (fread(&entry, sizeof(PartEntry), 1, f) == 1){
        if (entry.magic == PTABLE_END_MARKER){
            break;
        }
        if (entry.magic != PTABLE_MAGIC){
            continue;
        }
        
        printf("---\n");
        printf("name =%s\n", entry.name);
        printf("type =%u\n", entry.type);
        printf("subtype =%u\n", entry.subtype);
        printf("offset =%08X\n", entry.offset);
        printf("size =%08X\n", entry.size);
        printf("---\n");

    }
    fclose(f);

    return 0;
}
