// tools/dlss-host-tests.cpp - the DLSS transport on the host GPU, no game: a 32-bit D3D11
// client (this exe, like the proxy) drives the real x64 helper through core/gfx/dlss_client.
// Run by tools/dlss-host-test.ps1. Every check prints PASS/FAIL with its numbers; each one
// states what would make it fail.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>

#include "core/gfx/dlss_client.h"
#include "core/gfx/dlss_gpu.h"
#include "core/gfx/dlss_jitter.h"

using dvr::dlss::Client;
using dvr::dlss::EyeInputs;

static int g_fail = 0, g_pass = 0;
static void check(bool ok, const char* name, const char* fmt, ...) {
    char d[512]; va_list a; va_start(a, fmt); _vsnprintf_s(d, sizeof(d), _TRUNCATE, fmt, a); va_end(a);
    printf("%s  %-34s %s\n", ok ? "PASS" : "FAIL", name, d);
    (ok ? g_pass : g_fail)++;
}
static void logfn(int level, const char* line) { printf("      [%s] %s\n", level == 0 ? "info" : level == 1 ? "warn" : "error", line); }

static ID3D11Device* dev = nullptr;
static ID3D11DeviceContext* ctx = nullptr;

struct Tex { ID3D11Texture2D* t = nullptr; uint32_t w = 0, h = 0; DXGI_FORMAT f = DXGI_FORMAT_UNKNOWN; };
static Tex make(uint32_t w, uint32_t h, DXGI_FORMAT f) {
    Tex t; t.w = w; t.h = h; t.f = f;
    D3D11_TEXTURE2D_DESC d = {}; d.Width = w; d.Height = h; d.MipLevels = 1; d.ArraySize = 1; d.Format = f;
    d.SampleDesc.Count = 1; d.Usage = D3D11_USAGE_DEFAULT; d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    dev->CreateTexture2D(&d, nullptr, &t.t);
    return t;
}
static void upload(Tex& t, const void* data, uint32_t pitch) { ctx->UpdateSubresource(t.t, 0, nullptr, data, pitch, 0); }

// The output as luminance 0..1 (R8G8B8A8), read back through a staging copy.
static std::vector<float> readback(ID3D11Texture2D* src, uint32_t w, uint32_t h) {
    D3D11_TEXTURE2D_DESC d = {}; src->GetDesc(&d);
    d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
    ID3D11Texture2D* s = nullptr; dev->CreateTexture2D(&d, nullptr, &s);
    ctx->CopyResource(s, src);
    std::vector<float> out((size_t)w * h, 0.0f);
    D3D11_MAPPED_SUBRESOURCE m = {};
    if (SUCCEEDED(ctx->Map(s, 0, D3D11_MAP_READ, 0, &m))) {
        for (uint32_t y = 0; y < h; ++y) {
            const uint8_t* row = (const uint8_t*)m.pData + (size_t)y * m.RowPitch;
            for (uint32_t x = 0; x < w; ++x) {
                const uint8_t* p = row + x * 4;
                out[(size_t)y * w + x] = (0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2]) / 255.0f;   // R G B order for RGBA8
            }
        }
        ctx->Unmap(s, 0);
    }
    s->Release();
    return out;
}

// Scenes, BGRA8. A: a slanted high-contrast edge plus fine dots. B: its inverse. Shift moves it.
static std::vector<uint32_t> scene(uint32_t w, uint32_t h, int kind, float shiftPx) {
    std::vector<uint32_t> px((size_t)w * h);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x) {
            const float fx = (float)x - shiftPx;
            float v = (fx * 0.94f + y * 0.34f) > w * 0.55f ? 1.0f : 0.1f;       // a slanted edge
            if (((int)floorf(fx) / 7 + (int)y / 7) % 5 == 0) v = 0.6f;           // coarse dots, move with the shift
            if (kind == 1) v = 1.1f - v;
            const uint32_t c = (uint32_t)(v * 255.0f + 0.5f);
            px[(size_t)y * w + x] = 0xFF000000u | (c << 16) | (c << 8) | c;
        }
    return px;
}
static float mean(const std::vector<float>& a) { double s = 0; for (float v : a) s += v; return (float)(s / a.size()); }
static float mae(const std::vector<float>& a, const std::vector<float>& b, uint32_t w, uint32_t h, uint32_t border) {
    double s = 0; uint64_t n = 0;
    for (uint32_t y = border; y + border < h; ++y) for (uint32_t x = border; x + border < w; ++x) { s += fabs(a[y * w + x] - b[y * w + x]); ++n; }
    return (float)(s / (double)n);
}
static std::vector<float> lum_of(const std::vector<uint32_t>& px) {
    std::vector<float> l(px.size());
    for (size_t i = 0; i < px.size(); ++i) { const uint32_t c = px[i]; l[i] = (0.2126f * ((c >> 16) & 255) + 0.7152f * ((c >> 8) & 255) + 0.0722f * (c & 255)) / 255.0f; }
    return l;
}

int main(int argc, char** argv) {
    if (argc < 3) { printf("usage: dlss-host-tests <helper exe> <data dir>\n"); return 2; }
    wchar_t exe[MAX_PATH], data[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, exe, MAX_PATH);
    MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, data, MAX_PATH);
    D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx))) {
        printf("FAIL  no D3D11 device\n"); return 1;
    }
    printf("dlss host tests: 32-bit client (sizeof(void*)=%u), D3D11 feature level 0x%X\n", (unsigned)sizeof(void*), (unsigned)fl);

    // 0. The D3D9 side of the jitter, on the CPU: a camera-relative row-vector view-projection of
    //    the game's measured layout (clip = [P - C, 1] * M, clip.w = view depth; PERFORMANCE.md,
    //    "vector accuracy in pixels"), 103 degrees, a 2750x2850 viewport, patched by the hook's
    //    own shift_rows as three partial uploads. Every point, at 20 uu and at 20000 uu, must land
    //    at minus the sample offset in pixels (x right, y down): a pure screen shift of the sign
    //    dlss_jitter.h defines. Fails on a wrong sign, a depth-dependent shift or a lost row.
    {
        const uint32_t VW = 2750, VH = 2850;
        const float t = tanf(51.5f * 3.14159265f / 180.0f), p00 = 1.0f / t, p11 = p00 * VW / VH;
        // A yawed, pitched camera: forward f, right r, up u (UE world: X forward, Y right, Z up).
        const float yaw = 0.7f, pit = -0.2f;
        const float f[3] = {cosf(pit) * cosf(yaw), cosf(pit) * sinf(yaw), sinf(pit)};
        const float r[3] = {-sinf(yaw), cosf(yaw), 0};
        const float u[3] = {f[1] * r[2] - f[2] * r[1], f[2] * r[0] - f[0] * r[2], f[0] * r[1] - f[1] * r[0]};
        float M[16] = {};
        for (int j = 0; j < 3; ++j) { M[j * 4 + 0] = p00 * r[j]; M[j * 4 + 1] = p11 * u[j]; M[j * 4 + 2] = 0.1f * f[j]; M[j * 4 + 3] = f[j]; }
        M[14] = 10.0f;   // reversed-Z style constant depth term; w row stays 0 (camera-relative)
        auto project = [&](const float* m, const float* P, float* px, float* py) {
            float c[4];
            for (int i = 0; i < 4; ++i) c[i] = P[0] * m[i] + P[1] * m[4 + i] + P[2] * m[8 + i] + m[12 + i];
            *px = (c[0] / c[3] * 0.5f + 0.5f) * VW; *py = (0.5f - c[1] / c[3] * 0.5f) * VH;
        };
        float worst = 0; int pts = 0;
        for (uint32_t k = 1; k <= 8; ++k) {
            float sx, sy, ax, ay;
            dvr::dlss::jitter::phase_offset(k, 8, &sx, &sy);
            dvr::dlss::jitter::ndc_shift(sx, sy, VW, VH, &ax, &ay);
            float J[16]; memcpy(J, M, sizeof(J));
            dvr::dlss::jitter::shift_rows(J, 0, 0, 1, ax, ay);           // c0 alone
            dvr::dlss::jitter::shift_rows(J + 4, 1, 1, 3, ax, ay);       // c1..c2 as their own upload
            dvr::dlss::jitter::shift_rows(J + 12, 3, 3, 4, ax, ay);      // c3
            for (float d : {20.0f, 300.0f, 20000.0f})
                for (float a : {-0.6f, 0.0f, 0.5f}) {
                    const float P[3] = {d * (f[0] + a * r[0] + 0.3f * a * u[0]), d * (f[1] + a * r[1] + 0.3f * a * u[1]),
                                        d * (f[2] + a * r[2] + 0.3f * a * u[2])};
                    float x0, y0, x1, y1;
                    project(M, P, &x0, &y0); project(J, P, &x1, &y1);
                    const float ex = (x1 - x0) - (-sx), ey = (y1 - y0) - (-sy);
                    worst = fmaxf(worst, sqrtf(ex * ex + ey * ey)); ++pts;
                }
        }
        check(worst < 1e-3f, "jitter matrix patch = screen shift of -offset", "%d points, 8 phases, 3 depths: worst error %.2e px", pts, worst);
    }

    Client c;
    dvr::dlss::StartParams sp; sp.hostExe = exe; sp.dataDir = data; sp.log = logfn;
    char why[512] = "";
    LARGE_INTEGER f0, t0, t1; QueryPerformanceFrequency(&f0); QueryPerformanceCounter(&t0);
    const bool started = c.start(dev, sp, why, sizeof(why));
    QueryPerformanceCounter(&t1);
    check(started, "helper start + NGX init", "%s (%.0f ms) %s", started ? c.adapter() : "", (t1.QuadPart - t0.QuadPart) * 1000.0 / f0.QuadPart, why);
    if (!started) { printf("\n%d passed, %d failed\n", g_pass, g_fail); return 1; }

    const uint32_t W = 512, H = 512;
    bool b0 = c.build(0, W, H, W, H, DXGI_FORMAT_B8G8R8A8_UNORM, 0, why, sizeof(why));
    bool b1 = b0 && c.build(1, W, H, W, H, DXGI_FORMAT_B8G8R8A8_UNORM, 0, why, sizeof(why));
    check(b0 && b1, "two DLAA features (one per eye)", "%ux%u, %s", W, H, why);
    if (!(b0 && b1)) { printf("\n%d passed, %d failed\n", g_pass, g_fail); return 1; }

    Tex color[2] = {make(W, H, DXGI_FORMAT_B8G8R8A8_UNORM), make(W, H, DXGI_FORMAT_B8G8R8A8_UNORM)};
    Tex depth = make(W, H, DXGI_FORMAT_R32_FLOAT), motion = make(W, H, DXGI_FORMAT_R16G16_FLOAT);
    std::vector<float> dz((size_t)W * H, 0.5f);
    upload(depth, dz.data(), W * 4);
    std::vector<uint16_t> mv0((size_t)W * H * 2, 0);
    upload(motion, mv0.data(), W * 4);

    // 1. Static scenes, a different one per eye: each output converges to ITS eye's image.
    //    Fails if histories are shared (outputs mix), the output stays black/uninitialised,
    //    or the transport reads the wrong eye's inputs.
    auto sA = scene(W, H, 0, 0), sB = scene(W, H, 1, 0);
    upload(color[0], sA.data(), W * 4); upload(color[1], sB.data(), W * 4);
    bool allOk = true;
    for (int i = 0; i < 24; ++i)
        for (int e = 0; e < 2; ++e) {
            EyeInputs in; in.color = color[e].t; in.depth = depth.t; in.motion = motion.t; in.reset = i == 0;
            if (!c.evaluate(ctx, e, in, why, sizeof(why))) { allOk = false; printf("      evaluate: %s\n", why); }
        }
    check(allOk, "48 evaluates, alternating eyes", "%llu/%llu frames, %llu refused", (unsigned long long)c.stats.frames[0],
          (unsigned long long)c.stats.frames[1], (unsigned long long)(c.stats.refused[0] + c.stats.refused[1]));
    auto o0 = readback(c.output_texture(0), W, H), o1 = readback(c.output_texture(1), W, H);
    auto lA = lum_of(sA), lB = lum_of(sB);
    const float e00 = mae(o0, lA, W, H, 8), e01 = mae(o0, lB, W, H, 8), e11 = mae(o1, lB, W, H, 8), e10 = mae(o1, lA, W, H, 8);
    bool finite = true; for (float v : o0) finite = finite && isfinite(v); for (float v : o1) finite = finite && isfinite(v);
    check(finite && mean(o0) > 0.05f && mean(o1) > 0.05f, "outputs finite and not black", "mean L %.3f R %.3f (inputs %.3f %.3f)",
          mean(o0), mean(o1), mean(lA), mean(lB));
    check(e00 < 0.06f && e11 < 0.06f, "each eye reproduces its image", "|L-A| %.4f |R-B| %.4f (limit 0.06)", e00, e11);
    check(e00 * 4 < e01 && e11 * 4 < e10, "eyes isolated", "|L-B| %.4f vs |L-A| %.4f, |R-A| %.4f vs |R-B| %.4f", e01, e00, e10, e11);

    // 2. Motion vector convention: the scene moves +1 px/frame in x. The vector the proxy
    //    writes is previous UV minus current UV, so the true one is -1/W. The run with the
    //    true sign must track the current image better than the one with the flipped sign.
    //    Fails if DLSS reads the vectors with the other sign or ignores them.
    auto run_motion = [&](float mvx) {
        std::vector<uint16_t> mv((size_t)W * H * 2);
        // float -> half for a small value
        auto half = [](float v) -> uint16_t {
            uint32_t b; memcpy(&b, &v, 4);
            const uint32_t s = (b >> 16) & 0x8000; int e = (int)((b >> 23) & 0xFF) - 127 + 15; uint32_t m = (b >> 13) & 0x3FF;
            if (v == 0) return 0;
            if (e <= 0) return (uint16_t)s;
            return (uint16_t)(s | (e << 10) | m);
        };
        const uint16_t hx = half(mvx);
        for (size_t i = 0; i < (size_t)W * H; ++i) { mv[i * 2] = hx; mv[i * 2 + 1] = 0; }
        upload(motion, mv.data(), W * 4);
        float err = 0;
        for (int i = 0; i < 20; ++i) {
            auto s = scene(W, H, 0, (float)i);
            upload(color[0], s.data(), W * 4);
            EyeInputs in; in.color = color[0].t; in.depth = depth.t; in.motion = motion.t; in.reset = i == 0;
            c.evaluate(ctx, 0, in, why, sizeof(why));
            if (i == 19) err = mae(readback(c.output_texture(0), W, H), lum_of(s), W, H, 24);
        }
        return err;
    };
    const float good = run_motion(-1.0f / W), bad = run_motion(+1.0f / W), none = run_motion(0.0f);
    check(good < bad && good <= none * 1.05f, "motion vector sign (prev-cur UV)", "error true %.4f, flipped %.4f, zero %.4f", good, bad, none);
    upload(motion, mv0.data(), W * 4);

    // 3. Reset: after a hard cut to the other scene with reset, the first output is the new
    //    scene; without it, some of the old history may survive. Fails if reset is ignored.
    {
        upload(color[1], sA.data(), W * 4);
        EyeInputs in; in.color = color[1].t; in.depth = depth.t; in.motion = motion.t; in.reset = true;
        c.evaluate(ctx, 1, in, why, sizeof(why));
        const float er = mae(readback(c.output_texture(1), W, H), lA, W, H, 8);
        check(er < 0.08f, "reset takes the new image at once", "|R-A| after cut %.4f (limit 0.08)", er);
    }

    // 3b. The smear case and the anti-smear mask. Inside a window, a mid-contrast texture
    //     slides 3 px/frame while the vectors say nothing moved - what camera-only vectors say
    //     about the game's arms while walking. Plausible old texture is exactly what DLSS keeps,
    //     so it smears. With the production mask shader (dlss_gpu) feeding DLSS, the window takes
    //     the current image. Fails if the mask does not light the window, lights the static
    //     scene, or DLSS ignores it. (A bold solid object is rejected by DLSS without help.)
    {
        dvr::dlss::GuideGpu guides;
        char gwhy[256] = "";
        const bool gok = guides.init(dev, gwhy, sizeof(gwhy));
        check(gok, "guide + mask shaders compile", "%s", gwhy);
        ID3D11ShaderResourceView* csrv = nullptr;
        dev->CreateShaderResourceView(color[0].t, nullptr, &csrv);
        const int X0 = 120, Y0 = 150, WS = 160;
        auto texel = [](int x, int y) {   // a mid-contrast texture: soft stripes and blotches
            const float v = 0.45f + 0.12f * sinf(x * 0.45f) + 0.10f * sinf(y * 0.31f + x * 0.17f) + ((x / 5 + y / 9) % 4 == 0 ? 0.1f : 0.0f);
            return v;
        };
        auto frameImg = [&](int i) {
            auto px = scene(W, H, 0, 0);
            for (int y = Y0; y < Y0 + WS; ++y)
                for (int x = X0; x < X0 + WS; ++x) {
                    const uint32_t c = (uint32_t)(texel(x - 3 * i, y) * 255.0f + 0.5f);
                    px[(size_t)y * W + x] = 0xFF000000u | (c << 16) | (c << 8) | (c * 3 / 4);
                }
            return px;
        };
        auto region = [&](const std::vector<float>& out, const std::vector<float>& cur) {
            double e = 0; uint64_t n = 0;
            for (int y = Y0 + 8; y < Y0 + WS - 8; ++y)
                for (int x = X0 + 8; x < X0 + WS - 8; ++x) { e += fabs(out[y * W + x] - cur[y * W + x]); ++n; }
            return (float)(e / n);
        };
        float err[2] = {}, maskIn = 0, maskBg = 0;
        for (int useMask = 0; useMask < 2 && gok; ++useMask) {
            guides.forget(0);
            for (int i = 0; i < 24; ++i) {
                auto img = frameImg(i);
                upload(color[0], img.data(), W * 4);
                dvr::dlss::GuideParams gp; gp.w = W; gp.h = H; gp.historyValid = i > 0;
                guides.run(dev, ctx, gp, gwhy, sizeof(gwhy));
                guides.mask(dev, ctx, 0, csrv, gp.historyValid, 0.03f, 0.12f, gwhy, sizeof(gwhy));
                guides.keep(dev, ctx, 0, color[0].t);
                EyeInputs in; in.color = color[0].t; in.depth = guides.depth(); in.motion = guides.motion();
                in.bias = useMask ? guides.bias() : nullptr; in.reset = i == 0;
                c.evaluate(ctx, 0, in, why, sizeof(why));
                if (i == 23) {
                    err[useMask] = region(readback(c.output_texture(0), W, H), lum_of(img));
                    if (useMask) {
                        D3D11_TEXTURE2D_DESC d = {}; guides.bias()->GetDesc(&d);
                        d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                        ID3D11Texture2D* st = nullptr; dev->CreateTexture2D(&d, nullptr, &st);
                        ctx->CopyResource(st, guides.bias());
                        D3D11_MAPPED_SUBRESOURCE m = {};
                        if (SUCCEEDED(ctx->Map(st, 0, D3D11_MAP_READ, 0, &m))) {
                            auto at = [&](int x, int y) { return ((const uint8_t*)m.pData)[(size_t)y * m.RowPitch + x] / 255.0f; };
                            double a = 0, b = 0; int na = 0, nb = 0;
                            for (int y = Y0 + 8; y < Y0 + WS - 8; ++y) for (int x = X0 + 8; x < X0 + WS - 8; ++x) { a += at(x, y); ++na; }
                            for (int y = 350; y < 450; ++y) for (int x = 300; x < 450; ++x) { b += at(x, y); ++nb; }
                            maskIn = (float)(a / na); maskBg = (float)(b / nb);
                            ctx->Unmap(st, 0);
                        }
                        st->Release();
                    }
                }
            }
        }
        check(maskIn > 0.2f && maskBg < 0.02f, "mask lights moving texture only", "moving window %.2f, static scene %.3f",
              maskIn, maskBg);
        // Recorded, not asserted: this is the finding that DLSS already rejects large unexplained
        // motion by itself (both errors sit at the static-image level), so the mask ships off.
        printf("INFO  3 px/frame unexplained motion    window error without mask %.4f, with mask %.4f (static-image level ~0.005)\n",
               err[0], err[1]);
        csrv->Release();
        guides.shutdown();
        upload(color[0], sA.data(), W * 4);
    }

    // 3c. Super Resolution: 512 -> 768 (Quality, 1.5x). The feature must build, the output must be
    //     768x768 and must be the input scene enlarged (compared against a nearest-neighbour
    //     enlargement of the input). Fails if NGX refuses the ratio or the output is not the scene.
    {
        const uint32_t OW = 768, OH = 768;
        bool ok = c.build(0, W, H, OW, OH, DXGI_FORMAT_B8G8R8A8_UNORM, 0, why, sizeof(why));
        float err = 1;
        if (ok) {
            upload(color[0], sA.data(), W * 4);
            for (int i = 0; i < 16 && ok; ++i) {
                EyeInputs in; in.color = color[0].t; in.depth = depth.t; in.motion = motion.t; in.reset = i == 0;
                ok = c.evaluate(ctx, 0, in, why, sizeof(why));
            }
            D3D11_TEXTURE2D_DESC od = {}; c.output_texture(0)->GetDesc(&od);
            auto out = readback(c.output_texture(0), OW, OH);
            std::vector<float> big((size_t)OW * OH);
            for (uint32_t y = 0; y < OH; ++y) for (uint32_t x = 0; x < OW; ++x) big[y * OW + x] = lA[(y * H / OH) * W + (x * W / OW)];
            err = mae(out, big, OW, OH, 12);
            ok = ok && od.Width == OW && od.Height == OH;
        }
        check(ok && err < 0.08f, "Super Resolution 512 -> 768", "%s, error vs the enlarged input %.4f (limit 0.08)", ok ? "built" : why, err);
        c.build(0, W, H, W, H, DXGI_FORMAT_B8G8R8A8_UNORM, 0, why, sizeof(why));   // back to DLAA for what follows
    }

    // 3d. Projection jitter sign (dlss_jitter.h). A still scene with detail finer than the render
    //     grid (a slanted edge, a steep edge, stripes at 1.47 render px) is point-sampled at
    //     512x512 with the production Halton offsets and rebuilt to 768x768. Each run reports the
    //     offset to DLSS with one of the four sign pairs; the error is against the scene itself,
    //     4x4 supersampled at the output. The production pair (kReportX/Y) must be the best of the
    //     four and better than no jitter at all. Fails if DLSS reads the offset with another sign,
    //     ignores it, or jitter does not add detail.
    {
        const uint32_t OW = 768, OH = 768, N = 18;   // Quality: 8 * 1.5^2 phases
        auto g = [&](float u, float v) {              // the continuous scene, render-pixel units
            float s = (u * 0.94f + v * 0.34f) > W * 0.55f ? 0.9f : 0.15f;
            if ((u * 0.28f - v * 0.96f) > -H * 0.30f) s = 1.05f - s;
            if (u > 60 && u < 200 && v > 300 && v < 440) s = 0.5f + 0.4f * sinf((u * 0.9f + v * 0.44f) * 6.2831853f / 1.47f);
            return s;
        };
        std::vector<float> ref((size_t)OW * OH);
        for (uint32_t y = 0; y < OH; ++y)
            for (uint32_t x = 0; x < OW; ++x) {
                float a = 0;
                for (int j = 0; j < 4; ++j) for (int i = 0; i < 4; ++i)
                    a += g((x + (i + 0.5f) / 4) * W / OW, (y + (j + 0.5f) / 4) * H / OH);
                ref[(size_t)y * OW + x] = a / 16;
            }
        bool ok = c.build(0, W, H, OW, OH, DXGI_FORMAT_B8G8R8A8_UNORM, 0, why, sizeof(why));
        auto run_jit = [&](bool jit, float rx, float ry) {
            std::vector<uint32_t> px((size_t)W * H);
            double err = 0; int n = 0;
            for (uint32_t f = 0; f < 64 && ok; ++f) {
                float sx = 0, sy = 0;
                if (jit) dvr::dlss::jitter::phase_offset(f, N, &sx, &sy);
                for (uint32_t y = 0; y < H; ++y)
                    for (uint32_t x = 0; x < W; ++x) {
                        const float v = g(x + 0.5f + sx, y + 0.5f + sy);
                        const uint32_t cc = (uint32_t)(fminf(fmaxf(v, 0.0f), 1.0f) * 255.0f + 0.5f);
                        px[(size_t)y * W + x] = 0xFF000000u | (cc << 16) | (cc << 8) | cc;
                    }
                upload(color[0], px.data(), W * 4);
                EyeInputs in; in.color = color[0].t; in.depth = depth.t; in.motion = motion.t; in.reset = f == 0;
                in.jitterX = rx * sx; in.jitterY = ry * sy;
                ok = c.evaluate(ctx, 0, in, why, sizeof(why));
                if (f >= 56) { err += mae(readback(c.output_texture(0), OW, OH), ref, OW, OH, 16); ++n; }
            }
            return n ? (float)(err / n) : 1.0f;
        };
        const float none = run_jit(false, 0, 0), unreported = run_jit(true, 0, 0);
        const float pp = run_jit(true, 1, 1), mm = run_jit(true, -1, -1), pm = run_jit(true, 1, -1), mp = run_jit(true, -1, 1);
        const float prod = run_jit(true, dvr::dlss::jitter::kReportX, dvr::dlss::jitter::kReportY);
        const float best = fminf(fminf(pp, mm), fminf(pm, mp));
        check(ok && prod <= best * 1.0001f && prod < none, "projection jitter sign (Super Resolution)",
              "error: production (%+.0f,%+.0f) %.4f | +x+y %.4f, -x-y %.4f, +x-y %.4f, -x+y %.4f | no jitter %.4f, "
              "jittered but reported 0 %.4f%s", dvr::dlss::jitter::kReportX, dvr::dlss::jitter::kReportY, prod, pp, mm, pm, mp,
              none, unreported, ok ? "" : why);
        c.build(0, W, H, W, H, DXGI_FORMAT_B8G8R8A8_UNORM, 0, why, sizeof(why));
    }

    // 4. Cost at the real eye size (2750 x 2850 is this machine's F10 100% reference).
    {
        const uint32_t EW = 2752, EH = 2848;
        bool ok = c.build(0, EW, EH, EW, EH, DXGI_FORMAT_B8G8R8A8_UNORM, 0, why, sizeof(why)) &&
                  c.build(1, EW, EH, EW, EH, DXGI_FORMAT_B8G8R8A8_UNORM, 0, why, sizeof(why));
        Tex bc = make(EW, EH, DXGI_FORMAT_B8G8R8A8_UNORM), bd = make(EW, EH, DXGI_FORMAT_R32_FLOAT), bm = make(EW, EH, DXGI_FORMAT_R16G16_FLOAT);
        auto s = scene(EW, EH, 0, 0); upload(bc, s.data(), EW * 4);
        std::vector<float> z((size_t)EW * EH, 0.5f); upload(bd, z.data(), EW * 4);
        std::vector<uint16_t> m((size_t)EW * EH * 2, 0); upload(bm, m.data(), EW * 4);
        c.stats = dvr::dlss::Stats{};
        for (int i = 0; ok && i < 60; ++i)
            for (int e = 0; e < 2; ++e) {
                EyeInputs in; in.color = bc.t; in.depth = bd.t; in.motion = bm.t; in.reset = i == 0;
                ok = c.evaluate(ctx, e, in, why, sizeof(why));
            }
        ctx->Flush();
        const double gpu0 = c.stats.gpuN[0] ? c.stats.gpuMsSum[0] / c.stats.gpuN[0] : -1;
        const double gpu1 = c.stats.gpuN[1] ? c.stats.gpuMsSum[1] / c.stats.gpuN[1] : -1;
        check(ok, "DLAA at eye size 2752x2848", "GPU evaluate L %.2f R %.2f ms, present-thread CPU avg L %.3f R %.3f max %.3f ms, "
              "%.1f MiB shared", gpu0, gpu1, c.stats.cpuMsSum[0] / 60, c.stats.cpuMsSum[1] / 60,
              c.stats.cpuMsMax[0] > c.stats.cpuMsMax[1] ? c.stats.cpuMsMax[0] : c.stats.cpuMsMax[1],
              (double)c.bytes() / (1024.0 * 1024.0), why);
        bc.t->Release(); bd.t->Release(); bm.t->Release();
    }

    // 4b. Cost per preset and mode at this machine's output (recorded, not asserted): the GPU time
    //     of one evaluate at 2750x2850 output, per NGX render preset, for Performance (1374x1424
    //     in) and Quality (1832x1900 in). The number that decides whether SR can beat native.
    if (argc > 3 && !strcmp(argv[3], "--cost")) {
        const uint32_t OW = 2750, OH = 2850;
        const struct { uint32_t w, h; const char* name; } modes[] = {{1374, 1424, "Performance"}, {1832, 1900, "Quality"}, {2750, 2850, "DLAA"}};
        const int presets[] = {0, 5, 6, 10, 11, 12, 13};
        for (const auto& md : modes) {
            Tex bc = make(md.w, md.h, DXGI_FORMAT_B8G8R8A8_UNORM), bd = make(md.w, md.h, DXGI_FORMAT_R32_FLOAT), bm = make(md.w, md.h, DXGI_FORMAT_R16G16_FLOAT);
            auto sc = scene(md.w, md.h, 0, 0); upload(bc, sc.data(), md.w * 4);
            std::vector<float> z((size_t)md.w * md.h, 0.5f); upload(bd, z.data(), md.w * 4);
            std::vector<uint16_t> m((size_t)md.w * md.h * 2, 0); upload(bm, m.data(), md.w * 4);
            for (int pr : presets) {
                if (!c.build(0, md.w, md.h, OW, OH, DXGI_FORMAT_B8G8R8A8_UNORM, pr, why, sizeof(why))) {
                    printf("INFO  cost %-11s preset %2d: build refused (%s)\n", md.name, pr, why); continue;
                }
                c.stats = dvr::dlss::Stats{};
                bool ok = true;
                for (int i = 0; i < 60 && ok; ++i) {
                    EyeInputs in; in.color = bc.t; in.depth = bd.t; in.motion = bm.t; in.reset = i == 0;
                    ok = c.evaluate(ctx, 0, in, why, sizeof(why));
                }
                printf("INFO  cost %-11s preset %2d: %ux%u -> %ux%u GPU evaluate %.2f ms (%llu samples)%s\n", md.name, pr, md.w, md.h, OW, OH,
                       c.stats.gpuN[0] ? c.stats.gpuMsSum[0] / c.stats.gpuN[0] : -1.0, (unsigned long long)c.stats.gpuN[0], ok ? "" : " FAILED");
            }
            bc.t->Release(); bd.t->Release(); bm.t->Release();
        }
    }

    // 5. Helper loss: kill it; the next evaluate must fail fast and stop the client, never hang.
    {
        DWORD ids[1024], n = 0;
        // find the helper by name among this process's children: the simplest route is the job,
        // but the test kills by image name to act like a crash from outside.
        system("taskkill /F /IM dvr_dlss_host64.exe >NUL 2>&1");
        Sleep(200);
        (void)ids; (void)n;
        EyeInputs in; in.color = color[0].t; in.depth = depth.t; in.motion = motion.t;
        LARGE_INTEGER a, b; QueryPerformanceCounter(&a);
        const bool r = c.evaluate(ctx, 0, in, why, sizeof(why));
        QueryPerformanceCounter(&b);
        const double ms = (b.QuadPart - a.QuadPart) * 1000.0 / f0.QuadPart;
        check(!r && !c.running() && ms < 1500, "helper killed: fail fast, no hang", "evaluate %s in %.0f ms, running %d (%s)",
              r ? "succeeded" : "failed", ms, (int)c.running(), why);
    }
    c.stop();
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
