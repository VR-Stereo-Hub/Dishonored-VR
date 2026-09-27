// core/gfx/dlss_gpu.cpp - see dlss_gpu.h. No mod dependencies.
#include "core/gfx/dlss_gpu.h"

#include <windows.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace dvr::dlss {
namespace {

typedef HRESULT (WINAPI *PFN_D3DCompile)(LPCVOID, SIZE_T, LPCSTR, const void*, void*, LPCSTR,
                                         LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);
const UINT kOptimizationLevel3 = 1u << 15;
const int kAuditGrid = 64;

// The reprojection is clarity_gpu's Reproject, minus its screen-bounds rejection.
const char* kSrc =
    "struct VSOut { float4 pos : SV_Position; };\n"
    "VSOut vsmain(uint id : SV_VertexID) {\n"
    "    VSOut o;\n"
    "    float2 uv = float2((id << 1) & 2, id & 2);\n"
    "    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n"
    "    return o;\n"
    "}\n"
    "cbuffer CB : register(b0) {\n"
    "    float4 gRowF;   // prev-from-cur row 0\n"
    "    float4 gRowR;   // row 1, tanH\n"
    "    float4 gRowU;   // row 2, tanV\n"
    "    float4 gT;      // translation (previous axes, uu), depth scale\n"
    "    float4 gSize;   // w, h, depth w, depth h\n"
    "    float4 gFlags;  // prev tanH, prev tanV, use depth, history valid\n"
    "    float4 gMask;   // lo, hi, previous colour valid, audit grid\n"
    "    float4 gBody;   // body depth (depth units; nearer = arms/weapon: rotation only)\n"
    "};\n"
    "Texture2D tDepth : register(t0);\n"
    "struct PSOut { float depth : SV_Target0; float2 mv : SV_Target1; };\n"
    "PSOut psmain(VSOut i) {\n"
    "    PSOut o;\n"
    "    float2 uv = i.pos.xy / gSize.xy;\n"
    "    float z = 0;\n"
    "    if (gFlags.z > 0.5) {\n"
    "        z = tDepth.Load(int3(clamp(int2(uv * gSize.zw), int2(0,0), int2(gSize.zw)-1), 0)).a;\n"
    "        if (!(z > 0) || !isfinite(z)) z = 0;\n"
    "    }\n"
    "    bool scene = z > 0 && z < 1000;\n"
    "    o.depth = scene ? 1.0 / (1.0 + z) : 0.0;\n"
    "    o.mv = 0;\n"
    "    if (gFlags.w < 0.5) return o;\n"
    "    float3 ray = float3(1, (uv.x*2-1)*gRowR.w, (1-uv.y*2)*gRowU.w);\n"
    "    float3 q = float3(dot(gRowF.xyz,ray), dot(gRowR.xyz,ray), dot(gRowU.xyz,ray));\n"
    "    if (scene && z >= gBody.x) q += gT.xyz / (z*gT.w);\n"
    "    if (!(q.x > 1e-4) || !all(isfinite(q))) return o;\n"
    "    float2 puv = float2(0.5+0.5*q.y/(q.x*gFlags.x), 0.5-0.5*q.z/(q.x*gFlags.y));\n"
    "    o.mv = puv - uv;\n"
    "    return o;\n"
    "}\n"
    // The mask and the audit: t0 current colour, t1 previous colour, t2 motion, t3 guide
    // depth, t4 the mask itself (audit only).
    "Texture2D tCur : register(t0);\n"
    "Texture2D tPrev : register(t1);\n"
    "Texture2D tMv : register(t2);\n"
    "Texture2D tGd : register(t3);\n"
    "Texture2D tBias : register(t4);\n"
    "SamplerState sLinear : register(s0);\n"
    "float Luma(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }\n"
    "float MaskAt(int2 p) {\n"
    "    if (gMask.z < 0.5) return 0;\n"
    "    int2 last = int2(gSize.xy) - 1;\n"
    "    float3 c = tCur.Load(int3(p, 0)).rgb, mn = c, mx = c;\n"
    "    [unroll] for (int dy = -1; dy <= 1; ++dy)\n"
    "    [unroll] for (int dx = -1; dx <= 1; ++dx) {\n"
    "        float3 s = tCur.Load(int3(clamp(p + int2(dx, dy), int2(0,0), last), 0)).rgb;\n"
    "        mn = min(mn, s); mx = max(mx, s);\n"
    "    }\n"
    "    float2 uv = (float2(p) + 0.5) / gSize.xy;\n"
    "    float2 puv = uv + tMv.Load(int3(p, 0)).xy;\n"
    "    if (any(puv < 0) || any(puv > 1)) return 0;\n"
    "    float3 h = tPrev.SampleLevel(sLinear, puv, 0).rgb;\n"
    "    float3 ex = max(mn - h, h - mx);\n"
    "    return smoothstep(gMask.x, gMask.y, max(ex.r, max(ex.g, ex.b)));\n"
    "}\n"
    "float ps_mask(VSOut i) : SV_Target { return MaskAt(int2(i.pos.xy)); }\n"
    "float4 ps_audit(VSOut i) : SV_Target {\n"
    "    int2 p = int2((floor(i.pos.xy) + 0.5) / gMask.w * gSize.xy);\n"
    "    float2 uv = (float2(p) + 0.5) / gSize.xy;\n"
    "    float2 puv = uv + tMv.Load(int3(p, 0)).xy;\n"
    "    float lc = Luma(tCur.Load(int3(p, 0)).rgb);\n"
    "    float d = tGd.Load(int3(p, 0)).r;\n"
    "    float z = d > 0 ? 1.0 / d - 1.0 : -1.0;\n"
    "    float zeroErr = abs(lc - Luma(tPrev.Load(int3(p, 0)).rgb));\n"
    "    float vecErr = (any(puv < 0) || any(puv > 1)) ? -1.0 : abs(lc - Luma(tPrev.SampleLevel(sLinear, puv, 0).rgb));\n"
    "    return float4(vecErr, zeroErr, z, tBias.Load(int3(p, 0)).r);\n"
    "}\n";

void say(char* why, size_t cap, const char* fmt, ...) {
    if (!why || !cap) return;
    va_list a; va_start(a, fmt);
    _vsnprintf_s(why, cap, _TRUNCATE, fmt, a);
    va_end(a);
}
template <class T> void rel(T*& p) { if (p) { p->Release(); p = nullptr; } }

float g_cb[32] = {};

} // namespace

bool GuideGpu::init(ID3D11Device* dev, char* why, size_t cap) {
    if (ready_) return true;
    struct CompilerModule { HMODULE h; ~CompilerModule() { if (h) FreeLibrary(h); } } module{LoadLibraryA("d3dcompiler_47.dll")};
    PFN_D3DCompile compile = module.h ? (PFN_D3DCompile)GetProcAddress(module.h, "D3DCompile") : nullptr;
    if (!compile) { say(why, cap, "d3dcompiler_47.dll missing"); return false; }
    auto build = [&](const char* entry, const char* target, void** out, bool vs) -> bool {
        ID3DBlob *blob = nullptr, *err = nullptr;
        if (FAILED(compile(kSrc, strlen(kSrc), "dlss_guides", nullptr, nullptr, entry, target, kOptimizationLevel3, 0, &blob, &err))) {
            say(why, cap, "%s compile failed: %s", entry, err ? (const char*)err->GetBufferPointer() : "?");
            rel(err);
            return false;
        }
        rel(err);
        const HRESULT hr = vs ? dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, (ID3D11VertexShader**)out)
                              : dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, (ID3D11PixelShader**)out);
        blob->Release();
        if (FAILED(hr)) { say(why, cap, "%s create failed 0x%08lX", entry, (unsigned long)hr); return false; }
        return true;
    };
    if (!build("vsmain", "vs_4_0", (void**)&vs_, true) || !build("psmain", "ps_4_0", (void**)&ps_, false) ||
        !build("ps_mask", "ps_4_0", (void**)&psMask_, false) || !build("ps_audit", "ps_4_0", (void**)&psAudit_, false)) {
        shutdown(); return false;
    }
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(g_cb); bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    dev->CreateBuffer(&bd, nullptr, &cb_);
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    dev->CreateRasterizerState(&rd, &raster_);
    D3D11_BLEND_DESC bld = {};
    bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    bld.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    bld.IndependentBlendEnable = TRUE;
    dev->CreateBlendState(&bld, &blend_);
    D3D11_DEPTH_STENCIL_DESC dd = {};
    dd.DepthEnable = FALSE;
    dev->CreateDepthStencilState(&dd, &ds_);
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    dev->CreateSamplerState(&sd, &linear_);
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = kAuditGrid; td.MipLevels = 1; td.ArraySize = 1; td.SampleDesc.Count = 1;
    td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    dev->CreateTexture2D(&td, nullptr, &auditTex_);
    if (auditTex_) dev->CreateRenderTargetView(auditTex_, nullptr, &auditRtv_);
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    for (auto& s : auditStage_) dev->CreateTexture2D(&td, nullptr, &s);
    ready_ = cb_ && raster_ && blend_ && ds_ && linear_ && auditRtv_ && auditStage_[0] && auditStage_[1];
    if (!ready_) { say(why, cap, "state objects failed"); shutdown(); }
    return ready_;
}

bool GuideGpu::ensure(ID3D11Device* dev, uint32_t w, uint32_t h, char* why, size_t cap) {
    if (depth_ && w_ == w && h_ == h) return true;
    rel(depthRtv_); rel(motionRtv_); rel(biasRtv_); rel(depthSrv_); rel(motionSrv_); rel(biasSrv_);
    rel(depth_); rel(motion_); rel(bias_);
    forget(0); forget(1);
    w_ = h_ = 0;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    HRESULT hr = S_OK;
    auto make = [&](DXGI_FORMAT f, ID3D11Texture2D** t, ID3D11RenderTargetView** rtv, ID3D11ShaderResourceView** srv) {
        td.Format = f;
        if (SUCCEEDED(hr)) hr = dev->CreateTexture2D(&td, nullptr, t);
        if (SUCCEEDED(hr)) hr = dev->CreateRenderTargetView(*t, nullptr, rtv);
        if (SUCCEEDED(hr)) hr = dev->CreateShaderResourceView(*t, nullptr, srv);
    };
    make(DXGI_FORMAT_R32_FLOAT, &depth_, &depthRtv_, &depthSrv_);
    make(DXGI_FORMAT_R16G16_FLOAT, &motion_, &motionRtv_, &motionSrv_);
    make(DXGI_FORMAT_R8_UNORM, &bias_, &biasRtv_, &biasSrv_);
    if (FAILED(hr)) {
        say(why, cap, "guide targets %ux%u failed 0x%08lX", w, h, (unsigned long)hr);
        rel(depthRtv_); rel(motionRtv_); rel(biasRtv_); rel(depthSrv_); rel(motionSrv_); rel(biasSrv_);
        rel(depth_); rel(motion_); rel(bias_);
        return false;
    }
    w_ = w; h_ = h;
    return true;
}

static void full_screen(ID3D11DeviceContext* ctx, ID3D11RasterizerState* raster, ID3D11BlendState* blend,
                        ID3D11DepthStencilState* ds, ID3D11VertexShader* vs, ID3D11PixelShader* ps,
                        ID3D11Buffer* cb, uint32_t w, uint32_t h) {
    D3D11_VIEWPORT vp = {0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f};
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(raster);
    const float bf[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(blend, bf, 0xffffffff);
    ctx->OMSetDepthStencilState(ds, 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs, nullptr, 0);
    ctx->PSSetShader(ps, nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, &cb);
}

bool GuideGpu::run(ID3D11Device* dev, ID3D11DeviceContext* ctx, const GuideParams& p, char* why, size_t cap) {
    if (!ready_ && !init(dev, why, cap)) return false;
    if (!p.w || !p.h) { say(why, cap, "zero size"); return false; }
    if (!ensure(dev, p.w, p.h, why, cap)) return false;
    const bool useDepth = p.sceneDepth && p.depthW && p.depthH && p.depthScale > 0 && isfinite(p.depthScale);
    float* cb = g_cb;
    for (int j = 0; j < 3; ++j) { cb[j] = p.prevFromCur.m[0][j]; cb[4 + j] = p.prevFromCur.m[1][j]; cb[8 + j] = p.prevFromCur.m[2][j]; }
    cb[3] = 0; cb[7] = p.tanH; cb[11] = p.tanV;
    for (int j = 0; j < 3; ++j) cb[12 + j] = p.translation[j];
    cb[15] = p.depthScale;
    cb[16] = (float)p.w; cb[17] = (float)p.h; cb[18] = (float)p.depthW; cb[19] = (float)p.depthH;
    cb[20] = p.prevTanH > 0 ? p.prevTanH : p.tanH; cb[21] = p.prevTanV > 0 ? p.prevTanV : p.tanV;
    cb[22] = useDepth ? 1.0f : 0.0f; cb[23] = p.historyValid ? 1.0f : 0.0f;
    cb[28] = p.bodyDepth > 0 ? p.bodyDepth : 0.0f;
    ctx->UpdateSubresource(cb_, 0, nullptr, cb, 0, 0);
    full_screen(ctx, raster_, blend_, ds_, vs_, ps_, cb_, p.w, p.h);
    ID3D11RenderTargetView* rtvs[2] = {depthRtv_, motionRtv_};
    ctx->OMSetRenderTargets(2, rtvs, nullptr);
    ID3D11ShaderResourceView* srv = useDepth ? p.sceneDepth : nullptr;
    ctx->PSSetShaderResources(0, 1, &srv);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none = nullptr;
    ctx->PSSetShaderResources(0, 1, &none);
    ID3D11RenderTargetView* noRt[2] = {nullptr, nullptr};
    ctx->OMSetRenderTargets(2, noRt, nullptr);
    return true;
}

bool GuideGpu::mask(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11ShaderResourceView* color,
                    bool historyValid, float lo, float hi, char* why, size_t cap) {
    (void)dev;
    if (!ready_ || !bias_ || !color || (eye != 0 && eye != 1)) { say(why, cap, "mask: not ready"); return false; }
    const bool have = historyValid && prevOk_[eye] && prevSrv_[eye];
    g_cb[24] = lo; g_cb[25] = hi > lo ? hi : lo + 0.001f; g_cb[26] = have ? 1.0f : 0.0f; g_cb[27] = (float)kAuditGrid;
    ctx->UpdateSubresource(cb_, 0, nullptr, g_cb, 0, 0);
    full_screen(ctx, raster_, blend_, ds_, vs_, psMask_, cb_, w_, h_);
    ctx->OMSetRenderTargets(1, &biasRtv_, nullptr);
    ID3D11ShaderResourceView* srvs[3] = {color, have ? prevSrv_[eye] : nullptr, motionSrv_};
    ctx->PSSetShaderResources(0, 3, srvs);
    ctx->PSSetSamplers(0, 1, &linear_);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none[3] = {};
    ctx->PSSetShaderResources(0, 3, none);
    ID3D11RenderTargetView* noRt = nullptr;
    ctx->OMSetRenderTargets(1, &noRt, nullptr);
    return true;
}

bool GuideGpu::keep(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11Texture2D* color) {
    if ((eye != 0 && eye != 1) || !color) return false;
    D3D11_TEXTURE2D_DESC cd = {};
    color->GetDesc(&cd);
    if (prev_[eye]) {
        D3D11_TEXTURE2D_DESC pd = {};
        prev_[eye]->GetDesc(&pd);
        if (pd.Width != cd.Width || pd.Height != cd.Height || pd.Format != cd.Format) {
            prevBytes_ -= (uint64_t)pd.Width * pd.Height * 4;
            rel(prevSrv_[eye]); rel(prev_[eye]);
        }
    }
    if (!prev_[eye]) {
        D3D11_TEXTURE2D_DESC td = cd;
        td.MipLevels = 1; td.ArraySize = 1; td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE; td.CPUAccessFlags = 0; td.MiscFlags = 0;
        if (FAILED(dev->CreateTexture2D(&td, nullptr, &prev_[eye])) ||
            FAILED(dev->CreateShaderResourceView(prev_[eye], nullptr, &prevSrv_[eye]))) {
            rel(prevSrv_[eye]); rel(prev_[eye]); prevOk_[eye] = false;
            return false;
        }
        prevBytes_ += (uint64_t)cd.Width * cd.Height * 4;
    }
    ctx->CopyResource(prev_[eye], color);
    prevOk_[eye] = true;
    return true;
}

void GuideGpu::forget(int eye) { if (eye == 0 || eye == 1) prevOk_[eye] = false; }

void GuideGpu::audit(ID3D11Device* dev, ID3D11DeviceContext* ctx, int eye, ID3D11ShaderResourceView* color) {
    (void)dev;
    if (!ready_ || !color || (eye != 0 && eye != 1)) return;
    // Fold in whichever staging copy the GPU has finished, without waiting.
    for (int s = 0; s < 2; ++s) {
        if (!auditPending_[s]) continue;
        D3D11_MAPPED_SUBRESOURCE m = {};
        if (ctx->Map(auditStage_[s], 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m) != S_OK) continue;
        for (int y = 0; y < kAuditGrid; ++y) {
            const float* row = (const float*)((const uint8_t*)m.pData + (size_t)y * m.RowPitch);
            for (int x = 0; x < kAuditGrid; ++x) {
                const float* v = row + x * 4;
                if (v[0] < 0) continue;   // moved off screen
                const float z = v[2];
                const int b = z < 0 ? 7 : z < 0.1f ? 0 : z < 0.3f ? 1 : z < 1 ? 2 : z < 2 ? 3 : z < 10 ? 4 : z < 50 ? 5 : z < 1000 ? 6 : 7;
                bins[b].vecErr += v[0]; bins[b].zeroErr += v[1]; bins[b].masked += v[3]; ++bins[b].n;
            }
        }
        ctx->Unmap(auditStage_[s], 0);
        auditPending_[s] = false;
    }
    if (!prevOk_[eye] || !prevSrv_[eye]) return;
    const int s = auditNext_;
    if (auditPending_[s]) return;   // both copies still in flight: skip this image
    auditNext_ = 1 - auditNext_;
    full_screen(ctx, raster_, blend_, ds_, vs_, psAudit_, cb_, kAuditGrid, kAuditGrid);
    ctx->OMSetRenderTargets(1, &auditRtv_, nullptr);
    ID3D11ShaderResourceView* srvs[5] = {color, prevSrv_[eye], motionSrv_, depthSrv_, biasSrv_};
    ctx->PSSetShaderResources(0, 5, srvs);
    ctx->PSSetSamplers(0, 1, &linear_);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none[5] = {};
    ctx->PSSetShaderResources(0, 5, none);
    ID3D11RenderTargetView* noRt = nullptr;
    ctx->OMSetRenderTargets(1, &noRt, nullptr);
    ctx->CopyResource(auditStage_[s], auditTex_);
    auditPending_[s] = true;
}

uint64_t GuideGpu::bytes() const { return (uint64_t)w_ * h_ * 9 + prevBytes_; }

void GuideGpu::shutdown() {
    rel(depthRtv_); rel(motionRtv_); rel(biasRtv_); rel(depthSrv_); rel(motionSrv_); rel(biasSrv_);
    rel(depth_); rel(motion_); rel(bias_);
    for (int e = 0; e < 2; ++e) { rel(prevSrv_[e]); rel(prev_[e]); prevOk_[e] = false; }
    prevBytes_ = 0;
    rel(auditRtv_); rel(auditTex_);
    for (int s = 0; s < 2; ++s) { rel(auditStage_[s]); auditPending_[s] = false; }
    rel(linear_); rel(ds_); rel(blend_); rel(raster_); rel(cb_);
    rel(psAudit_); rel(psMask_); rel(ps_); rel(vs_);
    ready_ = false; w_ = h_ = 0;
    for (auto& b : bins) b = AuditBin{};
}

} // namespace dvr::dlss
