#include "core/util/paths.h"
#include <stdio.h>
#include <string.h>

namespace dvr::paths {
namespace {
char g_game[MAX_PATH] = "";
char g_data[MAX_PATH] = "";
char g_dumps[MAX_PATH] = "";
}

void init(HINSTANCE self)
{
    GetModuleFileNameA(self, g_game, MAX_PATH);
    char* slash = strrchr(g_game, '\\');
    if (slash) *slash = 0;
}

const char* game_dir() { return g_game; }

const char* data_dir()
{
    if (!g_data[0]) {
        if (!GetEnvironmentVariableA("DVR_DATA_DIR", g_data, MAX_PATH) || !g_data[0]) {
            char local[MAX_PATH] = "";
            if (GetEnvironmentVariableA("LOCALAPPDATA", local, MAX_PATH) && local[0])
                snprintf(g_data, MAX_PATH, "%s\\DishonoredVR", local);
            else
                snprintf(g_data, MAX_PATH, "%s\\DishonoredVR", g_game);
        }
        CreateDirectoryA(g_data, nullptr);
    }
    return g_data;
}

const char* dumps_dir()
{
    if (!g_dumps[0]) {
        snprintf(g_dumps, MAX_PATH, "%s\\dumps", data_dir());
        CreateDirectoryA(g_dumps, nullptr);
    }
    return g_dumps;
}

const char* in_game_dir(char* out, const char* name)
{
    snprintf(out, MAX_PATH, "%s\\%s", g_game, name);
    return out;
}

const char* in_data_dir(char* out, const char* name)
{
    snprintf(out, MAX_PATH, "%s\\%s", data_dir(), name);
    return out;
}

} // namespace dvr::paths
