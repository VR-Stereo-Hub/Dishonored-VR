#include "core/util/ini.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace dvr::ini {

float read_float(const char* ini, const char* section, const char* key, float def)
{
    char buf[64], defs[64];
    snprintf(defs, sizeof(defs), "%.4f", def);
    GetPrivateProfileStringA(section, key, defs, buf, sizeof(buf), ini);
    return (float)atof(buf);
}

int read_int(const char* ini, const char* section, const char* key, int def)
{
    return (int)GetPrivateProfileIntA(section, key, def, ini);
}

bool write_text_file(const char* path, const char* text)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fwrite(text, 1, strlen(text), f);
    fclose(f);
    return true;
}

} // namespace dvr::ini
