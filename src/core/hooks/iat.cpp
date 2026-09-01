// core/hooks/iat.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// Generic IAT slot finder: walk the exe's import descriptors for dllName and
// return the address of the IAT entry for funcName. (The pad hook predates
// this and uses hardcoded slots; new hooks should use this instead.)
// 32.76: the module matters now. The 1071 arrives while the game is inside
// Reset, BEFORE any call from the exe reaches our veto - so the shrink is not
// coming from Dishonored.exe at all. The only other code running there is our
// own DXVK fork, and it has its own import table, which the exe-only version
// of this function never touched.
static void** FindIatSlotIn(HMODULE mod, const char* dllName, const char* funcName)
{
    uint8_t* base = (uint8_t*)mod;
    if (!base) return NULL;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    IMAGE_NT_HEADERS32* nt = (IMAGE_NT_HEADERS32*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;
    IMAGE_DATA_DIRECTORY* dir =
        &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir->VirtualAddress) return NULL;
    IMAGE_IMPORT_DESCRIPTOR* imp =
        (IMAGE_IMPORT_DESCRIPTOR*)(base + dir->VirtualAddress);
    for (; imp->Name; imp++) {
        const char* nm = (const char*)(base + imp->Name);
        if (_stricmp(nm, dllName) != 0) continue;
        IMAGE_THUNK_DATA32* orig =
            (IMAGE_THUNK_DATA32*)(base + imp->OriginalFirstThunk);
        IMAGE_THUNK_DATA32* iat =
            (IMAGE_THUNK_DATA32*)(base + imp->FirstThunk);
        for (; orig->u1.AddressOfData; orig++, iat++) {
            if (orig->u1.Ordinal & IMAGE_ORDINAL_FLAG32) continue;
            IMAGE_IMPORT_BY_NAME* ibn =
                (IMAGE_IMPORT_BY_NAME*)(base + orig->u1.AddressOfData);
            if (!strcmp((const char*)ibn->Name, funcName))
                return (void**)&iat->u1.Function;
        }
    }
    return NULL;
}


static void** FindIatSlot(const char* dllName, const char* funcName)
{
    return FindIatSlotIn(GetModuleHandleA(NULL), dllName, funcName);
}
