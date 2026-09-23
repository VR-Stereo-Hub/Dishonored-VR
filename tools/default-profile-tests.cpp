#include <windows.h>
#include <cstdio>
#include <cstring>
static const int kConfigVersion=15;
#include "default_profile_body.inc"
static bool equalKey(const char* section, const char* key, const char* expected, const char* path) {
    char value[1024]; GetPrivateProfileStringA(section, key, "MISSING", value, sizeof(value), path);
    return !strcmp(value, expected);
}
int main() {
    if (!WriteDefaultIni("actual.ini")) return 1;
    char cwd[MAX_PATH], fixture[MAX_PATH], backup[MAX_PATH], temp[MAX_PATH];
    GetCurrentDirectoryA(sizeof(cwd), cwd);
    _snprintf(fixture, sizeof(fixture), "%s\\reset-test.ini", cwd);
    _snprintf(backup, sizeof(backup), "%s.pre-reset", fixture);
    _snprintf(temp, sizeof(temp), "%s.reset-tmp", fixture);
    if (!WriteDefaultIni(fixture)) return 2;
    WritePrivateProfileStringA("VR", "Runtime", "steamvr", fixture);
    WritePrivateProfileStringA("VR", "XrRuntimeJson", "C:\\test runtime\\runtime.json", fixture);
    WritePrivateProfileStringA("Paths", "DataDir", "D:\\test data", fixture);
    WritePrivateProfileStringA("Rain", "Hide", "1", fixture);
    WritePrivateProfileStringA("Meta", "ResetDefaults", "1", fixture);
    if (!ConfigRestoreDefaults(fixture) || !equalKey("Rain", "Hide", "0", fixture) ||
        !equalKey("VR", "Runtime", "steamvr", fixture) ||
        !equalKey("VR", "XrRuntimeJson", "C:\\test runtime\\runtime.json", fixture) ||
        !equalKey("Paths", "DataDir", "D:\\test data", fixture) ||
        !equalKey("Meta", "ResetDefaults", "MISSING", fixture) ||
        !equalKey("Rain", "Hide", "1", backup)) return 3;
    // A failed backup must not reset any settings.
    WritePrivateProfileStringA("Rain", "Hide", "1", fixture);
    SetFileAttributesA(backup, FILE_ATTRIBUTE_READONLY);
    const bool refused = !ConfigRestoreDefaults(fixture);
    SetFileAttributesA(backup, FILE_ATTRIBUTE_NORMAL);
    if (!refused || !equalKey("Rain", "Hide", "1", fixture)) return 4;
    // A failed staging write also leaves the installed profile intact.
    CreateDirectoryA(temp, nullptr);
    const bool refusedTemp = !ConfigRestoreDefaults(fixture);
    RemoveDirectoryA(temp);
    if (!refusedTemp || !equalKey("Rain", "Hide", "1", fixture)) return 5;
    DeleteFileA(fixture); DeleteFileA(backup);
    puts("Default writer and reset persistence/failure checks passed.");
    return 0;
}
