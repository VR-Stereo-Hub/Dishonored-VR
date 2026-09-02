// core/gfx/capture.h - the game's D3D9 backbuffer as a D3D11 texture (41.0).
//
// Once per Present the active stereo method asks for the frame the game just
// finished. The path is the one every build since 30.x shipped: GetBackBuffer
// -> GetRenderTargetData into a system-memory surface -> one row copy into a
// cached heap buffer -> UpdateSubresource into a B8G8R8A8 D3D11 texture (the
// same byte order as D3D9's X8R8G8B8, so no per-pixel conversion). It costs a
// GPU-to-CPU round trip per frame; the planned fix is a D3D9Ex shared surface
// opened on the D3D11 side (ROADMAP S1), which is why the consumer only ever
// sees texture()/srv() and never the pixels' path.
//
// The instrument: every grab samples the CPU pixels on a stride and keeps the
// bounding box of non-black content. A frame that fills its own backbuffer
// reads FULL; a game drawing into a corner or a band reads CROPPED, with the
// box - the 32.57 signature ("right eye black, pair off-centre") that was lost
// to guessing between five explanations. Logged once per size change and on a
// 10 s Debug cadence; `dump capture` writes the pixels.
//
// The cost instrument (S1): every grab is timed in its three phases (the
// GetRenderTargetData wait, the lock + row copy, the D3D11 upload) and the
// per-present averages print every 3 s as `capture: cost/present ...`. That
// line is the number a cheaper capture path is judged by - the headset run's
// presents/s alone cannot separate the capture from the game's own frame time.
#pragma once
#include <stdint.h>

struct IDirect3DDevice9;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;

namespace dvr::capture {

struct Bbox {
    bool     valid = false;
    uint32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // inclusive, in pixels
    float    pctW = 0.0f, pctH = 0.0f;         // box extent / frame extent
    float    nonBlackPct = 0.0f;               // sampled pixels above black
};

// Grab the current backbuffer. False = nothing new this present (format not
// handled, readback failed, no device); texture() then still holds the last
// good frame, if any. Present thread.
bool grab(IDirect3DDevice9* dev9, ID3D11Device* dev11, ID3D11DeviceContext* ctx11);

ID3D11Texture2D*          texture();   // B8G8R8A8, SHADER_RESOURCE; null before the first grab
ID3D11ShaderResourceView* srv();
uint32_t width();
uint32_t height();
const uint8_t* pixels();               // BGRA, pitch = width*4; the last grab's CPU copy
Bbox bbox();
uint32_t grabs();                      // lifetime count

// The cost of the last 3 s window, microseconds per present (0 before the
// first window closes): the readback wait, the lock + copy, the upload, and
// their sum. `grabsInWindow` is the population the averages come from.
struct Cost {
    uint32_t rtdUs = 0, copyUs = 0, uploadUs = 0, totalUs = 0;
    uint32_t grabsInWindow = 0;
};
Cost cost();

// The shared-surface probe's verdict (one-shot at the first grab; the log
// carries every HRESULT): can the game's D3D9 device hand D3D11 a surface
// without a CPU round trip? False on a plain IDirect3D9 device by design.
bool probed();
bool shared_available();

// The D3D9 device is about to Reset: the system-memory surface goes (every
// default-pool object this proxy creates must be released here - 38.63).
void on_reset();
void shutdown();

} // namespace dvr::capture
