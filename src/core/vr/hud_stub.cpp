// core/vr/hud_stub.cpp - see hud_stub.h. Every reader answers "nothing"; the
// setters remember their value so the debug panel's checkboxes round-trip.
#include "core/vr/hud_stub.h"

#include <atomic>
#include <cstring>

namespace dvr::hud {
namespace {
std::atomic<bool> g_enabled{false};
std::atomic<bool> g_barsHidden{true};
std::atomic<unsigned> g_barVerts{29};
std::atomic<bool> g_subsInFrame{false};
std::atomic<bool> g_effectsInFrame{false};
std::atomic<unsigned> g_effectMaxVerts{8};
std::atomic<bool> g_postfxRtOnly{true};
std::atomic<bool> g_postfxCineSize{false};
std::atomic<bool> g_restoreRt{true};
std::atomic<int> g_dumpEdge{0}, g_dumpCount{0};
} // namespace

ID3D11ShaderResourceView* srv(ID3D11DeviceContext*) { return nullptr; }
ID3D11Texture2D* texture(ID3D11DeviceContext*) { return nullptr; }
bool redirected_this_interval() { return false; }

void set_enabled(bool on) { g_enabled.store(on, std::memory_order_relaxed); }
bool enabled() { return g_enabled.load(std::memory_order_relaxed); }
void set_gate(bool) {}

void set_bars_hidden(bool on) { g_barsHidden.store(on, std::memory_order_relaxed); }
bool bars_hidden() { return g_barsHidden.load(std::memory_order_relaxed); }
void set_bar_verts(unsigned n) { g_barVerts.store(n, std::memory_order_relaxed); }
unsigned bar_verts() { return g_barVerts.load(std::memory_order_relaxed); }
void set_cine_subs_in_frame(bool on) { g_subsInFrame.store(on, std::memory_order_relaxed); }
bool cine_subs_in_frame() { return g_subsInFrame.load(std::memory_order_relaxed); }
void set_effects_in_frame(bool on) { g_effectsInFrame.store(on, std::memory_order_relaxed); }
bool effects_in_frame() { return g_effectsInFrame.load(std::memory_order_relaxed); }
void set_effect_max_verts(unsigned n) { g_effectMaxVerts.store(n, std::memory_order_relaxed); }
unsigned effect_max_verts() { return g_effectMaxVerts.load(std::memory_order_relaxed); }
void set_postfx_rt_only(bool on) { g_postfxRtOnly.store(on, std::memory_order_relaxed); }
bool postfx_rt_only() { return g_postfxRtOnly.load(std::memory_order_relaxed); }
void set_postfx_cine_size(bool on) { g_postfxCineSize.store(on, std::memory_order_relaxed); }
bool postfx_cine_size() { return g_postfxCineSize.load(std::memory_order_relaxed); }
void set_restore_rt(bool on) { g_restoreRt.store(on, std::memory_order_relaxed); }
bool restore_rt() { return g_restoreRt.load(std::memory_order_relaxed); }
void get_route_stats(RouteStats* out) { if (out) memset(out, 0, sizeof(*out)); }
bool bar_draw_active() { return false; }
void get_bar_stats(unsigned* skipped, unsigned* intervalsWithBars, unsigned* lastVertexCount) {
    if (skipped) *skipped = 0;
    if (intervalsWithBars) *intervalsWithBars = 0;
    if (lastVertexCount) *lastVertexCount = 0;
}
void set_dump_on_edge(int edge, int count) {
    g_dumpEdge.store(edge, std::memory_order_relaxed);
    g_dumpCount.store(count, std::memory_order_relaxed);
}
void get_dump_on_edge(int* edge, int* count) {
    if (edge) *edge = g_dumpEdge.load(std::memory_order_relaxed);
    if (count) *count = g_dumpCount.load(std::memory_order_relaxed);
}

bool fov_watch(float* tanH, float* tanV, unsigned long long* ageMs, unsigned long long) {
    if (tanH) *tanH = 0.0f;
    if (tanV) *tanV = 0.0f;
    if (ageMs) *ageMs = 0;
    return false;
}
bool fov_mismatch() { return false; }
bool screen_only() { return false; }
bool letterbox(unsigned* topPx, unsigned* botPx) {
    if (topPx) *topPx = 0;
    if (botPx) *botPx = 0;
    return false;
}

} // namespace dvr::hud
