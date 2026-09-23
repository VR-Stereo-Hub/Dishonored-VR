// tools/installer/sys/profile.h - dishonored_vr.ini through the private-profile
// API, the way the mod itself and tools/vr-runtime.ps1 write it: one key edited
// in place, comments and the file's CRLF untouched. Two traps the wrappers
// close: a nullptr value DELETES the key (and a missing [Paths] DataDir falls
// back to the dev PC's D:\dvr-data in config.cpp), and a relative path resolves
// to the Windows folder.
#pragma once
#include <windows.h>
#include <string>

namespace dvr::setup::profile {

std::wstring get(const std::wstring& ini, const wchar_t* section, const wchar_t* key, const wchar_t* def = L"");
int  get_int(const std::wstring& ini, const wchar_t* section, const wchar_t* key, int def);
// value may be empty (writes "Key="); it is never passed as nullptr.
bool set(const std::wstring& ini, const wchar_t* section, const wchar_t* key, const std::wstring& value, DWORD* err);
// The API re-serialises from the last line; a file without a final newline
// would have its last key glued to the new one. Appends CRLF when needed.
bool ensure_trailing_newline(const std::wstring& ini, DWORD* err);
// After a write: are CRLF and LF counts equal (no mixed endings)? Also reports
// a UTF-8 BOM, which the API does not understand.
bool inspect(const std::wstring& ini, size_t* crlf, size_t* lf, bool* utf8Bom);

} // namespace dvr::setup::profile
