// core/gfx/blit_quad.cpp - see blit_quad.h.
#define DVR_CAT ::dvr::log::Cat::d3d
#include "core/gfx/blit_quad.h"

#include "core/util/log.h"

#include <windows.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <string.h>

namespace dvr::gfx {
namespace {

typedef HRESULT (WINAPI *PFN_D3DCompile)(LPCVOID, SIZE_T, LPCSTR, const void*, void*, LPCSTR,
                                         LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

// SV_VertexID -> one triangle covering the clip-space square; uv derived.
const char* kSrc =
    "struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
    "VSOut vsmain(uint id : SV_VertexID) {\n"
    "    VSOut o;\n"
    "    float2 uv = float2((id << 1) & 2, id & 2);\n"
    "    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);\n"
    "    o.uv = uv;\n"
    "    return o;\n"
    "}\n"
    "Texture2D srcTex : register(t0);\n"
    "SamplerState samp : register(s0);\n"
    "float4 psmain(VSOut i) : SV_Target { return float4(srcTex.Sample(samp, i.uv).rgb, 1.0); }\n";

} // namespace

bool BlitQuad::init(ID3D11Device* dev) {
    if (ready_) return true;
    if (failed_ || !dev) return false;
    HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
    PFN_D3DCompile compile = compiler ? (PFN_D3DCompile)GetProcAddress(compiler, "D3DCompile") : nullptr;
    if (!compile) {
        failed_ = true;
        DVR_ERROR("blit: d3dcompiler_47.dll missing - no D3D11 blit, nothing reaches the headset");
        return false;
    }
    ID3DBlob *vsb = nullptr, *psb = nullptr, *err = nullptr;
    if (FAILED(compile(kSrc, strlen(kSrc), nullptr, nullptr, nullptr, "vsmain", "vs_4_0", 0, 0, &vsb, &err))) {
        failed_ = true;
        DVR_ERROR("blit: VS compile failed: %s", err ? (const char*)err->GetBufferPointer() : "?");
        if (err) err->Release();
        return false;
    }
    if (FAILED(compile(kSrc, strlen(kSrc), nullptr, nullptr, nullptr, "psmain", "ps_4_0", 0, 0, &psb, &err))) {
        failed_ = true;
        DVR_ERROR("blit: PS compile failed: %s", err ? (const char*)err->GetBufferPointer() : "?");
        if (err) err->Release();
        vsb->Release();
        return false;
    }
    HRESULT hr = dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &vs_);
    if (SUCCEEDED(hr)) hr = dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &ps_);
    vsb->Release(); psb->Release();
    if (FAILED(hr)) { failed_ = true; DVR_ERROR("blit: shader objects failed (0x%08lx)", (unsigned long)hr); return false; }

    D3D11_SAMPLER_DESC sd = {};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    dev->CreateSamplerState(&sd, &sampler_);
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    dev->CreateRasterizerState(&rd, &raster_);
    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    dev->CreateBlendState(&bd, &blend_);
    D3D11_DEPTH_STENCIL_DESC dd = {};
    dd.DepthEnable = FALSE;
    dev->CreateDepthStencilState(&dd, &depth_);
    ready_ = sampler_ && raster_ && blend_ && depth_;
    if (!ready_) { failed_ = true; DVR_ERROR("blit: state objects failed"); }
    else DVR_INFO("blit: D3D11 blit pipeline ready");
    return ready_;
}

void BlitQuad::shutdown() {
    if (depth_) { depth_->Release(); depth_ = nullptr; }
    if (blend_) { blend_->Release(); blend_ = nullptr; }
    if (raster_) { raster_->Release(); raster_ = nullptr; }
    if (sampler_) { sampler_->Release(); sampler_ = nullptr; }
    if (ps_) { ps_->Release(); ps_ = nullptr; }
    if (vs_) { vs_->Release(); vs_ = nullptr; }
    ready_ = false;
}

void BlitQuad::draw(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* src,
                    ID3D11RenderTargetView* dst, uint32_t w, uint32_t h) {
    if (!ready_ || !ctx || !src || !dst) return;
    D3D11_VIEWPORT vp = {0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f};
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(raster_);
    ctx->OMSetRenderTargets(1, &dst, nullptr);
    const float blendFactor[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(blend_, blendFactor, 0xffffffff);
    ctx->OMSetDepthStencilState(depth_, 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs_, nullptr, 0);
    ctx->PSSetShader(ps_, nullptr, 0);
    ctx->PSSetShaderResources(0, 1, &src);
    ctx->PSSetSamplers(0, 1, &sampler_);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none = nullptr;
    ctx->PSSetShaderResources(0, 1, &none);   // the source may be a target next
}

} // namespace dvr::gfx
