#include "hud_menu_lifecycle.h"
#include "game/dishonored/objective_marker_policy.h"
#include "hud_wheel_parts.h"
// core/gfx/hud_layout.cpp - see hud_layout.h.
#define DVR_CAT ::dvr::log::Cat::hud
#include "core/gfx/hud_layout.h"
#include "core/input/weapon_dial.h"
#include "core/vr/openxr_input.h"

#include "core/framework/status.h"
#include "core/framework/frame_hooks.h"
#include "core/gfx/hud_capture.h"
#include "core/gfx/capture.h"
#include "core/gfx/hud_route.h"
#include "core/gfx/hud_native_icon.h"
#include "core/gfx/hud_native_rune.h"
#include "core/util/log.h"
#include "core/util/clock.h"
#include "core/vr/hud_anchor.h"
#include "core/vr/openxr_runtime.h"

#include <d3d11.h>
#include <imgui.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <atomic>

namespace dvr::hudlayout {
namespace {

const char* const kAnchorNames[AnchorCount] = { "off", "frame", "window", "world", "handL", "handR" };

// The element table. The regions are CLAIMING rectangles: the measured union
// of each element's draws (ENGINE_NOTES, "How the Scaleform HUD identifies its
// elements", the sewer level, 2026-09-15) with a margin, and a draw is claimed
// when its centre lies inside. An all-zero region is unmeasured: that element
// routes to `default` until `hud region <name> x0,y0,x1,y1` names it. The six
// screens claim by their UI owner context (dvr::mono::Context 3..8) instead.
struct RowDef {
    const char* name;
    int         context;
    float       rect[4];
    bool        vignette;
    int         presetAnchor;
    const char* what;
};
const RowDef kRows[ElCount] = {
    { "default",      -1, {0, 0, 0, 0},                       false, AnchorWindow, "every draw no row claims; an unnamed element rides here and is counted" },
    { "vitals",       -1, {0.000f, 0.000f, 0.200f, 0.270f},   false, AnchorWindow, "the health and mana bars with their frames (top-left; the two interleave in x, so one row)" },
    { "reticle",      -1, {0.470f, 0.470f, 0.530f, 0.530f},   false, AnchorWindow, "the dot at the centre (it grows when an interactable is focused)" },
    { "prompt",       -1, {0.520f, 0.460f, 0.800f, 0.620f},   false, AnchorWindow, "the interaction label right of the reticle (a plate, a text run, a rule, two icons)" },
    { "equipment",    -1, {0, 0, 0, 0},                       false, AnchorWindow, "the equipped item icons (unmeasured: not drawn where the simulator can reach)" },
    { "subtitles",    -1, {0, 0, 0, 0},                       false, AnchorWindow, "conversation subtitles (unmeasured)" },
    { "objective",    -1, {0, 0, 0, 0},                       false, AnchorWindow, "the objective marker, a 0.033 square that moves with its target (no rectangle can claim it)" },
    { "toast",        -1, {0, 0, 0, 0},                       false, AnchorWindow, "the pickup toast (unmeasured)" },
    { "tutorial",     -1, {0, 0, 0, 0},                       false, AnchorWindow, "the tutorial window (unmeasured)" },
    { "detection",    -1, {0, 0, 0, 0},                       false, AnchorWindow, "the detection arrows around the reticle (unmeasured)" },
    { "skipgauge",    -1, {0, 0, 0, 0},                       false, AnchorWindow, "the hold-to-skip gauge (unmeasured)" },
    { "darkvision",   -1, {0, 0, 0, 0},                       false, AnchorWindow, "the dark vision overlay (unmeasured)" },
    { "vignette",     -1, {0, 0, 0, 0},                       true,  AnchorWindow, "any draw wider and taller than 60 % of the screen (a full-screen fill)" },
    { "pause",         3, {0, 0, 0, 0},                       false, AnchorWindow, "the pause menu (by its UI owner context while it rides)" },
    { "note",          4, {0, 0, 0, 0},                       false, AnchorWindow, "a readable note (by context)" },
    { "journal",       5, {0, 0, 0, 0},                       false, AnchorWindow, "the journal (by context)" },
    { "wheel",         6, {0, 0, 0, 0},                       false, AnchorWindow, "the power wheel: the weapon scroll and the grip-hold loadout (by context)" },
    { "store",         7, {0, 0, 0, 0},                       false, AnchorWindow, "the store (by context)" },
    { "missionstats",  8, {0, 0, 0, 0},                       false, AnchorWindow, "the mission stats (by context)" },
    { "wheelshortcuts",-1, {0,0,0,0},false,AnchorWindow,"wheel D-pad shortcuts, from the same captured image" },
    { "wheelpotions",  -1, {0,0,0,0},false,AnchorWindow,"wheel health and mana controls, from the same captured image" },
    { "vitalshealth",  -1, {0,0,0,0},false,AnchorHandR,"the health bar, cut from the vitals image along the split line (VitalsSplit=1)" },
    { "vitalsmana",    -1, {0,0,0,0},false,AnchorHandL,"the mana bar and the equipped item, cut from the vitals image (VitalsSplit=1)" },
};

// The presets: the window as the abandoned branch shipped it, the hands as
// 38.92 tuned the wrist HUD (0.22 m, 0.06 m up, billboarded), every element on
// the window (the VR-117 picture: a tester who updates sees no change until the
// headset judges the split), the alpha at identity.
const WindowCfg kPresetWindow = { 1.30f, 1.25f, 0.0f, -0.10f, 0.0f };
const HandCfg   kPresetHand   = { 0.0f, 0.0f, 0.0f, 0.06f, 0.22f, false, 0.0f, 0.0f };
const char* const kAlphaModeNames[3] = { "repair", "captured", "mix" };
const AlphaCfg  kPresetAlpha = { AlphaRepair, 1.0f, 0.0f, 1.0f, 1.0f };
const Backdrop  kPresetBackdrop = { 0.0f, 0.0f, 0.0f, 0.0f };
const char* const kBackdropKindNames[2] = { "window", "hand" };
const char* const kHandNames[2] = { "HandL", "HandR" };
// Pause, Note, Journal, Wheel, Store, MissionStats (dvr::mono::Context bits 3..8).
const uint32_t  kPresetMenuMask = (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8);
const char* const kMenuContextNames[] = { "Pause", "Note", "Journal", "Wheel", "Store", "MissionStats" };
const unsigned    kMenuContextBits[]  = { 3, 4, 5, 6, 7, 8 };
const int         kMenuContexts = 6;

ElementCfg g_el[ElCount];
hudroute::StableRoutes g_stableRoutes;
dvr::hudnative::Markers g_nativeMarkers,g_nativeChildContent;
dvr::hudnative::MarkerLabels g_nativeLabels;
dvr::hudnative::RuneIconContinuity g_runeIconContinuity;
bool g_nativeObjectiveLabels=false,g_nativeObjectiveUpright=false,g_wheelCloseAnimation=false;
hudroute::InteractionGroup g_interactionGroup;
bool g_groupInteractions=false,g_routeObjectives=false,g_objectiveScreen=false;
bool g_nativeObjectives=false;
bool g_nativeMarkerChildren=false;
bool g_nativeGameplayReference=false;
bool g_wheelParts=false;
float g_wheelPartCrop[2][4]={{.02f,.29f,.995f,.31f},{.70f,1.f,.995f,.16f}};
const char* kWheelPartKeys[2]={"WheelShortcuts","WheelPotions"};
const char* kWheelPartNames[2]={"D-pad shortcuts","Health and mana"};
// VR-142: the vitals image cut in two along a diagonal: the bars are slanted,
// so no rectangle separates them. The line runs from (Top, y0) to (Bottom, y1)
// of the vitals region, in screen fractions; health keeps the left side.
bool g_vitalsSplit=false;
float g_vitalsLine[2]={.125f,.065f};
float g_vitalsCrop[2][4]={{0.f,0.f,.200f,.270f},{0.f,0.f,.200f,.270f}};
const char* kVitalsPartKeys[2]={"VitalsHealth","VitalsMana"};
const char* kVitalsPartNames[2]={"Health","Mana and equipped item"};
// Run490: the left panel sat oddly - its HandX (0.078) was set by hand in the
// SAME direction as the right's (0.084), and each part texture still spanned
// the whole vitals region with its content off to one side. Mirror links the
// mana panel to health's placement (x negated), and AutoCrop trims each part
// at the split line so its content is centred.
bool g_vitalsMirror=true,g_vitalsAutoCrop=true;
// The back-of-hand mode: both panels lie ON the back of the hand like a watch
// face (FollowGrip), from their own offsets, leaving HandL/HandR untouched for
// every other hand element. [0] out from the back of the hand, [1] along the
// knuckles (grip +Y), [2] along the controller (grip Z), [3] tilt, [4] spin.
// The left hand mirrors the right: the out offset and the spin flip sign.
bool g_vitalsBack=false;
float g_vitalsBackCfg[5]={.045f,0.f,0.f,0.f,0.f};
const char* kVitalsBackKeys[5]={"VitalsBack.Out","VitalsBack.Along","VitalsBack.Forward","VitalsBack.Tilt","VitalsBack.Spin"};
// Candidate 493: the back-of-hand guess landed at the far end of the hand model
// with too little slider range. The ATTACH step measures instead of guessing:
// both panels freeze in front of the head, the tester holds each hand where its
// panel should ride, and after the countdown each panel's pose is stored in its
// hand's grip frame (position and rotation). No axis convention is assumed.
// [0] = left hand (mana), [1] = right hand (health), by the part's anchor.
bool g_vaOn=false,g_vaValid[2]={false,false};
float g_vaPos[2][3]={},g_vaQ[2][4]={{0,0,0,1},{0,0,0,1}};
float g_vaSeconds=10.f;   // the tester asked for 10 (candidate 495 had 5)
unsigned long long g_vaStart=0;              // 0 = not counting
float g_vaPanelPos[2][3]={},g_vaPanelQ[4]={0,0,0,1};
const char* kVaKeys[2]={"VitalsAttach.L","VitalsAttach.R"};
// Run497: anchored to the controller, the panels drifted from the drawn hand with
// a stance change (the hand model's place relative to the controller is not
// constant), and the animation move carried back from the controller did not
// land. The panels now ride the DRAWN palm: the hand draw publishes, once per
// present, where its palm appears in XR space, and the attach step stores each
// panel relative to that. Frame 1 = palm, 0 = grip (older captures).
int g_vaFrame[2]={0,0};
bool g_vaFlip[2]={false,false}, g_palmFlip[2]={false,false};
// VR-142: draw the vitals in the game frame on the hand instead of as XR quads.
bool g_vitalsScene=false;
float g_vsTrim[2][3]={};
SRWLOCK g_palmLock = SRWLOCK_INIT;
float g_palmPos[2][3] = {}, g_palmQ[2][4] = {{0,0,0,1},{0,0,0,1}};
unsigned long long g_palmMs[2] = {};
bool hand_palm_pose(int h, float p[3], float q[4]) {
    if (h < 0 || h > 1) return false;
    AcquireSRWLockShared(&g_palmLock);
    const bool fresh = g_palmMs[h] && GetTickCount64() - g_palmMs[h] < 150;
    memcpy(p, g_palmPos[h], 3 * sizeof(float)); memcpy(q, g_palmQ[h], 4 * sizeof(float));
    ReleaseSRWLockShared(&g_palmLock);
    return fresh;
}
int vitals_hand(int part) { const int a=g_el[part?ElVitalsMana:ElVitalsHealth].anchor; return a==AnchorHandL?0:a==AnchorHandR?1:-1; }
float g_nativeObjectiveScale=.70f;
hudroute::Row g_rows[ElCount];       // the routing view of g_el (rect + context), rebuilt on a region change
dvr::weapon_dial::State g_dial;
dvr::weapon_dial::State g_dialVisual; // survives grip release until the screen closes
bool g_dialDirection = true, g_dialCircle = true;
bool g_dialEntryTilt=false,g_dialEntryYaw=false;
float g_dialDeadM = .002f, g_dialDistance = 0;
float g_dialForward[3] = {0,0,-1};
std::atomic<uint32_t> g_menuHeadMask{0},g_menuBlurMask{0};
bool g_dialOn = false; // experimental placement: installed test opts in
float g_dialWidth = .35f, g_dialRadius = .04f;
float g_dialCropX = .40f, g_dialCropY = .40f;
WindowCfg  g_win = kPresetWindow;
HandCfg    g_hand[2] = { kPresetHand, kPresetHand };
dvr::hudalpha::Bank g_alphaBank;
AlphaCfg& g_alpha=g_alphaBank.general;
const char* kScopedAlpha[5]={"WeaponDialAlpha","ReadingAlpha","InteractionAlpha","PauseAlpha","WheelPartsAlpha"};
bool g_readHand[2]={false,false};
float g_readTilt=0;
float g_readUp[2]={0,0};
std::atomic<bool> g_pauseSceneFreshness{false},g_menuExitHeading{false};
bool g_visualRiding=false;
WheelVisualLease g_wheelVisual;
float g_readWidth[2]={.60f,.70f},g_readDistance[2]={-.05f,-.05f},g_readRight[2]={.20f,.20f};
const char* kReadNames[2]={"Note","Journal"};
Backdrop   g_backdrop[2] = { kPresetBackdrop, kPresetBackdrop };
bool       g_menuInWindow = true;
uint32_t   g_menuMask = kPresetMenuMask;
bool       g_menuRiding = false;
int        g_ridingContext = -1;
char       g_ini[MAX_PATH] = "";

// Sinks: private measured elements, shared catch-alls per anchor. Present thread only (draws, the seam poll, the
// overlay's draw callback and the runtime's provider all run there);
// configure() runs at DllMain before any of them.
struct SinkUse { int anchor; bool crop; bool rideOnly; int element = -1; };
SinkUse  g_sink[kMaxSinks];
int      g_sinkOf[AnchorCount][2];
int      g_elementSink[ElCount];
char     g_sinkLabel[kMaxSinks][24];
uint32_t g_routeCounts[ElCount];     // draws routed per element this window
uint32_t g_seen[ElCount];            // and this session
uint32_t g_lastRouted[ElCount];      // the present number a draw last routed to the element
uint32_t g_presentNo = 0;            // counted in provide()
uint32_t g_routeNoRegion = 0;        // draws with no readable region -> default
uint32_t g_routeOverflow = 0;        // draws whose (anchor, crop) had no free sink -> default's
uint32_t g_routeFrame = 0;           // draws left in the frame (AnchorFrame)
char g_statusLine[768] = "not configured";

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
bool read_s(const char* ini, const char* key, char* out, size_t n) {
    out[0] = 0;
    GetPrivateProfileStringA("Hud", key, "", out, (DWORD)n, ini);
    return out[0] != 0;
}

void save_read_rotation() {
    write_f("ReadingTilt",g_readTilt);
    write_i("ReadingTiltReference",1);
}

inline bool measured(int e) { return hudroute::row_measured(g_rows[e]); }
inline bool crop_eligible(int e) { return (e==ElObjective && g_routeObjectives) || (e != ElDefault && !kRows[e].vignette && kRows[e].context < 0 && measured(e)); }
inline int  anchor_kind(int a) { return anchor_is_hand(a) ? 1 : 0; }

void rebuild_rows() {
    g_stableRoutes.clear();g_interactionGroup.clear();
    for (int e = 0; e < ElCount; ++e) {
        g_rows[e].name = kRows[e].name;
        g_rows[e].context = kRows[e].context;
        g_rows[e].vignette = kRows[e].vignette;
        memcpy(g_rows[e].rect, g_el[e].rect, sizeof(g_rows[e].rect));
    }
}

void free_sink(int s) {
    if (s < 0 || s >= kMaxSinks || g_sink[s].anchor < 0) return;
    if(g_sink[s].element >= 0) g_elementSink[g_sink[s].element] = -1;
    else g_sinkOf[g_sink[s].anchor][g_sink[s].crop ? 1 : 0] = -1;
    g_sink[s].element = -1;
    DVR_INFO("hud/layout: sink %d (%s) released", s, g_sinkLabel[s]);
    g_sink[s].anchor = -1; g_sink[s].crop = false; g_sink[s].rideOnly = false;
    g_sinkLabel[s][0] = 0;
}

int acquire_sink(int anchor, bool crop, int element = -1) {
    const int have = element >= 0 ? g_elementSink[element] : g_sinkOf[anchor][crop ? 1 : 0];
    if (have >= 0) return have;
    for (int s = 0; s < kMaxSinks; ++s) {
        if (g_sink[s].anchor >= 0) continue;
        g_sink[s].anchor = anchor; g_sink[s].crop = crop; g_sink[s].rideOnly = g_visualRiding;
        g_sink[s].element = element;
        if(element >= 0) g_elementSink[element] = s;
        else g_sinkOf[anchor][crop ? 1 : 0] = s;
        _snprintf(g_sinkLabel[s], sizeof(g_sinkLabel[s]), "%s/%s", kAnchorNames[anchor], element>=0 ? kRows[element].name : "all");
        g_sinkLabel[s][sizeof(g_sinkLabel[s]) - 1] = 0;
        DVR_INFO("hud/layout: sink %d = %s%s (a copy per present from here on)", s, g_sinkLabel[s],
                 g_menuRiding ? ", for the riding screen" : "");
        return s;
    }
    return -1;
}

// Every sink goes back to the pool; the next draws re-acquire what they need
// (a config change costs one target rebuild, never a dropped draw).
void rebalance() {
    g_stableRoutes.clear();g_interactionGroup.clear();
    for (int s = 0; s < kMaxSinks; ++s) free_sink(s);
}

void refresh_status_line() {
    char* p = g_statusLine;
    size_t n = sizeof(g_statusLine);
    int w = _snprintf(p, n, "window %.2fm@%.2fm handL %.2fm handR %.2fm | ",
                      g_win.widthM, g_win.distM, g_hand[0].widthM, g_hand[1].widthM);
    if (w < 0 || (size_t)w >= n) return;
    p += w; n -= w;
    for (int e = 0; e < ElCount; ++e) {
        const char* why = "";
        if (g_el[e].anchor == AnchorOff) why = "(hidden)";
        else if(e==ElObjective && g_routeObjectives) why="(moving-shape candidate)";
        else if (kRows[e].context < 0 && e != ElDefault && !kRows[e].vignette && !measured(e)) why = "(no region: rides default)";
        w = _snprintf(p, n, "%s=%s%s ", kRows[e].name, kAnchorNames[g_el[e].anchor], why);
        if (w < 0 || (size_t)w >= n) break;
        p += w; n -= w;
    }
    g_statusLine[sizeof(g_statusLine) - 1] = 0;
}

} // namespace

// ---------------------------------------------------------------------------

const char* anchor_name(int a) { return kAnchorNames[(a >= 0 && a < AnchorCount) ? a : 0]; }
int anchor_from_name(const char* s) {
    for (int i = 0; i < AnchorCount; ++i) if (!_stricmp(s, kAnchorNames[i])) return i;
    if (!_stricmp(s, "hand")) return AnchorHandL;       // VR-117's single hand
    if (!_stricmp(s, "view")) return AnchorWindow;
    return -1;
}
const char* element_name(int e) { return (e >= 0 && e < ElCount) ? kRows[e].name : "?"; }
int element_from_name(const char* s) {
    for (int i = 0; i < ElCount; ++i) if (!_stricmp(s, kRows[i].name)) return i;
    if (!_stricmp(s, "health") || !_stricmp(s, "mana")) return ElVitals;   // VR-117's names
    if (!_stricmp(s, "menu")) return ElPause;
    return -1;
}
bool element_is_screen(int e) { return e >= 0 && e < ElCount && kRows[e].context >= 0; }
int element_for_context(int context) {
    for (int i = 0; i < ElCount; ++i) if (kRows[i].context == context) return i;
    return -1;
}

const ElementCfg& element(int e) { return g_el[(e >= 0 && e < ElCount) ? e : 0]; }
const WindowCfg&  window() { return g_win; }
const HandCfg&    hand(int which) { return g_hand[which ? 1 : 0]; }

// ---- VR-119: the alpha and the backdrops ------------------------------------

const char* alpha_mode_name(int m) { return kAlphaModeNames[(m >= 0 && m < 3) ? m : 0]; }
int alpha_mode_from_name(const char* s) {
    for (int i = 0; i < 3; ++i) if (!_stricmp(s, kAlphaModeNames[i])) return i;
    return -1;
}
const AlphaCfg& alpha() { return g_alpha; }
AlphaCfg wheel_parts_alpha() { return g_alphaBank.for_owner(dvr::hudalpha::WheelParts); }
AlphaCfg alpha_for_sink(int sink) {
    using namespace dvr::hudalpha;
    Owner owner=General;
    if(sink>=0 && sink<kMaxSinks) {
        if(g_visualRiding && !g_sink[sink].crop) {
            const int e=element_for_context(g_ridingContext);
            if(e>=0 && g_sink[sink].anchor==g_el[e].anchor) {
                if(e==ElWheel) owner=Wheel;
                else if(e==ElNote || e==ElJournal) owner=Reading;
                else if(e==ElPause) owner=Pause;
            }
        } else if(g_sink[sink].element==ElPrompt) owner=Interaction;
    }
    return g_alphaBank.for_owner(owner);
}
// Persist mode/mix as well, so resetting general alpha cannot alter a scoped group.
static void save_scoped_alpha(int i) {
    const auto& a=g_alphaBank.special[i];char key[64];
    _snprintf(key,sizeof(key),"%sMode",kScopedAlpha[i]);write_key(key,alpha_mode_name(a.mode));
    _snprintf(key,sizeof(key),"%sGain",kScopedAlpha[i]);write_f(key,a.gain);
    _snprintf(key,sizeof(key),"%sFloor",kScopedAlpha[i]);write_f(key,a.floorA);
    _snprintf(key,sizeof(key),"%sGamma",kScopedAlpha[i]);write_f(key,a.gamma);
    _snprintf(key,sizeof(key),"%sMix",kScopedAlpha[i]);write_f(key,a.mixK);
}
static void draw_scoped_alpha(int i) {
    ImGui::PushID(kScopedAlpha[i]);auto& a=g_alphaBank.special[i];
    bool change=ImGui::SliderFloat("Alpha gain",&a.gain,0,3,"%.2f");
    change|=ImGui::SliderFloat("Alpha floor",&a.floorA,0,1,"%.2f");
    change|=ImGui::SliderFloat("Alpha gamma",&a.gamma,.25f,4,"%.2f");
    change|=ImGui::Combo("Alpha source",&a.mode,kAlphaModeNames,3);
    if(a.mode==AlphaMix) change|=ImGui::SliderFloat("Repair mix weight",&a.mixK,0,4,"%.2f");
    if(change) save_scoped_alpha(i);
    ImGui::PopID();
}

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
    const int a = sink_anchor(sink);
    const int kind = a >= 0 ? anchor_kind(a) : 0;
    rgba[0] = g_backdrop[kind].r; rgba[1] = g_backdrop[kind].g; rgba[2] = g_backdrop[kind].b; rgba[3] = g_backdrop[kind].a;
}

// ---- the mutators (each writes its ini key at once) -----------------------

void set_element_anchor(int e, int anchor, const char* who) {
    if (e < 0 || e >= ElCount) return;
    if (anchor < 0 || anchor >= AnchorCount) return;
    if (g_el[e].anchor != anchor)
        DVR_INFO("hud/layout: element %s anchor %s -> %s (%s)%s", kRows[e].name,
                 kAnchorNames[g_el[e].anchor], kAnchorNames[anchor], who,
                 element_is_screen(e) && !anchor_visible(anchor) ? " - that screen takes the mono screen from now on" : "");
    g_el[e].anchor = anchor;
    rebalance();
    char key[64]; _snprintf(key, sizeof(key), "Element.%s", kRows[e].name); key[63] = 0;
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
                     kRows[e].name, onHand ? "hand" : "window", x, y, scale, who);
    char key[64];
    _snprintf(key, sizeof(key), "Element.%s.%sX", kRows[e].name, onHand ? "Hand" : "Win"); write_f(key, x);
    _snprintf(key, sizeof(key), "Element.%s.%sY", kRows[e].name, onHand ? "Hand" : "Win"); write_f(key, y);
    _snprintf(key, sizeof(key), "Element.%s.%sScale", kRows[e].name, onHand ? "Hand" : "Win"); write_f(key, scale);
}

void set_element_rect(int e, const float rect[4], const char* who) {
    if (e < 0 || e >= ElCount || !rect) return;
    memcpy(g_el[e].rect, rect, sizeof(g_el[e].rect));
    rebuild_rows();
    DVR_INFO("hud/layout: element %s region %.3f,%.3f-%.3f,%.3f (%s)", kRows[e].name,
             rect[0], rect[1], rect[2], rect[3], who);
    rebalance();
    char key[64], v[96];
    _snprintf(key, sizeof(key), "Region.%s", kRows[e].name); key[63] = 0;
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
                         "hud/layout: window dist %.2f width %.2f height %.2f up %.2f lateral %.2f (%s)",
                         c.distM, c.widthM, c.heightM, c.upM, c.latM, who);
    write_f("WindowDistance", c.distM);
    write_f("WindowWidth", c.widthM);
    write_f("WindowHeight", c.heightM);
    write_f("WindowUp", c.upM);
    write_f("WindowLateral", c.latM);
    refresh_status_line();
}

void set_hand(int which, const HandCfg& h, const char* who) {
    which = which ? 1 : 0;
    HandCfg c = h;
    if (c.widthM < 0.05f) c.widthM = 0.05f;
    if (c.widthM > 0.60f) c.widthM = 0.60f;
    if (c.liftM < -0.3f) c.liftM = -0.3f;
    if (c.liftM > 0.3f) c.liftM = 0.3f;
    float* axes[3] = { &c.x, &c.y, &c.z };
    for (float* f : axes) { if (*f < -0.3f) *f = -0.3f; if (*f > 0.3f) *f = 0.3f; }
    if (c.tiltDeg < -90.0f) c.tiltDeg = -90.0f;
    if (c.tiltDeg > 90.0f) c.tiltDeg = 90.0f;
    if (!(c.spinDeg >= -180.0f && c.spinDeg <= 180.0f)) c.spinDeg = 0.0f;
    const bool changed = memcmp(&c, &g_hand[which], sizeof(c)) != 0;
    g_hand[which] = c;
    if (changed)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
                         "hud/layout: hand panel %c offset (%.3f %.3f %.3f) lift %.3f width %.2f %s tilt %.0f (%s)",
                         which ? 'R' : 'L', c.x, c.y, c.z, c.liftM, c.widthM,
                         c.followGrip ? "follow-grip" : "billboard", c.tiltDeg, who);
    char key[64];
    const char* hn = kHandNames[which];
    _snprintf(key, sizeof(key), "%s.X", hn); write_f(key, c.x);
    _snprintf(key, sizeof(key), "%s.Y", hn); write_f(key, c.y);
    _snprintf(key, sizeof(key), "%s.Z", hn); write_f(key, c.z);
    _snprintf(key, sizeof(key), "%s.Lift", hn); write_f(key, c.liftM);
    _snprintf(key, sizeof(key), "%s.Width", hn); write_f(key, c.widthM);
    _snprintf(key, sizeof(key), "%s.Orient", hn); write_key(key, c.followGrip ? "grip" : "billboard");
    _snprintf(key, sizeof(key), "%s.Tilt", hn); write_f(key, c.tiltDeg);
    _snprintf(key, sizeof(key), "%s.Spin", hn); write_f(key, c.spinDeg);
    refresh_status_line();
}

void reset_presets(const char* who) {
    DVR_INFO("hud/layout: presets restored (%s)", who);
    for (int e = 0; e < ElCount; ++e) {
        ElementCfg c = { kRows[e].presetAnchor, 0, 0, 1, 0, 0, 1, {0, 0, 0, 0} };
        if(e==ElWheelShortcuts || e==ElWheelPotions) {c.winX=e==ElWheelShortcuts?-.32f:.32f;c.winY=-.22f;}
        memcpy(c.rect, kRows[e].rect, sizeof(c.rect));   // the compiled region; a live `hud region` is replaced
        g_el[e] = c;
        char key[64];
        _snprintf(key, sizeof(key), "Element.%s", kRows[e].name); key[63] = 0;
        write_key(key, kAnchorNames[c.anchor]);
        set_element_place(e, false, c.winX, c.winY, c.winScale, who);
        set_element_place(e, true, c.handX, c.handY, c.handScale, who);
        _snprintf(key, sizeof(key), "Region.%s", kRows[e].name); key[63] = 0;
        if (kRows[e].rect[2] > kRows[e].rect[0]) {
            char v[96];
            _snprintf(v, sizeof(v), "%.3f,%.3f,%.3f,%.3f", c.rect[0], c.rect[1], c.rect[2], c.rect[3]); v[95] = 0;
            write_key(key, v);
        } else {
            write_key(key, nullptr);   // a live region named by the seam goes with the reset
        }
    }
    rebuild_rows();
    set_window(kPresetWindow, who);
    set_hand(0, kPresetHand, who);
    set_hand(1, kPresetHand, who);
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
        DVR_INFO("hud/layout: in-game screens %s (%s)", on ? "ride the HUD anchors with the world in stereo behind"
                                                        : "take the mono screen, the old way", who);
    g_menuInWindow = on;
    write_key("MenuInWindow", on ? "1" : "0");
}
uint32_t menu_context_mask() { return g_menuMask; }
void set_menu_context_mask(uint32_t mask, const char* who) {
    if (mask != g_menuMask) DVR_INFO("hud/layout: screen contexts riding: 0x%x (%s)", mask, who);
    g_menuMask = mask;
    for (int i = 0; i < kMenuContexts; ++i) {
        char key[64]; _snprintf(key, sizeof(key), "Window%s", kMenuContextNames[i]); key[63] = 0;
        write_key(key, (mask & (1u << kMenuContextBits[i])) ? "1" : "0");
    }
}
bool screen_can_ride(int context) {
    const int e = element_for_context(context);
    return e >= 0 && anchor_visible(g_el[e].anchor);
}
void set_menu_riding(bool riding, int context, bool wheelClosing) {
    const uint32_t frame=(uint32_t)dvr::frame::count();
    const bool newWheel=riding && context==6 && (!g_menuRiding || g_ridingContext!=6);
    const bool wheelVisual=g_wheelVisual.update(riding,context,wheelClosing,frame,dvr::clock::now_ms(),g_wheelCloseAnimation);
    const bool tail=g_wheelVisual.tail;
    const bool visual=riding || wheelVisual;
    if(newWheel) {
        g_dial.reset();g_dialVisual.reset();
        // The game can open the wheel without our grip-held input path. Every
        // actual screen entry gets a fresh visual origin, never the prior wheel.
        float hp[3]{},hq[4]{},x=0,y=0;dvr::vr::HeadPose head{};
        const bool tracked=dvr::vr::peek_head_pose(head) && dvr::vr::input_get_hand_pose(0,false,hp,hq);
        const float eye[3]={head.px,head.py,head.pz},camera[4]={head.qx,head.qy,head.qz,head.qw},f[3]={0,0,-1};
        g_dialVisual.update(true,tracked,hp,eye,g_dialRadius,g_dialDeadM,x,y,camera,g_dialDirection,g_dialEntryTilt,g_dialEntryYaw);
        const auto* q=g_dialVisual.opening.q;
        dvr::xrmath::quat_rotate(q[0],q[1],q[2],q[3],f,g_dialForward);
        DVR_INFO("hud/dial: visual entry tracked=%d valid=%d center=%.3f/%.3f/%.3f frame=%u",
            (int)tracked,(int)g_dialVisual.valid,hp[0],hp[1],hp[2],frame);
    }
    const int visualContext=riding ? context : visual ? 6 : -1;
    const bool changed=visual!=g_visualRiding || visualContext!=g_ridingContext;
    g_menuRiding=riding;
    if(!changed) return;
    dvr::hudcap::invalidate_content();
    g_visualRiding=visual;g_ridingContext=visualContext;
    if(!visual) {
        for(int s=0;s<kMaxSinks;++s) if(g_sink[s].anchor>=0 && g_sink[s].rideOnly) free_sink(s);
    }
    g_stableRoutes.clear();g_interactionGroup.clear();g_nativeLabels.clear();g_runeIconContinuity.clear();
    DVR_INFO("hud/layout: visual context=%d inputRiding=%d closing=%d tail=%d frame=%u",
        visualContext,(int)riding,(int)wheelClosing,(int)tail,frame);
    if(visual) DVR_INFO("hud/layout: the screen is %s on the %s",kRows[element_for_context(visualContext)].name,kAnchorNames[g_el[element_for_context(visualContext)].anchor]);
    else DVR_INFO("hud/layout: the screen left: routing by element again");
}
void forget_draw_owners() { dvr::objectivemarkers::clear_rune_positions(); g_stableRoutes.clear();g_interactionGroup.clear();g_nativeMarkers.clear();g_nativeChildContent.clear();g_runeIconContinuity.clear();g_nativeLabels.clear(); }
bool native_gameplay_reference() {return g_nativeGameplayReference && !g_visualRiding;}
bool native_objective_upright(int e) {return !native_gameplay_reference() && g_nativeObjectives && g_nativeObjectiveUpright && !g_visualRiding && e==ElObjective;}
bool menu_exit_heading() {return g_menuExitHeading.load();}
bool pause_scene_freshness() {return g_pauseSceneFreshness.load();}
bool menu_riding() { return g_menuRiding; }
float native_objective_scale(int e) {return !native_gameplay_reference() && g_nativeObjectives && !g_menuRiding && e==ElObjective ? g_nativeObjectiveScale : 1.f;}
bool menu_stereo_hold() { return g_menuRiding && menu_head_look(g_ridingContext); }
bool menu_head_look(int c) { return c>=3 && c<=8 && (g_menuHeadMask.load() & (1u<<c)); }
bool menu_no_blur(int c) { return c>=3 && c<=8 && (g_menuBlurMask.load() & (1u<<c)); }
bool wheel_parts_for_sink(int sink) {
    return g_wheelParts && g_dialOn && g_visualRiding && g_ridingContext==6 &&
        sink>=0 && sink<kMaxSinks && !g_sink[sink].crop && g_sink[sink].anchor==g_el[ElWheel].anchor;
}
bool wheel_part_crop(int sink,int part,unsigned width,unsigned height,float* rect) {
    return wheel_parts_for_sink(sink) && part>=0 && part<2 &&
        dvr::wheelparts::crop((unsigned)part,width,height,g_wheelPartCrop[part],rect);
}
bool vitals_part(int sink,int part,float* rect,float* halfPlane) {
    if(!g_vitalsSplit || part<0 || part>1 || sink<0 || sink>=kMaxSinks || g_elementSink[ElVitals]!=sink || g_visualRiding) return false;
    const float* c=g_vitalsCrop[part];
    if(!(c[2]>c[0] && c[3]>c[1])) return false;
    for(int k=0;k<4;++k) rect[k]=c[k];
    if(g_vitalsAutoCrop) {   // trim at the line's far end (+0.01 of the screen), so each part's content is centred
        const float lo=fminf(g_vitalsLine[0],g_vitalsLine[1])-.01f,hi=fmaxf(g_vitalsLine[0],g_vitalsLine[1])+.01f;
        if(part==0) rect[2]=fminf(rect[2],hi); else rect[0]=fmaxf(rect[0],lo);
        if(!(rect[2]>rect[0])) return false;
    }
    const float y0=g_el[ElVitals].rect[1],y1=g_el[ElVitals].rect[3];
    const float slope=y1>y0 ? (g_vitalsLine[1]-g_vitalsLine[0])/(y1-y0) : 0.f;
    // f(u,v) = u - top - slope*(v-y0): < 0 left of the line (health), > 0 right (mana).
    const float s=part ? 1.f : -1.f;
    halfPlane[0]=s; halfPlane[1]=-s*slope; halfPlane[2]=-s*(g_vitalsLine[0]-slope*y0);
    return true;
}
bool wants_palm_pose() { return g_vitalsSplit && (g_vaStart || g_vaOn); }
bool vitals_scene_on() { return g_vitalsSplit && g_vitalsScene && !g_visualRiding; }
bool vitals_scene_cfg(int hand, VitalsSceneCfg* out) {
    if (!vitals_scene_on() || hand < 0 || hand > 1) return false;
    int part = -1;
    for (int p = 0; p < 2; ++p) if (vitals_hand(p) == hand) part = p;
    if (part < 0 || !g_vaValid[hand] || g_vaFrame[hand] != 1) return false;   // needs a palm attach
    out->part = part;
    memcpy(out->pos, g_vaPos[hand], sizeof(out->pos)); memcpy(out->q, g_vaQ[hand], sizeof(out->q));
    out->flip = g_vaFlip[hand];
    const int e = part ? ElVitalsMana : ElVitalsHealth;
    float w = g_hand[hand].widthM * g_el[e].handScale;
    if (g_vitalsAutoCrop) {
        float r[4], hp[3];
        const int s = g_elementSink[ElVitals];
        if (s >= 0 && vitals_part(s, part, r, hp)) {
            const float full = g_vitalsCrop[part][2] - g_vitalsCrop[part][0];
            if (full > 0) w *= (r[2] - r[0]) / full;
        }
    }
    out->widthM = w;
    memcpy(out->trimCm, g_vsTrim[hand], sizeof(out->trimCm));
    return true;
}
void set_hand_palm_pose(int hand, const float p[3], const float q[4], bool flipped) {
    if (hand < 0 || hand > 1) return;
    for (int k = 0; k < 3; ++k) if (!std::isfinite(p[k])) return;
    for (int k = 0; k < 4; ++k) if (!std::isfinite(q[k])) return;
    AcquireSRWLockExclusive(&g_palmLock);
    memcpy(g_palmPos[hand], p, 3 * sizeof(float)); memcpy(g_palmQ[hand], q, 4 * sizeof(float));
    g_palmMs[hand] = GetTickCount64();
    g_palmFlip[hand] = flipped;
    ReleaseSRWLockExclusive(&g_palmLock);
}
bool force_capture_alpha(int sink) {
    return alpha_for_sink(sink).mode!=AlphaRepair || (wheel_parts_for_sink(sink) && wheel_parts_alpha().mode!=AlphaRepair);
}
void circle_for_sink(int sink,uint32_t width,uint32_t height,float ellipse[4]) {
    memset(ellipse,0,4*sizeof(float));
    if(g_dialOn && g_dialCircle && g_visualRiding && g_ridingContext==6 &&
       sink>=0 && sink<kMaxSinks && g_sink[sink].anchor==g_el[ElWheel].anchor && !g_sink[sink].crop) {
        ellipse[0]=ellipse[1]=.5f;
        if(!width || !height) return;
        const float radius=.5f*fminf(g_dialCropX*width,g_dialCropY*height);
        ellipse[2]=radius/width; ellipse[3]=radius/height; // round in pixels/metres, not just UVs
    }
}

void wheel_input(bool held, bool permitted, float& x, float& y, bool& handSelected) {
    dvr::vr::HeadPose head{};
    float hp[3]{}, hq[4]{};
    const bool tracked = dvr::vr::peek_head_pose(head) &&
        dvr::vr::input_get_hand_pose(0, false, hp, hq);
    const float eye[3] = {head.px, head.py, head.pz};
    const float hqCamera[4] = {head.qx,head.qy,head.qz,head.qw};
    const bool was = g_dial.held;
    handSelected = g_dial.update(held && permitted && g_dialOn, tracked, hp, eye,
                                g_dialRadius, g_dialDeadM, x, y, hqCamera, g_dialDirection,g_dialEntryTilt,g_dialEntryYaw);
    if (!was && g_dial.held) {
        g_dialVisual.reset();
        const float f[3]={0,0,-1};
        const auto* q=g_dial.opening.q;
        dvr::xrmath::quat_rotate(q[0],q[1],q[2],q[3],f,g_dialForward);
    }
    if(g_dial.held) {if(g_dial.valid) g_dialVisual=g_dial;else g_dialVisual.reset();}
    if (was != g_dial.held)
        DVR_INFO("hud/dial: %s tracked=%d valid=%d center=(%.3f %.3f %.3f) width=%.3f radius=%.3f crop=%.3fx%.3f",
            g_dial.held ? "open" : "close", (int)tracked, (int)g_dial.valid,
            g_dial.center[0],g_dial.center[1],g_dial.center[2],g_dialWidth,g_dialRadius,g_dialCropX,g_dialCropY);
}


// ---- routing --------------------------------------------------------------

int sink_for(const float* bbox, int* elementOut, uint64_t drawKey, unsigned vertices, unsigned primitives, float* nativePivot) {
    if(native_gameplay_reference()) {
        if(elementOut) *elementOut=-1;
        ++g_routeFrame;
        DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,2000,
            "hud/native-reference: gameplay draw left in game image; capture/alpha/objective transforms bypassed frame=%u",(unsigned)dvr::frame::count());
        return -1;
    }
    if(nativePivot && bbox) memcpy(nativePivot,bbox,4*sizeof(float));
    hudroute::Identity id;
    id.context = g_visualRiding ? g_ridingContext : -1;
    id.hasRect = bbox != nullptr;
    if (bbox) memcpy(id.rect, bbox, sizeof(id.rect)); else memset(id.rect, 0, sizeof(id.rect));
    const int spatial = hudroute::route(g_rows, ElCount, id, ElDefault);
    const uint32_t drawFrame=(uint32_t)dvr::frame::count();
    const bool isolatedIcon=id.context<0 && g_nativeObjectives && bbox &&
        dvr::hudnative::square_icon(bbox,vertices,primitives) && !g_interactionGroup.near_group(bbox,drawFrame);
    int e = id.context >= 0 ? spatial : g_stableRoutes.resolve(drawKey, drawFrame,
        isolatedIcon && spatial==ElPrompt ? ElDefault : spatial,bbox);
    if(id.context<0 && bbox) {
        float runePivot[4]{};
        const bool runeDraw=g_nativeObjectives && dvr::objectivemarkers::match_rune_draw(
            bbox,(float)dvr::capture::width(),(float)dvr::capture::height(),runePivot);
        const bool runeBridge=g_nativeObjectives && dvr::objectivemarkers::rune_ownership() &&
            dvr::objectivemarkers::rune_enabled() && g_runeIconContinuity.route(
                drawKey,drawFrame,GetTickCount(),bbox,vertices,primitives,runeDraw);
        if(runeBridge && !runeDraw) {
            // This draw is current. Preserve native ownership, not an old pose.
            runePivot[0]=runePivot[2]=(bbox[0]+bbox[2])*.5f;
            runePivot[1]=runePivot[3]=(bbox[1]+bbox[3])*.5f;
            static uint32_t bridges=0;++bridges;
            DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,1000,
                "hud/rune-continuity: bridges=%u key=%016llx frame=%u; same confirmed icon, at most two frames/100ms, current draw center",
                bridges,drawKey,drawFrame);
        }
        if(runeDraw || runeBridge) {
            if(nativePivot)memcpy(nativePivot,runePivot,sizeof(runePivot));
            if(elementOut)*elementOut=ElObjective;
            ++g_routeFrame;++g_routeCounts[ElObjective];++g_seen[ElObjective];
            DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,1000,
                "hud/native-rune: rect=%.3f/%.3f/%.3f/%.3f pivot=%.3f/%.3f key=%016llx; source=%s",
                bbox[0],bbox[1],bbox[2],bbox[3],runePivot[0],runePivot[1],drawKey,runeDraw?"live parent":"confirmed icon continuity");
            return -1;
        }
        // Group decisions outrank the first spatial hint retained by the old
        // cache, otherwise title and action can stay split for their lifetime.
        const bool icon=dvr::hudnative::square_icon(bbox,vertices,primitives);
        const bool nativeIcon=g_nativeObjectives &&
            g_nativeMarkers.observe(drawKey,drawFrame,bbox,vertices,primitives,
                dvr::objectivemarkers::enabled()?dvr::objectivemarkers::inset():.05f,
                dvr::objectivemarkers::rune_enabled()?dvr::objectivemarkers::rune_inset():.05f);
        if(nativeIcon) g_nativeLabels.marker(bbox,drawFrame);
        float labelPivot[4]{};
        const bool nativeLabel=g_nativeObjectives && g_nativeObjectiveLabels && !nativeIcon &&
            g_nativeLabels.label(bbox,drawFrame,labelPivot);
        bool nativeChild=false;
        if(g_nativeObjectives && g_nativeMarkerChildren && !nativeIcon) {
            if(g_nativeChildContent.known(drawKey,drawFrame)) {
                nativeChild=true;memcpy(labelPivot,bbox,sizeof(labelPivot));
            } else if(g_nativeLabels.child(bbox,drawFrame,labelPivot)) {
                nativeChild=true;g_nativeChildContent.remember(drawKey,drawFrame);
            }
        }
        if(nativeChild) DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,1000,
            "hud/native-child: key=%016llx vertices=%u primitives=%u rect=%.3f/%.3f/%.3f/%.3f; centered artwork shares marker pivot",
            drawKey,vertices,primitives,bbox[0],bbox[1],bbox[2],bbox[3]);
        if((nativeLabel || nativeChild) && nativePivot) memcpy(nativePivot,labelPivot,sizeof(labelPivot));
        if(nativeLabel) DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,1000,
            "hud/native-label: rect=%.3f/%.3f/%.3f/%.3f marker=%.3f/%.3f/%.3f/%.3f; proximity candidate, native shared pivot",
            bbox[0],bbox[1],bbox[2],bbox[3],labelPivot[0],labelPivot[1],labelPivot[2],labelPivot[3]);
        if(nativeIcon || nativeLabel || nativeChild || (!g_nativeObjectives && g_routeObjectives && hudroute::objective_shape(bbox,vertices,primitives))) {
            e=ElObjective;if(!nativeLabel && !nativeChild) g_stableRoutes.adopt(drawKey,drawFrame,e);
        } else if(g_groupInteractions && spatial!=ElVitals && spatial!=ElVignette &&
            // A title crossing the central region is not the reticle. Preserve
            // the native measured dot/grown reticle rather than adopting it.
            !hudroute::centered_reticle(bbox,primitives) &&
            g_interactionGroup.claim(bbox,drawFrame,(!g_nativeObjectives || !icon) && (spatial==ElPrompt || e==ElPrompt))) {
            e=ElPrompt;g_stableRoutes.adopt(drawKey,drawFrame,e);
        }
    }
    if(e != spatial) DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,2000,
        "hud/owner: routed %s instead of positional %s; key=%016llx rect=%.3f/%.3f/%.3f/%.3f verts=%u prims=%u",
        kRows[e].name,kRows[spatial].name,drawKey,bbox?bbox[0]:0,bbox?bbox[1]:0,bbox?bbox[2]:0,bbox?bbox[3]:0,vertices,primitives);
    if (!bbox && !g_menuRiding) ++g_routeNoRegion;
    if (elementOut) *elementOut = e;
    ++g_routeCounts[e]; ++g_seen[e];
    g_lastRouted[e] = g_presentNo;
    int anchor = g_el[e].anchor;
    if(g_nativeObjectives && e==ElObjective && id.context<0) {++g_routeFrame;return -1;}
    if(g_nativeObjectives && id.context<0 && isolatedIcon && spatial!=ElVitals && e!=ElPrompt &&
       !hudroute::centered_reticle(bbox,primitives)) {if(elementOut) *elementOut=ElDefault;++g_routeFrame;return -1;}
    if (anchor == AnchorFrame) { ++g_routeFrame; return -1; }
    // This topology includes observed objective artwork, but is not semantic
    // identity. Record misses without stealing unrelated prompts from panels.
    if(g_nativeObjectives && id.context<0 && e!=ElObjective && bbox && vertices==8 && primitives==10)
        DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,1000,
            "hud/native-miss: topology candidate routed=%s positional=%s anchor=%s key=%016llx rect=%.3f/%.3f/%.3f/%.3f nearInteraction=%d; not confirmed objective",
            kRows[e].name,kRows[spatial].name,kAnchorNames[g_el[e].anchor],drawKey,
            bbox[0],bbox[1],bbox[2],bbox[3],(int)g_interactionGroup.near_group(bbox,drawFrame));
    if (anchor == AnchorOff) anchor = AnchorOff;   // a hidden sink: redirected, never delivered
    const bool crop = crop_eligible(e);
    int s = crop ? g_elementSink[e] : g_sinkOf[anchor][0];
    if (s < 0) s = acquire_sink(anchor, crop, crop ? e : -1);
    if (s < 0) {                    // out of sinks: it rides default's catch-all
        ++g_routeOverflow;
        const int da = g_el[ElDefault].anchor;
        s = anchor_visible(da) || da == AnchorOff ? acquire_sink(da, false) : -1;
        if (elementOut) *elementOut = ElDefault;
    }
    return s;
}

bool sink_in_use(int sink) { return sink >= 0 && sink < kMaxSinks && g_sink[sink].anchor >= 0; }
bool sink_hidden(int sink) { return sink_in_use(sink) && g_sink[sink].anchor == AnchorOff; }
const char* sink_label(int sink) { return sink_in_use(sink) ? g_sinkLabel[sink] : "free"; }
int sink_anchor(int sink) { return sink_in_use(sink) ? g_sink[sink].anchor : -1; }

// ---- the provider ---------------------------------------------------------

namespace {

void place(dvr::vr::HudQuadDesc& d, int e, int anchor, const float rect[4], float aspect, bool wholeSink) {
    const ElementCfg& c = g_el[e];
    const float rw = rect[2] - rect[0];
    // The whole texture spans the anchor's width; an element's centre sits at
    // its normalised offset times that width (and the texture's aspect for the
    // vertical), which is "where it is on the screen".
    const float cxN = (rect[0] + rect[2]) * 0.5f - 0.5f;             // -0.5..0.5, + = right
    const float cyN = (0.5f - (rect[1] + rect[3]) * 0.5f) * aspect;  // + = up, in widths
    if (anchor_is_hand(anchor)) {
        // A hand panel is a wrist HUD: the element fills the panel's width and
        // sits AT the hand (plus its own offset), not where it sat on the
        // screen. Measured before this rule: the vitals crop came out 0.044 m
        // wide and 0.16 m up-left of the grip, the reticle 0.013 m.
        const int h = anchor == AnchorHandR ? 1 : 0;
        d.anchor = dvr::vr::HudAnchor::Hand;
        d.hand = h;
        d.base[0] = g_hand[h].x; d.base[1] = g_hand[h].y; d.base[2] = g_hand[h].z;
        d.lift = g_hand[h].liftM;
        d.orient = g_hand[h].followGrip ? dvr::vr::HudOrient::FollowGrip : dvr::vr::HudOrient::Billboard;
        d.tiltDeg = g_hand[h].tiltDeg;
        d.spinDeg = g_hand[h].spinDeg;
        if(g_vitalsBack && (e==ElVitalsHealth || e==ElVitalsMana)) {   // VR-142: on the back of the hand
            const float side=h ? -1.f : 1.f;   // the back of the hand faces grip -X on the right, +X on the left
            d.base[0]=side*g_vitalsBackCfg[0]; d.base[1]=g_vitalsBackCfg[1]; d.base[2]=g_vitalsBackCfg[2];
            d.lift=0; d.orient=dvr::vr::HudOrient::FollowGrip;
            d.tiltDeg=g_vitalsBackCfg[3]; d.spinDeg=h ? g_vitalsBackCfg[4] : -g_vitalsBackCfg[4];
        }
        d.width = g_hand[h].widthM * (wholeSink ? rw : 1.0f) * c.handScale;
        d.height = 0.0f;
        d.planeOff[0] = (wholeSink ? cxN * g_hand[h].widthM : 0.0f) + c.handX;
        d.planeOff[1] = (wholeSink ? cyN * g_hand[h].widthM : 0.0f) + c.handY;
        const int vaPart=e==ElVitalsHealth?0:e==ElVitalsMana?1:-1;
        if(vaPart>=0 && g_vaStart) {   // VR-142: the attach countdown: frozen in front of the head
            d.anchor=dvr::vr::HudAnchor::LocalBillboard; d.orient=dvr::vr::HudOrient::OpeningPlane;
            memcpy(d.base,g_vaPanelPos[vaPart],sizeof(d.base)); memcpy(d.orientation,g_vaPanelQ,sizeof(d.orientation));
            d.planeOff[0]=d.planeOff[1]=0; d.lift=0;
        } else if(vaPart>=0 && g_vaOn && g_vaValid[h] && g_vaFrame[h]==0) {   // an older capture, in the grip frame
            d.orient=dvr::vr::HudOrient::GripLocal; d.lift=0;
            memcpy(d.base,g_vaPos[h],sizeof(d.base)); memcpy(d.orientation,g_vaQ[h],sizeof(d.orientation));
            d.planeOff[0]=d.planeOff[1]=0;
        } else if(vaPart>=0 && g_vaOn && g_vaValid[h] && g_vaFrame[h]==1) {   // on the drawn palm (the caller checked it is fresh)
            float pp[3],pq[4],r[3];
            if(hand_palm_pose(h,pp,pq)) {
                dvr::xrmath::quat_rotate(pq[0],pq[1],pq[2],pq[3],g_vaPos[h],r);
                for(int k=0;k<3;++k) d.base[k]=pp[k]+r[k];
                dvr::xrmath::quat_mul(pq,g_vaQ[h],d.orientation);
                d.anchor=dvr::vr::HudAnchor::LocalBillboard; d.orient=dvr::vr::HudOrient::OpeningPlane;
                d.lift=0; d.planeOff[0]=d.planeOff[1]=0;
            }
        }
    } else {
        d.anchor = anchor == AnchorWorld ? dvr::vr::HudAnchor::WindowWorld : dvr::vr::HudAnchor::Window;
        d.base[0] = g_win.latM; d.base[1] = g_win.upM; d.base[2] = -g_win.distM;
        d.width = g_win.widthM * rw * c.winScale;
        d.height = wholeSink ? g_win.heightM : 0.0f;
        d.planeOff[0] = cxN * g_win.widthM + c.winX;
        d.planeOff[1] = cyN * g_win.widthM + c.winY;
    }
}

} // namespace

// The attach step's clock, on the present thread: at the end of the countdown
// each part's frozen panel pose is taken into its hand's grip frame.
void vitals_attach_tick() {
    if(!g_vaStart || GetTickCount64()-g_vaStart < (unsigned long long)(g_vaSeconds*1000.f)) return;
    g_vaStart=0;
    for(int part=0;part<2;++part) {
        const int h=vitals_hand(part);
        if(h<0) { DVR_WARN("hud/vitals-attach: %s is not on a hand anchor - nothing captured for it",kVitalsPartNames[part]); continue; }
        float gp[3],gq[4];
        int frame=1;
        if(!hand_palm_pose(h,gp,gq)) {
            frame=0;
            if(!dvr::vr::input_get_hand_pose(h,false,gp,gq)) {
                DVR_WARN("hud/vitals-attach: the %s hand was neither drawn nor tracked at the end of the countdown - its previous attachment stands",h?"right":"left");
                continue;
            }
            DVR_WARN("hud/vitals-attach: the %s palm was not being drawn - captured against the controller grip instead (it will not follow stance or animation)",h?"right":"left");
        }
        float inv[4]; dvr::xrmath::quat_conj(gq,inv);
        const float rel[3]={g_vaPanelPos[part][0]-gp[0],g_vaPanelPos[part][1]-gp[1],g_vaPanelPos[part][2]-gp[2]};
        dvr::xrmath::quat_rotate(inv[0],inv[1],inv[2],inv[3],rel,g_vaPos[h]);
        dvr::xrmath::quat_mul(inv,g_vaPanelQ,g_vaQ[h]);
        g_vaValid[h]=true; g_vaFrame[h]=frame; g_vaFlip[h]=frame && g_palmFlip[h];
        char v[160];
        _snprintf(v,sizeof(v),"%.4f,%.4f,%.4f,%.6f,%.6f,%.6f,%.6f%s",g_vaPos[h][0],g_vaPos[h][1],g_vaPos[h][2],g_vaQ[h][0],g_vaQ[h][1],g_vaQ[h][2],g_vaQ[h][3],frame?(g_vaFlip[h]?",palmF":",palm"):"");
        v[sizeof(v)-1]=0; write_key(kVaKeys[h],v);
        DVR_INFO("hud/vitals-attach: %s captured on the %s %s: offset %.3f/%.3f/%.3f m (%.3f m away), rotation %s",
            kVitalsPartNames[part],h?"right":"left",frame?"DRAWN PALM":"grip",g_vaPos[h][0],g_vaPos[h][1],g_vaPos[h][2],
            sqrtf(g_vaPos[h][0]*g_vaPos[h][0]+g_vaPos[h][1]*g_vaPos[h][1]+g_vaPos[h][2]*g_vaPos[h][2]),v);
    }
    g_vaOn=g_vaValid[0] || g_vaValid[1];
    write_i("VitalsAttach",g_vaOn);
}
void vitals_attach_start() {
    dvr::vr::HeadPose head{};
    if(!dvr::vr::peek_head_pose(head)) { DVR_WARN("hud/vitals-attach: no head pose - not started"); return; }
    const float q[4]={head.qx,head.qy,head.qz,head.qw};
    const float fwd0[3]={0,0,-1},right0[3]={1,0,0},up0[3]={0,1,0};
    float f[3],r[3],u[3];
    dvr::xrmath::quat_rotate(q[0],q[1],q[2],q[3],fwd0,f);
    dvr::xrmath::quat_rotate(q[0],q[1],q[2],q[3],right0,r);
    dvr::xrmath::quat_rotate(q[0],q[1],q[2],q[3],up0,u);
    for(int part=0;part<2;++part) {
        const float side=vitals_hand(part)==0 ? -1.f : 1.f;   // the part's own hand's side
        for(int k=0;k<3;++k) g_vaPanelPos[part][k]=(&head.px)[k]+f[k]*.40f+r[k]*side*.12f-u[k]*.15f;
    }
    memcpy(g_vaPanelQ,q,sizeof(g_vaPanelQ));   // facing the head as it was at the press
    g_vaStart=GetTickCount64();
    DVR_INFO("hud/vitals-attach: countdown %.0f s - the panels are frozen 0.40 m ahead; hold each hand where its panel should ride",g_vaSeconds);
}

int provide(ID3D11DeviceContext* ctx, dvr::vr::HudQuadDesc* out, int max) {
    ++g_presentNo;
    vitals_attach_tick();
    int n = 0;
    if(native_gameplay_reference()) return 0; // no delayed panel can overlap the reference
    // The measured elements: one isolated full-texture quad each, preserving
    // its reference region's placement while allowing motion outside it.
    // Only while its draws keep arriving (the sink delivers the previous
    // present's slot, so two presents of grace).
    for (int e = 0; e < ElCount && n < max; ++e) {
        const int a = g_el[e].anchor;
        if (!anchor_visible(a) || !crop_eligible(e) || (g_nativeObjectives && e==ElObjective)) continue;
        if (g_presentNo - g_lastRouted[e] > 2) continue;
        const int s = g_elementSink[e];
        if (s < 0) continue;
        ID3D11Texture2D* tex = dvr::hudcap::sink_texture(s, ctx);
        if (!tex) continue;
        D3D11_TEXTURE2D_DESC td{};
        tex->GetDesc(&td);
        const float aspect = td.Width ? (float)td.Height / (float)td.Width : 1.0f;
        if(e==ElVitals && g_vitalsSplit && !g_visualRiding) {   // VR-142: two panels instead of one
            for(int part=0;part<2 && n<max;++part) {
                const int pe=part?ElVitalsMana:ElVitalsHealth;
                if(vitals_scene_on()) {   // drawn in the game frame on the hand instead
                    const int ph=vitals_hand(part);
                    if(ph>=0 && g_vaValid[ph] && g_vaFrame[ph]==1) continue;
                }
                const int pa=g_el[pe].anchor;if(!anchor_visible(pa)) continue;
                ID3D11Texture2D* partTex=dvr::hudcap::vitals_part_texture(s,part);
                if(!partTex) continue;
                {
                    const int ph=pa==AnchorHandL?0:pa==AnchorHandR?1:-1;float tp[3],tq[4];
                    if(ph>=0 && !g_vaStart && g_vaOn && g_vaValid[ph] && g_vaFrame[ph]==1 && !hand_palm_pose(ph,tp,tq)) {
                        DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,5000,
                            "hud/vitals-attach: the %s palm is not being drawn - its panel is hidden until it is",ph?"right":"left");
                        continue;
                    }
                }
                float c[4],hp[3];
                if(!vitals_part(s,part,c,hp)) continue;
                if(part==1 && g_vitalsMirror) {   // mana takes health's placement, mirrored across the body
                    g_el[ElVitalsMana].handX=-g_el[ElVitalsHealth].handX;g_el[ElVitalsMana].handY=g_el[ElVitalsHealth].handY;
                    g_el[ElVitalsMana].handScale=g_el[ElVitalsHealth].handScale;
                    g_el[ElVitalsMana].winX=-g_el[ElVitalsHealth].winX;g_el[ElVitalsMana].winY=g_el[ElVitalsHealth].winY;
                    g_el[ElVitalsMana].winScale=g_el[ElVitalsHealth].winScale;
                }
                auto& panel=out[n++];panel=dvr::vr::HudQuadDesc{};
                panel.tex=partTex;panel.element=pe;panel.slot=pe;
                const float whole[4]={0,0,1,1};
                memcpy(panel.subrect,whole,sizeof(panel.subrect));
                place(panel,pe,pa,whole,aspect,false);
                if(!anchor_is_hand(pa)) panel.width=g_win.widthM*(c[2]-c[0])*g_el[pe].winScale;
                else if(g_vitalsAutoCrop) {   // keep pixel scale: a trimmed part is narrower, not stretched
                    const float full=g_vitalsCrop[part][2]-g_vitalsCrop[part][0];
                    if(full>0) panel.width*=(c[2]-c[0])/full;
                }
                panel.height=0;
                ++g_seen[pe];
            }
            DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,5000,
                "hud/vitals-split: health on %s, mana on %s (line top=%.3f bottom=%.3f); the whole vitals quad is replaced",
                kAnchorNames[g_el[ElVitalsHealth].anchor],kAnchorNames[g_el[ElVitalsMana].anchor],g_vitalsLine[0],g_vitalsLine[1]);
            continue;
        }
        if(e==ElObjective && g_objectiveScreen && !anchor_is_hand(a)) {
            const auto* regions=dvr::hudcap::marker_regions(s);
            float th=0,tv=0;int source=0;unsigned sw=0,sh=0;
            dvr::vr::fov_audit(&th,&tv,&source,&sw,&sh);
            const float dist=g_win.distM;
            int reserve=0;
            for(int later=e+1;later<ElCount;++later)
                if(anchor_visible(g_el[later].anchor) && crop_eligible(later) &&
                   g_presentNo-g_lastRouted[later]<=2 && g_elementSink[later]>=0) ++reserve;
            for(int anchor=AnchorWindow;anchor<AnchorCount;++anchor) if(g_sinkOf[anchor][0]>=0) ++reserve;
            if(regions && dvr::hudmarker::separable(*regions,td.Width,td.Height) && regions->count>0 &&
               n+regions->count+reserve<=max && th>0 && tv>0 && dist>.05f) {
                for(int i=0;i<regions->count;++i) {
                    auto& marker=out[n++];marker=dvr::vr::HudQuadDesc{};
                    marker.tex=tex;marker.element=e;
                    marker.slot=i==0 ? ElObjective : ElCount+AnchorCount+i-1;
                    static_assert(ElCount+AnchorCount+dvr::hudmarker::kMax-1<=dvr::vr::kMaxHudQuads,"marker slots must be disjoint");
                    marker.anchor=dvr::vr::HudAnchor::Window;
                    marker.base[0]=marker.base[1]=0;marker.base[2]=-dist;
                    // Two source pixels of padding avoid trimming antialiased edges.
                    const float* r=regions->rect[i];
                    dvr::hudmarker::cropped_placement(r,td.Width,td.Height,dist,th,tv,g_win.widthM*g_el[e].winScale,
                                                       marker.subrect,marker.planeOff,marker.width);
                    marker.height=0;
                }
                DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,3000,
                    "hud/objective: %d image-owned marker crops, frustum=%.3f/%.3f sizeScale=%.3f; native clamp retained",
                    regions->count,th,tv,g_el[e].winScale);
                continue;
            }
            DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,3000,
                "hud/objective: complete panel fallback (count=%d overflow=%d frustum=%.3f/%.3f)",
                regions?regions->count:0,regions?regions->overflow:0,th,tv);
        }
        dvr::vr::HudQuadDesc& d = out[n++];
        d = dvr::vr::HudQuadDesc();
        d.tex = tex; d.element = e; d.slot = e;
        const float whole[4]={0,0,1,1};
        memcpy(d.subrect,whole,sizeof(d.subrect));
        const float* reference=measured(e) ? g_el[e].rect : whole;
        place(d, e, a, reference, aspect, !measured(e));
        // Expand the isolated texture around the same reference rectangle.
        // Preserve pixel scale/placement while allowing this element to move
        // outside its original identification region without clipping.
        dvr::hudanchor::expand_reference_panel(reference,aspect,d.width,d.planeOff);
        d.height=0;
    }
    // The catch-all sinks: one whole-sink quad per anchor in use, placed by
    // the riding screen's row while a screen rides, else by `default`.
    for (int a = AnchorWindow; a < AnchorCount && n < max; ++a) {
        const int s = g_sinkOf[a][0];
        if (s < 0) continue;
        ID3D11Texture2D* tex = dvr::hudcap::sink_texture(s, ctx);
        if (!tex) continue;
        D3D11_TEXTURE2D_DESC td{};
        tex->GetDesc(&td);
        const float aspect = td.Width ? (float)td.Height / (float)td.Width : 1.0f;
        int e = ElDefault;
        if (g_visualRiding) { const int se = element_for_context(g_ridingContext); if (se >= 0 && g_el[se].anchor == a) e = se; }
        const float whole[4] = {0, 0, 1, 1};
        dvr::vr::HudQuadDesc& d = out[n++];
        d = dvr::vr::HudQuadDesc();
        d.tex = tex; d.element = e; d.slot = ElCount + a;
        memcpy(d.subrect, whole, sizeof(d.subrect));
        place(d, e, a, whole, aspect, true);
        const int readPanel=e==ElNote?0:e==ElJournal?1:-1;
        if(readPanel>=0 && g_readHand[readPanel]) {
            float hp[3],hq[4],attached[4],page[4];
            if(!dvr::vr::input_get_hand_pose(0,false,hp,hq) ||
               !dvr::hudanchor::reading_grip_reference(hq,attached,page)) {--n;continue;}
            d.anchor=dvr::vr::HudAnchor::LocalBillboard;d.hand=0;
            d.orient=dvr::vr::HudOrient::OpeningPlane;
            dvr::hudanchor::camera_panel_position(hp,attached,g_readDistance[readPanel],d.base);
            const float offset[3]={g_readRight[readPanel],g_readUp[readPanel],0};float worldOffset[3];
            dvr::xrmath::quat_rotate(attached[0],attached[1],attached[2],attached[3],offset,worldOffset);
            for(int k=0;k<3;++k)d.base[k]+=worldOffset[k];
            dvr::hudanchor::reading_alignment(page,g_readTilt,d.orientation);
            DVR_LOG_EVERY_MS(DVR_CAT,::dvr::log::Level::Info,1000,
                "hud/reading-pose: reference=fixed425 panel=%s center=%.4f/%.4f/%.4f gripQ=%.6f/%.6f/%.6f/%.6f panelQ=%.6f/%.6f/%.6f/%.6f manual=%.3f",
                kReadNames[readPanel],d.base[0],d.base[1],d.base[2],hq[0],hq[1],hq[2],hq[3],
                d.orientation[0],d.orientation[1],d.orientation[2],d.orientation[3],g_readTilt);
            d.width=g_readWidth[readPanel];d.height=0;
            d.planeOff[0]=d.planeOff[1]=0;
        }
        if (e == ElWheel && g_dialOn) {
            if (!g_dialVisual.valid) { --n; continue; }
            d.anchor = dvr::vr::HudAnchor::LocalBillboard;
            d.orient = dvr::vr::HudOrient::OpeningPlane; d.hand = 0;
            memcpy(d.orientation,g_dialVisual.opening.q,sizeof(d.orientation));
            for(int k=0;k<3;++k) d.base[k]=g_dialVisual.center[k]+g_dialForward[k]*g_dialDistance;
            d.width = g_dialWidth; d.height = 0;
            d.planeOff[0] = d.planeOff[1] = 0;
            // Wheel ring measured [0.226,.275 - .774,.716] in ENGINE_NOTES.
            // Margin is adjustable for other aspect ratios and inventories.
            d.subrect[0] = .5f - g_dialCropX*.5f;
            d.subrect[2] = .5f + g_dialCropX*.5f;
            d.subrect[1] = .5f - g_dialCropY*.5f;
            d.subrect[3] = .5f + g_dialCropY*.5f;
            for(int part=0;part<2 && n<max;++part) {
                const int pe=part?ElWheelPotions:ElWheelShortcuts;
                const int pa=g_el[pe].anchor;if(!anchor_visible(pa)) continue;
                ID3D11Texture2D* partTex=dvr::hudcap::wheel_part_texture(s,part);
                if(!partTex) continue;
                auto& panel=out[n++];panel=dvr::vr::HudQuadDesc{};
                panel.tex=partTex;panel.element=pe;panel.slot=pe;
                place(panel,pe,pa,whole,aspect,false);
                panel.width=(part?.28f:.23f)*(anchor_is_hand(pa)?g_el[pe].handScale:g_el[pe].winScale);
                panel.height=0;
                ++g_seen[pe];
            }
        }
    }
    return n;
}

// ---- persistence ----------------------------------------------------------

void configure(const char* ini) {
    strncpy_s(g_ini, ini ? ini : "", _TRUNCATE);
    for (int s = 0; s < kMaxSinks; ++s) { g_sink[s].anchor = -1; g_sink[s].crop = false; g_sink[s].rideOnly = false; g_sink[s].element=-1; g_sinkLabel[s][0] = 0; }
    for (int a = 0; a < AnchorCount; ++a) g_sinkOf[a][0] = g_sinkOf[a][1] = -1;
    for(int e=0;e<ElCount;++e) g_elementSink[e]=-1;
    g_stableRoutes.clear();g_interactionGroup.clear();g_nativeMarkers.clear();g_nativeChildContent.clear();g_runeIconContinuity.clear();g_nativeLabels.clear();
    memset(g_seen, 0, sizeof(g_seen));
    memset(g_lastRouted, 0, sizeof(g_lastRouted));
    // VR-117's keys, read once and rewritten on the next save: one hand
    // (HandHand + Hand*), one window mode (WindowAnchor), the elements all,
    // health, mana and menu.
    const int legacyHand = read_i(ini, "HandHand", -1);
    char v[96] = "";
    const bool legacyWorld = read_s(ini, "WindowAnchor", v, sizeof(v)) && !_stricmp(v, "world");
    int legacyAll = -1, legacyMenu = -1, legacyHealth = -1;
    if (read_s(ini, "Element.all", v, sizeof(v))) legacyAll = anchor_from_name(v);
    if (read_s(ini, "Element.menu", v, sizeof(v))) legacyMenu = anchor_from_name(v);
    if (read_s(ini, "Element.health", v, sizeof(v))) legacyHealth = anchor_from_name(v);
    auto legacy_anchor = [&](int a) -> int {
        if (a == AnchorHandL && legacyHand == 1) return AnchorHandR;
        if (a == AnchorWindow && legacyWorld) return AnchorWorld;
        return a;
    };
    if (legacyHand >= 0 || legacyWorld || legacyAll >= 0 || legacyMenu >= 0)
        DVR_INFO("hud/layout: VR-117 keys found in [Hud] (HandHand=%d WindowAnchor=%s Element.all/menu/health) - read once and "
                 "mapped onto the per-element anchors; the next save writes the new keys", legacyHand, legacyWorld ? "world" : "view");
    for (int e = 0; e < ElCount; ++e) {
        ElementCfg c = { kRows[e].presetAnchor, 0, 0, 1, 0, 0, 1, {0, 0, 0, 0} };
        if(e==ElWheelShortcuts || e==ElWheelPotions) {c.winX=e==ElWheelShortcuts?-.32f:.32f;c.winY=-.22f;}
        memcpy(c.rect, kRows[e].rect, sizeof(c.rect));
        if (e == ElDefault && legacyAll >= 0) c.anchor = legacy_anchor(legacyAll);
        else if (element_is_screen(e) && legacyMenu >= 0) c.anchor = legacy_anchor(legacyMenu);
        else if (e == ElVitals && legacyHealth >= 0) c.anchor = legacy_anchor(legacyHealth);
        char key[64];
        _snprintf(key, sizeof(key), "Element.%s", kRows[e].name); key[63] = 0;
        if (read_s(ini, key, v, sizeof(v))) {
            const int a = anchor_from_name(v);
            if (a >= 0) c.anchor = legacy_anchor(a);
            else DVR_WARN("hud/layout: [Hud] %s=%s is not off|frame|window|world|handL|handR; the preset (%s) stands",
                          key, v, kAnchorNames[c.anchor]);
        }
        _snprintf(key, sizeof(key), "Element.%s.WinX", kRows[e].name);      c.winX = read_f(ini, key, c.winX);
        _snprintf(key, sizeof(key), "Element.%s.WinY", kRows[e].name);      c.winY = read_f(ini, key, c.winY);
        _snprintf(key, sizeof(key), "Element.%s.WinScale", kRows[e].name);  c.winScale = read_f(ini, key, c.winScale);
        _snprintf(key, sizeof(key), "Element.%s.HandX", kRows[e].name);     c.handX = read_f(ini, key, c.handX);
        _snprintf(key, sizeof(key), "Element.%s.HandY", kRows[e].name);     c.handY = read_f(ini, key, c.handY);
        _snprintf(key, sizeof(key), "Element.%s.HandScale", kRows[e].name); c.handScale = read_f(ini, key, c.handScale);
        _snprintf(key, sizeof(key), "Region.%s", kRows[e].name); key[63] = 0;
        if (read_s(ini, key, v, sizeof(v))) {
            float r[4] = {};
            if (sscanf(v, "%f,%f,%f,%f", &r[0], &r[1], &r[2], &r[3]) == 4 && r[2] > r[0] && r[3] > r[1])
                memcpy(c.rect, r, sizeof(c.rect));
            else if (!strcmp(v, "0,0,0,0") || !strcmp(v, "0.000,0.000,0.000,0.000"))
                memset(c.rect, 0, sizeof(c.rect));   // an explicit "unmeasured"
            else
                DVR_WARN("hud/layout: [Hud] %s=%s is not x0,y0,x1,y1 with x1>x0 and y1>y0; the compiled region stands", key, v);
        }
        g_el[e] = c;
    }
    rebuild_rows();
    WindowCfg w = kPresetWindow;
    w.distM = read_f(ini, "WindowDistance", w.distM);
    w.widthM = read_f(ini, "WindowWidth", w.widthM);
    w.heightM = read_f(ini, "WindowHeight", w.heightM);
    w.upM = read_f(ini, "WindowUp", w.upM);
    w.latM = read_f(ini, "WindowLateral", w.latM);
    g_win = w;
    for (int k = 0; k < 2; ++k) {
        HandCfg h = kPresetHand;
        char key[64];
        const char* hn = kHandNames[k];
        // VR-117's single hand fills the hand it named.
        if (legacyHand >= 0 && (legacyHand ? 1 : 0) == k) {
            h.x = read_f(ini, "HandX", h.x); h.y = read_f(ini, "HandY", h.y); h.z = read_f(ini, "HandZ", h.z);
            h.liftM = read_f(ini, "HandLift", h.liftM);
            h.widthM = read_f(ini, "HandWidth", h.widthM);
            if (read_s(ini, "HandOrient", v, sizeof(v))) h.followGrip = !_stricmp(v, "grip");
            h.tiltDeg = read_f(ini, "HandTilt", h.tiltDeg);
        }
        _snprintf(key, sizeof(key), "%s.X", hn); h.x = read_f(ini, key, h.x);
        _snprintf(key, sizeof(key), "%s.Y", hn); h.y = read_f(ini, key, h.y);
        _snprintf(key, sizeof(key), "%s.Z", hn); h.z = read_f(ini, key, h.z);
        _snprintf(key, sizeof(key), "%s.Lift", hn); h.liftM = read_f(ini, key, h.liftM);
        _snprintf(key, sizeof(key), "%s.Width", hn); h.widthM = read_f(ini, key, h.widthM);
        _snprintf(key, sizeof(key), "%s.Orient", hn);
        if (read_s(ini, key, v, sizeof(v))) h.followGrip = !_stricmp(v, "grip");
        _snprintf(key, sizeof(key), "%s.Tilt", hn); h.tiltDeg = read_f(ini, key, h.tiltDeg);
        _snprintf(key, sizeof(key), "%s.Spin", hn); h.spinDeg = read_f(ini, key, h.spinDeg);
        g_hand[k] = h;
    }
    {   // VR-119
        AlphaCfg a = kPresetAlpha;
        if (read_s(ini, "AlphaMode", v, sizeof(v))) {
            const int m = alpha_mode_from_name(v);
            if (m >= 0) a.mode = m;
            else DVR_WARN("hud/layout: [Hud] AlphaMode=%s is not repair|captured|mix; repair stands", v);
        }
        a.gain = read_f(ini, "AlphaGain", a.gain);
        a.floorA = read_f(ini, "AlphaFloor", a.floorA);
        a.gamma = read_f(ini, "AlphaGamma", a.gamma);
        a.mixK = read_f(ini, "AlphaMix", a.mixK);
        g_alpha = a;
        for (int k = 0; k < 2; ++k) {
            char key[64];
            _snprintf(key, sizeof(key), "Backdrop.%s", kBackdropKindNames[k]); key[63] = 0;
            Backdrop d = kPresetBackdrop;
            if (read_s(ini, key, v, sizeof(v)) && sscanf(v, "%f,%f,%f,%f", &d.r, &d.g, &d.b, &d.a) != 4) {
                DVR_WARN("hud/layout: [Hud] %s=%s is not r,g,b,a; no plate", key, v);
                d = kPresetBackdrop;
            }
            g_backdrop[k] = d;
        }
    }
    uint32_t headMask=0,blurMask=0;
    for(int i=0;i<kMenuContexts;++i) {
        char key[64];
        _snprintf(key,sizeof(key),"HeadLook%s",kMenuContextNames[i]);
        if(read_i(ini,key,0)) headMask|=1u<<kMenuContextBits[i];
        _snprintf(key,sizeof(key),"NoBlur%s",kMenuContextNames[i]);
        if(read_i(ini,key,0)) blurMask|=1u<<kMenuContextBits[i];
    }
    g_groupInteractions=read_i(ini,"GroupInteractions",0)!=0;
    g_routeObjectives=read_i(ini,"RouteObjectives",0)!=0;
    g_objectiveScreen=read_i(ini,"ObjectiveScreenTracking",0)!=0;
    g_menuExitHeading.store(read_i(ini,"MenuExitHeading",0)!=0);
    g_pauseSceneFreshness.store(read_i(ini,"PauseSceneFreshness",0)!=0);
    dvr::objectivemarkers::configure(read_i(ini,"NativeTaskMarkers",0)!=0,read_f(ini,"TaskMarkerEdgeInset",.12f));
    dvr::objectivemarkers::configure_runes(read_i(ini,"NativeRuneMarkers",0)!=0,read_f(ini,"RuneMarkerEdgeInset",.12f));
    g_nativeObjectiveLabels=read_i(ini,"NativeObjectiveLabels",0)!=0;
    g_nativeObjectiveUpright=read_i(ini,"NativeObjectiveUpright",0)!=0;
    g_wheelCloseAnimation=read_i(ini,"WheelCloseAnimation",0)!=0;
    dvr::objectivemarkers::configure_rune_ownership(read_i(ini,"NativeRuneOwnership",0)!=0);
    g_nativeMarkerChildren=read_i(ini,"NativeMarkerChildren",0)!=0;
    g_nativeObjectives=read_i(ini,"NativeObjectiveIcons",0)!=0;
    g_nativeGameplayReference=read_i(ini,"NativeGameplayReference",0)!=0;
    g_wheelParts=read_i(ini,"WheelSidePanels",0)!=0;
    g_vitalsSplit=read_i(ini,"VitalsSplit",0)!=0;
    g_vitalsMirror=read_i(ini,"VitalsMirror",1)!=0;g_vitalsAutoCrop=read_i(ini,"VitalsAutoCrop",1)!=0;
    g_vitalsBack=read_i(ini,"VitalsBack",0)!=0;
    g_vaOn=read_i(ini,"VitalsAttach",0)!=0;
    g_vitalsScene=read_i(ini,"VitalsInScene",0)!=0;
    for(int h=0;h<2;++h) for(int k=0;k<3;++k) {
        char key[48];_snprintf(key,sizeof(key),"VitalsInScene.%s.Trim%d",h?"R":"L",k);g_vsTrim[h][k]=read_f(ini,key,0.f);
    }
    for(int h=0;h<2;++h) {
        char v[160]="";float p[7];
        g_vaValid[h]=read_s(ini,kVaKeys[h],v,sizeof(v)) &&
            sscanf(v,"%f,%f,%f,%f,%f,%f,%f",&p[0],&p[1],&p[2],&p[3],&p[4],&p[5],&p[6])==7;
        g_vaFrame[h]=strstr(v,"palm") ? 1 : 0;
        g_vaFlip[h]=strstr(v,"palmF")!=nullptr;
        if(g_vaValid[h]) {
            const float n=sqrtf(p[3]*p[3]+p[4]*p[4]+p[5]*p[5]+p[6]*p[6]);
            if(!(n>.5f && n<1.5f)) { g_vaValid[h]=false; continue; }
            for(int k=0;k<3;++k) g_vaPos[h][k]=p[k];
            for(int k=0;k<4;++k) g_vaQ[h][k]=p[3+k]/n;
        }
    }
    DVR_INFO("hud/vitals-attach: %s (left %s, right %s)",g_vaOn?"ON":"off",g_vaValid[0]?"captured":"none",g_vaValid[1]?"captured":"none");
    for(int k=0;k<5;++k) g_vitalsBackCfg[k]=read_f(ini,kVitalsBackKeys[k],g_vitalsBackCfg[k]);
    DVR_INFO("hud/vitals-split: mirror=%d autocrop=%d back-of-hand=%d (out %.3f along %.3f forward %.3f tilt %.0f spin %.0f)",
        g_vitalsMirror,g_vitalsAutoCrop,g_vitalsBack,g_vitalsBackCfg[0],g_vitalsBackCfg[1],g_vitalsBackCfg[2],g_vitalsBackCfg[3],g_vitalsBackCfg[4]);
    g_vitalsLine[0]=read_f(ini,"VitalsSplit.Top",g_vitalsLine[0]);g_vitalsLine[1]=read_f(ini,"VitalsSplit.Bottom",g_vitalsLine[1]);
    for(int part=0;part<2;++part) for(int k=0;k<4;++k) {
        char key[64];_snprintf(key,sizeof(key),"%s.Crop%d",kVitalsPartKeys[part],k);
        g_vitalsCrop[part][k]=read_f(ini,key,g_vitalsCrop[part][k]);
    }
    DVR_INFO("hud/vitals-split: %s line top=%.3f bottom=%.3f | health crop %.3f,%.3f,%.3f,%.3f on %s | mana crop %.3f,%.3f,%.3f,%.3f on %s",
        g_vitalsSplit?"ON":"off",g_vitalsLine[0],g_vitalsLine[1],
        g_vitalsCrop[0][0],g_vitalsCrop[0][1],g_vitalsCrop[0][2],g_vitalsCrop[0][3],kAnchorNames[g_el[ElVitalsHealth].anchor],
        g_vitalsCrop[1][0],g_vitalsCrop[1][1],g_vitalsCrop[1][2],g_vitalsCrop[1][3],kAnchorNames[g_el[ElVitalsMana].anchor]);
    for(int part=0;part<2;++part) for(int k=0;k<4;++k) {
        char key[64];_snprintf(key,sizeof(key),"%s.Crop%d",kWheelPartKeys[part],k);
        g_wheelPartCrop[part][k]=read_f(ini,key,g_wheelPartCrop[part][k]);
    }
    g_nativeObjectiveScale=fminf(1.f,fmaxf(.25f,read_f(ini,"NativeObjectiveScale",.70f)));
    g_menuHeadMask.store(headMask); g_menuBlurMask.store(blurMask);
    for(int i=0;i<5;++i) {
        auto& a=g_alphaBank.special[i];a=g_alpha;char key[64],mode[32];
        _snprintf(key,sizeof(key),"%sMode",kScopedAlpha[i]);
        if(read_s(ini,key,mode,sizeof(mode))) {const int m=alpha_mode_from_name(mode);if(m>=0) a.mode=m;}
        _snprintf(key,sizeof(key),"%sGain",kScopedAlpha[i]);a.gain=fminf(4,fmaxf(0,read_f(ini,key,a.gain)));
        _snprintf(key,sizeof(key),"%sFloor",kScopedAlpha[i]);a.floorA=fminf(1,fmaxf(0,read_f(ini,key,a.floorA)));
        _snprintf(key,sizeof(key),"%sGamma",kScopedAlpha[i]);a.gamma=fminf(4,fmaxf(.25f,read_f(ini,key,a.gamma)));
        _snprintf(key,sizeof(key),"%sMix",kScopedAlpha[i]);a.mixK=fminf(4,fmaxf(0,read_f(ini,key,a.mixK)));
    }
    for(int i=0;i<2;++i) {
        char key[64];
        _snprintf(key,sizeof(key),"%sFollowHand",kReadNames[i]);g_readHand[i]=read_i(ini,key,0)!=0;
        _snprintf(key,sizeof(key),"%sHandWidth",kReadNames[i]);g_readWidth[i]=fminf(1.5f,fmaxf(.15f,read_f(ini,key,g_readWidth[i])));
        _snprintf(key,sizeof(key),"%sHandDistance",kReadNames[i]);g_readDistance[i]=fminf(.5f,fmaxf(-.3f,read_f(ini,key,-.05f)));
        _snprintf(key,sizeof(key),"%sHandRight",kReadNames[i]);g_readRight[i]=fminf(.75f,fmaxf(-.75f,read_f(ini,key,.20f)));
    }
    const bool currentReference=read_i(ini,"ReadingTiltReference",0)==1;
    g_readTilt=dvr::hudanchor::reading_trim(read_f(ini,"ReadingTilt",currentReference?0.f:-31.f),currentReference);
    for(int i=0;i<2;++i){char key[64];_snprintf(key,sizeof(key),"%sHandUp",kReadNames[i]);
        const float up=read_f(ini,key,0);g_readUp[i]=std::isfinite(up)?fmaxf(-.75f,fminf(.75f,up)):0;}

    g_dialDistance=fminf(.50f,fmaxf(-.30f,read_f(ini,"WeaponDialDistance",0)));
    g_dialDirection = read_i(ini,"WeaponDialDirectionOnly",1)!=0;
    g_dialCircle = read_i(ini,"WeaponDialCircle",1)!=0;
    g_dialEntryTilt=read_i(ini,"WeaponDialEntryTilt",0)!=0;
    g_dialEntryYaw=read_i(ini,"WeaponDialEntryYaw",0)!=0;
    g_dialDeadM = fminf(.01f,fmaxf(.0005f,read_f(ini,"WeaponDialDeadzone",.002f)));
    g_dialOn = read_i(ini, "WeaponDial", 0) != 0;
    g_dialWidth = fminf(1.2f, fmaxf(.15f, read_f(ini,"WeaponDialWidth",.35f)));
    g_dialRadius = fminf(.30f, fmaxf(.04f, read_f(ini,"WeaponDialRadius",.04f)));
    g_dialCropX = fminf(1.f, fmaxf(.30f, read_f(ini,"WeaponDialCropX",.40f)));
    g_dialCropY = fminf(1.f, fmaxf(.30f, read_f(ini,"WeaponDialCropY",.40f)));
    g_dial.reset();
    g_menuInWindow = read_i(ini, "MenuInWindow", 1) != 0;
    uint32_t mask = 0;
    for (int i = 0; i < kMenuContexts; ++i) {
        char key[64]; _snprintf(key, sizeof(key), "Window%s", kMenuContextNames[i]); key[63] = 0;
        if (read_i(ini, key, 1)) mask |= 1u << kMenuContextBits[i];
    }
    g_menuMask = mask;
    // configure reads; only a change writes. The values are clamped by hand.
    rebalance();
    refresh_status_line();
    DVR_INFO("hud/layout: %s | screens ride=%d mask=0x%x", g_statusLine, (int)g_menuInWindow, g_menuMask);
}

void save(const char* ini) {
    if (!ini) return;
    char keep[MAX_PATH]; strncpy_s(keep, g_ini, _TRUNCATE);
    strncpy_s(g_ini, ini, _TRUNCATE);
    for (int e = 0; e < ElCount; ++e) {
        char key[64];
        _snprintf(key, sizeof(key), "Element.%s", kRows[e].name); key[63] = 0;
        write_key(key, kAnchorNames[g_el[e].anchor]);
        _snprintf(key, sizeof(key), "Element.%s.WinX", kRows[e].name);      write_f(key, g_el[e].winX);
        _snprintf(key, sizeof(key), "Element.%s.WinY", kRows[e].name);      write_f(key, g_el[e].winY);
        _snprintf(key, sizeof(key), "Element.%s.WinScale", kRows[e].name);  write_f(key, g_el[e].winScale);
        _snprintf(key, sizeof(key), "Element.%s.HandX", kRows[e].name);     write_f(key, g_el[e].handX);
        _snprintf(key, sizeof(key), "Element.%s.HandY", kRows[e].name);     write_f(key, g_el[e].handY);
        _snprintf(key, sizeof(key), "Element.%s.HandScale", kRows[e].name); write_f(key, g_el[e].handScale);
        if (measured(e)) {
            char v[96];
            _snprintf(key, sizeof(key), "Region.%s", kRows[e].name); key[63] = 0;
            _snprintf(v, sizeof(v), "%.3f,%.3f,%.3f,%.3f", g_el[e].rect[0], g_el[e].rect[1], g_el[e].rect[2], g_el[e].rect[3]); v[95] = 0;
            write_key(key, v);
        }
    }
    // VR-117's keys retire with the first save (the read maps them once).
    write_key("WindowAnchor", nullptr); write_key("HandHand", nullptr);
    write_key("HandX", nullptr); write_key("HandY", nullptr); write_key("HandZ", nullptr);
    write_key("HandLift", nullptr); write_key("HandWidth", nullptr); write_key("HandOrient", nullptr); write_key("HandTilt", nullptr);
    write_key("Element.all", nullptr); write_key("Element.health", nullptr); write_key("Element.mana", nullptr); write_key("Element.menu", nullptr);
    write_f("WindowDistance", g_win.distM);
    write_f("WindowWidth", g_win.widthM);
    write_f("WindowHeight", g_win.heightM);
    write_f("WindowUp", g_win.upM);
    write_f("WindowLateral", g_win.latM);
    set_hand(0, g_hand[0], "save");
    set_hand(1, g_hand[1], "save");
    set_alpha(g_alpha, "save");
    write_i("MenuExitHeading",g_menuExitHeading.load());
    write_i("NativeRuneMarkers",dvr::objectivemarkers::rune_enabled());write_f("RuneMarkerEdgeInset",dvr::objectivemarkers::rune_inset());
    write_i("NativeTaskMarkers",dvr::objectivemarkers::enabled());write_f("TaskMarkerEdgeInset",dvr::objectivemarkers::inset());
    write_i("NativeObjectiveUpright",g_nativeObjectiveUpright);write_i("WheelCloseAnimation",g_wheelCloseAnimation);
    write_i("PauseSceneFreshness",g_pauseSceneFreshness.load());
    write_i("NativeRuneOwnership",dvr::objectivemarkers::rune_ownership());
    write_i("NativeMarkerChildren",g_nativeMarkerChildren);
    write_i("NativeGameplayReference",g_nativeGameplayReference);
    write_i("WheelSidePanels",g_wheelParts);
    for(int part=0;part<2;++part) for(int k=0;k<4;++k) {
        char key[64];_snprintf(key,sizeof(key),"%s.Crop%d",kWheelPartKeys[part],k);write_f(key,g_wheelPartCrop[part][k]);
    }
    write_i("VitalsMirror",g_vitalsMirror);write_i("VitalsAutoCrop",g_vitalsAutoCrop);write_i("VitalsBack",g_vitalsBack);
    write_i("VitalsAttach",g_vaOn);
    write_i("VitalsInScene",g_vitalsScene);
    for(int k=0;k<5;++k) write_f(kVitalsBackKeys[k],g_vitalsBackCfg[k]);
    write_i("VitalsSplit",g_vitalsSplit);write_f("VitalsSplit.Top",g_vitalsLine[0]);write_f("VitalsSplit.Bottom",g_vitalsLine[1]);
    for(int part=0;part<2;++part) for(int k=0;k<4;++k) {
        char key[64];_snprintf(key,sizeof(key),"%s.Crop%d",kVitalsPartKeys[part],k);write_f(key,g_vitalsCrop[part][k]);
    }
    write_i("GroupInteractions",g_groupInteractions);write_i("RouteObjectives",g_routeObjectives);
    write_i("ObjectiveScreenTracking",g_objectiveScreen);write_i("NativeObjectiveUpright",g_nativeObjectiveUpright);
    write_i("NativeObjectiveIcons",g_nativeObjectives);write_i("NativeObjectiveLabels",g_nativeObjectiveLabels);write_f("NativeObjectiveScale",g_nativeObjectiveScale);
    for(int i=0;i<5;++i) save_scoped_alpha(i);
    for(int i=0;i<2;++i) {
        char key[64];
        _snprintf(key,sizeof(key),"%sFollowHand",kReadNames[i]);write_i(key,g_readHand[i]);
        _snprintf(key,sizeof(key),"%sHandWidth",kReadNames[i]);write_f(key,g_readWidth[i]);
        _snprintf(key,sizeof(key),"%sHandDistance",kReadNames[i]);write_f(key,g_readDistance[i]);
        _snprintf(key,sizeof(key),"%sHandRight",kReadNames[i]);write_f(key,g_readRight[i]);
        _snprintf(key,sizeof(key),"%sHandUp",kReadNames[i]);write_f(key,g_readUp[i]);
    }

    save_read_rotation();
    set_backdrop(0, g_backdrop[0], "save");
    set_backdrop(1, g_backdrop[1], "save");
    for(int i=0;i<kMenuContexts;++i) {
        char key[64];
        _snprintf(key,sizeof(key),"HeadLook%s",kMenuContextNames[i]); write_i(key,menu_head_look(kMenuContextBits[i]));
        _snprintf(key,sizeof(key),"NoBlur%s",kMenuContextNames[i]); write_i(key,menu_no_blur(kMenuContextBits[i]));
    }
    write_f("WeaponDialDistance",g_dialDistance);
    write_i("WeaponDialDirectionOnly",g_dialDirection);
    write_i("WeaponDialCircle",g_dialCircle);
    write_i("WeaponDialEntryTilt",g_dialEntryTilt);
    write_i("WeaponDialEntryYaw",g_dialEntryYaw);
    write_f("WeaponDialDeadzone",g_dialDeadM);
    write_i("WeaponDial", g_dialOn ? 1 : 0);
    write_f("WeaponDialWidth",g_dialWidth);
    write_f("WeaponDialRadius",g_dialRadius);
    write_f("WeaponDialCropX",g_dialCropX);
    write_f("WeaponDialCropY",g_dialCropY);
    write_key("MenuInWindow", g_menuInWindow ? "1" : "0");
    set_menu_context_mask(g_menuMask, "save");
    strncpy_s(g_ini, keep, _TRUNCATE);
}

// ---- seam, status ---------------------------------------------------------

// `hud anchor <element|all> off|frame|window|world|handL|handR`, `hud window dist|width|
// height|up|lateral <f>`, `hud window view|world|recenter` (every window-kind element),
// `hud hand l|r x|y|z|lift|width|tilt <f>|billboard|grip`, `hud place <element>
// window|hand <x> <y> [scale]`, `hud region <element> x0,y0,x1,y1`, `hud alpha ...`,
// `hud menu on|off`, `hud menu <Context> on|off`, `hud reset`, `hud layout`, `hud list`.
bool command(const char* args) {
    char w1[24] = "", w2[24] = "", w3[24] = "", w4[24] = "", w5[24] = "", w6[24] = "";
    const int n = sscanf(args, "%23s %23s %23s %23s %23s %23s", w1, w2, w3, w4, w5, w6);
    if (n < 1) return false;
    if (!strcmp(w1, "reset")) { reset_presets("the seam"); return true; }
    if (!strcmp(w1, "layout")) { log_status(); return true; }
    if (!strcmp(w1, "list")) { log_list(); return true; }
    if (!strcmp(w1, "menu") && n >= 2) {
        if (!strcmp(w2, "on")) { set_menu_in_window(true, "the seam"); return true; }
        if (!strcmp(w2, "off")) { set_menu_in_window(false, "the seam"); return true; }
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
        const int a = anchor_from_name(w3);
        if (a < 0) { DVR_WARN("hud: anchor wants <element|all> off|frame|window|world|handL|handR (`hud list` names the elements)"); return true; }
        if (!_stricmp(w2, "all")) { for (int e = 0; e < ElCount; ++e) set_element_anchor(e, a, "the seam"); return true; }
        const int e = element_from_name(w2);
        if (e < 0) { DVR_WARN("hud: no element '%s' (`hud list` names them; 'all' = every row)", w2); return true; }
        set_element_anchor(e, a, "the seam");
        return true;
    }
    if (!strcmp(w1, "window") && n >= 2) {
        WindowCfg c = g_win;
        const float v = n >= 3 ? (float)atof(w3) : 0.0f;
        if (!strcmp(w2, "view") || !strcmp(w2, "world")) {
            const int from = !strcmp(w2, "view") ? AnchorWorld : AnchorWindow, to = !strcmp(w2, "view") ? AnchorWindow : AnchorWorld;
            for (int e = 0; e < ElCount; ++e) if (g_el[e].anchor == from) set_element_anchor(e, to, "the seam");
            return true;
        }
        if (!strcmp(w2, "recenter")) { dvr::vr::recenter_hud_world_anchor(); DVR_INFO("hud: the world window re-seeded where the head is now"); return true; }
        if (n < 3) { DVR_WARN("hud: window wants view|world|recenter or dist|width|height|up|lateral <m>"); return true; }
        if (!strcmp(w2, "dist")) c.distM = v;
        else if (!strcmp(w2, "width")) c.widthM = v;
        else if (!strcmp(w2, "height")) c.heightM = v;
        else if (!strcmp(w2, "up")) c.upM = v;
        else if (!strcmp(w2, "lateral")) c.latM = v;
        else { DVR_WARN("hud: window wants view|world|recenter or dist|width|height|up|lateral <m>"); return true; }
        set_window(c, "the seam");
        return true;
    }
    if (!strcmp(w1, "hand") && n >= 3) {
        int which = -1;
        if (!_stricmp(w2, "l") || !_stricmp(w2, "left")) which = 0;
        else if (!_stricmp(w2, "r") || !_stricmp(w2, "right")) which = 1;
        if (which < 0) { DVR_WARN("hud: hand wants l|r then billboard|grip or x|y|z|lift|width|tilt <v>"); return true; }
        HandCfg c = g_hand[which];
        const float v = n >= 4 ? (float)atof(w4) : 0.0f;
        if (!strcmp(w3, "billboard")) c.followGrip = false;
        else if (!strcmp(w3, "grip")) c.followGrip = true;
        else if (n < 4) { DVR_WARN("hud: hand wants l|r then billboard|grip or x|y|z|lift|width|tilt <v>"); return true; }
        else if (!strcmp(w3, "x")) c.x = v;
        else if (!strcmp(w3, "y")) c.y = v;
        else if (!strcmp(w3, "z")) c.z = v;
        else if (!strcmp(w3, "lift")) c.liftM = v;
        else if (!strcmp(w3, "width")) c.widthM = v;
        else if (!strcmp(w3, "tilt")) c.tiltDeg = v;
        else if (!strcmp(w3, "spin")) c.spinDeg = v;
        else { DVR_WARN("hud: hand wants l|r then billboard|grip or x|y|z|lift|width|tilt <v>"); return true; }
        set_hand(which, c, "the seam");
        return true;
    }
    if (!strcmp(w1, "place") && n >= 5) {
        const int e = element_from_name(w2);
        const bool onHand = !strcmp(w3, "hand");
        if (e < 0 || (!onHand && strcmp(w3, "window"))) { DVR_WARN("hud: place wants <element> window|hand <x> <y> [scale]"); return true; }
        const ElementCfg& c = g_el[e];
        const float scale = n >= 6 ? (float)atof(w6) : (onHand ? c.handScale : c.winScale);
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
        if(!strcmp(w2,"reset")) {
            for(int i=0;i<5;++i) save_scoped_alpha(i);
            g_alphaBank.reset_general();set_alpha(g_alpha,"original general alpha");return true;
        }
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
    DVR_INFO("hud/alpha: mode %s (repair = max(r,g,b); captured = the sink's coverage, forced per draw; mix = "
             "max(captured, repair*mix)) gain %.2f floor %.2f gamma %.2f mix %.2f | backdrop window %.2f,%.2f,%.2f a=%.2f "
             "hand %.2f,%.2f,%.2f a=%.2f | `hud alpha mode|gain|floor|gamma|mix|backdrop ...`",
             kAlphaModeNames[g_alpha.mode], g_alpha.gain, g_alpha.floorA, g_alpha.gamma, g_alpha.mixK,
             g_backdrop[0].r, g_backdrop[0].g, g_backdrop[0].b, g_backdrop[0].a,
             g_backdrop[1].r, g_backdrop[1].g, g_backdrop[1].b, g_backdrop[1].a);
}

void log_list() {
    DVR_INFO("hud/list: %d rows (anchor, region or context, draws seen this session; `hud anchor <name> <anchor>`, "
             "`hud region <name> x0,y0,x1,y1`, `hud place <name> window|hand <x> <y> [scale]`):", ElCount);
    for (int e = 0; e < ElCount; ++e) {
        char idn[96];
        if (kRows[e].context >= 0) _snprintf(idn, sizeof(idn), "context %d", kRows[e].context);
        else if (kRows[e].vignette) _snprintf(idn, sizeof(idn), "wider and taller than 60 %%");
        else if (measured(e)) _snprintf(idn, sizeof(idn), "region [%.3f,%.3f - %.3f,%.3f]", g_el[e].rect[0], g_el[e].rect[1], g_el[e].rect[2], g_el[e].rect[3]);
        else _snprintf(idn, sizeof(idn), e == ElDefault ? "everything unclaimed" : "UNMEASURED (rides default)");
        idn[sizeof(idn) - 1] = 0;
        DVR_INFO("hud/list:   %-13s %-7s %-44s seen %u | win (%.2f,%.2f)x%.2f hand (%.2f,%.2f)x%.2f | %s",
                 kRows[e].name, kAnchorNames[g_el[e].anchor], idn, g_seen[e], g_el[e].winX, g_el[e].winY, g_el[e].winScale,
                 g_el[e].handX, g_el[e].handY, g_el[e].handScale, kRows[e].what);
    }
    for (int s = 0; s < kMaxSinks; ++s)
        if (g_sink[s].anchor >= 0) DVR_INFO("hud/list:   sink %d = %s%s", s, g_sinkLabel[s], g_sink[s].rideOnly ? " (the ride's)" : "");
}

const char* status_line() { return g_statusLine; }

void log_status() {
    DVR_INFO("hud/layout: %s", g_statusLine);
    char per[512] = "";
    for (int e = 0; e < ElCount; ++e) {
        if (!g_routeCounts[e]) continue;
        char one[48];
        _snprintf(one, sizeof(one), " %s=%u", kRows[e].name, g_routeCounts[e]); one[47] = 0;
        strncat(per, one, sizeof(per) - strlen(per) - 1);
    }
    DVR_INFO("hud/layout: routed this window:%s | no-region->default %u, no-sink->default %u, left in frame %u "
             "(no-region reads the whole count while [Hud] Regions=0: that is by design, not a fault)",
             per[0] ? per : " nothing", g_routeNoRegion, g_routeOverflow, g_routeFrame);
    if (g_routeCounts[ElDefault] && !g_menuRiding)
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 30000,
                         "hud: %u draws this window reached no named element and ride `default` - `draws on` + `draws regions` "
                         "lists their rectangles as clusters; `hud region <name> x0,y0,x1,y1` names one live",
                         g_routeCounts[ElDefault]);
    memset(g_routeCounts, 0, sizeof(g_routeCounts));
    g_routeNoRegion = g_routeOverflow = g_routeFrame = 0;
    log_alpha();
}

void status(dvr::status::Writer& w) {
    w.kv("windowDist", (double)g_win.distM);
    w.kv("windowWidth", (double)g_win.widthM);
    w.kv("handLWidth", (double)g_hand[0].widthM);
    w.kv("handRWidth", (double)g_hand[1].widthM);
    w.kv("menuInWindow", g_menuInWindow);
    w.kv("menuRiding", g_menuRiding);
    w.kv("nativeGameplayReference", native_gameplay_reference());
    w.kv("ridingContext", g_ridingContext);
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
    for (int e = 0; e < ElCount; ++e) w.kv(kRows[e].name, kAnchorNames[g_el[e].anchor]);
    w.end_obj();
    w.obj("seen");
    for (int e = 0; e < ElCount; ++e) w.kv(kRows[e].name, (unsigned long)g_seen[e]);
    w.end_obj();
    w.obj("sinks");
    for (int s = 0; s < kMaxSinks; ++s) if (g_sink[s].anchor >= 0) { char k[8]; _snprintf(k, sizeof(k), "%d", s); w.kv(k, g_sinkLabel[s]); }
    w.end_obj();
}

// ---- F10 ------------------------------------------------------------------

void draw_ui() {
    if(g_nativeGameplayReference) {
        ImGui::TextWrapped("Native HUD comparison is ON. Your configured HUD panels are bypassed.");
        if(ImGui::Button("Restore my configured HUD")) {
            g_nativeGameplayReference=false;write_i("NativeGameplayReference",0);dvr::hudcap::invalidate_content();
        }
    }
    if(ImGui::CollapsingHeader("Split health / mana (VR-142)",ImGuiTreeNodeFlags_DefaultOpen)) {
        if(ImGui::Checkbox("Split the vitals into two panels",&g_vitalsSplit)) {write_i("VitalsSplit",g_vitalsSplit);dvr::hudcap::invalidate_content();}
        ImGui::TextWrapped("The health and mana bars are slanted, so they are cut along a diagonal line. Health keeps the left of the line, mana and the equipped item the right. Vitals must be on an anchor (not off) for its image to exist.");
        bool line=ImGui::SliderFloat("Split line at the top (screen x)",&g_vitalsLine[0],0,.3f,"%.3f");
        line|=ImGui::SliderFloat("Split line at the bottom (screen x)",&g_vitalsLine[1],0,.3f,"%.3f");
        if(line) {write_f("VitalsSplit.Top",g_vitalsLine[0]);write_f("VitalsSplit.Bottom",g_vitalsLine[1]);}
        if(ImGui::Checkbox("Mana mirrors health's placement",&g_vitalsMirror)) write_i("VitalsMirror",g_vitalsMirror);
        ImGui::SameLine();
        if(ImGui::Checkbox("Trim each part at the line",&g_vitalsAutoCrop)) {write_i("VitalsAutoCrop",g_vitalsAutoCrop);dvr::hudcap::invalidate_content();}
        if(g_vaStart) {
            const float left=g_vaSeconds-(GetTickCount64()-g_vaStart)/1000.f;
            ImGui::TextColored(ImVec4(1,.8f,.2f,1),"ATTACHING in %.1f s: hold each hand where its panel should ride",left>0?left:0);
        } else if(ImGui::Button("Attach to my hands (panels freeze in front of you, then a countdown)")) vitals_attach_start();
        ImGui::SameLine(); ImGui::SetNextItemWidth(90);
        ImGui::SliderFloat("seconds",&g_vaSeconds,2,20,"%.0f");
        if(g_vaValid[0] || g_vaValid[1]) {
            if(ImGui::Checkbox("Use the attached placement",&g_vaOn)) write_i("VitalsAttach",g_vaOn);
            ImGui::SameLine();
            if(ImGui::Button("Forget it")) { g_vaOn=false;g_vaValid[0]=g_vaValid[1]=false;write_i("VitalsAttach",0);write_key(kVaKeys[0],nullptr);write_key(kVaKeys[1],nullptr); }
            ImGui::TextDisabled("left %s, right %s. It overrides the back-of-hand and panel offsets.",
                g_vaValid[0]?(g_vaFrame[0]?"on the drawn palm":"on the grip (re-attach to follow the hand model)"):"none",
                g_vaValid[1]?(g_vaFrame[1]?"on the drawn palm":"on the grip (re-attach to follow the hand model)"):"none");
        }
        if(ImGui::Checkbox("Draw them ON the hand model (in the game image)",&g_vitalsScene)) write_i("VitalsInScene",g_vitalsScene);
        if(g_vitalsScene) {
            ImGui::TextDisabled("Uses the attachment above (re-attach once if it says 'on the grip'). Fine-tune per hand:");
            for(int h=0;h<2;++h) {
                ImGui::PushID(760+h);
                bool t=ImGui::SliderFloat3(h?"right hand trim (cm, palm frame)":"left hand trim (cm, palm frame)",g_vsTrim[h],-15,15,"%.1f");
                if(t) for(int k=0;k<3;++k) {char key[48];_snprintf(key,sizeof(key),"VitalsInScene.%s.Trim%d",h?"R":"L",k);write_f(key,g_vsTrim[h][k]);}
                ImGui::PopID();
            }
        }
        if(ImGui::Checkbox("Lay both on the back of the hands (moves with the hand)",&g_vitalsBack)) write_i("VitalsBack",g_vitalsBack);
        if(g_vitalsBack) {
            bool b=ImGui::SliderFloat("Out from the back of the hand (m)",&g_vitalsBackCfg[0],-.4f,.4f,"%.3f");
            b|=ImGui::SliderFloat("Along the knuckles (m)",&g_vitalsBackCfg[1],-.4f,.4f,"%.3f");
            b|=ImGui::SliderFloat("Along the controller (m)",&g_vitalsBackCfg[2],-.4f,.4f,"%.3f");
            b|=ImGui::SliderFloat("Tilt (deg)",&g_vitalsBackCfg[3],-90,90,"%.0f");
            b|=ImGui::SliderFloat("Spin (deg)",&g_vitalsBackCfg[4],-180,180,"%.0f");
            if(b) for(int k=0;k<5;++k) write_f(kVitalsBackKeys[k],g_vitalsBackCfg[k]);
            ImGui::TextDisabled("The left hand mirrors these. The panels' own size and offsets below still apply.");
        }
        if(ImGui::Button("Mirror the right hand panel onto the left hand")) {
            HandCfg m=g_hand[1];m.x=-m.x;m.spinDeg=-m.spinDeg;set_hand(0,m,"F10 vitals split (mirrored HandR)");
        }
        for(int part=0;part<2;++part) {
            const int e=part?ElVitalsMana:ElVitalsHealth;ImGui::PushID(720+part);
            ImGui::TextUnformatted(kVitalsPartNames[part]);
            int anchor=g_el[e].anchor;
            const char* names[]={"off","window","world","handL","handR"};
            int choice=anchor>=AnchorWindow?anchor-1:0;
            if(ImGui::Combo("Anchor",&choice,names,5)) {anchor=choice?choice+1:AnchorOff;set_element_anchor(e,anchor,"F10 vitals split");}
            const bool hand=anchor_is_hand(anchor);
            float x=hand?g_el[e].handX:g_el[e].winX,y=hand?g_el[e].handY:g_el[e].winY,scale=hand?g_el[e].handScale:g_el[e].winScale;
            if(part==1 && g_vitalsMirror) ImGui::TextDisabled("Placement mirrors health (x %.3f, y %.3f, size %.2fx)",x,y,scale);
            else {
                bool moved=ImGui::SliderFloat("Horizontal (m)",&x,-1.5f,1.5f,"%.3f");
                moved|=ImGui::SliderFloat("Vertical (m)",&y,-1.5f,1.5f,"%.3f");
                moved|=ImGui::SliderFloat("Size",&scale,.1f,3.f,"%.2fx");
                if(moved) set_element_place(e,hand,x,y,scale,"F10 vitals split");
            }
            if(ImGui::TreeNode("Crop (screen fractions)")) {
                bool changed=ImGui::SliderFloat("Left",&g_vitalsCrop[part][0],0,.4f,"%.3f");
                changed|=ImGui::SliderFloat("Top",&g_vitalsCrop[part][1],0,.4f,"%.3f");
                changed|=ImGui::SliderFloat("Right",&g_vitalsCrop[part][2],0,.4f,"%.3f");
                changed|=ImGui::SliderFloat("Bottom",&g_vitalsCrop[part][3],0,.4f,"%.3f");
                if(changed) for(int k=0;k<4;++k) {char key[64];_snprintf(key,sizeof(key),"%s.Crop%d",kVitalsPartKeys[part],k);write_f(key,g_vitalsCrop[part][k]);}
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    if(ImGui::CollapsingHeader("Weapon wheel side panels",ImGuiTreeNodeFlags_DefaultOpen)) {
        if(ImGui::Checkbox("Separate D-pad and health/mana panels",&g_wheelParts)) {
            write_i("WheelSidePanels",g_wheelParts);dvr::hudcap::invalidate_content();
        }
        ImGui::TextWrapped("Visible with the hand weapon dial. These share their own alpha controls and the same captured frame as the wheel. Position and scale each panel below.");
        ImGui::TextUnformatted("D-pad and health/mana alpha (shared)");
        draw_scoped_alpha(4);
        ImGui::TextWrapped("These controls affect only the two side panels. A lower gamma darkens grey artwork; it is not a selective background mask.");
        for(int part=0;part<2;++part) {
            const int e=part?ElWheelPotions:ElWheelShortcuts;ImGui::PushID(700+part);
            ImGui::TextUnformatted(kWheelPartNames[part]);
            int anchor=g_el[e].anchor;
            const char* names[]={"off","window","world","handL","handR"};
            int choice=anchor>=AnchorWindow?anchor-1:0;
            if(ImGui::Combo("Anchor",&choice,names,5)) {anchor=choice?choice+1:AnchorOff;set_element_anchor(e,anchor,"F10 wheel parts");}
            const bool hand=anchor_is_hand(anchor);
            float x=hand?g_el[e].handX:g_el[e].winX,y=hand?g_el[e].handY:g_el[e].winY,scale=hand?g_el[e].handScale:g_el[e].winScale;
            bool moved=ImGui::SliderFloat("Horizontal (m)",&x,-1.5f,1.5f,"%.3f");
            moved|=ImGui::SliderFloat("Vertical (m)",&y,-1.5f,1.5f,"%.3f");
            moved|=ImGui::SliderFloat("Size",&scale,.25f,3.f,"%.2fx");
            if(moved) set_element_place(e,hand,x,y,scale,"F10 wheel parts");
            if(ImGui::TreeNode("Adjust captured area")) {
                bool changed=ImGui::SliderFloat("Left edge",&g_wheelPartCrop[part][0],0,1,"%.3f");
                changed|=ImGui::SliderFloat("Right edge",&g_wheelPartCrop[part][1],0,1,"%.3f");
                changed|=ImGui::SliderFloat("Bottom edge",&g_wheelPartCrop[part][2],0,1,"%.3f");
                changed|=ImGui::SliderFloat("Height (fraction of image width)",&g_wheelPartCrop[part][3],.02f,.6f,"%.3f");
                if(changed) for(int k=0;k<4;++k) {char key[64];_snprintf(key,sizeof(key),"%s.Crop%d",kWheelPartKeys[part],k);write_f(key,g_wheelPartCrop[part][k]);}
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    }
    if(ImGui::CollapsingHeader("Menu immersion")) {
        bool keep=g_menuExitHeading.load();
        if(ImGui::Checkbox("Keep viewing direction when closing menus",&keep)) {g_menuExitHeading.store(keep);write_i("MenuExitHeading",keep);}
        ImGui::TextWrapped("Per-menu controls. Head look keeps the world paused and rotates the rendered camera. Blur suppression is experimental; reopen the menu after changing it.");
        for(int i=0;i<kMenuContexts;++i) {
            ImGui::PushID(100+i); ImGui::Text("%s",kMenuContextNames[i]);
            const auto bit=1u<<kMenuContextBits[i]; char key[64];
            bool h=(g_menuHeadMask.load()&bit)!=0,b=(g_menuBlurMask.load()&bit)!=0;
            if(ImGui::Checkbox("Live head look",&h)) {
                if(h) g_menuHeadMask.fetch_or(bit); else g_menuHeadMask.fetch_and(~bit);
                _snprintf(key,sizeof(key),"HeadLook%s",kMenuContextNames[i]);write_i(key,h);
            }
            ImGui::SameLine();
            if(ImGui::Checkbox("Remove menu blur",&b)) {
                if(b) g_menuBlurMask.fetch_or(bit); else g_menuBlurMask.fetch_and(~bit);
                _snprintf(key,sizeof(key),"NoBlur%s",kMenuContextNames[i]);write_i(key,b);
            }
            ImGui::PopID();
        }
    }
    if(ImGui::CollapsingHeader("Pause menu alpha")) {
        draw_scoped_alpha(3);
        if(ImGui::Checkbox("Keep wheel crop through closing animation (test)",&g_wheelCloseAnimation)) write_i("WheelCloseAnimation",g_wheelCloseAnimation);
        bool fresh=g_pauseSceneFreshness.load();
        if(ImGui::Checkbox("Recent pause scene uploads (test)",&fresh)) {g_pauseSceneFreshness.store(fresh);write_i("PauseSceneFreshness",fresh);}
    }
    if(ImGui::CollapsingHeader("Notes, books and journal alpha")) {
        draw_scoped_alpha(1);
        ImGui::TextWrapped("One shared alpha profile for notes, books and the journal.");
    }
    if(ImGui::CollapsingHeader("Objectives")) {
        bool runeTask=dvr::objectivemarkers::rune_enabled();
        float runeInset=dvr::objectivemarkers::rune_inset()*100.f;
        const bool runeChange=ImGui::Checkbox("Native rune arrow boundary (test)",&runeTask);
        const bool runeInsetChange=ImGui::SliderFloat("Rune arrow inset",&runeInset,5.f,30.f,"%.0f%%");
        if(runeChange || runeInsetChange) {
            dvr::objectivemarkers::configure_runes(runeTask,runeInset/100.f);
            write_i("NativeRuneMarkers",runeTask);write_f("RuneMarkerEdgeInset",runeInset/100.f);
        }
        bool nativeTask=dvr::objectivemarkers::enabled();
        float edgeInset=dvr::objectivemarkers::inset()*100.f;
        const bool taskChange=ImGui::Checkbox("Native objective arrow boundary (test)",&nativeTask);
        const bool insetChange=ImGui::SliderFloat("Offscreen arrow inset",&edgeInset,5.f,30.f,"%.0f%%");
        if(taskChange || insetChange) {
            dvr::objectivemarkers::configure(nativeTask,edgeInset*.01f);
            write_i("NativeTaskMarkers",nativeTask);write_f("TaskMarkerEdgeInset",edgeInset*.01f);
        }
        ImGui::TextWrapped("Higher inset brings offscreen objective arrows toward the center. Applies on the next game update; on-screen target positions stay unchanged.");
        if(ImGui::Checkbox("Native gameplay HUD reference (test)",&g_nativeGameplayReference)) {
            dvr::hudcap::invalidate_content();
            write_i("NativeGameplayReference",g_nativeGameplayReference);
            DVR_INFO("hud/native-reference: requested=%d; menu visual ownership takes precedence",(int)g_nativeGameplayReference);
        }
        ImGui::TextWrapped("Reference ON keeps all gameplay HUD in the game image, at native size and color. Turn OFF to restore your HUD settings. Menus keep their configured panels. Compare head turning in the same spot.");
        bool runeOwnership=dvr::objectivemarkers::rune_ownership();
        if(ImGui::Checkbox("Keep rune group native from first appearance (test)",&runeOwnership)) {
            dvr::objectivemarkers::configure_rune_ownership(runeOwnership);
            g_runeIconContinuity.clear();
            write_i("NativeRuneOwnership",runeOwnership);
        }
        if(ImGui::Checkbox("Keep marker inner artwork native (test)",&g_nativeMarkerChildren)) {
            write_i("NativeMarkerChildren",g_nativeMarkerChildren);
            g_nativeLabels.clear();g_nativeChildContent.clear();g_runeIconContinuity.clear();
        }
        if(g_nativeObjectives && !g_nativeGameplayReference) {
            if(ImGui::SliderFloat("Native objective size",&g_nativeObjectiveScale,.25f,1.f,"%.2fx")) {
                write_f("NativeObjectiveScale",g_nativeObjectiveScale);
                DVR_INFO("hud/native-size: requested=%.3f; applies to recognized icon/description draws",g_nativeObjectiveScale);
            }
            ImGui::TextWrapped("Size affects recognized native markers and descriptions. Position comes from the game target; panel offsets do not apply. Identification is still experimental.");
        }
    }
    if(ImGui::CollapsingHeader("HUD grouping")) {
        bool change=ImGui::Checkbox("Keep interaction labels together",&g_groupInteractions);
        change|=ImGui::Checkbox("Route moving objective markers",&g_routeObjectives);
        change|=ImGui::Checkbox("Objective markers follow screen",&g_objectiveScreen);
        change|=ImGui::Checkbox("Native objective icons (test)",&g_nativeObjectives);
        change|=ImGui::Checkbox("Native objective title and distance (test)",&g_nativeObjectiveLabels);
        change|=ImGui::Checkbox("Keep native objectives upright (test)",&g_nativeObjectiveUpright);
        ImGui::TextDisabled("Native size and comparison controls are in Objectives above.");
        ImGui::TextWrapped("Native test learns edge-clamped marker content and keeps it native when it moves through the center. Size preserves the native center. Unlearned isolated icons remain native at original size; similar artwork can match.");
        ImGui::TextWrapped("Screen tracking separates marker size from screen position. Window/world markers follow the rendered field of view; their window scale controls icon size. Native game edge indicators remain.");
        ImGui::TextWrapped("Test controls: group nearby interaction draws and recognize the measured objective-marker shape. Other similar icons may match; disable to compare. Panel placement below applies only when native icons are disabled.");
        if(change) {
            write_i("GroupInteractions",g_groupInteractions);write_i("RouteObjectives",g_routeObjectives);
            write_i("ObjectiveScreenTracking",g_objectiveScreen);write_i("NativeObjectiveUpright",g_nativeObjectiveUpright);
            write_i("NativeObjectiveIcons",g_nativeObjectives);write_i("NativeObjectiveLabels",g_nativeObjectiveLabels);write_f("NativeObjectiveScale",g_nativeObjectiveScale);
            rebalance();refresh_status_line();
        }
    }
    if(ImGui::CollapsingHeader("Interactables alpha")) {
        draw_scoped_alpha(2);
        ImGui::TextWrapped("Applies to the interaction title, action prompt and icons routed onto a HUD panel. Frame keeps native game rendering.");
    }
    if(ImGui::CollapsingHeader("Notes and journal on the hand")) {
        ImGui::TextWrapped("Books and notes use a fixed attachment to your hand, regardless of how you open them. This slider tilts the page without moving its attachment. Changes apply and save immediately.");
        if(ImGui::SliderFloat("Reading tilt (degrees)",&g_readTilt,-180.f,180.f,"%.0f"))save_read_rotation();
        for(int i=0;i<2;++i) {
            ImGui::PushID(kReadNames[i]);ImGui::TextUnformatted(kReadNames[i]);
            bool change=ImGui::Checkbox("Follow left hand",&g_readHand[i]);
            change|=ImGui::SliderFloat("Panel width (m)",&g_readWidth[i],.15f,1.5f,"%.2f");
            change|=ImGui::SliderFloat("Distance offset (m, + farther)",&g_readDistance[i],-.30f,.50f,"%.2f");
            change|=ImGui::SliderFloat("Horizontal offset (m, + right)",&g_readRight[i],-.75f,.75f,"%.2f");
            change|=ImGui::SliderFloat("Vertical offset (m, + up)",&g_readUp[i],-.75f,.75f,"%.2f");
            if(change) {
                char key[64];
                _snprintf(key,sizeof(key),"%sFollowHand",kReadNames[i]);write_i(key,g_readHand[i]);
                _snprintf(key,sizeof(key),"%sHandWidth",kReadNames[i]);write_f(key,g_readWidth[i]);
                _snprintf(key,sizeof(key),"%sHandDistance",kReadNames[i]);write_f(key,g_readDistance[i]);
                _snprintf(key,sizeof(key),"%sHandRight",kReadNames[i]);write_f(key,g_readRight[i]);
        _snprintf(key,sizeof(key),"%sHandUp",kReadNames[i]);write_f(key,g_readUp[i]);
            }
            ImGui::PopID();
        }
    }
    if (ImGui::CollapsingHeader("Weapon dial")) {
        draw_scoped_alpha(0);
        ImGui::TextWrapped("Wheel alpha is independent of reading, interactables and general HUD alpha.");
        bool changed = ImGui::Checkbox("World-space left-hand dial", &g_dialOn);
        changed |= ImGui::SliderFloat("Distance offset (m, + farther)",&g_dialDistance,-.30f,.50f,"%.2f");
        changed |= ImGui::Checkbox("Direction only (tiny movement selects)",&g_dialDirection);
        changed |= ImGui::Checkbox("Circular crop",&g_dialCircle);
        changed |= ImGui::Checkbox("Follow head tilt on opening",&g_dialEntryTilt);
        changed |= ImGui::Checkbox("Follow horizontal head angle on opening",&g_dialEntryYaw);
        ImGui::TextWrapped("Applies next opening. Off: upright and facing your position. On: use the selected head angles at entry. Orientation stays fixed while open.");
        changed |= ImGui::SliderFloat("Neutral radius (m)",&g_dialDeadM,.0005f,.010f,"%.4f");
        changed |= ImGui::SliderFloat("Dial width (m)", &g_dialWidth, .15f, 1.2f, "%.2f");
        if(!g_dialDirection) changed |= ImGui::SliderFloat("Hand travel for full input (m)", &g_dialRadius, .04f, .30f, "%.2f");
        changed |= ImGui::SliderFloat("Dial crop width", &g_dialCropX, .30f, 1.f, "%.2f");
        changed |= ImGui::SliderFloat("Dial crop height", &g_dialCropY, .30f, 1.f, "%.2f");
        ImGui::TextWrapped("Hold left grip: the dial stays at the opening hand position and faces your head. Move the left hand to select. Either stick overrides hand selection. Release grip to equip. Reopen after changing settings.");
        if (changed) {
            g_dial.reset();
            write_f("WeaponDialDistance",g_dialDistance);
            write_i("WeaponDialDirectionOnly",g_dialDirection);
            write_i("WeaponDialCircle",g_dialCircle);
    write_i("WeaponDialEntryTilt",g_dialEntryTilt);
    write_i("WeaponDialEntryYaw",g_dialEntryYaw);
            write_f("WeaponDialDeadzone",g_dialDeadM);
            write_i("WeaponDial",g_dialOn ? 1 : 0);
            write_f("WeaponDialWidth",g_dialWidth); write_f("WeaponDialRadius",g_dialRadius);
            write_f("WeaponDialCropX",g_dialCropX); write_f("WeaponDialCropY",g_dialCropY);
        }
    }
    ImGui::TextDisabled("%s", g_statusLine);
    ImGui::Separator();
    ImGui::Text("ELEMENTS (which anchor each one rides; 'seen' = draws routed to it this session; a row without a region rides 'default')");
    for (int e = 0; e < ElCount; ++e) {
        ImGui::PushID(e);
        int a = g_el[e].anchor;
        if(e==ElWheelShortcuts || e==ElWheelPotions || e==ElVitalsHealth || e==ElVitalsMana) {ImGui::PopID();continue;}
        if(e==ElObjective && g_nativeObjectives) {
            ImGui::Text("objective     native game target | seen %u",g_seen[e]);
            ImGui::TextDisabled("Use Objectives above. Panel anchor/offset/scale do not apply in native mode.");
            ImGui::PopID();
            continue;
        }
        ImGui::Text("%-13s", kRows[e].name); ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("##anchor", &a, kAnchorNames, AnchorCount)) set_element_anchor(e, a, "F10 HUD");
        ImGui::SameLine();
        if (kRows[e].context >= 0) ImGui::TextDisabled("screen");
        else if (kRows[e].vignette) ImGui::TextDisabled("full-screen rule");
        else if (e == ElDefault) ImGui::TextDisabled("unclaimed draws");
        else if(e==ElObjective && g_routeObjectives) ImGui::TextDisabled("moving marker");
        else if (measured(e)) ImGui::TextDisabled("region ok");
        else ImGui::TextDisabled("UNMEASURED");
        ImGui::SameLine();
        ImGui::TextDisabled("seen %u", g_seen[e]);
        const bool dedicated=(e==ElWheel && g_dialOn) || (e==ElNote && g_readHand[0]) || (e==ElJournal && g_readHand[1]);
        if(dedicated) ImGui::TextDisabled("Use this menu's dedicated panel controls above.");
        if (anchor_visible(a) && !dedicated) {
            const bool onHand = anchor_is_hand(a);
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
    ImGui::Text("THE WINDOW (in front of the player; 'window' is head-locked, 'world' parks where you recentred)");
    {
        WindowCfg c = g_win;
        bool ch = false;
        if (ImGui::Button("Recenter the world window")) { dvr::vr::recenter_hud_world_anchor(); DVR_INFO("hud: the world window re-seeded where the head is now (F10)"); }
        ch |= ImGui::SliderFloat("distance (m)", &c.distM, 0.5f, 3.0f, "%.2f");
        ch |= ImGui::SliderFloat("width (m)", &c.widthM, 0.3f, 3.0f, "%.2f");
        ch |= ImGui::SliderFloat("height (m, 0 = the texture's aspect)", &c.heightM, 0.0f, 3.0f, "%.2f");
        ch |= ImGui::SliderFloat("vertical offset (m)", &c.upM, -1.0f, 1.0f, "%.2f");
        ch |= ImGui::SliderFloat("lateral offset (m)", &c.latM, -1.0f, 1.0f, "%.2f");
        if (ch) set_window(c, "F10 HUD");
    }
    for (int k = 0; k < 2; ++k) {
        ImGui::Separator();
        ImGui::PushID(200 + k);
        ImGui::Text("THE %s HAND PANEL (38.92's wrist HUD, on the tracked hand)", k ? "RIGHT" : "LEFT");
        HandCfg c = g_hand[k];
        bool ch = false;
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
        if (c.followGrip) ch |= ImGui::SliderFloat("spin in its own plane (deg)", &c.spinDeg, -180.0f, 180.0f, "%.0f");
        if (ch) set_hand(k, c, "F10 HUD");
        ImGui::PopID();
    }
    ImGui::Separator();
    ImGui::Text("GENERAL HUD ALPHA (excludes all dedicated alpha groups)");
    {
        if(ImGui::Button("Restore original general alpha")) {
            for(int i=0;i<5;++i) save_scoped_alpha(i);
            g_alphaBank.reset_general();set_alpha(g_alpha,"F10 original general alpha");
        }
        ImGui::TextWrapped("General HUD only. Original: repair, gain 1, floor 0, gamma 1, mix 1. Wheel, side panels, reading, interactable and pause alpha remain independent.");
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
    ImGui::Text("SCREENS ON THE ANCHORS (the world stays in stereo behind them; the main menu keeps the mono screen)");
    {
        bool on = g_menuInWindow;
        if (ImGui::Checkbox("in-game screens ride their row's anchor", &on)) set_menu_in_window(on, "F10 HUD");
        uint32_t mask = g_menuMask;
        bool ch = false;
        for (int i = 0; i < kMenuContexts; ++i) {
            bool b = (mask & (1u << kMenuContextBits[i])) != 0;
            if (i) ImGui::SameLine();
            if (ImGui::Checkbox(kMenuContextNames[i], &b)) { ch = true; mask = b ? (mask | (1u << kMenuContextBits[i])) : (mask & ~(1u << kMenuContextBits[i])); }
        }
        if (ch) set_menu_context_mask(mask, "F10 HUD");
        ImGui::TextDisabled("%s", g_menuRiding ? "a screen is riding now" : "no screen riding");
    }
    ImGui::Separator();
    if (ImGui::Button("hud reset (the presets)")) reset_presets("F10 HUD");
    ImGui::SameLine();
    ImGui::TextDisabled("every change here is written to [Hud] in the ini at once");
}

} // namespace dvr::hudlayout
