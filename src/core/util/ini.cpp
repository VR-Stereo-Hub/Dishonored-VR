// core/util/ini.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static float IniFloat(const char* ini, const char* sec, const char* key, float def)
{
    char buf[64], defs[64];
    _snprintf(defs, sizeof(defs), "%.4f", def);
    GetPrivateProfileStringA(sec, key, defs, buf, sizeof(buf), ini);
    return (float)atof(buf);
}


static bool WriteTextFile(const char* path, const char* text)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fwrite(text, 1, strlen(text), f);
    fclose(f);
    return true;
}
