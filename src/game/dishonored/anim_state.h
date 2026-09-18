#pragma once
#include "animation_rules.h"
#include "game/dishonored/hands/hand_frame.h"
namespace dvr { namespace status { class Writer; } }
namespace dvr::anim {
struct Snapshot {
    char state[3][96] = {};
    char pending[96] = {}, sequence[96] = {}, reason[96] = {};
    unsigned generation = 0;
    unsigned long long stamp = 0, entered[3] = {};
    unsigned long long sequenceAt = 0, stateAddress[3] = {};
    int bodyMode = -1, picker = -1, dialogState = -1;
    bool valid = false, game = false, cameraAction = false, mantleSplit = false;
};
void tick();
void configure(const char* ini);
void save(const char* ini);   // F10 SAVE AS DEFAULTS: the switches and timings, never the state lists
bool command(const char* args);
void status(dvr::status::Writer& w);
Snapshot snapshot();
bool enabled();
bool cinematic_enabled();
bool mantle_enabled();
void set_mantle(bool on);
void set_cinematic(bool on);
void set_enabled(bool on);
bool arm_rule_enabled(int index);
void set_arm_rule(int index,bool on);
void reset_arm_rules();
bool action_enabled(int index);
void set_action_enabled(int index,bool on);
bool action_gate_ready();
float view_right_cm();
void set_view_right_cm(float cm);
float view_right_metres(); // zero outside native handback, blended with its ownership

bool active(); // immediate ownership, including release hysteresis
bool native_draw(); // blend reached identity: native pose
bool native_full_arms(); // native draw except explicit hidden-forearm mantle
float weight(); // controller correction: 1 = controller, 0 = native
hf::Xform blend(const hf::Xform& transform);
}
