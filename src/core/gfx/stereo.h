// core/gfx/stereo.h - the stereo-strategy seam (41.0).
//
// The game renders natively through D3D9 into its own window. Once per
// Present, the active stereo METHOD decides what the headset sees: it turns
// the game's frame into one D3D11 texture and tags it with the eye it belongs
// to, and the runtime layer (core/vr/openxr_runtime) copies that texture into
// the eye swapchain the tag selects and submits the frame. The methods are
// the rungs of the ladder in docs/ARCHITECTURE.md:
//
//   mono     rung 1: the game frame on a head-locked quad, the same image in
//            both eyes. Proves the whole path headset-to-eye; ships default.
//   aer      rung 2: AlternateEye - each game frame is rendered for one eye
//            (the camera seam offsets the eye on the script lane, the tag
//            says which), the compositor holds the other eye's last frame.
//   reentry  rung 3: SequentialReentry - the engine's scene draw is called a
//            second time per tick with the other eye's camera, so every
//            present carries a fresh eye and per-eye effects stay native.
//
// One method is active at a time ([Stereo] Method=, the `stereo <name>` seam
// word); switching is live and fails soft (a method that refuses keeps the
// previous one running). Every call below runs on the present thread.
#pragma once
#include <stdint.h>
#include <stddef.h>

struct IDirect3DDevice9;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
namespace dvr::status { class Writer; }

namespace dvr::stereo {

// What the frame path knows at Present-head, after the runtime located the
// head for the frame the game is about to render.
struct FrameInput {
    uint32_t frame = 0;          // present counter
    bool     headOk = false;     // head[] valid
    float    head[3][4] = {};    // device-to-tracking, meters (XR LOCAL space)
    bool     fovOk = false;      // halfFovDeg valid (0 until the first locate)
    float    halfFovDeg[2] = {}; // headset half-angles h, v (symmetric claim)
    float    ipdM = 0.0f;        // inter-eye separation, meters (0 = unknown)
    uint32_t eyeW = 0, eyeH = 0; // the runtime's recommended per-eye size
};

struct FrameDevices {
    IDirect3DDevice9*    dev9 = nullptr;   // the game's device (Present's `self`)
    ID3D11Device*        dev11 = nullptr;  // the mod's device (null = no D3D11)
    ID3D11DeviceContext* ctx11 = nullptr;
};

// What a method produced for THIS present. `tex` must be R8G8B8A8 (the
// runtime's swapchain family, dvr::vr::swapchain_format()); `eyeSign` is
// -1 left, +1 right, 0 = the same image for both eyes (the quad screen).
// Null tex = nothing to show this present (the runtime still ends the frame).
struct FrameOutput {
    ID3D11Texture2D* tex = nullptr;
    int              eyeSign = 0;
    uint32_t         w = 0, h = 0;
};

class IStereo {
public:
    virtual ~IStereo() {}
    virtual const char* name() const = 0;
    // False for a registered design stub: select() refuses it with the note.
    virtual bool implemented() const = 0;
    virtual const char* note() const { return ""; }
    // True when the method's output is a per-eye render the runtime should
    // submit as a PROJECTION layer (camera mode, the per-eye poses, the FOV
    // claim) rather than the head-locked quad screen. The frame path arms the
    // runtime on the transition and, while it holds, the FOV lever follows the
    // frame aspect and the layer claims the rendered FOV (present_tick.cpp).
    virtual bool wants_projection() const { return false; }
    // Presents per game tick the method produces (2 under SequentialReentry):
    // the frame path paces pairs, not presents, when this is above 1.
    virtual int presents_per_tick() const { return 1; }
    // Present-head, before the game-side tick: the method learns the head and
    // decides which eye the NEXT game frame renders (the camera seam reads it).
    virtual void begin_frame(const FrameInput& in) = 0;
    virtual int  eye_for_next_frame() const = 0;   // -1, +1, or 0 (mono)
    // Present-tail: turn the game's frame into the output texture. False =
    // nothing this present (the caller submits nothing; the game runs flat).
    virtual bool end_frame(const FrameDevices& d, FrameOutput& out) = 0;
    // The game's D3D9 device is about to Reset: release default-pool objects.
    virtual void on_reset() = 0;
    virtual void shutdown() = 0;
    virtual void status(dvr::status::Writer& w) = 0;
};

// The registry. register_all() runs once from Direct3DCreate9 (no static
// initialisers: the proxy is loaded under the loader lock). The first
// registered method is the default until select() names another.
void register_method(IStereo* m);
void register_all();
// Live switch by name; refuses (and logs why, keeping the current method)
// for an unknown name or a design stub. Returns true when the switch happened.
bool select(const char* name);
// The ini's choice ([Stereo] Method) is read inside Direct3DCreate9 BEFORE the
// game side registers its scene-draw hooks later in that same function, so a
// hook-backed method (reentry) refused there for a startup ORDER, not a
// verdict, and the session spent itself on mono. The config only RECORDS the
// name; the proxy applies it once the game side is up, through select()'s
// normal fail-soft (an unknown name or a stub still leaves mono running).
void set_config_method(const char* name);
void apply_config_method();
// The selection and the arm flag (41.1, the F10 tickbox). choose() records
// the player's method and selects it when armed; set_armed(false) parks on
// the mono screen WITHOUT forgetting the selection, set_armed(true) selects
// it again. [Stereo] Method= / Armed= persist both; `stereo <name>` chooses,
// `stereo arm on|off` parks and re-arms.
bool choose(const char* name);
void set_armed(bool on);
bool armed();
const char* wanted_name();
IStereo* active();
const char* active_name();

// The frame path's entry points (core/framework/frame_hooks.cpp).
void begin_frame(const FrameInput& in);
bool end_frame(const FrameDevices& d, FrameOutput& out);   // counts, beats
void on_reset();
void shutdown();
const FrameOutput& last_output();   // what the last end_frame produced
uint32_t frames_out();              // presents that produced a texture
// Does the active method want the projection layer? `stereo projection
// on|off|auto` (default auto = the method's own answer) overrides it: `on`
// puts the mono screen's frame into a projection layer as the same image for
// both eyes (rung 1.5, the way to exercise camera mode, the cinematic gate,
// the FOV lever and the claim before a per-eye method exists), `off` pins the
// quad. A lever, default auto, live.
bool wants_projection();
void set_projection_override(int mode);   // -1 auto, 0 off, 1 on
const char* projection_override_name();

// 41.1 (Dishonored): the untagged HOLD. A tick that fails the second draw's
// gates presents untagged, and an untagged present is the mono path - the
// same image in both eyes - so one stalled tick inside a healthy stereo
// stream reads as a one-frame flicker in the headset (measured on the dev
// rig: `mono/s` 5-17 against `out/s` 223, the present-stall guard firing).
// HoldUntagged=N suppresses up to N CONSECUTIVE untagged presents while the
// stream has been tagged recently: end_frame returns false, nothing is
// submitted, and the compositor holds the previous pair instead of flipping
// to mono. The (N+1)th in a row falls through to the mono path, so a real
// transition (a menu, a load, a cinematic) still gets there within N presents.
// A lever: default 0 (OFF, the pre-41.1 behaviour), `stereo hold <n>` live.
int  hold_untagged();
void set_hold_untagged(int n);
uint32_t holds_done();   // presents suppressed this session (the beat/status)
void note_hold();        // the method reports one suppressed present

// status.json "stereo" object and the log line the `stereo status` word prints.
void status(dvr::status::Writer& w);
void log_status();

// The F10 overlay is drawn INTO the output texture by the game side (ImGui
// only from the overlay's own draw callback); a method calls this after its
// own composition, with the target bound by the callee.
using OverlayDrawFn = void (*)(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* rtv,
                               uint32_t w, uint32_t h);
void set_overlay_draw(OverlayDrawFn fn);
OverlayDrawFn overlay_draw();

// The methods (one translation unit each under core/gfx/). aer is a design
// stub in 41.0: registered, named, refusing with its note.
IStereo* create_mono_screen();
IStereo* create_aer();
IStereo* create_reentry();

// SequentialReentry's game side (game/dishonored/scene_draw.cpp) registers
// itself here: whether the root's bytes verify (with the refusal text), the
// arm/disarm of the second draw, its status fields and its draw count (ticks).
// The pass-2 skip counters the game side keeps, in this order, so the method
// can name the OWNER of a one-sided tag stream without knowing the game:
// foreign, state, silent, stall, session, test, exit, forced (cumulative;
// `forced` is the `reentry skip2 <n>` instrument: pass 2 skipped on purpose
// AFTER pass 1's tag, the one-sided stream the headset fault is made of).
constexpr int kReentryGateCount = 8;
struct ReentryHooks {
    bool (*available)(char* why, size_t cap) = nullptr;
    void (*set_armed)(bool on) = nullptr;
    bool (*poisoned)() = nullptr;
    void (*status)(dvr::status::Writer& w) = nullptr;
    uint32_t (*draws)() = nullptr;
    void (*gates)(uint32_t out[kReentryGateCount]) = nullptr;
};
void set_reentry_hooks(const ReentryHooks& h);
// 41.1 (session 9): the within-tick invariant as the pairing's check ([Stereo]
// C5Pair=1, `reentry c5pair on|off`): a present whose c5 sits exactly
// -ipd*scale along the camera's right row from the previous present's is a
// pass-2 present whatever the ring's order says; a disagreement streak
// realigns the ring. Off = the ring's order alone (the pre-41.1 behaviour).
void set_reentry_c5_pair(bool on);
bool reentry_c5_pair();

// The game thread pushes one tag per draw (-1 pass 1, +1 pass 2) with the
// camera position the writer produced (null = unknown); the method pops one
// per present in end_frame and hands the eye to the runtime's tag ring.
void reentry_push_tag(int eyeSign, const float pos[3]);

} // namespace dvr::stereo
