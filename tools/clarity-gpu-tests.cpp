// tools/clarity-gpu-tests.cpp - the production clarity shaders (core/gfx/clarity_gpu.cpp)
// on a real D3D11 device against synthetic images with known answers. Never launches
// the game. Run through tools/clarity-gpu-host.ps1.
//
// Each claim the clarity passes make is held to a number here, with a negative control
// where one exists:
//   identity      nothing enabled but the final pass = the input, to within one step
//   resolve       a constant stays constant, the mean survives, and a pattern at the
//                 render's own pixel pitch resolves to an even grey. THE CONTROL: one
//                 bilinear tap per output pixel (what a compositor does when handed a
//                 larger image) leaves most of that pattern standing, and it crawls
//                 when the pattern moves by a quarter pixel
//   temporal      a still view converges to itself (no drift, no blur); a slanted edge
//                 point-sampled under small head rotations ends much nearer the true
//                 pixel coverage than any single raw frame; a changed scene is not
//                 smeared (the history is clipped to the current neighbourhood)
//   sharpen       flat areas are untouched, edge contrast rises, nothing leaves 0..1
//   the math      the rotation basis is orthonormal; a yaw of one pixel's angle moves
//                 the reprojected centre by one pixel
#include "core/gfx/clarity_gpu.h"
#include "core/gfx/clarity_math.h"

#include <windows.h>
#include <d3d11.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

using namespace dvr::clarity;

static int g_failed = 0, g_checks = 0;
static void check(const char* name, bool ok, const char* detail = "") {
    ++g_checks;
    if (!ok) ++g_failed;
    printf("%s  %s %s\n", ok ? "ok  " : "FAIL", name, detail);
}

static float to_linear(float c) { return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f); }
static float to_gamma(float c) {
    c = c < 0 ? 0 : (c > 1 ? 1 : c);
    return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1.0f / 2.4f) - 0.055f;
}

struct Dev {
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    const char* kind = "";
};

static bool make_device(Dev& d) {
    D3D_FEATURE_LEVEL fl[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    if (SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fl, 3, D3D11_SDK_VERSION,
                                    &d.dev, nullptr, &d.ctx))) { d.kind = "hardware"; return true; }
    if (SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, fl, 3, D3D11_SDK_VERSION,
                                    &d.dev, nullptr, &d.ctx))) { d.kind = "WARP"; return true; }
    return false;
}

// An 8-bit image, stored as gamma-encoded bytes like the game's capture (BGRA).
struct Img {
    uint32_t w = 0, h = 0;
    std::vector<float> g;   // gamma-encoded grey, 0..1 (all three channels equal)
    float& at(uint32_t x, uint32_t y) { return g[(size_t)y * w + x]; }
    float at(uint32_t x, uint32_t y) const { return g[(size_t)y * w + x]; }
};

static Img make(uint32_t w, uint32_t h, float v = 0) { Img i; i.w = w; i.h = h; i.g.assign((size_t)w * h, v); return i; }

struct Src {
    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    void release() { if (srv) srv->Release(); if (tex) tex->Release(); srv = nullptr; tex = nullptr; }
};

static Src upload(Dev& d, const Img& img) {
    std::vector<uint8_t> px((size_t)img.w * img.h * 4);
    for (size_t i = 0; i < img.g.size(); ++i) {
        float v = img.g[i]; v = v < 0 ? 0 : (v > 1 ? 1 : v);
        const uint8_t b = (uint8_t)(v * 255.0f + 0.5f);
        px[i * 4 + 0] = b; px[i * 4 + 1] = b; px[i * 4 + 2] = b; px[i * 4 + 3] = 255;
    }
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = img.w; td.Height = img.h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;   // the capture's format
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA sd = {px.data(), img.w * 4, 0};
    Src s;
    d.dev->CreateTexture2D(&td, &sd, &s.tex);
    if (s.tex) d.dev->CreateShaderResourceView(s.tex, nullptr, &s.srv);
    return s;
}

struct Dst {
    ID3D11Texture2D* tex = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    ID3D11Texture2D* staging = nullptr;
    uint32_t w = 0, h = 0;
    void release() {
        if (rtv) rtv->Release(); if (tex) tex->Release(); if (staging) staging->Release();
        rtv = nullptr; tex = nullptr; staging = nullptr;
    }
};

static Dst make_dst(Dev& d, uint32_t w, uint32_t h) {
    Dst o; o.w = w; o.h = h;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;   // the method's eye texture
    td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    d.dev->CreateTexture2D(&td, nullptr, &o.tex);
    d.dev->CreateRenderTargetView(o.tex, nullptr, &o.rtv);
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    d.dev->CreateTexture2D(&td, nullptr, &o.staging);
    return o;
}

static Img read(Dev& d, Dst& o) {
    d.ctx->CopyResource(o.staging, o.tex);
    D3D11_MAPPED_SUBRESOURCE m = {};
    Img img = make(o.w, o.h);
    if (SUCCEEDED(d.ctx->Map(o.staging, 0, D3D11_MAP_READ, 0, &m))) {
        for (uint32_t y = 0; y < o.h; ++y) {
            const uint8_t* row = (const uint8_t*)m.pData + (size_t)y * m.RowPitch;
            for (uint32_t x = 0; x < o.w; ++x) img.at(x, y) = row[x * 4 + 1] / 255.0f;   // green
        }
        d.ctx->Unmap(o.staging, 0);
    }
    return img;
}

static bool run(Dev& d, Gpu& gpu, const Img& in, Dst& out, PassParams p) {
    Src s = upload(d, in);
    char why[256] = "";
    p.w = in.w; p.h = in.h; p.ow = out.w; p.oh = out.h;
    const bool ok = gpu.run(d.dev, d.ctx, s.srv, out.rtv, p, why, sizeof(why));
    if (!ok) printf("     run refused: %s\n", why);
    s.release();
    return ok;
}

// ---- the compositor model: ONE bilinear tap per output pixel at its centre ------
static float bilinear_tap(const Img& src, float sxPos, float syPos) {   // positions in source pixels
    const float fx = sxPos - 0.5f, fy = syPos - 0.5f;
    int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
    const float ax = fx - x0, ay = fy - y0;
    auto L = [&](int x, int y) {
        x = x < 0 ? 0 : (x >= (int)src.w ? (int)src.w - 1 : x);
        y = y < 0 ? 0 : (y >= (int)src.h ? (int)src.h - 1 : y);
        return to_linear(src.at((uint32_t)x, (uint32_t)y));
    };
    const float v = (L(x0, y0) * (1 - ax) + L(x0 + 1, y0) * ax) * (1 - ay) +
                    (L(x0, y0 + 1) * (1 - ax) + L(x0 + 1, y0 + 1) * ax) * ay;
    return v;   // linear
}

static void stats(const std::vector<float>& v, float* mean, float* sd) {
    double s = 0, s2 = 0;
    for (float x : v) { s += x; s2 += (double)x * x; }
    const double m = s / v.size();
    *mean = (float)m;
    *sd = (float)sqrt(v.size() > 1 ? (s2 / v.size() - m * m > 0 ? s2 / v.size() - m * m : 0) : 0);
}

// ---- a world-fixed slanted edge seen through a rotating camera -------------------
// White where the world direction's azimuth exceeds `k * elevation + a0`. Point-
// sampled at pixel centres: the hard, aliased image a renderer produces. `cover`
// integrates the same test over the pixel (16 x 16 subsamples): the ground truth.
struct EdgeScene { float a0 = 0.004f, k = 0.35f, tanH = 0.8f, tanV = 0.8f; };

static float edge_value(const EdgeScene& sc, const Basis& b, float px, float py, uint32_t w, uint32_t h) {
    const float x = px / w * 2.0f - 1.0f, y = 1.0f - py / h * 2.0f;
    float d[3];
    for (int i = 0; i < 3; ++i) d[i] = b.f[i] + x * sc.tanH * b.r[i] + y * sc.tanV * b.u[i];
    const float az = atan2f(d[1], d[0]);
    const float el = atan2f(d[2], sqrtf(d[0] * d[0] + d[1] * d[1]));
    return az > sc.k * el + sc.a0 ? 1.0f : 0.0f;
}

static Img render_edge(const EdgeScene& sc, const Basis& b, uint32_t w, uint32_t h, int ss) {
    Img img = make(w, h);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            float acc = 0;
            for (int j = 0; j < ss; ++j)
                for (int i = 0; i < ss; ++i)
                    acc += edge_value(sc, b, x + (i + 0.5f) / ss, y + (j + 0.5f) / ss, w, h);
            img.at(x, y) = to_gamma(acc / (ss * ss));
        }
    return img;
}

static float edge_error(const Img& a, const Img& truth) {   // mean |linear difference| over edge pixels
    double e = 0; int n = 0;
    for (uint32_t y = 2; y + 2 < a.h; ++y)
        for (uint32_t x = 2; x + 2 < a.w; ++x) {
            const float t = to_linear(truth.at(x, y));
            if (t <= 0.001f || t >= 0.999f) continue;   // pixels the edge crosses
            e += fabsf(to_linear(a.at(x, y)) - t); ++n;
        }
    return n ? (float)(e / n) : 0.0f;
}

int main() {
    // ---- the math, no device -----------------------------------------------------
    {
        const Basis b = basis_from_rotator(12.0f, -37.0f, 8.0f);
        const float ff = dot3(b.f, b.f), rr = dot3(b.r, b.r), uu = dot3(b.u, b.u);
        const float fr = dot3(b.f, b.r), fu = dot3(b.f, b.u), ru = dot3(b.r, b.u);
        check("math: rotator basis is orthonormal",
              fabsf(ff - 1) < 1e-5f && fabsf(rr - 1) < 1e-5f && fabsf(uu - 1) < 1e-5f &&
              fabsf(fr) < 1e-5f && fabsf(fu) < 1e-5f && fabsf(ru) < 1e-5f);
        const Basis y90 = basis_from_rotator(0, 90, 0);
        check("math: yaw +90 faces +Y (right-handed turn to the right in UE)", fabsf(y90.f[1] - 1) < 1e-5f);
        const Mat3 id = prev_from_cur(b, b);
        bool eye = true;
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) eye &= fabsf(id.m[i][j] - (i == j ? 1.0f : 0.0f)) < 1e-5f;
        check("math: the same camera twice reprojects to itself", eye);
        // One pixel of yaw at the centre of a 1000-px image with tanH = 1.
        const float tanH = 1.0f, px = 1000.0f;
        const float onePx = atanf(2.0f * tanH / px) * 57.2957795f;
        const Mat3 m = prev_from_cur(basis_from_rotator(0, 0, 0), basis_from_rotator(0, onePx, 0));
        float pu = 0, pv = 0;
        reproject_uv(m, tanH, tanH, 0.5f, 0.5f, &pu, &pv);
        char t[96]; _snprintf_s(t, sizeof(t), _TRUNCATE, "(moved %.4f px)", (pu - 0.5f) * px);
        check("math: a yaw of one pixel's angle moves the centre one pixel right", fabsf((pu - 0.5f) * px - 1.0f) < 0.01f, t);
        uint32_t ow, oh;
        check("math: 100% render (2750x2850) is not resolved against 2688x2880",
              !resolve_size(2750, 2850, 2688, 2880, &ow, &oh));
        const bool r300 = resolve_size(4763, 4936, 2688, 2880, &ow, &oh);
        _snprintf_s(t, sizeof(t), _TRUNCATE, "(-> %ux%u)", ow, oh);
        check("math: 300% (4763x4936) resolves to the recommended pixel count", r300 && ow > 2700 && ow < 2760, t);
        View v0, v1;
        v0.ok = v1.ok = true; v0.w = v1.w = 100; v0.h = v1.h = 100; v0.tanH = v1.tanH = 1; v0.tanV = v1.tanV = 1;
        v1.yaw = 3.0f;
        check("history: a 3 degree turn keeps", keep_history(v0, v1) == Reset::None);
        v1.yaw = 35.0f;
        check("history: a 35 degree snap turn resets", keep_history(v0, v1) == Reset::Turn);
        v1.yaw = 0; v0.posOk = v1.posOk = true; v1.pos[0] = 300.0f;
        check("history: a Blink-sized jump resets", keep_history(v0, v1) == Reset::Move);
    }

    Dev d;
    if (!make_device(d)) { printf("FAIL  no D3D11 device\n"); return 1; }
    printf("device: %s\n", d.kind);
    Gpu gpu;
    char why[512] = "";
    if (!gpu.init(d.dev, why, sizeof(why))) { printf("FAIL  init: %s\n", why); return 1; }
    check("init: the production shaders compile (vs_4_0/ps_4_0) and the states create", true);

    // ---- identity ----------------------------------------------------------------
    {
        Img in = make(96, 80);
        srand(7);
        for (float& v : in.g) v = (float)(rand() % 256) / 255.0f;
        Dst out = make_dst(d, 96, 80);
        PassParams p;
        run(d, gpu, in, out, p);
        const Img o = read(d, out);
        float worst = 0;
        for (size_t i = 0; i < in.g.size(); ++i) worst = fmaxf(worst, fabsf(o.g[i] - in.g[i]));
        char t[64]; _snprintf_s(t, sizeof(t), _TRUNCATE, "(worst %.1f steps)", worst * 255.0f);
        check("identity: the final pass alone returns the input", worst <= 1.01f / 255.0f, t);
        out.release();
    }

    // ---- resolve -----------------------------------------------------------------
    {
        const uint32_t W = 519, H = 519, OW = 300, OH = 300;   // a 1.73x step, as 300% -> 100%
        Img flat = make(W, H, 0.5f);
        Dst out = make_dst(d, OW, OH);
        PassParams p; p.resolve = true;
        run(d, gpu, flat, out, p);
        Img o = read(d, out);
        float lo = 1, hi = 0;
        for (float v : o.g) { lo = fminf(lo, v); hi = fmaxf(hi, v); }
        check("resolve: a constant image stays constant", hi - lo <= 1.01f / 255.0f);

        // Stripes at the render's own pixel pitch: the finest detail a render has, and
        // exactly what supersampling exists to average.
        Img stripes = make(W, H);
        for (uint32_t y = 0; y < H; ++y) for (uint32_t x = 0; x < W; ++x) stripes.at(x, y) = (x & 1) ? 1.0f : 0.0f;
        run(d, gpu, stripes, out, p);
        o = read(d, out);
        std::vector<float> ours, control;
        for (uint32_t y = 4; y + 4 < OH; ++y)
            for (uint32_t x = 4; x + 4 < OW; ++x) {
                ours.push_back(to_linear(o.at(x, y)));
                control.push_back(bilinear_tap(stripes, (x + 0.5f) * W / OW, (y + 0.5f) * H / OH));
            }
        float mo, so, mc, sc;
        stats(ours, &mo, &so);
        stats(control, &mc, &sc);
        char t[160];
        _snprintf_s(t, sizeof(t), _TRUNCATE, "(Mitchell: mean %.3f sd %.3f | one bilinear tap: mean %.3f sd %.3f)", mo, so, mc, sc);
        check("resolve: stripes at the render pitch average to grey (mean 0.5 linear)", fabsf(mo - 0.5f) < 0.02f, t);
        check("resolve NEGATIVE CONTROL: one bilinear tap per pixel leaves the pattern standing", sc > 0.15f, t);
        check("resolve: ours leaves under a sixth of the control's pattern", so < sc / 6.0f, t);

        // The same stripes moved by a quarter of a render pixel per frame (a slow head
        // turn): how much one output pixel changes frame to frame is the crawl.
        float crawlOurs = 0, crawlCtl = 0;
        std::vector<float> prevO, prevC;
        for (int f = 0; f < 8; ++f) {
            const float shift = f * 0.25f;
            Img moving = make(W, H);
            for (uint32_t y = 0; y < H; ++y)
                for (uint32_t x = 0; x < W; ++x) {
                    // area-sampled stripes shifted by `shift` render pixels
                    const float a = x - shift, b = a + 1.0f;
                    auto whiteIn = [](float lo2, float hi2) {   // white on [2k+1, 2k+2)
                        float s = 0;
                        for (int k = (int)floorf(lo2) - 2; k <= (int)ceilf(hi2) + 2; ++k)
                            if (k & 1) s += fmaxf(0.0f, fminf(hi2, (float)k + 1) - fmaxf(lo2, (float)k));
                        return s;
                    };
                    moving.at(x, y) = to_gamma(whiteIn(a, b));
                }
            run(d, gpu, moving, out, p);
            o = read(d, out);
            std::vector<float> co, cc;
            for (uint32_t x = 20; x < 60; ++x) {
                co.push_back(to_linear(o.at(x, OH / 2)));
                cc.push_back(bilinear_tap(moving, (x + 0.5f) * W / OW, (OH / 2 + 0.5f) * H / OH));
            }
            if (f) for (size_t i = 0; i < co.size(); ++i) {
                crawlOurs = fmaxf(crawlOurs, fabsf(co[i] - prevO[i]));
                crawlCtl = fmaxf(crawlCtl, fabsf(cc[i] - prevC[i]));
            }
            prevO = co; prevC = cc;
        }
        _snprintf_s(t, sizeof(t), _TRUNCATE, "(worst frame-to-frame change: ours %.3f, one bilinear tap %.3f, linear)", crawlOurs, crawlCtl);
        check("resolve NEGATIVE CONTROL: the bilinear tap crawls as the pattern moves", crawlCtl > 0.2f, t);
        check("resolve: ours crawls under a quarter as much", crawlOurs < crawlCtl / 4.0f, t);

        // The mean of an arbitrary image survives the resolve (energy is not lost).
        Img noise = make(W, H);
        srand(3);
        double srcMean = 0;
        for (float& v : noise.g) { v = (float)(rand() % 256) / 255.0f; srcMean += to_linear(v); }
        srcMean /= noise.g.size();
        run(d, gpu, noise, out, p);
        o = read(d, out);
        double outMean = 0;
        for (float v : o.g) outMean += to_linear(v);
        outMean /= o.g.size();
        _snprintf_s(t, sizeof(t), _TRUNCATE, "(source %.4f, resolved %.4f)", srcMean, outMean);
        check("resolve: the mean brightness survives", fabs(outMean - srcMean) < 0.01, t);
        out.release();
    }

    // ---- temporal ----------------------------------------------------------------
    {
        const uint32_t W = 160, H = 160;
        EdgeScene sc;
        Dst out = make_dst(d, W, H);
        // A still view converges to itself: 40 frames of the same image, same rotation.
        Img still = render_edge(sc, basis_from_rotator(0, 0, 0), W, H, 1);
        PassParams p; p.temporal = true; p.eye = 0; p.tanH = sc.tanH; p.tanV = sc.tanV;
        for (int f = 0; f < 40; ++f) {
            p.historyValid = f > 0;
            p.prevFromCur = prev_from_cur(basis_from_rotator(0, 0, 0), basis_from_rotator(0, 0, 0));
            run(d, gpu, still, out, p);
        }
        Img o = read(d, out);
        float worst = 0;
        for (size_t i = 0; i < o.g.size(); ++i) worst = fmaxf(worst, fabsf(o.g[i] - still.g[i]));
        char t[160]; _snprintf_s(t, sizeof(t), _TRUNCATE, "(worst %.1f steps after 40 frames)", worst * 255.0f);
        check("temporal: a still view stays exactly itself (no drift, no blur)", worst <= 2.01f / 255.0f, t);

        // Head micro-motion: each frame the camera turns by a random sub-pixel amount.
        // The renderer point-samples (aliased stairs); the truth is the pixel coverage.
        const float pxDeg = atanf(2.0f * sc.tanH / W) * 57.2957795f;
        Img truth;
        float rawErr = 0, taaErr = 0;
        Basis prevB = basis_from_rotator(0, 0, 0);
        srand(11);
        float yaw = 0, pitch = 0;
        const int frames = 48;
        for (int f = 0; f < frames; ++f) {
            yaw = ((rand() % 1000) / 1000.0f - 0.5f) * pxDeg;          // +/- half a pixel
            pitch = ((rand() % 1000) / 1000.0f - 0.5f) * pxDeg;
            const Basis b = basis_from_rotator(pitch, yaw, 0);
            Img raw = render_edge(sc, b, W, H, 1);
            p.historyValid = f > 0;
            p.prevFromCur = prev_from_cur(prevB, b);
            p.blend = 0.1f;
            run(d, gpu, raw, out, p);
            prevB = b;
            if (f == frames - 1) {
                truth = render_edge(sc, b, W, H, 16);
                o = read(d, out);
                rawErr = edge_error(raw, truth);
                taaErr = edge_error(o, truth);
            }
        }
        _snprintf_s(t, sizeof(t), _TRUNCATE, "(mean coverage error on edge pixels: raw %.3f, temporal %.3f)", rawErr, taaErr);
        check("temporal NEGATIVE CONTROL: a point-sampled edge is far from its true coverage", rawErr > 0.2f, t);
        check("temporal: under head micro-motion the edge ends at under half the raw error", taaErr < rawErr * 0.5f, t);

        // A changed scene is not smeared: black for 20 frames, then white, same camera.
        Img black = make(W, H, 0.0f), white = make(W, H, 1.0f);
        for (int f = 0; f < 21; ++f) {
            p.historyValid = f > 0;
            p.prevFromCur = prev_from_cur(basis_from_rotator(0, 0, 0), basis_from_rotator(0, 0, 0));
            run(d, gpu, f < 20 ? black : white, out, p);
        }
        o = read(d, out);
        float lo = 1;
        for (float v : o.g) lo = fminf(lo, v);
        _snprintf_s(t, sizeof(t), _TRUNCATE, "(darkest pixel %.3f after the cut)", lo);
        check("temporal: a cut to a new picture shows the new picture at once (clip)", lo >= 0.99f, t);
        out.release();
    }

    // ---- sharpen -----------------------------------------------------------------
    {
        const uint32_t W = 64, H = 64;
        Dst out = make_dst(d, W, H);
        Img flat = make(W, H, 0.4f);
        PassParams p; p.sharpen = 1.0f;
        run(d, gpu, flat, out, p);
        Img o = read(d, out);
        float worst = 0;
        for (size_t i = 0; i < o.g.size(); ++i) worst = fmaxf(worst, fabsf(o.g[i] - flat.g[i]));
        check("sharpen: a flat area is untouched", worst <= 1.01f / 255.0f);
        Img soft = make(W, H);   // a soft vertical edge 0.2 -> 0.8 over four pixels
        for (uint32_t y = 0; y < H; ++y)
            for (uint32_t x = 0; x < W; ++x) {
                const float t2 = fminf(1.0f, fmaxf(0.0f, ((float)x - 30.0f) / 4.0f));
                soft.at(x, y) = 0.2f + 0.6f * t2;
            }
        run(d, gpu, soft, out, p);
        o = read(d, out);
        // A straight ramp has no curvature in its middle, so sharpening acts at its two
        // corners: the dark side gets darker and the light side lighter (acutance).
        float lo = 1, hi = 0, mn = 1, mx = 0;
        for (uint32_t x = 24; x < 40; ++x) { lo = fminf(lo, o.at(x, 32)); hi = fmaxf(hi, o.at(x, 32)); }
        for (float v : o.g) { mn = fminf(mn, v); mx = fmaxf(mx, v); }
        const float mid0 = soft.at(32, 32), mid1 = o.at(32, 32);
        char t[160]; _snprintf_s(t, sizeof(t), _TRUNCATE, "(edge span 0.200..0.800 -> %.3f..%.3f, middle %.3f -> %.3f)", lo, hi, mid0, mid1);
        check("sharpen: the edge's corners are pushed apart (acutance)", lo < 0.19f && hi > 0.81f, t);
        check("sharpen: the middle of the edge stays put", fabsf(mid1 - mid0) <= 2.01f / 255.0f, t);
        check("sharpen: nothing leaves 0..1", mn >= 0.0f && mx <= 1.0f, t);
        out.release();
    }

    printf("clarity-gpu: %d checks, %d failures (%s)\n", g_checks, g_failed, d.kind);
    gpu.shutdown();
    d.ctx->Release(); d.dev->Release();
    return g_failed ? 1 : 0;
}
