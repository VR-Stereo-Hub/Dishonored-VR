// core/gfx/motion_gpu.cpp - see motion_gpu.h. No mod dependencies.
#include "core/gfx/motion_gpu.h"

#include <windows.h>
#include <d3d11.h>
#include <d3dcommon.h>
#include <stdio.h>
#include <string.h>

namespace dvr::motion {
namespace {

typedef HRESULT (WINAPI *PFN_D3DCompile)(LPCVOID, SIZE_T, LPCSTR, const void*, void*, LPCSTR,
                                         LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);

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
    "    float4 gRowF, gRowR, gRowU;   // prev_from_cur rows\n"
    "    float4 gT;                    // translation (uu, previous camera axes), far depth\n"
    "    float4 gTan;                  // tanH, tanV, w, h\n"
    "    float4 gGrid;                 // grid w, grid h\n"
    "    float4 gScale[3];             // the 12 candidate uu per depth unit\n"
    "};\n"
    "Texture2D tCur : register(t0);\n"
    "Texture2D tPrev : register(t1);\n"
    "Texture2D tDepth : register(t2);\n"
    "SamplerState sLinear : register(s0);\n"
    "float Luma(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }\n"
    "float4 ps_calib(VSOut i) : SV_Target {\n"
    "    const int px = (int)i.pos.x, py = (int)i.pos.y;\n"
    "    const int k = px / (int)gGrid.x, g = px - k * (int)gGrid.x;\n"
    "    const float s = gScale[k / 4][k % 4];\n"
    "    const float2 uv = (float2(g, py) + 0.5) / gGrid.xy;\n"
    "    const int2 p = int2(uv * gTan.zw);\n"
    "    const float d = tDepth.Load(int3(p, 0)).a;\n"
    "    if (!(d > 0.0 && d < gT.w)) return float4(0, 0, 0, 0);\n"
    "    const float du = d * s;\n"
    "    const float3 dir = float3(du, (uv.x * 2.0 - 1.0) * gTan.x * du, (1.0 - uv.y * 2.0) * gTan.y * du);\n"
    "    const float3 q = float3(dot(gRowF.xyz, dir), dot(gRowR.xyz, dir), dot(gRowU.xyz, dir)) + gT.xyz;\n"
    "    if (q.x <= 1.0) return float4(0, 0, 0, 0);\n"
    "    const float2 puv = float2(0.5 + 0.5 * q.y / (q.x * gTan.x), 0.5 - 0.5 * q.z / (q.x * gTan.y));\n"
    "    if (any(puv < 0.0) || any(puv > 1.0)) return float4(0, 0, 0, 0);\n"
    "    const float lc = Luma(tCur.Load(int3(p, 0)).rgb);\n"
    "    const float lp = Luma(tPrev.SampleLevel(sLinear, puv, 0).rgb);\n"
    "    return float4(abs(lc - lp), 1.0, 0, 0);\n"
    "}\n";

void say(char* why, size_t cap, const char* t) { if (why && cap) strncpy_s(why, cap, t, _TRUNCATE); }
template <class T> void rel(T*& p) { if (p) { p->Release(); p = nullptr; } }

} // namespace

bool CalibGpu::init(ID3D11Device* dev, char* why, size_t cap) {
    if (ready_) return true;
    if (failed_ || !dev) { say(why, cap, "no device or an earlier failure"); return false; }
    HMODULE compiler = LoadLibraryA("d3dcompiler_47.dll");
    PFN_D3DCompile compile = compiler ? (PFN_D3DCompile)GetProcAddress(compiler, "D3DCompile") : nullptr;
    if (!compile) { failed_ = true; say(why, cap, "d3dcompiler_47.dll missing"); return false; }
    ID3DBlob *vb = nullptr, *pb = nullptr, *err = nullptr;
    if (FAILED(compile(kSrc, strlen(kSrc), "motion", nullptr, nullptr, "vsmain", "vs_4_0", 0, 0, &vb, &err)) ||
        FAILED(compile(kSrc, strlen(kSrc), "motion", nullptr, nullptr, "ps_calib", "ps_4_0", 0, 0, &pb, &err))) {
        char t[512]; _snprintf_s(t, sizeof(t), _TRUNCATE, "compile failed: %s", err ? (const char*)err->GetBufferPointer() : "?");
        say(why, cap, t);
        if (err) err->Release();
        if (vb) vb->Release();
        failed_ = true; return false;
    }
    HRESULT hr = dev->CreateVertexShader(vb->GetBufferPointer(), vb->GetBufferSize(), nullptr, &vs_);
    if (SUCCEEDED(hr)) hr = dev->CreatePixelShader(pb->GetBufferPointer(), pb->GetBufferSize(), nullptr, &ps_);
    vb->Release(); pb->Release();
    D3D11_BUFFER_DESC bd = {}; bd.ByteWidth = 9 * 16; bd.Usage = D3D11_USAGE_DEFAULT; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (SUCCEEDED(hr)) hr = dev->CreateBuffer(&bd, nullptr, &cb_);
    D3D11_SAMPLER_DESC sd = {}; sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP; sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (SUCCEEDED(hr)) hr = dev->CreateSamplerState(&sd, &linear_);
    D3D11_RASTERIZER_DESC rd = {}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE; rd.DepthClipEnable = TRUE;
    if (SUCCEEDED(hr)) hr = dev->CreateRasterizerState(&rd, &raster_);
    D3D11_BLEND_DESC bl = {}; bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (SUCCEEDED(hr)) hr = dev->CreateBlendState(&bl, &blend_);
    D3D11_DEPTH_STENCIL_DESC dd = {}; dd.DepthEnable = FALSE;
    if (SUCCEEDED(hr)) hr = dev->CreateDepthStencilState(&dd, &depth_);
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = kGridW * kScales; td.Height = kGridH; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R32G32_FLOAT; td.SampleDesc.Count = 1; td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (SUCCEEDED(hr)) hr = dev->CreateTexture2D(&td, nullptr, &out_);
    if (SUCCEEDED(hr)) hr = dev->CreateRenderTargetView(out_, nullptr, &outRtv_);
    td.Usage = D3D11_USAGE_STAGING; td.BindFlags = 0; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    if (SUCCEEDED(hr)) hr = dev->CreateTexture2D(&td, nullptr, &stage_);
    if (FAILED(hr)) {
        char t[96]; _snprintf_s(t, sizeof(t), _TRUNCATE, "objects failed 0x%08lx", (unsigned long)hr);
        say(why, cap, t); failed_ = true; shutdown(); return false;
    }
    ready_ = true;
    return true;
}

void CalibGpu::shutdown() {
    rel(stage_); rel(outRtv_); rel(out_); rel(depth_); rel(blend_); rel(raster_); rel(linear_); rel(cb_); rel(ps_); rel(vs_);
    ready_ = false;
}

bool CalibGpu::run(ID3D11Device* dev, ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* cur,
                   ID3D11ShaderResourceView* prev, ID3D11ShaderResourceView* depth, const CalibParams& p,
                   float err[kScales], int* samples, char* why, size_t cap) {
    (void)dev;
    if (!ready_ || !ctx || !cur || !prev || !depth || !p.w || !p.h) { say(why, cap, "not ready or missing input"); return false; }
    float cb[36] = {};
    for (int j = 0; j < 3; ++j) { cb[0 + j] = p.prevFromCur.m[0][j]; cb[4 + j] = p.prevFromCur.m[1][j]; cb[8 + j] = p.prevFromCur.m[2][j]; }
    cb[12] = p.t[0]; cb[13] = p.t[1]; cb[14] = p.t[2]; cb[15] = p.farDepth;
    cb[16] = p.tanH; cb[17] = p.tanV; cb[18] = (float)p.w; cb[19] = (float)p.h;
    cb[20] = (float)kGridW; cb[21] = (float)kGridH;
    for (int k = 0; k < kScales; ++k) cb[24 + k] = kScaleValues[k];
    ctx->UpdateSubresource(cb_, 0, nullptr, cb, 0, 0);
    D3D11_VIEWPORT vp = {0, 0, (float)(kGridW * kScales), (float)kGridH, 0, 1};
    ctx->RSSetViewports(1, &vp);
    ctx->RSSetState(raster_);
    ctx->OMSetRenderTargets(1, &outRtv_, nullptr);
    const float bf[4] = {0, 0, 0, 0};
    ctx->OMSetBlendState(blend_, bf, 0xffffffff);
    ctx->OMSetDepthStencilState(depth_, 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs_, nullptr, 0);
    ctx->PSSetShader(ps_, nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, &cb_);
    ID3D11ShaderResourceView* srvs[3] = {cur, prev, depth};
    ctx->PSSetShaderResources(0, 3, srvs);
    ctx->PSSetSamplers(0, 1, &linear_);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none[3] = {nullptr, nullptr, nullptr};
    ctx->PSSetShaderResources(0, 3, none);
    ID3D11RenderTargetView* noRt = nullptr;
    ctx->OMSetRenderTargets(1, &noRt, nullptr);
    ctx->CopyResource(stage_, out_);
    D3D11_MAPPED_SUBRESOURCE m = {};
    if (FAILED(ctx->Map(stage_, 0, D3D11_MAP_READ, 0, &m))) { say(why, cap, "readback refused"); return false; }
    int n = 0;
    for (int k = 0; k < kScales; ++k) {
        double e = 0, wsum = 0;
        for (int y = 0; y < kGridH; ++y) {
            const float* row = (const float*)((const uint8_t*)m.pData + y * m.RowPitch);
            for (int x = 0; x < kGridW; ++x) { e += row[(k * kGridW + x) * 2]; wsum += row[(k * kGridW + x) * 2 + 1]; }
        }
        err[k] = wsum > 0 ? (float)(e / wsum) : -1.0f;
        if (k == kScales - 1) n = (int)wsum;
    }
    ctx->Unmap(stage_, 0);
    if (samples) *samples = n;
    return true;
}

} // namespace dvr::motion
