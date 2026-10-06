// core/util/embedded_assets.cpp - see embedded_assets.h.
#include "core/util/embedded_assets.h"
#include "core/util/log.h"
#include "core/util/paths.h"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#define DVR_CAT ::dvr::log::Cat::core

#ifndef DVR_EMBEDDED_ASSET_MASK
#define DVR_EMBEDDED_ASSET_MASK 0
#endif

namespace dvr::assets {
uint64_t fnv64(const uint8_t* p, size_t n);   // exposed for the host test
namespace {
// Resource ids 9101.. in this order (src/CMakeLists.txt writes the .rc in the same order).
const char* const kNames[] = { "dishonored_vr_arm_rig.bin", "dishonored_vr_heart_rig.bin",
                               "dishonored_vr_heart_back.bin", "dishonored_vr_heart_material.bin" };
constexpr int kCount = sizeof(kNames) / sizeof(kNames[0]);
constexpr const char* kManifest = "dishonored_vr_assets.manifest";   // name=hash of what this mod last wrote

}  // namespace
uint64_t fnv64(const uint8_t* p, size_t n) {
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}
namespace {
bool read_file(const char* path, std::vector<uint8_t>* out) {
    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") || !f) return false;
    fseek(f, 0, SEEK_END); const long n = ftell(f); fseek(f, 0, SEEK_SET);
    bool ok = n >= 0;
    if (ok) { out->resize((size_t)n); ok = n == 0 || fread(out->data(), 1, (size_t)n, f) == (size_t)n; }
    fclose(f);
    return ok;
}
// Through a temporary file and a rename, so a crash mid-write never leaves a torn file the loaders read.
bool write_file(const char* path, const uint8_t* p, size_t n) {
    std::string tmp = std::string(path) + ".tmp";
    FILE* f = nullptr;
    if (fopen_s(&f, tmp.c_str(), "wb") || !f) return false;
    const bool ok = fwrite(p, 1, n, f) == n;
    if (fclose(f) != 0 || !ok) { DeleteFileA(tmp.c_str()); return false; }
    if (!MoveFileExA(tmp.c_str(), path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) { DeleteFileA(tmp.c_str()); return false; }
    return true;
}
uint64_t manifest_hash(const std::string& text, const char* name) {
    const std::string key = std::string(name) + "=";
    size_t at = text.find(key);
    if (at == std::string::npos || (at > 0 && text[at - 1] != '\n')) return 0;
    return _strtoui64(text.c_str() + at + key.size(), nullptr, 16);
}
}  // namespace

void install() {
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&install, &self);
    if (DVR_EMBEDDED_ASSET_MASK == 0) {
        DVR_WARN("assets: this build carries NO embedded data files (built without assets/vr/) - full-arm IK and the "
                 "Heart's backing work only if their files are already in the data directory");
        return;
    }
    char mpath[MAX_PATH]; dvr::paths::in_data_dir(mpath, kManifest);
    std::vector<uint8_t> mbytes; read_file(mpath, &mbytes);
    const std::string manifest(mbytes.begin(), mbytes.end());
    std::string next;
    int written = 0, current = 0, kept = 0, failed = 0;
    for (int i = 0; i < kCount; ++i) {
        if (!(DVR_EMBEDDED_ASSET_MASK & (1 << i))) {
            DVR_WARN("assets: %s is not embedded in this build", kNames[i]); continue;
        }
        HRSRC r = self ? FindResourceA(self, MAKEINTRESOURCEA(9101 + i), MAKEINTRESOURCEA(10)) : nullptr;
        const DWORD n = r ? SizeofResource(self, r) : 0;
        const uint8_t* data = r ? (const uint8_t*)LockResource(LoadResource(self, r)) : nullptr;
        if (!data || !n) { ++failed; DVR_WARN("assets: %s is listed as embedded but the resource is missing", kNames[i]); continue; }
        const uint64_t want = fnv64(data, n);
        char path[MAX_PATH]; dvr::paths::in_data_dir(path, kNames[i]);
        std::vector<uint8_t> have;
        const bool exists = read_file(path, &have);
        const uint64_t haveHash = exists ? fnv64(have.data(), have.size()) : 0;
        char line[160];
        if (exists && haveHash == want && have.size() == n) {
            ++current;
            _snprintf_s(line, _TRUNCATE, "%s=%016llx\n", kNames[i], (unsigned long long)want); next += line;
            continue;
        }
        const uint64_t ours = manifest_hash(manifest, kNames[i]);
        if (exists && ours != haveHash) {
            // Not a file this mod wrote (or wrote and the player then replaced): a hand-made or modded file.
            ++kept;
            DVR_INFO("assets: %s KEPT - the file in the data directory (%u bytes) is not one this mod wrote, so it is "
                     "treated as the player's own; delete it to get the built-in copy (%lu bytes)", kNames[i],
                     (unsigned)have.size(), (unsigned long)n);
            if (ours) { _snprintf_s(line, _TRUNCATE, "%s=%016llx\n", kNames[i], (unsigned long long)ours); next += line; }
            continue;
        }
        if (write_file(path, data, n)) {
            ++written;
            DVR_INFO("assets: %s %s (%lu bytes) -> %s", kNames[i], exists ? "UPDATED to this build's copy" : "installed",
                     (unsigned long)n, path);
            _snprintf_s(line, _TRUNCATE, "%s=%016llx\n", kNames[i], (unsigned long long)want); next += line;
        } else {
            ++failed;
            DVR_WARN("assets: %s could NOT be written to %s (err %lu) - the feature that needs it falls back",
                     kNames[i], path, GetLastError());
        }
    }
    if (next != manifest) write_file(mpath, (const uint8_t*)next.data(), next.size());
    DVR_INFO("assets: %d installed or updated, %d already current, %d kept as the player's own, %d failed (data files for "
             "full-arm IK and the Heart's backing)", written, current, kept, failed);
}
}  // namespace dvr::assets
