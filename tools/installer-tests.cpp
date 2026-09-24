// tools/installer-tests.cpp - unit tests for the installer's system helpers
// (src/tools/installer/sys), built and run by tools\installer-host.ps1 with the
// plain cl.exe the other *-host.ps1 suites use. Every fixture is SYNTHESISED
// here: no game ini is ever copied into the tree.
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include "sys/fs.h"
#include "sys/game_ini.h"
#include "sys/profile.h"
#include "sys/steam.h"
#include "sys/install_record.h"
#include "sys/process.h"
#include <shlobj.h>
#include "model/choices.h"

using namespace dvr::setup;

static int g_failed = 0, g_passed = 0;
#define CHECK(cond) do { if (cond) { ++g_passed; } else { ++g_failed; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { const auto _a = (a); const auto _b = (b); if (_a == _b) { ++g_passed; } else { ++g_failed; printf("FAIL %s:%d  %s != %s\n", __FILE__, __LINE__, #a, #b); } } while (0)

static std::vector<uint8_t> bytes(const std::string& s) { return std::vector<uint8_t>(s.begin(), s.end()); }
static std::string str(const std::vector<uint8_t>& b) { return std::string(b.begin(), b.end()); }

// A DishonoredEngine.ini shaped like the real one: CRLF, the same key in two
// sections, blank lines before the next header.
static const char* kEngine =
    "[Engine.Engine]\r\n"
    "bSmoothFrameRate=TRUE\r\n"
    "MinSmoothedFrameRate=22\r\n"
    "\r\n"
    "[SystemSettings]\r\n"
    "DepthOfField=True\r\n"
    "Fullscreen=True\r\n"
    "UseVsync=True\r\n"
    "\r\n"
    "\r\n"
    "[SystemSettingsMobile]\r\n"
    "DepthOfField=True\r\n"
    "Fullscreen=True\r\n"
    "UseVsync=True\r\n";

static void test_game_ini_scoped()
{
    GameIni ini;
    ini.parse(bytes(kEngine));
    std::string note;
    CHECK(ini.set("SystemSettings", "DepthOfField", "False", &note) == GameIni::Result::Changed);
    CHECK(note.find("True -> False") != std::string::npos);
    const std::string out = str(ini.serialise());
    // only the [SystemSettings] line changed; the Mobile one is untouched
    CHECK(out.find("[SystemSettings]\r\nDepthOfField=False\r\n") != std::string::npos);
    CHECK(out.find("[SystemSettingsMobile]\r\nDepthOfField=True\r\n") != std::string::npos);
    // every other byte identical
    std::string expect = kEngine;
    expect.replace(expect.find("DepthOfField=True"), strlen("DepthOfField=True"), "DepthOfField=False");
    CHECK_EQ(out, expect);
}

static void test_game_ini_case()
{
    GameIni ini;
    ini.parse(bytes(kEngine));
    std::string note;
    // FALSE is not False to the engine: a case difference is a change
    CHECK(ini.set("engine.engine", "bsmoothframerate", "FALSE", &note) == GameIni::Result::Changed);
    CHECK(ini.set("Engine.Engine", "bSmoothFrameRate", "FALSE", &note) == GameIni::Result::Unchanged);
    CHECK(note.find("already set") != std::string::npos);
    const std::string out = str(ini.serialise());
    CHECK(out.find("bsmoothframerate=FALSE\r\n") != std::string::npos);   // changed write uses supplied spelling; unchanged write preserves it
}

static void test_game_ini_append()
{
    GameIni ini;
    ini.parse(bytes(kEngine));
    std::string note;
    CHECK(ini.set("Engine.Engine", "bNewKey", "1", &note) == GameIni::Result::Changed);
    CHECK(note.find("MISSING") != std::string::npos);
    const std::string out = str(ini.serialise());
    // appended after the last non-blank line of the section, before its blank line
    CHECK(out.find("MinSmoothedFrameRate=22\r\nbNewKey=1\r\n\r\n[SystemSettings]") != std::string::npos);
    // the last section too, on a file that ends without a newline
    GameIni tail;
    tail.parse(bytes("[A]\r\nk=1\r\n[B]\r\nx=2"));
    CHECK(tail.set("B", "y", "3", &note) == GameIni::Result::Changed);
    CHECK_EQ(str(tail.serialise()), std::string("[A]\r\nk=1\r\n[B]\r\nx=2\r\ny=3\r\n"));
}

static void test_game_ini_no_section()
{
    GameIni ini;
    ini.parse(bytes(kEngine));
    std::string note;
    CHECK(ini.set("Nope", "k", "v", &note) == GameIni::Result::NoSection);
    CHECK(!ini.dirty());
    CHECK_EQ(str(ini.serialise()), std::string(kEngine));
}

static void test_game_ini_encodings()
{
    // UTF-8 BOM survives
    GameIni bom;
    bom.parse(bytes(std::string("\xEF\xBB\xBF") + kEngine));
    std::string note;
    bom.set("SystemSettings", "UseVsync", "False", &note);
    const std::string out = str(bom.serialise());
    CHECK(out.compare(0, 3, "\xEF\xBB\xBF") == 0);
    CHECK(out.find("UseVsync=False\r\n\r\n\r\n[SystemSettingsMobile]") != std::string::npos);
    // LF-only file keeps LF and appends with LF
    GameIni lf;
    lf.parse(bytes("[A]\nk=1\n"));
    lf.set("A", "n", "2", &note);
    CHECK_EQ(str(lf.serialise()), std::string("[A]\nk=1\nn=2\n"));
    // UTF-16 LE round trip
    const std::wstring w = L"\xFEFF[Engine.Engine]\r\nbSmoothFrameRate=TRUE\r\n";
    std::vector<uint8_t> raw((const uint8_t*)w.data(), (const uint8_t*)w.data() + w.size() * 2);
    GameIni u16;
    u16.parse(raw);
    CHECK(u16.set("Engine.Engine", "bSmoothFrameRate", "FALSE", &note) == GameIni::Result::Changed);
    const std::vector<uint8_t> back = u16.serialise();
    CHECK(back.size() >= 2 && back[0] == 0xFF && back[1] == 0xFE);
    const std::wstring w2((const wchar_t*)back.data(), back.size() / 2);
    CHECK_EQ(w2, std::wstring(L"\xFEFF[Engine.Engine]\r\nbSmoothFrameRate=FALSE\r\n"));
}

static void test_baseline_table()
{
    CHECK_EQ(std::string(kVrBaseline[0].value), std::string("FALSE"));
    CHECK_EQ(std::string(kVrBaseline[1].value), std::string("False"));
    CHECK_EQ(std::string(kVrBaseline[2].value), std::string("False"));
    CHECK_EQ(std::string(kVrBaseline[3].value), std::string("FALSE"));
    CHECK_EQ(std::string(kVrBaseline[3].file), std::string("DishonoredInput.ini"));
}

static void test_sizes()
{
    // the numbers PERFORMANCE.md records for the F10 picker
    CHECK((size_for_percent(100.0f) == Size{ 2750, 2850 }));
    CHECK((size_for_percent(110.0f) == Size{ 2884, 2989 }));
    CHECK((size_for_percent(120.0f) == Size{ 3012, 3122 }));
    CHECK((size_for_percent(75.0f) == Size{ 2382, 2468 }));
    CHECK(quality_for_percent(percent_for_size({ 2750, 2850 })) == Quality::Balanced);
    CHECK(quality_for_percent(percent_for_size({ 3012, 3122 })) == Quality::Quality);
    CHECK(quality_for_percent(110.0f) == Quality::Custom);
    Runtime r;
    CHECK(runtime_from_ini(L"native", L"C:\\x.json", &r) && r == Runtime::Vdxr);
    CHECK(runtime_from_ini(L"native", L"", &r) && r == Runtime::Auto);
    CHECK(runtime_from_ini(L"steamvr", L"", &r) && r == Runtime::SteamVr);
    CHECK(parse_runtime(L"shim", &r) && r == Runtime::SteamVr);
}

static void test_record()
{
    InstallRecord a;
    a.version = "41.0.0"; a.buildId = "702-g1a2b\"quoted"; a.config = "RelWithDebInfo"; a.dllSha256 = "abc";
    a.installedUtc = "2026-09-23T00:00:00Z"; a.runtime = "vdxr"; a.quality = "custom"; a.width = 2884; a.height = 2989; a.elevated = true;
    InstallRecord b;
    CHECK(record_from_json(record_to_json(a), &b));
    CHECK(b.valid);
    CHECK_EQ(b.buildId, a.buildId);
    CHECK_EQ(b.width, 2884); CHECK_EQ(b.height, 2989);
    CHECK(b.elevated);
    CHECK(!record_from_json("{ \"version\": 1 }", &b));
}

static void test_sha_and_search()
{
    std::string hex;
    CHECK(fs::sha256_bytes("abc", 3, &hex));
    CHECK_EQ(hex, std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
    CHECK(fs::contains_ascii("xx legacy code is COMPILED IN yy", 32, "legacy code is COMPILED IN"));
    CHECK(!fs::contains_ascii("xx legacy code is compiled in yy", 32, "legacy code is COMPILED IN"));
}

static void test_quote()
{
    CHECK_EQ(process::quote_arg(L"plain"), std::wstring(L"plain"));
    CHECK_EQ(process::quote_arg(L"C:\\Program Files\\x"), std::wstring(L"\"C:\\Program Files\\x\""));
    CHECK_EQ(process::quote_arg(L"a\\"), std::wstring(L"a\\"));
    CHECK_EQ(process::quote_arg(L"a b\\"), std::wstring(L"\"a b\\\\\""));
    CHECK_EQ(process::quote_arg(L"say \"hi\""), std::wstring(L"\"say \\\"hi\\\"\""));
}

static void test_profile_on_disk()
{
    const std::wstring dir = fs::join(fs::temp_dir(), L"dvr-installer-tests");
    DWORD err = 0;
    fs::make_dir(dir, &err);
    const std::wstring ini = fs::join(dir, L"t.ini");
    const std::string body = "; comment\r\n[VR]\r\nRuntime=auto\r\nXrRuntimeJson=\r\n[Paths]\r\nDataDir=D:\\dvr-data";   // no trailing newline
    CHECK(fs::write_file_atomic(ini, body.data(), body.size(), &err));
    CHECK(profile::ensure_trailing_newline(ini, &err));
    CHECK(profile::set(ini, L"VR", L"Runtime", L"steamvr", &err));
    CHECK(profile::set(ini, L"Paths", L"DataDir", L"", &err));
    CHECK_EQ(profile::get(ini, L"VR", L"Runtime"), std::wstring(L"steamvr"));
    CHECK_EQ(profile::get(ini, L"Paths", L"DataDir", L"unset"), std::wstring(L""));
    std::vector<uint8_t> b;
    CHECK(fs::read_file(ini, &b, &err));
    const std::string out = str(b);
    CHECK(out.find("; comment\r\n") == 0);                 // the comment survived
    CHECK(out.find("DataDir=\r\n") != std::string::npos);  // empty, not deleted
    size_t crlf = 0, lf = 0; bool bom = true;
    CHECK(profile::inspect(ini, &crlf, &lf, &bom));
    CHECK_EQ(crlf, lf); CHECK(!bom);
    fs::delete_file(ini, &err);
}

static void test_vdf()
{
    // steam::libraries() reads the real registry; exercise the game-dir normaliser instead
    const std::wstring dir = fs::join(fs::temp_dir(), L"dvr-installer-tests\\Dishonored");
    DWORD err = 0;
    fs::make_dir(fs::parent(dir), &err); fs::make_dir(dir, &err);
    fs::make_dir(fs::join(dir, L"Binaries"), &err); fs::make_dir(fs::join(dir, L"Binaries\\Win32"), &err);
    const std::wstring exe = fs::join(dir, L"Binaries\\Win32\\Dishonored.exe");
    CHECK(fs::write_file_atomic(exe, "MZ", 2, &err));
    CHECK_EQ(steam::normalise_game_dir(dir), fs::join(dir, L"Binaries\\Win32"));
    CHECK_EQ(steam::normalise_game_dir(fs::join(dir, L"Binaries")), fs::join(dir, L"Binaries\\Win32"));
    CHECK_EQ(steam::normalise_game_dir(fs::join(dir, L"Binaries\\Win32\\")), fs::join(dir, L"Binaries\\Win32"));
    CHECK_EQ(steam::normalise_game_dir(fs::temp_dir()), std::wstring(L""));
    fs::delete_file(exe, &err);
}

static void test_shortcut()
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    DWORD err = 0;
    const auto dir = fs::join(fs::temp_dir(), L"dvr-installer-tests");
    fs::make_dir(dir, &err);
    const auto path = fs::join(dir, L"launcher.lnk");
    const auto exe = fs::module_path();
    const std::wstring args = L"--game-dir " + process::quote_arg(L"C:\\Games with spaces\\Dishonored\\Binaries\\Win32");
    CHECK(process::write_shortcut(path, exe, args, &err));
    IShellLinkW* link = nullptr;
    CHECK(SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link))));
    if (link) {
        IPersistFile* file = nullptr;
        CHECK(SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&file))));
        if (file) {
            CHECK(SUCCEEDED(file->Load(path.c_str(), STGM_READ)));
            wchar_t actual[2048] = {};
            CHECK(SUCCEEDED(link->GetPath(actual, 2048, nullptr, SLGP_RAWPATH)));
            CHECK(fs::iequals(actual, exe));
            CHECK(SUCCEEDED(link->GetArguments(actual, 2048)));
            CHECK_EQ(std::wstring(actual), args);
            file->Release();
        }
        link->Release();
    }
    fs::delete_file(path, &err);
    CoUninitialize();
}

int main()
{
    test_shortcut();
    test_game_ini_scoped();
    test_game_ini_case();
    test_game_ini_append();
    test_game_ini_no_section();
    test_game_ini_encodings();
    test_baseline_table();
    test_sizes();
    test_record();
    test_sha_and_search();
    test_quote();
    test_profile_on_disk();
    test_vdf();
    printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed ? 1 : 0;
}
