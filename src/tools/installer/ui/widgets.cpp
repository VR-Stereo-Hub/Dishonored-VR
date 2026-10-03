// tools/installer/ui/widgets.cpp - see widgets.h.
#include "ui/widgets.h"
#include "core/ui/ovl_ui.h"
#include <math.h>
#include <string.h>
#include <string>
#include <d3d11.h>
#include "sys/resources.h"
#include "sys/fs.h"

namespace dvr::setup::ui {

ImVec4 col_bone()     { return ImGui::GetStyle().Colors[ImGuiCol_Text]; }
ImVec4 col_faded()    { return ImGui::GetStyle().Colors[ImGuiCol_TextDisabled]; }
ImVec4 col_brass()    { return ImGui::GetStyle().Colors[ImGuiCol_SliderGrab]; }
ImVec4 col_brass_hi() { return ImGui::GetStyle().Colors[ImGuiCol_CheckMark]; }
ImVec4 col_oxblood()
{
    dvr::ovl::push_primary();
    const ImVec4 c = ImGui::GetStyle().Colors[ImGuiCol_Button];
    dvr::ovl::pop_primary();
    return c;
}

namespace {
ID3D11ShaderResourceView* headerArt = nullptr;
ImFont* brandFont = nullptr;
float scale() { return ImGui::GetFontSize() / 16.0f; }
void header_image(ImVec2 a, ImVec2 b) {
    auto* dl = ImGui::GetWindowDrawList();
    if (headerArt) dl->AddImage((ImTextureID)(uintptr_t)headerArt, a, b, ImVec2(0,.29f), ImVec2(1,.71f));
    else dl->AddRectFilled(a, b, IM_COL32(216,210,190,255));
}
}
void load_widget_art(ID3D11Device* device) {
    release_widget_art();
    unsigned w=0,h=0; headerArt=resources::image(device,204,&w,&h);
    wchar_t windows[MAX_PATH]{}; GetWindowsDirectoryW(windows,MAX_PATH);
    const auto font = fs::narrow(std::wstring(windows) + L"\\Fonts\\PERTILI.TTF");
    if (GetFileAttributesA(font.c_str()) != INVALID_FILE_ATTRIBUTES)
        brandFont=ImGui::GetIO().Fonts->AddFontFromFileTTF(font.c_str(),36.0f);
}
void release_widget_art() { if(headerArt)headerArt->Release();headerArt=nullptr;brandFont=nullptr; }
void apply_launcher_style() {
    auto& st=ImGui::GetStyle();
    st.WindowPadding=ImVec2(24,20); st.FramePadding=ImVec2(13,7); st.ItemSpacing=ImVec2(8,6);
    st.Colors[ImGuiCol_Text]=ImVec4(216/255.f,210/255.f,190/255.f,1);
    st.Colors[ImGuiCol_TextDisabled]=ImVec4(178/255.f,173/255.f,156/255.f,1);
    st.Colors[ImGuiCol_FrameBg]=ImVec4(17/255.f,26/255.f,30/255.f,1);
    st.Colors[ImGuiCol_CheckMark]=ImVec4(177/255.f,152/255.f,83/255.f,1);
    st.Colors[ImGuiCol_Border]=ImVec4(87/255.f,96/255.f,94/255.f,1);
}
void brand_header(const char* version) {
    const float factor=scale(); const auto start=ImGui::GetCursorPos();
    ImGui::PushFont(brandFont ? brandFont : ImGui::GetFont(),36.0f);
    ImGui::TextUnformatted("DISHONORED VR"); ImGui::PopFont();
    wrapped_faded("A motion-controlled adventure in Dunwall");
    const float end=ImGui::GetCursorPosY();
    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth()-ImGui::GetStyle().WindowPadding.x-150*factor,start.y+5*factor));
    ImGui::PushFont(ImGui::GetFont(),13.0f);
    ImGui::TextDisabled("Dishonored VR %s",version);
    ImGui::PopFont();
    ImGui::SetCursorPos(ImVec2(start.x,end+19*factor));
}
bool sidebar_button(const char* label,bool selected) {
    const float factor=scale(); auto at=ImGui::GetCursorScreenPos();
    const ImVec2 size(ImGui::GetContentRegionAvail().x,43*factor);
    if(selected)header_image(at,ImVec2(at.x+size.x,at.y+size.y));
    ImGui::PushFont(ImGui::GetFont(),17.0f);
    ImGui::PushStyleColor(ImGuiCol_Text,selected?ImVec4(.145f,.145f,.137f,1):col_bone());
    ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,ImVec4(.65f,.6f,.4f,.16f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,ImVec4(.65f,.6f,.4f,.24f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize,0);
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign,ImVec2(0,.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(12*factor,7*factor));
    const bool clicked=ImGui::Button(label,size);
    ImGui::PopStyleVar(3);ImGui::PopStyleColor(4);ImGui::PopFont();
    return clicked;
}
void page_header(const char* title,const char* subtitle) {
    const float factor=scale();
    ImGui::PushFont(ImGui::GetFont(),26.0f);ImGui::TextUnformatted(title);ImGui::PopFont();
    if(subtitle) wrapped_faded(subtitle);
    ImGui::Dummy(ImVec2(0,7*factor));
}
bool heading(const char* name,const char* tip,bool defaultOpen) {
    if(!defaultOpen)return dvr::ovl::section(name,dvr::ovl::Basic,tip,false);
    const float factor=scale();ImGui::Dummy(ImVec2(0,4*factor));
    auto at=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x;
    header_image(at,ImVec2(at.x+width,at.y+26*factor));
    ImGui::GetWindowDrawList()->AddText(ImVec2(at.x+30*factor,at.y+5*factor),IM_COL32(41,42,39,255),name);
    ImGui::Dummy(ImVec2(width,26*factor));dvr::ovl::tip(tip);return true;
}

void wrapped(const char* text)
{
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
}
void wrapped_faded(const char* text)
{
    const float factor=scale();
    ImGui::PushFont(ImGui::GetFont(),14.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, col_faded());
    wrapped(text);
    ImGui::PopStyleColor(); ImGui::PopFont();
}

void status_slot(const char* id, int lines, const char* text, const ImVec4* colour)
{
    const float textH = ImGui::CalcTextSize(text ? text : "", nullptr, false, ImGui::GetContentRegionAvail().x).y;
    const float h = (textH > ImGui::GetTextLineHeightWithSpacing() * lines ? textH : ImGui::GetTextLineHeightWithSpacing() * lines)
                    + ImGui::GetStyle().FramePadding.y;
    ImGui::BeginChild(id, ImVec2(0, h), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    if (colour) ImGui::PushStyleColor(ImGuiCol_Text, *colour);
    wrapped(text ? text : "");
    if (colour) ImGui::PopStyleColor();
    ImGui::EndChild();
}

void mark(StepStatus status)
{
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const float fs = ImGui::GetFontSize();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float d = fs * 0.28f;
    const ImVec2 c(p.x + d + 2.0f, p.y + fs * 0.52f);
    const ImVec2 a(c.x, c.y - d), b(c.x + d, c.y), e(c.x, c.y + d), f(c.x - d, c.y);
    switch (status) {
    case StepStatus::Ok:      dl->AddQuadFilled(a, b, e, f, ImGui::GetColorU32(col_brass_hi())); break;
    case StepStatus::Warn:    dl->AddQuadFilled(a, b, e, f, ImGui::GetColorU32(col_brass())); break;
    case StepStatus::Failed:  dl->AddQuadFilled(a, b, e, f, ImGui::GetColorU32(col_oxblood()));
                              dl->AddQuad(a, b, e, f, ImGui::GetColorU32(col_brass_hi()), 1.0f); break;
    case StepStatus::Skipped: dl->AddQuad(a, b, e, f, ImGui::GetColorU32(col_faded()), 1.0f); break;
    }
    ImGui::Dummy(ImVec2(d * 2.0f + 6.0f, fs));
    ImGui::SameLine(0.0f, 4.0f);
}

void step_row(const StepResult& step)
{
    mark(step.status);
    ImGui::BeginGroup();
    if (step.status == StepStatus::Failed) ImGui::PushStyleColor(ImGuiCol_Text, col_brass_hi());
    wrapped(step.title.c_str());
    if (step.status == StepStatus::Failed) ImGui::PopStyleColor();
    if (!step.detail.empty()) wrapped_faded(step.detail.c_str());
    ImGui::EndGroup();
    ImGui::Spacing();
}

int pill_row(const char* const* labels, int count, int selected, const char* const* tips)
{
    int hit = -1;
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    const float width = (ImGui::GetContentRegionAvail().x - gap * (count - 1)) / count;
    for (int i = 0; i < count; ++i) {
        if (i) ImGui::SameLine(0.0f, gap);
        if (dvr::ovl::pill(labels[i], i == selected, width)) hit = i;
        if (tips && tips[i]) dvr::ovl::tip(tips[i]);
    }
    return hit;
}

void banner(const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyle().Colors[ImGuiCol_Header]);
    ImGui::PushStyleColor(ImGuiCol_Text, col_brass_hi());
    const float h = ImGui::GetTextLineHeightWithSpacing() * 2.0f + ImGui::GetStyle().FramePadding.y * 2.0f;
    ImGui::BeginChild("##banner", ImVec2(0, h), ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
    wrapped(text);
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::Spacing();
}

void spinner(const char* text)
{
    const float fs = ImGui::GetFontSize();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float r = fs * 0.9f;
    const ImVec2 c(p.x + avail.x * 0.5f, p.y + avail.y * 0.5f - fs);
    const float t = (float)ImGui::GetTime();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PathClear();
    dl->PathArcTo(c, r, t * 4.0f, t * 4.0f + 4.2f, 24);
    dl->PathStroke(ImGui::GetColorU32(col_brass()), 0, 2.0f);
    const float tw = ImGui::CalcTextSize(text).x;
    dl->AddText(ImVec2(c.x - tw * 0.5f, c.y + r + fs * 0.6f), ImGui::GetColorU32(col_faded()), text);
    ImGui::Dummy(avail);
}

void footer_begin(float height)
{
    const float y = ImGui::GetWindowHeight() - height - ImGui::GetStyle().WindowPadding.y;
    if (ImGui::GetCursorPosY() < y) ImGui::SetCursorPosY(y);
    dvr::ovl::ornament();
}

bool button(const char* label, bool primary, bool enabled, float width)
{
    ImGui::BeginDisabled(!enabled);
    ImGui::PushStyleColor(ImGuiCol_Button,primary?ImVec4(.23f,.105f,.09f,1):ImVec4(.067f,.102f,.118f,1));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,primary?ImVec4(.33f,.15f,.125f,1):ImVec4(.13f,.18f,.20f,1));
    const bool hit=ImGui::Button(label,ImVec2(width,34*scale()));
    ImGui::PopStyleColor(2);ImGui::EndDisabled();return hit;
}

// Right-to-left placement on one row: the first call of a frame starts a new
// line at the right edge, each next call sits to the left of the previous one.
static float s_footerRight = 0.0f;
static int s_footerFrame = -1;
bool footer_button(const char* label, bool primary, bool enabled)
{
    const ImGuiStyle& st = ImGui::GetStyle();
    const float w = ImGui::CalcTextSize(label, nullptr, true).x + st.FramePadding.x * 2.0f;
    const int frame = ImGui::GetFrameCount();
    if (s_footerFrame != frame) {
        s_footerFrame = frame;
        s_footerRight = ImGui::GetWindowSize().x - st.WindowPadding.x;
    } else {
        ImGui::SameLine();
    }
    const float x = s_footerRight - w;
    ImGui::SetCursorPosX(x);
    const bool hit = button(label, primary, enabled, w);
    s_footerRight = x - st.ItemSpacing.x;
    return hit;
}

// A path that is wider than the room is shown from its tail, which is the part
// that tells the folders apart; the full text is the tooltip.
void path_text(const char* text)
{
    const float avail = ImGui::GetContentRegionAvail().x;
    std::string shown = text;
    if (ImGui::CalcTextSize(shown.c_str()).x > avail) {
        size_t cut = 0;
        while (cut < shown.size() && ImGui::CalcTextSize(("..." + shown.substr(cut)).c_str()).x > avail) ++cut;
        shown = "..." + shown.substr(cut);
    }
    ImGui::TextUnformatted(shown.c_str());
    if (shown.size() != strlen(text) && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", text);
}

} // namespace dvr::setup::ui
