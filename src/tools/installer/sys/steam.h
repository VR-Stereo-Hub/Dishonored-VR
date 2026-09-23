// tools/installer/sys/steam.h - where Steam put the game (tools/lib/game-path.ps1,
// in C++): HKCU\Software\Valve\Steam SteamPath, then every "path" in
// steamapps\libraryfolders.vdf, then the library that holds
// appmanifest_205100.acf and steamapps\common\Dishonored\Binaries\Win32\
// Dishonored.exe. HKCU\Software\Valve is one shared view for 32- and 64-bit
// processes (only HKCU\Software\Classes is redirected), so no WOW64 flag.
#pragma once
#include <string>
#include <vector>

namespace dvr::setup::steam {

constexpr const wchar_t* kAppId = L"205100";

std::wstring steam_path();                      // "" when Steam is not installed
std::vector<std::wstring> libraries();          // every library root, Steam's own first
// The folder holding Dishonored.exe, or "" with `why` explaining the miss.
std::wstring find_game_dir(std::wstring* why);
// Accepts the Win32 folder, the Binaries folder or the game root; returns the
// Win32 folder when Dishonored.exe is found under it, else "".
std::wstring normalise_game_dir(const std::wstring& chosen);
bool steamvr_installed();                       // steamapps\common\SteamVR in any library

} // namespace dvr::setup::steam
