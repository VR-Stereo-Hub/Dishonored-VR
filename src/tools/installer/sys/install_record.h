// tools/installer/sys/install_record.h - dishonored_vr_install.json beside the
// exe: what the launcher put there and when, so a re-run can say "Installed
// 41.0.0 (build 702-gabc1234)" and an update can tell same-bytes from newer.
// d3d9.dll carries no version resource; the sha256 is the identity, the same
// one tools/archive-symbols.ps1 keys the symbol archive on.
#pragma once
#include <windows.h>
#include <string>

namespace dvr::setup {

struct InstallRecord {
    bool valid = false;
    std::string version;        // DVR_VERSION
    std::string buildId;        // DVR_BUILD_ID (git describe)
    std::string config;         // DVR_BUILD_CONFIG
    std::string dllSha256;
    std::string installedUtc;
    std::string runtime;        // vdxr | steamvr | auto
    std::string quality;        // performance | balanced | quality | custom
    int width = 0, height = 0;
    bool elevated = false;
};

constexpr const wchar_t* kRecordName = L"dishonored_vr_install.json";

bool read_record(const std::wstring& gameDir, InstallRecord* out);
bool write_record(const std::wstring& gameDir, const InstallRecord& r, DWORD* err);
std::string record_to_json(const InstallRecord& r);
bool record_from_json(const std::string& json, InstallRecord* out);

} // namespace dvr::setup
