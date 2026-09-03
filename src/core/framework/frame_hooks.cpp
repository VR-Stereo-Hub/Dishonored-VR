// core/framework/frame_hooks.cpp - see frame_hooks.h.
#define DVR_CAT ::dvr::log::Cat::present
#include "core/framework/frame_hooks.h"

#include "core/gfx/stereo.h"
#include "core/hooks/vtable.h"
#include "core/util/crash.h"
#include "core/util/log.h"
#include "core/vr/openxr_runtime.h"

#include <d3d11.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace dvr::frame {
namespace {

typedef HRESULT (__stdcall *PFN_CreateDevice)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
                                              D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);
typedef HRESULT (__stdcall *PFN_Present)(IDirect3DDevice9*, const RECT*, const RECT*, HWND,
                                         const RGNDATA*);
typedef HRESULT (__stdcall *PFN_Reset)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

Callbacks         g_cb;
PFN_CreateDevice  g_origCreateDevice = nullptr;
PFN_Present       g_origPresent = nullptr;
PFN_Reset         g_origReset = nullptr;
SetVsConstFn      g_origSetVsConst = nullptr;
SetRenderTargetFn g_origSetRt = nullptr;

uint32_t      g_count = 0;
uint32_t      g_submits = 0;
bool          g_disabled = false;
volatile LONG g_exiting = 0;
float         g_fpsCap = 0.0f;
bool          g_xrLive = false;
LONGLONG      g_qpcFreq = 0;

// XR pose (meters, quaternion; XR LOCAL space: right +X, up +Y, forward -Z)
// -> the 3x4 device-to-tracking matrix the game side consumes. XR LOCAL space
// matches the OpenVR standing space that side was written for.
void pose_to_3x4(const dvr::vr::HeadPose& p, float m[3][4]) {
    const float xx = p.qx * p.qx, yy = p.qy * p.qy, zz = p.qz * p.qz;
    const float xy = p.qx * p.qy, xz = p.qx * p.qz, yz = p.qy * p.qz;
    const float wx = p.qw * p.qx, wy = p.qw * p.qy, wz = p.qw * p.qz;
    m[0][0] = 1 - 2 * (yy + zz); m[0][1] = 2 * (xy - wz);     m[0][2] = 2 * (xz + wy);
    m[1][0] = 2 * (xy + wz);     m[1][1] = 1 - 2 * (xx + zz); m[1][2] = 2 * (yz - wx);
    m[2][0] = 2 * (xz - wy);     m[2][1] = 2 * (yz + wx);     m[2][2] = 1 - 2 * (xx + yy);
    m[0][3] = p.px; m[1][3] = p.py; m[2][3] = p.pz;
}

// [VR] FpsCap (38.14): hold the game at a rock-steady rate. The measured
// stutter cause on Quest: game fps wandering 66-80 against a fixed 72/90 Hz
// display = uneven frame cadence. Pin the game to the display rate (72 with
// VD at 72) or exactly half (45 at 90) and let reprojection keep head motion
// smooth. Sleep-then-spin for sub-millisecond accuracy. XR sessions only.
void fps_cap_wait() {
    if (g_fpsCap <= 0.0f || !g_xrLive) return;
    if (!g_qpcFreq) { LARGE_INTEGER f; QueryPerformanceFrequency(&f); g_qpcFreq = f.QuadPart; }
    if (!g_qpcFreq) return;
    static LONGLONG nextDue = 0;
    LARGE_INTEGER fc; QueryPerformanceCounter(&fc);
    const LONGLONG period = (LONGLONG)((double)g_qpcFreq / g_fpsCap);
    if (nextDue == 0 || fc.QuadPart > nextDue + 4 * period) nextDue = fc.QuadPart; // resync after a hitch
    const LONGLONG wait = nextDue - fc.QuadPart;
    if (wait > 0) {
        const double ms = (double)wait * 1000.0 / (double)g_qpcFreq;
        if (ms > 2.0) Sleep((DWORD)(ms - 1.5));
        do { QueryPerformanceCounter(&fc); } while (fc.QuadPart < nextDue);
    }
    nextDue += period;
}

// The session's coming and going, named once per transition so a log can say
// which runtime served the run (and the crash file can carry it).
void track_session() {
    static bool namedRuntime = false;
    if (!namedRuntime && strcmp(dvr::vr::runtime_name(), "none") != 0) {
        namedRuntime = true;
        DVR_INFO("xr: runtime \"%s\" (instance up; session %s)", dvr::vr::runtime_name(),
                 dvr::vr::session_state_name());
    }
    const bool live = dvr::vr::session_live();
    if (live != g_xrLive) {
        g_xrLive = live;
        if (live) {
            char ctx[160];
            _snprintf(ctx, sizeof(ctx), "backend=openxr runtime=\"%s\"", dvr::vr::runtime_name());
            ctx[sizeof(ctx) - 1] = 0;
            dvr::crash::set_context(ctx);
            DVR_INFO("xr: runtime \"%s\" - session live", dvr::vr::runtime_name());
        } else {
            DVR_INFO("xr: session gone (%s)", dvr::vr::session_state_name());
        }
    }
    static bool readySaid = false;
    if (live && !readySaid && dvr::vr::ever_focused()) {
        readySaid = true;
        DVR_INFO("xr: pipeline READY - frames flow to the headset from here");
    }
}

HRESULT __stdcall hkPresent(IDirect3DDevice9* self, const RECT* src, const RECT* dst, HWND wnd,
                            const RGNDATA* dirty) {
    // 38.79: once the game has announced shutdown, the VR work stands down
    // completely (a user's log ended in a call through freed memory AFTER
    // PreExit). The session comes down here, on the present thread, once.
    if (InterlockedCompareExchange(&g_exiting, 0, 0)) {
        static bool torn = false;
        if (!torn) { torn = true; dvr::stereo::shutdown(); dvr::vr::shutdown("PreExit"); }
        return g_origPresent(self, src, dst, wnd, dirty);
    }
    if (g_cb.pre_tick) g_cb.pre_tick(self);
    // 41.1: a method that presents twice per tick is paced by the runtime's
    // pair pacing (one xrWaitFrame per pair); a per-present cap would halve
    // the tick rate.
    if (!(dvr::stereo::active() && dvr::stereo::active()->presents_per_tick() > 1)) fps_cap_wait();
    ++g_count;
    if (g_disabled) return g_origPresent(self, src, dst, wnd, dirty);

    // Present-head: the runtime layer brings the session up, pumps events,
    // waits for the frame (this is what paces the game to the headset),
    // begins it and locates the head and the views.
    dvr::vr::on_present_begin();
    track_session();

    dvr::stereo::FrameInput in;
    in.frame = g_count;
    dvr::vr::HeadPose hp;
    if (dvr::vr::peek_head_pose(hp)) { pose_to_3x4(hp, in.head); in.headOk = true; }
    float hh = 0.0f, hv = 0.0f;
    if (dvr::vr::headset_half_fov_deg(&hh, &hv) && hh > 0.0f) {
        in.fovOk = true; in.halfFovDeg[0] = hh; in.halfFovDeg[1] = hv;
    }
    float sep = 0.0f;
    if (dvr::vr::eye_separation_m(&sep)) in.ipdM = sep;
    dvr::vr::recommended_eye_size(&in.eyeW, &in.eyeH);
    dvr::stereo::begin_frame(in);

    // 41.1: the projection arming. The runtime submits a projection layer only
    // in camera mode, and nothing in this game armed it before; a method that
    // wants it (or `stereo projection on`) turns it on here, on the transition,
    // and the gameplay verdict is published EVERY present while it holds - the
    // runtime's cinematic fallback drops to the quad on a stale publish, so a
    // silent game side would pin the quad forever.
    {
        static bool wantedPrev = false;
        const bool wanted = dvr::stereo::wants_projection();
        if (wanted != wantedPrev) {
            wantedPrev = wanted;
            dvr::vr::set_camera_mode(wanted);
            DVR_INFO("stereo: projection layer %s - runtime camera mode %s (method %s, override %s)",
                     wanted ? "CLAIMED" : "released", wanted ? "ON" : "off", dvr::stereo::active_name(),
                     dvr::stereo::projection_override_name());
        }
        if (wanted) dvr::vr::publish_gameplay_view(g_cb.gameplay_verdict ? g_cb.gameplay_verdict() : true);
    }

    if (g_cb.game_tick) g_cb.game_tick(self);

    // Present-tail: the method produces the eye texture; the runtime shows it.
    dvr::stereo::FrameDevices devs;
    devs.dev9 = self;
    if (g_cb.d3d11) devs.dev11 = g_cb.d3d11(&devs.ctx11);
    dvr::stereo::FrameOutput out;
    dvr::stereo::end_frame(devs, out);
    if (out.tex) ++g_submits;
    dvr::vr::on_present_end(out.tex);
    return g_origPresent(self, src, dst, wnd, dirty);
}

HRESULT __stdcall hkReset(IDirect3DDevice9* self, D3DPRESENT_PARAMETERS* pp) {
    if (g_cb.before_reset) g_cb.before_reset(pp);
    DVR_INFO("device Reset (%ux%u windowed=%d)", pp ? pp->BackBufferWidth : 0,
             pp ? pp->BackBufferHeight : 0, pp ? (int)pp->Windowed : -1);
    // Every default-pool resource this proxy creates must be released here
    // (38.63: a forgotten one made the game's Reset fail forever).
    dvr::stereo::on_reset();
    return g_origReset(self, pp);
}

HRESULT __stdcall hkSetVsConst(IDirect3DDevice9* self, UINT startReg, const float* data, UINT count) {
    if (g_cb.set_vs_const) return g_cb.set_vs_const(self, startReg, data, count);
    return g_origSetVsConst(self, startReg, data, count);
}

HRESULT __stdcall hkSetRenderTarget(IDirect3DDevice9* self, DWORD idx, IDirect3DSurface9* rt) {
    if (g_cb.set_render_target) return g_cb.set_render_target(self, idx, rt);
    return g_origSetRt(self, idx, rt);
}

HRESULT __stdcall hkCreateDevice(IDirect3D9* self, UINT adapter, D3DDEVTYPE type, HWND wnd,
                                 DWORD flags, D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** outDev) {
    if (g_cb.before_create_device) g_cb.before_create_device(pp);
    HRESULT hr = g_origCreateDevice(self, adapter, type, wnd, flags, pp, outDev);
    DVR_INFO("CreateDevice -> 0x%08lx (%ux%u windowed=%d)", (unsigned long)hr,
             pp ? pp->BackBufferWidth : 0, pp ? pp->BackBufferHeight : 0, pp ? (int)pp->Windowed : -1);
    if (g_cb.after_create_device) g_cb.after_create_device(hr, wnd, pp);
    if (SUCCEEDED(hr) && outDev && *outDev) {
        void* old = PatchVtable(*outDev, 17, (void*)hkPresent);
        if (old && !g_origPresent) g_origPresent = (PFN_Present)old;
        old = PatchVtable(*outDev, 16, (void*)hkReset);
        if (old && !g_origReset) g_origReset = (PFN_Reset)old;
        old = PatchVtable(*outDev, 94, (void*)hkSetVsConst);        // SetVertexShaderConstantF
        if (old && !g_origSetVsConst) g_origSetVsConst = (SetVsConstFn)old;
        old = PatchVtable(*outDev, 37, (void*)hkSetRenderTarget);   // SetRenderTarget
        if (old && !g_origSetRt) g_origSetRt = (SetRenderTargetFn)old;
        DVR_INFO("device hooks installed (Present/Reset/SetVSConstF/SetRenderTarget)");
    }
    return hr;
}

} // namespace

void set_callbacks(const Callbacks& cb) { g_cb = cb; }

bool hook_d3d9(IDirect3D9* d3d) {
    if (!d3d) return false;
    void* old = PatchVtable(d3d, 16, (void*)hkCreateDevice);
    if (old && !g_origCreateDevice) g_origCreateDevice = (PFN_CreateDevice)old;
    return g_origCreateDevice != nullptr;
}

HRESULT orig_set_vs_const(IDirect3DDevice9* dev, UINT startReg, const float* data, UINT count) {
    return g_origSetVsConst ? g_origSetVsConst(dev, startReg, data, count) : E_FAIL;
}

HRESULT orig_set_render_target(IDirect3DDevice9* dev, DWORD idx, IDirect3DSurface9* rt) {
    return g_origSetRt ? g_origSetRt(dev, idx, rt) : E_FAIL;
}

uint32_t count() { return g_count; }
uint32_t submit_count() { return g_submits; }
void set_disabled(bool on) { g_disabled = on; }
bool disabled() { return g_disabled; }
void set_exiting() { InterlockedExchange(&g_exiting, 1); }
bool exiting() { return InterlockedCompareExchange(&g_exiting, 0, 0) != 0; }
void set_fps_cap(float fps) { g_fpsCap = fps; }
float fps_cap() { return g_fpsCap; }
bool xr_live() { return g_xrLive; }

} // namespace dvr::frame
