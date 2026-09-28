// Host tests for AFW's held-eye warp (VR-39): the production shader and draw in
// src/core/gfx/afw_warp.cpp against a synthetic held image with known depth. Never touches the game.
// Build and run: tools\afw-warp-host.ps1
//
// The source image encodes its own UV in red/green, so every output pixel says which source pixel
// it sampled. A far striped wall (world) and a near quad (the "hand", blue = 1, depth below the body
// threshold). Each case compares the shader's choice against the same geometry done on the CPU, and
// the negative control proves the body test can fail: without the body threshold the hand moves.
#include "core/gfx/afw_warp.h"

#include <windows.h>
#include <d3d11.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>

// ---- stubs for the module's dependencies --------------------------------------------------------
#include "core/util/log.h"
namespace dvr::log {
uint8_t g_levels[(int)Cat::COUNT] = {};
void write(Cat, Level, const char*, ...) {}
}
static ID3D11ShaderResourceView* g_depthSrv = nullptr;
static UINT g_depthW = 0, g_depthH = 0;
namespace dvr::clarity { float depth_scale() { return 250.0f; } }
namespace dvr::depthprobe {
ID3D11ShaderResourceView* depth_srv_for(uint32_t, UINT* w, UINT* h) { if (w) *w = g_depthW; if (h) *h = g_depthH; return g_depthSrv; }
void read_done(ID3D11DeviceContext*) {}
}

// ---- the synthetic scene ------------------------------------------------------------------------
static const int N = 256;
static const float kWorldZ = 40.0f, kBodyZ = 0.2f;   // depth units (x2.5 m: 100 m and 0.5 m)
static bool inBody(float u, float v) { return u >= 0.40f && u <= 0.60f && v >= 0.60f && v <= 0.80f; }

static int g_fail = 0, g_pass = 0;
static void check(bool ok, const char* name, const char* fmt, double a, double b = 0) {
    char d[256]; snprintf(d, sizeof(d), fmt, a, b);
    printf("%-50s %s  %s\n", name, ok ? "PASS" : "FAIL", d); ok ? ++g_pass : ++g_fail;
}

struct Gpu {
    ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr;
    ID3D11Texture2D* frame = nullptr; ID3D11Texture2D* depth = nullptr; ID3D11Texture2D* dst = nullptr;
    ID3D11Texture2D* stage = nullptr;
};

static ID3D11Texture2D* tex(ID3D11Device* dev, UINT bind, D3D11_USAGE use, UINT cpu, const void* init) {
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = N; td.MipLevels = td.ArraySize = 1; td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    td.SampleDesc.Count = 1; td.Usage = use; td.BindFlags = bind; td.CPUAccessFlags = cpu;
    D3D11_SUBRESOURCE_DATA sd = {init, N * 16, 0};
    ID3D11Texture2D* t = nullptr;
    dev->CreateTexture2D(&td, init ? &sd : nullptr, &t);
    return t;
}

// XR view convention: -Z forward, +X right, +Y up; tan = 1 both ways.
struct V3 { double x, y, z; };
static V3 dirOf(double u, double v) { return {u * 2 - 1, 1 - v * 2, -1}; }
static void uvOf(V3 L, double* u, double* v) { const double iz = 1.0 / -L.z; *u = (L.x * iz) * 0.5 + 0.5; *v = 0.5 - (L.y * iz) * 0.5; }
static V3 ry(V3 p, double deg) { const double a = deg / 57.29578, c = cos(a), s = sin(a); return {c * p.x + s * p.z, p.y, -s * p.x + c * p.z}; }

static dvr::afw::Pose pose(double x) { dvr::afw::Pose p = {{0, 0, 0, 1}, {(float)x, 0, 0}}; return p; }

// Runs one warp: the held (left, eye 0) image at `srcX`, the fresh (right) image's generation places the
// held eye at `tgtX`, body yaw from held to fresh `yaw` degrees.
static std::vector<float> run(Gpu& g, double srcX, double tgtX, double yaw, float bodyDepth) {
    dvr::afw::set_enabled(true, "test");
    dvr::afw::set_body_depth(bodyDepth, "test");
    dvr::afw::set_world_scale(100.0f);
    const dvr::afw::Pose tg[2] = {pose(tgtX), pose(tgtX + 0.063)};
    dvr::afw::note_capture(g.dev, g.ctx, 0, g.frame, 1, pose(srcX), true, 0.0f, tg);          // the held eye's image
    dvr::afw::note_capture(g.dev, g.ctx, 1, g.frame, 2, pose(tgtX + 0.063), true, (float)yaw, tg); // the fresh eye
    dvr::afw::Pose out{};
    const char* why = nullptr;
    const bool ok = dvr::afw::warp_held(g.dev, g.ctx, 0, 1, g.dst, N, N, 1.0f, 1.0f, &out, &why);
    if (!ok) { printf("warp refused: %s\n", why ? why : "?"); return {}; }
    g.ctx->CopyResource(g.stage, g.dst);
    D3D11_MAPPED_SUBRESOURCE m;
    std::vector<float> px(N * N * 4);
    if (SUCCEEDED(g.ctx->Map(g.stage, 0, D3D11_MAP_READ, 0, &m))) {
        for (int y = 0; y < N; ++y) memcpy(&px[y * N * 4], (const uint8_t*)m.pData + y * m.RowPitch, N * 16);
        g.ctx->Unmap(g.stage, 0);
    }
    return px;
}
static void at(const std::vector<float>& px, double u, double v, float* r, float* gg, float* b) {
    const int x = (int)(u * N), y = (int)(v * N);
    const float* p = &px[(y * N + x) * 4]; *r = p[0]; *gg = p[1]; *b = p[2];
}

int main() {
    Gpu g;
    const D3D_FEATURE_LEVEL fl[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fl, 2, D3D11_SDK_VERSION, &g.dev, nullptr, &g.ctx)) &&
        FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, fl, 2, D3D11_SDK_VERSION, &g.dev, nullptr, &g.ctx))) {
        printf("no D3D11 device\n"); return 2;
    }
    std::vector<float> color(N * N * 4), depth(N * N * 4);
    for (int y = 0; y < N; ++y) for (int x = 0; x < N; ++x) {
        const float u = (x + 0.5f) / N, v = (y + 0.5f) / N;
        float* c = &color[(y * N + x) * 4]; float* d = &depth[(y * N + x) * 4];
        const bool b = inBody(u, v);
        c[0] = u; c[1] = v; c[2] = b ? 1.0f : 0.0f; c[3] = 1;
        d[0] = d[1] = d[2] = 0; d[3] = b ? kBodyZ : kWorldZ;
    }
    g.frame = tex(g.dev, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, color.data());
    g.depth = tex(g.dev, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, depth.data());
    g.dst = tex(g.dev, D3D11_BIND_RENDER_TARGET, D3D11_USAGE_DEFAULT, 0, nullptr);
    g.stage = tex(g.dev, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ, nullptr);
    g.dev->CreateShaderResourceView(g.depth, nullptr, &g_depthSrv);
    g_depthW = g_depthH = N;
    if (!g.frame || !g.depth || !g.dst || !g.stage || !g_depthSrv) { printf("resource creation failed\n"); return 2; }
    const double px1 = 1.0 / N;
    float r, gg, b;

    {   // 1. identity: every pixel samples itself
        auto p = run(g, 0.0, 0.0, 0.0, 0.40f);
        double worst = 0;
        for (double v = 0.1; v < 0.95; v += 0.1) for (double u = 0.1; u < 0.95; u += 0.1) {
            at(p, u, v, &r, &gg, &b);
            const double e = fmax(fabs(r - (floor(u * N) + 0.5) / N), fabs(gg - (floor(v * N) + 0.5) / N));
            if (e > worst) worst = e;
        }
        check(!p.empty() && worst < 1.0 * px1, "identity: each pixel samples itself", "worst %.2f px", worst * N);
    }
    {   // 2. a 5 deg stick turn: world moves by the yaw, the hand stays
        const double yaw = 5.0;
        auto p = run(g, 0.0, 0.0, yaw, 0.40f);
        double worst = 0, sign = 0;
        for (double v = 0.1; v < 0.55; v += 0.1) for (double u = 0.15; u < 0.9; u += 0.1) {
            // CPU: the world point behind target pixel t, carried back through Ry(-yaw), into the source.
            double su, sv;
            uvOf(ry(dirOf(u, v), -yaw), &su, &sv);
            at(p, u, v, &r, &gg, &b);
            worst = fmax(worst, fmax(fabs(r - su), fabs(gg - sv)));
            sign += su - u;
        }
        check(!p.empty() && worst < 1.5 * px1, "turn: world samples the yaw-rotated source", "worst %.2f px", worst * N);
        check(sign > 0, "turn: turning right shows content from further right", "mean source-target du %.4f", sign);
        at(p, 0.5, 0.7, &r, &gg, &b);
        check(b > 0.5f && fabs(r - 0.5) < 1.5 * px1 && fabs(gg - 0.7) < 1.5 * px1, "turn: the hand stays where it was",
              "sampled (%.3f, %.3f)", r, gg);
    }
    {   // 3. NEGATIVE CONTROL: no body threshold - the same turn moves the hand (the ghost)
        auto p = run(g, 0.0, 0.0, 5.0, 0.01f);
        at(p, 0.5, 0.7, &r, &gg, &b);
        check(!p.empty() && fabs(r - 0.5) > 3 * px1, "control: without the body test the hand moves", "sampled u %.3f (hand at 0.5)", r);
    }
    {   // 4. head translation 2 cm right: the near hand shifts left (parallax), the far wall does not
        auto p = run(g, 0.0, 0.02, 0.0, 0.40f);
        // CPU: a hand pixel at target t is the point at 0.5 m; from the source eye 2 cm left of the target,
        // it sits at x_src = x_tgt + 0.02 in view coords at z = 0.5 m.
        at(p, 0.47, 0.7, &r, &gg, &b);
        const double expect = 0.47 + 0.5 * (0.02 / 0.5);
        check(b > 0.5f && fabs(r - expect) < 1.5 * px1, "move: the hand shows its parallax", "sampled u %.4f want %.4f", r, expect);
        at(p, 0.2, 0.3, &r, &gg, &b);
        check(fabs(r - 0.2) < 1.5 * px1, "move: the far wall barely moves", "sampled u %.4f want 0.2", r);
    }
    printf("afw warp: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
