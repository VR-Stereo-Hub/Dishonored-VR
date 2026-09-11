// core/gfx/desktop_eye.h - the desktop window's eye pin (D3D9).
//
// The game window shows the game's OWN backbuffer, and under a sequential
// stereo method every eye draw reaches the original Present - so the desktop
// shows L, R, L, R while the headset receives correct pairs. That is why a
// recording of the game window looks like alternate-eye rendering even when the
// headset stream is healthy. `mirror_present()` in the runtime layer has
// declared this contract since 41.0 and never implemented the D3D9 copy; this
// is that implementation, kept out of the runtime layer so that file stays as
// close to the BioShock copy as the D3D9 host allows.
#pragma once
#include <d3d9.h>
#include <stdint.h>
#include "core/gfx/desktop_eye_policy.h"

namespace dvr::status { class Writer; }

namespace dvr::desktop_eye {

// The device the game presents on. Published every present by the frame path;
// never AddRef'd, never dereferenced outside a call from that thread.
void set_device(IDirect3DDevice9* dev);

// Reset the current identity before calling the stereo method (including its
// early failures). The method publishes the current backbuffer's resolved eye.
void begin_present(uint32_t present);
void note_drawn_eye(int eye);
void note_single_draw(); // game lane, counts actual single gameplay ticks

// Called once per runtime present, after capture and before the HUD hook.
// eyeSign belongs to the DELIVERED texture and can lag the live backbuffer.
void on_present(int eyeSign);
bool set_source(const char* name, const char* origin);
const char* source_name();

struct Record {
    uint32_t present = 0;
    int draw = 0, tag = 0, shown = 0;
    char action = '?'; // S snapshot, B blit, N none, F failed copy, ? no callback
    char source = 't';
};
bool record_for(uint32_t present, Record& out);

// Releases the DEFAULT-pool surface. MUST run before the game's Reset - a
// forgotten default-pool object makes Reset fail forever (the hkReset LAW).
void on_reset();
void shutdown();

void set_enabled(bool on);
bool enabled();
void status(dvr::status::Writer& w);
void log_status();

}  // namespace dvr::desktop_eye
