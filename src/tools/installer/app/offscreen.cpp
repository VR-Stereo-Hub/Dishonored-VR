// tools/installer/app/offscreen.cpp - see offscreen.h.
#include "app/offscreen.h"
#include "ui/screens.h"
#include "sys/fs.h"
#include "core/ui/ovl_ui.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <vector>
#include <string.h>

namespace dvr::setup::app {

namespace {
bool save_bmp(const std::wstring& path, const uint8_t* rgba, int w, int h, int pitch, std::string* why)
{
    const int rowBytes = w * 3;
    const int padded = (rowBytes + 3) & ~3;
    const uint32_t imageSize = (uint32_t)padded * h;
    std::vector<uint8_t> out(54 + imageSize, 0);
    uint8_t* p = out.data();
    p[0] = 'B'; p[1] = 'M';
    const uint32_t fileSize = 54 + imageSize;
    memcpy(p + 2, &fileSize, 4);
    const uint32_t off = 54; memcpy(p + 10, &off, 4);
    const uint32_t hdr = 40; memcpy(p + 14, &hdr, 4);
    memcpy(p + 18, &w, 4); memcpy(p + 22, &h, 4);
    const uint16_t planes = 1, bpp = 24; memcpy(p + 26, &planes, 2); memcpy(p + 28, &bpp, 2);
    memcpy(p + 34, &imageSize, 4);
    for (int y = 0; y < h; ++y) {
        const uint8_t* src = rgba + (size_t)(h - 1 - y) * pitch;
        uint8_t* dst = p + 54 + (size_t)y * padded;
        for (int x = 0; x < w; ++x) { dst[x * 3] = src[x * 4 + 2]; dst[x * 3 + 1] = src[x * 4 + 1]; dst[x * 3 + 2] = src[x * 4]; }
    }
    DWORD err = 0;
    if (!fs::write_file_atomic(path, out.data(), out.size(), &err)) { *why = "write failed: " + fs::narrow(fs::win_error_text(err)); return false; }
    return true;
}
}

bool render_offscreen(ViewState& state, float scale, const std::wstring& outBmp, std::string* why)
{
    const int W = (int)(kLogicalWidth * scale + 0.5f), H = (int)(kLogicalHeight * scale + 0.5f);
    ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr; D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx)) &&
        FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx))) {
        *why = "no D3D11 device (hardware or WARP)"; return false;
    }
    D3D11_TEXTURE2D_DESC td = {}; td.Width = W; td.Height = H; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D* rt = nullptr; dev->CreateTexture2D(&td, nullptr, &rt);
    ID3D11RenderTargetView* rtv = nullptr; dev->CreateRenderTargetView(rt, nullptr, &rtv);
    td.BindFlags = 0; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* st = nullptr; dev->CreateTexture2D(&td, nullptr, &st);

    ImGui::CreateContext();
    dvr::ovl::load_fonts();
    dvr::ovl::load_art(dev);
    ui::load_guide(dev);
    dvr::ovl::apply_theme();
    ImGui::GetStyle().ScaleAllSizes(scale);
    ImGui::GetStyle().FontScaleDpi = scale;
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2((float)W, (float)H);
    io.MousePos = ImVec2(-1, -1);
    ImGui_ImplDX11_Init(dev, ctx);
    for (int frame = 0; frame < 4; ++frame) {
        io.DeltaTime = 1.0f / 60.0f;
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        ui::draw(state);
        ImGui::Render();
        const float clear[4] = { 0x12 / 255.0f, 0x14 / 255.0f, 0x17 / 255.0f, 1 };
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ctx->ClearRenderTargetView(rtv, clear);
        D3D11_VIEWPORT vp = { 0, 0, (float)W, (float)H, 0, 1 }; ctx->RSSetViewports(1, &vp);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
    ctx->CopyResource(st, rt);
    D3D11_MAPPED_SUBRESOURCE m;
    bool ok = false;
    if (SUCCEEDED(ctx->Map(st, 0, D3D11_MAP_READ, 0, &m))) {
        ok = save_bmp(outBmp, (const uint8_t*)m.pData, W, H, (int)m.RowPitch, why);
        ctx->Unmap(st, 0);
    } else {
        *why = "staging map failed";
    }
    ui::release_guide();
    dvr::ovl::release_art();
    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext();
    st->Release(); rtv->Release(); rt->Release(); ctx->Release(); dev->Release();
    return ok;
}

} // namespace dvr::setup::app
