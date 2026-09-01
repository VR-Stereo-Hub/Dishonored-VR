// core/framework/status.h - status.json: one machine-readable snapshot of the
// mod's state, written atomically to <data_dir>\status.json once a second and
// on the `status` command. tools\status-dump.ps1 reads it; the harness
// asserts on it. The fields come from a provider the game layer registers
// (game/dishonored/commands.cpp), so the file and the log heartbeat draw from
// the same numbers.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace dvr::status {

// A tiny JSON builder over a fixed buffer. No allocation, no library.
class Writer {
public:
    void begin();                       // {
    void end();                         // }
    void obj(const char* key);          // "key": {
    void end_obj();                     // }
    void arr(const char* key);          // "key": [
    void end_arr();                     // ]
    void kv(const char* key, int v);
    void kv(const char* key, unsigned long v);
    void kv(const char* key, double v);
    void kv(const char* key, bool v);
    void kv(const char* key, const char* v);
    void item(double v);                // array element
    void item(const char* v);
    const char* text() const { return buf_; }
    size_t length() const { return len_; }
private:
    void sep();
    void key(const char* k);
    void put(const char* s);
    void str(const char* s);
    char   buf_[16384] = {};
    size_t len_ = 0;
    bool   first_ = true;
};

typedef void (*Provider)(Writer& w);
void set_provider(Provider p);
void tick(double nowMs);      // 1 Hz write
void write_now();
const char* path();

} // namespace dvr::status
