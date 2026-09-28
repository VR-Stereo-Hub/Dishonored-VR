// core/gfx/afw_warp.cpp - see afw_warp.h.
#define DVR_CAT ::dvr::log::Cat::d3d
#include "core/gfx/afw_warp.h"

#include "core/gfx/clarity.h"
#include "core/gfx/depth_probe.h"
#include "core/util/log.h"

#include <windows.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <atomic>
#include <initializer_list>
#include <cmath>
#include <stdio.h>
#include <string.h>

namespace dvr::afw {
namespace {

typedef HRESULT (WINAPI *PFN_D3DCompile)(LPCVOID, SIZE_T, LPCSTR, const void*, void*, LPCSTR,
                                         LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

// Rows are float4 with w unused. s/f rows map the held/fresh image's view space to tracking space
// (R, not transposed); d rows map tracking space into the target view (R_dst^T); y is the world's
// rotation in tracking space since the held image (Ry, about the head centre yc).
//
// Per target pixel, in order (the first that holds wins):
//   1 STEREO NEAR  the fresh eye (this instant) solved from a near seed: a consistent sample nearer
//                  than the body threshold is the hands/weapon as they are NOW -> no temporal ghost
//   2 TEMPORAL     the held eye's own last image, world hypothesis: a consistent sample at or beyond
//                  the body threshold is the world from this eye's own viewpoint (true stereo,
//                  view-dependent shading intact). The world moves by the GAME's own matrices when
//                  both images carry them (mD.w > 0: head, stick yaw AND walking, the camera-relative
//                  view-projection each image was drawn with, as the DLSS vectors use it), else by
//                  the head change and the body yaw alone (walking lags a tick)
//   3 STEREO FAR   the fresh eye from a far seed: the world where the held image has only an old
//                  hand (the region a moving hand or a turn uncovered)
//   4 last resort  whichever of 2/3 did not land on a near sample
// Without a fresh depth (prm3.x = 0) the temporal body/world split of the first version runs. It
// cannot see behind the old hand: where a turn uncovers the world the hand hid, the world search lands
// on the hand again and a copy of it trails (the host test measures ~26% of the hand at 5 deg; the
// fresh eye's colour cannot fill it without its depth - the fresh eye's own hand sits there).
const char* kSrc =
    "struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
    "VSOut vsmain(uint id : SV_VertexID) {\n"
    "    VSOut o; float2 uv = float2((id << 1) & 2, id & 2);\n"
    "    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0); o.uv = uv; return o;\n"
    "}\n"
    "Texture2D heldTex : register(t0);\n"
    "Texture2D heldDepth : register(t1);\n"
    "Texture2D freshTex : register(t2);\n"
    "Texture2D freshDepth : register(t3);\n"
    "SamplerState linSamp : register(s0);\n"
    "SamplerState pointSamp : register(s1);\n"
    "cbuffer P : register(b0) {\n"
    "    float4 s0, s1, s2, sp;\n"      // held image: R rows, position
    "    float4 f0, f1, f2, fp;\n"      // fresh image: R rows, position
    "    float4 d0, d1, d2, dp;\n"      // target: R^T rows, position
    "    float4 y0, y1, y2, yc;\n"      // world yaw rows, centre
    "    float4 prm;\n"                  // tanH, tanV, metres per depth unit, body threshold (units)
    "    float4 prm2;\n"                 // target w, h, tolerance (target texels), near seed (units)
    "    float4 prm3;\n"                 // stereo on, temporal on, debug tint, second near seed (units)
    "    float4 hA, hB, hW;\n"           // held image: clip x, y, w as linear forms of the camera-relative point
    "    float4 tA, tB, tW;\n"           // the target (the fresh image's matrix at the held eye's camera)
    "    float4 mD;\n"                   // held camera minus target camera (uu), uu per depth unit (0 = matrices off)
    "};\n"
    "float3 viewDir(float2 uv) { float2 n = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0); return float3(n.x * prm.x, n.y * prm.y, -1.0); }\n"
    "float2 toUV(float3 L) { float iz = 1.0 / max(-L.z, 1e-4); float2 n = float2(L.x * iz / prm.x, L.y * iz / prm.y);\n"
    "    return float2(n.x * 0.5 + 0.5, 0.5 - n.y * 0.5); }\n"
    "float3 mr(float4 a, float4 b, float4 c, float3 v) { return float3(dot(a.xyz, v), dot(b.xyz, v), dot(c.xyz, v)); }\n"
    "float3 mc(float4 a, float4 b, float4 c, float3 v) { return a.xyz * v.x + b.xyz * v.y + c.xyz * v.z; }\n"
    "float zH(float2 uv) { float z = heldDepth.SampleLevel(pointSamp, uv, 0).r; return z > 0.0 ? z : 60000.0; }\n"
    "float zF(float2 uv) { float z = freshDepth.SampleLevel(pointSamp, uv, 0).r; return z > 0.0 ? z : 60000.0; }\n"
    "float2 fwdH(float2 s, float z, bool world) {\n"
    "    float3 W = mr(s0, s1, s2, viewDir(s) * (z * prm.z)) + sp.xyz;\n"
    "    if (world) W = mr(y0, y1, y2, W - yc.xyz) + yc.xyz;\n"
    "    return toUV(mr(d0, d1, d2, W - dp.xyz));\n"
    "}\n"
    "float2 fwdF(float2 s, float z) { return toUV(mr(d0, d1, d2, mr(f0, f1, f2, viewDir(s) * (z * prm.z)) + fp.xyz - dp.xyz)); }\n"
    // The game-matrix world: a held pixel's camera-relative point from its NDC and view depth (the
    // DLSS guide's solve), moved to the target camera, projected through the target's matrix.
    "float3 relFrom(float4 A, float4 B, float4 Wr, float2 uv, float w) {\n"
    "    float nx = uv.x * 2.0 - 1.0, ny = 1.0 - uv.y * 2.0;\n"
    "    float3 r0 = A.xyz - nx * Wr.xyz, r1 = B.xyz - ny * Wr.xyz, r2 = Wr.xyz;\n"
    "    float3 rhs = float3(nx * Wr.w - A.w, ny * Wr.w - B.w, w - Wr.w);\n"
    "    float3 c0 = cross(r1, r2), c1 = cross(r2, r0), c2 = cross(r0, r1);\n"
    "    float det = dot(r0, c0);\n"
    "    return (rhs.x * c0 + rhs.y * c1 + rhs.z * c2) / (abs(det) > 1e-20 ? det : 1e-20);\n"
    "}\n"
    "float2 projM(float4 A, float4 B, float4 Wr, float3 P) {\n"
    "    float w = max(dot(Wr.xyz, P) + Wr.w, 1e-3);\n"
    "    return float2(0.5 + 0.5 * (dot(A.xyz, P) + A.w) / w, 0.5 - 0.5 * (dot(B.xyz, P) + B.w) / w);\n"
    "}\n"
    "float2 fwdHw(float2 s, float z, bool world) {\n"
    "    if (world && mD.w > 0.0) return projM(tA, tB, tW, relFrom(hA, hB, hW, s, z * mD.w) + mD.xyz);\n"
    "    return fwdH(s, z, world);\n"
    "}\n"
    // Seeds: the target ray at a depth (units), carried back into a source image. A fixed-point search
    // settles on ONE surface that maps to the target pixel; where a near and a far surface both do (an
    // edge that moved), the seed picks which. So each source is searched from more than one depth and
    // the NEAREST consistent answer wins - the z-buffer rule.
    "float2 seedHw(float2 t, bool world, float zu) {\n"
    "    if (world && mD.w > 0.0) return projM(hA, hB, hW, relFrom(tA, tB, tW, t, zu * mD.w) - mD.xyz);\n"
    "    float3 W = mc(d0, d1, d2, viewDir(t) * (zu * prm.z)) + dp.xyz;\n"
    "    if (world) W = mc(y0, y1, y2, W - yc.xyz) + yc.xyz;\n"
    "    return toUV(mc(s0, s1, s2, W - sp.xyz));\n"
    "}\n"
    "float2 seedF(float2 t, float zu) { float3 W = mc(d0, d1, d2, viewDir(t) * (zu * prm.z)) + dp.xyz; return toUV(mc(f0, f1, f2, W - fp.xyz)); }\n"
    "float2 solveH(float2 t, bool world, float zSeed, out float z, out float err) {\n"
    "    float2 s = saturate(seedHw(t, world, zSeed));\n"
    "    [unroll] for (int i = 0; i < 4; ++i) { z = zH(s); s = saturate(s + (t - fwdHw(s, z, world))); }\n"
    "    z = zH(s); err = length((fwdHw(s, z, world) - t) * prm2.xy); return s;\n"
    "}\n"
    "float2 solveF(float2 t, float zSeed, out float z, out float err) {\n"
    "    float2 s = saturate(seedF(t, zSeed));\n"
    "    [unroll] for (int i = 0; i < 5; ++i) { z = zF(s); s = saturate(s + (t - fwdF(s, z))); }\n"
    "    z = zF(s); err = length((fwdF(s, z) - t) * prm2.xy); return s;\n"
    "}\n"
    // Debug tint: green stereo near (hands now), none temporal, blue stereo far, red last resort,
    // yellow the temporal body hypothesis (no fresh depth).
    "float4 shade(Texture2D tex, float2 uv, int cls) {\n"
    "    float3 c = tex.SampleLevel(linSamp, uv, 0).rgb;\n"
    "    if (prm3.z > 0.5) {\n"
    "        float3 k = cls == 0 ? float3(0.5, 1.0, 0.5) : cls == 2 ? float3(0.5, 0.6, 1.0) : cls == 3 ? float3(1.0, 0.4, 0.4)\n"
    "                 : cls == 4 ? float3(1.0, 1.0, 0.4) : float3(1.0, 1.0, 1.0);\n"
    "        c *= k;\n"
    "    }\n"
    "    return float4(c, 1.0);\n"
    "}\n"
    "float4 psmain(VSOut i) : SV_Target {\n"
    "    float2 t = i.uv;\n"
    "    float tol = prm2.z, body = prm.w;\n"
    "    bool st = prm3.x > 0.5, tp = prm3.y > 0.5;\n"
    "    if (st) {\n"
    "        float zn, en; float2 sn = solveF(t, prm2.w, zn, en);\n"
    "        if (en < tol && zn < body) return shade(freshTex, sn, 0);\n"
    "        sn = solveF(t, prm3.w, zn, en);\n"
    "        if (en < tol && zn < body) return shade(freshTex, sn, 0);\n"
    "    }\n"
    // The world: the fresh eye from a far and a mid seed, the held eye from a far seed and from the
    // fresh eye's depth. Each keeps its nearest consistent world answer; then the nearer of the two
    // sources wins (within 4% they agree and the held eye's own view is kept).
    "    if (tp && st) {\n"
    "        float mid = 1.5 * body, big = 60000.0;\n"
    "        float zf, ef, z2, e2; float2 sf = solveF(t, big, zf, ef), s2 = solveF(t, mid, z2, e2);\n"
    "        bool fOk = ef < tol && zf >= body, f2 = e2 < tol && z2 >= body;\n"
    "        if (f2 && (!fOk || z2 < zf)) { sf = s2; zf = z2; fOk = true; }\n"
    "        float zw, ew, z3, e3; float2 sw = solveH(t, true, big, zw, ew), s3 = solveH(t, true, fOk ? zf : mid, z3, e3);\n"
    "        bool hOk = ew < tol && zw >= body, h3 = e3 < tol && z3 >= body;\n"
    "        if (h3 && (!hOk || z3 < zw)) { sw = s3; zw = z3; hOk = true; }\n"
    "        if (hOk && (!fOk || zw <= zf * 1.04)) return shade(heldTex, sw, 1);\n"
    "        if (fOk) return shade(freshTex, sf, 2);\n"
    "        return zw >= body ? shade(heldTex, sw, 3) : shade(freshTex, sf, 3);\n"
    "    }\n"
    "    if (tp) {\n"
    "        float zb, eb, zw, ew;\n"
    "        float2 sb = solveH(t, false, 60000.0, zb, eb);\n"
    "        if (zb < body && eb < tol) return shade(heldTex, sb, 4);\n"
    "        float2 sw = solveH(t, true, 60000.0, zw, ew);\n"
    "        return shade(heldTex, sw, 1);\n"
    "    }\n"
    "    float zf, ef; float2 sf = solveF(t, 60000.0, zf, ef);\n"
    "    return shade(freshTex, sf, ef < tol ? 2 : 3);\n"
    "}\n"
    // The depth snapshot: the scene target's alpha (linear view depth) into the eye's own R16F.
    "float psdepth(VSOut i) : SV_Target { return heldTex.Load(int3(i.pos.xy, 0)).a; }\n";

struct Held {
    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    uint32_t w = 0, h = 0, fmt = 0;
    uint32_t serial = 0;
    uint64_t seq = 0;                        // capture order; the fresh eye is the latest
    Pose pose{};
    bool bodyOk = false;
    float bodyYaw = 0;
    Pose targets[2]{};
    bool valid = false;
    float vp[16] = {};                       // the camera-relative world view-projection it was drawn with
    float c5[3] = {};                        // its rendered c5 (the NEGATIVE camera position, uu)
    bool vpOk = false;
    // Its own depth, taken from the shared ring at capture (the ring moves on; this does not).
    ID3D11Texture2D* dtex = nullptr;
    ID3D11ShaderResourceView* dsrv = nullptr;
    ID3D11RenderTargetView* drtv = nullptr;
    uint32_t dw = 0, dh = 0;
    bool depthOk = false;
};
Held g_held[2];
uint64_t g_seq = 0;

std::atomic<bool> g_on{false}, g_stereo{true}, g_debug{false}, g_matrices{true};
std::atomic<float> g_bodyDepth{0.40f};
std::atomic<float> g_worldScale{100.0f};
bool g_ready = false, g_failed = false;
ID3D11VertexShader* g_vs = nullptr;
ID3D11PixelShader* g_ps = nullptr;
ID3D11PixelShader* g_psDepth = nullptr;
ID3D11Buffer* g_cb = nullptr;
ID3D11SamplerState* g_lin = nullptr;
ID3D11SamplerState* g_point = nullptr;
ID3D11RasterizerState* g_rs = nullptr;
ID3D11BlendState* g_bs = nullptr;
ID3D11DepthStencilState* g_ds = nullptr;

// Counters for the beat (render thread only).
uint32_t g_warpsFull = 0, g_warpsTemporal = 0, g_warpsStereo = 0;
uint32_t g_noHeld = 0, g_notFresh = 0, g_noDepth = 0, g_noRtv = 0, g_notReady = 0;
uint32_t g_snaps = 0, g_snapLate = 0, g_snapMiss = 0;
uint32_t g_mtxUsed = 0, g_mtxNoVp = 0, g_mtxTurnRefused = 0, g_mtxEyeRefused = 0;
float g_mtxTurnMax = 0, g_mtxEyeMax = 0;
double g_yawAbs = 0; float g_yawMax = 0;
uint64_t g_beatMs = 0;

// GPU time of the warp draw: a small ring of timestamp sets, read without waiting.
struct Ts { ID3D11Query* dis = nullptr; ID3D11Query* a = nullptr; ID3D11Query* b = nullptr; bool pending = false; };
const int kTs = 6;
Ts g_ts[kTs];
bool g_tsOk = false;
double g_gpuSum = 0; float g_gpuMax = 0; uint32_t g_gpuN = 0;

template <class T> void rel(T*& p) { if (p) { p->Release(); p = nullptr; } }

DXGI_FORMAT typed(DXGI_FORMAT f) {
    switch (f) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
        case DXGI_FORMAT_R10G10B10A2_TYPELESS: return DXGI_FORMAT_R10G10B10A2_UNORM;
        case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        default: return f;
    }
}

// The pipeline state these passes touch, saved and put back: the passes run inside the runtime's
// submit path, and nothing after them should inherit a warp's bindings.
struct Saved {
    ID3D11RenderTargetView* rtv = nullptr; ID3D11DepthStencilView* dsv = nullptr;
    D3D11_VIEWPORT vp[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]; UINT nvp = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    ID3D11RasterizerState* rs = nullptr;
    ID3D11BlendState* bs = nullptr; float bf[4] = {}; UINT sm = 0;
    ID3D11DepthStencilState* ds = nullptr; UINT sref = 0;
    ID3D11InputLayout* il = nullptr; D3D11_PRIMITIVE_TOPOLOGY topo = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
    ID3D11VertexShader* vs = nullptr; ID3D11PixelShader* ps = nullptr;
    ID3D11Buffer* cb = nullptr;
    ID3D11ShaderResourceView* srv[4] = {};
    ID3D11SamplerState* samp[2] = {};
    void save(ID3D11DeviceContext* c) {
        c->OMGetRenderTargets(1, &rtv, &dsv);
        c->RSGetViewports(&nvp, vp);
        c->RSGetState(&rs);
        c->OMGetBlendState(&bs, bf, &sm);
        c->OMGetDepthStencilState(&ds, &sref);
        c->IAGetInputLayout(&il);
        c->IAGetPrimitiveTopology(&topo);
        c->VSGetShader(&vs, nullptr, nullptr);
        c->PSGetShader(&ps, nullptr, nullptr);
        c->PSGetConstantBuffers(0, 1, &cb);
        c->PSGetShaderResources(0, 4, srv);
        c->PSGetSamplers(0, 2, samp);
    }
    void restore(ID3D11DeviceContext* c) {
        c->OMSetRenderTargets(1, &rtv, dsv);
        c->RSSetViewports(nvp, vp);
        c->RSSetState(rs);
        c->OMSetBlendState(bs, bf, sm);
        c->OMSetDepthStencilState(ds, sref);
        c->IASetInputLayout(il);
        c->IASetPrimitiveTopology(topo);
        c->VSSetShader(vs, nullptr, 0);
        c->PSSetShader(ps, nullptr, 0);
        c->PSSetConstantBuffers(0, 1, &cb);
        c->PSSetShaderResources(0, 4, srv);
        c->PSSetSamplers(0, 2, samp);
        rel(rtv); rel(dsv); rel(rs); rel(bs); rel(ds); rel(il); rel(vs); rel(ps); rel(cb);
        for (auto*& s : srv) rel(s);
        for (auto*& s : samp) rel(s);
    }
};

bool init(ID3D11Device* dev) {
    if (g_ready) return true;
    if (g_failed || !dev) return false;
    HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
    PFN_D3DCompile compile = compiler ? (PFN_D3DCompile)GetProcAddress(compiler, "D3DCompile") : nullptr;
    if (!compile) { g_failed = true; DVR_ERROR("afw/warp: d3dcompiler_47.dll missing - the held eye stays rotation-only"); return false; }
    ID3DBlob *vsb = nullptr, *psb = nullptr, *pdb = nullptr, *err = nullptr;
    if (FAILED(compile(kSrc, strlen(kSrc), nullptr, nullptr, nullptr, "vsmain", "vs_4_0", 0, 0, &vsb, &err)) ||
        FAILED(compile(kSrc, strlen(kSrc), nullptr, nullptr, nullptr, "psmain", "ps_4_0", 0, 0, &psb, &err)) ||
        FAILED(compile(kSrc, strlen(kSrc), nullptr, nullptr, nullptr, "psdepth", "ps_4_0", 0, 0, &pdb, &err))) {
        g_failed = true;
        DVR_ERROR("afw/warp: shader compile failed: %s - the held eye stays rotation-only",
                  err ? (const char*)err->GetBufferPointer() : "?");
        rel(err); rel(vsb); rel(psb); rel(pdb);
        return false;
    }
    HRESULT hr = dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &g_vs);
    if (SUCCEEDED(hr)) hr = dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &g_ps);
    if (SUCCEEDED(hr)) hr = dev->CreatePixelShader(pdb->GetBufferPointer(), pdb->GetBufferSize(), nullptr, &g_psDepth);
    rel(vsb); rel(psb); rel(pdb);
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = 26 * 16;   // twenty-six float4s: s f d y (4 each), prm prm2 prm3, hA hB hW tA tB tW mD
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (SUCCEEDED(hr)) hr = dev->CreateBuffer(&bd, nullptr, &g_cb);
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (SUCCEEDED(hr)) hr = dev->CreateSamplerState(&sd, &g_lin);
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    if (SUCCEEDED(hr)) hr = dev->CreateSamplerState(&sd, &g_point);
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    if (SUCCEEDED(hr)) hr = dev->CreateRasterizerState(&rd, &g_rs);
    D3D11_BLEND_DESC bl = {};
    bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (SUCCEEDED(hr)) hr = dev->CreateBlendState(&bl, &g_bs);
    D3D11_DEPTH_STENCIL_DESC dd = {};
    dd.DepthEnable = FALSE;
    if (SUCCEEDED(hr)) hr = dev->CreateDepthStencilState(&dd, &g_ds);
    if (FAILED(hr)) {
        g_failed = true;
        DVR_ERROR("afw/warp: D3D11 objects failed (0x%08lx) - the held eye stays rotation-only", (unsigned long)hr);
        return false;
    }
    // GPU timing is an instrument, never a condition: a refusal only blanks the ms on the beat.
    g_tsOk = true;
    D3D11_QUERY_DESC qd = {D3D11_QUERY_TIMESTAMP_DISJOINT, 0}, qt = {D3D11_QUERY_TIMESTAMP, 0};
    for (Ts& s : g_ts)
        if (FAILED(dev->CreateQuery(&qd, &s.dis)) || FAILED(dev->CreateQuery(&qt, &s.a)) || FAILED(dev->CreateQuery(&qt, &s.b)))
            g_tsOk = false;
    g_ready = true;
    DVR_INFO("afw/warp: ready - the held eye is rebuilt each present: the hands/weapon (nearer than %.2f depth units) "
             "from the FRESH eye at this instant, the world from the held eye's own last image carried by the head "
             "change and the body yaw, the uncovered world from the fresh eye (GPU timing %s)",
             g_bodyDepth.load(), g_tsOk ? "on" : "unavailable");
    return true;
}

void rows(const Pose& p, float out[3][4], bool transpose) {
    const float x = p.q[0], y = p.q[1], z = p.q[2], w = p.q[3];
    const float m[3][3] = {
        {1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)},
        {2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)},
        {2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)}};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) out[r][c] = transpose ? m[c][r] : m[r][c];
        out[r][3] = 0;
    }
}

void setup_draw(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* rtv, uint32_t w, uint32_t h, ID3D11PixelShader* ps) {
    D3D11_VIEWPORT vp = {0, 0, (float)w, (float)h, 0, 1};
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(g_rs);
    const float bf[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(g_bs, bf, 0xffffffff);
    ctx->OMSetDepthStencilState(g_ds, 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(g_vs, nullptr, 0);
    ctx->PSSetShader(ps, nullptr, 0);
    ID3D11SamplerState* samps[2] = {g_lin, g_point};
    ctx->PSSetSamplers(0, 2, samps);
}

// Take this eye's depth out of the shared ring into its own R16F while the ring still holds it.
bool snapshot_depth(ID3D11Device* dev, ID3D11DeviceContext* ctx, Held& h) {
    UINT dw = 0, dh = 0;
    ID3D11ShaderResourceView* src = dvr::depthprobe::depth_srv_for(h.serial, &dw, &dh);
    if (!src || !dw || !dh) { dvr::depthprobe::read_done(ctx); return false; }
    if (!h.dtex || h.dw != dw || h.dh != dh) {
        rel(h.drtv); rel(h.dsrv); rel(h.dtex);
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = dw; td.Height = dh; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R16_FLOAT;
        td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        if (FAILED(dev->CreateTexture2D(&td, nullptr, &h.dtex)) || FAILED(dev->CreateShaderResourceView(h.dtex, nullptr, &h.dsrv)) ||
            FAILED(dev->CreateRenderTargetView(h.dtex, nullptr, &h.drtv))) {
            rel(h.drtv); rel(h.dsrv); rel(h.dtex); h.dw = h.dh = 0;
            dvr::depthprobe::read_done(ctx);
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000, "afw/warp: the per-eye depth copy %ux%u R16F was refused", dw, dh);
            return false;
        }
        h.dw = dw; h.dh = dh;
        DVR_INFO("afw/warp: per-eye depth copy %ux%u R16F (the ring moves on each present; this stays with its image)", dw, dh);
    }
    Saved sv; sv.save(ctx);
    setup_draw(ctx, h.drtv, dw, dh, g_psDepth);
    ID3D11ShaderResourceView* srvs[4] = {src, nullptr, nullptr, nullptr};
    ctx->PSSetShaderResources(0, 4, srvs);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none[4] = {};
    ctx->PSSetShaderResources(0, 4, none);
    sv.restore(ctx);
    dvr::depthprobe::read_done(ctx);
    h.depthOk = true;
    return true;
}

// ---- the game's matrices on the CPU: the same solve the shader runs, for the per-present checks ----
struct Lin { double A[4], B[4], W[4]; };
Lin lin_of(const float* vp) {   // row-vector: clip_i = sum_j P_j * M[j][i] + M[3][i]; columns 0, 1, 3
    Lin l;
    for (int j = 0; j < 4; ++j) { l.A[j] = vp[j * 4 + 0]; l.B[j] = vp[j * 4 + 1]; l.W[j] = vp[j * 4 + 3]; }
    return l;
}
void cross3(const double a[3], const double b[3], double o[3]) {
    o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0];
}
double dot3(const double a[3], const double b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
bool rel_from(const Lin& l, double nx, double ny, double w, double P[3]) {
    double r0[3], r1[3], r2[3], c0[3], c1[3], c2[3];
    for (int k = 0; k < 3; ++k) { r0[k] = l.A[k] - nx * l.W[k]; r1[k] = l.B[k] - ny * l.W[k]; r2[k] = l.W[k]; }
    const double rhs[3] = {nx * l.W[3] - l.A[3], ny * l.W[3] - l.B[3], w - l.W[3]};
    cross3(r1, r2, c0); cross3(r2, r0, c1); cross3(r0, r1, c2);
    const double det = dot3(r0, c0);
    if (!(fabs(det) > 1e-20)) return false;
    for (int k = 0; k < 3; ++k) P[k] = (rhs[0] * c0[k] + rhs[1] * c1[k] + rhs[2] * c2[k]) / det;
    return true;
}
bool proj(const Lin& l, const double P[3], double* nx, double* ny) {
    const double w = dot3(l.W, P) + l.W[3];
    if (!(w > 1e-3)) return false;
    *nx = (dot3(l.A, P) + l.A[3]) / w; *ny = (dot3(l.B, P) + l.B[3]) / w;
    return true;
}
// Rotate by a pose's rotation (R v) or its inverse (R^T v).
void rot(const Pose& p, const double v[3], double o[3], bool inverse) {
    float m[3][4]; rows(p, m, inverse);
    for (int r = 0; r < 3; ++r) o[r] = m[r][0] * v[0] + m[r][1] * v[1] + m[r][2] * v[2];
}
double angle_deg(double ax, double ay, double bx, double by, double tanH, double tanV) {
    const double a[3] = {ax * tanH, ay * tanV, -1}, b[3] = {bx * tanH, by * tanV, -1};
    const double c = dot3(a, b) / sqrt(dot3(a, a) * dot3(b, b));
    return acos(c > 1 ? 1 : c < -1 ? -1 : c) * 57.29577951;
}

void poll_timestamps(ID3D11DeviceContext* ctx) {
    if (!g_tsOk) return;
    for (Ts& s : g_ts) {
        if (!s.pending) continue;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dj = {};
        UINT64 a = 0, b = 0;
        if (ctx->GetData(s.dis, &dj, sizeof(dj), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) continue;
        if (ctx->GetData(s.a, &a, sizeof(a), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) continue;
        if (ctx->GetData(s.b, &b, sizeof(b), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) continue;
        s.pending = false;
        if (dj.Disjoint || !dj.Frequency || b < a) continue;
        const float ms = (float)((double)(b - a) * 1000.0 / (double)dj.Frequency);
        g_gpuSum += ms; ++g_gpuN;
        if (ms > g_gpuMax) g_gpuMax = ms;
    }
}

void beat() {
    const uint64_t now = GetTickCount64();
    if (!g_beatMs) { g_beatMs = now; return; }
    if (now - g_beatMs < 3000) return;
    g_beatMs = now;
    const uint32_t warps = g_warpsFull + g_warpsTemporal + g_warpsStereo;
    if (warps + g_noHeld + g_notFresh + g_noDepth + g_noRtv + g_notReady) {
        char gpu[64] = "n/a";
        if (g_gpuN) _snprintf_s(gpu, sizeof(gpu), _TRUNCATE, "%.3f ms mean, %.3f max", g_gpuSum / g_gpuN, g_gpuMax);
        DVR_INFO("afw/warp: beat %u held-eye rebuilds - full %u (hands from the fresh eye, world from the held eye), "
                 "temporal only %u (no fresh depth: the hands carried by the body hypothesis, they can ghost), stereo only "
                 "%u (no held depth) | mean |yaw| %.3f deg, max %.3f | NOT rebuilt (each falls back to the rotation-only "
                 "held eye, a visible pop): no held image %u, fresh eye not captured this present %u, no depth %u, no "
                 "render target %u, not ready %u | depth copies %u (%u taken late at the warp, %u missed: the ring had "
                 "moved on or its copy was not finished) | world by the GAME matrices %u (walking carried), by the "
                 "XR pose + body yaw %u: no matrices %u, refused %u by the turn check (worst %.2f deg) and %u by the "
                 "eye check (worst %.2f deg; each check compares the two models where they must agree) | GPU %s | "
                 "body < %.2f units, %.3f m per unit%s",
                 warps, g_warpsFull, g_warpsTemporal, g_warpsStereo, warps ? g_yawAbs / warps : 0.0, g_yawMax,
                 g_noHeld, g_notFresh, g_noDepth, g_noRtv, g_notReady, g_snaps, g_snapLate, g_snapMiss,
                 g_mtxUsed, g_mtxNoVp + g_mtxTurnRefused + g_mtxEyeRefused, g_mtxNoVp, g_mtxTurnRefused, g_mtxTurnMax,
                 g_mtxEyeRefused, g_mtxEyeMax, gpu,
                 g_bodyDepth.load(), dvr::clarity::depth_scale() / g_worldScale.load(),
                 g_stereo.load() ? "" : " | stereo source OFF (afw stereo off)");
    }
    g_warpsFull = g_warpsTemporal = g_warpsStereo = 0;
    g_noHeld = g_notFresh = g_noDepth = g_noRtv = g_notReady = 0;
    g_snaps = g_snapLate = g_snapMiss = 0;
    g_mtxUsed = g_mtxNoVp = g_mtxTurnRefused = g_mtxEyeRefused = 0;
    g_mtxTurnMax = g_mtxEyeMax = 0;
    g_yawAbs = 0; g_yawMax = 0;
    g_gpuSum = 0; g_gpuMax = 0; g_gpuN = 0;
}

} // namespace

void set_enabled(bool on, const char* who) {
    if (g_on.exchange(on) == on) return;
    DVR_INFO("afw/warp: %s (%s)%s", on ? "ON" : "off", who ? who : "?",
             on ? "" : " - the held eye is the compositor's rotation-only reprojection (plus the body-yaw pose)");
}
bool enabled() { return g_on.load(); }
void set_stereo(bool on, const char* who) {
    g_stereo.store(on);
    DVR_INFO("afw/warp: stereo source %s (%s)%s", on ? "ON" : "OFF", who ? who : "?",
             on ? " - the hands come from the fresh eye at this instant" : " - the first version: the held eye alone, "
             "hands by the body hypothesis (a moving hand ghosts)");
}
bool stereo() { return g_stereo.load(); }
void set_debug(bool on, const char* who) {
    g_debug.store(on);
    DVR_INFO("afw/warp: debug tint %s (%s)%s", on ? "ON" : "off", who ? who : "?",
             on ? " - in the HELD eye only: green = hands from the fresh eye, untinted = the held eye's own world, "
                  "blue = world from the fresh eye (uncovered), red = last resort, yellow = temporal body hypothesis" : "");
}
bool debug() { return g_debug.load(); }
void set_matrices(bool on, const char* who) {
    g_matrices.store(on);
    DVR_INFO("afw/warp: world by the game's matrices %s (%s)%s", on ? "ON" : "OFF", who ? who : "?",
             on ? " - head, stick yaw and walking, checked each present against the XR pose model"
                : " - the XR pose and the body yaw alone: the held eye's world lags a tick of walking");
}
bool matrices() { return g_matrices.load(); }
void set_body_depth(float units, const char* who) {
    if (!(units > 0.0f && units < 5.0f)) { DVR_WARN("afw/warp: body depth %.3f refused (0..5 units)", units); return; }
    g_bodyDepth.store(units);
    DVR_INFO("afw/warp: body depth %.2f units (%s) - nearer pixels are the hands and weapon", units, who ? who : "?");
}
float body_depth() { return g_bodyDepth.load(); }
void set_world_scale(float uuPerM) { if (uuPerM >= 1.0f && uuPerM <= 400.0f) g_worldScale.store(uuPerM); }

void note_capture(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11Texture2D* frame,
                  uint32_t grabSerial, const Pose& pose, bool bodyOk, float bodyYawDeg, const Pose targets[2],
                  const float* vp16, const float* c5) {
    if (!g_on.load() || eye < 0 || eye > 1 || !dev || !ctx || !frame) return;
    if (!init(dev)) return;
    Held& h = g_held[eye];
    D3D11_TEXTURE2D_DESC fd;
    frame->GetDesc(&fd);
    if (!h.tex || h.w != fd.Width || h.h != fd.Height || h.fmt != (uint32_t)fd.Format) {
        rel(h.srv); rel(h.tex); h.valid = false;
        D3D11_TEXTURE2D_DESC td = fd;
        td.MipLevels = 1; td.ArraySize = 1; td.SampleDesc.Count = 1; td.SampleDesc.Quality = 0;
        td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE; td.CPUAccessFlags = 0; td.MiscFlags = 0;
        if (FAILED(dev->CreateTexture2D(&td, nullptr, &h.tex))) { h.tex = nullptr; return; }
        D3D11_SHADER_RESOURCE_VIEW_DESC sv = {};
        sv.Format = typed(fd.Format);
        sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sv.Texture2D.MipLevels = 1;
        if (FAILED(dev->CreateShaderResourceView(h.tex, &sv, &h.srv))) { rel(h.tex); return; }
        h.w = fd.Width; h.h = fd.Height; h.fmt = (uint32_t)fd.Format;
        DVR_INFO("afw/warp: eye %c image copy %ux%u fmt %u", eye ? 'R' : 'L', h.w, h.h, h.fmt);
    }
    ctx->CopyResource(h.tex, frame);
    h.serial = grabSerial; h.pose = pose; h.bodyOk = bodyOk; h.bodyYaw = bodyYawDeg;
    h.targets[0] = targets[0]; h.targets[1] = targets[1];
    h.vpOk = vp16 && c5;
    if (h.vpOk) { memcpy(h.vp, vp16, sizeof(h.vp)); memcpy(h.c5, c5, sizeof(h.c5)); }
    h.valid = true;
    h.seq = ++g_seq;
    h.depthOk = false;
    if (snapshot_depth(dev, ctx, h)) ++g_snaps;   // a miss is retried at the warp, this same present
}

namespace {
// The held image's world carried to the target by the GAME's matrices: the camera-relative view-projection
// each image was drawn with and its rendered camera, the pair the DLSS vectors were verified with. The target
// camera is the fresh camera moved by the XR offset between the fresh eye and the held eye (the fresh eye's
// view axes, the world scale). Two checks where the two models must agree, each able to refuse:
//   turn  the far world (direction only, so walking cannot enter): the matrices against the XR poses and
//         the body yaw - a rotation or axis mistake shows here
//   eye   the same instant at half a metre: the target camera's offset against the XR eye poses - a wrong
//         sign, axis or scale of the eye offset shows here
// A refusal leaves mD.w = 0: the shader's XR pose + body yaw world.
void matrix_world(const Held& src, const Held& fr, const Pose& tgt, int held, float yawDeg, float tanH, float tanV,
                  float* hA, float* hB, float* hW, float* tA, float* tB, float* tW, float* mD) {
    mD[3] = 0;
    if (!g_matrices.load()) return;
    if (!src.vpOk || !fr.vpOk) { ++g_mtxNoVp; return; }
    const Lin lh = lin_of(src.vp), lt = lin_of(fr.vp);
    const float scale = g_worldScale.load();
    // The fresh camera's world axes from its matrix: w is the forward depth; x and y are right and up
    // (Gram-Schmidt against forward, which also drops a projection jitter).
    double fwd[3] = {lt.W[0], lt.W[1], lt.W[2]}, right[3] = {lt.A[0], lt.A[1], lt.A[2]}, up[3] = {lt.B[0], lt.B[1], lt.B[2]};
    const double fl = sqrt(dot3(fwd, fwd));
    if (!(fl > 1e-9)) { ++g_mtxNoVp; return; }
    for (double& v : fwd) v /= fl;
    const double ra = dot3(right, fwd), ua = dot3(up, fwd);
    for (int k = 0; k < 3; ++k) { right[k] -= ra * fwd[k]; up[k] -= ua * fwd[k]; }
    const double rl = sqrt(dot3(right, right)), ul = sqrt(dot3(up, up));
    if (!(rl > 1e-9) || !(ul > 1e-9)) { ++g_mtxNoVp; return; }
    for (int k = 0; k < 3; ++k) { right[k] /= rl; up[k] /= ul; }
    // The eye offset: tracking (fresh eye -> held eye) into the fresh XR view, then into the game world.
    const double de[3] = {tgt.p[0] - fr.pose.p[0], tgt.p[1] - fr.pose.p[1], tgt.p[2] - fr.pose.p[2]};
    double ev[3]; rot(fr.pose, de, ev, true);
    double O[3];
    for (int k = 0; k < 3; ++k) O[k] = scale * (-ev[2] * fwd[k] + ev[0] * right[k] + ev[1] * up[k]);
    // Check 1, the turn: far directions through both models.
    const double pts[5][2] = {{0, 0}, {0.5, 0}, {-0.5, 0}, {0, 0.5}, {0, -0.5}};
    const float a = yawDeg / 57.29578f, ca = cosf(a), sa = sinf(a);
    double turnWorst = 0, eyeWorst = 0;
    for (const auto& pt : pts) {
        const double v[3] = {pt[0] * tanH, pt[1] * tanV, -1};
        double dT[3]; rot(tgt, v, dT, false);
        const double dW[3] = {ca * dT[0] - sa * dT[2], dT[1], sa * dT[0] + ca * dT[2]};   // Ry^T
        double dS[3]; rot(src.pose, dW, dS, true);
        if (!(dS[2] < -1e-6)) { turnWorst = 180; break; }
        double P[3], mx, my;
        if (!rel_from(lt, pt[0], pt[1], 1.0e7, P) || !proj(lh, P, &mx, &my)) { turnWorst = 180; break; }
        const double t = angle_deg(dS[0] / -dS[2] / tanH, dS[1] / -dS[2] / tanV, mx, my, tanH, tanV);
        if (t > turnWorst) turnWorst = t;
    }
    // Check 2, the eye: the same instant, half a metre out, the target seen from the fresh eye.
    if (turnWorst <= 0.5)
        for (const auto& pt : pts) {
            const double v[3] = {pt[0] * tanH * 0.5, pt[1] * tanV * 0.5, -0.5};
            double wt[3]; rot(tgt, v, wt, false);
            const double rel[3] = {wt[0] + tgt.p[0] - fr.pose.p[0], wt[1] + tgt.p[1] - fr.pose.p[1], wt[2] + tgt.p[2] - fr.pose.p[2]};
            double lf[3]; rot(fr.pose, rel, lf, true);
            if (!(lf[2] < -1e-6)) { eyeWorst = 180; break; }
            double P[3], mx, my;
            if (!rel_from(lt, pt[0], pt[1], 0.5 * scale, P)) { eyeWorst = 180; break; }
            for (int k = 0; k < 3; ++k) P[k] += O[k];
            if (!proj(lt, P, &mx, &my)) { eyeWorst = 180; break; }
            const double e = angle_deg(lf[0] / -lf[2] / tanH, lf[1] / -lf[2] / tanV, mx, my, tanH, tanV);
            if (e > eyeWorst) eyeWorst = e;
        }
    if (turnWorst > g_mtxTurnMax) g_mtxTurnMax = (float)turnWorst;
    if (turnWorst > 0.5) { ++g_mtxTurnRefused; return; }
    if (eyeWorst > g_mtxEyeMax) g_mtxEyeMax = (float)eyeWorst;
    if (eyeWorst > 0.5) { ++g_mtxEyeRefused; return; }
    for (int j = 0; j < 4; ++j) {
        hA[j] = (float)lh.A[j]; hB[j] = (float)lh.B[j]; hW[j] = (float)lh.W[j];
        tA[j] = (float)lt.A[j]; tB[j] = (float)lt.B[j]; tW[j] = (float)lt.W[j];
    }
    // held camera minus target camera: C = -c5; target = fresh camera + O.
    for (int k = 0; k < 3; ++k) mD[k] = (float)((-src.c5[k]) - ((-fr.c5[k]) + O[k]));
    mD[3] = dvr::clarity::depth_scale();
    ++g_mtxUsed;
    (void)held;
}
} // namespace

bool warp_held(ID3D11Device* dev, ID3D11DeviceContext* ctx, int held, int fresh, ID3D11Texture2D* dst,
               uint32_t w, uint32_t h, float tanH, float tanV, Pose* outPose, const char** why) {
    beat();
    if (why) *why = nullptr;
    if (!g_on.load()) { if (why) *why = "off"; return false; }
    if (!init(dev)) { ++g_notReady; if (why) *why = "not ready"; return false; }
    if (held < 0 || held > 1 || fresh != 1 - held || !dst || !ctx) { if (why) *why = "bad call"; return false; }
    poll_timestamps(ctx);
    Held& src = g_held[held];
    Held& fr = g_held[fresh];
    if (!src.valid || !fr.valid || !src.srv || !fr.srv) { ++g_noHeld; if (why) *why = "no held image"; return false; }
    if (fr.seq != g_seq) { ++g_notFresh; if (why) *why = "the fresh eye was not captured this present"; return false; }
    for (Held* e : {&fr, &src})
        if (!e->depthOk) { if (snapshot_depth(dev, ctx, *e)) { ++g_snaps; ++g_snapLate; } else ++g_snapMiss; }
    const bool useS = g_stereo.load() && fr.depthOk && fr.dsrv;
    const bool useT = src.depthOk && src.dsrv;
    if (!useS && !useT) { ++g_noDepth; if (why) *why = "no depth for either image"; return false; }
    D3D11_TEXTURE2D_DESC dd;
    dst->GetDesc(&dd);
    D3D11_RENDER_TARGET_VIEW_DESC rv = {};
    rv.Format = typed(dd.Format);
    rv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    ID3D11RenderTargetView* rtv = nullptr;
    if (FAILED(dev->CreateRenderTargetView(dst, &rv, &rtv)) || !rtv) {
        ++g_noRtv; if (why) *why = "no render target"; return false;
    }
    // The target: the held eye's view pose of the FRESH image's locate generation.
    const Pose& tgt = fr.targets[held];
    float d = (src.bodyOk && fr.bodyOk) ? fr.bodyYaw - src.bodyYaw : 0.0f;
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    struct CB {
        float s[3][4]; float sp[4]; float f[3][4]; float fp[4]; float d[3][4]; float dp[4];
        float y[3][4]; float yc[4]; float prm[4]; float prm2[4]; float prm3[4];
        float hA[4], hB[4], hW[4], tA[4], tB[4], tW[4], mD[4];
    } cb;
    static_assert(sizeof(CB) == 26 * 16, "afw cbuffer layout");
    rows(src.pose, cb.s, false);
    cb.sp[0] = src.pose.p[0]; cb.sp[1] = src.pose.p[1]; cb.sp[2] = src.pose.p[2]; cb.sp[3] = 0;
    rows(fr.pose, cb.f, false);
    cb.fp[0] = fr.pose.p[0]; cb.fp[1] = fr.pose.p[1]; cb.fp[2] = fr.pose.p[2]; cb.fp[3] = 0;
    rows(tgt, cb.d, true);
    cb.dp[0] = tgt.p[0]; cb.dp[1] = tgt.p[1]; cb.dp[2] = tgt.p[2]; cb.dp[3] = 0;
    // UE yaw turns right positive; about XR +Y a positive angle turns LEFT. The world content the body
    // turned away from lies to the left of the fresh view: Ry(+d) (the runtime's pose fallback agrees).
    const float a = d / 57.29578f, ca = cosf(a), sa = sinf(a);
    const float yr[3][4] = {{ca, 0, sa, 0}, {0, 1, 0, 0}, {-sa, 0, ca, 0}};
    memcpy(cb.y, yr, sizeof(yr));
    cb.yc[0] = 0.5f * (fr.targets[0].p[0] + fr.targets[1].p[0]);
    cb.yc[1] = 0.5f * (fr.targets[0].p[1] + fr.targets[1].p[1]);
    cb.yc[2] = 0.5f * (fr.targets[0].p[2] + fr.targets[1].p[2]);
    cb.yc[3] = 0;
    const float body = g_bodyDepth.load();
    cb.prm[0] = tanH; cb.prm[1] = tanV;
    cb.prm[2] = dvr::clarity::depth_scale() / g_worldScale.load();
    cb.prm[3] = body;
    // Two near seeds for the fresh eye's search: the hands at arm's length and the weapon held close.
    // A single far seed would settle on the background beside a near object and never see it.
    cb.prm2[0] = (float)w; cb.prm2[1] = (float)h; cb.prm2[2] = 1.5f; cb.prm2[3] = 0.5f * body;
    cb.prm3[0] = useS ? 1.0f : 0.0f; cb.prm3[1] = useT ? 1.0f : 0.0f; cb.prm3[2] = g_debug.load() ? 1.0f : 0.0f;
    cb.prm3[3] = 0.2f * body;
    memset(cb.hA, 0, sizeof(float) * 28);
    if (useT) matrix_world(src, fr, tgt, held, d, tanH, tanV, &cb.hA[0], &cb.hB[0], &cb.hW[0], &cb.tA[0], &cb.tB[0], &cb.tW[0], cb.mD);
    ctx->UpdateSubresource(g_cb, 0, nullptr, &cb, 0, 0);

    Ts* ts = nullptr;
    if (g_tsOk) for (Ts& s : g_ts) if (!s.pending) { ts = &s; break; }
    Saved sv; sv.save(ctx);
    if (ts) { ctx->Begin(ts->dis); ctx->End(ts->a); }
    setup_draw(ctx, rtv, w, h, g_ps);
    ctx->PSSetConstantBuffers(0, 1, &g_cb);
    ID3D11ShaderResourceView* srvs[4] = {src.srv, useT ? src.dsrv : nullptr, fr.srv, useS ? fr.dsrv : nullptr};
    ctx->PSSetShaderResources(0, 4, srvs);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none[4] = {};
    ctx->PSSetShaderResources(0, 4, none);
    if (ts) { ctx->End(ts->b); ctx->End(ts->dis); ts->pending = true; }
    sv.restore(ctx);
    rtv->Release();

    (useS && useT ? g_warpsFull : useT ? g_warpsTemporal : g_warpsStereo)++;
    g_yawAbs += fabsf(d);
    if (fabsf(d) > g_yawMax) g_yawMax = fabsf(d);
    if (outPose) *outPose = tgt;
    return true;
}

bool has_held(int held) {
    return g_on.load() && held >= 0 && held <= 1 && g_held[held].valid && g_held[1 - held].valid && g_held[held].tex;
}

bool copy_held(ID3D11DeviceContext* ctx, int held, ID3D11Texture2D* dst) {
    if (!ctx || !dst || held < 0 || held > 1 || !g_held[held].tex) return false;
    D3D11_TEXTURE2D_DESC a, b;
    g_held[held].tex->GetDesc(&a); dst->GetDesc(&b);
    if (a.Width != b.Width || a.Height != b.Height) return false;
    ctx->CopyResource(dst, g_held[held].tex);
    return true;
}

void shutdown() {
    for (Held& h : g_held) { rel(h.srv); rel(h.tex); rel(h.drtv); rel(h.dsrv); rel(h.dtex); h = Held{}; }
    for (Ts& s : g_ts) { rel(s.dis); rel(s.a); rel(s.b); s.pending = false; }
    rel(g_vs); rel(g_ps); rel(g_psDepth); rel(g_cb); rel(g_lin); rel(g_point); rel(g_rs); rel(g_bs); rel(g_ds);
    g_ready = false; g_tsOk = false;
}

} // namespace dvr::afw
