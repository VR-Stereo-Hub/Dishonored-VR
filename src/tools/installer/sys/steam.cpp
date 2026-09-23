// tools/installer/sys/steam.cpp - see steam.h.
#include "sys/steam.h"
#include "sys/fs.h"
#include <windows.h>

namespace dvr::setup::steam {

namespace {
std::wstring normalise_slashes(std::wstring p)
{
    for (auto& c : p) if (c == L'/') c = L'\\';
    return fs::strip_trailing_slashes(p);
}
// libraryfolders.vdf lines look like:   "path"   "D:\\SteamLibrary"
// No regex: find a line whose first token is "path", take the next quoted
// string, and unescape the doubled backslashes.
bool vdf_path_line(const std::string& line, std::wstring* out)
{
    size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (line.compare(i, 6, "\"path\"") != 0) return false;
    i += 6;
    while (i < line.size() && line[i] != '"') ++i;
    if (i >= line.size()) return false;
    ++i;
    std::string v;
    while (i < line.size() && line[i] != '"') {
        if (line[i] == '\\' && i + 1 < line.size()) { v.push_back(line[i + 1]); i += 2; }
        else v.push_back(line[i++]);
    }
    *out = normalise_slashes(fs::widen(v));
    return !out->empty();
}
}

std::wstring steam_path()
{
    wchar_t buf[1024];
    DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS)
        return normalise_slashes(buf);
    const std::wstring fallback = L"C:\\Program Files (x86)\\Steam";
    return fs::is_dir(fallback) ? fallback : L"";
}

std::vector<std::wstring> libraries()
{
    std::vector<std::wstring> libs;
    const std::wstring steam = steam_path();
    if (steam.empty()) return libs;
    libs.push_back(steam);
    std::vector<uint8_t> bytes;
    if (fs::read_file(fs::join(steam, L"steamapps\\libraryfolders.vdf"), &bytes, nullptr)) {
        std::string text((const char*)bytes.data(), bytes.size());
        size_t start = 0;
        while (start <= text.size()) {
            size_t nl = text.find('\n', start);
            if (nl == std::string::npos) nl = text.size();
            std::wstring p;
            if (vdf_path_line(text.substr(start, nl - start), &p)) {
                bool dup = false;
                for (const auto& l : libs) if (fs::iequals(l, p)) dup = true;
                if (!dup) libs.push_back(p);
            }
            start = nl + 1;
        }
    }
    return libs;
}

std::wstring find_game_dir(std::wstring* why)
{
    const std::vector<std::wstring> libs = libraries();
    if (libs.empty()) { if (why) *why = L"Steam is not installed (no SteamPath in the registry)."; return L""; }
    for (const auto& lib : libs) {
        if (!fs::is_file(fs::join(lib, L"steamapps\\appmanifest_" + std::wstring(kAppId) + L".acf"))) continue;
        const std::wstring dir = fs::join(lib, L"steamapps\\common\\Dishonored\\Binaries\\Win32");
        if (fs::is_file(fs::join(dir, L"Dishonored.exe"))) return dir;
    }
    if (why) *why = fs::wformat(L"Dishonored (Steam app %s) is not in any of the %u Steam libraries.", kAppId, (unsigned)libs.size());
    return L"";
}

std::wstring normalise_game_dir(const std::wstring& chosen)
{
    const std::wstring c = fs::strip_trailing_slashes(chosen);
    if (c.empty()) return L"";
    const wchar_t* tails[] = { L"", L"Binaries\\Win32", L"Win32" };
    for (const wchar_t* t : tails) {
        const std::wstring dir = *t ? fs::join(c, t) : c;
        if (fs::is_file(fs::join(dir, L"Dishonored.exe"))) return dir;
    }
    return L"";
}

bool steamvr_installed()
{
    for (const auto& lib : libraries())
        if (fs::is_dir(fs::join(lib, L"steamapps\\common\\SteamVR"))) return true;
    return false;
}

} // namespace dvr::setup::steam
