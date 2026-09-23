// tools/installer/sys/resources.cpp - see resources.h.
#include "sys/resources.h"
#include <windows.h>

namespace dvr::setup::resources {

Blob rcdata(int id)
{
    Blob b;
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id), (LPCWSTR)RT_RCDATA);
    if (!r) return b;
    HGLOBAL g = LoadResource(nullptr, r);
    if (!g) return b;
    b.data = (const uint8_t*)LockResource(g);
    b.size = SizeofResource(nullptr, r);
    return b;
}

} // namespace dvr::setup::resources
