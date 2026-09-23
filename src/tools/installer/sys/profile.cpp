// tools/installer/sys/profile.cpp - see profile.h.
#include "sys/profile.h"
#include "sys/fs.h"
#include <vector>

namespace dvr::setup::profile {

std::wstring get(const std::wstring& ini, const wchar_t* section, const wchar_t* key, const wchar_t* def)
{
    wchar_t buf[2048];
    const DWORD n = GetPrivateProfileStringW(section, key, def, buf, 2048, ini.c_str());
    return std::wstring(buf, n);
}
int get_int(const std::wstring& ini, const wchar_t* section, const wchar_t* key, int def)
{
    return (int)GetPrivateProfileIntW(section, key, def, ini.c_str());
}

bool set(const std::wstring& ini, const wchar_t* section, const wchar_t* key, const std::wstring& value, DWORD* err)
{
    if (WritePrivateProfileStringW(section, key, value.c_str(), ini.c_str())) return true;
    if (err) *err = GetLastError();
    return false;
}

bool ensure_trailing_newline(const std::wstring& ini, DWORD* err)
{
    std::vector<uint8_t> bytes;
    if (!fs::read_file(ini, &bytes, err)) return false;
    if (bytes.empty() || bytes.back() == '\n') return true;
    bytes.push_back('\r'); bytes.push_back('\n');
    return fs::write_file_atomic(ini, bytes.data(), bytes.size(), err);
}

bool inspect(const std::wstring& ini, size_t* crlf, size_t* lf, bool* utf8Bom)
{
    std::vector<uint8_t> b;
    if (!fs::read_file(ini, &b, nullptr)) return false;
    size_t c = 0, l = 0;
    for (size_t i = 0; i < b.size(); ++i)
        if (b[i] == '\n') { ++l; if (i > 0 && b[i - 1] == '\r') ++c; }
    if (crlf) *crlf = c;
    if (lf) *lf = l;
    if (utf8Bom) *utf8Bom = b.size() >= 3 && b[0] == 0xEF && b[1] == 0xBB && b[2] == 0xBF;
    return true;
}

} // namespace dvr::setup::profile
