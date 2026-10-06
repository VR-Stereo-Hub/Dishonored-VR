// The embedded data-file install policy (core/util/embedded_assets.cpp), on the host with a real RCDATA
// resource: a fresh install writes the file, a second start leaves it, an older copy this mod wrote is
// updated, and a file the player put there is kept. Never touches the game or the real data directory.
#include "core/util/embedded_assets.cpp"
#include <windows.h>
#include <string>
static int fails = 0, checks = 0;
static void check(bool ok, const char* what) { ++checks; if (!ok) { ++fails; printf("FAIL %s\n", what); } }
static std::string slurp(const char* p) {
    FILE* f = nullptr; if (fopen_s(&f, p, "rb") || !f) return ""; std::string s; char b[4096]; size_t n;
    while ((n = fread(b, 1, sizeof b, f)) > 0) s.append(b, n); fclose(f); return s; }
static void put(const char* p, const std::string& s) { FILE* f = nullptr; fopen_s(&f, p, "wb"); fwrite(s.data(), 1, s.size(), f); fclose(f); }
int main() {
    CreateDirectoryA("data", nullptr);
    const char* file = ".\data\dishonored_vr_arm_rig.bin";
    const char* man = ".\data\dishonored_vr_assets.manifest";
    DeleteFileA(file); DeleteFileA(man);
    const std::string embedded = slurp("payload.bin");
    check(!embedded.empty(), "the test payload exists");
    dvr::assets::install();
    check(slurp(file) == embedded, "a fresh data directory gets the embedded file");
    check(slurp(man).find("dishonored_vr_arm_rig.bin=") == 0, "the manifest records what was written");
    const std::string man1 = slurp(man);
    dvr::assets::install();
    check(slurp(file) == embedded && slurp(man) == man1, "a second start changes nothing");
    // an older copy this mod wrote: the file and the manifest agree on its hash
    const std::string older = "older build's copy";
    put(file, older);
    char line[128]; _snprintf_s(line, _TRUNCATE, "dishonored_vr_arm_rig.bin=%016llx\n",
        (unsigned long long)dvr::assets::fnv64((const uint8_t*)older.data(), older.size()));
    put(man, line);
    dvr::assets::install();
    check(slurp(file) == embedded, "an older copy this mod wrote is updated");
    // a player's own file: not what the manifest says this mod wrote
    const std::string mine = "a player's replacement rig";
    put(file, mine);
    dvr::assets::install();
    check(slurp(file) == mine, "a file the player replaced is kept");
    // negative control: with the manifest claiming the player's file, the policy would overwrite it
    _snprintf_s(line, _TRUNCATE, "dishonored_vr_arm_rig.bin=%016llx\n",
        (unsigned long long)dvr::assets::fnv64((const uint8_t*)mine.data(), mine.size()));
    put(man, line);
    dvr::assets::install();
    check(slurp(file) == embedded, "control: the same file recorded as ours is replaced");
    printf("embedded assets: %d checks, %d failures\n", checks, fails);
    return fails ? 1 : 0;
}
