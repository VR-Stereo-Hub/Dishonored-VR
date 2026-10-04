// Host tests for AFW's held-eye rebuild (VR-39): the production shaders and draws in
// src/core/gfx/afw_warp.cpp against a ray-traced synthetic scene. Never touches the game.
// Build and run: tools\afw-warp-host.ps1
//
// The scene: a far wall, a near pillar and an optional thin bar fixed in the WORLD (they turn and slide
// against the tracking space when the body yaws or walks) and a "hand" quad fixed in TRACKING space (it
// moves only when the controller does). Each image also carries the game-style camera-relative
// view-projection (UE axes), c5 and camera rotator it was "drawn" with, so the held eye's world can move
// by matrices and the matrix checks run as in the game. Every image is ray-traced from its eye pose at
// its instant; every pixel's colour names the surface point it shows (red/green = the point's
// coordinates on that surface, blue = 1 on the hand), with the linear view depth in alpha as the game's
// scene target carries it. The held eye's image is traced at the OLD instant, the fresh eye's at the NEW
// one, and the truth is the held eye traced at the NEW instant. Each case compares pixel by pixel:
//   ghost    the rebuild shows the hand where the truth does not (the reported fault)
//   missing  the truth shows the hand where the rebuild does not
//   wrong    a world pixel showing another world surface, where some source image did show the right one
//   unseen   a world pixel no source image shows (cannot be rebuilt; scored only for a ghost)
//   error    on agreeing pixels, how far the shown point is from the true one, in target pixels
// The negative controls run the same motion with a source or a lever off and must show the fault, so
// the instrument can see what it claims to fix.
#include "core/gfx/afw_warp.h"

#include <windows.h>
#include <d3d11.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include <algorithm>

// ---- stubs for the module's dependencies --------------------------------------------------------
#include "core/util/log.h"
namespace dvr::log {
uint8_t g_levels[(int)Cat::COUNT] = {};
void write(Cat, Level, const char*, ...) {}
}
static ID3D11ShaderResourceView* g_depthBySerial[16] = {};
static UINT g_depthW = 0, g_depthH = 0;
namespace dvr::clarity { float depth_scale() { return 250.0f; } }
namespace dvr::depthprobe {
ID3D11ShaderResourceView* depth_srv_for(uint32_t serial, UINT* w, UINT* h) {
    if (serial >= 16 || !g_depthBySerial[serial]) return nullptr;
    if (w) *w = g_depthW; if (h) *h = g_depthH; return g_depthBySerial[serial];
}
void read_done(ID3D11DeviceContext*) {}
void set_prefg_wanted(unsigned, bool) {}
bool g_prefgReady = false;
bool prefg_ready() { return g_prefgReady; }   // a capture with signed (masked) depths replays in mask mode
ID3D11ShaderResourceView* prefg_srv_for(uint32_t, bool* saw) { if (saw) *saw = false; return nullptr; }
// Run 17: the drawn foreground mask, per serial (R = 1 where the hand is), when a case asks for it.
ID3D11ShaderResourceView* g_maskBySerial[16] = {};
ID3D11ShaderResourceView* fgmask_srv_for(uint32_t serial, uint32_t* draws, uint32_t* w, uint32_t* h) {
    if (draws) *draws = 0;
    if (serial >= 16 || !g_maskBySerial[serial]) return nullptr;
    if (draws) *draws = 1; if (w) *w = g_depthW; if (h) *h = g_depthH; return g_maskBySerial[serial];
}
}

// ---- the scene ----------------------------------------------------------------------------------
static const int N = 512;
static const double kTan = 1.0, kMPerUnit = 2.5;        // 250 uu per unit over 100 uu per metre
static const double kWallZ = -8.0;                       // world plane, metres
static const double kHandZ = -0.45, kHandW = 0.16, kHandH = 0.12, kHandY = -0.12;
static const double kIpd = 0.063;
static const double kPillarZ = -1.5, kPillarX0 = 0.15, kPillarX1 = 0.45;   // world, metres
static const double kScale = 100.0;                                          // uu per metre

struct V3 { double x, y, z; };
static V3 add(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static V3 sub(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static V3 mul(V3 a, double k) { return {a.x * k, a.y * k, a.z * k}; }
// The shader's yaw rows {{c,0,s},{0,1,0},{-s,0,c}}; an XR head yaw h about +Y has the same matrix.
static V3 ry(V3 p, double rad) { const double c = cos(rad), s = sin(rad); return {c * p.x + s * p.z, p.y, -s * p.x + c * p.z}; }
// The scene's world (x right, y up, -z forward) into UE axes (X forward, Y right, Z up).
static V3 ue(V3 v) { return {-v.z, v.x, v.y}; }

// tracking = Ry(body)(world - bodyPos): the body turns by bodyYawDeg and stands at bodyPos (world).
struct State {
    double bodyYawDeg, handX, headYawDeg; V3 headPos;
    double handZ = kHandZ; V3 bodyPos = {0, 0, 0}; double handW = kHandW;
    double barZ = 0, barX0 = 0, barX1 = 0;   // a thin world bar (barZ 0 = none)
    V3 platformPos = {0,0,0}; // independent motion of the pillar: vehicle surface
    double fgTan = 0;   // > 0: the hand is the FOREGROUND, drawn on top with its own projection (the game: its camera FOV)
};
struct Eye { V3 pos; double yawRad; };
static Eye eyeOf(const State& s, int eye) {   // eye 0 left, 1 right
    const double h = s.headYawDeg / 57.29577951;
    return {add(s.headPos, ry({eye ? kIpd / 2 : -kIpd / 2, 0, 0}, h)), h};
}
static dvr::afw::Pose poseOf(const Eye& e) {
    dvr::afw::Pose p = {{0, (float)sin(e.yawRad / 2), 0, (float)cos(e.yawRad / 2)}, {(float)e.pos.x, (float)e.pos.y, (float)e.pos.z}};
    return p;
}
// One pixel: rgb names the point, a is its view depth in units.
static void trace(const State& s, const Eye& e, double u, double v, float* out) {
    const V3 dv = {(u * 2 - 1) * kTan, (1 - v * 2) * kTan, -1};
    const V3 d = ry(dv, e.yawRad);
    double best = 1e30; float c[4] = {0, 0, 0, 0};
    if (s.fgTan > 0) {   // the foreground pass: its own rays, drawn over the world whatever the world's depth
        const V3 df = ry({(u * 2 - 1) * s.fgTan, (1 - v * 2) * s.fgTan, -1}, e.yawRad);
        const double t = (s.handZ - e.pos.z) / df.z;
        const V3 p = add(e.pos, mul(df, t));
        const double lu = (p.x - (s.handX - s.handW / 2)) / s.handW, lv = (p.y - (kHandY - kHandH / 2)) / kHandH;
        if (df.z < 0 && t > 0 && lu >= 0 && lu <= 1 && lv >= 0 && lv <= 1) {
            const V3 local = ry(mul(df, t), -e.yawRad);
            const float o[4] = {(float)lu, (float)lv, 1, (float)(-local.z / kMPerUnit)};
            memcpy(out, o, sizeof(o));
            return;
        }
    }
    if (d.z < 0 && s.fgTan <= 0) {   // the hand, fixed in tracking space
        const double t = (s.handZ - e.pos.z) / d.z;
        const V3 p = add(e.pos, mul(d, t));
        const double lu = (p.x - (s.handX - s.handW / 2)) / s.handW, lv = (p.y - (kHandY - kHandH / 2)) / kHandH;
        if (t > 0 && lu >= 0 && lu <= 1 && lv >= 0 && lv <= 1) { best = t; c[0] = (float)lu; c[1] = (float)lv; c[2] = 1; }
    }
    {   // the wall, the pillar and the bar, fixed in the world
        const double b = s.bodyYawDeg / 57.29577951;
        const V3 ow = add(ry(e.pos, -b), s.bodyPos), dw = ry(d, -b);
        if (dw.z < 0) {
            const double t = (kWallZ - ow.z) / dw.z;
            const V3 p = add(ow, mul(dw, t));
            if (t > 0 && t < best) { best = t; c[0] = (float)((p.x + 20) / 40); c[1] = (float)((p.y + 20) / 40); c[2] = 0; }
            const double tp = (kPillarZ + s.platformPos.z - ow.z) / dw.z;
            const V3 q = sub(add(ow, mul(dw, tp)),s.platformPos);
            if (tp > 0 && tp < best && q.x >= kPillarX0 && q.x <= kPillarX1 && q.y >= -1.5 && q.y <= 1.5) {
                best = tp; c[0] = (float)((q.x - kPillarX0) / (kPillarX1 - kPillarX0)); c[1] = (float)(2 + (q.y + 1.5) / 3); c[2] = 0;
            }
            if (s.barZ < 0) {
                const double tb = (s.barZ - ow.z) / dw.z;
                const V3 r = add(ow, mul(dw, tb));
                if (tb > 0 && tb < best && r.x >= s.barX0 && r.x <= s.barX1 && r.y >= -1.5 && r.y <= 1.5) {
                    best = tb; c[0] = (float)((r.x - s.barX0) / (s.barX1 - s.barX0)); c[1] = (float)(4 + (r.y + 1.5) / 3); c[2] = 0;
                }
            }
        }
    }
    const V3 local = ry(mul(d, best), -e.yawRad);   // back into the eye's view: depth = -z
    c[3] = (float)(-local.z / kMPerUnit);
    memcpy(out, c, sizeof(c));
}
static int surfOf(const float* q) { return q[2] > 0.5f ? 2 : q[1] >= 3.5f ? 3 : q[1] >= 1.5f ? 1 : 0; }   // 0 wall 1 pillar 2 hand 3 bar

// The game-style record: row-vector, camera-relative, uu, UE axes; clip x = right.P / tan, y = up.P / tan,
// w = forward.P; c5 = -camera; the rotator from the forward axis (no roll in these scenes).
struct Rec { float vp[16]; float c5[3]; float rot[3]; };
static Rec recordOf(const State& s, const Eye& e, bool mirrored, bool flipC5) {
    const double b = s.bodyYawDeg / 57.29577951;
    const V3 right = ue(ry(ry({1, 0, 0}, e.yawRad), -b)), up = ue(ry(ry({0, 1, 0}, e.yawRad), -b)), fwd = ue(ry(ry({0, 0, -1}, e.yawRad), -b));
    const V3 C = mul(ue(add(ry(e.pos, -b), s.bodyPos)), kScale);
    Rec m = {};
    const double sx = mirrored ? -1.0 : 1.0;   // a mirrored axis: the basis check must refuse it
    const double r3[3] = {right.x, right.y, right.z}, u3[3] = {up.x, up.y, up.z}, f3[3] = {fwd.x, fwd.y, fwd.z};
    for (int j = 0; j < 3; ++j) { m.vp[j * 4 + 0] = (float)(sx * r3[j] / kTan); m.vp[j * 4 + 1] = (float)(u3[j] / kTan); m.vp[j * 4 + 3] = (float)f3[j]; }
    const double cs = flipC5 ? 1.0 : -1.0;     // a flipped c5: the camera check must refuse it
    m.c5[0] = (float)(cs * C.x); m.c5[1] = (float)(cs * C.y); m.c5[2] = (float)(cs * C.z);
    m.rot[0] = (float)(asin(fwd.z) * 57.29577951); m.rot[1] = (float)(atan2(fwd.y, fwd.x) * 57.29577951); m.rot[2] = 0;
    return m;
}
// The foreground mask as the production snapshot writes it: a texel the foreground pass drew carries NEGATIVE
// depth. On for the mask cases (the hand is the foreground there).
static bool g_signForeground = false;
static std::vector<float> image(const State& s, const Eye& e, int w, int h) {
    std::vector<float> px((size_t)w * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float* q = &px[((size_t)y * w + x) * 4];
            trace(s, e, (x + 0.5) / w, (y + 0.5) / h, q);
            if (g_signForeground && q[2] > 0.5f) q[3] = -q[3];
        }
    return px;
}

// ---- the GPU side -------------------------------------------------------------------------------
struct Gpu { ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr; ID3D11Texture2D* dst = nullptr; ID3D11Texture2D* stage = nullptr; };
static ID3D11Texture2D* tex(ID3D11Device* dev, int w, int h, UINT bind, D3D11_USAGE use, UINT cpu, const void* init) {
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h; td.MipLevels = td.ArraySize = 1; td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    td.SampleDesc.Count = 1; td.Usage = use; td.BindFlags = bind; td.CPUAccessFlags = cpu;
    D3D11_SUBRESOURCE_DATA sd = {init, (UINT)w * 16, 0};
    ID3D11Texture2D* t = nullptr;
    dev->CreateTexture2D(&td, init ? &sd : nullptr, &t);
    return t;
}

struct Result { bool ok; int verdict; int handTruth, ghost, missing, agree, wrong, unseen; double errP50, errP95, errMax; };
struct Opt { bool stereo = true, heldDepth = true, freshDepth = true, matrices = true, mirrored = false, flipC5 = false, noHeld = false,
             mask = false, drawnMask = false, grips = false, freshWorld = false;   // grips: record each image's hand grip (run 25)
             double fgFovDeg = 0; };   // > 0: tell the rebuild the foreground FOV (the scene's State.fgTan draws it)

// Captures the two images as the runtime does (the held eye last present, the fresh eye now) and returns
// the fresh serial; the caller warps.
struct Scene { ID3D11Texture2D *ht = nullptr, *ft = nullptr; ID3D11ShaderResourceView *hs = nullptr, *fs = nullptr; uint32_t sf = 0; };
static Scene capture(Gpu& g, const State& s0, const State& s1, const Opt& o, int w, int h,
                     const std::vector<float>& hImg, const std::vector<float>& fImg,
                     const std::vector<float>* hClean = nullptr, const std::vector<float>* fClean = nullptr) {
    Scene sc;
    const Eye held0 = eyeOf(s0, 0), right0 = eyeOf(s0, 1), held1 = eyeOf(s1, 0), fresh1 = eyeOf(s1, 1);
    sc.ht = tex(g.dev, w, h, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, hImg.data());
    sc.ft = tex(g.dev, w, h, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, fImg.data());
    g.dev->CreateShaderResourceView(sc.ht, nullptr, &sc.hs);
    g.dev->CreateShaderResourceView(sc.ft, nullptr, &sc.fs);
    for (auto*& p : g_depthBySerial) p = nullptr;
    for (auto*& p : dvr::depthprobe::g_maskBySerial) if (p) { p->Release(); p = nullptr; }
    static uint32_t serial = 0;
    const uint32_t sh = (serial = (serial + 2) % 12) + 1;
    sc.sf = sh + 1;
    if (o.heldDepth) g_depthBySerial[sh] = sc.hs;
    if (o.freshDepth) g_depthBySerial[sc.sf] = sc.fs;
    if (o.drawnMask) {   // run 17: what the foreground draws covered, as the production redraw writes it (R = 1)
        auto maskOf = [&](const std::vector<float>& img) {
            std::vector<float> m((size_t)w * h * 4, 0.0f);
            for (size_t i = 0; i < (size_t)w * h; ++i) if (img[i * 4 + 2] > 0.5f) m[i * 4] = 1.0f;
            ID3D11Texture2D* t = tex(g.dev, w, h, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, m.data());
            ID3D11ShaderResourceView* v = nullptr;
            if (t) { g.dev->CreateShaderResourceView(t, nullptr, &v); t->Release(); }
            return v;
        };
        dvr::depthprobe::g_maskBySerial[sh] = maskOf(hImg);
        dvr::depthprobe::g_maskBySerial[sc.sf] = maskOf(fImg);
    }
    g_depthW = w; g_depthH = h;
    dvr::afw::set_enabled(true, "test");   // also drops the previous case's records
    dvr::afw::set_stereo(o.stereo, "test");
    dvr::afw::set_fresh_world(o.freshWorld,"test");
    dvr::afw::set_body_depth(0.40f, "test");
    dvr::afw::set_world_scale((float)kScale);
    dvr::afw::set_matrices(o.matrices, "test");
    dvr::afw::set_fg_fov((float)o.fgFovDeg); dvr::afw::set_fg(o.fgFovDeg > 0, "test"); dvr::afw::set_fg_depth(0.30f, "test");
    dvr::afw::set_near_miss(6.0f, "test");
    const Rec mh = recordOf(s0, held0, o.mirrored, o.flipC5), mf = recordOf(s1, fresh1, o.mirrored, o.flipC5);
    const dvr::afw::Pose tg0[2] = {poseOf(held0), poseOf(right0)}, tg1[2] = {poseOf(held1), poseOf(fresh1)};
    // Run 15: each image's clean copy (before the mod's own layers), handed over as the stereo method does, just before
    // the runtime captures that grab.
    auto clean = [&](const std::vector<float>* img, uint32_t serial) {
        if (!img) return;
        ID3D11Texture2D* t = tex(g.dev, w, h, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, img->data());
        dvr::afw::note_clean(g.dev, g.ctx, t, serial);
        if (t) t->Release();
    };
    if (!o.noHeld) {
        clean(hClean, sh);
        dvr::afw::note_capture(g.dev, g.ctx, 0, sc.ht, sh, poseOf(held0), true, (float)s0.bodyYawDeg, tg0, mh.vp, mh.c5, mh.rot, nullptr);
    }
    clean(fClean, sc.sf);
    dvr::afw::note_capture(g.dev, g.ctx, 1, sc.ft, sc.sf, poseOf(fresh1), true, (float)s1.bodyYawDeg, tg1, mf.vp, mf.c5, mf.rot, nullptr);
    {   // run 25: the grip each image was drawn with (the hand's centre, tracking space); none unless the case asks
        dvr::afw::HandPose g0[2], g1[2];
        if (o.grips) {
            g0[0].ok = g1[0].ok = true;
            g0[0].p[0] = (float)s0.handX; g0[0].p[1] = (float)kHandY; g0[0].p[2] = (float)s0.handZ;
            g1[0].p[0] = (float)s1.handX; g1[0].p[1] = (float)kHandY; g1[0].p[2] = (float)s1.handZ;
        }
        dvr::afw::note_hands(0, g0); dvr::afw::note_hands(1, g1);
    }
    return sc;
}
static void release(Scene& sc) {
    if (sc.hs) sc.hs->Release(); if (sc.fs) sc.fs->Release(); if (sc.ht) sc.ht->Release(); if (sc.ft) sc.ft->Release();
    sc = Scene{};
}

static Result run(Gpu& g, const State& s0, const State& s1, Opt o = Opt()) {
    Result r = {};
    g_signForeground = o.mask; dvr::depthprobe::g_prefgReady = o.mask;
    dvr::afw::set_fg_mask(true, "test");
    const Eye held0 = eyeOf(s0, 0), held1 = eyeOf(s1, 0), fresh1 = eyeOf(s1, 1);
    auto hImg = image(s0, held0, N, N), fImg = image(s1, fresh1, N, N), truth = image(s1, held1, N, N);
    Scene sc = capture(g, s0, s1, o, N, N, hImg, fImg);
    dvr::afw::Pose out{};
    const char* why = nullptr;
    r.ok = dvr::afw::warp_held(g.dev, g.ctx, 0, 1, sc.sf, g.dst, N, N, (float)kTan, (float)kTan, &out, &why);
    r.verdict = dvr::afw::matrix_verdict();
    std::vector<float> px(N * N * 4);
    if (r.ok) {
        g.ctx->CopyResource(g.stage, g.dst);
        D3D11_MAPPED_SUBRESOURCE m;
        if (SUCCEEDED(g.ctx->Map(g.stage, 0, D3D11_MAP_READ, 0, &m))) {
            for (int y = 0; y < N; ++y) memcpy(&px[y * N * 4], (const uint8_t*)m.pData + y * m.RowPitch, N * 16);
            g.ctx->Unmap(g.stage, 0);
        }
        r.ok = fabsf(out.p[0] - (float)held1.pos.x) < 1e-5f;   // submitted at the fresh generation's held-eye pose
    } else {
        printf("  warp refused: %s\n", why ? why : "?");
    }
    release(sc);
    if (!r.ok) return r;
    // Compare, skipping the 2-pixel band around every true outline (sampling there is a blend).
    std::vector<double> errs;
    auto surf = [&](const std::vector<float>& p, int x, int y) { return surfOf(&p[(y * N + x) * 4]); };
    for (int y = 2; y < N - 2; ++y)
        for (int x = 2; x < N - 2; ++x) {
            const int ts = surf(truth, x, y);
            bool edge = false;
            for (int dy = -2; dy <= 2 && !edge; ++dy) for (int dx = -2; dx <= 2; ++dx) if (surf(truth, x + dx, y + dy) != ts) { edge = true; break; }
            if (edge) continue;
            const float* ov = &px[(y * N + x) * 4];
            const float* t = &truth[(y * N + x) * 4];
            const bool th = ts == 2, oh = ov[2] > 0.5f;
            // Surface units to target pixels: each surface's span over its distance.
            const double k = N / (2 * kTan);
            const double sx = ts == 2 ? s1.handW / (-s1.handZ) * k : ts == 1 ? (kPillarX1 - kPillarX0) / -kPillarZ * k
                            : ts == 3 ? (s1.barX1 - s1.barX0) / -s1.barZ * k : 40.0 / 8.0 * k;
            const double sy = ts == 2 ? kHandH / (-s1.handZ) * k : ts == 1 ? 3.0 / -kPillarZ * k : ts == 3 ? 3.0 / -s1.barZ * k : sx;
            if (th) ++r.handTruth;
            if (oh && !th) { ++r.ghost; continue; }
            if (!oh && th) { ++r.missing; continue; }
            if (!th) {   // a world point no source image shows cannot be rebuilt: counted apart, scored only for a ghost
                const double d = t[3] * kMPerUnit, u = (x + 0.5) / N, v = (y + 0.5) / N;
                const V3 P1 = add(ry({(u * 2 - 1) * kTan * d, (1 - v * 2) * kTan * d, -d}, held1.yawRad), held1.pos);
                const V3 Pw = add(ry(P1, -s1.bodyYawDeg / 57.29577951), s1.bodyPos);
                auto seen = [&](const std::vector<float>& img, const State& st, const Eye& e) {
                    const V3 P = ry(sub(Pw, st.bodyPos), st.bodyYawDeg / 57.29577951);
                    const V3 L = ry(sub(P, e.pos), -e.yawRad);
                    if (L.z >= -1e-6) return false;
                    const int X = (int)(((L.x / -L.z / kTan) * 0.5 + 0.5) * N), Y = (int)((0.5 - (L.y / -L.z / kTan) * 0.5) * N);
                    if (X < 0 || Y < 0 || X >= N || Y >= N) return false;
                    const float* q = &img[(Y * N + X) * 4];
                    if (surfOf(q) != ts) return false;
                    return hypot((q[0] - t[0]) * sx, (q[1] - t[1]) * sy) < 3.0;
                };
                if ((o.noHeld || !seen(hImg, s0, held0)) && !seen(fImg, s1, fresh1)) { ++r.unseen; continue; }
            }
            if (ov[2] > 0.02f && ov[2] < 0.98f) continue;                                          // a blend across the hand outline
            if (!th && ((ov[1] > 0.6f && ov[1] < 1.9f) || (ov[1] > 3.1f && ov[1] < 3.9f))) continue;  // a blend across a world outline
            if (!th && surfOf(ov) != ts) { ++r.wrong; continue; }
            ++r.agree;
            errs.push_back(hypot((ov[0] - t[0]) * sx, (ov[1] - t[1]) * sy));
        }
    std::sort(errs.begin(), errs.end());
    if (!errs.empty()) {
        r.errP50 = errs[errs.size() / 2]; r.errP95 = errs[errs.size() * 95 / 100]; r.errMax = errs.back();
    }
    return r;
}

static int g_fail = 0, g_pass = 0;
static void check(const char* name, bool ok, const char* detail) {
    printf("%-58s %s  %s\n", name, ok ? "PASS" : "FAIL", detail);
    ok ? ++g_pass : ++g_fail;
}
static void report(const char* name, const Result& r, bool ok) {
    char d[256];
    snprintf(d, sizeof(d), "hand %5d ghost %4d (%.2f%%) missing %4d (%.2f%%) wrong world %4d (unseen %4d) | err p50 %.2f p95 %.2f px | mtx %d",
             r.handTruth, r.ghost, r.handTruth ? 100.0 * r.ghost / r.handTruth : 0.0, r.missing,
             r.handTruth ? 100.0 * r.missing / r.handTruth : 0.0, r.wrong, r.unseen, r.errP50, r.errP95, r.verdict);
    check(name, ok, d);
}
// The bounds: ghost and missing under 1% of the hand (with a real hand to measure), the wrong world
// surface under 0.1% of the image, the median error under a pixel and 95% under two.
static bool clean(const Result& r, int minHand = 500) {
    return r.ok && r.handTruth >= minHand && r.ghost * 100 <= r.handTruth && r.missing * 100 <= r.handTruth &&
           r.wrong < N * N / 1000 && r.agree > N * N / 2 && r.errP50 < 1.0 && r.errP95 < 2.0;
}

int main() {
    Gpu g;
    const D3D_FEATURE_LEVEL fl[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fl, 2, D3D11_SDK_VERSION, &g.dev, nullptr, &g.ctx)) &&
        FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, fl, 2, D3D11_SDK_VERSION, &g.dev, nullptr, &g.ctx))) {
        printf("no D3D11 device\n"); return 2;
    }
    g.dst = tex(g.dev, N, N, D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, nullptr);
    g.stage = tex(g.dev, N, N, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ, nullptr);
    if (!g.dst || !g.stage) { printf("resource creation failed\n"); return 2; }

    const State still = {0, 0.0, 0, {0, 0, 0}};
    State turn = still;   turn.bodyYawDeg = 5;                    // a stick turn between the images
    State moved = still;  moved.handX = 0.06;                     // the controller moved 6 cm
    State head = still;   head.headYawDeg = 3; head.headPos = {0.015, 0, 0};   // a head turn and shift
    State all = still;    all.bodyYawDeg = 5; all.handX = 0.03; all.headYawDeg = 2;
    State fast = still;   fast.bodyYawDeg = 15;                   // a fast turn in one tick
    State close0 = still; close0.handZ = -0.20;                   // the weapon held close
    State close1 = close0; close1.bodyYawDeg = 5; close1.handX = 0.02;
    State walk = still;   walk.bodyPos = {0.04, 0, -0.03};        // one tick of walking: 4 cm right, 3 cm on
    State walkTurn = walk; walkTurn.bodyYawDeg = 4; walkTurn.headYawDeg = 2;
    State bar = still;    bar.barZ = -0.20; bar.barX0 = -0.01; bar.barX1 = 0.01;   // a thin world bar in front of the hand
    State thin = still;   thin.handZ = -0.75; thin.handW = 0.02;  // a 2 cm object at 0.75 m
    State far0 = still;   far0.handZ = -1.10;                     // a weapon beyond the body threshold
    State far1 = far0;    far1.handX = 0.06;
    Opt noStereo; noStereo.stereo = false;
    Opt noMtx; noMtx.matrices = false;
    Opt mirrored; mirrored.mirrored = true;
    Opt flip; flip.flipC5 = true;
    Opt noHeld; noHeld.noHeld = true;
    // The run-6 fault: the foreground drawn at a wider FOV than the world. The game's 108 against 103
    // degrees is a 1.098 tangent ratio; 1.25 here, so the fault clears this test's 2-pixel edge band at 512.
    const double kFgTan = 1.25;
    State fgStill = still; fgStill.fgTan = kFgTan;
    State fgTurn = turn;   fgTurn.fgTan = kFgTan;
    State fgMoved = moved; fgMoved.fgTan = kFgTan;
    Opt fgOn; fgOn.fgFovDeg = 2 * atan(kFgTan) * 57.29577951;

    { Result r = run(g, still, still);   report("still: the other eye's stereo only", r, clean(r) && r.verdict == 1); }
    { Result r = run(g, still, turn);    report("stick turn 5 deg: no hand ghost, world turned", r, clean(r) && r.verdict == 1); }
    { Result r = run(g, still, moved);   report("hand moved 6 cm: the hand where it is NOW", r, clean(r)); }
    { Result r = run(g, still, head);    report("head turn 3 deg + 1.5 cm: no drift", r, clean(r) && r.verdict == 1); }
    { Result r = run(g, still, all);     report("turn + hand move + head turn together", r, clean(r) && r.verdict == 1); }
    { Result r = run(g, still, fast);    report("fast turn 15 deg in one tick", r, clean(r)); }
    { Result r = run(g, close0, close1); report("weapon close (0.2 m), turn + move", r, clean(r)); }
    { // A boat carries its surface and the player together. Correct camera matrices
      // still smear a tangentially moving surface because its depth stays unchanged.
      State boat=still;boat.bodyPos={.08,0,0};boat.platformPos=boat.bodyPos;
      Opt fresh;fresh.freshWorld=true;
      Result old=run(g,still,boat);report("moving platform control: old world texture trails",old,old.ok && old.errP95>2);
      Result now=run(g,still,boat,fresh);report("moving platform: current-time world",now,clean(now));
      boat.bodyPos={-.08,0,0};boat.platformPos=boat.bodyPos;
      now=run(g,still,boat,fresh);report("moving platform: opposite travel",now,clean(now));
      boat=still;boat.platformPos={.08,0,0};
      now=run(g,still,boat,fresh);report("moving object, stationary player",now,clean(now));
      now=run(g,still,still,fresh);report("current-time world: static scene",now,clean(now));
    }
    { Opt fresh;fresh.freshWorld=true;
      for(const State* next:{&turn,&head,&all,&walkTurn}) {
        Result r=run(g,still,*next,fresh);report("current-time world: head/body/controller motion",r,clean(r));
      }
    }
    { Result r = run(g, still, walk);    report("walking one tick: the pillar's parallax carried", r, clean(r) && r.verdict == 1); }
    { Result r = run(g, still, walkTurn); report("walking + turning + head turn", r, clean(r) && r.verdict == 1); }
    // The review's counterexamples.
    { Result r = run(g, bar, bar);       report("a thin bar in front of the hand hides it", r, clean(r, 300)); }
    { Result r = run(g, thin, thin);     report("a 2 cm object at 0.75 m is kept", r, clean(r, 100)); }
    { Result r = run(g, far0, far1);     report("a weapon at 1.1 m moved 6 cm: no ghost", r, clean(r, 100)); }
    { Result r = run(g, still, moved, noHeld);
      report("no held image: the fresh eye alone", r, r.ok && r.ghost * 100 <= r.handTruth && r.missing * 100 <= r.handTruth); }
    { Result r = run(g, still, walk, mirrored);
      report("a mirrored matrix, walking: refused by the basis check", r, r.ok && r.verdict == 3); }
    {   // A flipped c5 is a permanent fault: the camera check votes over still presents and latches. One walking
        // present is carried; after enough still ones the matrices are refused for good. Then a toggle resets it.
        Result r = run(g, still, walk, flip);
        const bool carried = r.ok && r.verdict == 1;
        for (int i = 0; i < 34; ++i) r = run(g, still, still, flip);
        const bool latched = r.ok && r.verdict == 6;
        r = run(g, still, walk, flip);
        const bool stays = r.ok && r.verdict == 6;
        dvr::afw::set_matrices(false, "test"); dvr::afw::set_matrices(true, "test");
        Result ok = run(g, still, walk);
        char d[160]; snprintf(d, sizeof(d), "first present carried %d, latched after 35 still %d, stays latched %d, a toggle clears it %d",
                             carried, latched, stays, ok.verdict == 1);
        check("a flipped c5: the still-present vote latches the refusal", carried && latched && stays && ok.verdict == 1, d); }
    {   // Running: a large body step with a correct c5 is carried, not refused (the run-7 blur).
        State run1 = still; run1.bodyPos = {0.20, 0, -0.15};   // 25 cm in one tick
        Result r = run(g, still, run1);
        report("running 25 cm in a tick: the matrices carry it", r, r.ok && r.verdict == 1 && r.errP95 < 2.0); }
    {   // VR-39: the depth layer. Each eye's XR depth must say how far what it SHOWS is: the rebuilt eye's from the
        // compose (scored against the truth traced at the new instant), the fresh eye's from its own image. Standard
        // depth for [0.05, 1000] m, written through a typeless D32 image as a runtime's swapchain hands it over.
        // The control: the held eye's OWN depth (what a layer without the compose output would carry) against the
        // same truth must fail - the hand moved and the body walked and turned.
        State s1 = walkTurn; s1.handX = 0.03;
        const Eye h0 = eyeOf(still, 0), h1 = eyeOf(s1, 0), f1 = eyeOf(s1, 1);
        auto hImg = image(still, h0, N, N), fImg = image(s1, f1, N, N), truth = image(s1, h1, N, N);
        Scene sc = capture(g, still, s1, Opt(), N, N, hImg, fImg);
        dvr::afw::set_xr_depth_wanted(true);
        dvr::afw::Pose out{}; const char* why = nullptr;
        const bool warped = dvr::afw::warp_held(g.dev, g.ctx, 0, 1, sc.sf, g.dst, N, N, (float)kTan, (float)kTan, &out, &why);
        const float nearM = 0.05f, farM = 1000.0f;
        auto depthOf = [&](int eye, bool rebuilt, std::vector<float>& m) -> bool {
            D3D11_TEXTURE2D_DESC td = {};
            td.Width = N; td.Height = N; td.MipLevels = td.ArraySize = 1; td.Format = DXGI_FORMAT_R32_TYPELESS;
            td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
            ID3D11Texture2D *dt = nullptr, *st = nullptr;
            g.dev->CreateTexture2D(&td, nullptr, &dt);
            td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            g.dev->CreateTexture2D(&td, nullptr, &st);
            const char* w = nullptr;
            bool ok = dt && st && dvr::afw::write_xr_depth(g.dev, g.ctx, eye, rebuilt, dt, DXGI_FORMAT_D32_FLOAT, N, N, nearM, farM, &w);
            if (ok) {
                g.ctx->CopyResource(st, dt);
                D3D11_MAPPED_SUBRESOURCE mm;
                ok = SUCCEEDED(g.ctx->Map(st, 0, D3D11_MAP_READ, 0, &mm));
                if (ok) {
                    m.resize(N * N);
                    for (int y = 0; y < N; ++y)
                        for (int x = 0; x < N; ++x) {
                            const float d = ((const float*)((const uint8_t*)mm.pData + y * mm.RowPitch))[x];
                            m[y * N + x] = nearM / (1.0f - d * (farM - nearM) / farM);   // standard depth back to metres
                        }
                    g.ctx->Unmap(st, 0);
                }
            } else printf("  depth write refused: %s\n", w ? w : "?");
            if (dt) dt->Release(); if (st) st->Release();
            return ok;
        };
        // The fraction of hand and of world pixels (off the 2-pixel outline bands) within 3% of the reference.
        auto score = [&](const std::vector<float>& m, const std::vector<float>& ref, double* hand, double* world) {
            int hn = 0, hg = 0, wn = 0, wg = 0;
            for (int y = 2; y < N - 2; ++y)
                for (int x = 2; x < N - 2; ++x) {
                    const int ts = surfOf(&ref[(y * N + x) * 4]);
                    bool edge = false;
                    for (int dy = -2; dy <= 2 && !edge; ++dy)
                        for (int dx = -2; dx <= 2; ++dx) if (surfOf(&ref[((y + dy) * N + x + dx) * 4]) != ts) { edge = true; break; }
                    if (edge) continue;
                    const double want = fabs(ref[(y * N + x) * 4 + 3]) * kMPerUnit;
                    const bool good = fabs(m[y * N + x] - want) < 0.03 * want;
                    if (ts == 2) { ++hn; hg += good; } else { ++wn; wg += good; }
                }
            *hand = hn ? (double)hg / hn : 0; *world = wn ? (double)wg / wn : 0;
        };
        std::vector<float> mR, mF, mO;
        const bool okR = warped && depthOf(0, true, mR), okF = depthOf(1, false, mF), okO = depthOf(0, false, mO);
        double rh = 0, rw = 0, fh = 0, fw = 0, oh = 0, ow = 0;
        if (okR) score(mR, truth, &rh, &rw);
        if (okF) score(mF, fImg, &fh, &fw);
        if (okO) score(mO, truth, &oh, &ow);
        char d[220];
        snprintf(d, sizeof(d), "within 3%%: rebuilt hand %.3f world %.3f | fresh hand %.3f world %.3f | control (held's own) hand %.3f world %.3f",
                 rh, rw, fh, fw, oh, ow);
        check("depth layer: each eye's depth is what it shows", okR && okF && okO && rh > 0.97 && rw > 0.97 && fh > 0.99 && fw > 0.99 &&
              (oh < 0.9 || ow < 0.9), d);
        release(sc);
        // A new present's capture with no rebuild yet: the rebuilt eye's depth is refused, never last present's.
        Scene sc2 = capture(g, still, s1, Opt(), N, N, hImg, fImg);
        std::vector<float> mS;
        const bool stale = depthOf(0, true, mS);
        check("depth layer: no rebuilt depth before this present's rebuild", !stale, stale ? "a stale depth was written" : "refused");
        release(sc2);
        dvr::afw::set_xr_depth_wanted(false);
    }
    {   // MSW: the mod's own spacewarp. Running at 5 m/s: the left image at t = 0, the right at 10 ms, and display slots
        // at 20 ms (left, from a 20 ms old image) and 20 ms (right, 10 ms old) that the game did not fill. Each eye is
        // rebuilt from its OWN image at the slot's eye position, the walk extrapolated from the two images' cameras.
        // Scored on world and hand pixels off the outline bands against the scene traced at 20 ms. The control: the
        // same slots with the extrapolation off must show the world a whole run step behind.
        State r0 = still, r1 = still, r2 = still;
        r1.bodyPos = {0.03, 0, -0.04};   // 5 cm in 10 ms
        r2.bodyPos = {0.06, 0, -0.08};
        auto synthCase = [&](bool extrap, int eye, double* within, double* p95, int* wrongOut, double* handOk) -> bool {
            dvr::afw::set_enabled(true, "test");
            dvr::afw::set_stereo(true, "test");
            dvr::afw::set_body_depth(0.40f, "test");
            dvr::afw::set_world_scale((float)kScale);
            dvr::afw::set_matrices(true, "test");
            dvr::afw::set_fg(false, "test"); dvr::afw::set_fg_fov(0);
            dvr::afw::set_synth_extrapolate(extrap);
            g_signForeground = false; dvr::depthprobe::g_prefgReady = false;
            const Eye e0 = eyeOf(r0, 0), e1 = eyeOf(r1, 1), t2 = eyeOf(r2, eye);
            auto i0 = image(r0, e0, N, N), i1 = image(r1, e1, N, N), truth = image(r2, t2, N, N);
            ID3D11Texture2D* x0 = tex(g.dev, N, N, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, i0.data());
            ID3D11Texture2D* x1 = tex(g.dev, N, N, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, i1.data());
            ID3D11ShaderResourceView *v0 = nullptr, *v1 = nullptr;
            g.dev->CreateShaderResourceView(x0, nullptr, &v0); g.dev->CreateShaderResourceView(x1, nullptr, &v1);
            for (auto*& q : g_depthBySerial) q = nullptr;
            g_depthBySerial[3] = v0; g_depthBySerial[4] = v1; g_depthW = N; g_depthH = N;
            const Rec m0 = recordOf(r0, e0, false, false), m1 = recordOf(r1, e1, false, false);
            const dvr::afw::Pose tg0[2] = {poseOf(eyeOf(r0, 0)), poseOf(eyeOf(r0, 1))}, tg1[2] = {poseOf(eyeOf(r1, 0)), poseOf(eyeOf(r1, 1))};
            dvr::afw::CaptureMeta c0, c1; c0.captureMs = 1000.0; c1.captureMs = 1010.0;
            dvr::afw::note_capture(g.dev, g.ctx, 0, x0, 3, poseOf(e0), true, 0.0f, tg0, m0.vp, m0.c5, m0.rot, &c0);
            dvr::afw::note_capture(g.dev, g.ctx, 1, x1, 4, poseOf(e1), true, 0.0f, tg1, m1.vp, m1.c5, m1.rot, &c1);
            const float tp[3] = {(float)t2.pos.x, (float)t2.pos.y, (float)t2.pos.z};
            dvr::afw::Pose out{}; const char* why = nullptr;
            const bool ok = dvr::afw::synth_eye(g.dev, g.ctx, eye, g.dst, N, N, (float)kTan, (float)kTan, tp, 1020.0, &out, &why);
            if (!ok) printf("  synth refused: %s\n", why ? why : "?");
            std::vector<float> px(N * N * 4);
            if (ok) {
                g.ctx->CopyResource(g.stage, g.dst);
                D3D11_MAPPED_SUBRESOURCE m;
                if (SUCCEEDED(g.ctx->Map(g.stage, 0, D3D11_MAP_READ, 0, &m))) {
                    for (int y = 0; y < N; ++y) memcpy(&px[y * N * 4], (const uint8_t*)m.pData + y * m.RowPitch, N * 16);
                    g.ctx->Unmap(g.stage, 0);
                }
            }
            v0->Release(); v1->Release(); x0->Release(); x1->Release();
            if (!ok) return false;
            std::vector<double> errs; int wrong = 0, hn = 0, hg = 0;
            const double k = N / (2 * kTan);
            for (int y = 3; y < N - 3; ++y)
                for (int x = 3; x < N - 3; ++x) {
                    const int ts = surfOf(&truth[(y * N + x) * 4]);
                    bool edge = false;
                    for (int dy = -3; dy <= 3 && !edge; ++dy)
                        for (int dx = -3; dx <= 3; ++dx) if (surfOf(&truth[((y + dy) * N + x + dx) * 4]) != ts) { edge = true; break; }
                    if (edge) continue;
                    const float* o = &px[(y * N + x) * 4];
                    const float* t = &truth[(y * N + x) * 4];
                    if (ts == 2) { ++hn; hg += (o[2] > 0.5f && fabs(o[0] - t[0]) * kHandW / -kHandZ * k < 1.5); continue; }
                    if (surfOf(o) != ts || o[2] > 0.02f) { ++wrong; continue; }
                    const double sx = ts == 1 ? (kPillarX1 - kPillarX0) / -kPillarZ * k : 40.0 / 8.0 * k;
                    const double sy = ts == 1 ? 3.0 / -kPillarZ * k : sx;
                    errs.push_back(hypot((o[0] - t[0]) * sx, (o[1] - t[1]) * sy));
                }
            std::sort(errs.begin(), errs.end());
            int in = 0; for (double e : errs) in += e < 1.5;
            *within = errs.empty() ? 0 : (double)in / errs.size();
            *p95 = errs.empty() ? 1e9 : errs[errs.size() * 95 / 100];
            *wrongOut = wrong; *handOk = hn ? (double)hg / hn : 0;
            return true;
        };
        double w0, p0, h0, w1, p1, h1, wc, pc, hc; int x0, x1, xc;
        const bool a0 = synthCase(true, 0, &w0, &p0, &x0, &h0), a1 = synthCase(true, 1, &w1, &p1, &x1, &h1);
        const bool ac = synthCase(false, 0, &wc, &pc, &xc, &hc);
        dvr::afw::set_synth_extrapolate(true);
        char d[240];
        snprintf(d, sizeof(d), "left (20 ms old): world within 1.5 px %.3f p95 %.2f px, wrong %d, hands %.3f | right (10 ms old): %.3f p95 %.2f, "
                 "wrong %d, hands %.3f | control, no extrapolation: %.3f p95 %.2f", w0, p0, x0, h0, w1, p1, x1, h1, wc, pc);
        check("msw: a slot the game missed, running, rebuilt from each eye's own image", a0 && a1 && ac && w0 > 0.97 && w1 > 0.97 &&
              h0 > 0.97 && h1 > 0.97 && p0 < 1.5 && p1 < 1.5 && pc > 3.0, d);
    }
    {   // MSW hands: standing still, the controller moves 3 cm right between the image and the slot. With the hands
        // following their controllers the hand pixels must land where the scene at the slot has them; off, they lag
        // (the control). The hand is the foreground (the mask), its grip at the quad's centre.
        // depthMode: no foreground mask (run 11's case), the foreground is "nearer than the depth limit", and a near world
        // bar 40 cm from the grip falls inside that limit: it must stay where it is (barOk: its pixels still the bar).
        auto handCase = [&](bool follow, double* handOk, double* worldOk, bool depthMode = false, double* barOk = nullptr) -> bool {
            State h0 = still, h1 = still; h1.handX = 0.03;
            if (depthMode) { h0.handX = 0.10; h1.handX = 0.13; h0.barZ = h1.barZ = -0.30; h0.barX0 = h1.barX0 = -0.28; h0.barX1 = h1.barX1 = -0.25; }
            dvr::afw::set_enabled(true, "test"); dvr::afw::set_stereo(true, "test");
            dvr::afw::set_body_depth(0.40f, "test"); dvr::afw::set_world_scale((float)kScale);
            dvr::afw::set_matrices(true, "test"); dvr::afw::set_fg(false, "test"); dvr::afw::set_fg_fov(0);
            dvr::afw::set_fg_mask(true, "test");
            dvr::afw::set_synth_hands(follow);
            g_signForeground = !depthMode; dvr::depthprobe::g_prefgReady = !depthMode;
            if (depthMode) { dvr::afw::set_fg_fov(90.0f); dvr::afw::set_fg(true, "test"); dvr::afw::set_fg_depth(0.30f, "test"); }
            const Eye e0 = eyeOf(h0, 0), e1 = eyeOf(h0, 1);
            auto i0 = image(h0, e0, N, N), i1 = image(h0, e1, N, N), truth = image(h1, e0, N, N);
            ID3D11Texture2D* x0 = tex(g.dev, N, N, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, i0.data());
            ID3D11Texture2D* x1 = tex(g.dev, N, N, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, i1.data());
            ID3D11ShaderResourceView *v0 = nullptr, *v1 = nullptr;
            g.dev->CreateShaderResourceView(x0, nullptr, &v0); g.dev->CreateShaderResourceView(x1, nullptr, &v1);
            for (auto*& q : g_depthBySerial) q = nullptr;
            g_depthBySerial[5] = v0; g_depthBySerial[6] = v1; g_depthW = N; g_depthH = N;
            const Rec m0 = recordOf(h0, e0, false, false), m1 = recordOf(h0, e1, false, false);
            const dvr::afw::Pose tg[2] = {poseOf(e0), poseOf(e1)};
            dvr::afw::CaptureMeta c0, c1; c0.captureMs = 2000.0; c1.captureMs = 2010.0;
            dvr::afw::note_capture(g.dev, g.ctx, 0, x0, 5, poseOf(e0), true, 0.0f, tg, m0.vp, m0.c5, m0.rot, &c0);
            dvr::afw::note_capture(g.dev, g.ctx, 1, x1, 6, poseOf(e1), true, 0.0f, tg, m1.vp, m1.c5, m1.rot, &c1);
            dvr::afw::HandPose then[2], now[2];
            then[1].ok = now[1].ok = true;   // the right grip at the hand quad's centre
            then[1].p[0] = (float)h0.handX; then[1].p[1] = (float)kHandY; then[1].p[2] = (float)kHandZ;
            now[1] = then[1]; now[1].p[0] = (float)h1.handX;
            dvr::afw::note_hands(0, then); dvr::afw::note_hands(1, then);
            const float tp[3] = {(float)e0.pos.x, (float)e0.pos.y, (float)e0.pos.z};
            dvr::afw::Pose out{}; const char* why = nullptr;
            const bool ok = dvr::afw::synth_eye(g.dev, g.ctx, 0, g.dst, N, N, (float)kTan, (float)kTan, tp, 2020.0, &out, &why, now);
            std::vector<float> px(N * N * 4);
            if (ok) {
                g.ctx->CopyResource(g.stage, g.dst);
                D3D11_MAPPED_SUBRESOURCE m;
                if (SUCCEEDED(g.ctx->Map(g.stage, 0, D3D11_MAP_READ, 0, &m))) {
                    for (int y = 0; y < N; ++y) memcpy(&px[y * N * 4], (const uint8_t*)m.pData + y * m.RowPitch, N * 16);
                    g.ctx->Unmap(g.stage, 0);
                }
            } else printf("  synth refused: %s\n", why ? why : "?");
            v0->Release(); v1->Release(); x0->Release(); x1->Release();
            g_signForeground = false; dvr::depthprobe::g_prefgReady = false;
            dvr::afw::set_synth_hands(false);
            if (!ok) return false;
            int hn = 0, hg = 0, wn = 0, wg = 0, bn = 0, bg = 0;
            const double k = N / (2 * kTan);
            for (int y = 3; y < N - 3; ++y)
                for (int x = 3; x < N - 3; ++x) {
                    const int ts = surfOf(&truth[(y * N + x) * 4]);
                    bool edge = false;
                    for (int dy = -3; dy <= 3 && !edge; ++dy)
                        for (int dx = -3; dx <= 3; ++dx) if (surfOf(&truth[((y + dy) * N + x + dx) * 4]) != ts) { edge = true; break; }
                    if (edge) continue;
                    const float* o = &px[(y * N + x) * 4];
                    const float* t = &truth[(y * N + x) * 4];
                    if (ts == 3) { ++bn; bg += surfOf(o) == 3; }
                    if (ts == 2) { ++hn; hg += (o[2] > 0.5f && fabs(o[0] - t[0]) * kHandW / -kHandZ * k < 1.5 && fabs(o[1] - t[1]) * kHandH / -kHandZ * k < 1.5); }
                    else if (o[2] < 0.5f) { ++wn; ++wg; }   // world where the truth is world (the uncovered strip excepted below)
                    else ++wn;
                }
            *handOk = hn ? (double)hg / hn : 0; *worldOk = wn ? (double)wg / wn : 0;
            if (barOk) *barOk = bn ? (double)bg / bn : 0;
            if (depthMode) { dvr::afw::set_fg(false, "test"); dvr::afw::set_fg_fov(0); }
            return true;
        };
        double hf = 0, wf = 0, hc = 0, wc = 0;
        const bool a = handCase(true, &hf, &wf), b = handCase(false, &hc, &wc);
        char d[200];
        snprintf(d, sizeof(d), "hands on: hand %.3f, world free of hand %.3f | control, off: hand %.3f, world free of hand %.3f", hf, wf, hc, wc);
        check("msw: the hands follow their controllers into a synthesized slot", a && b && hf > 0.97 && wf > 0.99 && hc < 0.7, d);
        double hd = 0, wd = 0, bd = 0;
        const bool e = handCase(true, &hd, &wd, true, &bd);
        snprintf(d, sizeof(d), "no mask (depth limit): hand %.3f, the near bar (0.3 m away, 0.4 m from the grip) still the bar %.3f", hd, bd);
        check("msw: without the mask, near world geometry does not follow the hand", e && hd > 0.97 && bd > 0.97, d);
    }
    {   // MSW turning (run 10: the turn rode the submitted pose and turned the hands with the world). A stick turn of
        // 2 deg per 10 ms: the left image at 0 ms (yaw 0), the right at 10 ms (yaw 2), the slot at 20 ms (yaw 4). The
        // world must be where the scene at the slot has it AND the hands must stay put (tracking space), in the
        // image as submitted at the image's own orientation. Both world models: the game's matrices and the XR one.
        // The control: the extrapolation off leaves the world 4 deg behind. About 7% of the world is unseen by the
        // 20 ms old image (4 deg of new view at the edge, the strip behind the pillar): 0.9 is the bar.
        auto turnCase = [&](bool extrap, bool mtx, double* worldOk, double* handOk,
                            bool slotClock = false, int eye = 0, bool headMotion = false, bool staleWriter = false,
                            int64_t slotTime = 1020000000, bool offOrigin = false, double yawStep = 2.0,
                            V3 jump = {0, 0, 0}) -> bool {
            State t0 = still, t1 = still, t2 = still;
            t1.bodyYawDeg = yawStep; t2.bodyYawDeg = 2 * yawStep;
            t1.bodyPos = jump; t2.bodyPos = jump;
            if (headMotion) { t0.headYawDeg = 8.0; t1.headYawDeg = -3.0; }
            if (offOrigin) t0.headPos = t1.headPos = t2.headPos = {1.0, 0, 0.3};
            // Synthesis preserves the source orientation; the compositor supplies the remaining head turn.
            t2.headYawDeg = eye == 0 ? t0.headYawDeg : t1.headYawDeg;
            dvr::afw::set_enabled(true, "test"); dvr::afw::set_stereo(true, "test");
            dvr::afw::set_body_depth(0.40f, "test"); dvr::afw::set_world_scale((float)kScale);
            dvr::afw::set_matrices(mtx, "test"); dvr::afw::set_fg(false, "test"); dvr::afw::set_fg_fov(0);
            dvr::afw::set_fg_mask(true, "test"); dvr::afw::set_synth_hands(false);
            dvr::afw::set_synth_extrapolate(extrap);
            g_signForeground = true; dvr::depthprobe::g_prefgReady = true;
            const Eye e0 = eyeOf(t0, 0), e1 = eyeOf(t1, 1), e2 = eyeOf(t2, eye);
            auto i0 = image(t0, e0, N, N), i1 = image(t1, e1, N, N), truth = image(t2, e2, N, N);
            ID3D11Texture2D* x0 = tex(g.dev, N, N, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, i0.data());
            ID3D11Texture2D* x1 = tex(g.dev, N, N, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, i1.data());
            ID3D11ShaderResourceView *v0 = nullptr, *v1 = nullptr;
            g.dev->CreateShaderResourceView(x0, nullptr, &v0); g.dev->CreateShaderResourceView(x1, nullptr, &v1);
            for (auto*& q : g_depthBySerial) q = nullptr;
            g_depthBySerial[7] = v0; g_depthBySerial[8] = v1; g_depthW = N; g_depthH = N;
            const Rec m0 = recordOf(t0, e0, false, false), m1 = recordOf(t1, e1, false, false);
            const dvr::afw::Pose tg0[2] = {poseOf(eyeOf(t0, 0)), poseOf(eyeOf(t0, 1))}, tg1[2] = {poseOf(eyeOf(t1, 0)), poseOf(eyeOf(t1, 1))};
            dvr::afw::CaptureMeta c0, c1; c0.captureMs = 3000.0; c1.captureMs = 3010.0;
            if (slotClock) { c0.displayTime = 1000000000; c1.displayTime = 1010000000; c1.captureMs = 3002.0; }
            dvr::afw::note_capture(g.dev, g.ctx, 0, x0, 7, poseOf(e0), true, staleWriter ? -20.0f : (float)t0.bodyYawDeg, tg0, m0.vp, m0.c5, m0.rot, &c0);
            dvr::afw::note_capture(g.dev, g.ctx, 1, x1, 8, poseOf(e1), true, staleWriter ? -20.0f : (float)t1.bodyYawDeg, tg1, m1.vp, m1.c5, m1.rot, &c1);
            const float tp[3] = {(float)e2.pos.x, (float)e2.pos.y, (float)e2.pos.z};
            dvr::afw::Pose out{}; const char* why = nullptr;
            const bool ok = dvr::afw::synth_eye(g.dev, g.ctx, eye, g.dst, N, N, (float)kTan, (float)kTan, tp,
                                              slotClock ? 3075.0 : 3020.0, &out, &why, nullptr, slotClock ? slotTime : 0);
            std::vector<float> px(N * N * 4);
            if (ok) {
                g.ctx->CopyResource(g.stage, g.dst);
                D3D11_MAPPED_SUBRESOURCE m;
                if (SUCCEEDED(g.ctx->Map(g.stage, 0, D3D11_MAP_READ, 0, &m))) {
                    for (int y = 0; y < N; ++y) memcpy(&px[y * N * 4], (const uint8_t*)m.pData + y * m.RowPitch, N * 16);
                    g.ctx->Unmap(g.stage, 0);
                }
            } else printf("  synth refused: %s\n", why ? why : "?");
            v0->Release(); v1->Release(); x0->Release(); x1->Release();
            g_signForeground = false; dvr::depthprobe::g_prefgReady = false;
            dvr::afw::set_synth_extrapolate(true); dvr::afw::set_matrices(true, "test");
            if (!ok) return false;
            int wn = 0, wg = 0, hn = 0, hg = 0;
            const double k = N / (2 * kTan);
            for (int y = 3; y < N - 3; ++y)
                for (int x = 3; x < N - 3; ++x) {
                    const int ts = surfOf(&truth[(y * N + x) * 4]);
                    bool edge = false;
                    for (int dy = -3; dy <= 3 && !edge; ++dy)
                        for (int dx = -3; dx <= 3; ++dx) if (surfOf(&truth[((y + dy) * N + x + dx) * 4]) != ts) { edge = true; break; }
                    if (edge) continue;
                    const float* o = &px[(y * N + x) * 4];
                    const float* t = &truth[(y * N + x) * 4];
                    if (ts == 2) { ++hn; hg += (o[2] > 0.5f && fabs(o[0] - t[0]) * kHandW / -kHandZ * k < 1.5); continue; }
                    ++wn;
                    if (surfOf(o) != ts || o[2] > 0.02f) continue;
                    const double sx = ts == 1 ? (kPillarX1 - kPillarX0) / -kPillarZ * k : 40.0 / 8.0 * k;
                    const double sy = ts == 1 ? 3.0 / -kPillarZ * k : sx;
                    wg += hypot((o[0] - t[0]) * sx, (o[1] - t[1]) * sy) < 1.5;
                }
            *worldOk = wn ? (double)wg / wn : 0; *handOk = hn ? (double)hg / hn : 0;
            return true;
        };
        double wm, hm, wx, hx, wc, hc;
        const bool a = turnCase(true, true, &wm, &hm), b = turnCase(true, false, &wx, &hx), c = turnCase(false, true, &wc, &hc);
        char d[240];
        snprintf(d, sizeof(d), "matrices: world %.3f hands %.3f | XR model: world %.3f hands %.3f | control, no extrapolation: world %.3f hands %.3f",
                 wm, hm, wx, hx, wc, hc);
        check("msw: a stick turn turns the world in the image, the hands stay", a && b && c && wm > 0.9 && hm > 0.97 && wx > 0.9 &&
              hx > 0.97 && wc < 0.5, d);
        for (int eye = 0; eye < 2; ++eye) {
            double w = 0, h = 0;
            const bool ok = turnCase(true, true, &w, &h, true, eye, false, true);
            snprintf(d, sizeof(d), "eye %d world %.3f hands %.3f: irregular capture times, stale writer, common display slot", eye, w, h);
            check("msw: stick turn follows the display timeline and rendered yaw", ok && w > 0.9 && h > 0.97, d);
            const bool head = turnCase(true, true, &w, &h, true, eye, true, true);
            snprintf(d, sizeof(d), "eye %d world %.3f hands %.3f: 11 deg head change must not become extra body motion", eye, w, h);
            check("msw: rendered turn prediction removes the source head rotation", head && w > 0.9 && h > 0.97, d);
        }
        double wb = 0, hb = 0;
        const bool clockOnly = turnCase(true, true, &wb, &hb, true);
        check("msw: irregular readback timing cannot change the predicted display slot", clockOnly && wb > 0.9 && hb > 0.97,
              "correct camera-writer yaw; only the capture clock is perturbed");
        const bool writerOnly = turnCase(true, true, &wb, &hb, false, 0, false, true);
        check("msw: stale camera-writer yaw cannot suppress a rendered stick turn", writerOnly && wb > 0.9 && hb > 0.97,
              "regular timing; only the camera-writer yaw is perturbed");
        const bool bad = turnCase(true, true, &wb, &hb, true, 0, false, true, 999000000);
        check("msw: a target before the source refuses backward prediction", bad && wb < 0.5 && hb > 0.97,
              "invalid display order keeps the original body motion; no clock-domain fallback");
        for (int eye = 0; eye < 2; ++eye) {
            const bool off = turnCase(true, true, &wb, &hb, true, eye, true, true, 1020000000, true);
            snprintf(d, sizeof(d), "eye %d world %.3f: head 1 metre from tracking origin must orbit with the turn, not walk", eye, wb);
            check("msw: off-centre stick turn keeps the body pivot", off && wb > 0.9, d);
        }
        // VR-39 MSW guards (PLAN-mod-spacewarp section 7). Each case has its control: the same images with the guard
        // (or the stick stop) off must show the fault the guard exists for.
        {
            dvr::afw::set_synth_guard(true);
            const bool reg = turnCase(true, true, &wb, &hb, true);
            const char* holdReg = dvr::afw::synth_hold_reason(true);
            snprintf(d, sizeof(d), "200 deg/s stick turn: hold %s, world %.3f hands %.3f", holdReg ? holdReg : "none", wb, hb);
            check("msw guard: an ordinary stick turn is neither held nor bent", reg && !holdReg && wb > 0.9 && hb > 0.97, d);

            turnCase(true, true, &wb, &hb, true, 0, false, false, 1020000000, false, 30.0);   // a 30 deg snap in one 10 ms tick
            const char* holdSnap = dvr::afw::synth_hold_reason(true);
            dvr::afw::set_synth_guard(false);
            const char* holdSnapOff = dvr::afw::synth_hold_reason(true);
            snprintf(d, sizeof(d), "guard on: %s | control, guard off: %s", holdSnap ? holdSnap : "none", holdSnapOff ? holdSnapOff : "none");
            check("msw guard: a snap turn between the held images holds the slot", holdSnap && !strcmp(holdSnap, "turn jump") && !holdSnapOff, d);

            dvr::afw::set_synth_guard(true);
            turnCase(true, true, &wb, &hb, true, 0, false, false, 1020000000, false, 0.0, V3{0, 0, -3.0});   // a 3 m Blink step
            const char* holdBlink = dvr::afw::synth_hold_reason(true);
            dvr::afw::set_synth_guard(false);
            const char* holdBlinkOff = dvr::afw::synth_hold_reason(true);
            snprintf(d, sizeof(d), "guard on: %s | control, guard off: %s", holdBlink ? holdBlink : "none", holdBlinkOff ? holdBlinkOff : "none");
            check("msw guard: a Blink-sized camera jump holds the slot", holdBlink && !strcmp(holdBlink, "camera jump") && !holdBlinkOff, d);

            // The per-eye clamp: a 4 deg slot turn clamped to 1 deg leaves the world mostly behind (like the control);
            // the default 25 deg leaves it alone.
            dvr::afw::set_synth_guard(true);
            float s0 = 0, r0 = 0, c0 = 0; dvr::afw::synth_limits(&s0, &r0, &c0);
            dvr::afw::set_synth_limits(0, 0, 1.0f);
            double wClamp = 0, hClamp = 0;
            const bool clamp = turnCase(true, true, &wClamp, &hClamp, true);
            dvr::afw::set_synth_limits(0, 0, c0);
            const bool unclamped = turnCase(true, true, &wb, &hb, true);
            dvr::afw::set_synth_guard(false);
            snprintf(d, sizeof(d), "clamp 1 deg: world %.3f | clamp %.0f deg: world %.3f", wClamp, c0, wb);
            check("msw guard: the per-eye turn clamp bounds the extrapolated turn", clamp && unclamped && wClamp < 0.5 &&
                  hClamp > 0.97 && wb > 0.9, d);

            // The stick stop: stick released, the 4 deg slot turn is not extrapolated; held, it is.
            dvr::afw::set_synth_stick_stop(true);
            dvr::afw::note_turn_stick(0.0f);
            double wStop = 0, hStop = 0;
            const bool stop = turnCase(true, true, &wStop, &hStop, true);
            dvr::afw::note_turn_stick(1.0f);
            const bool held = turnCase(true, true, &wb, &hb, true);
            dvr::afw::set_synth_stick_stop(false);
            dvr::afw::note_turn_stick(0.0f);
            const bool off = turnCase(true, true, &wc, &hc, true);
            snprintf(d, sizeof(d), "stick released: world %.3f | stick held: world %.3f | stick stop off, released: world %.3f",
                     wStop, wb, wc);
            check("msw stick stop: a released stick stops the extrapolated turn", stop && held && off && wStop < 0.5 &&
                  hStop > 0.97 && wb > 0.9 && wc > 0.9, d);
        }
    }
    {   // Freshness: a record from an earlier present, and a toggle without a capture, are refused.
        const Eye h0 = eyeOf(still, 0), f1 = eyeOf(turn, 1);
        auto hImg = image(still, h0, N, N), fImg = image(turn, f1, N, N);
        Scene sc = capture(g, still, turn, Opt(), N, N, hImg, fImg);
        dvr::afw::Pose out{}; const char* why = nullptr;
        const bool now = dvr::afw::warp_held(g.dev, g.ctx, 0, 1, sc.sf, g.dst, N, N, 1, 1, &out, &why);
        const bool later = dvr::afw::warp_held(g.dev, g.ctx, 0, 1, sc.sf + 2, g.dst, N, N, 1, 1, &out, &why);
        dvr::afw::set_enabled(false, "test"); dvr::afw::set_enabled(true, "test");
        const bool toggled = dvr::afw::warp_held(g.dev, g.ctx, 0, 1, sc.sf, g.dst, N, N, 1, 1, &out, &why);
        char d[128]; snprintf(d, sizeof(d), "this present %d, a later present with no capture %d, after a toggle %d", now, later, toggled);
        check("freshness: only this present's capture is rebuilt from", now && !later && !toggled, d);
        release(sc);
    }
    {   // The diagnostic capture: two consecutive presents, every file present and the right size.
        char root[MAX_PATH]; GetTempPathA(MAX_PATH, root);
        strcat_s(root, "afw-dump-test");
        CreateDirectoryA(root, nullptr);
        const Eye h0 = eyeOf(still, 0), f1 = eyeOf(turn, 1);
        auto hImg = image(still, h0, N, N), fImg = image(turn, f1, N, N);
        Scene sc = capture(g, still, turn, Opt(), N, N, hImg, fImg);
        dvr::afw::request_dump(2, 0, root, "test");
        dvr::afw::Pose out{}; const char* why = nullptr;
        for (int i = 0; i < 3; ++i) dvr::afw::warp_held(g.dev, g.ctx, 0, 1, sc.sf, g.dst, N, N, 1, 1, &out, &why);
        const char* st = dvr::afw::dump_status();
        const char* dir = strstr(st, " in ") ? strstr(st, " in ") + 4 : "";
        int ok = 0;
        const char* names[5] = {"_fresh.raw", "_fresh_depth.raw", "_held.raw", "_held_depth.raw", "_rebuilt.raw"};
        const long sizes[5] = {N * N * 16, N * N * 2, N * N * 16, N * N * 2, N * N * 16};
        for (int p = 0; p < 2; ++p)
            for (int k = 0; k < 5; ++k) {
                char path[MAX_PATH]; snprintf(path, sizeof(path), "%s\\p%02d%s", dir, p, names[k]);
                WIN32_FILE_ATTRIBUTE_DATA fa = {};
                if (GetFileAttributesExA(path, GetFileExInfoStandard, &fa) && (long)fa.nFileSizeLow == sizes[k]) ++ok;
            }
        char d[300]; snprintf(d, sizeof(d), "%d of 10 files with the right size | status: %s", ok, st);
        check("diagnostic capture writes both presents", ok == 10 && !strncmp(st, "done", 4), d);
        release(sc);
    }
    { Result r = run(g, fgStill, fgStill, fgOn); report("foreground at its own FOV, still: hands exact", r, clean(r)); }
    { Result r = run(g, still, fgTurn, fgOn);    report("foreground at its own FOV, turn", r, clean(r)); }
    { Result r = run(g, still, fgMoved, fgOn);   report("foreground at its own FOV, hand moved", r, clean(r)); }
    {   // Run 25: a hand moving 3 cm left at the left edge of view. Its left strip is outside the fresh (right) eye's frame,
        // and the held image drew it 3 cm to the right: with the grips, the held eye's own hand is moved to where the hand is
        // NOW and fills the strip; without them (the control), the strip is missing.
        State e0 = still; e0.handX = -0.33;
        State e1 = still; e1.handX = -0.36;
        Opt gOn; gOn.grips = true; gOn.mask = true;   // the foreground known from the mask, as in the game
        Opt gOff; gOff.mask = true;
        Result a = run(g, e0, e1, gOn), b = run(g, e0, e1, gOff);
        char d[200]; snprintf(d, sizeof(d), "with grips: hand %d, missing %d, ghost %d | control (no grips): missing %d, ghost %d",
                              a.handTruth, a.missing, a.ghost, b.missing, b.ghost);
        check("held hands follow the controllers: a moving hand's edge strip fills", a.ok && b.ok && a.handTruth > 500 &&
              a.missing < b.missing / 3 && a.ghost < 50 && b.missing > 100, d);
    }
    { Result r = run(g, fgStill, fgStill);
      report("control: foreground FOV not applied -> the hands misplaced", r, r.ok && (r.ghost + r.missing) * 20 > r.handTruth); }
    // The run-8 faults: a foreground farther than the depth limit (a sword pointed away), and a WORLD surface
    // nearer than it (a wall close by). The mask tells them apart by what the foreground pass drew.
    {   State tip0 = still; tip0.fgTan = kFgTan; tip0.handZ = -1.2; tip0.handX = 0.45; tip0.handW = 0.3;   // 1.2 m, off-centre like a sword tip: 0.48 units, past the 0.30 limit
        State tip1 = tip0; tip1.bodyYawDeg = 3; tip1.handX = 0.47;
        Opt m = fgOn; m.mask = true;
        Result r = run(g, tip0, tip1, m); report("mask: a foreground at 1.2 m keeps its projection", r, clean(r, 100));
        // No control here: a flat foreground wholly past the limit is carried well by the held eye even without the
        // mask. The headset fault was a sword CROSSING the limit along its length (half fresh, half held), which a
        // fronto-parallel quad cannot model; the near-wall control below does fail without the mask.
    }
    {   State wall0 = still; wall0.fgTan = kFgTan; wall0.barZ = -0.35; wall0.barX0 = -0.30; wall0.barX1 = -0.10;   // a world bar at 0.35 m
        State wall1 = wall0; wall1.bodyYawDeg = 3;
        Opt m = fgOn; m.mask = true;
        Result r = run(g, wall0, wall1, m); report("mask: a world surface at 0.35 m stays world", r, clean(r, 100));
        Opt c = fgOn;
        r = run(g, wall0, wall1, c); report("control: depth limit, the near wall misprojected", r, r.ok && r.errP95 > 2.0); }
    {   // Run 17: the same two cases classified by the DRAWN mask (every foreground draw drawn again into it), with the
        // depths unsigned: a close world surface stays world and a far foreground keeps the foreground's projection.
        State tip0 = still; tip0.fgTan = kFgTan; tip0.handZ = -1.2; tip0.handX = 0.45; tip0.handW = 0.3;
        State tip1 = tip0; tip1.bodyYawDeg = 3; tip1.handX = 0.47;
        Opt m = fgOn; m.drawnMask = true;
        Result r = run(g, tip0, tip1, m); report("drawn mask: a foreground at 1.2 m keeps its projection", r, clean(r, 100));
        State wall0 = still; wall0.fgTan = kFgTan; wall0.barZ = -0.35; wall0.barX0 = -0.30; wall0.barX1 = -0.10;
        State wall1 = wall0; wall1.bodyYawDeg = 3;
        r = run(g, wall0, wall1, m); report("drawn mask: a world surface at 0.35 m stays world", r, clean(r, 100)); }
    {   // A foreground hand is drawn over a closer wall. Moving toward the wall must not make the
        // disocclusion fill stretch the geometrically farther hand into a second copy.
        State a = still; a.fgTan = kFgTan; a.barZ = -0.28; a.barX0 = -2.0; a.barX1 = 2.0;
        State b = a; b.bodyPos = {0, 0, -0.05};
        Opt m = fgOn; m.drawnMask = true;
        Result r = run(g, a, b, m);
        report("foreground inside a wall: background fill does not duplicate the hand", r,
               r.ok && r.handTruth > 500 && r.ghost < r.handTruth / 100 && r.missing < r.handTruth / 100);
    }
    {   // The foreground draws on top of a nearer wall. In a still scene the held eye already
        // has the exact wall colour: the other eye's foreground cannot prove that wall moved.
        State a = still; a.fgTan = kFgTan; a.barZ = -0.28; a.barX0 = -2.0; a.barX1 = 2.0;
        Opt m = fgOn; m.drawnMask = true;
        Result r = run(g, a, a, m);
        report("foreground inside a wall: still background stays exact", r,
               clean(r) && r.errMax < 2.0);
        printf("  still wall maximum coordinate error %.3f px\n", r.errMax);
        m.freshWorld=true;r=run(g,a,a,m);
        report("current-time world: foreground inside wall",r,clean(r) && r.errMax<2.0);
        State b=a;b.bodyPos={0,0,-.05};r=run(g,a,b,m);
        report("current-time world: moving into wall",r,r.ok && r.handTruth>500 && r.ghost<r.handTruth/100 && r.missing<r.handTruth/100);
    }
    g_signForeground = false; dvr::depthprobe::g_prefgReady = false;
    // NEGATIVE CONTROLS: the same motion with a lever off must show the fault.
    {   // At the pre-run-18 stale tolerance: the tighter test (0.015) already catches this lag through the fresh eye, which
        // would leave the matrices lever without a control.
        dvr::afw::set_stale(0.03f, "test");
        Result r = run(g, still, walk, noMtx);
        dvr::afw::set_stale(0.015f, "test");
        report("control: no matrices, walking -> the pillar lags", r, r.ok && r.errP95 > 3.0); }
    { Result r = run(g, still, moved, noStereo);
      report("control: held eye alone, hand moved -> ghost", r, r.ok && r.ghost > r.handTruth / 5); }
    { Result r = run(g, still, all, noStereo);
      report("control: held eye alone, everything -> ghost", r, r.ok && r.ghost > r.handTruth / 10); }
    // Fallbacks. The temporal-only rebuild (the A/B and the no-fresh-depth fallback) keeps the hand and
    // turns the world, but cannot see behind the old hand: its trailing copy is measured, not hidden.
    { Result r = run(g, still, turn, noStereo);
      report("held eye alone, turn: hand kept, world turned (trail known)", r,
             r.ok && r.missing * 100 <= r.handTruth && r.errP50 < 1.0 && r.ghost < r.handTruth * 3 / 10); }
    { Opt o; o.heldDepth = false; Result r = run(g, still, moved, o);
      report("no held depth: the fresh eye alone, hand moved", r, r.ok && r.ghost * 100 <= r.handTruth && r.missing * 50 <= r.handTruth); }
    { Opt o; o.freshDepth = false; Result r = run(g, still, turn, o);
      report("no fresh depth: the temporal split, turn (trail known)", r,
             r.ok && r.missing * 100 <= r.handTruth && r.errP50 < 1.0 && r.ghost < r.handTruth * 3 / 10); }

    {   // The cost at the headset's eye size: the whole rebuild (seed maps + compose), GPU timestamps.
        const int W = 2750, H = 2850;
        const Eye h0 = eyeOf(still, 0), f1 = eyeOf(all, 1);
        auto hImg = image(still, h0, W, H), fImg = image(all, f1, W, H);
        ID3D11Texture2D* big = tex(g.dev, W, H, D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, nullptr);
        Scene sc = capture(g, still, all, Opt(), W, H, hImg, fImg);
        D3D11_QUERY_DESC qd = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0}, qt = {D3D11_QUERY_TIMESTAMP, 0};
        ID3D11Query *dj = nullptr, *qa = nullptr, *qb = nullptr;
        g.dev->CreateQuery(&qd, &dj); g.dev->CreateQuery(&qt, &qa); g.dev->CreateQuery(&qt, &qb);
        double sum = 0; int n = 0; bool ok = big && dj && qa && qb;
        for (int i = 0; ok && i < 40; ++i) {
            dvr::afw::Pose out{}; const char* why = nullptr;
            g.ctx->Begin(dj); g.ctx->End(qa);
            ok = dvr::afw::warp_held(g.dev, g.ctx, 0, 1, sc.sf, big, W, H, 1, 1, &out, &why);
            g.ctx->End(qb); g.ctx->End(dj);
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d = {}; UINT64 a = 0, b = 0;
            while (g.ctx->GetData(dj, &d, sizeof(d), 0) == S_FALSE) {}
            g.ctx->GetData(qa, &a, sizeof(a), 0); g.ctx->GetData(qb, &b, sizeof(b), 0);
            if (i >= 5 && !d.Disjoint && d.Frequency) { sum += (double)(b - a) * 1000.0 / (double)d.Frequency; ++n; }
        }
        char dsc[128]; snprintf(dsc, sizeof(dsc), "%.3f ms mean over %d rebuilds (seed maps + compose, excluding the depth copies)", n ? sum / n : -1.0, n);
        check("cost: one rebuild at 2750x2850 (informational)", ok && n > 0, dsc);
        // A synthesized eye (MSW), 20 back to back between one pair of timestamps (single spaced passes read the
        // GPU's idle clock), per seed grid step; and the full rebuild measured the same way for comparison.
        auto timed = [&](auto fn) {
            double s = 0; int m = 0;
            for (int it = 0; it < 6; ++it) {
                g.ctx->Begin(dj); g.ctx->End(qa);
                for (int r = 0; r < 20; ++r) fn();
                g.ctx->End(qb); g.ctx->End(dj);
                D3D11_QUERY_DATA_TIMESTAMP_DISJOINT d = {}; UINT64 a = 0, b = 0;
                while (g.ctx->GetData(dj, &d, sizeof(d), 0) == S_FALSE) {}
                g.ctx->GetData(qa, &a, sizeof(a), 0); g.ctx->GetData(qb, &b, sizeof(b), 0);
                if (it >= 2 && !d.Disjoint && d.Frequency) { s += (double)(b - a) * 1000.0 / (double)d.Frequency / 20.0; ++m; }
            }
            return m ? s / m : -1.0;
        };
        const Eye t0 = eyeOf(all, 0);
        const float tp[3] = {(float)t0.pos.x, (float)t0.pos.y, (float)t0.pos.z};
        double c[4] = {};
        const int grids[4] = {2, 4, 8, 16};
        for (int k = 0; k < 4; ++k) {
            dvr::afw::set_synth_grid(grids[k]);
            c[k] = timed([&] { dvr::afw::Pose o{}; const char* w = nullptr;
                               dvr::afw::synth_eye(g.dev, g.ctx, 0, big, W, H, 1, 1, tp, 1.0, &o, &w); });
        }
        dvr::afw::set_synth_grid(4);
        const double full = timed([&] { dvr::afw::Pose o{}; const char* w = nullptr;
                                        dvr::afw::warp_held(g.dev, g.ctx, 0, 1, sc.sf, big, W, H, 1, 1, &o, &w); });
        snprintf(dsc, sizeof(dsc), "synthesized eye: grid 2 %.3f ms, 4 %.3f, 8 %.3f, 16 %.3f | full rebuild %.3f ms (back to back)",
                 c[0], c[1], c[2], c[3], full);
        check("cost: a synthesized eye at 2750x2850 (informational)", c[1] > 0, dsc);
        release(sc);
        if (dj) dj->Release(); if (qa) qa->Release(); if (qb) qb->Release(); if (big) big->Release();
    }
    {   // Run 26: a render-size change first (DLSS on/off), as the headset session did: three captures with clean images at
        // another size, so the clean textures that rotate between the pending slot and the two records hold the old size.
        // The UI case below must still hold; before the fix a stale wrong-size clean image kept circulating.
        const int S = 256;
        State elsewhere = still; elsewhere.handX = 0.25;   // a stale image would put the hand somewhere else
        auto a = image(elsewhere, eyeOf(elsewhere, 0), S, S), b = image(elsewhere, eyeOf(elsewhere, 1), S, S);
        for (int i = 0; i < 3; ++i) { Scene sc = capture(g, elsewhere, elsewhere, Opt(), S, S, a, b, &a, &b); release(sc); }
    }
    {   // Run 15: the mod's own layers (objective markers, the F10 panel) are drawn into each eye's image AFTER the game.
        // The held eye's hands come from the fresh eye; from its COMPOSED image they carried that eye's UI into the other
        // eye (text on the sword). The fresh eye's hand carries a UI stamp (colour 99) in its composed image only; the held
        // eye has its own UI stamp (colour 77) over part of its hand. With the clean sources, the rebuilt hand must show
        // no 99 and must keep the 77 where the held eye drew it. The control (clean off) must show the 99.
        const Eye h0 = eyeOf(still, 0), f1 = eyeOf(still, 1);
        auto hClean = image(still, h0, N, N), fClean = image(still, f1, N, N);
        auto hComp = hClean, fComp = fClean;
        int heldUi = 0;
        for (int y = 0; y < N; ++y)
            for (int x = 0; x < N; ++x) {
                float* q = &fComp[((size_t)y * N + x) * 4];
                if (q[2] > 0.5f) { q[0] = 99.0f; q[1] = 99.0f; }                 // the fresh eye's UI over its whole hand
                float* r = &hComp[((size_t)y * N + x) * 4];
                if (r[2] > 0.5f && x < N / 2) { r[0] = 77.0f; r[1] = 77.0f; ++heldUi; }   // the held eye's UI over half of its hand
            }
        auto count = [&](bool cleanOn, int* stamp99, int* stamp77, int* hand) {
            dvr::afw::set_clean(cleanOn, "test");
            g_signForeground = false; dvr::depthprobe::g_prefgReady = false;
            Scene sc = capture(g, still, still, Opt(), N, N, hComp, fComp, &hClean, &fClean);
            dvr::afw::Pose out{}; const char* why = nullptr;
            const bool ok = dvr::afw::warp_held(g.dev, g.ctx, 0, 1, sc.sf, g.dst, N, N, (float)kTan, (float)kTan, &out, &why);
            *stamp99 = *stamp77 = *hand = 0;
            if (ok) {
                g.ctx->CopyResource(g.stage, g.dst);
                D3D11_MAPPED_SUBRESOURCE m;
                if (SUCCEEDED(g.ctx->Map(g.stage, 0, D3D11_MAP_READ, 0, &m))) {
                    for (int y = 0; y < N; ++y)
                        for (int x = 0; x < N; ++x) {
                            const float* o = (const float*)((const uint8_t*)m.pData + y * m.RowPitch) + x * 4;
                            if (o[2] > 0.5f) ++*hand;
                            if (fabsf(o[0] - 99.0f) < 0.5f) ++*stamp99;
                            if (fabsf(o[0] - 77.0f) < 0.5f) ++*stamp77;
                        }
                    g.ctx->Unmap(g.stage, 0);
                }
            }
            release(sc);
            return ok;
        };
        int a99, a77, ah, b99, b77, bh;
        const bool on = count(true, &a99, &a77, &ah), off = count(false, &b99, &b77, &bh);
        dvr::afw::set_clean(true, "test");
        char d[200];
        snprintf(d, sizeof(d), "clean: hand %d px, other eye's UI %d, own UI %d of %d | control (off): other eye's UI %d",
                 ah, a99, a77, heldUi, b99);
        check("the other eye's UI stays off the rebuilt sword, the held eye's own UI stays",
              on && off && ah > 6000 && a99 == 0 && a77 > heldUi * 9 / 10 && b99 > 500, d);   // 6108 hand texels when right
    }
    {   // Run 21: a still weapon keeps each eye's own shine. The held eye's hand carries a marker (+100 in channel 1: its own,
        // view-dependent shading); with the controllers reported still, the rebuilt hand must be the held eye's own; reported
        // moving (the control), the fresh eye's.
        const Eye h0 = eyeOf(still, 0), f1 = eyeOf(still, 1);
        auto hImg = image(still, h0, N, N), fImg = image(still, f1, N, N);
        for (size_t i = 0; i < (size_t)N * N; ++i) if (hImg[i * 4 + 2] > 0.5f) hImg[i * 4 + 1] += 100.0f;
        auto own = [&](bool stillHands, int* kept, int* hand) {
            dvr::afw::note_hands_still(stillHands);
            g_signForeground = false; dvr::depthprobe::g_prefgReady = false;
            Scene sc = capture(g, still, still, Opt(), N, N, hImg, fImg);
            dvr::afw::Pose out{}; const char* why = nullptr;
            const bool ok = dvr::afw::warp_held(g.dev, g.ctx, 0, 1, sc.sf, g.dst, N, N, (float)kTan, (float)kTan, &out, &why);
            *kept = *hand = 0;
            if (ok) {
                g.ctx->CopyResource(g.stage, g.dst);
                D3D11_MAPPED_SUBRESOURCE m;
                if (SUCCEEDED(g.ctx->Map(g.stage, 0, D3D11_MAP_READ, 0, &m))) {
                    for (int y = 0; y < N; ++y)
                        for (int x = 0; x < N; ++x) {
                            const float* o = (const float*)((const uint8_t*)m.pData + y * m.RowPitch) + x * 4;
                            if (o[2] > 0.5f) { ++*hand; if (o[1] > 50.0f) ++*kept; }
                        }
                    g.ctx->Unmap(g.stage, 0);
                }
            }
            release(sc);
            dvr::afw::note_hands_still(false);
            return ok;
        };
        int k1, h1, k2, h2;
        const bool a = own(true, &k1, &h1), b = own(false, &k2, &h2);
        char d[160]; snprintf(d, sizeof(d), "still: %d of %d hand texels the held eye's own | moving (control): %d of %d", k1, h1, k2, h2);
        check("a still weapon keeps the held eye's own shading", a && b && h1 > 500 && k1 > h1 * 8 / 10 && k2 < h2 / 20, d);
    }
    printf("afw warp: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
