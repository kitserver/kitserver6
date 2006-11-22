#include <stdio.h>
#include "keycfg.h"

void ReadKeyCfg(KEYCFG* cfg, const char* filename)
{
    FILE* f = fopen(filename, "rb");
    if (f) {
        fread(cfg, sizeof(KEYCFG), 1, f);
        fclose(f);
    }
}

void WriteKeyCfg(KEYCFG* cfg, const char* filename)
{
    FILE* f = fopen(filename, "wb");
    if (f) {
        fwrite(cfg, sizeof(KEYCFG), 1, f);
        fclose(f);
    }
}

