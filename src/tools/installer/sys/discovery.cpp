#include "sys/discovery.h"
#include "sys/fs.h"
#include "sys/steam.h"
#include "sys/process.h"
#include <json/json.h>
#include <filesystem>
#include <memory>
#include <algorithm>
namespace dvr::setup::discovery {
namespace {
std::wstring read_reg(HKEY hive,const std::wstring& key,const wchar_t* name,REGSAM view) {
    HKEY h=nullptr; if(RegOpenKeyExW(hive,key.c_str(),0,KEY_READ|view,&h)!=ERROR_SUCCESS) return {};
    wchar_t value[32768]; DWORD bytes=sizeof(value);
    LONG rc=RegGetValueW(h,nullptr,name,RRF_RT_REG_SZ,nullptr,value,&bytes); RegCloseKey(h);
    return rc==ERROR_SUCCESS?value:L"";
}
std::vector<std::wstring> subkeys(HKEY hive,const wchar_t* path,REGSAM view) {
    std::vector<std::wstring> out; HKEY h=nullptr;
    if(RegOpenKeyExW(hive,path,0,KEY_READ|view,&h)!=ERROR_SUCCESS) return out;
    for(DWORD i=0;i<4096;++i) { wchar_t name[1024]; DWORD n=1024;
        if(RegEnumKeyExW(h,i,name,&n,nullptr,nullptr,nullptr,nullptr)!=ERROR_SUCCESS) break;
        out.emplace_back(name,n);
    }
    RegCloseKey(h);return out;
}
bool digits(const std::wstring& s) { return !s.empty() && s.size()<32 && s.find_first_not_of(L"0123456789")==std::wstring::npos; }
void gog_info(Game& game) {
    std::wstring root=game.dir;
    for(int depth=0;depth<3 && !root.empty();++depth,root=fs::parent(root)) {
        WIN32_FIND_DATAW fd; HANDLE h=FindFirstFileW(fs::join(root,L"goggame-*.info").c_str(),&fd);
        if(h==INVALID_HANDLE_VALUE) continue;
        do {
            const auto path=fs::join(root,fd.cFileName); std::vector<uint8_t> bytes;
            if(fs::file_size(path)>1024*1024 || !fs::read_file(path,&bytes,nullptr)) continue;
            Json::CharReaderBuilder builder; Json::Value j; std::string why;
            std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
            if(!reader->parse((char*)bytes.data(),(char*)bytes.data()+bytes.size(),&j,&why) || !j.isObject()) continue;
            auto id=j["gameId"].isString()?fs::widen(j["gameId"].asString()):L"";
            if(!digits(id)) continue;
            // Ignore DLC metadata: its rootGameId identifies the installed base game.
            if(j["rootGameId"].isString() && digits(fs::widen(j["rootGameId"].asString()))) id=fs::widen(j["rootGameId"].asString());
            game.store=Store::Gog; game.gogId=id; game.root=root;
            FindClose(h); return;
        } while(FindNextFileW(h,&fd));
        FindClose(h);
    }
}
}
Game inspect(const std::wstring& chosen) {
    Game g; auto c=fs::strip_trailing_slashes(chosen); for(auto& ch:c) if(ch==L'/') ch=L'\\';
    if(fs::iequals(fs::filename(c),L"Dishonored.exe")) c=fs::parent(c);
    if(c.empty()) {g.note="Select your Steam or GOG game folder.";return g;}
    if(fs::iequals(fs::filename(c),L"Win64") && fs::is_dir(c)) {
        g.dir=c;g.unsupported64=true;
        g.note="INCOMPATIBLE 64-BIT GAME: this VR mod will not work with Win64. Select the original 32-bit Dishonored in Binaries\\Win32.";return g;
    }
    std::vector<std::wstring> tries={c,fs::join(c,L"Binaries\\Win32"),fs::join(c,L"Win32"),fs::join(c,L"Binaries\\Win64"),fs::join(c,L"Win64")};
    for(const auto& dir:tries) {
        const auto exe=fs::join(dir,L"Dishonored.exe");
        if(!fs::is_file(exe)) continue;
        g.dir=dir; g.root=fs::iequals(fs::filename(fs::parent(dir)),L"Binaries")?fs::parent(fs::parent(dir)):dir;
        DWORD binary=0;
        if(fs::iequals(fs::filename(dir),L"Win64") || (GetBinaryTypeW(exe.c_str(),&binary) && binary==SCS_64BIT_BINARY)) {
            g.unsupported64=true;g.note="INCOMPATIBLE 64-BIT GAME: this VR mod will not work with Win64. Select the original 32-bit Dishonored in Binaries\\Win32.";return g;
        }
        if(!GetBinaryTypeW(exe.c_str(),&binary) || binary!=SCS_32BIT_BINARY) { g.note="Dishonored.exe is not a valid 32-bit Windows executable. Select Binaries\\Win32.";return g; }
        g.valid=true;g.note="32-bit Dishonored found";
        std::wstring lower=dir; std::transform(lower.begin(),lower.end(),lower.begin(),towlower);
        if(lower.find(L"steamapps\\common\\")!=std::wstring::npos || fs::is_file(fs::join(dir,L"steam_api.dll"))) g.store=Store::Steam;
        gog_info(g);
        if(g.store==Store::Gog) g.note="GOG installation (32-bit)";
        else if(g.store==Store::Steam) g.note="Steam installation (32-bit)";
        else g.note="32-bit installation found; store not identified. Launch it from your store library.";
        return g;
    }
    g.note="Dishonored was not found. Choose its installation folder, Binaries\\Win32, or another detected library."; return g;
}
std::wstring galaxy_path() {
    for(HKEY hive:{HKEY_LOCAL_MACHINE,HKEY_CURRENT_USER}) for(REGSAM view:{KEY_WOW64_32KEY,KEY_WOW64_64KEY}) {
        auto p=read_reg(hive,L"SOFTWARE\\GOG.com\\GalaxyClient\\paths",L"client",view);
        if(fs::is_dir(p)) p=fs::join(p,L"GalaxyClient.exe"); if(fs::is_file(p)) return p;
    }
    for(const wchar_t* variable:{L"ProgramFiles(x86)",L"ProgramW6432",L"ProgramFiles"}) {
        auto p=fs::join(fs::env(variable),L"GOG Galaxy\\GalaxyClient.exe"); if(fs::is_file(p)) return p;
    }
    return {};
}
std::vector<Game> find_games() {
    std::vector<Game> out;
    auto add=[&](const std::wstring& root,Store hint=Store::Unknown,const std::wstring& id=L"") {
        if(root.empty())return;
        Game g=inspect(root); if(!g.valid && !g.unsupported64)return;
        for(const auto& old:out) if(fs::iequals(old.dir,g.dir)) return;
        if(g.store==Store::Unknown)g.store=hint;
        if(g.store==Store::Gog && g.gogId.empty() && digits(id))g.gogId=id;
        out.push_back(g);
    };
    for(const auto& lib:steam::libraries()) {
        // Honour renamed/custom install directories from the app manifest.
        std::vector<uint8_t> bytes; std::wstring folder=L"Dishonored";
        if(fs::read_file(fs::join(lib,L"steamapps\\appmanifest_205100.acf"),&bytes,nullptr)) {
            std::string s(bytes.begin(),bytes.end());auto at=s.find("\"installdir\"");
            if(at!=std::string::npos) { at=s.find('"',at+12);auto end=at==std::string::npos?at:s.find('"',at+1);
                if(end!=std::string::npos) { auto name=fs::widen(s.substr(at+1,end-at-1));if(name.find_first_of(L"\\/:")==std::wstring::npos && name!=L"..")folder=name; }
            }
        }
        add(fs::join(lib,L"steamapps\\common\\"+folder),Store::Steam);
    }
    for(HKEY hive:{HKEY_LOCAL_MACHINE,HKEY_CURRENT_USER}) for(REGSAM view:{KEY_WOW64_32KEY,KEY_WOW64_64KEY}) {
        const wchar_t* base=L"SOFTWARE\\GOG.com\\Games";
        for(const auto& id:subkeys(hive,base,view)) {
            auto key=std::wstring(base)+L"\\"+id;
            auto name=read_reg(hive,key,L"gameName",view);
            if(name.find(L"Dishonored")==std::wstring::npos)continue;
            add(read_reg(hive,key,L"path",view),Store::Gog,id);
        }
        base=L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall";
        for(const auto& id:subkeys(hive,base,view)) {
            auto key=std::wstring(base)+L"\\"+id;
            auto name=read_reg(hive,key,L"DisplayName",view);
            if(name.find(L"Dishonored")==std::wstring::npos)continue;
            add(read_reg(hive,key,L"InstallLocation",view));
        }
    }
    for(const wchar_t* var:{L"ProgramFiles(x86)",L"ProgramW6432",L"ProgramFiles"}) {
        const auto base=fs::env(var); if(base.empty())continue;
        for(const wchar_t* tail:{L"GOG Galaxy\\Dishonored",L"GOG Galaxy\\Games\\Dishonored",L"GOG Games\\Dishonored",L"GOG Games\\Dishonored - Definitive Edition"})add(fs::join(base,tail),Store::Gog);
    }
    // GOG offline installers commonly use a drive-root GOG Games folder.
    wchar_t drives[1024];DWORD len=GetLogicalDriveStringsW(1024,drives);
    if(len && len<1024)for(const wchar_t* d=drives;*d;d+=wcslen(d)+1)if(GetDriveTypeW(d)==DRIVE_FIXED) {
        add(fs::join(d,L"GOG Games\\Dishonored"),Store::Gog);
        add(fs::join(d,L"GOG Games\\Dishonored - Definitive Edition"),Store::Gog);
    }
    return out;
}
Launch launch_command(const Game& g,const std::wstring& galaxy) {
    Launch r;
    if(!g.valid || g.unsupported64) {r.error="Select a supported 32-bit game first.";return r;}
    if(g.store==Store::Steam)r.target=L"steam://rungameid/205100";
    else if(g.store==Store::Gog) {
        if(galaxy.empty() || !digits(g.gogId))r.error="GOG Galaxy or this game's GOG ID could not be found. Open the game from your GOG library.";
        else {r.target=galaxy;r.args=L"/command=runGame /gameId="+g.gogId+L" /path="+process::quote_arg(g.root);}
    } else r.error="The store could not be identified. Launch Dishonored from your Steam or GOG library.";
    return r;
}
const char* launch_label(Store s) {return s==Store::Gog?"Launch via GOG":s==Store::Steam?"Launch via Steam":"Launch via store";}
}
