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
// The attack-source record (VR-220), read on the script lane by anim_state: both attack
// sources reach the game as the same trigger press, so whether an attack the game just
// started was the player's arm or their finger is knowable only here. Stores only; nothing
// in the detection, the pulse or the honour check reads these back.
unsigned long long last_fire_tick();        // GetTickCount64 at the last FIRE (the honour check's clock); 0 = never
unsigned long long last_pulse_close_tick(); // when that press stopped; 0 while it is still held
unsigned fires();                           // the FIRE counter, to name the swing in a log line
bool last_fire_real_trigger();              // the player's own trigger was down at or after that fire
}
