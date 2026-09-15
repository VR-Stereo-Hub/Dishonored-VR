// core/gfx/hud_layout.h - which HUD element goes where (VR-117).
//
// The game's HUD is one Scaleform movie painted onto the backbuffer at the
// tail of the frame (ENGINE_NOTES, "The Scaleform HUD draw class, measured").
// core/gfx/hud_class recognises those draws and core/gfx/hud_capture redirects
// them into private targets ("sinks"). This module is the ONE OWNER of the
// choices a player makes about them: which anchor each element rides (the
// window in front of the player, the tracked hand, the eye textures, or off),
// where it sits on that anchor, and the window's and the hand panel's own
// placement. Every value is an ini key under [Hud], every change has an F10
// control and a seam word, and `hud reset` puts the presets back.
//
// Elements are told apart by the screen REGION of the redirected draws
// (hud_class's region probe): health top-left, mana beside it, the equipment
// bottom corners, the reticle at the centre, subtitles bottom centre, the
// interaction prompt, the objective marker. A draw that spans regions (a
// vignette, a full-screen fill) goes to the window; a draw whose region cannot
// be read goes to the window too and is counted, so a misroute is visible in
// `hud status` and fixable with one `Region.<name>=` line in the ini.
//
// The runtime layer knows nothing of elements: provide() hands it a flat list
// of quad descriptors (texture + anchor + placement), one per sink in use.
#pragma once
#include <stdint.h>

struct ID3D11DeviceContext;
namespace dvr::status { class Writer; }
namespace dvr::vr { struct HudQuadDesc; }

namespace dvr::hudlayout {

enum Anchor : int { AnchorOff = 0, AnchorFrame = 1, AnchorWindow = 2, AnchorHand = 3 };
const char* anchor_name(int a);
int anchor_from_name(const char* s);   // -1 when unknown

// The elements, in the order the ini and the F10 tab list them. "all" is the
// fallback every draw without a readable region takes; "menu" is what the
// in-game screens (pause, journal, note, store, mission stats) are routed to
// while they ride the window.
enum Element : int {
    ElAll = 0, ElHealth, ElMana, ElEquipment, ElReticle, ElSubtitles, ElPrompt,
    ElObjective, ElVignette, ElMenu, ElCount
};
const char* element_name(int e);
int element_from_name(const char* s);   // -1 when unknown

struct ElementCfg {
    int   anchor;                    // Anchor
    float winX, winY, winScale;      // placement within the window (m, m, x)
    float handX, handY, handScale;   // placement within the hand panel (m, m, x)
    float rect[4];                   // the region, normalised backbuffer x0,y0,x1,y1 (0,0,0,0 = unmeasured)
};
struct WindowCfg {
    bool  worldLocked;               // false = VIEW space (head-locked), true = LOCAL, seeded at recenter
    float distM, widthM, heightM;    // heightM 0 = the texture's aspect
    float upM, latM;
};
struct HandCfg {
    int   hand;                      // 0 left, 1 right
    float x, y, z;                   // offset in the grip frame, metres
    float liftM;                     // along world up
    float widthM;
    bool  followGrip;                // false = billboard to the head (38.92), true = watch face
    float tiltDeg;                   // FollowGrip: nod about the panel's right axis
};

const ElementCfg& element(int e);
const WindowCfg&  window();
const HandCfg&    hand();
void set_element_anchor(int e, int anchor, const char* who);
void set_element_place(int e, bool onHand, float x, float y, float scale, const char* who);
void set_element_rect(int e, const float rect[4], const char* who);
void set_window(const WindowCfg& w, const char* who);
void set_hand(const HandCfg& h, const char* who);
void reset_presets(const char* who);

// Menus riding the window: the master and the per-context opt-ins, kept here
// so the F10 tab and the ini have one owner; the game side reads them
// through the mask (bit = dvr::mono::Context).
bool     menu_in_window();
void     set_menu_in_window(bool on, const char* who);
uint32_t menu_context_mask();
void     set_menu_context_mask(uint32_t mask, const char* who);
void     set_menu_riding(bool riding);   // published by the game side each poll
bool     menu_riding();

// ---- routing (the classifier's side, present thread) ----------------------
// The sink a draw goes to. bbox = the draw's normalised backbuffer rectangle
// (x0,y0,x1,y1), or null when the region probe could not read it. Returns -1
// when the element stays in the frame (AnchorFrame), else a sink index.
int  sink_for(const float* bbox, int* elementOut);
// The element a sink currently carries (-1 = free), and the sink of an element.
int  sink_element(int sink);
int  element_sink(int e);
static const int kMaxSinks = 6;

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
const char* status_line();              // one line: each element's anchor and why any is hidden
void draw_ui();                         // the F10 HUD tab (ImGui; overlay draw callback only)

} // namespace dvr::hudlayout
