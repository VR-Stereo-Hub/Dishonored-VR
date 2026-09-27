// tools/motion-gpu-tests.cpp - the motion-vector calibration shader (core/gfx/motion_gpu.cpp) on a
// real D3D11 device against a synthetic room with KNOWN depth and a KNOWN camera move. Never
// launches the game. Run through tools/motion-gpu-host.ps1.
//
// The room is ray-cast on the CPU from two cameras (a step sideways and forward, a small turn).
// The depth written for the current view is in "depth units" of 100 uu, as the game's appears to
// be. Claims held to numbers:
//   - the CPU reprojection sends a current pixel to the exact previous pixel the same world point
//     was drawn at (checked against the ray caster);
//   - the GPU error curve has its minimum at the true 100 uu per unit, well below its neighbours;
//   - NEGATIVE CONTROL: rotation-only reprojection (the last candidate) is clearly worse, and with
//     the camera merely turning (no translation) every scale scores the same - the instrument
//     cannot see a scale it has no parallax for, and says so by being flat.
#include "core/gfx/motion_gpu.h"
#include "core/gfx/clarity_math.h"

#include <windows.h>
#include <d3d11.h>
#include <math.h>
#include <stdio.h>
#include <vector>

using namespace dvr::motion;
using dvr::clarity::Basis;
using dvr::clarity::basis_from_rotator;
using dvr::clarity::prev_from_cur;

static int g_failed = 0, g_checks = 0;
static void check(const char* name, bool ok, const char* detail = "") {
    ++g_checks; if (!ok) ++g_failed;
    printf("%s  %s %s\n", ok ? "ok  " : "FAIL", name, detail);
}

const int W = 320, H = 320;
const float kTan = 1.0f, kTrueScale = 100.0f;

struct Cam { Basis b; float pos[3]; };

// A room: floor z=-80, ceiling z=+90, walls y=+-160, far wall x=700 (uu). Texture: sums of sines.
static float tex(const float p[3]) {
    const float a = sinf(p[0] * 0.21f) * sinf(p[1] * 0.17f) + 0.5f * sinf(p[2] * 0.33f + p[0] * 0.05f) +
                    0.35f * sinf((p[0] + p[1] + p[2]) * 0.47f);
    return 0.5f + 0.3f * a;
}
static bool cast(const Cam& c, float u, float v, float* lum, float* fwd) {
    const float x = u * 2 - 1, y = 1 - v * 2;
    float d[3];
    for (int i = 0; i < 3; ++i) d[i] = c.b.f[i] + x * kTan * c.b.r[i] + y * kTan * c.b.u[i];
    float best = 1e30f;
    auto plane = [&](int axis, float value) {
        if (fabsf(d[axis]) < 1e-6f) return;
        const float t = (value - c.pos[axis]) / d[axis];
        if (t > 0.1f && t < best) best = t;
    };
    plane(2, -80); plane(2, 90); plane(1, -160); plane(1, 160); plane(0, 700); plane(0, -300);
    if (best > 1e29f) return false;
    const float p[3] = {c.pos[0] + d[0] * best, c.pos[1] + d[1] * best, c.pos[2] + d[2] * best};
    *lum = tex(p);
    *fwd = best;   // dir's forward component is 1: the parameter IS the forward (view z) distance
    return true;
}

struct Tex { ID3D11Texture2D* t = nullptr; ID3D11ShaderResourceView* s = nullptr; void rel() { if (s) s->Release(); if (t) t->Release(); } };
static Tex upload(ID3D11Device* dev, DXGI_FORMAT fmt, const void* data, UINT pitch) {
    D3D11_TEXTURE2D_DESC td = {}; td.Width = W; td.Height = H; td.MipLevels = 1; td.ArraySize = 1; td.Format = fmt;
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd = {data, pitch, 0};
    Tex x; dev->CreateTexture2D(&td, &sd, &x.t); if (x.t) dev->CreateShaderResourceView(x.t, nullptr, &x.s);
    return x;
}

static void render(const Cam& c, std::vector<uint8_t>& rgba, std::vector<float>* depth) {
    rgba.assign(W * H * 4, 0);
    if (depth) depth->assign(W * H * 4, 0.0f);
    for (int j = 0; j < H; ++j)
        for (int i = 0; i < W; ++i) {
            float l = 0, f = 0;
            if (!cast(c, (i + 0.5f) / W, (j + 0.5f) / H, &l, &f)) continue;
            const uint8_t b = (uint8_t)(fminf(fmaxf(l, 0), 1) * 255 + 0.5f);
            uint8_t* px = &rgba[(j * W + i) * 4]; px[0] = px[1] = px[2] = b; px[3] = 255;
            if (depth) (*depth)[(j * W + i) * 4 + 3] = f / kTrueScale;
        }
}

static bool curve(ID3D11Device* dev, ID3D11DeviceContext* ctx, CalibGpu& g, const Cam& prev, const Cam& cur, float err[kScales], int* n) {
    std::vector<uint8_t> cp, pp; std::vector<float> cd;
    render(cur, cp, &cd); render(prev, pp, nullptr);
    Tex tc = upload(dev, DXGI_FORMAT_R8G8B8A8_UNORM, cp.data(), W * 4);
    Tex tp = upload(dev, DXGI_FORMAT_R8G8B8A8_UNORM, pp.data(), W * 4);
    Tex td = upload(dev, DXGI_FORMAT_R32G32B32A32_FLOAT, cd.data(), W * 16);
    CalibParams p;
    p.prevFromCur = prev_from_cur(prev.b, cur.b);
    const float dp[3] = {cur.pos[0] - prev.pos[0], cur.pos[1] - prev.pos[1], cur.pos[2] - prev.pos[2]};
    p.t[0] = dvr::clarity::dot3(prev.b.f, dp); p.t[1] = dvr::clarity::dot3(prev.b.r, dp); p.t[2] = dvr::clarity::dot3(prev.b.u, dp);
    p.tanH = kTan; p.tanV = kTan; p.w = W; p.h = H; p.farDepth = 1000.0f;
    char why[256] = "";
    const bool ok = g.run(dev, ctx, tc.s, tp.s, td.s, p, err, n, why, sizeof(why));
    if (!ok) printf("     run refused: %s\n", why);
    tc.rel(); tp.rel(); td.rel();
    return ok;
}

int main() {
    // ---- the CPU reprojection against the ray caster ---------------------------------
    Cam a{basis_from_rotator(0, 0, 0), {0, 0, 0}};
    Cam b{basis_from_rotator(1.0f, 2.0f, 0), {6.0f, 14.0f, 0}};   // forward 6 uu, sideways 14 uu, a small turn
    {
        float worst = 0; int n = 0;
        const dvr::clarity::Mat3 m = prev_from_cur(a.b, b.b);
        const float dp[3] = {b.pos[0] - a.pos[0], b.pos[1] - a.pos[1], b.pos[2] - a.pos[2]};
        const float t[3] = {dvr::clarity::dot3(a.b.f, dp), dvr::clarity::dot3(a.b.r, dp), dvr::clarity::dot3(a.b.u, dp)};
        for (int j = 8; j < H; j += 16)
            for (int i = 8; i < W; i += 16) {
                const float u = (i + 0.5f) / W, v = (j + 0.5f) / H;
                float l, f, pu, pv;
                if (!cast(b, u, v, &l, &f) || !reproject_depth(m, t, kTan, kTan, u, v, f, &pu, &pv)) continue;
                if (pu < 0 || pu > 1 || pv < 0 || pv > 1) continue;
                float l2, f2;
                if (!cast(a, pu, pv, &l2, &f2)) continue;
                worst = fmaxf(worst, fabsf(l2 - l)); ++n;
            }
        char d[96]; _snprintf_s(d, sizeof(d), _TRUNCATE, "(%d points, worst luminance mismatch %.5f)", n, worst);
        check("cpu: a reprojected pixel lands where the previous camera saw the same point", n > 100 && worst < 1e-3f, d);
    }
    // ---- the GPU instrument ------------------------------------------------------------
    ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr;
    D3D_FEATURE_LEVEL fl[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fl, 2, D3D11_SDK_VERSION, &dev, nullptr, &ctx)) &&
        FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, fl, 2, D3D11_SDK_VERSION, &dev, nullptr, &ctx))) {
        printf("FAIL  no D3D11 device\n"); return 1;
    }
    CalibGpu g; char why[512] = "";
    check("init: the calibration shader compiles", g.init(dev, why, sizeof(why)), why);
    float err[kScales]; int n = 0;
    if (curve(dev, ctx, g, a, b, err, &n)) {
        int best = 0;
        for (int k = 1; k < kScales; ++k) if (err[k] >= 0 && (err[best] < 0 || err[k] < err[best])) best = k;
        char d[400]; int m = _snprintf_s(d, sizeof(d), _TRUNCATE, "(%d samples;", n);
        for (int k = 0; k < kScales; ++k) m += _snprintf_s(d + m, sizeof(d) - m, _TRUNCATE, " %g:%.4f", kScaleValues[k], err[k]);
        _snprintf_s(d + m, sizeof(d) - m, _TRUNCATE, ")");
        check("gpu: the error curve's minimum is the true 100 uu per unit", kScaleValues[best] == kTrueScale, d);
        check("gpu: the neighbours of the minimum are clearly worse (a sharp curve)",
              err[best] * 1.5f < err[best - 1] && err[best] * 1.5f < err[best + 1], d);
        check("gpu NEGATIVE CONTROL: rotation-only reprojection is far worse than the true scale",
              err[kScales - 1] > err[best] * 4.0f, d);
    }
    Cam turn{basis_from_rotator(0.5f, 3.0f, 0), {0, 0, 0}};   // the same place, only a turn
    if (curve(dev, ctx, g, a, turn, err, &n)) {
        float lo = 1e9f, hi = 0;
        for (int k = 0; k < kScales; ++k) if (err[k] >= 0) { lo = fminf(lo, err[k]); hi = fmaxf(hi, err[k]); }
        char d[96]; _snprintf_s(d, sizeof(d), _TRUNCATE, "(errors %.4f..%.4f)", lo, hi);
        check("gpu: with no translation every scale scores the same (no parallax, no verdict)", hi - lo < 0.002f, d);
    }
    printf("motion-gpu: %d checks, %d failures\n", g_checks, g_failed);
    g.shutdown(); ctx->Release(); dev->Release();
    return g_failed ? 1 : 0;
}
