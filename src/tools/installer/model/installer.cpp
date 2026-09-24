// tools/installer/model/installer.cpp - see installer.h.
#include "model/installer.h"
#include "sys/fs.h"
#include "sys/steam.h"
#include "sys/profile.h"
#include "sys/game_ini.h"
#include "sys/resources.h"
#include "payload_ids.h"
#include "dvr_version.h"
#include "core/util/log.h"
#include <shlobj.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

namespace dvr::setup {

namespace {
constexpr const wchar_t* kIniName = L"dishonored_vr.ini";
constexpr const wchar_t* kLogName = L"dishonored_vr.log";
constexpr const wchar_t* kDisableName = L"disable_vr.txt";
constexpr const wchar_t* kBackupName = L"d3d9.dll.dvr-backup";
constexpr const char* kLegacyMarker = "legacy code is COMPILED IN";
constexpr const wchar_t* kDevDataDir = L"D:\\dvr-data";   // the dev PC's drive, shipped by mistake once

std::string n(const std::wstring& w) { return fs::narrow(w); }

std::wstring active_runtime_name()
{
    // HKLM\SOFTWARE\Khronos is redirected to WOW6432Node for this 32-bit process,
    // which is the view the game itself reads (openxr_runtime.cpp).
    wchar_t buf[2048]; DWORD size = sizeof(buf);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Khronos\\OpenXR\\1", L"ActiveRuntime", RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS)
        return L"";
    return fs::filename(buf);
}

struct Payload {
    resources::Blob d3d9, shim, openvr, ini;
    bool ok() const { return d3d9.ok() && shim.ok() && openvr.ok() && ini.ok(); }
};
// [Meta] Version of the embedded ini: the version the mod's own refresh writes
// (kConfigVersion, which the packaged ini is a byte copy of). An installed ini
// below it is thrown away by the mod at the next launch, keeping three keys.
int embedded_ini_version(const resources::Blob& ini)
{
    if (!ini.ok()) return 0;
    const std::string text((const char*)ini.data, ini.size);
    const size_t meta = text.find("[Meta]");
    if (meta == std::string::npos) return 0;
    const size_t v = text.find("Version=", meta);
    if (v == std::string::npos) return 0;
    return atoi(text.c_str() + v + 8);
}
// The mod's version refresh, done here instead of at the next launch: the mod
// would replace the whole file and keep only [VR] Runtime, XrRuntimeJson and a
// non-empty DataDir, so the render size written below would be lost and an
// empty DataDir would come back as the dev PC's drive (measured 2026-09-23 on an
// ini at version 13). Replacing it now, with the old file kept beside it, means
// every choice the player just made survives the first launch.
bool refresh_outdated_ini(Report* r, const Detection& det, const Payload& p)
{
    if (!det.iniExists || det.iniVersion >= det.embeddedIniVersion) return true;
    const std::wstring ini = fs::join(det.gameDir, kIniName);
    const std::wstring backup = ini + L"." + fs::timestamp_local() + L".dvr-backup";
    DWORD err = 0;
    if (!fs::copy_file(ini, backup, &err)) { r->fail("Could not back up dishonored_vr.ini", err); return false; }
    if (!fs::write_file_atomic(ini, p.ini.data, p.ini.size, &err)) { r->fail("Could not refresh dishonored_vr.ini", err); return false; }
    r->add(StepStatus::Ok, fs::format("Refreshed dishonored_vr.ini from version %d to %d", det.iniVersion, det.embeddedIniVersion),
           fs::format("It was from an older build, and the mod would have replaced it at the next launch keeping only the runtime choice. Replaced now with the tested settings, so the choices below stick. The old file is beside it as %s.", n(fs::filename(backup)).c_str()));
    return true;
}
Payload payload()
{
    Payload p;
    p.d3d9 = resources::rcdata(IDR_D3D9);
    p.shim = resources::rcdata(IDR_SHIM);
    p.openvr = resources::rcdata(IDR_OPENVR);
    p.ini = resources::rcdata(IDR_INI);
    return p;
}

bool game_running_blocks(Report* r, process::Running running)
{
    if (running == process::Running::No) return false;
    if (running == process::Running::Yes)
        r->add(StepStatus::Failed, "Dishonored is running",
               "Its d3d9.dll is in use. Quit the game all the way to the desktop (through its own menu, not the window), then try again.");
    else
        r->add(StepStatus::Failed, "Could not check whether Dishonored is running",
               "The process list could not be read, and replacing a loaded DLL would fail. Close the game if it is open and try again.");
    return true;
}

bool write_payload_file(Report* r, const std::wstring& dir, const wchar_t* name, const resources::Blob& b)
{
    DWORD err = 0;
    if (!fs::write_file_atomic(fs::join(dir, name), b.data, b.size, &err)) {
        r->fail(fs::format("Could not write %s", n(name).c_str()), err);
        return false;
    }
    r->add(StepStatus::Ok, fs::format("Installed %s", n(name).c_str()), fs::format("%u KB", (unsigned)(b.size / 1024)));
    return true;
}

bool remove_if_present(Report* r, const std::wstring& dir, const wchar_t* name, const char* why)
{
    const std::wstring p = fs::join(dir, name);
    if (!fs::exists(p)) return true;
    DWORD err = 0;
    if (!fs::delete_file(p, &err)) { r->fail(fs::format("Could not remove %s", n(name).c_str()), err); return false; }
    r->add(StepStatus::Ok, fs::format("Removed %s", n(name).c_str()), why);
    return true;
}

// Baseline choices plus explicitly selected preferences; all other lines stay.
bool apply_choices(Report* r, const Detection& det, const Choices& c)
{
    const std::wstring ini = fs::join(det.gameDir, kIniName);
    DWORD err = 0;
    if (!profile::ensure_trailing_newline(ini, &err)) { r->fail("Could not prepare dishonored_vr.ini", err); return false; }

    Runtime rt = c.runtime;
    std::wstring json;
    if (rt == Runtime::Vdxr) {
        json = c.vdxrJson.empty() ? det.vdxrJson : c.vdxrJson;
        if (!fs::is_file(json)) {
            r->add(StepStatus::Warn, "Virtual Desktop's runtime was not found",
                   fs::format("No manifest at %s. The mod will pick the runtime itself (Runtime=auto); choose again from this launcher once Virtual Desktop Streamer is installed.", n(json).c_str()));
            rt = Runtime::Auto; json.clear();
        }
    }
    if (!profile::set(ini, L"VR", L"Runtime", runtime_ini_value(rt), &err) ||
        !profile::set(ini, L"VR", L"XrRuntimeJson", json, &err)) {
        r->fail("Could not write the VR runtime choice", err); return false;
    }
    r->add(StepStatus::Ok, fs::format("Headset: %s", runtime_label(rt)),
           rt == Runtime::Vdxr ? fs::format("[VR] Runtime=native, XrRuntimeJson=%s", n(json).c_str())
           : rt == Runtime::SteamVr ? "[VR] Runtime=steamvr (the bundled dvr_steamvr32.dll shim; start SteamVR before the game)"
           : "[VR] Runtime=auto (the system's 32-bit OpenXR runtime, the shim when there is none)");

    const Size s = c.size();
    if (!profile::set(ini, L"Screen", L"RenderWidth", std::to_wstring(s.w), &err) ||
        !profile::set(ini, L"Screen", L"RenderHeight", std::to_wstring(s.h), &err)) {
        r->fail("Could not write the render size", err); return false;
    }
    r->add(StepStatus::Ok, fs::format("Render size: %s, %ux%u per eye", quality_label(c.quality), s.w, s.h),
           fs::format("%.0f%% of the tested 2750x2850%s. Recommended: 120 Hz, or 144 Hz with Virtual Desktop Beta.",
                      percent_for_size(s), (c.exact.w && c.exact.h) ? ", kept exactly as it was" : ""));

    for (int i = 0; i < PreferenceCount; ++i) {
        const int value = c.preferences[i];
        if (value < 0) continue;
        const Preference& p = kPreferences[i];
        if (!profile::set(ini, p.section, p.key, std::to_wstring(value), &err)) {
            r->fail(std::string("Could not save ") + p.label, err); return false;
        }
        r->add(StepStatus::Ok, std::string(p.label) + ": " +
               (i == Modifier ? std::to_string(value) : ((p.inverted ? !value : value) ? "on" : "off")),
               fs::format("[%s] %s=%d", n(p.section).c_str(), n(p.key).c_str(), value));
    }

    // [Paths] DataDir: empty means %LOCALAPPDATA%\DishonoredVR. A value the player
    // set on purpose is kept; the dev PC's drive that a build once shipped is not.
    const std::wstring dd = profile::get(ini, L"Paths", L"DataDir");
    if (dd.empty() || fs::iequals(dd, kDevDataDir)) {
        if (!profile::set(ini, L"Paths", L"DataDir", L"", &err)) { r->fail("Could not write the data folder", err); return false; }
        r->add(StepStatus::Ok, "Data folder: %LOCALAPPDATA%\\DishonoredVR", "[Paths] DataDir= (empty)");
    } else {
        r->add(StepStatus::Skipped, "Data folder kept", fs::format("[Paths] DataDir=%s was set by hand and stays.", n(dd).c_str()));
    }

    size_t crlf = 0, lf = 0; bool bom = false;
    if (profile::inspect(ini, &crlf, &lf, &bom) && (crlf != lf || bom))
        r->add(StepStatus::Warn, "dishonored_vr.ini has mixed line endings or a byte-order mark",
               fs::format("CRLF %u, LF %u%s. The mod reads it anyway; delete it to get a fresh copy at the next launch.", (unsigned)crlf, (unsigned)lf, bom ? ", UTF-8 BOM" : ""));
    return true;
}

// The four game-ini values, exactly tools/setup-game-ini.ps1 -VRBaseline.
bool apply_baseline(Report* r, const Detection& det)
{
    if (!det.configExists) {
        r->add(StepStatus::Skipped, "Game settings: waiting for the game's first run",
               fs::format("The game writes its own settings folder the first time it runs (%s). Launch Dishonored once from Steam or GOG, quit to the desktop, and this window applies the last four settings by itself.", n(det.configDir).c_str()));
        r->baselinePending = true;
        return true;
    }
    bool ok = true;
    const char* files[] = { "DishonoredEngine.ini", "DishonoredInput.ini" };
    for (const char* file : files) {
        const std::wstring path = fs::join(det.configDir, fs::widen(file));
        GameIni ini;
        std::wstring err;
        if (!GameIni::load(path, &ini, &err)) {
            r->add(StepStatus::Failed, fs::format("Could not read %s", file), n(err));
            ok = false; continue;
        }
        std::string notes;
        bool refused = false;
        for (const BaselineWrite& w : kVrBaseline) {
            if (strcmp(w.file, file) != 0) continue;
            std::string note;
            const GameIni::Result res = ini.set(w.section, w.key, w.value, &note);
            if (res == GameIni::Result::NoSection) refused = true;
            notes += (notes.empty() ? "" : "\n") + note;
        }
        if (refused) { r->add(StepStatus::Failed, fs::format("%s is not the shape this expects", file), notes + "\nNothing was written to it."); ok = false; continue; }
        if (!ini.dirty()) { r->add(StepStatus::Ok, fs::format("%s already set", file), notes); continue; }
        const std::wstring backup = path + L"." + fs::timestamp_local() + L".dvr-backup";
        DWORD werr = 0;
        if (!fs::copy_file(path, backup, &werr)) { r->fail(fs::format("Could not back up %s", file), werr); ok = false; continue; }
        if (!ini.save(&err)) { r->add(StepStatus::Failed, fs::format("Could not write %s", file), n(err)); ok = false; continue; }
        r->add(StepStatus::Ok, fs::format("%s: VR baseline applied", file), notes + "\nBacked up to " + n(fs::filename(backup)) + ". The game rewrites these when you use its own video options; re-run Change settings afterwards.");
    }
    if (ok) r->baselineApplied = true;
    return ok;
}

bool write_install_record(Report* r, const Detection& det, const Choices* c)
{
    InstallRecord rec;
    rec.version = det.version; rec.buildId = det.buildId; rec.config = det.config;
    rec.dllSha256 = det.embeddedSha;
    rec.installedUtc = fs::utc_now_iso();
    rec.elevated = process::is_elevated();
    if (c) {
        rec.runtime = runtime_token(c->runtime); rec.quality = quality_token(c->quality);
        const Size s = c->size(); rec.width = (int)s.w; rec.height = (int)s.h;
    } else if (det.record.valid) {
        rec.runtime = det.record.runtime; rec.quality = det.record.quality; rec.width = det.record.width; rec.height = det.record.height;
    }
    DWORD err = 0;
    if (!write_record(det.gameDir, rec, &err)) { r->fail("Could not write the install record", err); return false; }
    r->add(StepStatus::Ok, fs::format("Recorded the install: %s (%s, %s)", rec.version.c_str(), rec.buildId.c_str(), rec.config.c_str()),
           "dishonored_vr_install.json beside the game, so this launcher can update or remove exactly what it put there.");
    return true;
}
} // namespace

void Report::add(StepStatus s, std::string title, std::string detail)
{
    if (s == StepStatus::Failed) ok = false;
    std::string oneLine = detail;
    for (size_t at = oneLine.find('\n'); at != std::string::npos; at = oneLine.find('\n', at + 3)) oneLine.replace(at, 1, " | ");
    DVR_LOG(::dvr::log::Cat::core, s == StepStatus::Failed ? ::dvr::log::Level::Error : s == StepStatus::Warn ? ::dvr::log::Level::Warn : ::dvr::log::Level::Info,
            "setup: %s%s%s", title.c_str(), oneLine.empty() ? "" : " - ", oneLine.c_str());
    steps.push_back({ std::move(title), std::move(detail), s });
}
void Report::fail(std::string title, DWORD err, std::string prefix)
{
    if (err == ERROR_ACCESS_DENIED) accessDenied = true;
    add(StepStatus::Failed, std::move(title), prefix + n(fs::win_error_text(err)));
}

std::wstring default_config_dir()
{
    const std::wstring docs = fs::known_folder(FOLDERID_Documents);
    return docs.empty() ? L"" : fs::join(docs, L"My Games\\Dishonored\\DishonoredGame\\Config");
}

Detection detect(const Env& env)
{
    Detection d;
    d.version = DVR_VERSION; d.buildId = DVR_BUILD_ID; d.config = DVR_BUILD_CONFIG;

    const Payload p = payload();
    d.payloadOk = p.ok();
    if (p.d3d9.ok()) {
        fs::sha256_bytes(p.d3d9.data, p.d3d9.size, &d.embeddedSha);
        d.embeddedLegacy = fs::contains_ascii(p.d3d9.data, p.d3d9.size, kLegacyMarker);
    }
    d.embeddedIniVersion = embedded_ini_version(p.ini);

    // the game
    d.games = discovery::find_games();
    std::wstring selected = env.gameDirOverride.empty() ? fs::env(L"DVR_GAME_DIR") : env.gameDirOverride;
    if (!selected.empty()) {
        d.game = discovery::inspect(selected);
        for (const auto& game : d.games) if (fs::iequals(game.dir,d.game.dir)) { d.game=game; break; }
    } else {
        // Prefer a valid installation, retaining an incompatible candidate for its warning.
        for (const auto& game : d.games) if (game.valid) { d.game=game; break; }
        if (d.game.dir.empty() && !d.games.empty()) d.game=d.games.front();
        if (d.games.empty()) d.game=discovery::inspect(L"");
    }
    d.gameDir=d.game.dir; d.gameFound=d.game.valid; d.gameNote=d.game.note;
    d.running = process::is_running(kGameExe);
    if (d.gameFound) {
        d.gameWritable = fs::probe_writable(d.gameDir, &d.gameWriteErr);
    }
    d.configDir = env.configDirOverride.empty() ? default_config_dir() : env.configDirOverride;
    d.configExists = !d.configDir.empty() && fs::is_dir(d.configDir);
    if (d.configExists) d.configWritable = fs::probe_writable(d.configDir, &d.configWriteErr);
    d.needsElevation = !env.elevated && !process::is_elevated() &&
                       ((d.gameFound && !d.gameWritable && d.gameWriteErr == ERROR_ACCESS_DENIED) ||
                        (d.configExists && !d.configWritable && d.configWriteErr == ERROR_ACCESS_DENIED));
    d.d3dcompiler = fs::is_file(fs::join(fs::system_dir(), L"d3dcompiler_47.dll"));

    // the runtimes
    d.vdxrJson = env.vdxrJsonOverride.empty() ? default_vdxr_json() : env.vdxrJsonOverride;
    d.vdxrPresent = fs::is_file(d.vdxrJson);
    d.steamvrPresent = steam::steamvr_installed();
    d.activeRuntime = active_runtime_name();
    d.gpu = gpu::primary();

    // what is there already
    if (d.gameFound) {
        const std::wstring dll = fs::join(d.gameDir, L"d3d9.dll");
        d.d3d9Present = fs::is_file(dll);
        if (d.d3d9Present) fs::sha256_file(dll, &d.installedSha, nullptr);
        d.backupPresent = fs::is_file(fs::join(d.gameDir, kBackupName));
        read_record(d.gameDir, &d.record);
        d.iniExists = fs::is_file(fs::join(d.gameDir, kIniName));
        const bool trace = d.iniExists || fs::is_file(fs::join(d.gameDir, kLogName)) || fs::is_file(fs::join(d.gameDir, L"dxvk_d3d9.dll"));
        d.modInstalled = d.d3d9Present && (d.record.valid || trace || d.installedSha == d.embeddedSha);
        d.foreignD3d9 = d.d3d9Present && !d.modInstalled && !d.backupPresent;
        d.disabled = fs::is_file(fs::join(d.gameDir, kDisableName));
        if (d.iniExists) {
            const std::wstring ini = fs::join(d.gameDir, kIniName);
            d.iniVersion = profile::get_int(ini, L"Meta", L"Version", 0);
            d.iniJson = profile::get(ini, L"VR", L"XrRuntimeJson");
            runtime_from_ini(profile::get(ini, L"VR", L"Runtime", L"auto"), d.iniJson, &d.iniRuntime);
            d.iniSize.w = (uint32_t)profile::get_int(ini, L"Screen", L"RenderWidth", 0);
            d.iniSize.h = (uint32_t)profile::get_int(ini, L"Screen", L"RenderHeight", 0);
            d.iniDataDir = profile::get(ini, L"Paths", L"DataDir");
            for (int i = 0; i < PreferenceCount; ++i)
                d.suggested.preferences[i] = profile::get_int(ini, kPreferences[i].section, kPreferences[i].key, -1);
        }
    }

    // the preselection: what is installed, then the ini, then the machine
    if (d.iniExists) {   // an ini beside the game is the player's, with or without the DLL
        d.suggested.runtime = d.iniRuntime;
        if (d.iniRuntime == Runtime::Vdxr) d.suggested.vdxrJson = d.iniJson;
        if (d.iniSize.w && d.iniSize.h) d.suggested.keep(d.iniSize);
        else d.suggested.choose(Quality::Balanced);
    } else {
        d.suggested.runtime = Runtime::Auto;
        d.suggested.quality = Quality::Balanced;
        d.suggested.pixelPercent = percent_for_quality(d.suggested.quality, kBalancedPercent);
    }
    DVR_INFO("setup: detect game=%s (%s) running=%d writable=%d config=%s exists=%d elevate=%d d3dcompiler=%d vdxr=%d steamvr=%d active=%s gpu=%s budget=%llu MB installed=%d sha=%.8s embedded=%.8s legacy=%d",
             n(d.gameDir).c_str(), d.gameNote.c_str(), (int)d.running, d.gameWritable, n(d.configDir).c_str(), d.configExists, d.needsElevation,
             d.d3dcompiler, d.vdxrPresent, d.steamvrPresent, n(d.activeRuntime).c_str(), n(d.gpu.name).c_str(), (unsigned long long)(d.gpu.budgetBytes >> 20),
             d.modInstalled, d.installedSha.c_str(), d.embeddedSha.c_str(), d.embeddedLegacy);
    for (int i = 0; i < PreferenceCount; ++i)
        DVR_INFO("launcher: saved [%s] %s=%d (-1=use shipped default)",
                 n(kPreferences[i].section).c_str(), n(kPreferences[i].key).c_str(), d.suggested.preferences[i]);
    return d;
}

Report do_install(const Env& env, const Detection& det, const Choices& choices)
{
    (void)env;
    Report r;
    const Payload p = payload();
    if (!p.ok()) { r.add(StepStatus::Failed, "This installer is missing its payload", "The exe was built without the mod inside it. Download it again."); return r; }
    if (det.embeddedLegacy)
        r.add(StepStatus::Warn, "This build carries the retired diagnostics (legacy)", "It stalls on every trigger pull (VR-180). Use a release build for play.");
    if (!det.gameFound) { r.add(StepStatus::Failed, "Dishonored was not found", det.gameNote); return r; }
    if (game_running_blocks(&r, det.running)) return r;
    if (!det.d3dcompiler)
        r.add(StepStatus::Warn, "d3dcompiler_47.dll is missing from Windows",
              "The mod needs it to reach the headset (the log will say 'no D3D11 blit'). It ships with the DirectX End-User Runtime and every supported Windows; run Windows Update.");

    // a foreign d3d9.dll (another mod, a wrapper) is kept once, restored by Uninstall
    if (det.foreignD3d9) {
        DWORD err = 0;
        if (!fs::copy_file(fs::join(det.gameDir, L"d3d9.dll"), fs::join(det.gameDir, kBackupName), &err)) { r.fail("Could not back up the existing d3d9.dll", err); return r; }
        r.add(StepStatus::Ok, "Backed up the existing d3d9.dll", "It was not this mod's. Uninstall puts it back (d3d9.dll.dvr-backup).");
    }
    if (!write_payload_file(&r, det.gameDir, L"d3d9.dll", p.d3d9)) return r;
    if (!write_payload_file(&r, det.gameDir, L"dvr_steamvr32.dll", p.shim)) return r;
    if (!write_payload_file(&r, det.gameDir, L"openvr_api.dll", p.openvr)) return r;
    remove_if_present(&r, det.gameDir, L"dxvk_d3d9.dll", "The DXVK layer from releases before 41.0; the game renders natively now.");
    remove_if_present(&r, det.gameDir, L"dxvk_stereo.txt", "Its marker file.");

    const std::wstring ini = fs::join(det.gameDir, kIniName);
    if (!det.iniExists) {
        DWORD err = 0;
        if (!fs::write_file_atomic(ini, p.ini.data, p.ini.size, &err)) { r.fail("Could not write dishonored_vr.ini", err); return r; }
        r.add(StepStatus::Ok, "Wrote dishonored_vr.ini", "A byte copy of the settings this build was tuned and tested with; only the choices below differ.");
    } else if (det.iniVersion < det.embeddedIniVersion) {
        if (!refresh_outdated_ini(&r, det, p)) return r;
    } else {
        r.add(StepStatus::Skipped, "Kept your dishonored_vr.ini", "Your F10 settings stay; only the choices below are written.");
    }
    if (!apply_choices(&r, det, choices)) return r;
    apply_baseline(&r, det);
    write_install_record(&r, det, &choices);
    return r;
}

static Report update_impl(const Env& env, const Detection& det, bool overwriteSettings)
{
    (void)env;
    Report r;
    const Payload p = payload();
    if (!p.ok()) { r.add(StepStatus::Failed, "This installer is missing its payload", "Download it again."); return r; }
    if (!det.gameFound) { r.add(StepStatus::Failed, "Dishonored was not found", det.gameNote); return r; }
    if (game_running_blocks(&r, det.running)) return r;
    if (det.installedIsEmbedded()) r.add(StepStatus::Skipped, "The installed mod is already this build", "Reinstalling the same bytes anyway.");
    if (!write_payload_file(&r, det.gameDir, L"d3d9.dll", p.d3d9)) return r;
    if (!write_payload_file(&r, det.gameDir, L"dvr_steamvr32.dll", p.shim)) return r;
    if (!write_payload_file(&r, det.gameDir, L"openvr_api.dll", p.openvr)) return r;
    remove_if_present(&r, det.gameDir, L"dxvk_d3d9.dll", "The DXVK layer from releases before 41.0.");
    remove_if_present(&r, det.gameDir, L"dxvk_stereo.txt", "Its marker file.");
    if (overwriteSettings) {
        const std::wstring ini = fs::join(det.gameDir, kIniName);
        DWORD err = 0;
        if (det.iniExists) {
            const std::wstring backup = ini + L"." + fs::timestamp_local() + L"." + std::to_wstring(GetTickCount64()) + L".dvr-backup";
            if (!fs::copy_file(ini, backup, &err)) { r.fail("Could not back up your settings", err); return r; }
            r.add(StepStatus::Ok, "Backed up your INI and F10 settings", n(backup));
        }
        if (!fs::write_file_atomic(ini, p.ini.data, p.ini.size, &err)) { r.fail("Could not reset settings", err); return r; }
        // Fresh public defaults: auto runtime, Balanced, and no developer data path.
        if (!apply_choices(&r, det, Choices{})) return r;
        // apply_choices preserves a custom data directory for normal installs; a
        // requested reset must use the portable default even when one was saved.
        if (!profile::set(ini, L"Paths", L"DataDir", L"", &err)) { r.fail("Could not reset data folder", err); return r; }
        r.add(StepStatus::Ok, "Replaced INI and F10 settings with this build's defaults");
    } else if (det.iniExists && det.iniVersion < det.embeddedIniVersion) {
        // the refresh the mod would do at launch, with the runtime and size it would
        // have lost carried across from the old file (det.suggested read them)
        if (!refresh_outdated_ini(&r, det, p)) return r;
        Choices c = det.suggested;
        apply_choices(&r, det, c);
    } else if (det.iniExists) {
        r.add(StepStatus::Skipped, "Kept your dishonored_vr.ini", "Your settings stay as they are.");
    } else {
        DWORD err = 0;
        if (!fs::write_file_atomic(fs::join(det.gameDir, kIniName), p.ini.data, p.ini.size, &err)) { r.fail("Could not write dishonored_vr.ini", err); return r; }
        r.add(StepStatus::Ok, "Wrote dishonored_vr.ini", "There was none; this is the tested copy.");
        Choices c = det.suggested;
        apply_choices(&r, det, c);
    }
    const Choices defaults;
    write_install_record(&r, det, overwriteSettings ? &defaults : nullptr);
    return r;
}

Report do_update(const Env& env, const Detection& det, bool overwriteSettings) {
    Report result;
    if(!det.gameFound) {result.add(StepStatus::Failed,"Dishonored was not found",det.gameNote);return result;}
    if(game_running_blocks(&result,process::is_running(kGameExe)))return result;
    // Back up the complete update set before the first mutation. A later failure
    // restores earlier successful writes, not just the file that failed.
    struct Saved {std::wstring path,backup;bool existed=false;std::string hash;};
    std::vector<Saved> saved;
    const auto backupDir=fs::join(det.gameDir,L"dvr-update-backup-"+fs::timestamp_local()+L"-"+std::to_wstring(GetTickCount64()));
    DWORD err=0;
    if(!fs::make_dir(backupDir,&err)) {result.fail("Could not prepare the update backup",err);return result;}
    for(const wchar_t* name:{L"d3d9.dll",L"dvr_steamvr32.dll",L"openvr_api.dll",L"dishonored_vr.ini",L"dishonored_vr_install.json",L"dxvk_d3d9.dll",L"dxvk_stereo.txt"}) {
        Saved item;item.path=fs::join(det.gameDir,name);item.backup=fs::join(backupDir,name);item.existed=fs::is_file(item.path);
        if(item.existed && (!fs::sha256_file(item.path,&item.hash,&err) || !fs::copy_file(item.path,item.backup,&err))) {
            result.fail("Could not back up the installed version",err);return result;
        }
        saved.push_back(item);
    }
    result=update_impl(env,det,overwriteSettings);
    if(result.ok) {result.add(StepStatus::Ok,"Saved previous version",n(backupDir));return result;}
    bool restored=true;
    for(const auto& item:saved) {
        std::string now; const bool exists=fs::is_file(item.path);
        if(item.existed && exists && fs::sha256_file(item.path,&now,nullptr) && now==item.hash)continue;
        if(!item.existed) {if(exists && !fs::delete_file(item.path,&err))restored=false;continue;}
        std::vector<uint8_t> bytes;
        if(!fs::read_file(item.backup,&bytes,&err) || !fs::write_file_atomic(item.path,bytes.data(),bytes.size(),&err))restored=false;
    }
    result.add(restored?StepStatus::Warn:StepStatus::Failed,
               restored?"Update failed; restored the previous version":"Update failed; some files could not be restored",
               "The complete backup is in "+n(backupDir));
    return result;
}

Report do_change(const Env& env, const Detection& det, const Choices& choices)
{
    (void)env;
    Report r;
    if (!det.gameFound) { r.add(StepStatus::Failed, "Dishonored was not found", det.gameNote); return r; }
    if (!det.iniExists) { r.add(StepStatus::Failed, "There is no dishonored_vr.ini to change", "Install the mod first."); return r; }
    if (det.running == process::Running::Yes)
        r.add(StepStatus::Warn, "Dishonored is running", "The new values apply at its next launch.");
    if (det.iniVersion < det.embeddedIniVersion && !refresh_outdated_ini(&r, det, payload())) return r;
    if (!apply_choices(&r, det, choices)) return r;
    if (det.configExists) apply_baseline(&r, det);
    write_install_record(&r, det, &choices);
    return r;
}

Report do_baseline(const Env& env, const Detection& det)
{
    (void)env;
    Report r;
    if (det.running == process::Running::Yes) { r.add(StepStatus::Failed, "Dishonored is running", "It rewrites its settings on exit; quit it first."); return r; }
    apply_baseline(&r, det);
    return r;
}

Report do_disable(const Env& env, const Detection& det, bool disabled)
{
    (void)env;
    Report r;
    if (!det.gameFound) { r.add(StepStatus::Failed, "Dishonored was not found", det.gameNote); return r; }
    const std::wstring marker = fs::join(det.gameDir, kDisableName);
    DWORD err = 0;
    if (disabled) {
        const char* text = "The mod is disabled while this file exists. Delete it, or use Enable VR in DishonoredVR-Launcher.exe.\r\n";
        if (!fs::write_file_atomic(marker, text, strlen(text), &err)) { r.fail("Could not write disable_vr.txt", err); return r; }
        r.add(StepStatus::Ok, "VR disabled", "disable_vr.txt is beside the game: the mod loads and does nothing, so Dishonored runs flat until you enable it again.");
    } else {
        if (!fs::delete_file(marker, &err)) { r.fail("Could not remove disable_vr.txt", err); return r; }
        r.add(StepStatus::Ok, "VR enabled", "disable_vr.txt removed.");
    }
    return r;
}

Report do_uninstall(const Env& env, const Detection& det, bool deleteIni)
{
    (void)env;
    Report r;
    if (!det.gameFound) { r.add(StepStatus::Failed, "Dishonored was not found", det.gameNote); return r; }
    if (game_running_blocks(&r, det.running)) return r;
    DWORD err = 0;
    const std::wstring proxy = fs::join(det.gameDir, L"d3d9.dll");
    if (det.d3d9Present) {
        if (det.modInstalled) {
            if (!fs::delete_file(proxy, &err)) { r.fail("Could not remove d3d9.dll", err); return r; }
            r.add(StepStatus::Ok, "Removed d3d9.dll", "The mod.");
        } else {
            r.add(StepStatus::Skipped, "Left d3d9.dll in place", "Nothing beside it says it is this mod's, so it may belong to something else.");
        }
    }
    remove_if_present(&r, det.gameDir, L"dvr_steamvr32.dll", "The SteamVR shim.");
    remove_if_present(&r, det.gameDir, L"openvr_api.dll", "Valve's OpenVR loader, installed with the shim.");
    remove_if_present(&r, det.gameDir, L"dxvk_d3d9.dll", "The DXVK layer from releases before 41.0.");
    remove_if_present(&r, det.gameDir, L"dxvk_stereo.txt", "Its marker file.");
    remove_if_present(&r, det.gameDir, kDisableName, "The kill switch.");
    remove_if_present(&r, det.gameDir, kRecordName, "The install record.");
    if (det.backupPresent) {
        if (!fs::move_file(fs::join(det.gameDir, kBackupName), proxy, &err)) { r.fail("Could not restore the original d3d9.dll", err); return r; }
        r.add(StepStatus::Ok, "Restored the original d3d9.dll", "From d3d9.dll.dvr-backup.");
    }
    if (deleteIni) {
        remove_if_present(&r, det.gameDir, kIniName, "Your settings, as asked.");
        remove_if_present(&r, det.gameDir, L"dishonored_vr_launch.txt", "The render-size mirror.");
    } else if (det.iniExists) {
        r.add(StepStatus::Skipped, "Kept dishonored_vr.ini", "Your settings survive a reinstall. Delete the file to reset them.");
    }
    r.add(StepStatus::Skipped, "The game's own settings were not changed", "The four values the mod set (frame smoothing, depth of field, vsync, mouse smoothing) are harmless flat; the timestamped .dvr-backup files in the game's Config folder hold the originals.");
    return r;
}

std::string report_to_text(const Report& r)
{
    std::string t = fs::format("ok=%d accessDenied=%d baselineApplied=%d baselinePending=%d\n", r.ok, r.accessDenied, r.baselineApplied, r.baselinePending);
    for (const auto& s : r.steps) {
        std::string d = s.detail;
        for (auto& c : d) if (c == '\n') c = '\x1f';
        t += fs::format("%d\t%s\t%s\n", (int)s.status, s.title.c_str(), d.c_str());
    }
    return t;
}

bool report_from_text(const std::string& text, Report* out)
{
    *out = Report();
    size_t start = 0;
    bool header = true;
    while (start < text.size()) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        start = nl + 1;
        if (line.empty()) continue;
        if (header) {
            int ok = 0, ad = 0, ba = 0, bp = 0;
            if (sscanf_s(line.c_str(), "ok=%d accessDenied=%d baselineApplied=%d baselinePending=%d", &ok, &ad, &ba, &bp) != 4) return false;
            out->ok = ok != 0; out->accessDenied = ad != 0; out->baselineApplied = ba != 0; out->baselinePending = bp != 0;
            header = false;
            continue;
        }
        const size_t t1 = line.find('\t');
        const size_t t2 = t1 == std::string::npos ? std::string::npos : line.find('\t', t1 + 1);
        if (t1 == std::string::npos || t2 == std::string::npos) return false;
        StepResult s;
        s.status = (StepStatus)atoi(line.substr(0, t1).c_str());
        s.title = line.substr(t1 + 1, t2 - t1 - 1);
        s.detail = line.substr(t2 + 1);
        for (auto& c : s.detail) if (c == '\x1f') c = '\n';
        out->steps.push_back(s);
    }
    return !header;
}

} // namespace dvr::setup
