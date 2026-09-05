// core/gfx/device_census.h - what the game asks of its D3D9 device (41.1, session 8).
//
// The GPU-resident capture needs the game's device created as D3D9Ex, and a
// 9Ex device refuses D3DPOOL_MANAGED, which UE3's D3D9 RHI is BELIEVED to
// depend on (ENGINE_NOTES, "The capture cost, measured"). This module turns
// that belief into a measurement before any Ex code runs: it patches the
// device's eight resource-creation calls and counts every creation by call,
// pool, usage class and format class (with bytes, failures and the first
// failure's full ask), and it patches each resource class's Lock once so the
// question that decides the TRANSLATION - not "how many MANAGED" but "how
// does UE3 lock them" (a plain write lock survives DEFAULT + DYNAMIC; a
// READONLY lock of a streaming source does not) - is a table in the log.
//
// Always on: creation calls are rare and a lock is one hash lookup and one
// increment. The summary prints once at the first GAMEPLAY and on `device
// census`; deltas print every 60 s at Debug; `device status` is one line; the
// status.json `census` object carries the verdict counts. Never changes a
// creation: the translation (core/gfx/d3d9ex) is a separate lever.
#pragma once
#include <stdint.h>
#include <windows.h>
#include <d3d9.h>

namespace dvr::status { class Writer; }

namespace dvr::census {

// hkCreateDevice: patch the creation slots on this device and log the
// creation flags, the present parameters and the caps that decide the
// translation. Idempotent per device (PatchVtable refuses a re-patch).
void install(IDirect3DDevice9* dev, IDirect3D9* d3d, UINT adapter, D3DDEVTYPE type, DWORD createFlags,
             const D3DPRESENT_PARAMETERS* pp);

// The table and the verdict, at Info; `why` names the trigger.
void log_summary(const char* why);
// Rows that moved since the last delta, at Debug (a "0 rows moved" line too).
void log_deltas();
// One line: creations, MANAGED count and bytes, what 9Ex would refuse.
void log_status();

// VR-15 (the black texture bug): the UPLOAD census. The creation census says
// what the game asked for; this says whether what the game WROTE into a
// texture ever reached the GPU copy it draws from. Under [Device]
// Managed=shadow a write is lost when the lock is refused, when the texture
// has no twin, or when the game took a surface off the texture and locked
// that instead (GetSurfaceLevel bypasses the texture vtable entirely). Each
// is counted with its first HRESULT and the verdict prints "NO LOST UPLOAD"
// just as readily - four zeros falsify the shadow as the cause. Rides with
// log_summary and answers `device upload`.
void log_upload(const char* why);

// [Device] ShadowSurfaces (default OFF, `device shadowsurfaces on|off` live):
// redirect a lock taken through GetSurfaceLevel to the twin's matching
// surface, the way the texture path already is. The A/B for the bypass.
void set_shadow_surfaces(bool on);
bool shadow_surfaces();
// status.json "census" object.
void status(dvr::status::Writer& w);

// The verdict counts (cumulative).
uint32_t creations();
uint32_t managed_creations();
uint64_t managed_bytes();
uint32_t readonly_locks_on_managed();

} // namespace dvr::census
