// tools/ovl-theme-preview.cpp - VR-197: render a sample F10 panel with the real theme
// (src/core/ui/ovl_ui.cpp) offscreen and save it as ovl-theme-preview.bmp, so the look can be
// judged without a headset. Built and run by tools/ovl-theme-preview.ps1. The sample content
// is representative, not the live panel: the real tabs need the game.
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <vector>
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "core/ui/ovl_ui.h"

static void SamplePanel(float w, float h, bool advanced)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::Begin("Dishonored VR", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
    dvr::ovl::title("Dishonored VR");
    dvr::ovl::controller_hint();
    ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("SHOW"); ImGui::SameLine();
    dvr::ovl::pill("Basic", !advanced); ImGui::SameLine(0, 4);
    dvr::ovl::pill("Advanced", advanced); ImGui::SameLine(0, 4);
    dvr::ovl::pill("Debug", false);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("IPD 63 mm").x);
    ImGui::TextDisabled("IPD 63 mm");
    ImGui::Spacing();
    const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    dvr::ovl::push_primary(); ImGui::Button("RECENTER", ImVec2(-1, 0)); dvr::ovl::pop_primary();
    ImGui::Button("SAVE AS DEFAULTS", ImVec2(half, 0));
    ImGui::SameLine(); ImGui::Button("RESET TO DEFAULTS", ImVec2(half, 0));
    static float height = 0.06f; ImGui::SliderFloat("Height offset (m)", &height, -1, 1, "%+.2f");
    ImGui::Spacing();
    if (ImGui::BeginTabBar("tabs")) {
        if (dvr::ovl::tab("Hands")) {
            if (dvr::ovl::section("Hand size and position", 0, nullptr, true)) {
                static float size = 0.85f; ImGui::SliderFloat("Hand / weapon size", &size, 0.4f, 1.6f, "%.2f");
                static int which = 0;
                ImGui::RadioButton("Left", &which, 0); ImGui::SameLine();
                ImGui::RadioButton("Right (in use)", &which, 1); ImGui::SameLine();
                ImGui::RadioButton("Left, powers", &which, 2);
                static int step = 1;
                ImGui::RadioButton("fine", &step, 0); ImGui::SameLine();
                ImGui::RadioButton("normal", &step, 1); ImGui::SameLine();
                ImGui::RadioButton("coarse", &step, 2);
                const char* rows[3][2] = { { "move left", "move right" }, { "move down", "move up" }, { "turn pitch down", "turn pitch up" } };
                for (auto& r : rows) {
                    ImGui::Button(r[0], ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 4, 0)); ImGui::SameLine();
                    ImGui::Button(r[1], ImVec2(ImGui::GetContentRegionAvail().x, 0));
                }
                if (advanced) { static bool p = true; ImGui::Checkbox("Separate left-hand position for powers", &p); }
            }
            if (dvr::ovl::section("Sleeve", 0, nullptr, true)) {
                if (ImGui::BeginCombo("Sleeve preset", "Cuffs")) ImGui::EndCombo();
                static float len = 12.0f; ImGui::SliderFloat("Sleeve length", &len, 0, 30, "%.1f");
                static bool round = true; ImGui::Checkbox("Rounded wrist ends", &round);
            }
            dvr::ovl::section("Game arms during actions", 0, nullptr);
            ImGui::EndTabItem();
        }
        if (dvr::ovl::tab("Aim")) ImGui::EndTabItem();
        if (dvr::ovl::tab("Controls")) ImGui::EndTabItem();
        if (dvr::ovl::tab("Comfort")) ImGui::EndTabItem();
        if (dvr::ovl::tab("HUD")) ImGui::EndTabItem();
        if (dvr::ovl::tab("Display")) ImGui::EndTabItem();
        ImGui::EndTabBar();
    }
    ImGui::Spacing();
    dvr::ovl::ornament();
    static float ts = 1.54f; ImGui::SliderFloat("Text size", &ts, 0.8f, 2.5f, "%.2f");
    ImGui::TextDisabled("F10 or a stick-click tap closes | point, trigger clicks, stick scrolls / nudges a slider");
    ImGui::End();
    // A tooltip, placed by hand, to show the parchment style.
    ImGui::SetNextWindowPos(ImVec2(w * 0.42f, h * 0.30f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0xDC / 255.f, 0xCF / 255.f, 0xB4 / 255.f, 1));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0x2A / 255.f, 0x22 / 255.f, 0x1A / 255.f, 1));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0x6E / 255.f, 0x56 / 255.f, 0x30 / 255.f, 1));
    ImGui::Begin("##tip", nullptr, ImGuiWindowFlags_Tooltip | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26.0f);
    ImGui::TextUnformatted("Scales the hands and whatever they hold, around your palm. Not the world scale.");
    ImGui::PopTextWrapPos();
    ImGui::End();
    ImGui::PopStyleColor(3);
}

static bool SaveBmp(const char* path, const uint8_t* rgba, int w, int h, int pitch)
{
    FILE* f = fopen(path, "wb"); if (!f) return false;
    BITMAPFILEHEADER fh = {}; BITMAPINFOHEADER ih = {};
    ih.biSize = sizeof(ih); ih.biWidth = w; ih.biHeight = -h; ih.biPlanes = 1; ih.biBitCount = 32; ih.biCompression = BI_RGB;
    fh.bfType = 0x4D42; fh.bfOffBits = sizeof(fh) + sizeof(ih); fh.bfSize = fh.bfOffBits + w * h * 4;
    fwrite(&fh, sizeof(fh), 1, f); fwrite(&ih, sizeof(ih), 1, f);
    std::vector<uint8_t> row(w * 4);
    for (int y = 0; y < h; ++y) {
        const uint8_t* s = rgba + y * pitch;
        for (int x = 0; x < w; ++x) { row[x*4+0] = s[x*4+2]; row[x*4+1] = s[x*4+1]; row[x*4+2] = s[x*4+0]; row[x*4+3] = 255; }
        fwrite(row.data(), 1, row.size(), f);
    }
    fclose(f); return true;
}

int main(int argc, char** argv)
{
    const bool advanced = argc > 1 && !strcmp(argv[1], "advanced");
    const int W = 1060, H = 1100;
    ID3D11Device* dev = nullptr; ID3D11DeviceContext* ctx = nullptr;
    D3D_FEATURE_LEVEL fl;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx)) &&
        FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, &ctx))) {
        printf("no D3D11 device\n"); return 1;
    }
    D3D11_TEXTURE2D_DESC td = {}; td.Width = W; td.Height = H; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1; td.BindFlags = D3D11_BIND_RENDER_TARGET;
    ID3D11Texture2D* rt = nullptr; dev->CreateTexture2D(&td, nullptr, &rt);
    ID3D11RenderTargetView* rtv = nullptr; dev->CreateRenderTargetView(rt, nullptr, &rtv);
    td.BindFlags = 0; td.Usage = D3D11_USAGE_STAGING; td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* st = nullptr; dev->CreateTexture2D(&td, nullptr, &st);

    ImGui::CreateContext();
    dvr::ovl::load_fonts();
    dvr::ovl::apply_theme();
    ImGui::GetStyle().ScaleAllSizes(1.6f);
    ImGui::GetStyle().FontScaleMain = 1.54f;
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2((float)W, (float)H);
    io.MousePos = ImVec2(-1, -1);
    ImGui_ImplDX11_Init(dev, ctx);
    for (int frame = 0; frame < 4; ++frame) {
        io.DeltaTime = 1.0f / 60.0f;
        ImGui_ImplDX11_NewFrame();
        ImGui::NewFrame();
        SamplePanel((float)W, (float)H, advanced);
        ImGui::Render();
        const float clear[4] = { 0.25f, 0.27f, 0.30f, 1 };   // stands in for the game image behind
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        ctx->ClearRenderTargetView(rtv, clear);
        D3D11_VIEWPORT vp = { 0, 0, (float)W, (float)H, 0, 1 }; ctx->RSSetViewports(1, &vp);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }
    ctx->CopyResource(st, rt);
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ctx->Map(st, 0, D3D11_MAP_READ, 0, &m))) { printf("map failed\n"); return 1; }
    const bool ok = SaveBmp("ovl-theme-preview.bmp", (const uint8_t*)m.pData, W, H, (int)m.RowPitch);
    ctx->Unmap(st, 0);
    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext();
    printf(ok ? "wrote ovl-theme-preview.bmp\n" : "write failed\n");
    return ok ? 0 : 1;
}
