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
    "    if (scene) q += gT.xyz / (z*gT.w);\n"
    "    if (!(q.x > 1e-4) || !all(isfinite(q))) return o;\n"
    "    float2 puv = float2(0.5+0.5*q.y/(q.x*gFlags.x), 0.5-0.5*q.z/(q.x*gFlags.y));\n"
    "    o.mv = puv - uv;\n"
    "    return o;\n"
    "}\n";

void say(char* why, size_t cap, const char* fmt, ...) {
    if (!why || !cap) return;
    va_list a; va_start(a, fmt);
    _vsnprintf_s(why, cap, _TRUNCATE, fmt, a);
    va_end(a);
}
template <class T> void rel(T*& p) { if (p) { p->Release(); p = nullptr; } }

} // namespace

bool GuideGpu::init(ID3D11Device* dev, char* why, size_t cap) {
    if (ready_) return true;
    struct CompilerModule { HMODULE h; ~CompilerModule() { if (h) FreeLibrary(h); } } module{LoadLibraryA("d3dcompiler_47.dll")};
    PFN_D3DCompile compile = module.h ? (PFN_D3DCompile)GetProcAddress(module.h, "D3DCompile") : nullptr;
    if (!compile) { say(why, cap, "d3dcompiler_47.dll missing"); return false; }
    auto build = [&](const char* entry, const char* target, bool vs) -> bool {
        ID3DBlob *blob = nullptr, *err = nullptr;
        if (FAILED(compile(kSrc, strlen(kSrc), "dlss_guides", nullptr, nullptr, entry, target, kOptimizationLevel3, 0, &blob, &err))) {
            say(why, cap, "%s compile failed: %s", entry, err ? (const char*)err->GetBufferPointer() : "?");
            rel(err);
            return false;
        }
        rel(err);
        const HRESULT hr = vs ? dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &vs_)
                              : dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &ps_);
        blob->Release();
        if (FAILED(hr)) { say(why, cap, "%s create failed 0x%08lX", entry, (unsigned long)hr); return false; }
        return true;
    };
    if (!build("vsmain", "vs_4_0", true) || !build("psmain", "ps_4_0", false)) { shutdown(); return false; }
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = 6 * 16; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    dev->CreateBuffer(&bd, nullptr, &cb_);
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    dev->CreateRasterizerState(&rd, &raster_);
    D3D11_BLEND_DESC bld = {};
    bld.IndependentBlendEnable = FALSE;
    bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    dev->CreateBlendState(&bld, &blend_);
    D3D11_DEPTH_STENCIL_DESC dd = {};
    dd.DepthEnable = FALSE;
    dev->CreateDepthStencilState(&dd, &ds_);
    ready_ = cb_ && raster_ && blend_ && ds_;
    if (!ready_) { say(why, cap, "state objects failed"); shutdown(); }
    return ready_;
}

bool GuideGpu::ensure(ID3D11Device* dev, uint32_t w, uint32_t h, char* why, size_t cap) {
    if (depth_ && w_ == w && h_ == h) return true;
    rel(depthRtv_); rel(motionRtv_); rel(depth_); rel(motion_);
    w_ = h_ = 0;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h; td.MipLevels = 1; td.ArraySize = 1; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.Format = DXGI_FORMAT_R32_FLOAT;
    HRESULT hr = dev->CreateTexture2D(&td, nullptr, &depth_);
    if (SUCCEEDED(hr)) hr = dev->CreateRenderTargetView(depth_, nullptr, &depthRtv_);
    td.Format = DXGI_FORMAT_R16G16_FLOAT;
    if (SUCCEEDED(hr)) hr = dev->CreateTexture2D(&td, nullptr, &motion_);
    if (SUCCEEDED(hr)) hr = dev->CreateRenderTargetView(motion_, nullptr, &motionRtv_);
    if (FAILED(hr)) {
        say(why, cap, "guide targets %ux%u failed 0x%08lX", w, h, (unsigned long)hr);
        rel(depthRtv_); rel(motionRtv_); rel(depth_); rel(motion_);
        return false;
    }
    w_ = w; h_ = h;
    return true;
}

bool GuideGpu::run(ID3D11Device* dev, ID3D11DeviceContext* ctx, const GuideParams& p, char* why, size_t cap) {
    if (!ready_ && !init(dev, why, cap)) return false;
    if (!p.w || !p.h) { say(why, cap, "zero size"); return false; }
    if (!ensure(dev, p.w, p.h, why, cap)) return false;
    const bool useDepth = p.sceneDepth && p.depthW && p.depthH && p.depthScale > 0 && isfinite(p.depthScale);
    float cb[24] = {};
    for (int j = 0; j < 3; ++j) { cb[j] = p.prevFromCur.m[0][j]; cb[4 + j] = p.prevFromCur.m[1][j]; cb[8 + j] = p.prevFromCur.m[2][j]; }
    cb[7] = p.tanH; cb[11] = p.tanV;
    for (int j = 0; j < 3; ++j) cb[12 + j] = p.translation[j];
    cb[15] = p.depthScale;
    cb[16] = (float)p.w; cb[17] = (float)p.h; cb[18] = (float)p.depthW; cb[19] = (float)p.depthH;
    cb[20] = p.prevTanH > 0 ? p.prevTanH : p.tanH; cb[21] = p.prevTanV > 0 ? p.prevTanV : p.tanV;
    cb[22] = useDepth ? 1.0f : 0.0f; cb[23] = p.historyValid ? 1.0f : 0.0f;
    ctx->UpdateSubresource(cb_, 0, nullptr, cb, 0, 0);
    D3D11_VIEWPORT vp = {0.0f, 0.0f, (float)p.w, (float)p.h, 0.0f, 1.0f};
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(raster_);
    ID3D11RenderTargetView* rtvs[2] = {depthRtv_, motionRtv_};
    ctx->OMSetRenderTargets(2, rtvs, nullptr);
    const float bf[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(blend_, bf, 0xffffffff);
    ctx->OMSetDepthStencilState(ds_, 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs_, nullptr, 0);
    ctx->PSSetShader(ps_, nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, &cb_);
    ID3D11ShaderResourceView* srv = useDepth ? p.sceneDepth : nullptr;
    ctx->PSSetShaderResources(0, 1, &srv);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none = nullptr;
    ctx->PSSetShaderResources(0, 1, &none);
    ID3D11RenderTargetView* noRt[2] = {nullptr, nullptr};
    ctx->OMSetRenderTargets(2, noRt, nullptr);
    return true;
}

void GuideGpu::shutdown() {
    rel(depthRtv_); rel(motionRtv_); rel(depth_); rel(motion_);
    rel(ds_); rel(blend_); rel(raster_); rel(cb_); rel(ps_); rel(vs_);
    ready_ = false; w_ = h_ = 0;
}

} // namespace dvr::dlss
