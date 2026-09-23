// core/ui/ovl_ui.cpp - see ovl_ui.h (VR-196 tiers, VR-197 theme).
#include "core/ui/ovl_ui.h"
#include <atomic>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>
#include "imgui.h"
#include "core/util/log.h"
#undef DVR_CAT
#define DVR_CAT ::dvr::log::Cat::overlay

namespace dvr::ovl {
namespace {
std::atomic<int> g_level{Basic};
ImFont* g_body = nullptr;
ImFont* g_head = nullptr;
ImFont* g_headBold = nullptr;
ImFont* g_italic = nullptr;
ImFont* g_section = nullptr;
using Microsoft::WRL::ComPtr;
ComPtr<ID3D11Device> g_artDevice;
ComPtr<ID3D11ShaderResourceView> g_art[5];
bool g_primary = false;

bool decode_art(ID3D11Device* device, HMODULE module, int id, ID3D11ShaderResourceView** out)
{
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    if (!resource) return false;
    const DWORD bytes = SizeofResource(module, resource);
    auto* data = (BYTE*)LockResource(LoadResource(module, resource));
    if (!data || !bytes) return false;
    ComPtr<IWICImagingFactory> factory; ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder; ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
        FAILED(factory->CreateStream(&stream)) || FAILED(stream->InitializeFromMemory(data, bytes)) ||
        FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) ||
        FAILED(decoder->GetFrame(0, &frame)) || FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
            nullptr, 0, WICBitmapPaletteTypeCustom))) return false;
    UINT w = 0, h = 0;
    if (FAILED(converter->GetSize(&w, &h)) || !w || !h || w > 4096 || h > 4096) return false;
    std::vector<BYTE> rgba((size_t)w * h * 4);
    if (FAILED(converter->CopyPixels(nullptr, w * 4, (UINT)rgba.size(), rgba.data()))) return false;
    D3D11_TEXTURE2D_DESC desc{}; desc.Width=w; desc.Height=h; desc.MipLevels=1; desc.ArraySize=1;
    desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
    desc.Usage=D3D11_USAGE_IMMUTABLE; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{rgba.data(), w * 4, 0};
    ComPtr<ID3D11Texture2D> texture;
    return SUCCEEDED(device->CreateTexture2D(&desc, &initial, &texture)) &&
        SUCCEEDED(device->CreateShaderResourceView(texture.Get(), nullptr, out));
}
void material(int index, ImVec2 a, ImVec2 b, ImU32 tint = IM_COL32_WHITE)
{
    ImDrawList* dl=ImGui::GetWindowDrawList();
    ImVec4 color=ImGui::ColorConvertU32ToFloat4(tint);
    color.w *= ImGui::GetStyle().Alpha;
    tint=ImGui::ColorConvertFloat4ToU32(color);
    if (index == 2) {
        // Quiet slate gradient, with the painted metal grain barely visible.
        const bool red=g_primary;
        dl->AddRectFilledMultiColor(a,b,
            ImGui::GetColorU32(red ? ImVec4(.26f,.085f,.065f,1) : ImVec4(.10f,.14f,.16f,1)),
            ImGui::GetColorU32(red ? ImVec4(.20f,.055f,.045f,1) : ImVec4(.075f,.10f,.115f,1)),
            ImGui::GetColorU32(red ? ImVec4(.16f,.05f,.04f,1) : ImVec4(.06f,.08f,.09f,1)),
            ImGui::GetColorU32(red ? ImVec4(.21f,.065f,.05f,1) : ImVec4(.085f,.115f,.13f,1)));
        if (g_art[2]) dl->AddImage((ImTextureID)(uintptr_t)g_art[2].Get(),a,b,
            ImVec2(0,0),ImVec2(1,1),ImGui::GetColorU32(ImVec4(1,1,1,.07f)));
        dl->AddRect(a,b,ImGui::GetColorU32(red ? ImVec4(.48f,.24f,.20f,.8f) : ImVec4(.32f,.39f,.41f,.65f)),2);
        return;
    }
    if (!g_art[index] && index != 0) dl->AddRectFilled(a,b,ImGui::GetColorU32(ImVec4(.86f,.81f,.71f,1)));
    ImVec2 uv0(0,0),uv1(1,1);
    if (index == 1) { uv0.y=.35f; uv1.y=.65f; }
    if (index == 3) { uv0.y=.29f; uv1.y=.71f; }
    if (index == 4) { uv0.y=.06f; uv1.y=.94f; }
    if (index == 3 && g_art[3]) {
        const float cap=(b.y-a.y)*.65f;
        const ImTextureID texture=(ImTextureID)(uintptr_t)g_art[3].Get();
        dl->AddImage(texture,a,ImVec2(a.x+cap,b.y),ImVec2(0,uv0.y),ImVec2(.15f,uv1.y),tint);
        dl->AddImage(texture,ImVec2(a.x+cap,a.y),ImVec2(b.x-cap,b.y),ImVec2(.15f,uv0.y),ImVec2(.85f,uv1.y),tint);
        dl->AddImage(texture,ImVec2(b.x-cap,a.y),b,ImVec2(.85f,uv0.y),ImVec2(1,uv1.y),tint);
        return;
    }
    if (g_art[index]) dl->AddImage((ImTextureID)(uintptr_t)g_art[index].Get(),a,b,uv0,uv1,tint);
}
// Keep native ImGui interaction/IDs/nav and put a reusable texture behind its draw commands.
struct Skin {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImDrawListSplitter split;
    Skin() { split.Split(dl, 2); split.SetCurrentChannel(dl, 1); }
    void finish(int index, ImVec2 a, ImVec2 b, ImU32 tint=IM_COL32_WHITE) {
        split.SetCurrentChannel(dl, 0); material(index, a, b, tint); split.Merge(dl);
    }
};

// The palette, sRGB. Named for what they are in Dunwall, not for where they are used.
constexpr ImVec4 rgb(int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); }
const ImVec4 kInk       = rgb(0x12, 0x14, 0x17);   // the night sky over the Wrenhaven
const ImVec4 kSlate     = rgb(0x1B, 0x1E, 0x22);   // wet slate
const ImVec4 kSlateHi   = rgb(0x26, 0x2A, 0x2F);
const ImVec4 kSlateAct  = rgb(0x30, 0x35, 0x3B);
const ImVec4 kSoot      = rgb(0x0E, 0x0F, 0x11);
const ImVec4 kUmber     = rgb(0x1F, 0x1B, 0x17);   // oiled wood
const ImVec4 kUmberHi   = rgb(0x2E, 0x26, 0x1C);
const ImVec4 kUmberAct  = rgb(0x3A, 0x2F, 0x22);
const ImVec4 kBone      = rgb(0xE8, 0xDF, 0xCC);   // bone and parchment
const ImVec4 kFaded     = rgb(0x8F, 0x85, 0x74);   // faded sepia ink
const ImVec4 kBrass     = rgb(0xC3, 0x9A, 0x4E);   // whale-oil lamplight on brass
const ImVec4 kBrassHi   = rgb(0xDD, 0xB5, 0x66);
const ImVec4 kBrassDim  = rgb(0x6E, 0x56, 0x30);
const ImVec4 kOxblood   = rgb(0x5A, 0x1C, 0x17);   // dried blood, the Lord Protector's red
const ImVec4 kOxbloodHi = rgb(0x74, 0x25, 0x1E);
const ImVec4 kRule      = rgb(0x55, 0x5D, 0x5C);   // brass gone dull
const ImVec4 kParchment = rgb(0xDC, 0xCF, 0xB4);
const ImVec4 kInkText   = rgb(0x2A, 0x22, 0x1A);

ImFont* try_font(const char* const* files, int n, float size, const char* role)
{
    char dir[MAX_PATH] = "";
    if (!GetWindowsDirectoryA(dir, sizeof(dir))) return nullptr;
    for (int i = 0; i < n; ++i) {
        char path[MAX_PATH];
        _snprintf(path, sizeof(path), "%s\\Fonts\\%s", dir, files[i]);
        path[sizeof(path) - 1] = 0;
        if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) continue;
        ImFontConfig cfg;
        cfg.OversampleH = 2;
        if (ImFont* f = ImGui::GetIO().Fonts->AddFontFromFileTTF(path, size, &cfg)) {
            DVR_INFO("overlay/theme: %s font %s", role, files[i]);
            return f;
        }
    }
    DVR_WARN("overlay/theme: no %s font found in %s\\Fonts - ImGui's own font is used", role, dir);
    return nullptr;
}
} // namespace

int level() { return g_level.load(std::memory_order_relaxed); }
void set_level(int tier) { g_level.store(tier < Basic ? Basic : tier > Debug ? Debug : tier, std::memory_order_relaxed); }
const char* level_name(int tier) { return tier == Debug ? "debug" : tier == Advanced ? "advanced" : "basic"; }
int parse_level(const char* s, int fallback)
{
    if (!s || !*s) return fallback;
    if (!_stricmp(s, "basic") || !strcmp(s, "0")) return Basic;
    if (!_stricmp(s, "advanced") || !strcmp(s, "1")) return Advanced;
    if (!_stricmp(s, "debug") || !strcmp(s, "2")) return Debug;
    return fallback;
}

void load_fonts()
{
    // The body first: the first font added is ImGui's default.
    static const char* const kBody[] = { "times.ttf", "constan.ttf", "segoeui.ttf" };
    static const char* const kHead[] = { "times.ttf", "constan.ttf", "pala.ttf", "georgia.ttf" };
    static const char* const kHeadBold[] = { "PERTILI.TTF", "times.ttf", "constan.ttf", "georgia.ttf" };
    g_body = try_font(kBody, 3, 16.0f, "body");
    if (!g_body) ImGui::GetIO().Fonts->AddFontDefault();
    g_head = try_font(kHead, 4, 16.0f, "heading");
    static const char* const sectionFont[] = { "timesbd.ttf", "constanb.ttf" };
    g_section = try_font(sectionFont,2,17.0f,"section");
    g_headBold = try_font(kHeadBold, 4, 36.0f, "title");
    if (!g_headBold) g_headBold = g_head;
    static const char* const italic[] = { "timesi.ttf", "constani.ttf" };
    g_italic = try_font(italic, 2, 15.0f, "note");
}

ImFont* heading_font() { return g_head; }

void apply_theme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    ImGui::StyleColorsDark(&s);   // a complete base; everything that matters is set below
    s.FontSizeBase = 16.0f;
    s.WindowPadding = ImVec2(24, 16);
    s.FramePadding = ImVec2(12, 5);
    s.ItemSpacing = ImVec2(8, 6);
    s.ItemInnerSpacing = ImVec2(6, 4);
    s.IndentSpacing = 16;
    s.ScrollbarSize = 12;
    s.GrabMinSize = 10;
    s.WindowRounding = 3;
    s.ChildRounding = 2;
    s.FrameRounding = 2;
    s.PopupRounding = 2;
    s.ScrollbarRounding = 2;
    s.GrabRounding = 1;
    s.TabRounding = 2;
    s.WindowBorderSize = 1;
    s.FrameBorderSize = 1;
    s.PopupBorderSize = 1;
    s.TabBorderSize = 0;
    s.TabBarBorderSize = 1;
    s.TabBarOverlineSize = 2;
    s.SeparatorTextBorderSize = 1;
    s.SeparatorTextPadding = ImVec2(16, 3);
    s.WindowTitleAlign = ImVec2(0.5f, 0.5f);

    ImVec4* c = s.Colors;
    c[ImGuiCol_Text]                  = kBone;
    c[ImGuiCol_TextDisabled]          = kFaded;
    c[ImGuiCol_WindowBg]              = ImVec4(kInk.x, kInk.y, kInk.z, 0.96f);
    c[ImGuiCol_ChildBg]               = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PopupBg]               = ImVec4(kSoot.x, kSoot.y, kSoot.z, 0.98f);
    c[ImGuiCol_Border]                = ImVec4(kRule.x, kRule.y, kRule.z, 0.85f);
    c[ImGuiCol_BorderShadow]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg]               = kSlate;
    c[ImGuiCol_FrameBgHovered]        = kSlateHi;
    c[ImGuiCol_FrameBgActive]         = kSlateAct;
    c[ImGuiCol_TitleBg]               = kSoot;
    c[ImGuiCol_TitleBgActive]         = kSoot;
    c[ImGuiCol_TitleBgCollapsed]      = kSoot;
    c[ImGuiCol_MenuBarBg]             = kSoot;
    c[ImGuiCol_ScrollbarBg]           = ImVec4(kSoot.x, kSoot.y, kSoot.z, 0.5f);
    c[ImGuiCol_ScrollbarGrab]         = kRule;
    c[ImGuiCol_ScrollbarGrabHovered]  = kBrassDim;
    c[ImGuiCol_ScrollbarGrabActive]   = kBrass;
    c[ImGuiCol_CheckMark]             = kBrassHi;
    c[ImGuiCol_CheckboxSelectedBg]    = ImVec4(0,0,0,0);
    c[ImGuiCol_SliderGrab]            = kBrass;
    c[ImGuiCol_SliderGrabActive]      = kBrassHi;
    c[ImGuiCol_Button]                = kSlateHi;
    c[ImGuiCol_ButtonHovered]         = kUmberAct;
    c[ImGuiCol_ButtonActive]          = kBrassDim;
    c[ImGuiCol_Header]                = kUmber;
    c[ImGuiCol_HeaderHovered]         = kUmberHi;
    c[ImGuiCol_HeaderActive]          = kUmberAct;
    c[ImGuiCol_Separator]             = kRule;
    c[ImGuiCol_SeparatorHovered]      = kBrassDim;
    c[ImGuiCol_SeparatorActive]       = kBrass;
    c[ImGuiCol_ResizeGrip]            = ImVec4(kBrassDim.x, kBrassDim.y, kBrassDim.z, 0.35f);
    c[ImGuiCol_ResizeGripHovered]     = ImVec4(kBrass.x, kBrass.y, kBrass.z, 0.6f);
    c[ImGuiCol_ResizeGripActive]      = kBrassHi;
    c[ImGuiCol_InputTextCursor]       = kBrassHi;
    c[ImGuiCol_Tab]                   = kSoot;
    c[ImGuiCol_TabHovered]            = kUmberHi;
    c[ImGuiCol_TabSelected]           = kUmber;
    c[ImGuiCol_TabSelectedOverline]   = kBrass;
    // The panel is rarely the focused window in the headset, so the dimmed tab colours are
    // the same as the focused ones: the selected tab must read either way.
    c[ImGuiCol_TabDimmed]             = kSoot;
    c[ImGuiCol_TabDimmedSelected]     = kUmber;
    c[ImGuiCol_TabDimmedSelectedOverline] = kBrass;
    c[ImGuiCol_PlotLines]             = kBrass;
    c[ImGuiCol_PlotLinesHovered]      = kBrassHi;
    c[ImGuiCol_PlotHistogram]         = kBrass;
    c[ImGuiCol_PlotHistogramHovered]  = kBrassHi;
    c[ImGuiCol_TableHeaderBg]         = kUmber;
    c[ImGuiCol_TableBorderStrong]     = kRule;
    c[ImGuiCol_TableBorderLight]      = ImVec4(kRule.x, kRule.y, kRule.z, 0.5f);
    c[ImGuiCol_TableRowBg]            = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]         = ImVec4(1, 1, 1, 0.02f);
    c[ImGuiCol_TextLink]              = kBrassHi;
    c[ImGuiCol_TextSelectedBg]        = ImVec4(kBrass.x, kBrass.y, kBrass.z, 0.35f);
    c[ImGuiCol_TreeLines]             = kRule;
    c[ImGuiCol_DragDropTarget]        = kBrassHi;
    c[ImGuiCol_NavCursor]             = kBrass;
    c[ImGuiCol_NavWindowingHighlight] = kBrass;
    c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0, 0, 0, 0.5f);
    c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0, 0, 0, 0.5f);
    (void)kOxbloodHi;
}

void note(const char* text, float width)
{
    Skin skin;
    if (g_italic) ImGui::PushFont(g_italic,0);
    ImGui::PushStyleColor(ImGuiCol_Text, kInkText);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (width > 0 ? width : ImGui::GetFontSize() * 17.0f));
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    if (g_italic) ImGui::PopFont();
    const ImVec2 at=ImGui::GetCursorScreenPos();
    const float w=width>0?width:ImGui::GetFontSize()*17;
    const float mid=at.x+w*.5f, y=at.y+5;
    const ImU32 ink=ImGui::GetColorU32(ImVec4(.24f,.26f,.24f,.65f));
    auto* dl=ImGui::GetWindowDrawList();
    dl->AddLine(ImVec2(at.x+w*.15f,y),ImVec2(mid-13,y),ink);
    dl->AddLine(ImVec2(mid+13,y),ImVec2(at.x+w*.85f,y),ink);
    dl->AddQuad(ImVec2(mid,y-7),ImVec2(mid+4,y),ImVec2(mid,y+7),ImVec2(mid-4,y),ink,1.5f);
    ImGui::Dummy(ImVec2(w,14));
    const ImVec2 p=ImGui::GetWindowPos(), z=ImGui::GetWindowSize();
    skin.finish(4, p, ImVec2(p.x+z.x,p.y+z.y));
}
void tip(const char* text)
{
    if (!text || !*text || !ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) return;
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0,0,0,0));
    ImGui::BeginTooltip();
    note(text);
    ImGui::EndTooltip();
    ImGui::PopStyleColor(2);
}

bool section(const char* name, int tier, const char* tipText, bool defaultOpen)
{
    if (!show(tier)) return false;
    ImGui::Spacing();
    if (g_section) ImGui::PushFont(g_section, ImGui::GetStyle().FontSizeBase*1.06f);
    Skin skin;
    const ImVec2 start=ImGui::GetCursorScreenPos();
    const float width=ImGui::GetContentRegionAvail().x;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(1,1,1,0.10f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0,0,0,0.10f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
    const bool open = ImGui::CollapsingHeader(name, defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    ImGui::PopStyleVar();
    const ImVec2 end(start.x+width,ImGui::GetItemRectMax().y);
    skin.finish(3,start,end);
    auto* dl=ImGui::GetWindowDrawList();
    const ImU32 ink=ImGui::GetColorU32(kInkText);
    const float h=end.y-start.y, cx=start.x+h*.85f, cy=start.y+h*.5f, r=h*.27f;
    dl->AddQuad(ImVec2(cx,cy-r),ImVec2(cx+r,cy),ImVec2(cx,cy+r),ImVec2(cx-r,cy),ink,1.4f);
    const float inner=r*.62f;
    if (open) dl->AddQuadFilled(ImVec2(cx,cy-inner),ImVec2(cx+inner,cy),ImVec2(cx,cy+inner),ImVec2(cx-inner,cy),ink);
    else dl->AddQuad(ImVec2(cx,cy-inner),ImVec2(cx+inner,cy),ImVec2(cx,cy+inner),ImVec2(cx-inner,cy),ink,1);
    dl->AddText(ImVec2(start.x+h*1.9f,start.y+ImGui::GetStyle().FramePadding.y),ink,name);
    ImGui::PopStyleColor(4);
    if (g_section) ImGui::PopFont();
    tip(tipText);
    ImGui::Spacing();
    return open;
}

bool tab(const char* label)
{
    static ImGuiID selected = 0;
    const ImGuiID id = ImGui::GetID(label);
    const bool wasSelected = id == selected;
    if (g_head) ImGui::PushFont(g_head, 0.0f);
    Skin skin;
    ImGui::PushStyleColor(ImGuiCol_Text, wasSelected ? kInkText : kBone);
    ImGui::PushStyleColor(ImGuiCol_Tab, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_TabSelected, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_TabDimmedSelected, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_TabHovered, ImVec4(1,1,1,0.08f));
    const bool open = ImGui::BeginTabItem(label);
    skin.finish(open ? 1 : 2, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    ImGui::PopStyleColor(5);
    if (g_head) ImGui::PopFont();
    if (open) selected = id;
    return open;
}

void ornament()
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    const float y = p.y + 4.0f;
    const float mid = p.x + w * 0.5f;
    const ImU32 clear = ImGui::GetColorU32(ImVec4(kBrass.x, kBrass.y, kBrass.z, 0.0f));
    const ImU32 brass = ImGui::GetColorU32(kBrass);
    const float d = 4.0f;   // the diamond's half-size
    dl->AddRectFilledMultiColor(ImVec2(p.x, y - 0.5f), ImVec2(mid - d - 3.0f, y + 0.5f), clear, brass, brass, clear);
    dl->AddRectFilledMultiColor(ImVec2(mid + d + 3.0f, y - 0.5f), ImVec2(p.x + w, y + 0.5f), brass, clear, clear, brass);
    dl->AddQuadFilled(ImVec2(mid, y - d), ImVec2(mid + d, y), ImVec2(mid, y + d), ImVec2(mid - d, y), brass);
    ImGui::Dummy(ImVec2(w, 9.0f));
}

void controller_hint()
{
    ImGui::PushStyleColor(ImGuiCol_Text, kBone);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted("L3 + R3: CLICK for F10 menu  |  HOLD for VD overlay");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
}

void title(const char* text)
{
    if (g_headBold) ImGui::PushFont(g_headBold, ImGui::GetStyle().FontSizeBase * 2.48f);
    ImGui::PushStyleColor(ImGuiCol_Text, kBone);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    if (g_headBold) ImGui::PopFont();
    // A short bone rule under the title, leaving the skyline unobstructed.
    const float width=ImGui::GetContentRegionAvail().x;
    ImGui::PushItemWidth(width*.47f);
    const ImVec2 p=ImGui::GetCursorScreenPos();
    auto* dl=ImGui::GetWindowDrawList();
    const float mid=p.x+width*.235f, y=p.y+2;
    const ImU32 col=ImGui::GetColorU32(ImVec4(.77f,.72f,.61f,.8f));
    dl->AddLine(p,ImVec2(mid-12,p.y),col);
    dl->AddLine(ImVec2(mid+12,p.y),ImVec2(p.x+width*.47f,p.y),col);
    dl->AddQuad(ImVec2(mid,y-5),ImVec2(mid+3,y),ImVec2(mid,y+5),ImVec2(mid-3,y),col,1.5f);
    ImGui::Dummy(ImVec2(width,6));
    ImGui::PopItemWidth();
}

bool pill(const char* label, bool selected)
{
    Skin skin;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1,1,1,0.12f));
    ImGui::PushStyleColor(ImGuiCol_Text, selected ? kInkText : kBone);
    const bool hit = ImGui::Button(label, ImVec2(ImGui::GetFontSize()*5, 0));
    skin.finish(selected ? 1 : 2, ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    ImGui::PopStyleColor(3);
    return hit;
}

void push_primary()
{
    g_primary = true;
    ImGui::PushStyleColor(ImGuiCol_Button, kOxblood);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kOxbloodHi);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kBrassDim);
}
void pop_primary() { g_primary = false; ImGui::PopStyleColor(3); }

void release_art() { for (auto& texture : g_art) texture.Reset(); g_artDevice.Reset(); }
void load_art(ID3D11Device* device)
{
    if (!device || g_artDevice.Get() == device) return;
    release_art(); g_artDevice = device;
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)&load_art, &module);
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    for (int i=0; i<5; ++i)
        if (!decode_art(device, module, 201+i, g_art[i].GetAddressOf()))
            DVR_WARN("overlay/art: embedded resource %d unavailable; using flat theme fallback", 201+i);
    if (SUCCEEDED(com)) CoUninitialize();
    DVR_INFO("overlay/art: backdrop=%d parchment=%d metal=%d header=%d note=%d",
        !!g_art[0], !!g_art[1], !!g_art[2], !!g_art[3], !!g_art[4]);
}
float body_footer_height()
{
    // Two footer rows plus the bottom air of the reference; scale with text.
    return ImGui::GetFrameHeightWithSpacing()*4.0f;
}
void begin_body(const char* id)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(14,4));
    ImGui::BeginChild(id,ImVec2(0,-body_footer_height()),ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PopStyleVar();
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x*.65f);
}
void end_body()
{
    ImGui::PopItemWidth();
    ImGui::EndChild();
}
void backdrop()
{
    const ImVec2 p=ImGui::GetWindowPos(), z=ImGui::GetWindowSize();
    material(0,p,ImVec2(p.x+z.x,p.y+z.y));
}
bool button(const char* label, const ImVec2& size)
{
    Skin skin;
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
    const bool changed=ImGui::Button(label,size);
    skin.finish(2,ImGui::GetItemRectMin(),ImGui::GetItemRectMax(),
        g_primary ? IM_COL32(255,105,85,255) : IM_COL32_WHITE);
    ImGui::PopStyleColor();
    return changed;
}
struct FrameSkin {
    Skin skin;
    ImVec2 start=ImGui::GetCursorScreenPos();
    float width=ImGui::CalcItemWidth(), height=ImGui::GetFrameHeight();
    FrameSkin() { ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0,0,0,0)); }
    void finish(bool compact=false) {
        skin.finish(2,start,ImVec2(start.x+(compact?height:width),start.y+height));
        ImGui::PopStyleColor();
    }
};
bool checkbox(const char* label, bool* value)
{
    const ImVec2 p=ImGui::GetCursorScreenPos(); const float h=ImGui::GetFrameHeight();
    ImGui::PushStyleColor(ImGuiCol_FrameBg,ImVec4(0,0,0,0));
    const bool changed=ImGui::Checkbox(label,value);
    ImGui::PopStyleColor();
    if (*value) ImGui::GetWindowDrawList()->AddRect(p,ImVec2(p.x+h,p.y+h),ImGui::GetColorU32(kBrass),1,0,1.4f);
    return changed;
}
bool radio_button(const char* label, bool selected)
{
    const ImVec2 p=ImGui::GetCursorScreenPos(); const float h=ImGui::GetFrameHeight();
    ImGui::PushStyleColor(ImGuiCol_CheckMark,ImVec4(0,0,0,0));
    const bool changed=ImGui::RadioButton(label,selected);
    ImGui::PopStyleColor();
    if (selected) {
        const ImVec2 center(p.x+h*.5f,p.y+h*.5f); auto* dl=ImGui::GetWindowDrawList();
        dl->AddCircle(center,h*.45f,ImGui::GetColorU32(kBrassHi),0,1.4f);
        dl->AddCircleFilled(center,h*.32f,ImGui::GetColorU32(kBrass));
        dl->AddCircle(center,h*.32f,ImGui::GetColorU32(kBrassHi));
    }
    return changed;
}
bool radio_button(const char* label, int* value, int choice)
{
    const bool changed=radio_button(label,*value==choice);
    if (changed) *value=choice;
    return changed;
}
bool slider_float(const char* label,float* value,float min,float max,const char* format,ImGuiSliderFlags flags)
{
    FrameSkin skin; const bool changed=ImGui::SliderFloat(label,value,min,max,format,flags); skin.finish(); return changed;
}
bool slider_int(const char* label,int* value,int min,int max,const char* format,ImGuiSliderFlags flags)
{
    FrameSkin skin; const bool changed=ImGui::SliderInt(label,value,min,max,format,flags); skin.finish(); return changed;
}
} // namespace dvr::ovl
