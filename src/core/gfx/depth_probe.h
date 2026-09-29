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

} // namespace dvr::depthprobe
