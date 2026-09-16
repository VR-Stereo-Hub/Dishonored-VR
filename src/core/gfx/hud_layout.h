// core/gfx/hud_layout.h - which HUD element goes where (VR-117, VR-120).
//
// The game's HUD is one Scaleform movie painted onto the backbuffer at the
// tail of the frame (ENGINE_NOTES, "The Scaleform HUD draw class, measured").
// core/gfx/hud_class recognises those draws and core/gfx/hud_capture redirects
// them into private targets ("sinks"). This module is the ONE OWNER of the
// choices a player makes about them: which ANCHOR each element rides (off, the
// eye textures, the head-locked window, the world-parked window, the left
// hand, the right hand), where it sits on that anchor, the window's and the
// two hand panels' own placement, and the alpha. Every value is an ini key
// under [Hud], every change has an F10 control and a seam word, and `hud
// reset` puts the presets back.
//
// Elements are a TABLE (hud_layout.cpp, kRows): one row per element with the
// identity that claims a draw (core/gfx/hud_route.h: a riding screen's UI
// owner context, or the draw's screen rectangle read through the vertex
// shader's own transform) and its placement. A draw no row claims routes to
// `default`, is drawn on whatever anchor `default` rides, and is counted, so
// an element the mod has not named yet is visible and can be named from the
// log (`hud list`). A new element is one row.
//
// Measured elements have private sinks: overlapping/moving content cannot
// leak between element quads. Initial regions establish scale/placement but
// no longer clip the isolated texture. Unmeasured elements share the anchor's
// CATCH-ALL sink and one whole-sink quad. Only sinks in use pay a copy.
//
// The runtime layer knows nothing of elements: provide() hands it a flat list
// of quad descriptors (texture + crop + anchor + placement + a stable slot).
#pragma once
#include <stdint.h>
#include "core/gfx/hud_alpha.h"

struct ID3D11DeviceContext;
namespace dvr::status { class Writer; }
namespace dvr::vr { struct HudQuadDesc; }

namespace dvr::hudlayout {

enum Anchor : int { AnchorOff = 0, AnchorFrame = 1, AnchorWindow = 2, AnchorWorld = 3, AnchorHandL = 4,
                    AnchorHandR = 5, AnchorCount = 6 };
const char* anchor_name(int a);
int anchor_from_name(const char* s);   // -1 when unknown; the legacy "hand" resolves by the legacy HandHand
inline bool anchor_visible(int a) { return a >= AnchorWindow && a < AnchorCount; }
inline bool anchor_is_hand(int a) { return a == AnchorHandL || a == AnchorHandR; }

// The elements, in the order the ini, the F10 tab and `hud list` show them.
// `default` takes every draw no row claims; the six screens route by their UI
// owner context while they ride.
enum Element : int {
    ElDefault = 0, ElVitals, ElReticle, ElPrompt, ElEquipment, ElSubtitles, ElObjective, ElToast,
    ElTutorial, ElDetection, ElSkipGauge, ElDarkVision, ElVignette,
    ElPause, ElNote, ElJournal, ElWheel, ElStore, ElMissionStats, ElCount
};
const char* element_name(int e);
int element_from_name(const char* s);   // -1 when unknown (the legacy names all, health, mana, menu map)
bool element_is_screen(int e);
int  element_for_context(int context);  // dvr::mono::Context -> the screen's row, or -1

struct ElementCfg {
    int   anchor;                    // Anchor
    float winX, winY, winScale;      // placement on the window or the world window (m, m, x)
    float handX, handY, handScale;   // placement on either hand panel (m, m, x)
    float rect[4];                   // the region, normalised backbuffer x0,y0,x1,y1 (0,0,0,0 = unmeasured)
};
struct WindowCfg {
    float distM, widthM, heightM;    // heightM 0 = the texture's aspect
    float upM, latM;
};
struct HandCfg {
    float x, y, z;                   // offset in the grip frame, metres
    float liftM;                     // along world up
    float widthM;
    bool  followGrip;                // false = billboard to the head (38.92), true = watch face
    float tiltDeg;                   // FollowGrip: nod about the panel's right axis
};

// VR-119: the HUD alpha (core/gfx/blit_quad.h explains the modes) and a
// backdrop plate per anchor KIND: 0 = the window (view or world), 1 = a hand.
enum AlphaMode : int { AlphaRepair = 0, AlphaCaptured = 1, AlphaMix = 2 };
using AlphaCfg = dvr::hudalpha::Config;
struct Backdrop { float r, g, b, a; };
const char* alpha_mode_name(int m);
int  alpha_mode_from_name(const char* s);    // -1 when unknown
const AlphaCfg& alpha();
AlphaCfg alpha_for_sink(int sink);
void set_alpha(const AlphaCfg& a, const char* who);
const Backdrop& backdrop(int kind);
void set_backdrop(int kind, const Backdrop& b, const char* who);
void backdrop_for_sink(int sink, float rgba[4]);
void circle_for_sink(int sink, uint32_t width, uint32_t height, float ellipse[4]);

const ElementCfg& element(int e);
const WindowCfg&  window();
const HandCfg&    hand(int which);   // 0 left, 1 right
void set_element_anchor(int e, int anchor, const char* who);
void set_element_place(int e, bool onHand, float x, float y, float scale, const char* who);
void set_element_rect(int e, const float rect[4], const char* who);
void set_window(const WindowCfg& w, const char* who);
void set_hand(int which, const HandCfg& h, const char* who);
void reset_presets(const char* who);

// Screens riding the HUD anchors: the master and the per-context opt-ins, kept
// here so the F10 tab and the ini have one owner; the game side reads them
// through the mask (bit = dvr::mono::Context). A screen rides only when its
// row's anchor is visible (off or frame = the mono screen takes it).
bool     menu_in_window();
void     set_menu_in_window(bool on, const char* who);
uint32_t menu_context_mask();
void     set_menu_context_mask(uint32_t mask, const char* who);
bool     screen_can_ride(int context);          // the row exists and its anchor is visible
void     set_menu_riding(bool riding, int context);   // published by the game side each poll
void forget_draw_owners();
bool menu_riding();
bool menu_stereo_hold();
bool menu_head_look(int context);
bool menu_no_blur(int context);

// ---- routing (the classifier's side, present thread) ----------------------
// The sink a draw goes to. bbox = the draw's normalised backbuffer rectangle
// (x0,y0,x1,y1), or null when the region probe could not read it. Returns -1
// when the element stays in the frame (AnchorFrame), else a sink index.
int  sink_for(const float* bbox, int* elementOut, uint64_t drawKey = 0, unsigned vertices = 0, unsigned primitives = 0);
// Sinks: in use, and a label for the log ("window/crop", "handL/all").
bool sink_in_use(int sink);
bool sink_hidden(int sink);                     // an "off" element's sink: redirected, cleared, never delivered
const char* sink_label(int sink);
int  sink_anchor(int sink);                     // -1 when free
static const int kMaxSinks = 12;                // bounded pool for private measured elements and anchor catch-alls

// ---- the runtime's side (present thread) ---------------------------------
// Fills `out` with up to `max` quad descriptors from the sinks that delivered
// a texture this present. Registered once with dvr::vr::set_hud_quad_provider.
int provide(ID3D11DeviceContext* ctx, dvr::vr::HudQuadDesc* out, int max);

// ---- persistence, seam, status --------------------------------------------
void configure(const char* ini);        // LoadConfig
void save(const char* ini);             // OverlaySaveDefaults
bool command(const char* args);         // the `hud` word's layout half (see commands.cpp)
void status(dvr::status::Writer& w);
void log_status();
void log_alpha();                       // `hud alpha status`
void log_list();                        // `hud list`: every row, its anchor, its region, draws seen
const char* status_line();              // one line: each element's anchor and why any is hidden
// Present-thread wheel input and placement share one opening-position latch.
void wheel_input(bool held, bool permitted, float& x, float& y, bool& handSelected);
void draw_ui();                         // the F10 HUD tab (ImGui; overlay draw callback only)

} // namespace dvr::hudlayout
