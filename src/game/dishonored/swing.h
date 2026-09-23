// game/dishonored/swing.h - the motion sword (VR-37): swing the right controller
// and Corvo swings the sword. The decision is swing_core.h (pure, host-tested);
// this is the game side of it, implemented in melee.cpp: the gates that say a
// swing is allowed right now, the attack pulse on the virtual pad, the check that
// the game really started an attack, and the levers, words and status for tuning
// it in the headset. Present lane throughout.
#pragma once
namespace dvr { namespace status { class Writer; } }
namespace dvr::swing {
void configure(const char* ini);   // [Melee], after the legacy keys' globals exist
void save(const char* ini);        // F10 SAVE AS DEFAULTS and `swing save`
bool command(const char* args);    // the `swing` word
void status(dvr::status::Writer& w);
void draw_ui();                    // ImGui: only from the overlay's draw callback
void tick();                       // once per present, before the pad is composed
bool pulse_active();               // the attack input is being held for a swing
bool output_rb();                  // [Melee] Output=rb: the pulse goes to RB, not RT
void note_real_trigger(float v);   // the player's own trigger, for the honoured-check
void note_pad_poll();              // the game polled the pad (never reset, unlike g_padPolls)
long pad_polls();                  // how many times it has, for a press that must be seen
}
