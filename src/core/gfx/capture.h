// core/gfx/capture.h - the game's D3D9 backbuffer as a D3D11 texture (41.0).
//
// Once per Present the active stereo method asks for the frame the game just
// finished. The default path is the one every build since 30.x shipped:
// GetBackBuffer -> GetRenderTargetData into a system-memory surface -> one row
// copy into a cached heap buffer -> UpdateSubresource into a B8G8R8A8 D3D11
// texture (the same byte order as D3D9's X8R8G8B8, so no per-pixel
// conversion). It costs a GPU-to-CPU round trip per frame. [Capture] Mode=
// picks the path (sync|deferred|shared, `capture mode <m>` live; the modes are
// described at the top of capture.cpp), which is why the consumer only ever
// sees texture()/srv() and never the pixels' path.
//
// The instrument: every grab samples the CPU pixels on a stride and keeps the
// bounding box of non-black content. A frame that fills its own backbuffer
// reads FULL; a game drawing into a corner or a band reads CROPPED, with the
// box - the 32.57 signature ("right eye black, pair off-centre") that was lost
// to guessing between five explanations. Logged once per size change and on a
// 10 s Debug cadence; `dump capture` writes the pixels.
//
// The cost instrument (S1): every grab is timed per phase (the
// GetRenderTargetData call, the lock, the row copy, the D3D11 upload, the
// blit) and the per-present averages print every 3 s as `capture: cost/present
// ...`. That line is the number a cheaper capture path is judged by - the
// headset run's presents/s alone cannot separate the capture from the game's
// own frame time. Measured 2026-09-03 at 1920x1080 (ENGINE_NOTES, "The capture
// cost, measured"): sync ~5 ms (the lock waits 2.4-3.1 ms), deferred ~2.3 ms.
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
// first window closes): the GetRenderTargetData call, the LockRect (where the
// CPU waits for the queued readback), the row copy, the D3D11 upload, the
// StretchRect (deferred/shared) and their sum. `grabsInWindow` is the
// population the averages come from.
struct Cost {
    uint32_t rtdUs = 0, lockUs = 0, copyUs = 0, uploadUs = 0, blitUs = 0, totalUs = 0;
    uint32_t grabsInWindow = 0;
};
Cost cost();
// The LAST grab's phases (this present's; zeros when the present grabbed
// nothing): the tick budget (core/framework/perf) reads it once per present.
Cost last_grab();

// The shared-surface probe's verdict (one-shot at the first grab; the log
// carries every HRESULT): can the game's D3D9 device hand D3D11 a surface
// without a CPU round trip? False on a plain IDirect3D9 device by design.
bool probed();
bool shared_available();

// The mode. set_mode() queues the change for the next grab (any thread, the
// config loader or the command seam); an impossible mode (shared on a device
// that cannot share) is refused with the reason and the current mode stays.
// `off` (41.1, session 8) is the tick budget's A/B control: grab() takes
// nothing, texture() keeps re-showing the last frame (the headset image is
// FROZEN on purpose and the cost line and the beat say so), and the tick rate
// without any capture is what the perf line then measures. Live only; the
// ini refuses it.
enum class Mode { Sync = 0, Deferred = 1, Shared = 2, Off = 3 };
bool        set_mode(const char* name);   // "sync" | "deferred" | "shared" | "off"
Mode        mode();
const char* mode_name();

// The eye tag of the frame being grabbed (a stereo method sets it before
// grab()), and the tag of the content the last grab DELIVERED: the same tag
// in sync and shared mode, the previous present's under deferred. serial()
// counts grabs; delivered_serial() says which grab's pixels texture() holds.
void     set_pending_tag(int eyeSign);
int      delivered_tag();
uint32_t delivered_serial();
uint32_t serial();
// shared (41.1, session 8): the delivery. SharedWait=0 (default) delivers the
// PREVIOUS present's slot, whose blit had a whole present to finish (the tag
// travels with the slot as under deferred); 1 delivers this present's after
// waiting on its fence (zero latency; the CPU waits for the frame in flight).
// The fence is a D3D9 event query, waited on with a bound (10 ms) and
// counted: there is no cross-API GPU fence in D3D9, so this IS the fence.
void     set_shared_wait(bool on);
bool     shared_wait();
uint32_t fence_waits();      // deliveries that had to spin on the fence (window)
uint32_t fence_timeouts();   // deliveries whose fence had not signalled at the bound (lifetime)

// `dump capture` under shared mode: read the shared surface back now so
// pixels() is this present's frame, not the last 3 s sample. True when
// pixels() is usable.
bool snapshot_pixels(IDirect3DDevice9* dev);

// The D3D9 device is about to Reset: the system-memory surface goes (every
// default-pool object this proxy creates must be released here - 38.63).
void on_reset();
void shutdown();

} // namespace dvr::capture
