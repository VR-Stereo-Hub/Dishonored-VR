#define DVR_CAT ::dvr::log::Cat::core
#include "core/framework/status.h"
#include "core/util/log.h"
#include "core/util/paths.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

namespace dvr::status {

void Writer::put(const char* s)
{
    size_t n = strlen(s);
    if (len_ + n >= sizeof(buf_) - 1) return;   // truncate silently; the file stays valid up to here
    memcpy(buf_ + len_, s, n);
    len_ += n;
    buf_[len_] = 0;
}

void Writer::str(const char* s)
{
    put("\"");
    char esc[8];
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { esc[0] = '\\'; esc[1] = (char)c; esc[2] = 0; put(esc); }
        else if (c < 0x20) { snprintf(esc, sizeof(esc), "\\u%04x", c); put(esc); }
        else { esc[0] = (char)c; esc[1] = 0; put(esc); }
    }
    put("\"");
}

void Writer::sep() { if (!first_) put(","); first_ = false; }
void Writer::key(const char* k) { sep(); str(k); put(":"); }
void Writer::begin() { len_ = 0; buf_[0] = 0; first_ = true; put("{"); }
void Writer::end() { put("}\n"); }
void Writer::obj(const char* k) { key(k); put("{"); first_ = true; }
void Writer::end_obj() { put("}"); first_ = false; }
void Writer::arr(const char* k) { key(k); put("["); first_ = true; }
void Writer::end_arr() { put("]"); first_ = false; }
void Writer::kv(const char* k, int v) { char t[32]; snprintf(t, sizeof(t), "%d", v); key(k); put(t); }
void Writer::kv(const char* k, unsigned long v) { char t[32]; snprintf(t, sizeof(t), "%lu", v); key(k); put(t); }
void Writer::kv(const char* k, double v)
{
    char t[48];
    if (v != v || v > 1e300 || v < -1e300) snprintf(t, sizeof(t), "null");
    else snprintf(t, sizeof(t), "%.4f", v);
    key(k); put(t);
}
void Writer::kv(const char* k, bool v) { key(k); put(v ? "true" : "false"); }
void Writer::kv(const char* k, const char* v) { key(k); if (v) str(v); else put("null"); }
void Writer::item(double v) { char t[48]; snprintf(t, sizeof(t), "%.4f", v); sep(); put(t); }
void Writer::item(const char* v) { sep(); str(v); }

namespace {
Provider g_provider = nullptr;
double   g_lastMs = 0.0;
char     g_path[MAX_PATH] = "";
Writer   g_writer;
}

void set_provider(Provider p) { g_provider = p; }

const char* path()
{
    if (!g_path[0]) dvr::paths::in_data_dir(g_path, "status.json");
    return g_path;
}

void write_now()
{
    if (!g_provider) return;
    g_writer.begin();
    g_provider(g_writer);
    g_writer.end();
    char tmp[MAX_PATH];
    dvr::paths::in_data_dir(tmp, "status.json.tmp");
    FILE* f = fopen(tmp, "wb");
    if (!f) return;
    fwrite(g_writer.text(), 1, g_writer.length(), f);
    fclose(f);
    MoveFileExA(tmp, path(), MOVEFILE_REPLACE_EXISTING);   // readers never see a torn file
}

void tick(double nowMs)
{
    if (nowMs - g_lastMs < 1000.0) return;
    g_lastMs = nowMs;
    write_now();
}

} // namespace dvr::status
