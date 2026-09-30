// core/gfx/clarity.cpp - see clarity.h. Present thread (the stereo method's
// end_frame); the levers are atomics so F10 and the seam can move them live.
#define DVR_CAT ::dvr::log::Cat::perf
#include "core/gfx/clarity.h"
#include "core/gfx/clarity_gpu.h"
#include "core/gfx/clarity_math.h"
#include "core/gfx/motion_gpu.h"
#include "core/gfx/depth_probe.h"
#include "core/gfx/capture.h"
#include "core/gfx/dlss.h"
#include "core/gfx/dlss_gpu.h"
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
std::atomic<bool>  g_motion{false};
std::atomic<float> g_depthScale{250.0f};   // flow-check fit through the render matrices (PERFORMANCE.md)
std::atomic<float> g_blend{0.15f};
std::atomic<float> g_sharpen{0.40f};
std::atomic<uint32_t> g_epoch{1};
std::atomic<int>   g_posSource{1};
std::atomic<float> g_tAxis[3] = {1.0f, 1.0f, 1.0f};
std::atomic<float> g_bodyDepth{0.3f};
std::atomic<bool>  g_useVp{true};
dvr::pose::Record  g_dlssPrevRec[2];
bool               g_dlssPrevRecOk[2] = {};   // 1: the rendered c5 (measured better while walking), 0: the written position   // bumped by any lever change: histories restart

Gpu      g_gpu;
bool     g_initTried = false, g_initOk = false;
uint32_t g_seenEpoch = 0;
View     g_prev[2];
View     g_dlssPrev[2];   // DLAA's own eye histories (dlss.h); the custom TAA's stay in g_prev

// The window the status line reports (reset each line).
struct Window {
    uint64_t draws = 0, resolved = 0, temporal = 0, plain = 0, refused = 0, dlaa = 0;
    uint64_t vectors[2] = {}, motionFallback = 0;
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
    dvr::depthprobe::retry();
    DVR_INFO("clarity: %s -> %s (live, %s); the eye histories restart", what, value, who ? who : "?");
}

// The view an eye image was rendered with: the camera rotation and position
// the pose record carries (the rotator the engine consumed, in degrees) and the
// projection the runtime will claim for it.
View view_for(uint32_t recId, uint32_t w, uint32_t h, int eyeSign) {
    View v;
    dvr::pose::Record rec = {};
    if (!recId || !dvr::pose::copy(recId, &rec) || !rec.cam.ok || rec.eye != eyeSign) return v;
    const float hfov = rec.hfovDeg;
    if (!(hfov > 10.0f && hfov < 170.0f) || !w || !h) return v;
    v.tanH = tanf(hfov * 0.5f * 3.14159265f / 180.0f);
    v.tanV = v.tanH * (float)h / (float)w;
    v.pitch = rec.cam.pitchDeg; v.yaw = rec.cam.yawDeg; v.roll = rec.cam.rollDeg;
    v.posOk = rec.eyePosOk;
    v.timeMs = rec.openedMs; v.cameraIdentity = rec.cameraIdentity; v.sceneEpoch = rec.sceneEpoch;
    // last_written_pos publishes c5 = -world position (ENGINE_NOTES, 2026-09-03).
    // Keep the transport record unchanged; convert once at this consumer boundary.
    if (g_posSource.load() == 1 && rec.renderPosOk) world_from_c5(rec.renderPos, v.pos);
    else world_from_c5(rec.eyePos, v.pos);
    v.posOk = g_posSource.load() == 1 ? (rec.renderPosOk || rec.eyePosOk) : rec.eyePosOk;
    {   // How far the written position is from the rendered one, per eye, and how different
        // their frame-to-frame steps are: the part of the camera translation the record gets wrong.
        static float lastW[2][3], lastR[2][3]; static bool have[2];
        static double sumOff[2], sumStep[2], sumTrue[2]; static uint32_t n[2]; static uint64_t ms;
        const int e = eyeSign < 0 ? 0 : 1;
        if (rec.renderPosOk && rec.eyePosOk) {
            float off = 0, dstep = 0, tstep = 0;
            for (int j = 0; j < 3; ++j) {
                const float d = rec.renderPos[j] - rec.eyePos[j]; off += d * d;
                if (have[e]) {
                    const float sw = rec.eyePos[j] - lastW[e][j], sr = rec.renderPos[j] - lastR[e][j];
                    dstep += (sw - sr) * (sw - sr); tstep += sr * sr;
                }
                lastW[e][j] = rec.eyePos[j]; lastR[e][j] = rec.renderPos[j];
            }
            if (have[e]) { sumOff[e] += sqrtf(off); sumStep[e] += sqrtf(dstep); sumTrue[e] += sqrtf(tstep); ++n[e]; }
            have[e] = true;
        } else have[e] = false;
        const uint64_t now = GetTickCount64();
        if (now - ms >= 5000 && (n[0] || n[1])) {
            DVR_INFO("clarity/pos: written vs rendered camera (uu), per eye L|R: mean offset %.2f|%.2f, mean step error %.2f|%.2f "
                     "against a mean rendered step of %.2f|%.2f (%u|%u pairs); vectors use the %s position",
                     n[0] ? sumOff[0] / n[0] : 0.0, n[1] ? sumOff[1] / n[1] : 0.0, n[0] ? sumStep[0] / n[0] : 0.0,
                     n[1] ? sumStep[1] / n[1] : 0.0, n[0] ? sumTrue[0] / n[0] : 0.0, n[1] ? sumTrue[1] / n[1] : 0.0,
                     n[0], n[1], g_posSource.load() == 1 ? "RENDERED" : "written");
            for (int k = 0; k < 2; ++k) { sumOff[k] = sumStep[k] = sumTrue[k] = 0; n[k] = 0; }
            ms = now;
        }
    }
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
    if (g_motion.load()) DVR_INFO("clarity/motion: completed depth-vector TAA L=%llu R=%llu, rotation fallback=%llu, scale %.1f uu/unit",
        (unsigned long long)w.vectors[0], (unsigned long long)w.vectors[1], (unsigned long long)w.motionFallback, g_depthScale.load());
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
    if (!g_calibOn.load()) {
        if (g_calibTried) {
            g_calib.shutdown(); g_calibTried = g_calibOk = false;
            for (auto& e : g_eh) { if (e.srv) e.srv->Release(); if (e.tex) e.tex->Release(); e = EyeHist{}; }
        }
        return;
    }
    if (!(eyeSign == -1 || eyeSign == 1) || !src) {
        g_eh[0].view = View{}; g_eh[1].view = View{};
        return;
    }
    if (!g_calibOk) {
        if (g_calibTried) return;
        g_calibTried = true;
        char why[512] = "";
        g_calibOk = g_calib.init(dev, why, sizeof(why));
        if (!g_calibOk) { DVR_ERROR("motion/calib: unavailable (%s)", why); return; }
    }
    EyeHist& e = g_eh[eyeSign < 0 ? 0 : 1];
    const View cur = view_for(recId, w, h, eyeSign);
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
    if (keep_history(e.view, cur) == Reset::None && e.view.posOk && cur.posOk) {
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
    if (now - g_calibMs >= 5000) g_calibMs = now;
}

} // namespace

void set_calib(bool on, const char* who) {
    g_calibOn.store(on);
    DVR_INFO("motion/calib: %s (%s)%s", on ? "ON" : "off", who ? who : "?",
             on ? " - needs [Diagnostics] DepthShare=1; measures the depth scale whenever the camera moves" : "");
}

void set_pos_source(int src, const char* who) {
    src = src ? 1 : 0;
    if (g_posSource.exchange(src) != src) note_change("vector camera position", src ? "rendered c5" : "written", who);
}
int pos_source() { return g_posSource.load(); }
void set_body_depth(float z, const char* who) {
    if (!(z >= 0.0f && z <= 5.0f)) z = 0.3f;
    g_bodyDepth.store(z);
    DVR_INFO("clarity: DLSS body depth %.3f depth units (%s): nearer pixels are the arms/weapon - rotation only%s",
             z, who ? who : "?", z > 0 ? "" : " (OFF: every pixel gets the walking parallax)");
}
float body_depth() { return g_bodyDepth.load(); }
void set_use_vp(bool on, const char* who) {
    g_useVp.store(on);
    DVR_INFO("clarity: DLSS vectors from %s (%s)", on ? "the game's own view-projection matrices" : "the rotator/FOV reconstruction",
             who ? who : "?");
}
bool use_vp() { return g_useVp.load(); }
void set_translation_axes(float f, float r, float u, const char* who) {
    g_tAxis[0].store(f); g_tAxis[1].store(r); g_tAxis[2].store(u);
    DVR_INFO("clarity: DLSS guide translation axes x%.2f forward, x%.2f right, x%.2f up (%s, diagnostic)", f, r, u, who ? who : "?");
}

void set_resolve(bool on, const char* who) {
    if (g_resolve.exchange(on) != on) note_change("resolve", on_off(on), who);
}
bool resolve_on() { return g_resolve.load(); }
void set_temporal(bool on, const char* who) {
    if (g_temporal.exchange(on) != on) note_change("temporal", on_off(on), who);
}
bool temporal_on() { return g_temporal.load(); }
void set_motion(bool on, const char* who) {
    if (g_motion.exchange(on) != on) note_change("motion vectors", on_off(on), who);
}
bool motion_on() { return g_motion.load(); }
void set_depth_scale(float v, const char* who) {
    if (!isfinite(v) || v < 25 || v > 7000) return;
    if (g_depthScale.exchange(v) != v) {
        g_epoch.fetch_add(1);
        DVR_INFO("clarity: depth scale %.1f uu/unit (%s); histories restart", v, who);
    }
}
float depth_scale() { return g_depthScale.load(); }
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
    // DLSS Super Resolution: this eye image is the reduced render of a larger output.
    if (dvr::dlss::sr_output_for(w, h, ow, oh)) {
        static uint32_t saidW = 0, saidH = 0;
        if (saidW != *ow || saidH != *oh) {
            saidW = *ow; saidH = *oh;
            DVR_INFO("clarity: DLSS Super Resolution - the game renders %ux%u, the eye texture and swapchain are the "
                     "%ux%u output (%.2fx per axis)", w, h, *ow, *oh, (double)*ow / w);
        }
        return;
    }
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
    struct DepthReads { ID3D11DeviceContext* ctx; ~DepthReads() { dvr::depthprobe::read_done(ctx); } } reads{ctx};
    calib_frame(dev, ctx, src, w, h, eyeSign, recId);   // motion vectors step 3 (off by default)
    // DLAA (dlss.h) replaces the capture as this draw's source when it runs. Its guides use the
    // same view the custom TAA reprojects with; the custom TAA then does not also accumulate.
    ID3D11ShaderResourceView* dlaa = nullptr;
    // VR-39: the pre-foreground copy (the hands mask) runs while DLSS does and wants it.
    dvr::depthprobe::set_prefg_wanted(2, dvr::dlss::mode() != dvr::dlss::ModeOff && dvr::dlss::fg_bias());
    if (dvr::dlss::mode() != dvr::dlss::ModeOff && dev && ctx && src && dst) {
        if (eyeSign == -1 || eyeSign == 1) {
            const int e = eyeSign < 0 ? 0 : 1;
            const View cur = view_for(recId, w, h, eyeSign);
            const Reset why = keep_history(g_dlssPrev[e], cur);
            if (cur.ok) {
                dvr::dlss::GuideParams gp;
                gp.w = w; gp.h = h;
                gp.tanH = cur.tanH; gp.tanV = cur.tanV;
                gp.historyValid = why == Reset::None;
                if (gp.historyValid) {
                    const Basis pb = basis_from_rotator(g_dlssPrev[e].pitch, g_dlssPrev[e].yaw, g_dlssPrev[e].roll);
                    const Basis cb = basis_from_rotator(cur.pitch, cur.yaw, cur.roll);
                    gp.prevFromCur = prev_from_cur(pb, cb);
                    gp.prevTanH = g_dlssPrev[e].tanH; gp.prevTanV = g_dlssPrev[e].tanV;
                    if (cur.posOk && g_dlssPrev[e].posOk) {
                        const float dp[3] = {cur.pos[0]-g_dlssPrev[e].pos[0],cur.pos[1]-g_dlssPrev[e].pos[1],cur.pos[2]-g_dlssPrev[e].pos[2]};
                        gp.translation[0] = dot3(pb.f,dp) * g_tAxis[0].load(); gp.translation[1] = dot3(pb.r,dp) * g_tAxis[1].load();
                        gp.translation[2] = dot3(pb.u,dp) * g_tAxis[2].load();
                    }
                }
                UINT dw = 0, dh = 0;
                if (auto* depth = dvr::depthprobe::depth_srv_for(dvr::capture::delivered_serial(), &dw, &dh)) {
                    gp.sceneDepth = depth; gp.depthW = dw; gp.depthH = dh; gp.depthScale = g_depthScale.load();
                    gp.bodyDepth = g_bodyDepth.load();
                    if (dvr::dlss::fg_bias()) gp.preFg = dvr::depthprobe::prefg_srv_for(dvr::capture::delivered_serial(), nullptr);
                }
                {   // the matrices the game drew this and the previous image of the eye with
                    dvr::pose::Record rc = {};
                    const bool copied = dvr::pose::copy(recId, &rc);
                    const bool have = copied && rc.renderVpOk && rc.renderPosOk;
                    // The projection jitter each image was drawn with travels in its record.
                    if (copied) { gp.jitter[0] = rc.jitter[0]; gp.jitter[1] = rc.jitter[1]; }
                    if (copied && g_dlssPrevRecOk[e]) {
                        gp.prevJitter[0] = g_dlssPrevRec[e].jitter[0]; gp.prevJitter[1] = g_dlssPrevRec[e].jitter[1];
                        gp.jitterKnown = true;
                    }
                    if (g_useVp.load() && gp.historyValid && have && g_dlssPrevRecOk[e]) {
                        gp.useVp = true;
                        memcpy(gp.vpCur, rc.renderVp, sizeof(gp.vpCur));
                        memcpy(gp.vpPrev, g_dlssPrevRec[e].renderVp, sizeof(gp.vpPrev));
                        // c5 is the NEGATED world position: current minus previous camera = prev c5 - cur c5.
                        for (int j = 0; j < 3; ++j) gp.camDelta[j] = g_dlssPrevRec[e].renderPos[j] - rc.renderPos[j];
                    }
                    g_dlssPrevRecOk[e] = have;
                    if (have) g_dlssPrevRec[e] = rc;
                }
                dlaa = dvr::dlss::run(dev, ctx, src, w, h, ow, oh, e, gp, !gp.historyValid);
            }
            g_dlssPrev[e] = cur;
        } else {
            g_dlssPrev[0] = View{}; g_dlssPrev[1] = View{};
        }
    } else if (dvr::dlss::mode() == dvr::dlss::ModeOff) {
        dvr::dlss::idle();
    }
    if (dlaa) {
        // The reconstructed image through the rest of the chain: resolve and sharpen as set,
        // never the custom temporal blend on top of DLAA's own accumulation.
        if (!g_initOk && !g_initTried) {
            g_initTried = true; g_seenEpoch = g_epoch.load();
            char why[512] = "";
            g_initOk = g_gpu.init(dev, why, sizeof(why));
            if (!g_initOk) DVR_ERROR("clarity: the passes are unavailable (%s) - DLAA output is blitted plainly", why);
        }
        if (g_initOk) {
            PassParams p;
            p.srcGamma = true;
            // The DLSS image is already the output size under Super Resolution.
            const bool sr = ow > w || oh > h;
            p.w = sr ? ow : w; p.h = sr ? oh : h; p.ow = ow; p.oh = oh;
            p.resolve = g_resolve.load() && (ow < p.w || oh < p.h);
            p.sharpen = g_sharpen.load();
            if (g_gpu.bytes()) g_gpu.trim(p.resolve, false);
            g_prev[0] = View{}; g_prev[1] = View{};
            char why[256] = "";
            if (g_gpu.run(dev, ctx, dlaa, dst, p, why, sizeof(why))) { ++g_win.draws; ++g_win.dlaa; status_tick(); return true; }
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000, "clarity: pass refused on the DLAA image (%s)", why);
        }
        // Passes unavailable: the capture takes the caller's plain blit, as with every lever off.
    }
    if (!any_on()) { if (g_initOk && g_gpu.bytes()) g_gpu.trim(false, false); return false; }
    if (!dev || !ctx || !src || !dst) return false;
    if (!g_initOk && g_initTried && g_seenEpoch != g_epoch.load()) { g_gpu.shutdown(); g_initTried = false; }
    if (!g_initOk) {
        if (g_initTried) return false;
        g_initTried = true; g_seenEpoch = g_epoch.load();
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
            const View cur = view_for(recId, ow, oh, eyeSign);
            const Reset why = keep_history(g_prev[e], cur);
            ++g_win.resets[(int)why];
            if (cur.ok) {
                p.temporal = true;
                p.eye = e;
                p.historyValid = why == Reset::None;
                if (g_motion.load() && cur.posOk) {
                    UINT dw = 0, dh = 0;
                    auto* depth = dvr::depthprobe::depth_srv_for(dvr::capture::delivered_serial(), &dw, &dh);
                    if (depth && dw == w && dh == h) { p.sceneDepth = depth; p.depthScale = g_depthScale.load(); }
                }
                if (p.historyValid) {
                    const Basis pb = basis_from_rotator(g_prev[e].pitch, g_prev[e].yaw, g_prev[e].roll);
                    const Basis cb = basis_from_rotator(cur.pitch, cur.yaw, cur.roll);
                    p.prevFromCur = prev_from_cur(pb, cb);
                    // Matching depth explains camera parallax. Without it, retain the
                    // existing motion-weighted rotation-only fallback.
                    float move = 0.0f;
                    if (g_prev[e].posOk && cur.posOk) {
                        const float dx = cur.pos[0] - g_prev[e].pos[0], dy = cur.pos[1] - g_prev[e].pos[1],
                                    dz = cur.pos[2] - g_prev[e].pos[2];
                        move = sqrtf(dx * dx + dy * dy + dz * dz);
                    }
                    if (p.sceneDepth && g_prev[e].posOk) {
                        const float dp[3] = {cur.pos[0]-g_prev[e].pos[0],cur.pos[1]-g_prev[e].pos[1],cur.pos[2]-g_prev[e].pos[2]};
                        p.translation[0] = dot3(pb.f,dp); p.translation[1] = dot3(pb.r,dp); p.translation[2] = dot3(pb.u,dp);
                    }
                    p.prevTanH = g_prev[e].tanH; p.prevTanV = g_prev[e].tanV;
                    const float m = p.sceneDepth ? 0.0f : motion_weight(move, basis_angle_deg(pb, cb));
                    p.blend = p.blend + (0.6f - p.blend) * m;
                    p.blend = blend_for_interval(p.blend, cur.timeMs-g_prev[e].timeMs);
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
        // No history was produced for this pose. Never pair an old texture with it.
        if (p.temporal) g_prev[p.eye] = View{};
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
                         "clarity: pass refused (%s) at %ux%u -> %ux%u - this present takes the plain copy", why, w, h, ow, oh);
        status_tick();
        return false;
    }
    ++g_win.draws;
    if (p.resolve) { ++g_win.resolved; g_win.srcW = w; g_win.srcH = h; g_win.outW = ow; g_win.outH = oh; }
    if (p.temporal && p.historyValid) {
        ++g_win.temporal;
        if (p.sceneDepth) ++g_win.vectors[p.eye];
        else if (g_motion.load()) ++g_win.motionFallback;
    }
    status_tick();
    return true;
}

void invalidate() { g_prev[0] = View{}; g_prev[1] = View{}; g_dlssPrev[0] = View{}; g_dlssPrev[1] = View{}; }

void shutdown() {
    dvr::dlss::shutdown();
    g_dlssPrev[0] = View{}; g_dlssPrev[1] = View{};
    g_gpu.shutdown();
    g_calib.shutdown(); g_calibTried = g_calibOk = false;
    for (auto& e : g_eh) { if (e.srv) e.srv->Release(); if (e.tex) e.tex->Release(); e = EyeHist{}; }
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
    if (n >= 2 && !_stricmp(sub, "motion") && onoff(&b)) { set_motion(b, "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "depthscale")) { set_depth_scale((float)atof(val), "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "blend")) { set_blend((float)atof(val), "the seam"); return true; }
    if (n >= 2 && !_stricmp(sub, "sharpen")) { set_sharpen((float)atof(val), "the seam"); return true; }
    if (n >= 1 && !_stricmp(sub, "off")) {
        set_resolve(false, "the seam"); set_temporal(false, "the seam"); set_sharpen(0.0f, "the seam");
        return true;
    }
    DVR_INFO("clarity: resolve %s, temporal %s (blend %.2f), sharpen %.2f | %s | words: clarity resolve on|off, "
             "temporal on|off, motion on|off, depthscale <25..7000>, blend <0.05..0.5>, sharpen <0..1>, off",
             on_off(g_resolve.load()), on_off(g_temporal.load()), g_blend.load(), g_sharpen.load(), summary());
    return true;
}

} // namespace dvr::clarity
