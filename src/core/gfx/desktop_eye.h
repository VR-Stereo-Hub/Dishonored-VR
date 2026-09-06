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

namespace dvr::status { class Writer; }

namespace dvr::desktop_eye {

// The device the game presents on. Published every present by the frame path;
// never AddRef'd, never dereferenced outside a call from that thread.
void set_device(IDirect3DDevice9* dev);

// The runtime layer's eye pin, implemented. -1 = this backbuffer is the LEFT
// eye: snapshot it. +1 = RIGHT eye: put the held LEFT back over it, so the
// desktop shows one eye consistently. Called only AFTER that eye's XR capture,
// so the headset still gets the true image. 0 = mono, nothing to pin.
void on_present(int eyeSign);

// Releases the DEFAULT-pool surface. MUST run before the game's Reset - a
// forgotten default-pool object makes Reset fail forever (the hkReset LAW).
void on_reset();
void shutdown();

void set_enabled(bool on);
bool enabled();
void status(dvr::status::Writer& w);
void log_status();

}  // namespace dvr::desktop_eye
