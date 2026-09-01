#include "core/hooks/iat.h"
#include <stdint.h>
#include <string.h>

namespace dvr::hooks {

// 32.76: the module matters. A window shrink arrived while the game was inside
// Reset, before any call from the exe reached our veto - it came from the DXVK
// fork, which has its own import table, so the exe-only version of this walk
// never saw it.
void** find_iat_slot_in(HMODULE mod, const char* dllName, const char* funcName)
{
    uint8_t* base = (uint8_t*)mod;
    if (!base) return nullptr;
    IMAGE_DOS_HEADER* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    IMAGE_NT_HEADERS32* nt = (IMAGE_NT_HEADERS32*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return nullptr;
    IMAGE_DATA_DIRECTORY* dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir->VirtualAddress) return nullptr;
    IMAGE_IMPORT_DESCRIPTOR* imp = (IMAGE_IMPORT_DESCRIPTOR*)(base + dir->VirtualAddress);
    for (; imp->Name; imp++) {
        const char* nm = (const char*)(base + imp->Name);
        if (_stricmp(nm, dllName) != 0) continue;
        IMAGE_THUNK_DATA32* orig = (IMAGE_THUNK_DATA32*)(base + imp->OriginalFirstThunk);
        IMAGE_THUNK_DATA32* iat = (IMAGE_THUNK_DATA32*)(base + imp->FirstThunk);
        for (; orig->u1.AddressOfData; orig++, iat++) {
            if (orig->u1.Ordinal & IMAGE_ORDINAL_FLAG32) continue;
            IMAGE_IMPORT_BY_NAME* ibn = (IMAGE_IMPORT_BY_NAME*)(base + orig->u1.AddressOfData);
            if (!strcmp((const char*)ibn->Name, funcName))
                return (void**)&iat->u1.Function;
        }
    }
    return nullptr;
}

void** find_iat_slot(const char* dllName, const char* funcName)
{
    return find_iat_slot_in(GetModuleHandleA(nullptr), dllName, funcName);
}

void* patch_iat_slot(void** slot, void* hook)
{
    if (!slot) return nullptr;
    DWORD op = 0;
    if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &op)) return nullptr;
    void* old = *slot;
    *slot = hook;
    VirtualProtect(slot, sizeof(void*), op, &op);
    return old;
}

} // namespace dvr::hooks
