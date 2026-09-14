#pragma once
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
    bool valid = false, game = false;
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
bool active(); // immediate ownership, including release hysteresis
bool native_draw(); // blend reached identity: release split/suppression
float weight(); // controller correction: 1 = controller, 0 = native
hf::Xform blend(const hf::Xform& transform);
}
