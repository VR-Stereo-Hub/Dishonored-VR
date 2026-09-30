// core/gfx/depth_probe.h - where does the game keep its scene depth? (the first step of
// docs/dishonored/PLAN-motion-vectors-dlss.md). READ-ONLY: nothing the game draws changes.
//
// Hypothesis: UE3 on D3D9 keeps linear scene depth in the ALPHA of its floating-point
// scene colour target (the exe has no INTZ/RAWZ depth-texture path). The probe remembers
// every floating-point render-target texture the game creates at eye size, and a few times
// a run reads a 5x5 grid of texels from each (small StretchRect copies, one readback) and
// logs colour and alpha. Depth predicts: alpha positive, small where the hands are (lower
// middle), large on distant walls, and changing when the player walks toward something.
// A target whose alpha is constant, zero or 1.0 everywhere fails the hypothesis, and says so.
#pragma once
#include <windows.h>
#include <d3d9.h>
#include <stdint.h>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11ShaderResourceView;

namespace dvr::depthprobe {

// device_census's CreateTexture hook: every texture the game creates passes through.
void note_texture(IDirect3DTexture9* tex, UINT w, UINT h, DWORD usage, D3DFORMAT fmt);
// Once per game Present (render thread), before any of our own writers.
void tick(IDirect3DDevice9* dev, UINT backW, UINT backH);
// Release every reference the probe holds (Reset, exit): they are DEFAULT-pool objects.
void on_reset();
void set_enabled(bool on, const char* who);
bool enabled();
void request(const char* who);   // one probe at the next present
bool command(const char* args);  // `depthprobe [on|off|now]`, `depthprobe share on|off`

// Step 2: the depth SHARED to our D3D11 device. Each present the scene target (the eye-size
// RGBA16F whose alpha is depth) is copied into a D3D9 texture opened on D3D11, fenced by an
// event query. Every 5 s the same present is read both ways (D3D9 from the game's target,
// D3D11 from the shared copy) and the two grids compared: the transport is proven when they
// agree to the texel. Off by default; [Diagnostics] DepthShare.
void share_tick(IDirect3DDevice9* dev, ID3D11Device* dev11, ID3D11DeviceContext* ctx11, UINT backW, UINT backH);
void set_share(bool on, const char* who);
bool share_on();
// Close every borrowed depth read, including diagnostic and failed clarity passes.
void read_done(ID3D11DeviceContext* ctx);
void retry(); // atomic request, serviced on the present thread after an explicit setting change
// Present-thread service gate, including release after the last consumer switches off.
bool share_tick_needed();
// Step 3: the shared depth that belongs to the colour grab `grabSerial` (capture::delivered_serial()),
// its copy fenced complete; null when none is held (not shared, refused, or aged out of the ring).
// Alpha is linear view depth. Step 3 coarse simulator minimum: 200 uu/unit.
// Temporal + MotionVectors requests the copy independently of diagnostic readbacks.
ID3D11ShaderResourceView* depth_srv_for(uint32_t grabSerial, UINT* w, UINT* h);

// VR-39: the AFW foreground mask. While wanted, the scene target is copied once per frame at the first
// foreground viewport (MaxZ < 0.5: the player's arms and weapon) into a second ring keyed like the depth ring.
// `note_viewport` is called from the device's SetViewport hook (render thread). `prefg_srv_for` hands the
// copy for a grab (nullptr when none; `sawForeground` false then means no foreground pass was seen that frame);
// its read is closed by read_done like the depth ring's.
void set_prefg_wanted(unsigned owner, bool on);   // owner bit: 1 AFW, 2 DLSS; the ring runs while any wants it
bool prefg_ready();
void note_viewport(IDirect3DDevice9* dev, const D3DVIEWPORT9* vp);
void note_draw(IDirect3DDevice9* dev);   // every draw (render thread): takes the armed snapshot at the first scene-target draw
ID3D11ShaderResourceView* prefg_srv_for(uint32_t grabSerial, bool* sawForeground);

// VR-39 run 17: the foreground mask, DRAWN. Every draw under the crushed-depth viewport is issued again into a mask
// slot (constant pixel shader, the game's vertex shader and depth test, no depth or stencil writes) from inside
// orig_draw_* (fgmask_begin, the raw draw, fgmask_end), keyed by the capture serial (fgmask_seal at the grab). AFW
// signs its depth snapshot with it. Runs while AFW wants its foreground mask; `depthprobe fgmask on|off`.
void fgmask_prepare(IDirect3DDevice9* d9, ID3D11Device* d11, ID3D11DeviceContext* ctx, uint32_t w, uint32_t h);
bool fgmask_begin(IDirect3DDevice9* dev);
void fgmask_end(IDirect3DDevice9* dev, HRESULT drawn);
bool fgmask_in_draw();
// Run 19: the game draw that note_draw opened has returned (frame_hooks, after the callbacks).
void note_draw_end();
void fgmask_seal(uint32_t serial);
// The mask of a grab (R = 1 where a foreground draw covered the texel), nullptr when none or not finished; `draws` the
// foreground draws it holds (0 = no foreground pass that frame: nothing is foreground). Closed by read_done.
ID3D11ShaderResourceView* fgmask_srv_for(uint32_t serial, uint32_t* draws, uint32_t* w, uint32_t* h);
void fgmask_read_done(ID3D11DeviceContext* ctx);
void fgmask_reset();
bool fgmask_wanted();
bool fgmask_on();   // the switch alone
void fgmask_set(bool on, const char* who);
void fgmask_beat();

} // namespace dvr::depthprobe
