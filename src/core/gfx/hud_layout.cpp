// core/gfx/hud_layout.cpp - see hud_layout.h.
#define DVR_CAT ::dvr::log::Cat::hud
#include "core/gfx/hud_layout.h"

#include "core/framework/status.h"
#include "core/gfx/hud_capture.h"
#include "core/util/log.h"
#include "core/vr/hud_anchor.h"
#include "core/vr/openxr_runtime.h"

#include <d3d11.h>
#include <imgui.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

namespace dvr::hudlayout {
namespace {

const char* const kAnchorNames[4] = { "off", "frame", "window", "hand" };
const char* const kElementNames[ElCount] = {
    "all", "health", "mana", "equipment", "reticle", "subtitles", "prompt",
    "objective", "vignette", "menu" };

// The presets (section 4.6 of the redo brief): the window as the abandoned
// branch shipped it (never judged), the hand as 38.92 tuned it (0.22 m was the
// one HUD change the tester asked for), status elements on the hand, text on
// the window. The region rectangles start UNMEASURED (all zero): until the
// probe has measured them (or the ini names them) every draw is "all".
const ElementCfg kPresetElements[ElCount] = {
    // anchor        winX winY winS  handX handY handS  rect
    { AnchorWindow,  0,   0,   1,    0,    0,    1,     {0,0,0,0} },   // all
    { AnchorHand,    0,   0,   1,    0,    0,    1,     {0,0,0,0} },   // health
    { AnchorHand,    0,   0,   1,    0,    0,    1,     {0,0,0,0} },   // mana
    { AnchorHand,    0,   0,   1,    0,    0,    1,     {0,0,0,0} },   // equipment
    { AnchorWindow,  0,   0,   1,    0,    0,    1,     {0,0,0,0} },   // reticle
    { AnchorWindow,  0,   0,   1,    0,    0,    1,     {0,0,0,0} },   // subtitles
    { AnchorWindow,  0,   0,   1,    0,    0,    1,     {0,0,0,0} },   // prompt
    { AnchorWindow,  0,   0,   1,    0,    0,    1,     {0,0,0,0} },   // objective
    { AnchorWindow,  0,   0,   1,    0,    0,    1,     {0,0,0,0} },   // vignette
    { AnchorWindow,  0,   0,   1,    0,    0,    1,     {0,0,0,0} },   // menu
};
const WindowCfg kPresetWindow = { false, 1.30f, 1.25f, 0.0f, -0.10f, 0.0f };
const HandCfg   kPresetHand   = { 0, 0.0f, 0.0f, 0.0f, 0.06f, 0.22f, false, 0.0f };
// Pause, Note, Journal, Wheel, Store, MissionStats (dvr::mono::Context bits 3,4,5,6,7,8).
const uint32_t  kPresetMenuMask = (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8);
const char* const kMenuContextNames[] = { "Pause", "Note", "Journal", "Wheel", "Store", "MissionStats" };
const unsigned    kMenuContextBits[]  = { 3, 4, 5, 6, 7, 8 };
const int         kMenuContexts = 6;

// VR-119: every alpha value at identity reproduces 41.2's picture exactly.
const char* const kAlphaModeNames[3] = { "repair", "captured", "mix" };
const AlphaCfg  kPresetAlpha = { AlphaRepair, 1.0f, 0.0f, 1.0f, 1.0f };
const Backdrop  kPresetBackdrop = { 0.0f, 0.0f, 0.0f, 0.0f };
const char* const kBackdropKindNames[2] = { "window", "hand" };

ElementCfg g_el[ElCount];
WindowCfg  g_win = kPresetWindow;
HandCfg    g_hand = kPresetHand;
AlphaCfg   g_alpha = kPresetAlpha;
Backdrop   g_backdrop[2] = { kPresetBackdrop, kPresetBackdrop };
bool       g_menuInWindow = true;
uint32_t   g_menuMask = kPresetMenuMask;
bool       g_menuRiding = false;
char       g_ini[MAX_PATH] = "";

// element -> sink, sink -> element. Present thread only (draws, the seam poll,
// the overlay's draw callback and the runtime's provider all run there);
// configure() runs at DllMain before any of them.
int g_elSink[ElCount];
int g_sinkEl[kMaxSinks];
uint32_t g_routeCounts[ElCount];    // draws routed per element this window
uint32_t g_routeNoRegion = 0;        // draws with no readable region -> "all"
uint32_t g_routeOverflow = 0;        // draws whose element had no free sink -> "all"
uint32_t g_routeFrame = 0;           // draws left in the frame (AnchorFrame)
unsigned long g_routeWinMs = 0;
char g_statusLine[512] = "not configured";

void write_key(const char* key, const char* value) {
    if (!g_ini[0]) return;
    WritePrivateProfileStringA("Hud", key, value, g_ini);
}
void write_f(const char* key, float v) { char b[32]; _snprintf(b, sizeof(b), "%.3f", v); b[31] = 0; write_key(key, b); }
void write_i(const char* key, int v)   { char b[32]; _snprintf(b, sizeof(b), "%d", v); b[31] = 0; write_key(key, b); }

float read_f(const char* ini, const char* key, float def) {
    char b[64] = "";
    GetPrivateProfileStringA("Hud", key, "", b, sizeof(b), ini);
    if (!b[0]) return def;
    return (float)atof(b);
}
int read_i(const char* ini, const char* key, int def) {
    return GetPrivateProfileIntA("Hud", key, def, ini);
}

void free_sink_of(int e) {
    const int s = g_elSink[e];
    if (s < 0) return;
    g_elSink[e] = -1;
    g_sinkEl[s] = -1;
}

int acquire_sink(int e) {
    if (g_elSink[e] >= 0) return g_elSink[e];
    for (int s = 0; s < kMaxSinks; ++s) {
        if (g_sinkEl[s] < 0) {
            g_sinkEl[s] = e; g_elSink[e] = s;
            DVR_INFO("hud/layout: element %s takes sink %d (anchor %s)", kElementNames[e], s,
                     kAnchorNames[g_el[e].anchor & 3]);
            return s;
        }
    }
    return -1;
}

// Every element with a visible or hidden anchor needs a sink; "all" always has
// one so an unreadable region has somewhere to go. Called after any anchor
// change and at configure().
void rebalance() {
    if (g_elSink[ElAll] < 0) acquire_sink(ElAll);
    for (int e = 0; e < ElCount; ++e) {
        if (g_el[e].anchor == AnchorFrame) free_sink_of(e);
    }
    // Every measured element gets a sink eagerly, so the first draw that
    // arrives is not lost to a late allocation. The menu element takes its
    // sink on the first riding draw instead: a sink in use costs a copy every
    // present (7.5 MB at the shipped size), and a menu is up for seconds.
    for (int e = 1; e < ElCount; ++e) {
        if (g_el[e].anchor == AnchorFrame || e == ElMenu) continue;
        const bool measured = g_el[e].rect[2] > g_el[e].rect[0] && g_el[e].rect[3] > g_el[e].rect[1];
        if (measured) acquire_sink(e);
    }
    if (!g_menuRiding) free_sink_of(ElMenu);
}

void refresh_status_line() {
    char* p = g_statusLine;
    size_t n = sizeof(g_statusLine);
    int w = _snprintf(p, n, "window %s %.2fm@%.2fm hand %c %.2fm | ",
                      g_win.worldLocked ? "world" : "view", g_win.widthM, g_win.distM,
                      g_hand.hand ? 'R' : 'L', g_hand.widthM);
    if (w < 0 || (size_t)w >= n) return;
    p += w; n -= w;
    for (int e = 0; e < ElCount; ++e) {
        const bool measured = g_el[e].rect[2] > g_el[e].rect[0];
        const char* why = "";
        if (g_el[e].anchor == AnchorFrame) why = "";
        else if (e != ElAll && e != ElMenu && !measured) why = "(no region: rides all)";
        else if (g_elSink[e] < 0 && g_el[e].anchor != AnchorFrame) why = "(no sink free: rides all)";
        else if (g_el[e].anchor == AnchorOff) why = "(hidden)";
        w = _snprintf(p, n, "%s=%s%s ", kElementNames[e], kAnchorNames[g_el[e].anchor & 3], why);
        if (w < 0 || (size_t)w >= n) break;
        p += w; n -= w;
    }
    g_statusLine[sizeof(g_statusLine) - 1] = 0;
}

} // namespace

// ---------------------------------------------------------------------------

const char* anchor_name(int a) { return kAnchorNames[a & 3]; }
int anchor_from_name(const char* s) {
    for (int i = 0; i < 4; ++i) if (!_stricmp(s, kAnchorNames[i])) return i;
    return -1;
}
const char* element_name(int e) { return (e >= 0 && e < ElCount) ? kElementNames[e] : "?"; }
int element_from_name(const char* s) {
    for (int i = 0; i < ElCount; ++i) if (!_stricmp(s, kElementNames[i])) return i;
    return -1;
}

const ElementCfg& element(int e) { return g_el[(e >= 0 && e < ElCount) ? e : 0]; }
const WindowCfg&  window() { return g_win; }
const HandCfg&    hand() { return g_hand; }

// ---- VR-119: the alpha and the backdrops ------------------------------------

const char* alpha_mode_name(int m) { return kAlphaModeNames[(m >= 0 && m < 3) ? m : 0]; }
int alpha_mode_from_name(const char* s) {
    for (int i = 0; i < 3; ++i) if (!_stricmp(s, kAlphaModeNames[i])) return i;
    return -1;
}
const AlphaCfg& alpha() { return g_alpha; }
void set_alpha(const AlphaCfg& a, const char* who) {
    AlphaCfg c = a;
    if (c.mode < 0 || c.mode > 2) c.mode = AlphaRepair;
    if (c.gain < 0.0f) c.gain = 0.0f;   if (c.gain > 4.0f) c.gain = 4.0f;
    if (c.floorA < 0.0f) c.floorA = 0.0f; if (c.floorA > 1.0f) c.floorA = 1.0f;
    if (c.gamma < 0.25f) c.gamma = 0.25f; if (c.gamma > 4.0f) c.gamma = 4.0f;
    if (c.mixK < 0.0f) c.mixK = 0.0f;   if (c.mixK > 4.0f) c.mixK = 4.0f;
    const bool changed = memcmp(&c, &g_alpha, sizeof(c)) != 0;
    g_alpha = c;
    if (changed)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
                         "hud/alpha: mode %s gain %.2f floor %.2f gamma %.2f mix %.2f (%s)%s",
                         kAlphaModeNames[c.mode], c.gain, c.floorA, c.gamma, c.mixK, who,
                         c.mode == AlphaRepair ? "" : " - the redirect forces the coverage equation on every HUD draw");
    write_key("AlphaMode", kAlphaModeNames[c.mode]);
    write_f("AlphaGain", c.gain);
    write_f("AlphaFloor", c.floorA);
    write_f("AlphaGamma", c.gamma);
    write_f("AlphaMix", c.mixK);
}
const Backdrop& backdrop(int kind) { return g_backdrop[kind ? 1 : 0]; }
void set_backdrop(int kind, const Backdrop& b, const char* who) {
    Backdrop c = b;
    float* v[4] = { &c.r, &c.g, &c.b, &c.a };
    for (float* f : v) { if (*f < 0.0f) *f = 0.0f; if (*f > 1.0f) *f = 1.0f; }
    kind = kind ? 1 : 0;
    const bool changed = memcmp(&c, &g_backdrop[kind], sizeof(c)) != 0;
    g_backdrop[kind] = c;
    if (changed)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
                         "hud/alpha: %s backdrop %.2f %.2f %.2f alpha %.2f (%s)", kBackdropKindNames[kind],
                         c.r, c.g, c.b, c.a, who);
    char key[64], val[96];
    _snprintf(key, sizeof(key), "Backdrop.%s", kBackdropKindNames[kind]); key[63] = 0;
    _snprintf(val, sizeof(val), "%.3f,%.3f,%.3f,%.3f", c.r, c.g, c.b, c.a); val[95] = 0;
    write_key(key, val);
}
void backdrop_for_sink(int sink, float rgba[4]) {
    const int e = sink_element(sink);
    const int kind = (e >= 0 && g_el[e].anchor == AnchorHand) ? 1 : 0;
    rgba[0] = g_backdrop[kind].r; rgba[1] = g_backdrop[kind].g; rgba[2] = g_backdrop[kind].b; rgba[3] = g_backdrop[kind].a;
}

void set_element_anchor(int e, int anchor, const char* who) {
    if (e < 0 || e >= ElCount) return;
    if (anchor < 0 || anchor > 3) return;
    if (e == ElMenu && anchor == AnchorHand) {
        DVR_WARN("hud/layout: the menu element cannot ride the hand (a menu is read, not glanced at); it stays on the window");
        anchor = AnchorWindow;
    }
    if (g_el[e].anchor != anchor)
        DVR_INFO("hud/layout: element %s anchor %s -> %s (%s)", kElementNames[e],
                 kAnchorNames[g_el[e].anchor & 3], kAnchorNames[anchor], who);
    g_el[e].anchor = anchor;
    rebalance();
    char key[64]; _snprintf(key, sizeof(key), "Element.%s", kElementNames[e]); key[63] = 0;
    write_key(key, kAnchorNames[anchor]);
    refresh_status_line();
}

void set_element_place(int e, bool onHand, float x, float y, float scale, const char* who) {
    if (e < 0 || e >= ElCount) return;
    if (scale < 0.1f) scale = 0.1f;
    if (scale > 4.0f) scale = 4.0f;
    if (onHand) { g_el[e].handX = x; g_el[e].handY = y; g_el[e].handScale = scale; }
    else        { g_el[e].winX = x;  g_el[e].winY = y;  g_el[e].winScale = scale; }
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
                     "hud/layout: element %s on the %s at (%.3f, %.3f) x%.2f (%s)",
                     kElementNames[e], onHand ? "hand" : "window", x, y, scale, who);
    char key[64];
    _snprintf(key, sizeof(key), "Element.%s.%sX", kElementNames[e], onHand ? "Hand" : "Win"); write_f(key, x);
    _snprintf(key, sizeof(key), "Element.%s.%sY", kElementNames[e], onHand ? "Hand" : "Win"); write_f(key, y);
    _snprintf(key, sizeof(key), "Element.%s.%sScale", kElementNames[e], onHand ? "Hand" : "Win"); write_f(key, scale);
}

void set_element_rect(int e, const float rect[4], const char* who) {
    if (e < 0 || e >= ElCount || !rect) return;
    memcpy(g_el[e].rect, rect, sizeof(g_el[e].rect));
    DVR_INFO("hud/layout: element %s region %.3f,%.3f-%.3f,%.3f (%s)", kElementNames[e],
             rect[0], rect[1], rect[2], rect[3], who);
    rebalance();
    char key[64], v[96];
    _snprintf(key, sizeof(key), "Region.%s", kElementNames[e]); key[63] = 0;
    _snprintf(v, sizeof(v), "%.3f,%.3f,%.3f,%.3f", rect[0], rect[1], rect[2], rect[3]); v[95] = 0;
    write_key(key, v);
    refresh_status_line();
}

void set_window(const WindowCfg& w, const char* who) {
    WindowCfg c = w;
    if (c.distM < 0.3f) c.distM = 0.3f;
    if (c.distM > 5.0f) c.distM = 5.0f;
    if (c.widthM < 0.2f) c.widthM = 0.2f;
    if (c.widthM > 4.0f) c.widthM = 4.0f;
    if (c.heightM < 0.0f) c.heightM = 0.0f;
    if (c.heightM > 4.0f) c.heightM = 4.0f;
    if (c.upM < -2.0f) c.upM = -2.0f;
    if (c.upM > 2.0f) c.upM = 2.0f;
    if (c.latM < -2.0f) c.latM = -2.0f;
    if (c.latM > 2.0f) c.latM = 2.0f;
    const bool changed = memcmp(&c, &g_win, sizeof(c)) != 0;
    g_win = c;
    if (changed)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
                         "hud/layout: window %s dist %.2f width %.2f height %.2f up %.2f lateral %.2f (%s)",
                         c.worldLocked ? "world-locked" : "head-locked", c.distM, c.widthM, c.heightM,
                         c.upM, c.latM, who);
    write_key("WindowAnchor", c.worldLocked ? "world" : "view");
    write_f("WindowDistance", c.distM);
    write_f("WindowWidth", c.widthM);
    write_f("WindowHeight", c.heightM);
    write_f("WindowUp", c.upM);
    write_f("WindowLateral", c.latM);
    refresh_status_line();
}

void set_hand(const HandCfg& h, const char* who) {
    HandCfg c = h;
    c.hand = c.hand ? 1 : 0;
    if (c.widthM < 0.05f) c.widthM = 0.05f;
    if (c.widthM > 0.60f) c.widthM = 0.60f;
    if (c.liftM < -0.3f) c.liftM = -0.3f;
    if (c.liftM > 0.3f) c.liftM = 0.3f;
    float* axes[3] = { &c.x, &c.y, &c.z };
    for (float* f : axes) { if (*f < -0.3f) *f = -0.3f; if (*f > 0.3f) *f = 0.3f; }
    if (c.tiltDeg < -90.0f) c.tiltDeg = -90.0f;
    if (c.tiltDeg > 90.0f) c.tiltDeg = 90.0f;
    const bool changed = memcmp(&c, &g_hand, sizeof(c)) != 0;
    g_hand = c;
    if (changed)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
                         "hud/layout: hand panel %c offset (%.3f %.3f %.3f) lift %.3f width %.2f %s tilt %.0f (%s)",
                         c.hand ? 'R' : 'L', c.x, c.y, c.z, c.liftM, c.widthM,
                         c.followGrip ? "follow-grip" : "billboard", c.tiltDeg, who);
    write_i("HandHand", c.hand);
    write_f("HandX", c.x); write_f("HandY", c.y); write_f("HandZ", c.z);
    write_f("HandLift", c.liftM);
    write_f("HandWidth", c.widthM);
    write_key("HandOrient", c.followGrip ? "grip" : "billboard");
    write_f("HandTilt", c.tiltDeg);
    refresh_status_line();
}

void reset_presets(const char* who) {
    DVR_INFO("hud/layout: presets restored (%s)", who);
    for (int e = 0; e < ElCount; ++e) {
        ElementCfg c = kPresetElements[e];
        memcpy(c.rect, g_el[e].rect, sizeof(c.rect));   // a measured region survives a reset
        g_el[e] = c;
        char key[64];
        _snprintf(key, sizeof(key), "Element.%s", kElementNames[e]); key[63] = 0;
        write_key(key, kAnchorNames[c.anchor]);
        set_element_place(e, false, c.winX, c.winY, c.winScale, who);
        set_element_place(e, true, c.handX, c.handY, c.handScale, who);
    }
    set_window(kPresetWindow, who);
    set_hand(kPresetHand, who);
    set_alpha(kPresetAlpha, who);
    set_backdrop(0, kPresetBackdrop, who);
    set_backdrop(1, kPresetBackdrop, who);
    set_menu_in_window(true, who);
    set_menu_context_mask(kPresetMenuMask, who);
    rebalance();
    refresh_status_line();
}

bool menu_in_window() { return g_menuInWindow; }
void set_menu_in_window(bool on, const char* who) {
    if (g_menuInWindow != on)
        DVR_INFO("hud/layout: in-game menus %s (%s)", on ? "ride the HUD window with the world in stereo behind"
                                                        : "take the mono screen, the old way", who);
    g_menuInWindow = on;
    write_key("MenuInWindow", on ? "1" : "0");
}
uint32_t menu_context_mask() { return g_menuMask; }
void set_menu_context_mask(uint32_t mask, const char* who) {
    if (mask != g_menuMask) DVR_INFO("hud/layout: menu contexts riding the window: 0x%x (%s)", mask, who);
    g_menuMask = mask;
    for (int i = 0; i < kMenuContexts; ++i) {
        char key[64]; _snprintf(key, sizeof(key), "Window%s", kMenuContextNames[i]); key[63] = 0;
        write_key(key, (mask & (1u << kMenuContextBits[i])) ? "1" : "0");
    }
}
void set_menu_riding(bool riding) {
    if (riding == g_menuRiding) return;
    g_menuRiding = riding;
    if (!riding) free_sink_of(ElMenu);   // the sink's copy stops with the ride
    DVR_INFO("hud/layout: %s", riding ? "a menu is riding the window: every HUD-class draw routes to the menu element"
                                      : "the menu left the window: routing by element again");
}
bool menu_riding() { return g_menuRiding; }

// ---- routing --------------------------------------------------------------

int sink_for(const float* bbox, int* elementOut) {
    int e = ElAll;
    if (g_menuRiding) {
        e = ElMenu;
    } else if (bbox) {
        const float cx = (bbox[0] + bbox[2]) * 0.5f, cy = (bbox[1] + bbox[3]) * 0.5f;
        const float w = bbox[2] - bbox[0], h = bbox[3] - bbox[1];
        if (w > 0.6f && h > 0.6f) {
            e = ElVignette;
        } else {
            for (int i = 1; i < ElCount; ++i) {
                if (i == ElMenu || i == ElVignette) continue;
                const float* r = g_el[i].rect;
                if (!(r[2] > r[0]) || !(r[3] > r[1])) continue;
                if (cx >= r[0] && cx <= r[2] && cy >= r[1] && cy <= r[3]) { e = i; break; }
            }
        }
    } else {
        ++g_routeNoRegion;
    }
    if (elementOut) *elementOut = e;
    if (e < ElCount) ++g_routeCounts[e];
    const int anchor = g_el[e].anchor;
    if (anchor == AnchorFrame) { ++g_routeFrame; return -1; }
    int s = g_elSink[e];
    if (s < 0) s = acquire_sink(e);
    if (s < 0) {                    // out of sinks: it rides "all"
        ++g_routeOverflow;
        s = g_elSink[ElAll];
        if (s < 0) s = acquire_sink(ElAll);
        if (elementOut) *elementOut = ElAll;
    }
    return s;
}

int sink_element(int sink) { return (sink >= 0 && sink < kMaxSinks) ? g_sinkEl[sink] : -1; }
int element_sink(int e) { return (e >= 0 && e < ElCount) ? g_elSink[e] : -1; }

// ---- the provider ---------------------------------------------------------

int provide(ID3D11DeviceContext* ctx, dvr::vr::HudQuadDesc* out, int max) {
    int n = 0;
    for (int s = 0; s < kMaxSinks && n < max; ++s) {
        const int e = g_sinkEl[s];
        if (e < 0) continue;
        const ElementCfg& c = g_el[e];
        if (c.anchor == AnchorOff || c.anchor == AnchorFrame) continue;
        ID3D11Texture2D* tex = dvr::hudcap::sink_texture(s, ctx);
        if (!tex) continue;
        dvr::vr::HudQuadDesc& d = out[n];
        d = dvr::vr::HudQuadDesc();
        d.tex = tex;
        d.element = e;
        // The element's own pixels: its region, or the whole texture for
        // "all" / "menu" / an unmeasured one.
        float rect[4] = {0, 0, 1, 1};
        const bool measured = c.rect[2] > c.rect[0] && c.rect[3] > c.rect[1];
        if (measured && e != ElAll && e != ElMenu) memcpy(rect, c.rect, sizeof(rect));
        memcpy(d.subrect, rect, sizeof(d.subrect));
        const float rw = rect[2] - rect[0];
        // The whole texture spans the anchor's width; an element's centre sits
        // at its normalised offset times that width (and the texture's aspect
        // for the vertical), which is "where it is on the screen".
        D3D11_TEXTURE2D_DESC td{};
        tex->GetDesc(&td);
        const float aspect = td.Width ? (float)td.Height / (float)td.Width : 1.0f;
        const float cxN = (rect[0] + rect[2]) * 0.5f - 0.5f;             // -0.5..0.5, + = right
        const float cyN = (0.5f - (rect[1] + rect[3]) * 0.5f) * aspect;  // + = up, in widths
        if (c.anchor == AnchorHand) {
            d.anchor = dvr::vr::HudAnchor::Hand;
            d.hand = g_hand.hand;
            d.base[0] = g_hand.x; d.base[1] = g_hand.y; d.base[2] = g_hand.z;
            d.lift = g_hand.liftM;
            d.orient = g_hand.followGrip ? dvr::vr::HudOrient::FollowGrip : dvr::vr::HudOrient::Billboard;
            d.tiltDeg = g_hand.tiltDeg;
            d.width = g_hand.widthM * rw * c.handScale;
            d.height = 0.0f;
            d.planeOff[0] = cxN * g_hand.widthM + c.handX;
            d.planeOff[1] = cyN * g_hand.widthM + c.handY;
        } else {
            d.anchor = g_win.worldLocked ? dvr::vr::HudAnchor::WindowWorld : dvr::vr::HudAnchor::Window;
            d.base[0] = g_win.latM; d.base[1] = g_win.upM; d.base[2] = -g_win.distM;
            d.width = g_win.widthM * rw * c.winScale;
            d.height = (e == ElAll || e == ElMenu) ? g_win.heightM : 0.0f;
            d.planeOff[0] = cxN * g_win.widthM + c.winX;
            d.planeOff[1] = cyN * g_win.widthM + c.winY;
        }
        ++n;
    }
    return n;
}

// ---- persistence ----------------------------------------------------------

void configure(const char* ini) {
    strncpy_s(g_ini, ini ? ini : "", _TRUNCATE);
    for (int e = 0; e < ElCount; ++e) g_elSink[e] = -1;
    for (int s = 0; s < kMaxSinks; ++s) g_sinkEl[s] = -1;
    for (int e = 0; e < ElCount; ++e) {
        ElementCfg c = kPresetElements[e];
        char key[64], v[96] = "";
        _snprintf(key, sizeof(key), "Element.%s", kElementNames[e]); key[63] = 0;
        GetPrivateProfileStringA("Hud", key, "", v, sizeof(v), ini);
        const int a = v[0] ? anchor_from_name(v) : -1;
        if (a >= 0) c.anchor = a;
        else if (v[0]) DVR_WARN("hud/layout: [Hud] %s=%s is not off|frame|window|hand; the preset (%s) stands",
                                key, v, kAnchorNames[c.anchor]);
        _snprintf(key, sizeof(key), "Element.%s.WinX", kElementNames[e]);      c.winX = read_f(ini, key, c.winX);
        _snprintf(key, sizeof(key), "Element.%s.WinY", kElementNames[e]);      c.winY = read_f(ini, key, c.winY);
        _snprintf(key, sizeof(key), "Element.%s.WinScale", kElementNames[e]);  c.winScale = read_f(ini, key, c.winScale);
        _snprintf(key, sizeof(key), "Element.%s.HandX", kElementNames[e]);     c.handX = read_f(ini, key, c.handX);
        _snprintf(key, sizeof(key), "Element.%s.HandY", kElementNames[e]);     c.handY = read_f(ini, key, c.handY);
        _snprintf(key, sizeof(key), "Element.%s.HandScale", kElementNames[e]); c.handScale = read_f(ini, key, c.handScale);
        _snprintf(key, sizeof(key), "Region.%s", kElementNames[e]); key[63] = 0;
        v[0] = 0;
        GetPrivateProfileStringA("Hud", key, "", v, sizeof(v), ini);
        if (v[0]) {
            float r[4] = {};
            if (sscanf(v, "%f,%f,%f,%f", &r[0], &r[1], &r[2], &r[3]) == 4 && r[2] > r[0] && r[3] > r[1])
                memcpy(c.rect, r, sizeof(c.rect));
            else
                DVR_WARN("hud/layout: [Hud] %s=%s is not x0,y0,x1,y1 with x1>x0 and y1>y0; the region stays unmeasured", key, v);
        }
        if (e == ElMenu && c.anchor == AnchorHand) c.anchor = AnchorWindow;
        g_el[e] = c;
    }
    WindowCfg w = kPresetWindow;
    { char v[16] = ""; GetPrivateProfileStringA("Hud", "WindowAnchor", "view", v, sizeof(v), ini); w.worldLocked = !_stricmp(v, "world"); }
    w.distM = read_f(ini, "WindowDistance", w.distM);
    w.widthM = read_f(ini, "WindowWidth", w.widthM);
    w.heightM = read_f(ini, "WindowHeight", w.heightM);
    w.upM = read_f(ini, "WindowUp", w.upM);
    w.latM = read_f(ini, "WindowLateral", w.latM);
    g_win = w;
    HandCfg h = kPresetHand;
    h.hand = read_i(ini, "HandHand", h.hand) ? 1 : 0;
    h.x = read_f(ini, "HandX", h.x); h.y = read_f(ini, "HandY", h.y); h.z = read_f(ini, "HandZ", h.z);
    h.liftM = read_f(ini, "HandLift", h.liftM);
    h.widthM = read_f(ini, "HandWidth", h.widthM);
    { char v[16] = ""; GetPrivateProfileStringA("Hud", "HandOrient", "billboard", v, sizeof(v), ini); h.followGrip = !_stricmp(v, "grip"); }
    h.tiltDeg = read_f(ini, "HandTilt", h.tiltDeg);
    g_hand = h;
    {   // VR-119
        AlphaCfg a = kPresetAlpha;
        char v[24] = "";
        GetPrivateProfileStringA("Hud", "AlphaMode", "repair", v, sizeof(v), ini);
        const int m = alpha_mode_from_name(v);
        if (m >= 0) a.mode = m;
        else DVR_WARN("hud/layout: [Hud] AlphaMode=%s is not repair|captured|mix; repair stands", v);
        a.gain = read_f(ini, "AlphaGain", a.gain);
        a.floorA = read_f(ini, "AlphaFloor", a.floorA);
        a.gamma = read_f(ini, "AlphaGamma", a.gamma);
        a.mixK = read_f(ini, "AlphaMix", a.mixK);
        g_alpha = a;
        for (int k = 0; k < 2; ++k) {
            char key[64], b[96] = "";
            _snprintf(key, sizeof(key), "Backdrop.%s", kBackdropKindNames[k]); key[63] = 0;
            GetPrivateProfileStringA("Hud", key, "", b, sizeof(b), ini);
            Backdrop d = kPresetBackdrop;
            if (b[0] && sscanf(b, "%f,%f,%f,%f", &d.r, &d.g, &d.b, &d.a) != 4) {
                DVR_WARN("hud/layout: [Hud] %s=%s is not r,g,b,a; no plate", key, b);
                d = kPresetBackdrop;
            }
            g_backdrop[k] = d;
        }
    }
    g_menuInWindow = read_i(ini, "MenuInWindow", 1) != 0;
    uint32_t mask = 0;
    for (int i = 0; i < kMenuContexts; ++i) {
        char key[64]; _snprintf(key, sizeof(key), "Window%s", kMenuContextNames[i]); key[63] = 0;
        if (read_i(ini, key, 1)) mask |= 1u << kMenuContextBits[i];
    }
    g_menuMask = mask;
    // The values, clamped, without rewriting the ini (configure reads; only a
    // change writes). set_window/set_hand would write; clamp by hand here.
    g_win = w; g_hand = h;
    rebalance();
    refresh_status_line();
    DVR_INFO("hud/layout: %s | menus in window=%d mask=0x%x", g_statusLine, (int)g_menuInWindow, g_menuMask);
}

void save(const char* ini) {
    if (!ini) return;
    char keep[MAX_PATH]; strncpy_s(keep, g_ini, _TRUNCATE);
    strncpy_s(g_ini, ini, _TRUNCATE);
    for (int e = 0; e < ElCount; ++e) {
        char key[64];
        _snprintf(key, sizeof(key), "Element.%s", kElementNames[e]); key[63] = 0;
        write_key(key, kAnchorNames[g_el[e].anchor & 3]);
        _snprintf(key, sizeof(key), "Element.%s.WinX", kElementNames[e]);      write_f(key, g_el[e].winX);
        _snprintf(key, sizeof(key), "Element.%s.WinY", kElementNames[e]);      write_f(key, g_el[e].winY);
        _snprintf(key, sizeof(key), "Element.%s.WinScale", kElementNames[e]);  write_f(key, g_el[e].winScale);
        _snprintf(key, sizeof(key), "Element.%s.HandX", kElementNames[e]);     write_f(key, g_el[e].handX);
        _snprintf(key, sizeof(key), "Element.%s.HandY", kElementNames[e]);     write_f(key, g_el[e].handY);
        _snprintf(key, sizeof(key), "Element.%s.HandScale", kElementNames[e]); write_f(key, g_el[e].handScale);
        if (g_el[e].rect[2] > g_el[e].rect[0]) {
            char v[96];
            _snprintf(key, sizeof(key), "Region.%s", kElementNames[e]); key[63] = 0;
            _snprintf(v, sizeof(v), "%.3f,%.3f,%.3f,%.3f", g_el[e].rect[0], g_el[e].rect[1], g_el[e].rect[2], g_el[e].rect[3]); v[95] = 0;
            write_key(key, v);
        }
    }
    write_key("WindowAnchor", g_win.worldLocked ? "world" : "view");
    write_f("WindowDistance", g_win.distM);
    write_f("WindowWidth", g_win.widthM);
    write_f("WindowHeight", g_win.heightM);
    write_f("WindowUp", g_win.upM);
    write_f("WindowLateral", g_win.latM);
    write_i("HandHand", g_hand.hand);
    write_f("HandX", g_hand.x); write_f("HandY", g_hand.y); write_f("HandZ", g_hand.z);
    write_f("HandLift", g_hand.liftM);
    write_f("HandWidth", g_hand.widthM);
    write_key("HandOrient", g_hand.followGrip ? "grip" : "billboard");
    write_f("HandTilt", g_hand.tiltDeg);
    set_alpha(g_alpha, "save");
    set_backdrop(0, g_backdrop[0], "save");
    set_backdrop(1, g_backdrop[1], "save");
    write_key("MenuInWindow", g_menuInWindow ? "1" : "0");
    set_menu_context_mask(g_menuMask, "save");
    strncpy_s(g_ini, keep, _TRUNCATE);
}

// ---- seam, status ---------------------------------------------------------

// `hud anchor <element|all> off|frame|window|hand`, `hud window dist|width|height|up|lateral <f>`,
// `hud window view|world|recenter`, `hud hand left|right|x|y|z|lift|width|tilt <f>|billboard|grip`,
// `hud place <element> window|hand <x> <y> [scale]`, `hud region <element> x0,y0,x1,y1`,
// `hud menu on|off`, `hud reset`, `hud layout`.
bool command(const char* args) {
    char w1[24] = "", w2[24] = "", w3[24] = "", w4[24] = "", w5[24] = "";
    const int n = sscanf(args, "%23s %23s %23s %23s %23s", w1, w2, w3, w4, w5);
    if (n < 1) return false;
    if (!strcmp(w1, "reset")) { reset_presets("the seam"); return true; }
    if (!strcmp(w1, "layout")) { log_status(); return true; }
    if (!strcmp(w1, "menu") && n >= 2) {
        if (!strcmp(w2, "on")) { set_menu_in_window(true, "the seam"); return true; }
        if (!strcmp(w2, "off")) { set_menu_in_window(false, "the seam"); return true; }
        // hud menu <Pause|Note|Journal|Store|MissionStats> on|off
        for (int i = 0; i < kMenuContexts && n >= 3; ++i) {
            if (_stricmp(w2, kMenuContextNames[i])) continue;
            const uint32_t bit = 1u << kMenuContextBits[i];
            set_menu_context_mask(!strcmp(w3, "on") ? (g_menuMask | bit) : (g_menuMask & ~bit), "the seam");
            return true;
        }
        DVR_WARN("hud: menu wants on|off or <Pause|Note|Journal|Wheel|Store|MissionStats> on|off");
        return true;
    }
    if (!strcmp(w1, "anchor") && n >= 3) {
        const int e = element_from_name(w2);
        const int a = anchor_from_name(w3);
        if (e < 0 || a < 0) { DVR_WARN("hud: anchor wants <element> off|frame|window|hand (elements: all health mana equipment reticle subtitles prompt objective vignette menu)"); return true; }
        set_element_anchor(e, a, "the seam");
        return true;
    }
    if (!strcmp(w1, "window") && n >= 2) {
        WindowCfg c = g_win;
        const float v = n >= 3 ? (float)atof(w3) : 0.0f;
        if (!strcmp(w2, "view")) c.worldLocked = false;
        else if (!strcmp(w2, "world")) c.worldLocked = true;
        else if (!strcmp(w2, "recenter")) { dvr::vr::recenter_hud_world_anchor(); DVR_INFO("hud: window re-seeded where the head is now"); return true; }
        else if (n < 3) { DVR_WARN("hud: window wants view|world|recenter or dist|width|height|up|lateral <m>"); return true; }
        else if (!strcmp(w2, "dist")) c.distM = v;
        else if (!strcmp(w2, "width")) c.widthM = v;
        else if (!strcmp(w2, "height")) c.heightM = v;
        else if (!strcmp(w2, "up")) c.upM = v;
        else if (!strcmp(w2, "lateral")) c.latM = v;
        else { DVR_WARN("hud: window wants view|world|recenter or dist|width|height|up|lateral <m>"); return true; }
        set_window(c, "the seam");
        return true;
    }
    if (!strcmp(w1, "hand") && n >= 2) {
        HandCfg c = g_hand;
        const float v = n >= 3 ? (float)atof(w3) : 0.0f;
        if (!strcmp(w2, "left")) c.hand = 0;
        else if (!strcmp(w2, "right")) c.hand = 1;
        else if (!strcmp(w2, "billboard")) c.followGrip = false;
        else if (!strcmp(w2, "grip")) c.followGrip = true;
        else if (n < 3) { DVR_WARN("hud: hand wants left|right|billboard|grip or x|y|z|lift|width|tilt <v>"); return true; }
        else if (!strcmp(w2, "x")) c.x = v;
        else if (!strcmp(w2, "y")) c.y = v;
        else if (!strcmp(w2, "z")) c.z = v;
        else if (!strcmp(w2, "lift")) c.liftM = v;
        else if (!strcmp(w2, "width")) c.widthM = v;
        else if (!strcmp(w2, "tilt")) c.tiltDeg = v;
        else { DVR_WARN("hud: hand wants left|right|billboard|grip or x|y|z|lift|width|tilt <v>"); return true; }
        set_hand(c, "the seam");
        return true;
    }
    if (!strcmp(w1, "place") && n >= 5) {
        const int e = element_from_name(w2);
        const bool onHand = !strcmp(w3, "hand");
        if (e < 0 || (!onHand && strcmp(w3, "window"))) { DVR_WARN("hud: place wants <element> window|hand <x> <y> [scale]"); return true; }
        const ElementCfg& c = g_el[e];
        const float scale = n >= 6 ? (float)atof(w5) : (onHand ? c.handScale : c.winScale);
        set_element_place(e, onHand, (float)atof(w4), (float)atof(w5), scale, "the seam");
        return true;
    }
    if (!strcmp(w1, "region") && n >= 3) {
        const int e = element_from_name(w2);
        float r[4] = {};
        if (e < 0 || sscanf(w3, "%f,%f,%f,%f", &r[0], &r[1], &r[2], &r[3]) != 4) { DVR_WARN("hud: region wants <element> x0,y0,x1,y1 (normalised backbuffer)"); return true; }
        set_element_rect(e, r, "the seam");
        return true;
    }
    // VR-119: hud alpha mode repair|captured|mix, hud alpha gain|floor|gamma|mix <f>,
    // hud alpha backdrop window|hand r,g,b,a, hud alpha status
    if (!strcmp(w1, "alpha")) {
        if (n < 2 || !strcmp(w2, "status")) { log_alpha(); return true; }
        AlphaCfg c = g_alpha;
        if (!strcmp(w2, "mode") && n >= 3) {
            const int m = alpha_mode_from_name(w3);
            if (m < 0) { DVR_WARN("hud: alpha mode wants repair|captured|mix"); return true; }
            c.mode = m; set_alpha(c, "the seam"); return true;
        }
        if (!strcmp(w2, "backdrop") && n >= 4) {
            const int kind = !_stricmp(w3, "hand") ? 1 : !_stricmp(w3, "window") ? 0 : -1;
            Backdrop b = {};
            if (kind < 0 || sscanf(w4, "%f,%f,%f,%f", &b.r, &b.g, &b.b, &b.a) != 4) {
                DVR_WARN("hud: alpha backdrop wants window|hand r,g,b,a (0..1; a=0 removes the plate)"); return true;
            }
            set_backdrop(kind, b, "the seam"); return true;
        }
        if (n < 3) { DVR_WARN("hud: alpha wants mode <m>, gain|floor|gamma|mix <f>, backdrop window|hand r,g,b,a, or status"); return true; }
        const float v = (float)atof(w3);
        if (!strcmp(w2, "gain")) c.gain = v;
        else if (!strcmp(w2, "floor")) c.floorA = v;
        else if (!strcmp(w2, "gamma")) c.gamma = v;
        else if (!strcmp(w2, "mix")) c.mixK = v;
        else { DVR_WARN("hud: alpha wants mode <m>, gain|floor|gamma|mix <f>, backdrop window|hand r,g,b,a, or status"); return true; }
        set_alpha(c, "the seam");
        return true;
    }
    return false;
}

void log_alpha() {
    DVR_INFO("hud/alpha: mode %s (0 repair = max(r,g,b); captured = the sink's coverage, forced per draw; mix = "
             "max(captured, repair*mix)) gain %.2f floor %.2f gamma %.2f mix %.2f | backdrop window %.2f,%.2f,%.2f a=%.2f "
             "hand %.2f,%.2f,%.2f a=%.2f | `hud alpha mode|gain|floor|gamma|mix|backdrop ...`",
             kAlphaModeNames[g_alpha.mode], g_alpha.gain, g_alpha.floorA, g_alpha.gamma, g_alpha.mixK,
             g_backdrop[0].r, g_backdrop[0].g, g_backdrop[0].b, g_backdrop[0].a,
             g_backdrop[1].r, g_backdrop[1].g, g_backdrop[1].b, g_backdrop[1].a);
}

const char* status_line() { return g_statusLine; }

void log_status() {
    DVR_INFO("hud/layout: %s", g_statusLine);
    log_alpha();
    DVR_INFO("hud/layout: routed this window: all=%u health=%u mana=%u equipment=%u reticle=%u subtitles=%u "
             "prompt=%u objective=%u vignette=%u menu=%u | no-region->all %u, no-sink->all %u, left in frame %u "
             "(no-region reads the whole count while [Hud] Regions=0: that is by design, not a fault)",
             g_routeCounts[0], g_routeCounts[1], g_routeCounts[2], g_routeCounts[3], g_routeCounts[4],
             g_routeCounts[5], g_routeCounts[6], g_routeCounts[7], g_routeCounts[8], g_routeCounts[9],
             g_routeNoRegion, g_routeOverflow, g_routeFrame);
    memset(g_routeCounts, 0, sizeof(g_routeCounts));
    g_routeNoRegion = g_routeOverflow = g_routeFrame = 0;
}

void status(dvr::status::Writer& w) {
    w.kv("window", g_win.worldLocked ? "world" : "view");
    w.kv("windowDist", (double)g_win.distM);
    w.kv("windowWidth", (double)g_win.widthM);
    w.kv("hand", g_hand.hand ? "right" : "left");
    w.kv("handWidth", (double)g_hand.widthM);
    w.kv("menuInWindow", g_menuInWindow);
    w.kv("menuRiding", g_menuRiding);
    w.kv("alphaMode", kAlphaModeNames[g_alpha.mode]);
    w.kv("alphaGain", (double)g_alpha.gain);
    w.kv("alphaFloor", (double)g_alpha.floorA);
    w.kv("alphaGamma", (double)g_alpha.gamma);
    w.kv("alphaMix", (double)g_alpha.mixK);
    w.kv("backdropWindowA", (double)g_backdrop[0].a);
    w.kv("backdropHandA", (double)g_backdrop[1].a);
    w.kv("noRegion", (unsigned long)g_routeNoRegion);
    w.kv("noSink", (unsigned long)g_routeOverflow);
    w.obj("elements");
    for (int e = 0; e < ElCount; ++e) w.kv(kElementNames[e], kAnchorNames[g_el[e].anchor & 3]);
    w.end_obj();
}

// ---- F10 ------------------------------------------------------------------

void draw_ui() {
    ImGui::TextDisabled("%s", g_statusLine);
    ImGui::Separator();
    ImGui::Text("ELEMENTS (which anchor each one rides; a region must be measured before an element separates from 'all')");
    for (int e = 0; e < ElCount; ++e) {
        ImGui::PushID(e);
        int a = g_el[e].anchor;
        ImGui::Text("%-10s", kElementNames[e]); ImGui::SameLine();
        bool changed = false;
        changed |= ImGui::RadioButton("off", &a, AnchorOff); ImGui::SameLine();
        changed |= ImGui::RadioButton("frame", &a, AnchorFrame); ImGui::SameLine();
        changed |= ImGui::RadioButton("window", &a, AnchorWindow); ImGui::SameLine();
        if (e == ElMenu) ImGui::BeginDisabled();
        changed |= ImGui::RadioButton("hand", &a, AnchorHand);
        if (e == ElMenu) ImGui::EndDisabled();
        if (changed) set_element_anchor(e, a, "F10 HUD");
        if (a == AnchorWindow || a == AnchorHand) {
            const bool onHand = a == AnchorHand;
            float x = onHand ? g_el[e].handX : g_el[e].winX;
            float y = onHand ? g_el[e].handY : g_el[e].winY;
            float s = onHand ? g_el[e].handScale : g_el[e].winScale;
            const float lim = onHand ? 0.3f : 1.5f;
            bool moved = false;
            ImGui::Indent();
            moved |= ImGui::SliderFloat("x (m)", &x, -lim, lim, "%.3f");
            moved |= ImGui::SliderFloat("y (m)", &y, -lim, lim, "%.3f");
            moved |= ImGui::SliderFloat("scale", &s, 0.25f, 3.0f, "%.2f");
            ImGui::Unindent();
            if (moved) set_element_place(e, onHand, x, y, s, "F10 HUD");
        }
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::Text("THE WINDOW (in front of the player)");
    {
        WindowCfg c = g_win;
        bool ch = false;
        int mode = c.worldLocked ? 1 : 0;
        ch |= ImGui::RadioButton("head-locked (VIEW space)", &mode, 0); ImGui::SameLine();
        ch |= ImGui::RadioButton("world-locked in front (LOCAL space)", &mode, 1);
        c.worldLocked = mode == 1;
        ImGui::SameLine();
        if (ImGui::Button("Recenter HUD window")) { dvr::vr::recenter_hud_world_anchor(); DVR_INFO("hud: window re-seeded where the head is now (F10)"); }
        ch |= ImGui::SliderFloat("distance (m)", &c.distM, 0.5f, 3.0f, "%.2f");
        ch |= ImGui::SliderFloat("width (m)", &c.widthM, 0.3f, 3.0f, "%.2f");
        ch |= ImGui::SliderFloat("height (m, 0 = the texture's aspect)", &c.heightM, 0.0f, 3.0f, "%.2f");
        ch |= ImGui::SliderFloat("vertical offset (m)", &c.upM, -1.0f, 1.0f, "%.2f");
        ch |= ImGui::SliderFloat("lateral offset (m)", &c.latM, -1.0f, 1.0f, "%.2f");
        if (ch) set_window(c, "F10 HUD");
    }
    ImGui::Separator();
    ImGui::Text("THE HAND PANEL (38.92's wrist HUD, on the tracked hand)");
    {
        HandCfg c = g_hand;
        bool ch = false;
        int hnd = c.hand;
        ch |= ImGui::RadioButton("left hand", &hnd, 0); ImGui::SameLine();
        ch |= ImGui::RadioButton("right hand", &hnd, 1);
        c.hand = hnd;
        int orient = c.followGrip ? 1 : 0;
        ch |= ImGui::RadioButton("billboard: faces the head, never rolls (38.92)", &orient, 0); ImGui::SameLine();
        ch |= ImGui::RadioButton("follow the grip: a watch face", &orient, 1);
        c.followGrip = orient == 1;
        ch |= ImGui::SliderFloat("x in the grip frame (m)", &c.x, -0.3f, 0.3f, "%.3f");
        ch |= ImGui::SliderFloat("y in the grip frame (m)", &c.y, -0.3f, 0.3f, "%.3f");
        ch |= ImGui::SliderFloat("z in the grip frame (m)", &c.z, -0.3f, 0.3f, "%.3f");
        ch |= ImGui::SliderFloat("lift along world up (m)", &c.liftM, 0.0f, 0.3f, "%.3f");
        ch |= ImGui::SliderFloat("panel width (m)", &c.widthM, 0.06f, 0.40f, "%.2f");
        if (c.followGrip) ch |= ImGui::SliderFloat("tilt toward the eyes (deg)", &c.tiltDeg, -90.0f, 90.0f, "%.0f");
        if (ch) set_hand(c, "F10 HUD");
    }
    ImGui::Separator();
    ImGui::Text("THE ALPHA (VR-119: how the quads' transparency is derived; repair = 41.2's max(r,g,b))");
    {
        AlphaCfg c = g_alpha;
        bool ch = false;
        int mode = c.mode;
        ch |= ImGui::RadioButton("repair (max of r,g,b)", &mode, AlphaRepair); ImGui::SameLine();
        ch |= ImGui::RadioButton("captured (the sink's coverage)", &mode, AlphaCaptured); ImGui::SameLine();
        ch |= ImGui::RadioButton("mix (max of both)", &mode, AlphaMix);
        c.mode = mode;
        ch |= ImGui::SliderFloat("alpha gain", &c.gain, 0.0f, 3.0f, "%.2f");
        ch |= ImGui::SliderFloat("alpha floor (pixels with any colour)", &c.floorA, 0.0f, 1.0f, "%.2f");
        ch |= ImGui::SliderFloat("gamma nudge", &c.gamma, 0.5f, 2.0f, "%.2f");
        if (c.mode == AlphaMix) ch |= ImGui::SliderFloat("mix: repair weight", &c.mixK, 0.0f, 2.0f, "%.2f");
        if (ch) set_alpha(c, "F10 HUD");
        for (int k = 0; k < 2; ++k) {
            ImGui::PushID(100 + k);
            Backdrop b = g_backdrop[k];
            float col[4] = { b.r, b.g, b.b, b.a };
            ImGui::Text("%s backdrop", kBackdropKindNames[k]); ImGui::SameLine();
            bool bc = ImGui::ColorEdit3("colour", col, ImGuiColorEditFlags_NoInputs); ImGui::SameLine();
            bc |= ImGui::SliderFloat("opacity (0 = no plate)", &col[3], 0.0f, 1.0f, "%.2f");
            if (bc) { b.r = col[0]; b.g = col[1]; b.b = col[2]; b.a = col[3]; set_backdrop(k, b, "F10 HUD"); }
            ImGui::PopID();
        }
    }
    ImGui::Separator();
    ImGui::Text("MENUS IN THE WINDOW (the world stays in stereo behind them; the main menu keeps the screen)");
    {
        bool on = g_menuInWindow;
        if (ImGui::Checkbox("in-game screens ride the HUD window", &on)) set_menu_in_window(on, "F10 HUD");
        uint32_t mask = g_menuMask;
        bool ch = false;
        for (int i = 0; i < kMenuContexts; ++i) {
            bool b = (mask & (1u << kMenuContextBits[i])) != 0;
            if (i) ImGui::SameLine();
            if (ImGui::Checkbox(kMenuContextNames[i], &b)) { ch = true; mask = b ? (mask | (1u << kMenuContextBits[i])) : (mask & ~(1u << kMenuContextBits[i])); }
        }
        if (ch) set_menu_context_mask(mask, "F10 HUD");
        ImGui::TextDisabled("%s", g_menuRiding ? "a menu is riding the window now" : "no menu riding");
    }
    ImGui::Separator();
    if (ImGui::Button("hud reset (the presets)")) reset_presets("F10 HUD");
    ImGui::SameLine();
    ImGui::TextDisabled("every change here is written to [Hud] in the ini at once");
}

} // namespace dvr::hudlayout
