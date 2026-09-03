// core/gfx/stereo.cpp - the stereo-strategy registry and the frame path's
// entry points (see stereo.h). Present thread only.
#define DVR_CAT ::dvr::log::Cat::present
#include "core/gfx/stereo.h"

#include "core/framework/status.h"
#include "core/util/log.h"
#include "core/vr/openxr_runtime.h"

#include <windows.h>
#include <string.h>

namespace dvr::stereo {
namespace {

constexpr int kMaxMethods = 4;
IStereo*     g_methods[kMaxMethods] = {};
int          g_methodCount = 0;
IStereo*     g_active = nullptr;
bool         g_registered = false;
FrameOutput  g_last;
uint32_t     g_framesOut = 0;
OverlayDrawFn g_overlay = nullptr;

// The beat: one line every 3 s naming the method and what it produced, so a
// log can say which eyes flowed and at what rate. The counters here are
// populations, not evidence on their own: a mono method produces 0 left and 0
// right BY DESIGN, and the line says so.
uint64_t g_beatMs = 0;
uint32_t g_beatOut = 0, g_beatL = 0, g_beatR = 0, g_beatMono = 0, g_beatNone = 0;
int      g_projOverride = -1;   // -1 auto, 0 off, 1 on
char     g_configMethod[16] = "";   // [Stereo] Method, applied once the game side is up

} // namespace

void register_method(IStereo* m) {
    if (!m || g_methodCount >= kMaxMethods) return;
    for (int i = 0; i < g_methodCount; ++i)
        if (!_stricmp(g_methods[i]->name(), m->name())) return;
    g_methods[g_methodCount++] = m;
    if (!g_active && m->implemented()) g_active = m;
    DVR_INFO("stereo: method '%s' registered%s%s", m->name(),
             m->implemented() ? "" : " (design stub - not implemented)",
             g_active == m ? " - default" : "");
}

void register_all() {
    if (g_registered) return;
    g_registered = true;
    register_method(create_mono_screen());
    register_method(create_aer());
    register_method(create_reentry());
}

bool select(const char* name) {
    if (!name || !name[0]) {
        DVR_WARN("stereo: select needs a name (mono|aer|reentry); staying on '%s'", active_name());
        return false;
    }
    IStereo* found = nullptr;
    for (int i = 0; i < g_methodCount; ++i)
        if (!_stricmp(g_methods[i]->name(), name)) found = g_methods[i];
    if (!found) {
        char list[64] = "";
        for (int i = 0; i < g_methodCount; ++i) {
            strncat(list, i ? " " : "", sizeof(list) - strlen(list) - 1);
            strncat(list, g_methods[i]->name(), sizeof(list) - strlen(list) - 1);
        }
        DVR_WARN("stereo: no method named '%s' (registered: %s); staying on '%s'", name,
                 g_methodCount ? list : "none", active_name());
        return false;
    }
    if (!found->implemented()) {
        DVR_WARN("stereo: '%s' is a design stub, not implemented - staying on '%s'. %s",
                 found->name(), active_name(), found->note());
        return false;
    }
    if (found == g_active) {
        DVR_DEBUG("stereo: '%s' is already the active method", found->name());
        return true;
    }
    IStereo* prev = g_active;
    if (prev) prev->shutdown();
    g_active = found;
    g_last = FrameOutput{};
    DVR_INFO("stereo: method %s -> %s (live; the next present uses it)",
             prev ? prev->name() : "none", found->name());
    return true;
}

void set_config_method(const char* name) {
    strncpy(g_configMethod, name ? name : "", sizeof(g_configMethod) - 1);
    g_configMethod[sizeof(g_configMethod) - 1] = 0;
}

void apply_config_method() {
    if (!g_configMethod[0]) return;
    const bool ok = select(g_configMethod);
    if (!ok && !g_active) select("mono");
    DVR_INFO("stereo: [Stereo] Method=%s applied after the game side registered -> active '%s'%s",
             g_configMethod, active_name(), ok ? "" : " (refused above; the mono screen runs)");
}

IStereo* active() { return g_active; }
const char* active_name() { return g_active ? g_active->name() : "none"; }

void begin_frame(const FrameInput& in) {
    if (g_active) g_active->begin_frame(in);
}

bool end_frame(const FrameDevices& d, FrameOutput& out) {
    out = FrameOutput{};
    bool ok = g_active && g_active->end_frame(d, out);
    if (!ok) out = FrameOutput{};
    g_last = out;
    if (out.tex) {
        ++g_framesOut;
        ++g_beatOut;
        if (out.eyeSign < 0) ++g_beatL;
        else if (out.eyeSign > 0) ++g_beatR;
        else ++g_beatMono;
    } else {
        ++g_beatNone;
    }
    const uint64_t now = GetTickCount64();
    if (g_beatMs == 0) g_beatMs = now;
    else if (now - g_beatMs >= 3000) {
        const double s = (double)(now - g_beatMs) / 1000.0;
        DVR_INFO("stereo: beat method=%s out/s=%.0f L/s=%.0f R/s=%.0f mono/s=%.0f none/s=%.0f %ux%u"
                 "%s",
                 active_name(), g_beatOut / s, g_beatL / s, g_beatR / s, g_beatMono / s,
                 g_beatNone / s, out.w, out.h,
                 (g_beatL == 0 && g_beatR == 0 && g_beatMono > 0)
                     ? " (L/R read 0 by design on the mono screen)" : "");
        g_beatMs = now;
        g_beatOut = g_beatL = g_beatR = g_beatMono = g_beatNone = 0;
    }
    return ok;
}

void on_reset() {
    for (int i = 0; i < g_methodCount; ++i) g_methods[i]->on_reset();
    g_last = FrameOutput{};
}

void shutdown() {
    for (int i = 0; i < g_methodCount; ++i) g_methods[i]->shutdown();
    g_last = FrameOutput{};
}

const FrameOutput& last_output() { return g_last; }
uint32_t frames_out() { return g_framesOut; }

bool wants_projection() {
    if (g_projOverride >= 0) return g_projOverride == 1;
    return g_active && g_active->wants_projection();
}
void set_projection_override(int mode) {
    const int m = mode < 0 ? -1 : mode > 0 ? 1 : 0;
    if (m == g_projOverride) return;
    g_projOverride = m;
    DVR_INFO("stereo: projection %s (the frame path arms camera mode on the next present: %s)",
             projection_override_name(),
             wants_projection() ? "PROJECTION layer, per-eye poses, the FOV lever follows the frame aspect"
                                : "the head-locked quad screen, lever as configured");
}
const char* projection_override_name() {
    return g_projOverride < 0 ? "auto" : g_projOverride ? "on" : "off";
}

void status(dvr::status::Writer& w) {
    w.kv("method", active_name());
    w.kv("w", (int)g_last.w);
    w.kv("h", (int)g_last.h);
    w.kv("eyeSign", (int)g_last.eyeSign);
    w.kv("framesOut", (unsigned long)g_framesOut);
    w.kv("nextEye", g_active ? g_active->eye_for_next_frame() : 0);
    w.kv("projection", wants_projection());
    w.kv("projectionOverride", projection_override_name());
    w.kv("camMode", dvr::vr::vr_camera_mode());
    w.kv("cineActive", dvr::vr::cinematic_active());
    if (g_active) g_active->status(w);
}

void log_status() {
    DVR_INFO("stereo: method=%s framesOut=%lu last=%ux%u eyeSign=%d nextEye=%d projection=%s (%s) "
             "camMode=%d cineQuad=%d registered=%d",
             active_name(), (unsigned long)g_framesOut, g_last.w, g_last.h, g_last.eyeSign,
             g_active ? g_active->eye_for_next_frame() : 0, wants_projection() ? "yes" : "no",
             projection_override_name(), (int)dvr::vr::vr_camera_mode(), (int)dvr::vr::cinematic_active(),
             g_methodCount);
    for (int i = 0; i < g_methodCount; ++i)
        DVR_INFO("stereo:   %s%s%s", g_methods[i]->name(),
                 g_methods[i] == g_active ? " (active)" : "",
                 g_methods[i]->implemented() ? "" : " (design stub)");
}

void set_overlay_draw(OverlayDrawFn fn) { g_overlay = fn; }
OverlayDrawFn overlay_draw() { return g_overlay; }

} // namespace dvr::stereo
