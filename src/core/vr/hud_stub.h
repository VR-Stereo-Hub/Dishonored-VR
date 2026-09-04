// core/vr/hud_stub.h - the HUD-capture surface the runtime layer was written
// against, as inert stand-ins. The BioShock runtime layer (openxr_runtime.cpp,
// adopted in 41.0) grew up next to a gameswf HUD-capture module and reads ~30
// of its knobs for the cinematic fallback and the debug panel. Dishonored's
// HUD is Scaleform and nothing captures it yet (ROADMAP S3: the HUD comes
// back on the winning stereo method through the runtime's texture-provider
// seam, set_hud_texture_provider), so every reader here answers "nothing":
// no texture, no letterbox, no fov watch, no gameswf routing. Keeping the
// runtime file verbatim behind a stub is deliberate: it is a 5k-line proven
// layer, and every divergence from the BioShock copy is a place a fix stops
// porting cleanly.
#pragma once
#include <d3d11.h>

namespace dvr::hud {

enum RoutePass {
    kRouteTonemap, kRouteNotHud, kRouteBarsShown, kRouteEffect, kRouteCineSubs,
    kRoutePostFx, kRouteUnarmed, kRoutePassCount
};
struct RouteStats {
    unsigned pass[kRoutePassCount];
    unsigned stranded[kRoutePassCount];
    unsigned restored;
    unsigned postFx;
    unsigned postFxRejected;
    unsigned effectsInFrame;
    unsigned effectsRejected;
    bool squareTarget;
};

ID3D11ShaderResourceView* srv(ID3D11DeviceContext* ctx);   // always null
ID3D11Texture2D* texture(ID3D11DeviceContext* ctx);        // always null
bool redirected_this_interval();                            // always false

// The runtime layer's own gate, remembered rather than discarded (41.2): true
// while a projection-mode present carries a SequentialReentry eye tag, i.e.
// while stereo gameplay frames are flowing. Menus, loading screens and the
// cinematic quad all drop it, because they drop the projection or the tag.
bool gate();

void set_enabled(bool on);
bool enabled();
void set_gate(bool stereoActive);

void set_bars_hidden(bool on);
bool bars_hidden();
void set_bar_verts(unsigned n);
unsigned bar_verts();
void set_cine_subs_in_frame(bool on);
bool cine_subs_in_frame();
void set_effects_in_frame(bool on);
bool effects_in_frame();
void set_effect_max_verts(unsigned n);
unsigned effect_max_verts();
void set_postfx_rt_only(bool on);
bool postfx_rt_only();
void set_postfx_cine_size(bool on);
bool postfx_cine_size();
void set_restore_rt(bool on);
bool restore_rt();
void get_route_stats(RouteStats* out);
bool bar_draw_active();
void get_bar_stats(unsigned* skipped, unsigned* intervalsWithBars, unsigned* lastVertexCount);
void set_dump_on_edge(int edge, int count);
void get_dump_on_edge(int* edge, int* count);

bool fov_watch(float* tanH, float* tanV, unsigned long long* ageMs,
               unsigned long long maxAgeMs = 500);
bool fov_mismatch();
bool screen_only();
bool letterbox(unsigned* topPx, unsigned* botPx);

} // namespace dvr::hud
