// core/gfx/frame_id.h - the frame-identity trace (41.1, session 9).
//
// THE QUESTION. After a level load the headset showed the SAME picture in
// both eyes while every tag instrument read clean (headset runs 16-17): the
// two draws of a tick uploaded camera positions one IPD apart (c5), the tags
// alternated, the slots alternated, and the two eye dumps were one image.
// Nothing in the build could say at which STAGE the two images became one:
// the game's backbuffer as the capture found it, the shared slot after the
// blit, the method's output texture after the blit_quad, or the swapchain
// image the runtime copied into.
//
// THE INSTRUMENT. One 64x64 luma thumbnail per present at each of the four
// stages, keyed by the capture serial of the pixels (so the thumbnails of one
// frame line up across the stages whatever the delivery lag), read three
// presents later and never waited on (D3D9 DONOTWAIT / D3D11 DO_NOT_WAIT; a
// busy read is counted, not waited for), with the c5 the constant hook saw at
// the grab riding on the same record - the camera and the pixels of ONE draw,
// on one line. Per pair (a -1 record and the +1 record after it) the line
// prints the checksums and the mean absolute luma difference per stage,
// beside the same-eye floor (this left against the previous left: what one
// tick of head motion costs), and names the first stage at which the two
// eyes read as one picture. It is evidence, never a trigger: nothing here
// re-arms, switches or kicks anything.
//
//   stage bb    D3D9: StretchRect(backbuffer -> 64x64 RT), GetRenderTargetData
//               into a system-memory surface (capture::grab, every grab)
//   stage slot  D3D11: the delivered slot's SRV drawn into a 64x64 RT (what
//               the method samples, inside its read fence)
//   stage out   D3D11: the method's output texture, the same way
//   stage sc    D3D11: the centre 64x64 of the swapchain image after the
//               runtime's CopyResource (the 41.0 frame-texture seam)
//
// Comparisons are LEFT vs RIGHT within a stage. Stage bb is a filtered
// downscale and stage sc a centre patch, so across stages the numbers are
// not comparable and are not compared.
//
// [Perf] FrameId=1 ships it ON (16 KB of readback per present, three small
// draws); `frameid on|off|status` live. The summary prints every 3 s; the
// pair line at most once a second; a state CHANGE (the eyes became one
// picture / two pictures at stage bb) once, at Warn.
#pragma once
#include <stdint.h>

struct IDirect3DDevice9;
struct IDirect3DSurface9;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
namespace dvr::status { class Writer; }

namespace dvr::frameid {

void set_enabled(bool on);
bool enabled();
// One pair every n ticks is sampled (default 8): the backbuffer stage's readback
// is a pipeline sync on some GPUs (headset run 07: 1.5 ms of GPU idle per
// present when read every present). [Perf] FrameIdEvery, `frameid every N`.
void set_every(uint32_t n);
uint32_t every();

// Present thread, in this order per present:
//  0. the method, first thing: the previous present's delivery is closed (its
//     sc stage ran in the runtime's tail, after the method returned), the
//     serials old enough are judged, the lines print
void begin_present();
//  1. the method, before capture::grab: the c5 the constant hook saw for the
//     frame the game just drew (the grab attaches it to its serial), and the
//     camera's right row for the side check (a +1 present's c5 must sit at
//     -ipd*scale along it: the other sign = the tags rode the other draw)
void note_c5(const float c5[3], bool ok, const float right[3], bool rightOk);

//  2. capture::grab, right after the backbuffer is fetched: stage bb for
//     grab `serial` with the tag the method attached
void stage_backbuffer(IDirect3DDevice9* dev, IDirect3DSurface9* bb, uint32_t serial, int tag);
//  3. the method, after its draw and BEFORE its read fence: the delivery
//     (which serial, which slot) then stages slot and out
void note_delivery(uint32_t serial, int tag, int slot, const char* modeName);
void stage_slot(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* slotSrv);
void stage_out(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* outSrv);
//  4. the runtime, after CopyResource into the acquired swapchain image and
//     before its release: stage sc for the serial note_delivery named
void stage_swapchain(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11Texture2D* image, int target, uint32_t index);


void on_reset();     // the D3D9 ring goes (default pool) before the device resets
void shutdown();
void log_status();   // `frameid status`
// The last judged pair, for the F10 Display tab: the L-R difference per stage
// (bb, slot, out, sc; -1 = the stage was not read), the same-eye floor, the
// side check, the picture shift, the state, the lifetime counts.
struct Last {
    uint32_t pairs = 0;
    float    diff[4] = {-1, -1, -1, -1};
    float    floorBb = -1;
    char     side[12] = "-";
    int      shift = 0;
    bool     onePicture = false;
    uint32_t swapped = 0;
};
Last last();
void status(dvr::status::Writer& w);   // status.json "frameid"

} // namespace dvr::frameid
