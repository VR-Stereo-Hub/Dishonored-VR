// core/framework/stage_profile.h - the engine's own render stages, timed.
//
// Dishonored brackets every stage of its scene renderer with D3DPERF_BeginEvent / EndEvent
// ("DPG World", "PrePass", "BasePass", "ShadowedLights", "Translucency", "LightShafts ...",
// "PostProcessEffects": 123 call sites, IDA series pf, 2026-10-04) and imports both from
// d3d9.dll, which is this proxy. The events are emitted only while the engine's draw-event
// switch is set (game/dishonored: `stages on` sets it, byte-verified). So with no hook and no
// stage address the proxy receives a named begin and end for every stage, on the render thread.
//
// While enabled this records per stage: calls, inclusive CPU wall time on the render thread,
// the draws issued inside it, and (with `stages gpu on`) inclusive GPU time from D3D9 timestamp
// queries. Every 5 s it logs the stages, the costliest first. Diagnostic only: default off,
// nothing is measured and nothing is allocated until it is switched on.
//
// Read the numbers as they are: times are INCLUSIVE (a parent contains its children; the depth
// is printed), CPU time includes the driver and this mod's own draw hooks, a GPU interval is the
// time between two points on the GPU's own clock and so includes whatever the GPU did for other
// processes in between, and a stage whose GPU sample was still pending at the report is counted
// in `pending`, not as zero.
#pragma once
#include <d3d9.h>
namespace dvr::stageprof {
void begin(const wchar_t* name);            // the proxy's D3DPERF_BeginEvent
void end();                                 // the proxy's D3DPERF_EndEvent
void set_enabled(bool on);
bool enabled();
void set_gpu(bool on);
bool gpu();
void note_device(IDirect3DDevice9* dev);    // render thread, any hook that has the device
void release_gpu(const char* why);          // before a device reset, and at teardown
extern unsigned g_draws;                    // bumped once per game draw (render thread)
}
