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
#include <cmath>
#include <string.h>

namespace dvr::afw {
namespace {

typedef HRESULT (WINAPI *PFN_D3DCompile)(LPCVOID, SIZE_T, LPCSTR, const void*, void*, LPCSTR,
                                         LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

// Rows are float4 with w unused. R_src maps the held image's view space to tracking space;
// dstT maps tracking space into the target view (the transpose of its rotation); yaw is the
// world's rotation in tracking space since the held image (Ry, about the head centre).
const char* kSrc =
    "struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
    "VSOut vsmain(uint id : SV_VertexID) {\n"
    "    VSOut o; float2 uv = float2((id << 1) & 2, id & 2);\n"
    "    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0); o.uv = uv; return o;\n"
    "}\n"
    "Texture2D colorTex : register(t0);\n"
    "Texture2D depthTex : register(t1);\n"
    "SamplerState linSamp : register(s0);\n"
    "SamplerState pointSamp : register(s1);\n"
    "cbuffer P : register(b0) {\n"
    "    float4 s0, s1, s2, sp;\n"      // R_src rows, src position
    "    float4 d0, d1, d2, dp;\n"      // R_dst^T rows, dst position
    "    float4 y0, y1, y2, yc;\n"      // world yaw rows, centre
    "    float4 prm;\n"                  // tanH, tanV, metres per depth unit, body threshold (units)
    "    float4 prm2;\n"                 // depth texels w, h, error tolerance (texels), 0
    "};\n"
    "float3 viewDir(float2 uv) { float2 n = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0); return float3(n.x * prm.x, n.y * prm.y, -1.0); }\n"
    "float2 toUV(float3 L) { float iz = 1.0 / max(-L.z, 1e-4); float2 n = float2(L.x * iz / prm.x, L.y * iz / prm.y);\n"
    "    return float2(n.x * 0.5 + 0.5, 0.5 - n.y * 0.5); }\n"
    "float3 rs(float3 v) { return float3(dot(s0.xyz, v), dot(s1.xyz, v), dot(s2.xyz, v)); }\n"
    "float3 rsT(float3 v) { return s0.xyz * v.x + s1.xyz * v.y + s2.xyz * v.z; }\n"
    "float3 rd(float3 v) { return float3(dot(d0.xyz, v), dot(d1.xyz, v), dot(d2.xyz, v)); }\n"
    "float3 rdT(float3 v) { return d0.xyz * v.x + d1.xyz * v.y + d2.xyz * v.z; }\n"
    "float3 ry(float3 v) { return float3(dot(y0.xyz, v), dot(y1.xyz, v), dot(y2.xyz, v)); }\n"
    "float3 ryT(float3 v) { return y0.xyz * v.x + y1.xyz * v.y + y2.xyz * v.z; }\n"
    "float depthAt(float2 uv) { float z = depthTex.SampleLevel(pointSamp, uv, 0).a; return z > 0.0 ? z : 60000.0; }\n"
    "float2 fwd(float2 s, float z, bool world) {\n"
    "    float3 W = rs(viewDir(s) * (z * prm.z)) + sp.xyz;\n"
    "    if (world) W = ry(W - yc.xyz) + yc.xyz;\n"
    "    return toUV(rd(W - dp.xyz));\n"
    "}\n"
    // The infinitely-far inverse: the direction through the target pixel carried back.
    "float2 guess(float2 t, bool world) {\n"
    "    float3 dir = rdT(viewDir(t));\n"
    "    if (world) dir = ryT(dir);\n"
    "    return toUV(rsT(dir) * 1.0);\n"
    "}\n"
    "float2 solve(float2 t, bool world, out float z, out float err) {\n"
    "    float2 s = saturate(guess(t, world));\n"
    "    [unroll] for (int i = 0; i < 4; ++i) { z = depthAt(s); s = saturate(s + (t - fwd(s, z, world))); }\n"
    "    z = depthAt(s); err = length((fwd(s, z, world) - t) * prm2.xy); return s;\n"
    "}\n"
    "float4 psmain(VSOut i) : SV_Target {\n"
    "    float zb, eb, zw, ew;\n"
    "    float2 sb = solve(i.uv, false, zb, eb);\n"
    "    float2 sw = solve(i.uv, true, zw, ew);\n"
    "    bool body = zb < prm.w && eb < prm2.z;\n"
    "    return float4(colorTex.SampleLevel(linSamp, body ? sb : sw, 0).rgb, 1.0);\n"
    "}\n";

struct Held {
    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    uint32_t w = 0, h = 0, fmt = 0;
    uint32_t serial = 0;
    Pose pose{};
    bool bodyOk = false;
    float bodyYaw = 0;
    Pose targets[2]{};
    bool valid = false;
};
Held g_held[2];

std::atomic<bool> g_on{false};
std::atomic<float> g_bodyDepth{0.40f};
std::atomic<float> g_worldScale{100.0f};
bool g_ready = false, g_failed = false;
ID3D11VertexShader* g_vs = nullptr;
ID3D11PixelShader* g_ps = nullptr;
ID3D11Buffer* g_cb = nullptr;
ID3D11SamplerState* g_lin = nullptr;
ID3D11SamplerState* g_point = nullptr;
ID3D11RasterizerState* g_rs = nullptr;
ID3D11BlendState* g_bs = nullptr;
ID3D11DepthStencilState* g_ds = nullptr;
uint32_t g_warps = 0, g_noHeld = 0, g_noDepth = 0, g_noRtv = 0, g_notReady = 0;
double g_yawAbs = 0; float g_yawMax = 0;
uint64_t g_beatMs = 0;

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

bool init(ID3D11Device* dev) {
    if (g_ready) return true;
    if (g_failed || !dev) return false;
    HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
    PFN_D3DCompile compile = compiler ? (PFN_D3DCompile)GetProcAddress(compiler, "D3DCompile") : nullptr;
    if (!compile) { g_failed = true; DVR_ERROR("afw/warp: d3dcompiler_47.dll missing - the held eye stays rotation-only"); return false; }
    ID3DBlob *vsb = nullptr, *psb = nullptr, *err = nullptr;
    if (FAILED(compile(kSrc, strlen(kSrc), nullptr, nullptr, nullptr, "vsmain", "vs_4_0", 0, 0, &vsb, &err)) ||
        FAILED(compile(kSrc, strlen(kSrc), nullptr, nullptr, nullptr, "psmain", "ps_4_0", 0, 0, &psb, &err))) {
        g_failed = true;
        DVR_ERROR("afw/warp: shader compile failed: %s - the held eye stays rotation-only",
                  err ? (const char*)err->GetBufferPointer() : "?");
        rel(err); rel(vsb); rel(psb);
        return false;
    }
    HRESULT hr = dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &g_vs);
    if (SUCCEEDED(hr)) hr = dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &g_ps);
    rel(vsb); rel(psb);
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = 14 * 16;   // fourteen float4s: s0..2 sp d0..2 dp y0..2 yc prm prm2
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
    g_ready = true;
    DVR_INFO("afw/warp: ready - the held eye is re-rendered each present from its own image and depth at the fresh "
             "eye's head pose (world pixels carry the body yaw, body pixels nearer than %.2f depth units do not)",
             g_bodyDepth.load());
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

void beat() {
    const uint64_t now = GetTickCount64();
    if (!g_beatMs) { g_beatMs = now; return; }
    if (now - g_beatMs < 3000) return;
    g_beatMs = now;
    if (g_warps + g_noHeld + g_noDepth + g_noRtv + g_notReady)
        DVR_INFO("afw/warp: beat %u held-eye warps (mean |yaw| %.3f deg, max %.3f) | not warped: no held image %u, "
                 "no depth for its grab %u, no render target %u, not ready %u (each falls back to the "
                 "rotation-only held eye) | body < %.2f units, %.3f m per unit",
                 g_warps, g_warps ? g_yawAbs / g_warps : 0.0, g_yawMax, g_noHeld, g_noDepth, g_noRtv, g_notReady,
                 g_bodyDepth.load(), dvr::clarity::depth_scale() / g_worldScale.load());
    g_warps = g_noHeld = g_noDepth = g_noRtv = g_notReady = 0;
    g_yawAbs = 0; g_yawMax = 0;
}

} // namespace

void set_enabled(bool on, const char* who) {
    if (g_on.exchange(on) == on) return;
    DVR_INFO("afw/warp: %s (%s)%s", on ? "ON" : "off", who ? who : "?",
             on ? "" : " - the held eye is the compositor's rotation-only reprojection (plus the body-yaw pose)");
}
bool enabled() { return g_on.load(); }
void set_body_depth(float units, const char* who) {
    if (!(units > 0.0f && units < 5.0f)) { DVR_WARN("afw/warp: body depth %.3f refused (0..5 units)", units); return; }
    g_bodyDepth.store(units);
    DVR_INFO("afw/warp: body depth %.2f units (%s) - nearer pixels are the hands and weapon", units, who ? who : "?");
}
float body_depth() { return g_bodyDepth.load(); }
void set_world_scale(float uuPerM) { if (uuPerM >= 1.0f && uuPerM <= 400.0f) g_worldScale.store(uuPerM); }

void note_capture(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11Texture2D* frame,
                  uint32_t grabSerial, const Pose& pose, bool bodyOk, float bodyYawDeg, const Pose targets[2]) {
    if (!g_on.load() || eye < 0 || eye > 1 || !dev || !ctx || !frame) return;
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
    h.valid = true;
}

bool warp_held(ID3D11Device* dev, ID3D11DeviceContext* ctx, int held, int fresh, ID3D11Texture2D* dst,
               uint32_t w, uint32_t h, float tanH, float tanV, Pose* outPose, const char** why) {
    beat();
    const char* reason = nullptr;
    if (why) *why = nullptr;
    if (!g_on.load()) { if (why) *why = "off"; return false; }
    if (!init(dev)) { ++g_notReady; if (why) *why = "not ready"; return false; }
    if (held < 0 || held > 1 || fresh != 1 - held || !dst || !ctx) { if (why) *why = "bad call"; return false; }
    const Held& src = g_held[held];
    const Held& fr = g_held[fresh];
    if (!src.valid || !fr.valid || !src.srv) { ++g_noHeld; if (why) *why = "no held image"; return false; }
    UINT dw = 0, dh = 0;
    ID3D11ShaderResourceView* depth = dvr::depthprobe::depth_srv_for(src.serial, &dw, &dh);
    if (!depth || !dw || !dh) { ++g_noDepth; if (why) *why = "no depth for the held image's grab"; return false; }
    D3D11_TEXTURE2D_DESC dd;
    dst->GetDesc(&dd);
    D3D11_RENDER_TARGET_VIEW_DESC rv = {};
    rv.Format = typed(dd.Format);
    rv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    ID3D11RenderTargetView* rtv = nullptr;
    if (FAILED(dev->CreateRenderTargetView(dst, &rv, &rtv)) || !rtv) {
        ++g_noRtv; dvr::depthprobe::read_done(ctx); if (why) *why = "no render target"; return false;
    }
    // The target: the held eye's view pose of the FRESH image's locate generation.
    const Pose& tgt = fr.targets[held];
    float d = (src.bodyOk && fr.bodyOk) ? fr.bodyYaw - src.bodyYaw : 0.0f;
    while (d > 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    struct CB { float s[3][4]; float sp[4]; float d[3][4]; float dp[4]; float y[3][4]; float yc[4]; float prm[4]; float prm2[4]; } cb;
    static_assert(sizeof(CB) == 14 * 16, "afw cbuffer layout");
    rows(src.pose, cb.s, false);
    cb.sp[0] = src.pose.p[0]; cb.sp[1] = src.pose.p[1]; cb.sp[2] = src.pose.p[2]; cb.sp[3] = 0;
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
    cb.prm[0] = tanH; cb.prm[1] = tanV;
    cb.prm[2] = dvr::clarity::depth_scale() / g_worldScale.load();
    cb.prm[3] = g_bodyDepth.load();
    cb.prm2[0] = (float)dw; cb.prm2[1] = (float)dh; cb.prm2[2] = 1.5f; cb.prm2[3] = 0;
    ctx->UpdateSubresource(g_cb, 0, nullptr, &cb, 0, 0);

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
    ctx->PSSetShader(g_ps, nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, &g_cb);
    ID3D11ShaderResourceView* srvs[2] = {src.srv, depth};
    ctx->PSSetShaderResources(0, 2, srvs);
    ID3D11SamplerState* samps[2] = {g_lin, g_point};
    ctx->PSSetSamplers(0, 2, samps);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none[2] = {nullptr, nullptr};
    ctx->PSSetShaderResources(0, 2, none);
    ctx->OMSetRenderTargets(0, nullptr, nullptr);
    rtv->Release();
    dvr::depthprobe::read_done(ctx);

    ++g_warps;
    g_yawAbs += fabsf(d);
    if (fabsf(d) > g_yawMax) g_yawMax = fabsf(d);
    if (outPose) *outPose = tgt;
    (void)reason;
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
    for (Held& h : g_held) { rel(h.srv); rel(h.tex); h = Held{}; }
    rel(g_vs); rel(g_ps); rel(g_cb); rel(g_lin); rel(g_point); rel(g_rs); rel(g_bs); rel(g_ds);
    g_ready = false;
}

} // namespace dvr::afw
