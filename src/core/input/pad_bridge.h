// core/input/pad_bridge.h - the virtual gamepad (41.0).
//
// The VR controllers reach Dishonored as an Xbox 360 pad on slot 0. The
// runtime layer publishes a RAW controller snapshot once per present
// (core/vr/openxr_input.h: sticks, triggers, grips, buttons, no shaping);
// this module composes it into an XINPUT_STATE and serves that state from
// the exe's hooked XInput import slots. The game's own input system does the
// rest, so every binding, every menu and every context the game has for a
// gamepad works without us knowing anything about them.
//
// WHAT THIS MODULE IS NOT. It is not the motion controls. Hands, motion aim,
// motion melee, motion crouch and the controller Blink aim used to compose
// themselves INTO this path, which made "the pad is dead" and "a hand feature
// misfired" the same bug. They are gone from here: what a controller does as
// a GAMEPAD is composed below, and everything that knows what Corvo can do
// with a hand lives behind the Callbacks seam or in the game tick that runs
// beside it. This file knows no engine address and no game global.
//
// THREADS. tick() runs on the present thread (the game tick, once per
// present) and is the only writer. The XInput detours run on whatever thread
// the game polls from and only read, under the state lock. haptic requests
// are handed to the runtime layer's queue, which is safe from any thread.
#pragma once
#include <stdint.h>
#include <windows.h>

namespace dvr::status { class Writer; }

namespace dvr::pad {

// The game-specific seams, registered from the unity build
// (game/dishonored/present_tick.cpp). Every one of them is optional: a null
// pointer means "that gate is open" or "that shaping does not apply", so the
// module composes a plain gamepad on its own and the game side adds Dishonored
// to it. Called from tick() on the present thread, except haptic.
struct Callbacks {
    // Gates. menu_open is the SCRIPT-confirmed menu (discrete stick steps);
    // cursor_menu is the cursor-visible one, which can ghost during gameplay
    // and therefore may only drive the stale-cursor nudge, never the sticks.
    bool (*menu_open)()   = nullptr;
    bool (*cursor_menu)() = nullptr;
    bool (*cinematic)()   = nullptr;    // a scripted sequence owns the player

    // Composition the game owns.
    bool  (*sprint_bit)(bool clickNow)    = nullptr;  // click -> the sprint bit
    void  (*health_hold)(bool held)       = nullptr;  // R-click long press
    SHORT (*menu_step)(SHORT v, int axis) = nullptr;  // analog -> discrete steps
    // Bits the game contributes to the composed mask (slide assist, the
    // physical-crouch pulse). Gets the mask so far and the movement stick.
    WORD  (*shape_buttons)(WORD composed, float mx, float my, bool stealthHeld) = nullptr;
    // Triggers the game contributes on top of the physical ones (motion melee
    // swinging the sword). Off under [Mode] GamepadOnly, which leaves them.
    void  (*shape_triggers)(BYTE* lt, BYTE* rt) = nullptr;
    // Room-scale locomotion: how far the player has walked from the recenter
    // point, in meters, right/forward. False = nothing to add this present.
    bool  (*locomotion)(float* rightM, float* fwdM) = nullptr;
    // The game's own rumble, on its way back out to the controllers.
    void  (*haptic)(int hand, float amp, float durSec) = nullptr;
};
void set_callbacks(const Callbacks& cb);

void init();            // the state lock; from DllMain, before anything polls
// Patch the exe's two XInput import slots, byte-verifying that they still
// point at the real xinput1_3 exports first. The ADDRESSES come from the game
// side (game/dishonored/patterns.h) so this file stays engine-agnostic.
void install_hook(uintptr_t getStateSlot, uintptr_t setStateSlot);
void tick();            // once per present: snapshot -> XINPUT_STATE
void shutdown();

// Config ([Controllers] and the crouch bit crouch.cpp watches for).
void  set_enabled(bool on);
bool  enabled();
void  set_deadzone(float dz);
void  set_crouch_mask(uint32_t mask);

// State, for status.json, the `cfg` seam word, the overlay and the heartbeat.
bool hooked();
bool active();          // the runtime is publishing a LIVE snapshot right now
long polls();           // game XInputGetState calls since the last reset
void reset_polls();
WORD buttons();         // the mask exactly as the game will receive it
bool crouch_down();     // buttons() carries the crouch bit
bool wheel_held();      // the power-wheel grip is open (a gate several
                        // subsystems read: head tracking, melee, crouch)
void delivered_stick(SHORT* lx, SHORT* ly);   // movement stick, post-shaping
void raw_move(float* mx, float* my);          // ...and pre-shaping
void status(dvr::status::Writer& w);

} // namespace dvr::pad
