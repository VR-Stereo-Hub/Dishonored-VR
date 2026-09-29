// Host tests for the DLSS object-motion pass (VR-39): src/core/gfx/dlss_gpu.cpp's production shaders against a
// synthetic eye image pair. No NGX, no game. Build and run: tools\dlss-objmotion-host.ps1
//
// The scene (512x512, 90 deg): a textured panorama fixed in the world while the camera yaws between the images
// (the camera's vectors are exact there), a "character" that also moves on its own, and a "boat" region that
// rides with the camera (zero screen motion while the camera's vectors say the yaw). The truth per pixel is
// known. The controls: the camera vectors alone must be wrong on the character and the boat, and a flat region
// must keep the camera's vectors.
#include "core/gfx/dlss_gpu.h"

#include <windows.h>
#include <d3d11.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <stdlib.h>

using dvr::dlss::GuideGpu;
using dvr::dlss::GuideParams;

static int g_fail = 0, g_pass = 0;
static void check(bool ok, const char* name, const char* detail) {
    printf("%-64s %s  %s\n", name, ok ? "PASS" : "FAIL", detail);
    ok ? ++g_pass : ++g_fail;
}

static const int W = 512, H = 512;
static const double kTan = 1.0;
static ID3D11Device* dev = nullptr;
static ID3D11DeviceContext* ctx = nullptr;

static double hash2(int x, int y) {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xffff) / 65535.0;
}
static double noise(double x, double y) {   // smooth value noise, features about one unit wide
    const int ix = (int)floor(x), iy = (int)floor(y);
    const double fx = x - ix, fy = y - iy;
    const double sx = fx * fx * (3 - 2 * fx), sy = fy * fy * (3 - 2 * fy);
    const double a = hash2(ix, iy), b = hash2(ix + 1, iy), c = hash2(ix, iy + 1), d = hash2(ix + 1, iy + 1);
    return a + (b - a) * sx + (c - a) * sy + (a - b - c + d) * sx * sy;
}
static double pattern(double x, double y, int seed) { return 0.15 + 0.7 * (0.6 * noise(x + seed * 31.7, y) + 0.4 * noise(2.1 * x, 2.1 * y + seed * 11.3)); }

// Directions in the camera's (forward, right, up) axes; the world texture is a function of direction.
static double worldAt(double f, double r, double u) {
    const double az = atan2(r, f), el = atan2(u, sqrt(f * f + r * r));
    return pattern(az * 120.0, el * 120.0, 1);   // ~0.008 rad features, about 2.5 px at the centre
}
struct Scene {
    double yawDeg = 2.5;                 // camera yaw between the images (the previous camera turned by -yaw)
    double npcDx = 2, npcDy = -6;        // the character's own motion, previous -> current, px
    int npcX0 = 300, npcY0 = 150, npcX1 = 390, npcY1 = 250;   // its rectangle in the CURRENT image
    int boatX0 = 80, boatY0 = 320, boatX1 = 200, boatY1 = 440;   // rides with the camera (same place both images)
    int flatX0 = 260, flatY0 = 330, flatX1 = 330, flatY1 = 400;  // a flat grey world patch (no texture)
};
static void dirOf(double px, double py, double o[3]) {   // pixel centre -> direction (f, r, u)
    const double x = (px + 0.5) / W * 2 - 1, y = 1 - (py + 0.5) / H * 2;
    o[0] = 1; o[1] = x * kTan; o[2] = y * kTan;
}
// The previous camera relative to the current: prevFromCur rotates a current-axes direction into previous axes.
static void rotYaw(double deg, double m[3][3]) {   // about the up axis: forward/right plane
    const double a = deg / 57.29577951, c = cos(a), s = sin(a);
    const double r[3][3] = {{c, -s, 0}, {s, c, 0}, {0, 0, 1}};
    memcpy(m, r, sizeof(r));
}
static bool inRect(double x, double y, int x0, int y0, int x1, int y1) { return x >= x0 && x < x1 && y >= y0 && y < y1; }

static uint32_t rgba(double l) { const uint32_t v = (uint32_t)(fmin(fmax(l, 0.0), 1.0) * 255.0 + 0.5); return v | (v << 8) | (v << 16) | 0xff000000u; }

static void images(const Scene& sc, std::vector<uint32_t>& cur, std::vector<uint32_t>& prev, double M[3][3]) {
    rotYaw(sc.yawDeg, M);
    cur.assign(W * H, 0); prev.assign(W * H, 0);
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) {
            double d[3]; dirOf(x, y, d);
            // current
            double l;
            if (inRect(x, y, sc.npcX0, sc.npcY0, sc.npcX1, sc.npcY1)) l = pattern((x - sc.npcX0) / 3.0, (y - sc.npcY0) / 3.0, 2);
            else if (inRect(x, y, sc.boatX0, sc.boatY0, sc.boatX1, sc.boatY1)) l = pattern(x / 3.0, y / 3.0, 3);
            else if (inRect(x, y, sc.flatX0, sc.flatY0, sc.flatX1, sc.flatY1)) l = 0.5;
            else l = worldAt(d[0], d[1], d[2]);
            cur[y * W + x] = rgba(l);
            // previous: the world through the rotation, the character where it was, the boat where it is
            double pl;
            const double nx = x + sc.npcDx, ny = y + sc.npcDy;   // the current-image place of this previous pixel's character texel
            if (inRect(nx, ny, sc.npcX0, sc.npcY0, sc.npcX1, sc.npcY1)) pl = pattern((nx - sc.npcX0) / 3.0, (ny - sc.npcY0) / 3.0, 2);
            else if (inRect(x, y, sc.boatX0, sc.boatY0, sc.boatX1, sc.boatY1)) pl = pattern(x / 3.0, y / 3.0, 3);
            else {
                // this previous pixel's direction in current axes: M^T d
                const double c0 = M[0][0] * d[0] + M[1][0] * d[1] + M[2][0] * d[2];
                const double c1 = M[0][1] * d[0] + M[1][1] * d[1] + M[2][1] * d[2];
                const double c2 = M[0][2] * d[0] + M[1][2] * d[1] + M[2][2] * d[2];
                // the flat patch is world-fixed: its previous place is where its current directions came from
                const double fx = (c1 / c0 / kTan + 1) * 0.5 * W - 0.5, fy = (1 - c2 / c0 / kTan) * 0.5 * H - 0.5;
                pl = inRect(fx, fy, sc.flatX0, sc.flatY0, sc.flatX1, sc.flatY1) ? 0.5 : worldAt(c0, c1, c2);
            }
            prev[y * W + x] = rgba(pl);
        }
}

static ID3D11Texture2D* tex(DXGI_FORMAT f, UINT bind, D3D11_USAGE use, UINT cpu, const void* init, UINT pitch) {
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = W; td.Height = H; td.MipLevels = td.ArraySize = 1; td.Format = f; td.SampleDesc.Count = 1;
    td.Usage = use; td.BindFlags = bind; td.CPUAccessFlags = cpu;
    D3D11_SUBRESOURCE_DATA sd = {init, pitch, 0};
    ID3D11Texture2D* t = nullptr;
    dev->CreateTexture2D(&td, init ? &sd : nullptr, &t);
    return t;
}
static std::vector<float> readMv(ID3D11Texture2D* mv) {   // R16G16F -> px (uv * size)
    ID3D11Texture2D* st = tex(DXGI_FORMAT_R16G16_FLOAT, 0, D3D11_USAGE_STAGING, D3D11_CPU_ACCESS_READ, nullptr, 0);
    ctx->CopyResource(st, mv);
    std::vector<float> out(W * H * 2);
    D3D11_MAPPED_SUBRESOURCE m;
    if (SUCCEEDED(ctx->Map(st, 0, D3D11_MAP_READ, 0, &m))) {
        for (int y = 0; y < H; ++y) {
            const uint16_t* r = (const uint16_t*)((const uint8_t*)m.pData + y * m.RowPitch);
            for (int x = 0; x < W * 2; ++x) {
                const uint16_t hbits = r[x];
                const uint32_t s = (hbits & 0x8000u) << 16, e = (hbits >> 10) & 0x1f, fr = hbits & 0x3ff;
                uint32_t f32 = e == 0 ? s : e == 31 ? (s | 0x7f800000u | (fr << 13)) : (s | ((e + 112) << 23) | (fr << 13));
                float v; memcpy(&v, &f32, 4);
                out[y * W * 2 + x] = v * ((x & 1) ? H : W);
            }
        }
        ctx->Unmap(st, 0);
    }
    st->Release();
    return out;
}

struct Err { double mean = 0, p95 = 0; int n = 0; };
static Err score(const std::vector<float>& mv, const std::vector<float>& truth, int x0, int y0, int x1, int y1,
                 const std::vector<uint8_t>& use) {
    std::vector<double> e;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) {
            if (!use[y * W + x]) continue;
            e.push_back(hypot(mv[(y * W + x) * 2] - truth[(y * W + x) * 2], mv[(y * W + x) * 2 + 1] - truth[(y * W + x) * 2 + 1]));
        }
    Err r;
    if (e.empty()) return r;
    std::sort(e.begin(), e.end());
    double s = 0; for (double v : e) s += v;
    r.mean = s / e.size(); r.p95 = e[e.size() * 95 / 100]; r.n = (int)e.size();
    return r;
}

int main() {
    const D3D_FEATURE_LEVEL fl[] = {D3D_FEATURE_LEVEL_11_0};
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, fl, 1, D3D11_SDK_VERSION, &dev, nullptr, &ctx)) &&
        FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, fl, 1, D3D11_SDK_VERSION, &dev, nullptr, &ctx))) {
        printf("no D3D11 device\n"); return 2;
    }
    Scene sc;
    std::vector<uint32_t> cur, prev; double M[3][3];
    images(sc, cur, prev, M);
    ID3D11Texture2D* tc = tex(DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, cur.data(), W * 4);
    ID3D11Texture2D* tp = tex(DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, prev.data(), W * 4);
    ID3D11ShaderResourceView* sc0 = nullptr;
    dev->CreateShaderResourceView(tc, nullptr, &sc0);

    GuideGpu g;
    char why[4096] = "";
    if (!g.init(dev, why, sizeof(why))) { printf("guide init failed: %s\n", why); return 2; }
    GuideParams p;
    p.w = W; p.h = H; p.historyValid = true; p.tanH = p.tanV = (float)kTan;
    for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) p.prevFromCur.m[r][c] = (float)M[r][c];
    bool ok = g.run(dev, ctx, p, why, sizeof(why));
    check(ok, "guide pass (the camera's vectors: a 2.5 deg yaw)", why);
    const std::vector<float> camMv = readMv(g.motion());
    // The truth: the world keeps the camera's vector; the character's texel came from (x + dx, y + dy) in current
    // terms, i.e. its previous place is (x - dx, y - dy); the boat stayed.
    std::vector<float> truth = camMv;
    std::vector<uint8_t> useW(W * H, 0), useN(W * H, 0), useB(W * H, 0), useF(W * H, 0);
    const int b = 12;   // image border (the search reaches past it) and 3 px around every region edge
    for (int y = b; y < H - b; ++y)
        for (int x = b; x < W - b; ++x) {
            const int i = y * W + x;
            if (inRect(x, y, sc.npcX0 + 3, sc.npcY0 + 3, sc.npcX1 - 3, sc.npcY1 - 3)) {
                truth[i * 2] = (float)-sc.npcDx; truth[i * 2 + 1] = (float)-sc.npcDy; useN[i] = 1;
            } else if (inRect(x, y, sc.boatX0 + 3, sc.boatY0 + 3, sc.boatX1 - 3, sc.boatY1 - 3)) {
                truth[i * 2] = 0; truth[i * 2 + 1] = 0; useB[i] = 1;
            } else if (inRect(x, y, sc.flatX0 + 3, sc.flatY0 + 3, sc.flatX1 - 3, sc.flatY1 - 3)) {
                useF[i] = 1;
            } else if (!inRect(x, y, sc.npcX0 - 24, sc.npcY0 - 24, sc.npcX1 + 24, sc.npcY1 + 24) &&
                       !inRect(x, y, sc.boatX0 - 24, sc.boatY0 - 24, sc.boatX1 + 24, sc.boatY1 + 24) &&
                       !inRect(x, y, sc.flatX0 - 24, sc.flatY0 - 24, sc.flatX1 + 24, sc.flatY1 + 24) &&
                       x + camMv[i * 2] > b && x + camMv[i * 2] < W - b) {
                // world, clear of every region, its disocclusion (the camera's 11 px plus a tile) and anything whose
                // previous place is outside the image
                useW[i] = 1;
            }
        }
    const Err cN = score(camMv, truth, 0, 0, W, H, useN), cB = score(camMv, truth, 0, 0, W, H, useB);
    char d[256];
    snprintf(d, sizeof(d), "character mean %.2f px, boat mean %.2f px", cN.mean, cB.mean);
    check(cN.mean > 4.0 && cB.mean > 4.0, "control: the camera's vectors are wrong on the character and the boat", d);

    // Object motion: the eye's previous image first (keep), then the pass on the current one.
    g.keep(dev, ctx, 0, tp);
    GuideGpu::ObjParams op;
    ok = g.objmotion(dev, ctx, 0, sc0, 0, 0, op, why, sizeof(why));
    check(ok, "object motion pass", why);
    const std::vector<float> mv = readMv(g.motion());
    const Err eW = score(mv, truth, 0, 0, W, H, useW), eN = score(mv, truth, 0, 0, W, H, useN),
              eB = score(mv, truth, 0, 0, W, H, useB), eF = score(mv, camMv, 0, 0, W, H, useF);
    if (getenv("OBJM_DEBUG")) {
        int hist[8] = {}; int nb = 0;
        for (int i = 0; i < W * H; ++i) if (useW[i] && hypot(mv[i*2]-truth[i*2], mv[i*2+1]-truth[i*2+1]) > 1) {
            ++nb; hist[(i % W) * 8 / W]++; }
        printf("  world outliers %d by x-eighth: %d %d %d %d %d %d %d %d | cam at centre %.2f,%.2f\n", nb, hist[0], hist[1], hist[2], hist[3], hist[4], hist[5], hist[6], hist[7],
               camMv[(H/2*W+W/2)*2], camMv[(H/2*W+W/2)*2+1]);
        int nh[4] = {};
        for (int i = 0; i < W * H; ++i) if (useN[i] && hypot(mv[i*2]-truth[i*2], mv[i*2+1]-truth[i*2+1]) > 1) {
            const int x = i % W, y = i / W; nh[(x - sc.npcX0) * 2 / (sc.npcX1 - sc.npcX0) + 2 * ((y - sc.npcY0) * 2 / (sc.npcY1 - sc.npcY0))]++; }
        printf("  character misses by quadrant: %d %d %d %d\n", nh[0], nh[1], nh[2], nh[3]);
    }
    snprintf(d, sizeof(d), "mean %.3f px, p95 %.3f px over %d px", eW.mean, eW.p95, eW.n);
    check(eW.mean < 0.05 && eW.p95 < 0.1, "the static world keeps the camera's vectors", d);
    snprintf(d, sizeof(d), "mean %.2f px, p95 %.2f px over %d px (true motion %.0f,%.0f)", eN.mean, eN.p95, eN.n, -sc.npcDx, -sc.npcDy);
    check(eN.mean < 0.5 && eN.p95 < 1.0, "a character moving on its own gets its own vector", d);
    snprintf(d, sizeof(d), "mean %.2f px, p95 %.2f px over %d px", eB.mean, eB.p95, eB.n);
    check(eB.mean < 0.5 && eB.p95 < 1.0, "a boat riding with the camera gets zero motion", d);
    snprintf(d, sizeof(d), "mean change %.3f px over %d px", eF.mean, eF.n);
    check(eF.mean < 0.05, "a flat patch (nothing to match) keeps the camera's vectors", d);

    // A second frame with the same motion uses last frame's tiles as candidates: still right.
    g.run(dev, ctx, p, why, sizeof(why));
    g.keep(dev, ctx, 0, tp);
    ok = g.objmotion(dev, ctx, 0, sc0, 0, 0, op, why, sizeof(why));
    const std::vector<float> mv2 = readMv(g.motion());
    const Err e2 = score(mv2, truth, 0, 0, W, H, useN), e2w = score(mv2, truth, 0, 0, W, H, useW);
    snprintf(d, sizeof(d), "character mean %.2f px, world mean %.3f px", e2.mean, e2w.mean);
    check(ok && e2.mean < 0.5 && e2w.mean < 0.05, "the temporal candidates keep it", d);

    {   // Sub-pixel motion (a real character never moves whole pixels): the +-0.5 refinement must get within half a pixel.
        Scene s2 = sc; s2.npcDx = 2.4; s2.npcDy = -5.7;
        std::vector<uint32_t> c2, p2; double M2[3][3];
        images(s2, c2, p2, M2);
        ID3D11Texture2D* tc2 = tex(DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, c2.data(), W * 4);
        ID3D11Texture2D* tp2 = tex(DXGI_FORMAT_R8G8B8A8_UNORM, D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, p2.data(), W * 4);
        ID3D11ShaderResourceView* sv2 = nullptr;
        dev->CreateShaderResourceView(tc2, nullptr, &sv2);
        g.forget(0);
        g.run(dev, ctx, p, why, sizeof(why));
        g.keep(dev, ctx, 0, tp2);
        ok = g.objmotion(dev, ctx, 0, sv2, 0, 0, op, why, sizeof(why));
        const std::vector<float> mv3 = readMv(g.motion());
        std::vector<float> t3 = truth;
        for (int i = 0; i < W * H; ++i) if (useN[i]) { t3[i * 2] = (float)-s2.npcDx; t3[i * 2 + 1] = (float)-s2.npcDy; }
        const Err e3 = score(mv3, t3, 0, 0, W, H, useN), e3w = score(mv3, t3, 0, 0, W, H, useW);
        snprintf(d, sizeof(d), "character mean %.2f px p95 %.2f px (true -2.4,5.7), world mean %.3f px", e3.mean, e3.p95, e3w.mean);
        check(ok && e3.mean < 0.4 && e3.p95 < 0.75 && e3w.mean < 0.05, "a sub-pixel character motion", d);
        sv2->Release(); tc2->Release(); tp2->Release();
    }
    {   // Cost at the headset's render size (2114x2192, run 8's DLSS Ultra Quality input), timestamped.
        const int RW = 2114, RH = 2192;
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = RW; td.Height = RH; td.MipLevels = td.ArraySize = 1; td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        // Typical: the camera still (identity), a 400x500 character moved (3, -4); worst: every tile disagrees with a
        // turning camera (the previous image is not the turned one).
        std::vector<uint32_t> cb(RW * RH), pbv(RW * RH), pw(RW * RH);
        for (int y = 0; y < RH; ++y)
            for (int x = 0; x < RW; ++x) {
                const bool inC = x >= 800 && x < 1200 && y >= 800 && y < 1300;
                cb[y * RW + x] = rgba(inC ? pattern(x / 3.0, y / 3.0, 5) : pattern(x / 3.0, y / 3.0, 4));
                const bool inP = x - 3 >= 800 && x - 3 < 1200 && y + 4 >= 800 && y + 4 < 1300;   // where it was: (x-3, y+4) now
                pbv[y * RW + x] = rgba(inP ? pattern((x - 3) / 3.0, (y + 4) / 3.0, 5) : pattern(x / 3.0, y / 3.0, 4));
                pw[y * RW + x] = rgba(pattern(x / 3.0, y / 3.0, 4) * 0.98);
            }
        auto mk = [&](const std::vector<uint32_t>& v) {
            D3D11_SUBRESOURCE_DATA sd = {v.data(), (UINT)RW * 4, 0};
            ID3D11Texture2D* t = nullptr; dev->CreateTexture2D(&td, &sd, &t); return t;
        };
        ID3D11Texture2D *bc = mk(cb), *bp = mk(pbv), *bw = mk(pw);
        ID3D11ShaderResourceView* bs = nullptr;
        dev->CreateShaderResourceView(bc, nullptr, &bs);
        GuideGpu g2;
        g2.init(dev, why, sizeof(why));
        D3D11_QUERY_DESC qd = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0}, qt = {D3D11_QUERY_TIMESTAMP, 0};
        ID3D11Query *dj = nullptr, *q0 = nullptr, *q1 = nullptr;
        dev->CreateQuery(&qd, &dj); dev->CreateQuery(&qt, &q0); dev->CreateQuery(&qt, &q1);
        // 20 frames back to back between one pair of timestamps (single spaced-out passes read the GPU's idle clock:
        // the same case measured 7 and 19 ms on consecutive runs). Each frame is the camera vectors then the pass,
        // as in the game (the pass overwrites the vectors, so repeating it alone would find them already right);
        // the camera vectors' own time is measured the same way and subtracted.
        auto timed = [&](const GuideParams& gp, ID3D11Texture2D* prevTex, bool withObj) {
            double total = 0; int n = 0;
            g2.keep(dev, ctx, 0, prevTex);
            for (int it = 0; it < 5; ++it) {
                ctx->Begin(dj); ctx->End(q0);
                for (int r = 0; r < 20; ++r) {
                    g2.run(dev, ctx, gp, why, sizeof(why));
                    if (withObj) g2.objmotion(dev, ctx, 0, bs, 0, 0, op, why, sizeof(why));
                }
                ctx->End(q1); ctx->End(dj);
                D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dd; UINT64 a = 0, bb = 0;
                while (ctx->GetData(dj, &dd, sizeof(dd), 0) != S_OK) {}
                while (ctx->GetData(q0, &a, sizeof(a), 0) != S_OK) {}
                while (ctx->GetData(q1, &bb, sizeof(bb), 0) != S_OK) {}
                if (it >= 2 && !dd.Disjoint) { total += (bb - a) * 1000.0 / dd.Frequency / 20.0; ++n; }
            }
            return n ? total / n : -1.0;
        };
        auto timeIt = [&](const GuideParams& gp, ID3D11Texture2D* prevTex) {
            return timed(gp, prevTex, true) - timed(gp, prevTex, false);
        };
        GuideParams still; still.w = RW; still.h = RH; still.historyValid = true; still.tanH = still.tanV = (float)kTan;
        GuideParams turn = p; turn.w = RW; turn.h = RH;
        timeIt(turn, bw); timeIt(still, bp);   // warm-up: the first case measured reads the GPU's ramping clock (8 ms, then 1.5 ms, for the same work)
        const double typical = timeIt(still, bp), worst = timeIt(turn, bw);
        if (getenv("OBJM_DEBUG")) {
            GuideGpu::ObjParams keepOp = op;
            op.minContrast = 2.0f;   // every tile exits at once: the passes' own overhead
            printf("  overhead with every tile exiting: %.3f ms\n", timeIt(still, bp));
            op = keepOp; op.ratio = -1.0f;   // tiles searched, none wins: the per-pixel pass has nothing to do
            printf("  tiles searched, no winner: %.3f ms\n", timeIt(still, bp));
            op = keepOp; op.minGain = 10.0f;   // every tile exits at the camera match: the static world's cost
            printf("  every tile exits at the camera match: %.3f ms\n", timeIt(still, bp));
            op = keepOp;
        }
        snprintf(d, sizeof(d), "%.3f ms per eye image typical (still camera, one character), %.3f ms worst (every tile disagrees), %dx%d",
                 typical, worst, RW, RH);
        check(typical > 0 && typical < 1.0, "cost at the headset's render size", d);
        dj->Release(); q0->Release(); q1->Release(); bs->Release(); bc->Release(); bp->Release(); bw->Release();
        g2.shutdown();
    }

    printf("dlss objmotion: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
