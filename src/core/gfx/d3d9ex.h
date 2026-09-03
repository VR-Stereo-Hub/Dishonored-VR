// core/gfx/d3d9ex.h - the game's device as D3D9Ex, and the managed-pool
// translation that makes it possible (41.1, session 8).
//
// WHY. The headset image reaches D3D11 through a CPU readback of the game's
// backbuffer on every present, and the tick budget measured that readback as
// the tick's owner at the Quest 3 size: 16 ms of GPU copy per present into
// system memory (GetRenderTargetData at ~1.6 GB/s) plus 7.5 ms of CPU copy
// and upload, against 5 ms of actual 3D work per draw. A shared render
// target (a D3D9 surface opened as a D3D11 texture) keeps the frame in VRAM,
// and D3D9 shares surfaces only on a D3D9Ex device. The proxy owns
// Direct3DCreate9, so it can create the game's device as 9Ex - but a 9Ex
// device refuses D3DPOOL_MANAGED, and the creation census measured UE3
// asking for MANAGED on 4614 of 4665 creations (every static texture, vertex
// and index buffer) and locking those textures READONLY 8433 times before the
// first level was up (its mip streaming copies from the old texture).
//
// THE TRANSLATION ([Device] Managed=), applied inside the creation hooks:
//   none     pass through: the refusals are the measurement (the game dies at
//            its first MANAGED creation; the count is the fact)
//   default  MANAGED -> DEFAULT, usage untouched. Right for vertex and index
//            buffers (lockable in DEFAULT); textures lose their locks (the A/B
//            that proves the shadow matters)
//   dynamic  DEFAULT + D3DUSAGE_DYNAMIC on textures: locks work, but a
//            READONLY lock reads VRAM through an uncached map and can return
//            garbage after streaming - measured as the wrong answer for UE3
//   shadow   DEFAULT texture + a SYSTEMMEM twin with the same levels and
//            format. Every LockRect/UnlockRect/AddDirtyRect on the real
//            texture is redirected to the twin (the class-wide Lock hooks the
//            census installs), and each unlock pushes the twin's dirty
//            regions to the real texture with UpdateTexture: a READONLY lock
//            reads the twin (always complete, every write went through it),
//            the game keeps its pointer to the real texture for everything
//            else. What MANAGED did inside the runtime, done here. The shape
//            Special K's 9Ex upgrade and dgVoodoo use (concepts only).
// Buffers translate to DEFAULT under every mode but none.
//
// [Device] Ex=0|1 is launch-time (the device is created once), default 0;
// `device ex on|off` writes the key for the next launch. Every step fails
// soft with its HRESULT: CreateDeviceEx refused -> the plain CreateDevice on
// the Ex object -> the plain IDirect3D9. The capture's shared mode then works
// or refuses with its own reason (core/gfx/capture).
#pragma once
#include <stdint.h>
#include <windows.h>
#include <d3d9.h>

namespace dvr::status { class Writer; }

namespace dvr::d3d9ex {

enum class Managed { None = 0, Default, Dynamic, Shadow };
bool        parse_managed(const char* s, Managed* out);
const char* managed_name(Managed m);

// The config, read before the first Direct3DCreate9 (config.cpp).
void    set_config(bool ex, Managed m);
bool    ex_wanted();
Managed managed_mode();

// Direct3DCreate9: the object the game gets. Ex=1: Direct3DCreate9Ex, handed
// back as IDirect3D9 (it inherits); on failure, or Ex=0, the plain one.
typedef IDirect3D9* (WINAPI *PFN_Create9)(UINT);
typedef HRESULT (WINAPI *PFN_Create9Ex)(UINT, IDirect3D9Ex**);
IDirect3D9* create_d3d(UINT sdk, PFN_Create9 plain, PFN_Create9Ex ex);
bool is_ex_object(IDirect3D9* d3d);

// hkCreateDevice: the device. On an Ex object: CreateDeviceEx with a
// D3DDISPLAYMODEEX built from pp when fullscreen (NULL when windowed), the
// fallbacks in order, every HRESULT logged; on a plain object: orig.
typedef HRESULT (__stdcall *PFN_CreateDevice)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS*,
                                              IDirect3DDevice9**);
HRESULT create_device(IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND wnd, DWORD flags,
                      D3DPRESENT_PARAMETERS* pp, PFN_CreateDevice orig, IDirect3DDevice9** outDev);
bool device_is_ex();          // the created device answers QueryInterface(IDirect3DDevice9Ex)
bool adapter_luid(LUID* out); // from IDirect3D9Ex::GetAdapterLUID (false on a plain object)

// The translation, called by the creation hooks BEFORE the original call.
// Returns what was done so the census counts it. `texture` = textures, cube
// and volume textures (the DYNAMIC rule); buffers never get DYNAMIC.
enum class Translate { Untouched = 0, Translated, Refused };
Translate translate_texture(DWORD* usage, D3DPOOL* pool);
Translate translate_buffer(DWORD* usage, D3DPOOL* pool);
bool      translating();      // Ex device live and Managed != None

// The shadow. After a translated MANAGED texture was created (the real one,
// now DEFAULT), make its twin; the lock hooks redirect through twin(); an
// unlock pushes the dirty regions; the last Release drops the twin.
void  shadow_register_texture(IDirect3DDevice9* dev, IDirect3DTexture9* real, UINT w, UINT h, UINT levels, D3DFORMAT fmt);
void  shadow_register_cube(IDirect3DDevice9* dev, IDirect3DCubeTexture9* real, UINT edge, UINT levels, D3DFORMAT fmt);
void  shadow_register_volume(IDirect3DDevice9* dev, IDirect3DVolumeTexture9* real, UINT w, UINT h, UINT d, UINT levels, D3DFORMAT fmt);
IDirect3DBaseTexture9* shadow_twin(void* real);   // null = not shadowed
void  shadow_unlocked(void* real);                // UpdateTexture twin -> real
void  shadow_released(void* real);                // the real texture's last Release

// Lines and status.json "device" object.
void log_status();
void status(dvr::status::Writer& w);

} // namespace dvr::d3d9ex
