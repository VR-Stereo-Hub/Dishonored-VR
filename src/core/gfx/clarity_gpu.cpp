// core/gfx/clarity_gpu.cpp - see clarity_gpu.h. No mod dependencies: the host
// test compiles this file as it is.
#include "core/gfx/clarity_gpu.h"

#include <windows.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <stdio.h>
#include <string.h>

namespace dvr::clarity {
namespace {

typedef HRESULT (WINAPI *PFN_D3DCompile)(LPCVOID, SIZE_T, LPCSTR, const void*, void*, LPCSTR,
                                         LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);
const UINT kOptimizationLevel3 = 1u << 15;   // D3DCOMPILE_OPTIMIZATION_LEVEL3 (d3dcompiler.h is not included)

// Every pass draws one full-screen triangle and reads its inputs with Load at
// integer pixels, except the history, which is resampled (Catmull-Rom, the
// nine-tap form built from bilinear fetches) at the reprojected position.
//
// Colour: the game's capture holds gamma-encoded values in a UNORM texture, so
// the first read decodes them (gFlags.x) and the final pass encodes again.
// Filtering and blending in gamma space darkens thin bright edges, which is the
// very detail anti-aliasing is judged on.
//
// The sharpening is the contrast-adaptive weighting of AMD FidelityFX CAS (MIT):
// a negative-lobe cross filter whose strength falls where the local minimum or
// maximum is already near the range limits, so edges are not haloed.
const char* kSrc =
    "struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
    "VSOut vsmain(uint id : SV_VertexID) {\n"
    "    VSOut o;\n"
    "    float2 uv = float2((id << 1) & 2, id & 2);\n"
    "    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n"
    "    o.uv = uv;\n"
    "    return o;\n"
    "}\n"
    "cbuffer CB : register(b0) {\n"
    "    float4 gSize;   // source w, h, output w, h\n"
    "    float4 gScale;  // sx, sy, radius x, radius y (source pixels)\n"
    "    float4 gFlags;  // source is gamma, history valid, sharpen, blend\n"
    "    float4 gRowF;   // prev-from-cur row 0, clip gamma\n"
    "    float4 gRowR;   // row 1, tanH\n"
    "    float4 gRowU;   // row 2, tanV\n"
    "    float4 gPad;\n"
    "};\n"
    "Texture2D t0 : register(t0);\n"
    "Texture2D t1 : register(t1);\n"
    "SamplerState sLinear : register(s0);\n"
    "float3 ToLinear(float3 c) {\n"
    "    c = saturate(c);\n"
    "    float3 lo = c / 12.92;\n"
    "    float3 hi = pow((c + 0.055) / 1.055, 2.4);\n"
    "    return float3(c.r <= 0.04045 ? lo.r : hi.r, c.g <= 0.04045 ? lo.g : hi.g, c.b <= 0.04045 ? lo.b : hi.b);\n"
    "}\n"
    "float3 ToGamma(float3 c) {\n"
    "    c = saturate(c);\n"
    "    float3 lo = c * 12.92;\n"
    "    float3 hi = 1.055 * pow(c, 1.0 / 2.4) - 0.055;\n"
    "    return float3(c.r <= 0.0031308 ? lo.r : hi.r, c.g <= 0.0031308 ? lo.g : hi.g, c.b <= 0.0031308 ? lo.b : hi.b);\n"
    "}\n"
    "float3 Fetch(int2 p) {\n"
    "    float3 c = t0.Load(int3(p, 0)).rgb;\n"
    "    return gFlags.x > 0.5 ? ToLinear(c) : c;\n"
    "}\n"
    // The resolve kernel is the Mitchell-Netravali family, gPad.xy = B, C. The first build
    // used B = C = 1/3 (smooth; judged soft in the headset next to the compositor's
    // aliased-but-crisp single tap). The default is Catmull-Rom (B = 0, C = 0.5): the same
    // every-pixel footprint with a sharper edge.
    "float Mitchell(float x) {\n"
    "    x = abs(x);\n"
    "    const float B = gPad.x, C = gPad.y;\n"
    "    if (x < 1.0) return ((12.0 - 9.0 * B - 6.0 * C) * x * x * x + (-18.0 + 12.0 * B + 6.0 * C) * x * x + (6.0 - 2.0 * B)) / 6.0;\n"
    "    if (x < 2.0) return ((-B - 6.0 * C) * x * x * x + (6.0 * B + 30.0 * C) * x * x + (-12.0 * B - 48.0 * C) * x + (8.0 * B + 24.0 * C)) / 6.0;\n"
    "    return 0.0;\n"
    "}\n"
    // Horizontal: output (ow x h). The output pixel's centre sits at pos.x * sx
    // in source coordinates; every source pixel within two output pixels of it
    // contributes, weighted by the kernel at that distance in OUTPUT pixels.
    "float4 ps_resolve_h(VSOut i) : SV_Target {\n"
    "    const float sx = gScale.x;\n"
    "    const float c = i.pos.x * sx;\n"
    "    const int y = (int)i.pos.y;\n"
    "    const int last = (int)gSize.x - 1;\n"
    "    const int i0 = (int)floor(c - gScale.z);\n"
    "    float3 acc = 0; float wsum = 0;\n"
    "    [loop] for (int k = 0; k < 40; ++k) {\n"
    "        const int ix = i0 + k;\n"
    "        const float d = ((float)ix + 0.5 - c) / sx;\n"
    "        if (d >= 2.0) break;\n"
    "        const float w = Mitchell(d);\n"
    "        acc += Fetch(int2(clamp(ix, 0, last), y)) * w;\n"
    "        wsum += w;\n"
    "    }\n"
    "    return float4(acc / max(wsum, 1e-6), 1.0);\n"
    "}\n"
    "float4 ps_resolve_v(VSOut i) : SV_Target {\n"
    "    const float sy = gScale.y;\n"
    "    const float c = i.pos.y * sy;\n"
    "    const int x = (int)i.pos.x;\n"
    "    const int last = (int)gSize.y - 1;\n"
    "    const int i0 = (int)floor(c - gScale.w);\n"
    "    float3 acc = 0; float wsum = 0;\n"
    "    [loop] for (int k = 0; k < 40; ++k) {\n"
    "        const int iy = i0 + k;\n"
    "        const float d = ((float)iy + 0.5 - c) / sy;\n"
    "        if (d >= 2.0) break;\n"
    "        const float w = Mitchell(d);\n"
    "        acc += Fetch(int2(x, clamp(iy, 0, last))) * w;\n"
    "        wsum += w;\n"
    "    }\n"
    "    return float4(acc / max(wsum, 1e-6), 1.0);\n"
    "}\n"
    "float3 RGBToYCoCg(float3 c) { return float3(0.25 * c.r + 0.5 * c.g + 0.25 * c.b, 0.5 * c.r - 0.5 * c.b, -0.25 * c.r + 0.5 * c.g - 0.25 * c.b); }\n"
    "float3 YCoCgToRGB(float3 c) { return float3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z); }\n"
    "float Luma(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }\n"
    "float3 HistoryCatmullRom(float2 uv, float2 size) {\n"
    "    const float2 sp = uv * size;\n"
    "    const float2 t1p = floor(sp - 0.5) + 0.5;\n"
    "    const float2 f = sp - t1p;\n"
    "    const float2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f));\n"
    "    const float2 w1 = 1.0 + f * f * (-2.5 + 1.5 * f);\n"
    "    const float2 w2 = f * (0.5 + f * (2.0 - 1.5 * f));\n"
    "    const float2 w3 = f * f * (-0.5 + 0.5 * f);\n"
    "    const float2 w12 = w1 + w2;\n"
    "    const float2 u0 = (t1p - 1.0) / size, u3 = (t1p + 2.0) / size, u12 = (t1p + w2 / w12) / size;\n"
    "    float3 r = 0;\n"
    "    r += t1.SampleLevel(sLinear, float2(u0.x,  u0.y),  0).rgb * (w0.x  * w0.y);\n"
    "    r += t1.SampleLevel(sLinear, float2(u12.x, u0.y),  0).rgb * (w12.x * w0.y);\n"
    "    r += t1.SampleLevel(sLinear, float2(u3.x,  u0.y),  0).rgb * (w3.x  * w0.y);\n"
    "    r += t1.SampleLevel(sLinear, float2(u0.x,  u12.y), 0).rgb * (w0.x  * w12.y);\n"
    "    r += t1.SampleLevel(sLinear, float2(u12.x, u12.y), 0).rgb * (w12.x * w12.y);\n"
    "    r += t1.SampleLevel(sLinear, float2(u3.x,  u12.y), 0).rgb * (w3.x  * w12.y);\n"
    "    r += t1.SampleLevel(sLinear, float2(u0.x,  u3.y),  0).rgb * (w0.x  * w3.y);\n"
    "    r += t1.SampleLevel(sLinear, float2(u12.x, u3.y),  0).rgb * (w12.x * w3.y);\n"
    "    r += t1.SampleLevel(sLinear, float2(u3.x,  u3.y),  0).rgb * (w3.x  * w3.y);\n"
    "    return r;\n"
    "}\n"
    "float3 ClipAABB(float3 mn, float3 mx, float3 q) {\n"
    "    const float3 center = 0.5 * (mx + mn);\n"
    "    const float3 ext = 0.5 * (mx - mn) + 1e-5;\n"
    "    const float3 v = q - center;\n"
    "    const float3 a = abs(v / ext);\n"
    "    const float m = max(a.x, max(a.y, a.z));\n"
    "    return m > 1.0 ? center + v / m : q;\n"
    "}\n"
    // Temporal: the current eye image blended with the same eye's history,
    // reprojected by rotation only. What the rotation cannot explain (walking
    // parallax, moving hands, NPCs) falls outside the neighbourhood's colour
    // spread and is clipped back to it: that pixel shows the current frame.
    "float4 ps_temporal(VSOut i) : SV_Target {\n"
    "    const int2 p = int2(i.pos.xy);\n"
    "    const int2 last = int2(gSize.zw) - 1;\n"
    "    const float3 c = Fetch(p);\n"
    "    if (gFlags.y < 0.5) return float4(c, 1.0);\n"
    "    float3 m1 = 0, m2 = 0;\n"
    "    [unroll] for (int dy = -1; dy <= 1; ++dy)\n"
    "    [unroll] for (int dx = -1; dx <= 1; ++dx) {\n"
    "        const float3 s = RGBToYCoCg(Fetch(clamp(p + int2(dx, dy), int2(0, 0), last)));\n"
    "        m1 += s; m2 += s * s;\n"
    "    }\n"
    "    const float2 uv = i.pos.xy / gSize.zw;\n"
    "    const float tanH = gRowR.w, tanV = gRowU.w;\n"
    "    const float3 d = float3(1.0, (uv.x * 2.0 - 1.0) * tanH, (1.0 - uv.y * 2.0) * tanV);\n"
    "    const float3 q = float3(dot(gRowF.xyz, d), dot(gRowR.xyz, d), dot(gRowU.xyz, d));\n"
    "    if (q.x <= 1e-4) return float4(c, 1.0);\n"
    "    const float2 puv = float2(0.5 + 0.5 * q.y / (q.x * tanH), 0.5 - 0.5 * q.z / (q.x * tanV));\n"
    "    if (any(puv < 0.0) || any(puv > 1.0)) return float4(c, 1.0);\n"
    "    float3 h = max(HistoryCatmullRom(puv, gSize.zw), 0.0);\n"
    "    const float3 mu = m1 / 9.0;\n"
    "    const float3 sigma = sqrt(max(m2 / 9.0 - mu * mu, 0.0));\n"
    "    const float g = gRowF.w;\n"
    "    h = YCoCgToRGB(ClipAABB(mu - g * sigma, mu + g * sigma, RGBToYCoCg(h)));\n"
    "    const float wc = gFlags.w / (1.0 + Luma(c));\n"
    "    const float wh = (1.0 - gFlags.w) / (1.0 + Luma(h));\n"
    "    return float4((c * wc + h * wh) / (wc + wh), 1.0);\n"
    "}\n"
    "float4 ps_final(VSOut i) : SV_Target {\n"
    "    const int2 p = int2(i.pos.xy);\n"
    "    const int2 last = int2(gSize.zw) - 1;\n"
    "    float3 c = Fetch(p);\n"
    "    const float sharp = gFlags.z;\n"
    "    if (sharp > 0.0) {\n"
    "        const float3 a = Fetch(clamp(p + int2(0, -1), int2(0, 0), last));\n"
    "        const float3 b = Fetch(clamp(p + int2(-1, 0), int2(0, 0), last));\n"
    "        const float3 d = Fetch(clamp(p + int2(1, 0), int2(0, 0), last));\n"
    "        const float3 e = Fetch(clamp(p + int2(0, 1), int2(0, 0), last));\n"
    "        const float3 mn = min(min(min(a, b), min(d, e)), c);\n"
    "        const float3 mx = max(max(max(a, b), max(d, e)), c);\n"
    "        const float3 amp = sqrt(saturate(min(mn, 1.0 - mx) / max(mx, 1e-5)));\n"
    "        const float3 w = amp * (-1.0 / lerp(8.0, 5.0, saturate(sharp)));\n"
    "        c = saturate((c + (a + b + d + e) * w) / (1.0 + 4.0 * w));\n"
    "    }\n"
    "    return float4(ToGamma(c), 1.0);\n"
    "}\n";

void say(char* why, size_t cap, const char* text) {
    if (why && cap) { strncpy_s(why, cap, text, _TRUNCATE); }
}

template <class T> void rel(T*& p) { if (p) { p->Release(); p = nullptr; } }

} // namespace

bool Gpu::init(ID3D11Device* dev, char* why, size_t cap) {
    if (ready_) return true;
    if (failed_ || !dev) { say(why, cap, failed_ ? "an earlier init failed" : "no device"); return false; }
    HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
    PFN_D3DCompile compile = compiler ? (PFN_D3DCompile)GetProcAddress(compiler, "D3DCompile") : nullptr;
    if (!compile) { failed_ = true; say(why, cap, "d3dcompiler_47.dll missing"); return false; }
    struct Entry { const char* name; const char* target; void** out; bool vs; };
    Entry entries[] = {
        {"vsmain", "vs_4_0", (void**)&vs_, true},
        {"ps_resolve_h", "ps_4_0", (void**)&psResolveH_, false},
        {"ps_resolve_v", "ps_4_0", (void**)&psResolveV_, false},
        {"ps_temporal", "ps_4_0", (void**)&psTemporal_, false},
        {"ps_final", "ps_4_0", (void**)&psFinal_, false},
    };
    for (const Entry& e : entries) {
        ID3DBlob *blob = nullptr, *err = nullptr;
        if (FAILED(compile(kSrc, strlen(kSrc), "clarity", nullptr, nullptr, e.name, e.target,
                           kOptimizationLevel3, 0, &blob, &err))) {
            char text[512];
            _snprintf_s(text, sizeof(text), _TRUNCATE, "%s compile failed: %s", e.name,
                        err ? (const char*)err->GetBufferPointer() : "?");
            say(why, cap, text);
            if (err) err->Release();
            failed_ = true;
            shutdown();
            return false;
        }
        HRESULT hr = e.vs ? dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr,
                                                    (ID3D11VertexShader**)e.out)
                          : dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr,
                                                   (ID3D11PixelShader**)e.out);
        blob->Release();
        if (err) err->Release();
        if (FAILED(hr)) {
            char text[128];
            _snprintf_s(text, sizeof(text), _TRUNCATE, "%s create failed 0x%08lx", e.name, (unsigned long)hr);
            say(why, cap, text);
            failed_ = true;
            shutdown();
            return false;
        }
    }
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = 7 * 16;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    dev->CreateBuffer(&bd, nullptr, &cb_);
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    dev->CreateSamplerState(&sd, &linear_);
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    dev->CreateRasterizerState(&rd, &raster_);
    D3D11_BLEND_DESC bld = {};
    bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    dev->CreateBlendState(&bld, &blend_);
    D3D11_DEPTH_STENCIL_DESC dd = {};
    dd.DepthEnable = FALSE;
    dev->CreateDepthStencilState(&dd, &depth_);
    ready_ = cb_ && linear_ && raster_ && blend_ && depth_;
    if (!ready_) { failed_ = true; say(why, cap, "state objects failed"); shutdown(); return false; }
    return true;
}

void Gpu::release(Target& t) {
    if (t.tex) bytes_ -= (uint64_t)t.w * t.h * 8u;
    rel(t.rtv); rel(t.srv); rel(t.tex);
    t.w = t.h = 0;
}

bool Gpu::ensure(ID3D11Device* dev, Target& t, uint32_t w, uint32_t h, char* why, size_t cap) {
    if (t.tex && t.w == w && t.h == h) return true;
    release(t);
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h;
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;   // linear light, 10+ bits: the history feeds back on itself
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &t.tex)) ||
        FAILED(dev->CreateShaderResourceView(t.tex, nullptr, &t.srv)) ||
        FAILED(dev->CreateRenderTargetView(t.tex, nullptr, &t.rtv))) {
        char text[96];
        _snprintf_s(text, sizeof(text), _TRUNCATE, "intermediate %ux%u RGBA16F failed", w, h);
        say(why, cap, text);
        rel(t.rtv); rel(t.srv); rel(t.tex);
        return false;
    }
    t.w = w; t.h = h;
    bytes_ += (uint64_t)w * h * 8u;
    return true;
}

void Gpu::shutdown() {
    release(tmpH_); release(lin_);
    for (int e = 0; e < 2; ++e) {
        release(hist_[e][0]); release(hist_[e][1]);
        histHave_[e] = false; histRead_[e] = 0;
    }
    rel(depth_); rel(blend_); rel(raster_); rel(linear_); rel(cb_);
    rel(psFinal_); rel(psTemporal_); rel(psResolveV_); rel(psResolveH_); rel(vs_);
    ready_ = false;
    bytes_ = 0;
}

void Gpu::trim(bool keepResolve, bool keepTemporal) {
    if (!keepResolve) { release(tmpH_); release(lin_); }
    if (!keepTemporal)
        for (int e = 0; e < 2; ++e) {
            release(hist_[e][0]); release(hist_[e][1]);
            histHave_[e] = false; histRead_[e] = 0;
        }
}

ID3D11ShaderResourceView* Gpu::history(int eye) const {
    const int e = eye & 1;
    return histHave_[e] ? hist_[e][histRead_[e]].srv : nullptr;
}

void Gpu::pass(ID3D11DeviceContext* ctx, ID3D11PixelShader* ps, ID3D11ShaderResourceView* t0,
               ID3D11ShaderResourceView* t1, ID3D11RenderTargetView* dst, uint32_t w, uint32_t h) {
    D3D11_VIEWPORT vp = {0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f};
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(raster_);
    ctx->OMSetRenderTargets(1, &dst, nullptr);
    const float bf[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(blend_, bf, 0xffffffff);
    ctx->OMSetDepthStencilState(depth_, 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs_, nullptr, 0);
    ctx->PSSetShader(ps, nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, &cb_);
    ID3D11ShaderResourceView* srvs[2] = {t0, t1};
    ctx->PSSetShaderResources(0, 2, srvs);
    ctx->PSSetSamplers(0, 1, &linear_);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none[2] = {nullptr, nullptr};
    ctx->PSSetShaderResources(0, 2, none);   // a pass's input may be the next pass's target
    ID3D11RenderTargetView* noRt = nullptr;
    ctx->OMSetRenderTargets(1, &noRt, nullptr);
}

bool Gpu::run(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src,
              ID3D11RenderTargetView* dst, const PassParams& p, char* why, size_t cap) {
    if (!ready_ || !dev || !ctx || !src || !dst) { say(why, cap, "not ready"); return false; }
    if (!p.w || !p.h || !p.ow || !p.oh) { say(why, cap, "zero size"); return false; }
    const bool resolve = p.resolve && (p.ow < p.w || p.oh < p.h);
    if (!resolve && (p.ow != p.w || p.oh != p.h)) { say(why, cap, "output size differs without a resolve"); return false; }
    const float sx = (float)p.w / (float)p.ow, sy = (float)p.h / (float)p.oh;
    if (resolve && (sx > 8.0f || sy > 8.0f)) { say(why, cap, "resolve step above 8x"); return false; }
    float cb[28] = {};
    auto upload = [&](float sw, float sh, float tw, float th, float srcGamma, float histValid) {
        cb[0] = sw; cb[1] = sh; cb[2] = tw; cb[3] = th;
        cb[4] = sx; cb[5] = sy; cb[6] = 2.0f * (sx > 1.0f ? sx : 1.0f); cb[7] = 2.0f * (sy > 1.0f ? sy : 1.0f);
        cb[8] = srcGamma; cb[9] = histValid; cb[10] = p.sharpen; cb[11] = p.blend;
        for (int j = 0; j < 3; ++j) {
            cb[12 + j] = p.prevFromCur.m[0][j];
            cb[16 + j] = p.prevFromCur.m[1][j];
            cb[20 + j] = p.prevFromCur.m[2][j];
        }
        cb[15] = p.clipGamma; cb[19] = p.tanH; cb[23] = p.tanV;
        cb[24] = p.kernelB; cb[25] = p.kernelC;
        ctx->UpdateSubresource(cb_, 0, nullptr, cb, 0, 0);
    };
    ID3D11ShaderResourceView* cur = src;
    float curGamma = p.srcGamma ? 1.0f : 0.0f;
    if (resolve) {
        if (!ensure(dev, tmpH_, p.ow, p.h, why, cap) || !ensure(dev, lin_, p.ow, p.oh, why, cap)) return false;
        upload((float)p.w, (float)p.h, (float)p.ow, (float)p.h, curGamma, 0.0f);
        pass(ctx, psResolveH_, src, nullptr, tmpH_.rtv, p.ow, p.h);
        upload((float)p.ow, (float)p.h, (float)p.ow, (float)p.oh, 0.0f, 0.0f);
        pass(ctx, psResolveV_, tmpH_.srv, nullptr, lin_.rtv, p.ow, p.oh);
        cur = lin_.srv; curGamma = 0.0f;
    }
    if (p.temporal) {
        const int e = p.eye & 1;
        for (int s = 0; s < 2; ++s) {
            if (hist_[e][s].tex && (hist_[e][s].w != p.ow || hist_[e][s].h != p.oh)) histHave_[e] = false;
            if (!ensure(dev, hist_[e][s], p.ow, p.oh, why, cap)) { histHave_[e] = false; return false; }
        }
        const int r = histRead_[e], w = 1 - r;
        const bool valid = p.historyValid && histHave_[e];
        upload((float)p.ow, (float)p.oh, (float)p.ow, (float)p.oh, curGamma, valid ? 1.0f : 0.0f);
        pass(ctx, psTemporal_, cur, valid ? hist_[e][r].srv : nullptr, hist_[e][w].rtv, p.ow, p.oh);
        histRead_[e] = w; histHave_[e] = true;
        cur = hist_[e][w].srv; curGamma = 0.0f;
    }
    upload((float)p.ow, (float)p.oh, (float)p.ow, (float)p.oh, curGamma, 0.0f);
    pass(ctx, psFinal_, cur, nullptr, dst, p.ow, p.oh);
    return true;
}

} // namespace dvr::clarity
