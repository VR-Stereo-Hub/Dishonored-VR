// tools/ovl-theme-preview.cpp - VR-197: render a sample F10 panel with the real theme
// (src/core/ui/ovl_ui.cpp) offscreen and save it as ovl-theme-preview.bmp, so the look can be
// judged without a headset. Built and run by tools/ovl-theme-preview.ps1. The sample content
// is representative, not the live panel: the real tabs need the game.
#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <vector>
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx11.h"
#include "core/ui/ovl_ui.h"

static ImVec2 tipTarget(-1,-1);
static bool previewTip=false;
static bool previewBottom=false;
static bool previewDebug=false;
static bool layoutOk=true;
static int layoutChecks=0;
static float bodyBottom=0;
static void CheckLayout(bool pass, const char* what)
{
    ++layoutChecks;
    if (!pass) { layoutOk=false; printf("layout FAIL: %s\\n",what); }
}
static void SamplePanel(float w, float h, bool advanced)
{
    dvr::ovl::set_level(previewDebug?dvr::ovl::Debug:advanced?dvr::ovl::Advanced:dvr::ovl::Basic);
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(w, h));
    ImGui::Begin("Dishonored VR", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);
    dvr::ovl::backdrop();
    const ImVec2 titleStart=ImGui::GetCursorPos();
    dvr::ovl::title("DISHONORED VR");
    const ImVec2 titleEnd=ImGui::GetCursorPos();
    const float closeSize=ImGui::GetFrameHeight();
    ImGui::SetCursorPos(ImVec2(w-ImGui::GetStyle().WindowPadding.x-closeSize,titleStart.y));
    dvr::ovl::button("X##previewclose",ImVec2(closeSize,closeSize));
    ImGui::SetCursorPos(titleEnd);
    dvr::ovl::controller_hint();
    ImGui::AlignTextToFramePadding(); ImGui::TextDisabled("SHOW"); ImGui::SameLine();
    dvr::ovl::pill("Basic", !advanced); ImGui::SameLine(0, 4);
    dvr::ovl::pill("Advanced", advanced && !previewDebug); ImGui::SameLine(0, 4);
    dvr::ovl::pill("Debug", previewDebug);
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("IPD 63 mm").x);
    ImGui::TextDisabled("IPD 63 mm");
    ImGui::Spacing();
    const float actionWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3;
    dvr::ovl::push_primary(); dvr::ovl::button("RECENTER", ImVec2(actionWidth, 0)); dvr::ovl::pop_primary();
    ImGui::SameLine(); dvr::ovl::button("SAVE AS DEFAULTS", ImVec2(actionWidth, 0));
    ImGui::SameLine(); dvr::ovl::button("RESET TO DEFAULTS", ImVec2(actionWidth, 0));
    CheckLayout(actionWidth >= ImGui::CalcTextSize("RESET TO DEFAULTS").x + ImGui::GetStyle().FramePadding.x*2,"action label fit");
    static float height = 0.06f; dvr::ovl::slider_float("Height offset (m)", &height, -1, 1, "%+.2f");
    ImGui::Spacing();
    if (ImGui::BeginTabBar("tabs",ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_TabListPopupButton)) {
        if (dvr::ovl::tab("Hands")) {
            dvr::ovl::begin_body("hands-scroll");
            if (dvr::ovl::section("Hand size and position", 0, nullptr, true)) {
                static float size = 0.85f; dvr::ovl::slider_float("Hand / weapon size", &size, 0.4f, 1.6f, "%.2f");
                auto a=ImGui::GetItemRectMin(), b=ImGui::GetItemRectMax();
                tipTarget=ImVec2(a.x+(b.x-a.x)*.55f,b.y-3);
                if (previewTip) dvr::ovl::tip("Scales the hands and whatever they hold, around your palm. Not the world scale.");
                static int which = 0;
                dvr::ovl::radio_button("Left", &which, 0); ImGui::SameLine();
                dvr::ovl::radio_button("Right (in use)", &which, 1); ImGui::SameLine();
                dvr::ovl::radio_button("Left, powers", &which, 2);
                static int step = 1;
                dvr::ovl::radio_button("fine", &step, 0); ImGui::SameLine();
                dvr::ovl::radio_button("normal", &step, 1); ImGui::SameLine();
                dvr::ovl::radio_button("coarse", &step, 2);
                ImGui::SameLine();
                bool more=ImGui::TreeNode("More adjustments##mptrim");
                if (more) ImGui::TreePop();
                const char* rows[6][2] = { { "move left", "move right" }, { "move down", "move up" }, { "move back", "move forward" }, { "turn pitch down", "turn pitch up" }, { "turn yaw left", "turn yaw right" }, { "turn roll left", "turn roll right" } };
                auto row = [&](int i) { const auto& r=rows[i];
                    dvr::ovl::button(r[0], ImVec2(ImGui::GetContentRegionAvail().x * 0.5f - 4, 0)); ImGui::SameLine();
                    dvr::ovl::button(r[1], ImVec2(ImGui::GetContentRegionAvail().x, 0));
                };
                row(0); row(1); row(3);
                if (more) { row(2); row(4); row(5); }
                if (advanced) { static bool p = true; dvr::ovl::checkbox("Separate left-hand position for powers", &p);
                    if (more) { static bool view=true; dvr::ovl::checkbox("Numpad steps follow my view (not the palm axes)",&view); } }
            }
            if (dvr::ovl::section("Sleeve", 0, nullptr, true)) {
                if (ImGui::BeginCombo("Sleeve preset", "Cuffs")) ImGui::EndCombo();
                static float len = 12.0f; dvr::ovl::slider_float("Sleeve length", &len, 0, 30, "%.1f");
                if (advanced) { static bool round = true; dvr::ovl::checkbox("Rounded wrist ends", &round);
                    static float wrist=.2f; dvr::ovl::slider_float("Wrist roundness",&wrist,.05f,.8f); }
            }
            dvr::ovl::section("Game arms during actions", dvr::ovl::Advanced, nullptr);
            if (previewBottom) ImGui::SetScrollY(ImGui::GetScrollMaxY());
            bodyBottom=ImGui::GetWindowPos().y+ImGui::GetWindowSize().y;
            dvr::ovl::end_body();
            ImGui::EndTabItem();
        }
        if (dvr::ovl::tab("Aim")) ImGui::EndTabItem();
        if (dvr::ovl::tab("Controls")) ImGui::EndTabItem();
        if (dvr::ovl::tab("Comfort")) ImGui::EndTabItem();
        if (dvr::ovl::tab("HUD")) ImGui::EndTabItem();
        if (dvr::ovl::tab("Display")) ImGui::EndTabItem();
        if (advanced && dvr::ovl::tab("Game options")) ImGui::EndTabItem();
        if (previewDebug && dvr::ovl::tab("Runtime")) ImGui::EndTabItem();
        if (previewDebug && dvr::ovl::tab("Log")) ImGui::EndTabItem();
        ImGui::EndTabBar();
    }
    ImGui::Spacing();
    dvr::ovl::ornament();
    CheckLayout(ImGui::GetCursorScreenPos().y >= bodyBottom, "footer below scrolling body");
    float ts = ImGui::GetStyle().FontScaleMain; dvr::ovl::slider_float("Text size", &ts, 0.8f, 2.5f, "%.2f");
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("F10 or a stick-click tap closes | point, trigger clicks, stick scrolls / nudges a slider");
    ImGui::PopTextWrapPos();
    CheckLayout(ImGui::GetItemRectMax().y < h-ImGui::GetStyle().WindowPadding.y, "footer fits panel");
    ImGui::End();

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

// Exercise the production themed widgets through actual ImGui pointer events.
static bool InteractionChecks()
{
    dvr::ovl::set_level(dvr::ovl::Basic);
    bool checked=false, disabled=false, opened=false;
    int presses=0, selected=0; float value=0;
    ImVec2 check{}, button{}, slider{}, section{}, disabledBox{}, radio{};
    auto frame = [&](ImVec2 pointer, bool down) {
        auto& io=ImGui::GetIO(); io.ConfigInputTrickleEventQueue=false;
        io.AddMousePosEvent(pointer.x,pointer.y); io.AddMouseButtonEvent(0,down);
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(ImVec2(640,600));
        ImGui::Begin("interaction-check",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoSavedSettings);
        auto center=[] { auto a=ImGui::GetItemRectMin(), b=ImGui::GetItemRectMax(); return ImVec2((a.x+b.x)*.5f,(a.y+b.y)*.5f); };
        dvr::ovl::checkbox("Check",&checked); check=center();
        if (dvr::ovl::button("Press")) ++presses; button=center();
        ImGui::SetNextItemWidth(200);
        dvr::ovl::slider_float("Slide",&value,0,1);
        auto a=ImGui::GetItemRectMin(); slider=ImVec2(a.x+160,center().y);
        opened=dvr::ovl::section("Expand",0,nullptr); section=center();
        dvr::ovl::radio_button("Radio",&selected,1); radio=center();
        ImGui::BeginDisabled(); dvr::ovl::checkbox("Disabled",&disabled); disabledBox=center(); ImGui::EndDisabled();
        ImGui::End(); ImGui::Render();
    };
    auto click=[&](ImVec2 p) { frame(p,false); frame(p,true); frame(p,false); };
    frame(ImVec2(-1,-1),false); frame(ImVec2(-1,-1),false);
    click(check); click(button); click(slider); click(section); click(radio); click(disabledBox);
    const bool pass=checked && presses==1 && value>.65f && opened && selected==1 && !disabled;
    printf("widget interaction: checkbox=%d button=%d slider=%.3f section=%d disabled=%d: %s\n",
        checked,presses,value,opened,disabled,pass?"PASS":"FAIL");
    return pass;
}

// Exercise the always-visible tab list through pointer events, including the
// offscreen last tab at the accepted narrow panel size.
static bool NavigationChecks()
{
    auto& io=ImGui::GetIO();
    ImGui::GetStyle().FontScaleMain=1.0f;
    ImVec2 arrow{};
    bool logSelected=false;
    auto frame=[&](ImVec2 pointer,bool down) {
        io.AddMousePosEvent(pointer.x,pointer.y); io.AddMouseButtonEvent(0,down);
        ImGui_ImplDX11_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(ImVec2(649,685));
        ImGui::Begin("navigation-check",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoSavedSettings);
        const ImVec2 p=ImGui::GetCursorScreenPos();
        arrow=ImVec2(p.x+8,p.y+ImGui::GetFrameHeight()*.5f);
        logSelected=false;
        if (ImGui::BeginTabBar("navtabs",ImGuiTabBarFlags_FittingPolicyScroll|ImGuiTabBarFlags_TabListPopupButton)) {
            const char* labels[]={"Hands","Aim","Controls","Comfort","HUD","Display","Game options","Runtime","Log"};
            for (auto label:labels) if (dvr::ovl::tab(label)) {
                if (!strcmp(label,"Log")) logSelected=true;
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End(); ImGui::Render();
    };
    auto click=[&](ImVec2 p) { frame(p,false); frame(p,true); frame(p,false); };
    frame(ImVec2(-1,-1),false); frame(ImVec2(-1,-1),false);
    click(arrow); frame(arrow,false);
    ImGuiWindow* popup=ImGui::FindWindowByName("##Combo_00");
    const bool opened=popup && popup->Active && !GImGui->OpenPopupStack.empty();
    if (opened) {
        const float line=ImGui::GetFontSize()+ImGui::GetStyle().ItemSpacing.y;
        click(ImVec2(popup->Pos.x+popup->WindowPadding.x+20,
            popup->Pos.y+popup->WindowPadding.y+line*8+ImGui::GetFontSize()*.5f));
        frame(ImVec2(-1,-1),false);
    }
    printf("tab navigation: list=%d offscreen Log selected=%d: %s\n",
        opened,logSelected,opened&&logSelected?"PASS":"FAIL");
    return opened&&logSelected;
}

int main(int argc, char** argv)
{
    previewDebug=argc>1 && !strcmp(argv[1],"debug");
    const bool advanced = previewDebug || (argc > 1 && !strcmp(argv[1], "advanced"));
    previewTip = argc > 3 && !strcmp(argv[3],"tip");
    previewBottom = argc>3 && !strcmp(argv[3],"bottom");
    const int W = argc > 2 ? atoi(argv[2]) : 1254, H = argc>5 ? atoi(argv[5]) : W;
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
    dvr::ovl::load_art(dev);
    ImGui::GetStyle().ScaleAllSizes(1.6f);
    ImGui::GetStyle().FontScaleMain = argc>4 ? (float)atof(argv[4]) : 1.54f * W / 1254.0f;
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2((float)W, (float)H);
    io.MousePos = ImVec2(-1, -1);
    ImGui_ImplDX11_Init(dev, ctx);
    for (int frame = 0; frame < 4; ++frame) {
        io.DeltaTime = 1.0f / 60.0f;
        ImGui_ImplDX11_NewFrame();
        if (previewTip && frame>1) io.AddMousePosEvent(tipTarget.x,tipTarget.y);
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
    const bool interactions = InteractionChecks();
    const bool navigation=NavigationChecks();
    dvr::ovl::release_art();
    ImGui_ImplDX11_Shutdown();
    ImGui::DestroyContext();
    printf(ok ? "wrote ovl-theme-preview.bmp\n" : "write failed\n");
    printf("layout: %d checks: %s\n",layoutChecks,layoutOk?"PASS":"FAIL");
    return ok && interactions && navigation && layoutOk ? 0 : 1;
}
