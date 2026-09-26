// core/gfx/clarity.cpp - see clarity.h. Present thread (the stereo method's
// end_frame); the levers are atomics so F10 and the seam can move them live.
#define DVR_CAT ::dvr::log::Cat::perf
#include "core/gfx/clarity.h"
#include "core/gfx/clarity_gpu.h"
#include "core/gfx/clarity_math.h"
#include "core/gfx/motion_gpu.h"
#include "core/gfx/depth_probe.h"
#include "core/gfx/capture.h"
#include <d3d11.h>

#include "core/util/log.h"
#include "core/vr/openxr_runtime.h"
#include "core/vr/pose_record.h"

#include <windows.h>
#include <atomic>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace dvr::clarity {
namespace {

std::atomic<bool>  g_resolve{false};
std::atomic<bool>  g_temporal{false};
std::atomic<float> g_blend{0.15f};
std::atomic<float> g_sharpen{0.40f};
std::atomic<uint32_t> g_epoch{1};   // bumped by any lever change: histories restart

Gpu      g_gpu;
bool     g_initTried = false, g_initOk = false;
uint32_t g_seenEpoch = 0;
View     g_prev[2];

// The window the status line reports (reset each line).
struct Window {
    uint64_t draws = 0, resolved = 0, temporal = 0, plain = 0, refused = 0;
    double motionSum = 0; uint64_t motionN = 0;   // the motion weight the temporal pass used
    uint32_t resets[(int)Reset::Count] = {};
    uint32_t srcW = 0, srcH = 0, outW = 0, outH = 0;
};
Window   g_win;
uint64_t g_winMs = 0;
char     g_summary[200] = "all off";

const char* on_off(bool b) { return b ? "on" : "off"; }

void note_change(const char* what, const char* value, const char* who) {
    g_epoch.fetch_add(1);
    DVR_INFO("clarity: %s -> %s (live, %s); the eye histories restart", what, value, who ? who : "?");
}

// The view an eye image was rendered with: the camera rotation and position
// the pose record carries (the rotator the engine consumed, in degrees) and the
// projection the runtime will claim for it.
View view_for(uint32_t recId, uint32_t w, uint32_t h) {
    View v;
    dvr::pose::Record rec = {};
    if (!recId || !dvr::pose::copy(recId, &rec) || !rec.cam.ok) return v;
    float hfov = dvr::vr::rendered_hfov_deg();
    if (!(hfov > 10.0f && hfov < 170.0f)) hfov = dvr::vr::suggested_hfov_deg();
    if (!(hfov > 10.0f && hfov < 170.0f) || !w || !h) return v;
    v.tanH = tanf(hfov * 0.5f * 3.14159265f / 180.0f);
    v.tanV = v.tanH * (float)h / (float)w;
    v.pitch = rec.cam.pitchDeg; v.yaw = rec.cam.yawDeg; v.roll = rec.cam.rollDeg;
    v.posOk = rec.cam.posOk;
    memcpy(v.pos, rec.cam.pos, sizeof(v.pos));
    v.w = w; v.h = h;
    v.ok = true;
    return v;
}

void status_tick() {
    const uint64_t now = GetTickCount64();
    if (!g_winMs) { g_winMs = now; return; }
    if (now - g_winMs < 5000) return;
    const double s = (now - g_winMs) / 1000.0;
    const Window& w = g_win;
    char resolveText[96];
    if (w.resolved)
        _snprintf_s(resolveText, sizeof(resolveText), _TRUNCATE, "%ux%u -> %ux%u (%.2fx per axis, Catmull-Rom)",
                    w.srcW, w.srcH, w.outW, w.outH, w.outW ? (double)w.srcW / w.outW : 0.0);
    else
        _snprintf_s(resolveText, sizeof(resolveText), _TRUNCATE, "%s",
                    g_resolve.load() ? "ON but NOT RUNNING: the render is not above the runtime's recommended size"
                                     : "off");
    DVR_INFO("clarity: %.0f draws/s | resolve %s | temporal %s blend %.2f: %.0f/s blended; history kept %u, "
             "restarted first %u size %u record %u turn %u move %u fov %u off %u | motion weight mean %.2f (0 = still, "
             "full accumulation; 1 = moving, the new frame dominates) | sharpen %.2f | plain copy %.0f/s, "
             "refused %.0f/s | %.1f MB intermediates | GPU cost: the bridge line's Conversion stage",
             w.draws / s, resolveText, on_off(g_temporal.load()), g_blend.load(), w.temporal / s,
             w.resets[(int)Reset::None], w.resets[(int)Reset::First], w.resets[(int)Reset::Size],
             w.resets[(int)Reset::Record], w.resets[(int)Reset::Turn], w.resets[(int)Reset::Move],
             w.resets[(int)Reset::Fov], w.resets[(int)Reset::Off], w.motionN ? w.motionSum / w.motionN : 0.0,
             g_sharpen.load(), w.plain / s, w.refused / s,
             g_gpu.bytes() / (1024.0 * 1024.0));
    _snprintf_s(g_summary, sizeof(g_summary), _TRUNCATE, "resolve %s | temporal %.0f/s (kept %u, restarted %u) | sharpen %.2f",
                resolveText, w.temporal / s, w.resets[(int)Reset::None],
                w.resets[(int)Reset::First] + w.resets[(int)Reset::Size] + w.resets[(int)Reset::Record] +
                    w.resets[(int)Reset::Turn] + w.resets[(int)Reset::Move] + w.resets[(int)Reset::Fov],
                g_sharpen.load());
    g_win = Window{};
    g_winMs = now;
}

// ---- motion vectors, step 3: the calibration run on the live game --------------------
// Per eye: the previous colour and camera. When the camera moved between two frames of the same
// eye, the error curve over candidate depth scales is measured (motion_gpu.h) and accumulated;
// the log says which scale the game's depth units are, or that no scale explains the motion.
std::atomic<bool> g_calibOn{false};
dvr::motion::CalibGpu g_calib;
bool g_calibTried = false, g_calibOk = false;
struct EyeHist { ID3D11Texture2D* tex = nullptr; ID3D11ShaderResourceView* srv = nullptr; View view; };
EyeHist g_eh[2];
double g_calibSum[dvr::motion::kScales] = {};
double g_calibFlipSum[dvr::motion::kScales] = {};   // the same pairs with the translation reversed (the sign test)
double g_turnNormal = 0, g_turnMirror = 0; int g_turnRuns = 0;   // pure turns: rotation-only, normal vs mirrored
int g_calibRuns = 0, g_calibBest[dvr::motion::kScales] = {}, g_calibNoDepth = 0, g_calibStill = 0;
uint64_t g_calibMs = 0;

void calib_frame(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src, uint32_t w, uint32_t h,
                 int eyeSign, uint32_t recId) {
    if (!g_calibOn.load() || !(eyeSign == -1 || eyeSign == 1) || !src) return;
    if (!g_calibOk) {
        if (g_calibTried) return;
        g_calibTried = true;
        char why[512] = "";
        g_calibOk = g_calib.init(dev, why, sizeof(why));
        if (!g_calibOk) { DVR_ERROR("motion/calib: unavailable (%s)", why); return; }
    }
    EyeHist& e = g_eh[eyeSign < 0 ? 0 : 1];
    const View cur = view_for(recId, w, h);
    ID3D11Resource* res = nullptr;
    src->GetResource(&res);
    if (!res || !cur.ok) { if (res) res->Release(); e.view = View{}; return; }
    D3D11_TEXTURE2D_DESC sd = {};
    ((ID3D11Texture2D*)res)->GetDesc(&sd);
    if (e.tex) { D3D11_TEXTURE2D_DESC hd = {}; e.tex->GetDesc(&hd); if (hd.Width != sd.Width || hd.Height != sd.Height || hd.Format != sd.Format) {
        e.srv->Release(); e.tex->Release(); e.srv = nullptr; e.tex = nullptr; e.view = View{}; } }
    if (!e.tex) {
        D3D11_TEXTURE2D_DESC td = sd;
        td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE; td.CPUAccessFlags = 0; td.MiscFlags = 0;
        td.MipLevels = 1; td.ArraySize = 1;
        if (FAILED(dev->CreateTexture2D(&td, nullptr, &e.tex)) || FAILED(dev->CreateShaderResourceView(e.tex, nullptr, &e.srv))) {
            if (e.tex) { e.tex->Release(); e.tex = nullptr; }
            res->Release(); return;
        }
        e.view = View{};
    }
    if (e.view.ok && e.view.posOk && cur.posOk) {
        const float dp[3] = {cur.pos[0] - e.view.pos[0], cur.pos[1] - e.view.pos[1], cur.pos[2] - e.view.pos[2]};
        const float moved = sqrtf(dp[0] * dp[0] + dp[1] * dp[1] + dp[2] * dp[2]);
        const Basis tpb = basis_from_rotator(e.view.pitch, e.view.yaw, e.view.roll);
        const Basis tcb = basis_from_rotator(cur.pitch, cur.yaw, cur.roll);
        const float turned = basis_angle_deg(tpb, tcb);
        if (moved < 0.3f && turned > 0.5f && turned < 20.0f) {   // the mirror test: a pure turn
            UINT dw = 0, dh = 0;
            ID3D11ShaderResourceView* depth = dvr::depthprobe::depth_srv_for(dvr::capture::delivered_serial(), &dw, &dh);
            if (depth && dw == w && dh == h) {
                dvr::motion::CalibParams p;
                p.prevFromCur = prev_from_cur(tpb, tcb);
                p.tanH = cur.tanH; p.tanV = cur.tanV; p.w = w; p.h = h; p.farDepth = 1000.0f;
                dvr::motion::CalibParams mp = p;
                for (int i = 0; i < 3; ++i)
                    for (int j = 0; j < 3; ++j) mp.prevFromCur.m[i][j] *= (i == 1 ? -1.0f : 1.0f) * (j == 1 ? -1.0f : 1.0f);
                float a[dvr::motion::kScales], b[dvr::motion::kScales]; int na = 0, nb = 0; char w1[64], w2[64];
                if (g_calib.run(dev, ctx, src, e.srv, depth, p, a, &na, w1, sizeof(w1)) &&
                    g_calib.run(dev, ctx, src, e.srv, depth, mp, b, &nb, w2, sizeof(w2)) && na > 200 && nb > 200) {
                    g_turnNormal += a[dvr::motion::kScales - 1]; g_turnMirror += b[dvr::motion::kScales - 1]; ++g_turnRuns;
                    DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 8,
                        "motion/calib: MIRROR TEST eye %+d turned %.2f deg (moved %.2f uu) | rotation-only error: normal %.4f, "
                        "right axis mirrored %.4f", eyeSign, turned, moved, a[dvr::motion::kScales - 1], b[dvr::motion::kScales - 1]);
                }
            }
        }
        if (moved < 2.0f || moved > 80.0f) ++g_calibStill;
        else {
            UINT dw = 0, dh = 0;
            ID3D11ShaderResourceView* depth = dvr::depthprobe::depth_srv_for(dvr::capture::delivered_serial(), &dw, &dh);
            if (!depth || dw != w || dh != h) ++g_calibNoDepth;
            else {
                const Basis pb = basis_from_rotator(e.view.pitch, e.view.yaw, e.view.roll);
                const Basis cb = basis_from_rotator(cur.pitch, cur.yaw, cur.roll);
                dvr::motion::CalibParams p;
                p.prevFromCur = prev_from_cur(pb, cb);
                p.t[0] = dot3(pb.f, dp); p.t[1] = dot3(pb.r, dp); p.t[2] = dot3(pb.u, dp);
                p.tanH = cur.tanH; p.tanV = cur.tanV; p.w = w; p.h = h; p.farDepth = 1000.0f;
                float err[dvr::motion::kScales]; int n = 0; char why[256] = "";
                if (g_calib.run(dev, ctx, src, e.srv, depth, p, err, &n, why, sizeof(why)) && n > 200) {
                    int best = 0;
                    for (int k = 1; k < dvr::motion::kScales; ++k) if (err[k] >= 0 && err[k] < err[best]) best = k;
                    ++g_calibBest[best]; ++g_calibRuns;
                    for (int k = 0; k < dvr::motion::kScales; ++k) g_calibSum[k] += err[k] >= 0 ? err[k] : 0;
                    {   // the sign test: a reversed translation must score WORSE if the convention is right
                        dvr::motion::CalibParams f = p;
                        f.t[0] = -p.t[0]; f.t[1] = -p.t[1]; f.t[2] = -p.t[2];
                        float fe[dvr::motion::kScales]; int fn = 0; char fw[64] = "";
                        if (g_calib.run(dev, ctx, src, e.srv, depth, f, fe, &fn, fw, sizeof(fw)))
                            for (int k = 0; k < dvr::motion::kScales; ++k) g_calibFlipSum[k] += fe[k] >= 0 ? fe[k] : 0;
                    }
                    DVR_LOG_FIRST_N(DVR_CAT, ::dvr::log::Level::Info, 12,
                        "motion/calib: eye %+d moved %.1f uu | error 25:%.4f 100:%.4f 400:%.4f 1000:%.4f 1500:%.4f 2500:%.4f "
                        "7000:%.4f rotation-only:%.4f | best %g uu per depth unit (%d samples)",
                        eyeSign, moved, err[0], err[2], err[4], err[6], err[7], err[8], err[10], err[11],
                        dvr::motion::kScaleValues[best], n);
                }
            }
        }
    }
    ctx->CopyResource(e.tex, res);
    res->Release();
    e.view = cur;
    const uint64_t now = GetTickCount64();
    if (now - g_calibMs >= 5000 && g_turnRuns)
        DVR_INFO("motion/calib: MIRROR TEST over %d pure turns - rotation-only error normal %.4f, mirrored %.4f (the lower "
                 "one is the convention the image really has)", g_turnRuns, g_turnNormal / g_turnRuns, g_turnMirror / g_turnRuns);
    if (now - g_calibMs >= 5000 && g_calibRuns) {
        g_calibMs = now;
        char t[600]; int m = 0;
        for (int k = 0; k < dvr::motion::kScales; ++k)
            m += _snprintf_s(t + m, sizeof(t) - m, _TRUNCATE, " %g:%.4f(%d)", dvr::motion::kScaleValues[k],
                             g_calibSum[k] / g_calibRuns, g_calibBest[k]);
        int best = 0;
        for (int k = 1; k < dvr::motion::kScales; ++k) if (g_calibSum[k] < g_calibSum[best]) best = k;
        {
            char ft[400]; int fm = 0, fb = 0;
            for (int k = 0; k < dvr::motion::kScales; ++k) {
                fm += _snprintf_s(ft + fm, sizeof(ft) - fm, _TRUNCATE, " %g:%.4f", dvr::motion::kScaleValues[k], g_calibFlipSum[k] / g_calibRuns);
                if (g_calibFlipSum[k] < g_calibFlipSum[fb]) fb = k;
            }
            DVR_INFO("motion/calib: SIGN TEST, the same pairs with the translation reversed:%s | best %g (a reversed "
                     "translation that fits better than the real one means the camera convention is flipped)",
                     ft, dvr::motion::kScaleValues[fb]);
        }
        DVR_INFO("motion/calib: %d moving frame pairs | mean error (times best) per uu-per-depth-unit:%s | "
                 "BEST %g%s | skipped: still %d, no matching depth %d",
                 g_calibRuns, t, dvr::motion::kScaleValues[best],
                 best == dvr::motion::kScales - 1 ? " = ROTATION ONLY: the depth does not explain the motion (wrong eye/frame pairing, or not depth)" : "",
                 g_calibStill, g_calibNoDepth);
    }
}

} // namespace

void set_calib(bool on, const char* who) {
    g_calibOn.store(on);
    DVR_INFO("motion/calib: %s (%s)%s", on ? "ON" : "off", who ? who : "?",
             on ? " - needs [Diagnostics] DepthShare=1; measures the depth scale whenever the camera moves" : "");
}

void set_resolve(bool on, const char* who) {
    if (g_resolve.exchange(on) != on) note_change("resolve", on_off(on), who);
}
bool resolve_on() { return g_resolve.load(); }
void set_temporal(bool on, const char* who) {
    if (g_temporal.exchange(on) != on) note_change("temporal", on_off(on), who);
}
bool temporal_on() { return g_temporal.load(); }
void set_blend(float v, const char* who) {
    if (!(v >= 0.05f)) v = 0.05f;
    if (v > 0.5f) v = 0.5f;
    if (fabsf(g_blend.exchange(v) - v) > 1e-4f) {
        char t[16]; _snprintf_s(t, sizeof(t), _TRUNCATE, "%.2f", v);
        note_change("temporal blend", t, who);
    }
}
float blend() { return g_blend.load(); }
void set_sharpen(float v, const char* who) {
    if (!(v >= 0.0f)) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    if (fabsf(g_sharpen.exchange(v) - v) > 1e-4f) {
        char t[16]; _snprintf_s(t, sizeof(t), _TRUNCATE, "%.2f", v);
        DVR_INFO("clarity: sharpen -> %s (live, %s)", t, who ? who : "?");
    }
}
float sharpen() { return g_sharpen.load(); }
bool any_on() { return g_resolve.load() || g_temporal.load() || g_sharpen.load() > 0.0f; }

void output_size(uint32_t w, uint32_t h, uint32_t* ow, uint32_t* oh) {
    *ow = w; *oh = h;
    if (!g_resolve.load()) return;
    uint32_t rw = 0, rh = 0;
    if (!dvr::vr::recommended_eye_size(&rw, &rh)) return;
    uint32_t tw = w, th = h;
    if (resolve_size(w, h, rw, rh, &tw, &th)) {
        static uint32_t saidW = 0, saidH = 0, saidOw = 0, saidOh = 0;
        if (w != saidW || h != saidH || tw != saidOw || th != saidOh) {
            saidW = w; saidH = h; saidOw = tw; saidOh = th;
            DVR_INFO("clarity: resolve target %ux%u -> %ux%u (%.2fx per axis; the runtime recommends %ux%u, "
                     "kept at the render's aspect). The swapchain follows this size, so VDXR receives the "
                     "pixel count it asked for instead of resampling a larger image itself",
                     w, h, tw, th, (double)w / tw, rw, rh);
        }
        *ow = tw; *oh = th;
    }
}

bool draw(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src,
          uint32_t w, uint32_t h, ID3D11RenderTargetView* dst, uint32_t ow, uint32_t oh,
          int eyeSign, uint32_t recId) {
    calib_frame(dev, ctx, src, w, h, eyeSign, recId);   // motion vectors step 3 (off by default)
    if (!any_on()) { if (g_initOk && g_gpu.bytes()) g_gpu.trim(false, false); return false; }
    if (!dev || !ctx || !src || !dst) return false;
    if (!g_initOk) {
        if (g_initTried) return false;
        g_initTried = true;
        char why[512] = "";
        g_initOk = g_gpu.init(dev, why, sizeof(why));
        if (!g_initOk) {
            DVR_ERROR("clarity: the passes are unavailable (%s) - the plain copy runs and every clarity lever is inert", why);
            return false;
        }
        DVR_INFO("clarity: pipeline ready (resolve, temporal, sharpen shaders compiled)");
    }
    const uint32_t epoch = g_epoch.load();
    if (epoch != g_seenEpoch) {
        g_seenEpoch = epoch;
        if (g_prev[0].ok || g_prev[1].ok) { ++g_win.resets[(int)Reset::Off]; }
        g_prev[0] = View{}; g_prev[1] = View{};
    }
    PassParams p;
    p.srcGamma = true;
    p.w = w; p.h = h; p.ow = ow; p.oh = oh;
    p.resolve = g_resolve.load() && (ow < w || oh < h);
    p.sharpen = g_sharpen.load();
    p.blend = g_blend.load();
    p.clipGamma = 1.0f;
    if (g_gpu.bytes()) g_gpu.trim(p.resolve, g_temporal.load());
    if (g_temporal.load()) {
        if (eyeSign == -1 || eyeSign == 1) {
            const int e = eyeSign < 0 ? 0 : 1;
            const View cur = view_for(recId, ow, oh);
            const Reset why = keep_history(g_prev[e], cur);
            ++g_win.resets[(int)why];
            if (cur.ok) {
                p.temporal = true;
                p.eye = e;
                p.historyValid = why == Reset::None;
                if (p.historyValid) {
                    const Basis pb = basis_from_rotator(g_prev[e].pitch, g_prev[e].yaw, g_prev[e].roll);
                    const Basis cb = basis_from_rotator(cur.pitch, cur.yaw, cur.roll);
                    p.prevFromCur = prev_from_cur(pb, cb);
                    // The reprojection is rotation-only: walking parallax, and a fast
                    // turn's resampling, are what smear. As the camera moves the history
                    // counts for less (and its clip tightens), so a still or slowly
                    // looking view keeps the full accumulation and a moving one stays sharp.
                    float move = 0.0f;
                    if (g_prev[e].posOk && cur.posOk) {
                        const float dx = cur.pos[0] - g_prev[e].pos[0], dy = cur.pos[1] - g_prev[e].pos[1],
                                    dz = cur.pos[2] - g_prev[e].pos[2];
                        move = sqrtf(dx * dx + dy * dy + dz * dz);
                    }
                    const float m = motion_weight(move, basis_angle_deg(pb, cb));
                    p.blend = p.blend + (0.6f - p.blend) * m;
                    p.clipGamma = 1.0f - 0.35f * m;
                    g_win.motionSum += m; ++g_win.motionN;
                }
                p.tanH = cur.tanH; p.tanV = cur.tanV;
            }
            g_prev[e] = cur;
        } else {
            // An untagged present is a transition (menu, load, mono): neither eye's
            // history belongs to what comes after it.
            if (g_prev[0].ok || g_prev[1].ok) ++g_win.resets[(int)Reset::Record];
            g_prev[0] = View{}; g_prev[1] = View{};
        }
    }
    if (!p.resolve && !p.temporal && p.sharpen <= 0.0f && ow == w && oh == h) {
        ++g_win.plain;   // nothing to do this present (resolve not needed, untagged): the plain copy
        status_tick();
        return false;
    }
    char why[256] = "";
    if (!g_gpu.run(dev, ctx, src, dst, p, why, sizeof(why))) {
        ++g_win.refused;
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
                         "clarity: pass refused (%s) at %ux%u -> %ux%u - this present takes the plain copy", why, w, h, ow, oh);
        status_tick();
        return false;
    }
    ++g_win.draws;
    if (p.resolve) { ++g_win.resolved; g_win.srcW = w; g_win.srcH = h; g_win.outW = ow; g_win.outH = oh; }
    if (p.temporal && p.historyValid) ++g_win.temporal;
    status_tick();
    return true;
}

void shutdown() {
    g_gpu.shutdown();
    g_initTried = false; g_initOk = false;
    g_prev[0] = View{}; g_prev[1] = View{};
}

const char* summary() { return any_on() ? g_summary : "all off"; }

bool command(const char* args) {
    char sub[24] = "", val[24] = "";
    const int n = args ? sscanf(args, "%23s %23s", sub, val) : 0;
    auto onoff = [&](bool* out) {
        if (!_stricmp(val, "on") || !strcmp(val, "1")) { *out = true; return true; }
        if (!_stricmp(val, "off") || !strcmp(val, "0")) { *out = false; return true; }
        return false;
    };
    bool b = false;
    if (n >= 2 && !_stricmp(sub, "resolve") && onoff(&b)) { set_resolve(b, "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "temporal") && onoff(&b)) { set_temporal(b, "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "blend")) { set_blend((float)atof(val), "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "sharpen")) { set_sharpen((float)atof(val), "the seam"); return true; }
    if (n >= 1 && !_stricmp(sub, "off")) {
        set_resolve(false, "the seam"); set_temporal(false, "the seam"); set_sharpen(0.0f, "the seam");
        return true;
    }
    DVR_INFO("clarity: resolve %s, temporal %s (blend %.2f), sharpen %.2f | %s | words: clarity resolve on|off, "
             "temporal on|off, blend <0.05..0.5>, sharpen <0..1>, off",
             on_off(g_resolve.load()), on_off(g_temporal.load()), g_blend.load(), g_sharpen.load(), summary());
    return true;
}

} // namespace dvr::clarity
