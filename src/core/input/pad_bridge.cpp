// core/input/pad_bridge.cpp - the virtual gamepad (41.0, a real module).
//
// See pad_bridge.h for the contract. This file used to be the place every
// hand feature reached into: motion melee forced the right trigger, motion
// aim ran off the trigger values, a physical-crouch state machine pulsed B,
// the room-scale offset pushed the movement stick and the overlay's pointer
// ray was computed here from a hand pose. All of that is out. What is left is
// one job - turn the runtime layer's raw controller snapshot into the
// XINPUT_STATE the game reads - and one seam (Callbacks) for the parts of
// that job that are Dishonored's rather than a gamepad's.
//
// The mapping is GingasVR's, tuned in a headset and deliberately unchanged:
//   left stick  move            right stick X  turn (Y belongs to the head)
//   L trigger   left hand       R trigger      right hand
//   L grip      power wheel     R grip         choke
//   A jump      B stealth       X interact     Y / menu  pause
//   L click     sprint          R click        health elixir hold
#define DVR_CAT ::dvr::log::Cat::pad
#include "core/input/pad_bridge.h"

#include "core/framework/frame_hooks.h"
#include "core/framework/status.h"
#include "core/util/clock.h"
#include "core/util/log.h"
#include "core/vr/openxr_input.h"

#include <math.h>
#include <string.h>
#include <xinput.h>

namespace dvr::pad {
namespace {

typedef DWORD (WINAPI *XInputGetState_t)(DWORD, XINPUT_STATE*);
typedef DWORD (WINAPI *XInputSetState_t)(DWORD, XINPUT_VIBRATION*);

Callbacks g_cb;

XInputGetState_t g_realGetState = nullptr;
XInputSetState_t g_realSetState = nullptr;
uintptr_t g_getSlot = 0, g_setSlot = 0;

bool             g_hooked   = false;
bool             g_enabled  = true;      // [Controllers] Enabled
float            g_deadzone = 0.12f;     // [Controllers] Deadzone
uint32_t         g_crouchMask = 0x2000;  // [Hands] CrouchButtonMask (B)

CRITICAL_SECTION g_lock;                 // guards g_state
bool             g_lockReady = false;
XINPUT_STATE     g_state;                // the synthesized pad (slot 0)
DWORD            g_packet   = 1;         // starts at 1, bumped on CHANGE
volatile bool    g_active   = false;     // controllers currently feeding it
volatile LONG    g_polls    = 0;         // game GetState calls this window

// Published for the readers that used to reach into the old globals.
volatile LONG    g_btnsPub   = 0;
volatile bool    g_wheelHeld = false;
volatile LONG    g_outStick  = 0;        // packed lx | ly<<16, post-shaping
float            g_rawMx = 0.0f, g_rawMy = 0.0f;

// Instrument populations: a counter is not evidence until you know how many
// presents it could have moved on.
unsigned long    g_composed = 0, g_bumped = 0;

// ---------------------------------------------------------------------------

inline SHORT to_axis(float v)
{
    if (v >  1.0f) v =  1.0f;
    if (v < -1.0f) v = -1.0f;
    return (SHORT)(v * 32767.0f);
}

// The MOVEMENT stick's deadzone: radial, applied to the magnitude and
// rescaled, not to each axis on its own. A per-axis threshold is crossed at
// a different stick magnitude depending on the angle, so it notches the
// diagonals and - worse - rotates the direction the game is handed near the
// edge of the dead region. Ported from the BioShock Remastered VR pad layer,
// which measured it. The turn axis keeps axis1: it has no partner to be
// radial with.
void deadzone2(float* x, float* y, float dz)
{
    float m = sqrtf((*x) * (*x) + (*y) * (*y));
    if (m <= dz || m <= 1e-6f) { *x = 0.0f; *y = 0.0f; return; }
    float scaled = (m - dz) / (1.0f - dz);
    if (scaled > 1.0f) scaled = 1.0f;
    float k = scaled / m;
    *x *= k; *y *= k;
}

// A single axis: the per-axis deadzone with rescale, as PadStick always did.
inline SHORT axis1(float v, float dz)
{
    float a = fabsf(v);
    if (a < dz) return 0;
    float s = (a - dz) / (1.0f - dz);
    if (s > 1.0f) s = 1.0f;
    return (SHORT)((v < 0 ? -s : s) * 32767.0f);
}

inline bool menu_open()   { return g_cb.menu_open   && g_cb.menu_open(); }
inline bool cursor_menu() { return g_cb.cursor_menu && g_cb.cursor_menu(); }
inline bool cinematic()   { return g_cb.cinematic   && g_cb.cinematic(); }

// ---------------------------------------------------------------------------
// The XInput detours. They run on whatever thread the game polls from.

DWORD WINAPI hkXInputGetState(DWORD user, XINPUT_STATE* st)
{
    // Always report a connected pad on slot 0 (neutral until the VR
    // controllers come online). The game's input system only enables its
    // gamepad path if the pad is there from the very first poll at startup.
    if (user == 0 && g_enabled && st) {
        InterlockedIncrement(&g_polls);
        EnterCriticalSection(&g_lock);
        *st = g_state;
        LeaveCriticalSection(&g_lock);
        return ERROR_SUCCESS;
    }
    return g_realGetState ? g_realGetState(user, st) : ERROR_DEVICE_NOT_CONNECTED;
}

DWORD WINAPI hkXInputSetState(DWORD user, XINPUT_VIBRATION* vib)
{
    if (user == 0 && g_enabled && vib) {
        // game rumble -> controller haptic pulse on the stronger motor's hand
        if (g_cb.haptic) {
            WORD lm = vib->wLeftMotorSpeed, rm = vib->wRightMotorSpeed;
            WORD m = lm > rm ? lm : rm;
            if (m > 2500) {
                int h = (lm >= rm) ? 0 : 1;
                // 38.13: magic/power feedback arrives THIS way (XInputSetState).
                g_cb.haptic(h, (float)m / 65535.0f, 0.08f);
            }
        }
        return ERROR_SUCCESS;
    }
    return g_realSetState ? g_realSetState(user, vib) : ERROR_SUCCESS;
}

} // namespace

// ---------------------------------------------------------------------------

void set_callbacks(const Callbacks& cb) { g_cb = cb; }

void init()
{
    if (g_lockReady) return;
    InitializeCriticalSection(&g_lock);
    memset(&g_state, 0, sizeof(g_state));
    g_state.dwPacketNumber = g_packet;   // present but neutral, and holding
    g_lockReady = true;
}

void shutdown() { /* the IAT slots are restored by the process teardown */ }

void set_enabled(bool on) { g_enabled = on; }
bool enabled()            { return g_enabled; }
void set_crouch_mask(uint32_t mask) { g_crouchMask = mask ? mask : 0x2000; }

void set_deadzone(float dz)
{
    if (dz < 0.0f) dz = 0.0f;
    if (dz > 0.6f) dz = 0.6f;
    g_deadzone = dz;
}

bool hooked()      { return g_hooked; }
bool active()      { return g_active; }
long polls()       { return InterlockedCompareExchange(&g_polls, 0, 0); }
void reset_polls() { InterlockedExchange(&g_polls, 0); }
WORD buttons()     { return (WORD)InterlockedCompareExchange(&g_btnsPub, 0, 0); }
bool crouch_down() { return (buttons() & (WORD)g_crouchMask) != 0; }
bool wheel_held()  { return g_wheelHeld; }

void delivered_stick(SHORT* lx, SHORT* ly)
{
    LONG p = InterlockedCompareExchange(&g_outStick, 0, 0);
    if (lx) *lx = (SHORT)(p & 0xffff);
    if (ly) *ly = (SHORT)((p >> 16) & 0xffff);
}

void raw_move(float* mx, float* my)
{
    if (mx) *mx = g_rawMx;
    if (my) *my = g_rawMy;
}

void install_hook(uintptr_t getStateSlot, uintptr_t setStateSlot)
{
    if (g_hooked || !g_enabled) return;
    HMODULE xm = GetModuleHandleA("XINPUT1_3.dll");
    if (!xm) {
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Warn,
                "pad: xinput1_3.dll not loaded - no pad hook, the VR controllers "
                "will not reach the game as a gamepad");
        return;
    }
    g_getSlot = getStateSlot; g_setSlot = setStateSlot;
    g_realGetState = (XInputGetState_t)GetProcAddress(xm, (LPCSTR)2);
    g_realSetState = (XInputSetState_t)GetProcAddress(xm, (LPCSTR)3);
    void** slotGet = (void**)g_getSlot;
    void** slotSet = (void**)g_setSlot;
    // sanity: the IAT slots must currently point at the real functions
    if (*slotGet != (void*)g_realGetState || *slotSet != (void*)g_realSetState) {
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Error,
                "pad: IAT mismatch (get %p vs %p, set %p vs %p) - NOT hooking, tell Claude",
                *slotGet, (void*)g_realGetState, *slotSet, (void*)g_realSetState);
        return;
    }
    DWORD op;
    if (!VirtualProtect((void*)g_setSlot, sizeof(void*) * 2, PAGE_READWRITE, &op)) {
        DVR_LOG(DVR_CAT, ::dvr::log::Level::Error,
                "pad: VirtualProtect failed (%lu) - NOT hooking", GetLastError());
        return;
    }
    *slotGet = (void*)hkXInputGetState;
    *slotSet = (void*)hkXInputSetState;
    VirtualProtect((void*)g_setSlot, sizeof(void*) * 2, op, &op);
    g_hooked = true;
    DVR_LOG(DVR_CAT, ::dvr::log::Level::Info,
            "pad: IAT hooked - the VR controllers will appear as a 360 pad");
}

// ---------------------------------------------------------------------------
// The composition. Present thread, once per present.

void tick()
{
    // The same gate the game tick uses for g_xrOn: the session as of THIS
// present. No session means no snapshot to compose from.
    if (!g_enabled || !dvr::frame::xr_live()) { g_active = false; return; }

    bool active_ = false;
    XINPUT_STATE xs; memset(&xs, 0, sizeof(xs));
    g_wheelHeld = false;   // re-established below when the wheel grip is held

    // ONE input path. The controllers arrive as the runtime layer's raw
    // snapshot with the Touch semantics the Quest bindings always had.
    dvr::vr::InputSnapshot in;
    dvr::vr::input_snapshot(&in);

    if (in.active) {
        active_ = true;
        g_rawMx = in.mv[0]; g_rawMy = in.mv[1];       // 38.25 pre-shaping
        float mx = in.mv[0], my = in.mv[1], tx = in.lk[0], ty = in.lk[1];
        float hr = in.trigR, hl = in.trigL;
        WORD b = 0;

        // Grip hysteresis: press at 0.9, release at 0.7, so a hand resting on
        // the grip cannot chatter the wheel open and shut.
        static bool wheelWas = false, chokeWas = false;
        bool wheel = in.gripL > (wheelWas ? 0.7f : 0.9f);
        bool choke = in.gripR > (chokeWas ? 0.7f : 0.9f);
        wheelWas = wheel; chokeWas = choke;
        g_wheelHeld = wheel;
        if (wheel)           b |= XINPUT_GAMEPAD_LEFT_SHOULDER;
        if (choke)           b |= XINPUT_GAMEPAD_RIGHT_SHOULDER;
        if (in.a)            b |= XINPUT_GAMEPAD_A;       // jump
        bool userStealth = in.b;
        if (userStealth)     b |= XINPUT_GAMEPAD_B;       // stealth
        if (in.x)            b |= XINPUT_GAMEPAD_X;       // interact
        if (in.y || in.menu) b |= XINPUT_GAMEPAD_START;   // pause
        // SPRINT 38.28: the game side owns whether the click is a hold or a
        // toggle pulse; with no hook it is the plain click.
        bool sprint = g_cb.sprint_bit ? g_cb.sprint_bit(in.clkL) : in.clkL;
        if (sprint)          b |= XINPUT_GAMEPAD_LEFT_THUMB;
        if (g_cb.health_hold) g_cb.health_hold(in.clkR);  // health hold

        {   // stick-click edges stay measured facts
            static bool sw = false, hw = false;
            if (in.clkL != sw) { sw = in.clkL;
                DVR_LOG(DVR_CAT, ::dvr::log::Level::Info,
                        "pad: L-stick click (sprint) %s", sw ? "DOWN" : "UP"); }
            if (in.clkR != hw) { hw = in.clkR;
                DVR_LOG(DVR_CAT, ::dvr::log::Level::Info,
                        "pad: R-stick click (health hold) %s", hw ? "DOWN" : "UP"); }
        }

        // The game's own contributions to the mask: slide assist, and the
        // physical-crouch pulse when motion crouch is on. Motion crouch is
        // OFF under [Mode] GamepadOnly and contributes nothing there.
        if (g_cb.shape_buttons) b = g_cb.shape_buttons(b, mx, my, userStealth);

        deadzone2(&mx, &my, g_deadzone);
        xs.Gamepad.wButtons      = b;
        xs.Gamepad.sThumbLX      = to_axis(mx);
        xs.Gamepad.sThumbLY      = to_axis(my);
        xs.Gamepad.sThumbRX      = axis1(tx, g_deadzone);
        // pitch belongs to the head - EXCEPT in menus (stick navigates)
        // and while the power wheel is held open (stick points at wedges)
        xs.Gamepad.sThumbRY      = (cursor_menu() || wheel) ? axis1(ty, g_deadzone) : 0;
        xs.Gamepad.bRightTrigger = (BYTE)(hr * 255.0f);
        xs.Gamepad.bLeftTrigger  = (BYTE)(hl * 255.0f);
        if (g_cb.shape_triggers)
            g_cb.shape_triggers(&xs.Gamepad.bLeftTrigger, &xs.Gamepad.bRightTrigger);

        static WORD lastB = 0xffff;
        if (b != lastB) { lastB = b;
            DVR_LOG(DVR_CAT, ::dvr::log::Level::Info, "pad: xbtn=0x%04x", b); }

        // 38.81: "can't use my right stick on Quest" - nothing logged the
        // right stick, so the loss point was invisible. One line per second
        // while it is pushed: raw action value vs what the game gets.
        // No line at all while pushing = the ACTION isn't delivering
        // (runtime/binding side); raw nonzero but RX 0 = our shaping ate
        // it (the flags say which); raw and RX both live = game-side.
        if (tx > 0.15f || tx < -0.15f || ty > 0.15f || ty < -0.15f) {
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 1000,
                    "pad/rs: raw=(%.2f,%.2f) -> RX=%d RY=%d (menu=%d/%d cine=%d wheel=%d)",
                    tx, ty, (int)xs.Gamepad.sThumbRX, (int)xs.Gamepad.sThumbRY,
                    (int)menu_open(), (int)cursor_menu(), (int)cinematic(),
                    (int)wheel);
        }
    }

    // 38.46: walking in the room pushes the movement stick, so the pawn goes
    // where you went - through the game's own collision, no wall clipping.
    // Never during a menu; that stick is navigation there. The offset itself
    // is positional tracking's (head_track owns it, behind the callback).
    if (g_cb.locomotion && active_ && !menu_open() && !cursor_menu() && !cinematic()) {
        float rr = 0.0f, f = 0.0f;
        if (g_cb.locomotion(&rr, &f)) {
            float sx = (float)xs.Gamepad.sThumbLX / 32767.0f + rr;
            float sy = (float)xs.Gamepad.sThumbLY / 32767.0f + f;
            float sl = sqrtf(sx * sx + sy * sy);
            if (sl > 1.0f) { sx /= sl; sy /= sl; }
            xs.Gamepad.sThumbLX = to_axis(sx);
            xs.Gamepad.sThumbLY = to_axis(sy);
        }
    }

    // 38.65: cinematic running - park the pad. Buttons except START are
    // dropped (a stray A or a chair-shuffle B pulse must never eject the
    // player from a scripted sequence again); sticks and triggers go to zero.
    if (active_ && !menu_open() && !cursor_menu() && cinematic()) {
        xs.Gamepad.wButtons &= XINPUT_GAMEPAD_START;
        xs.Gamepad.sThumbLX = 0; xs.Gamepad.sThumbLY = 0;
        xs.Gamepad.sThumbRX = 0; xs.Gamepad.sThumbRY = 0;
        xs.Gamepad.bLeftTrigger = 0; xs.Gamepad.bRightTrigger = 0;
    }

    // 30.59 MENU NAVIGATION SHAPING. Analog values wandering around the game's
    // navigation threshold made steps fire erratically; menus want discrete
    // presses. 38.82: gate on the SCRIPT-confirmed menu only - the cursor flag
    // can GHOST during gameplay, and while it is up this block hard-zeroes the
    // right stick and chops the left into pulses, which reads exactly like
    // "right stick dead, left stick works".
    if (menu_open() && active_ && g_cb.menu_step) {
        xs.Gamepad.sThumbLX = g_cb.menu_step(xs.Gamepad.sThumbLX, 0);
        xs.Gamepad.sThumbLY = g_cb.menu_step(xs.Gamepad.sThumbLY, 1);
        xs.Gamepad.sThumbRX = 0;   // one navigation axis only - a second one
        xs.Gamepad.sThumbRY = 0;   // double-steps the same list
    }

    // 38.25 crawlbox: mirror the delivered (post-shaping) movement stick.
    InterlockedExchange(&g_outStick, active_
        ? (LONG)((((unsigned long)(unsigned short)xs.Gamepad.sThumbLY) << 16) |
                   (unsigned long)(unsigned short)xs.Gamepad.sThumbLX)
        : 0);

    EnterCriticalSection(&g_lock);
    if (active_) {
        // THE PACKET RULE (ported from the BioShock Remastered VR pad layer,
        // which measured it there). Games poll dwPacketNumber to decide
        // whether the pad changed; some ignore a state whose number never
        // moves, and a number that moves EVERY poll is just as much a lie -
        // it says the pad is jittering ninety times a second when the player
        // is holding perfectly still. Bump it only when the gamepad actually
        // changed, and let a held stick keep its number.
        g_composed++;
        if (memcmp(&xs.Gamepad, &g_state.Gamepad, sizeof(XINPUT_GAMEPAD)) != 0) {
            g_state.Gamepad = xs.Gamepad;
            g_state.dwPacketNumber = ++g_packet;
            g_bumped++;
        }
    } else if (g_active) {
        // controllers just dropped - neutralize so no button/stick sticks
        memset(&g_state.Gamepad, 0, sizeof(XINPUT_GAMEPAD));
        g_state.dwPacketNumber = ++g_packet;
        g_bumped++;
    }
    g_active = active_;
    // 32.41: publish the button mask exactly as the game will receive it.
    InterlockedExchange(&g_btnsPub, active_ ? (LONG)xs.Gamepad.wButtons : 0);
    LeaveCriticalSection(&g_lock);

    // The instrument, with its population named. "The pad is dead" splits
    // three ways here: polls=0 the game never asked, composed=0 the runtime
    // published no snapshot, btn=0 with both moving means we composed nothing.
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Debug, 3000,
        "pad: beat active=%d composed=%lu bumped=%lu polls=%ld btn=0x%04x "
        "(bumped < composed is CORRECT - the pad was holding still)",
        (int)g_active, g_composed, g_bumped,
        InterlockedCompareExchange(&g_polls, 0, 0), (unsigned)buttons());

    // UE3 only re-evaluates cursor visibility on a REAL mouse event, so after
    // leaving a menu with the pad the cursor stays "visible" and our
    // head-mouse stays paused until the player touches the mouse. Send a
    // net-zero 1-count wiggle to make the game update its cursor state.
    // 30.58: only when the SCRIPT says we are not in a menu - inside a real
    // menu each 1px move re-homes the highlight under the cursor.
    if (cursor_menu()) {
        bool padBusy = !menu_open() && active_ &&
                       (xs.Gamepad.wButtons != 0 ||
                        xs.Gamepad.sThumbLX >  16000 || xs.Gamepad.sThumbLX < -16000 ||
                        xs.Gamepad.sThumbLY >  16000 || xs.Gamepad.sThumbLY < -16000);
        static int nudgeTimer = 0;
        if (++nudgeTimer >= 45) { nudgeTimer = 0; padBusy = !menu_open(); }
        if (padBusy) {
            INPUT nin[2]; memset(nin, 0, sizeof(nin));
            nin[0].type = INPUT_MOUSE;
            nin[0].mi.dx = 1;  nin[0].mi.dwFlags = MOUSEEVENTF_MOVE;
            nin[1].type = INPUT_MOUSE;
            nin[1].mi.dx = -1; nin[1].mi.dwFlags = MOUSEEVENTF_MOVE;
            SendInput(2, nin, sizeof(INPUT));
        }
    }
}

void status(dvr::status::Writer& w)
{
    w.kv("padHooked", (bool)g_hooked);
    w.kv("padActive", (bool)g_active);
    w.kv("padPolls", (unsigned long)InterlockedCompareExchange(&g_polls, 0, 0));
    w.kv("padComposed", (unsigned long)g_composed);
    w.kv("padBumped", (unsigned long)g_bumped);
    w.kv("padButtons", (int)buttons());
}

} // namespace dvr::pad
