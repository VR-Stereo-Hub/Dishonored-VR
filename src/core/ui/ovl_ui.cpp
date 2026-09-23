// core/ui/ovl_ui.cpp - see ovl_ui.h (VR-196 tiers, VR-197 theme).
#include "core/ui/ovl_ui.h"
#include <atomic>
#include <stdio.h>
#include <string.h>
#include <windows.h>
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
const ImVec4 kRule      = rgb(0x3A, 0x32, 0x27);   // brass gone dull
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
    static const char* const kBody[] = { "segoeui.ttf", "tahoma.ttf", "arial.ttf" };
    static const char* const kHead[] = { "constan.ttf", "georgia.ttf", "pala.ttf", "times.ttf" };
    static const char* const kHeadBold[] = { "constanb.ttf", "georgiab.ttf", "palab.ttf", "timesbd.ttf" };
    g_body = try_font(kBody, 3, 15.0f, "body");
    if (!g_body) ImGui::GetIO().Fonts->AddFontDefault();
    g_head = try_font(kHead, 4, 16.0f, "heading");
    g_headBold = try_font(kHeadBold, 4, 22.0f, "title");
    if (!g_headBold) g_headBold = g_head;
}

ImFont* heading_font() { return g_head; }

void apply_theme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    ImGui::StyleColorsDark(&s);   // a complete base; everything that matters is set below
    s.FontSizeBase = 15.0f;
    s.WindowPadding = ImVec2(14, 12);
    s.FramePadding = ImVec2(8, 4);
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
    s.FrameBorderSize = 0;
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
    c[ImGuiCol_CheckboxSelectedBg]    = kSlateHi;
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

void tip(const char* text)
{
    if (!text || !*text || !ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) return;
    // Parchment and ink, like the notes you find in Dunwall.
    ImGui::PushStyleColor(ImGuiCol_PopupBg, kParchment);
    ImGui::PushStyleColor(ImGuiCol_Text, kInkText);
    ImGui::PushStyleColor(ImGuiCol_Border, kBrassDim);
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 26.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
    ImGui::PopStyleColor(3);
}

bool section(const char* name, int tier, const char* tipText, bool defaultOpen)
{
    if (!show(tier)) return false;
    if (g_head) ImGui::PushFont(g_head, 0.0f);
    const bool open = ImGui::CollapsingHeader(name, defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    if (g_head) ImGui::PopFont();
    tip(tipText);
    return open;
}

bool tab(const char* label)
{
    // The selected tab's label in brass, the rest in faded ink. The selection is known only
    // after BeginTabItem draws the label, so it is last frame's: one frame late on a click.
    static ImGuiID s_selected = 0;
    const ImGuiID id = ImGui::GetID(label);
    if (g_head) ImGui::PushFont(g_head, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, id == s_selected ? kBrassHi : kFaded);
    const bool open = ImGui::BeginTabItem(label);
    ImGui::PopStyleColor();
    if (g_head) ImGui::PopFont();
    if (open) s_selected = id;
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

void title(const char* text)
{
    {   // lamplight: a faint warm glow behind the title, fading down into the ink
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        const float gh = ImGui::GetFontSize() * 5.0f;
        const ImU32 glow = ImGui::GetColorU32(ImVec4(kBrass.x, kBrass.y, kBrass.z, 0.10f));
        const ImU32 none = ImGui::GetColorU32(ImVec4(kBrass.x, kBrass.y, kBrass.z, 0.0f));
        dl->AddRectFilledMultiColor(ImVec2(wp.x + 1, wp.y + 1), ImVec2(wp.x + ws.x - 1, wp.y + gh), glow, glow, none, none);
    }
    if (g_headBold) ImGui::PushFont(g_headBold, ImGui::GetStyle().FontSizeBase * 1.45f);
    const float tw = ImGui::CalcTextSize(text).x;
    const float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail > tw ? (avail - tw) * 0.5f : 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, kBrassHi);
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    if (g_headBold) ImGui::PopFont();
    ornament();
}

bool pill(const char* label, bool selected)
{
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, kBrassDim);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kBrassDim);
        ImGui::PushStyleColor(ImGuiCol_Text, kBone);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, kSlate);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kUmberAct);
        ImGui::PushStyleColor(ImGuiCol_Text, kFaded);
    }
    const bool hit = ImGui::Button(label);
    ImGui::PopStyleColor(3);
    return hit;
}

void push_primary()
{
    ImGui::PushStyleColor(ImGuiCol_Button, kOxblood);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kOxbloodHi);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, kBrassDim);
}
void pop_primary() { ImGui::PopStyleColor(3); }
} // namespace dvr::ovl
