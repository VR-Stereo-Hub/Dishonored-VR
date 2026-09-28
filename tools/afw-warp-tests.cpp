// Host tests for AFW's held-eye rebuild (VR-39): the production shader and draw in
// src/core/gfx/afw_warp.cpp against a ray-traced synthetic scene. Never touches the game.
// Build and run: tools\afw-warp-host.ps1
//
// The scene: a far wall and a near pillar fixed in the WORLD (they turn and slide against the tracking
// space when the body yaws or walks) and a "hand" quad fixed in TRACKING space (it moves only when the
// controller does). Each image also carries the game-style camera-relative view-projection and c5 it
// was "drawn" with, so the held eye's world can move by matrices as it does in the game. Every image is
// ray-traced from its eye pose at its instant, and every pixel's colour names the surface point it
// shows (red/green = the point's coordinates on that surface, blue = 1 on the hand), with the linear
// view depth in alpha as the game's scene target carries it. The held eye's image is traced at the
// OLD instant, the fresh eye's at the NEW one, and the truth is the held eye traced at the NEW instant.
// Each case compares the rebuilt eye against that truth pixel by pixel:
//   ghost    the rebuild shows the hand where the truth does not (the reported fault)
//   missing  the truth shows the hand where the rebuild does not
//   error    on agreeing pixels, how far the shown point is from the true one, in target pixels
// The negative controls run the same motion with the fresh-eye source off (the first version) and
// must FAIL the ghost bound, so the instrument can see the fault it claims to fix.
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
static ID3D11ShaderResourceView* g_depthBySerial[8] = {};
static UINT g_depthN = 0;
namespace dvr::clarity { float depth_scale() { return 250.0f; } }
namespace dvr::depthprobe {
ID3D11ShaderResourceView* depth_srv_for(uint32_t serial, UINT* w, UINT* h) {
    if (serial >= 8 || !g_depthBySerial[serial]) return nullptr;
    if (w) *w = g_depthN; if (h) *h = g_depthN; return g_depthBySerial[serial];
}
void read_done(ID3D11DeviceContext*) {}
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

// tracking = Ry(body)(world - bodyPos): the body turns by bodyYawDeg and stands at bodyPos (world).
struct State { double bodyYawDeg, handX, headYawDeg; V3 headPos; double handZ = kHandZ; V3 bodyPos = {0, 0, 0}; };
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
    if (d.z < 0) {   // the hand, fixed in tracking space
        const double t = (s.handZ - e.pos.z) / d.z;
        const V3 p = add(e.pos, mul(d, t));
        const double lu = (p.x - (s.handX - kHandW / 2)) / kHandW, lv = (p.y - (kHandY - kHandH / 2)) / kHandH;
        if (t > 0 && lu >= 0 && lu <= 1 && lv >= 0 && lv <= 1) { best = t; c[0] = (float)lu; c[1] = (float)lv; c[2] = 1; }
    }
    {   // the wall and the pillar, fixed in the world
        const double b = s.bodyYawDeg / 57.29577951;
        const V3 ow = add(ry(e.pos, -b), s.bodyPos), dw = ry(d, -b);
        if (dw.z < 0) {
            const double t = (kWallZ - ow.z) / dw.z;
            const V3 p = add(ow, mul(dw, t));
            if (t > 0 && t < best) { best = t; c[0] = (float)((p.x + 20) / 40); c[1] = (float)((p.y + 20) / 40); c[2] = 0; }
            const double tp = (kPillarZ - ow.z) / dw.z;
            const V3 q = add(ow, mul(dw, tp));
            if (tp > 0 && tp < best && q.x >= kPillarX0 && q.x <= kPillarX1 && q.y >= -1.5 && q.y <= 1.5) {
                best = tp; c[0] = (float)((q.x - kPillarX0) / (kPillarX1 - kPillarX0)); c[1] = (float)(2 + (q.y + 1.5) / 3); c[2] = 0;
            }
        }
    }
    const V3 local = ry(mul(d, best), -e.yawRad);   // back into the eye's view: depth = -z
    c[3] = (float)(-local.z / kMPerUnit);
    memcpy(out, c, sizeof(c));
}
// The game-style matrix: row-vector, camera-relative, uu; clip x = right.P / tan, y = up.P / tan, w = forward.P.
struct Mtx { float vp[16]; float c5[3]; };
static Mtx matrixOf(const State& s, const Eye& e, bool broken) {
    const double b = s.bodyYawDeg / 57.29577951;
    const V3 right = ry(ry({1, 0, 0}, e.yawRad), -b), up = ry(ry({0, 1, 0}, e.yawRad), -b), fwd = ry(ry({0, 0, -1}, e.yawRad), -b);
    const V3 C = mul(add(ry(e.pos, -b), s.bodyPos), kScale);
    Mtx m = {};
    const double sx = broken ? -1.0 : 1.0;   // a mirrored axis: the convention check must refuse it
    const double r3[3] = {right.x, right.y, right.z}, u3[3] = {up.x, up.y, up.z}, f3[3] = {fwd.x, fwd.y, fwd.z};
    for (int j = 0; j < 3; ++j) { m.vp[j * 4 + 0] = (float)(sx * r3[j] / kTan); m.vp[j * 4 + 1] = (float)(u3[j] / kTan); m.vp[j * 4 + 3] = (float)f3[j]; }
    m.c5[0] = (float)-C.x; m.c5[1] = (float)-C.y; m.c5[2] = (float)-C.z;
    return m;
}
static std::vector<float> image(const State& s, const Eye& e) {
    std::vector<float> px(N * N * 4);
    for (int y = 0; y < N; ++y)
        for (int x = 0; x < N; ++x) trace(s, e, (x + 0.5) / N, (y + 0.5) / N, &px[(y * N + x) * 4]);
    return px;
}

// ---- the GPU side -------------------------------------------------------------------------------
struct Gpu { ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr; ID3D11Texture2D* dst = nullptr; ID3D11Texture2D* stage = nullptr; };
static ID3D11Texture2D* tex(ID3D11Device* dev, UINT bind, D3D11_USAGE use, UINT cpu, const void* init) {
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = N; td.MipLevels = td.ArraySize = 1; td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    td.SampleDesc.Count = 1; td.Usage = use; td.BindFlags = bind; td.CPUAccessFlags = cpu;
    D3D11_SUBRESOURCE_DATA sd = {init, N * 16, 0};
    ID3D11Texture2D* t = nullptr;
    dev->CreateTexture2D(&td, init ? &sd : nullptr, &t);
    return t;
}

struct Result { bool ok; int handTruth, ghost, missing, agree, wrong, unseen; double errP50, errP95, errMax; };
struct Opt { bool stereo = true, heldDepth = true, freshDepth = true, matrices = true, broken = false; };

static Result run(Gpu& g, const State& s0, const State& s1, Opt o = Opt()) {
    const bool stereo = o.stereo, heldDepth = o.heldDepth, freshDepth = o.freshDepth;
    Result r = {};
    const Eye held0 = eyeOf(s0, 0), right0 = eyeOf(s0, 1), held1 = eyeOf(s1, 0), fresh1 = eyeOf(s1, 1);
    auto hImg = image(s0, held0), fImg = image(s1, fresh1), truth = image(s1, held1);
    ID3D11Texture2D* ht = tex(g.dev, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, hImg.data());
    ID3D11Texture2D* ft = tex(g.dev, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, fImg.data());
    ID3D11ShaderResourceView *hs = nullptr, *fs = nullptr;
    g.dev->CreateShaderResourceView(ht, nullptr, &hs);
    g.dev->CreateShaderResourceView(ft, nullptr, &fs);
    for (auto*& p : g_depthBySerial) p = nullptr;
    static uint32_t serial = 0;
    const uint32_t sh = (serial = (serial + 2) % 6) + 1, sf = sh + 1;
    if (heldDepth) g_depthBySerial[sh] = hs;
    if (freshDepth) g_depthBySerial[sf] = fs;
    g_depthN = N;
    dvr::afw::set_enabled(true, "test");
    dvr::afw::set_stereo(stereo, "test");
    dvr::afw::set_body_depth(0.40f, "test");
    dvr::afw::set_world_scale((float)kScale);
    dvr::afw::set_matrices(o.matrices, "test");
    const Mtx mh = matrixOf(s0, held0, o.broken), mf = matrixOf(s1, fresh1, o.broken);
    const dvr::afw::Pose tg0[2] = {poseOf(held0), poseOf(right0)}, tg1[2] = {poseOf(held1), poseOf(fresh1)};
    dvr::afw::note_capture(g.dev, g.ctx, 0, ht, sh, poseOf(held0), true, (float)s0.bodyYawDeg, tg0, mh.vp, mh.c5);   // last present's image
    dvr::afw::note_capture(g.dev, g.ctx, 1, ft, sf, poseOf(fresh1), true, (float)s1.bodyYawDeg, tg1, mf.vp, mf.c5);  // this present's
    dvr::afw::Pose out{};
    const char* why = nullptr;
    r.ok = dvr::afw::warp_held(g.dev, g.ctx, 0, 1, g.dst, N, N, (float)kTan, (float)kTan, &out, &why);
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
    hs->Release(); fs->Release(); ht->Release(); ft->Release();
    if (!r.ok) return r;
    // Compare, skipping the 2-pixel band around every true outline (sampling there is a blend).
    std::vector<double> errs;
    auto surf = [&](const std::vector<float>& p, int x, int y) {   // 2 hand, 1 pillar, 0 wall
        const float* q = &p[(y * N + x) * 4]; return q[2] > 0.5f ? 2 : q[1] >= 1.5f ? 1 : 0;
    };
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
            const double sx = ts == 2 ? kHandW / (-s1.handZ) * k : ts == 1 ? (kPillarX1 - kPillarX0) / -kPillarZ * k : 40.0 / 8.0 * k;
            const double sy = ts == 2 ? kHandH / (-s1.handZ) * k : ts == 1 ? 3.0 / -kPillarZ * k : sx;
            if (th) ++r.handTruth;
            if (oh && !th) { ++r.ghost; continue; }
            if (!oh && th) { ++r.missing; continue; }
            if (!th) {   // a world point neither source image shows cannot be rebuilt: counted apart, scored only for a ghost
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
                    if ((q[2] > 0.5f ? 2 : q[1] >= 1.5f ? 1 : 0) != ts) return false;
                    return hypot((q[0] - t[0]) * sx, (q[1] - t[1]) * sy) < 3.0;
                };
                if (!seen(hImg, s0, held0) && !seen(fImg, s1, fresh1)) { ++r.unseen; continue; }
            }
            if (ov[2] > 0.02f && ov[2] < 0.98f) continue;                 // a blend across the hand outline
            if (!th && ov[1] > 1.1f && ov[1] < 1.9f) continue;           // a blend across the pillar outline
            if (!th && (ov[1] >= 1.5f ? 1 : 0) != ts) { ++r.wrong; continue; }
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
static void report(const char* name, const Result& r, bool ok) {
    printf("%-58s %s  hand %5d ghost %4d (%.2f%%) missing %4d (%.2f%%) wrong world %4d (unseen %4d) | err p50 %.2f p95 %.2f px\n",
           name, ok ? "PASS" : "FAIL", r.handTruth, r.ghost, r.handTruth ? 100.0 * r.ghost / r.handTruth : 0.0, r.missing,
           r.handTruth ? 100.0 * r.missing / r.handTruth : 0.0, r.wrong, r.unseen, r.errP50, r.errP95);
    ok ? ++g_pass : ++g_fail;
}
// The bounds: ghost and missing under 1% of the hand, the wrong world surface under 0.1% of the image,
// the median error under a pixel and 95% under two.
static bool clean(const Result& r) {
    return r.ok && r.handTruth > 500 && r.ghost < r.handTruth / 100 && r.missing < r.handTruth / 100 &&
           r.wrong < N * N / 1000 && r.errP50 < 1.0 && r.errP95 < 2.0;
}

int main() {
    Gpu g;
    const D3D_FEATURE_LEVEL fl[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fl, 2, D3D11_SDK_VERSION, &g.dev, nullptr, &g.ctx)) &&
        FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, fl, 2, D3D11_SDK_VERSION, &g.dev, nullptr, &g.ctx))) {
        printf("no D3D11 device\n"); return 2;
    }
    g.dst = tex(g.dev, D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, nullptr);
    g.stage = tex(g.dev, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ, nullptr);
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
    Opt noStereo; noStereo.stereo = false;
    Opt noMtx; noMtx.matrices = false;
    Opt broken; broken.broken = true;

    { Result r = run(g, still, still);   report("still: the other eye's stereo only", r, clean(r)); }
    { Result r = run(g, still, turn);    report("stick turn 5 deg: no hand ghost, world turned", r, clean(r)); }
    { Result r = run(g, still, moved);   report("hand moved 6 cm: the hand where it is NOW", r, clean(r)); }
    { Result r = run(g, still, head);    report("head turn 3 deg + 1.5 cm: no drift", r, clean(r)); }
    { Result r = run(g, still, all);     report("turn + hand move + head turn together", r, clean(r)); }
    { Result r = run(g, still, fast);    report("fast turn 15 deg in one tick", r, clean(r)); }
    { Result r = run(g, close0, close1); report("weapon close (0.2 m), turn + move", r, clean(r)); }
    { Result r = run(g, still, walk);           report("walking one tick: the pillar's parallax carried", r, clean(r)); }
    { Result r = run(g, still, walkTurn);       report("walking + turning + head turn", r, clean(r)); }
    { Result r = run(g, still, walk, noMtx);
      report("control: no matrices, walking -> the pillar lags", r, r.ok && r.errP95 > 3.0); }
    { Result r = run(g, still, turn, broken);
      report("a mirrored matrix is refused: the XR model turns it", r, clean(r)); }
    // NEGATIVE CONTROLS: the first version (held eye alone) under the same motion must show the fault.
    { Result r = run(g, still, moved, noStereo);
      report("control: held eye alone, hand moved -> ghost", r, r.ok && r.ghost > r.handTruth / 5); }
    { Result r = run(g, still, all, noStereo);
      report("control: held eye alone, everything -> ghost", r, r.ok && r.ghost > r.handTruth / 10); }
    // Fallbacks: one depth missing still rebuilds.
    // The temporal-only rebuild (the A/B and the no-fresh-depth fallback) keeps the hand and turns the
    // world, but cannot see behind the old hand: its trailing copy is measured, not hidden.
    { Result r = run(g, still, turn, noStereo);
      report("held eye alone, turn: hand kept, world turned (trail known)", r,
             r.ok && r.missing < r.handTruth / 100 && r.errP50 < 1.0 && r.ghost < r.handTruth * 3 / 10); }
    { Result r = run(g, still, moved, [] { Opt o; o.heldDepth = false; return o; }());
      report("no held depth: the fresh eye alone, hand moved", r, r.ok && r.ghost < r.handTruth / 100 && r.missing < r.handTruth / 50); }
    { Result r = run(g, still, turn, [] { Opt o; o.freshDepth = false; return o; }());
      report("no fresh depth: the temporal split, turn (trail known)", r,
             r.ok && r.missing < r.handTruth / 100 && r.errP50 < 1.0 && r.ghost < r.handTruth * 3 / 10); }
    printf("afw warp: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
