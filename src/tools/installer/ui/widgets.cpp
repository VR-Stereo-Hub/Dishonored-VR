// tools/installer/ui/widgets.cpp - see widgets.h.
#include "ui/widgets.h"
#include "core/ui/ovl_ui.h"
#include <math.h>
#include <string.h>
#include <string>

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

void page_header(const char* subtitle)
{
    dvr::ovl::title("Dishonored VR");
    if (subtitle && *subtitle) {
        const float tw = ImGui::CalcTextSize(subtitle).x;
        const float avail = ImGui::GetContentRegionAvail().x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail > tw ? (avail - tw) * 0.5f : 0.0f));
        wrapped_faded(subtitle);
    }
    ImGui::Spacing();
}

bool heading(const char* name, const char* tip, bool defaultOpen)
{
    return dvr::ovl::section(name, dvr::ovl::Basic, tip, defaultOpen);
}

void wrapped(const char* text)
{
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
}
void wrapped_faded(const char* text)
{
    ImGui::PushStyleColor(ImGuiCol_Text, col_faded());
    wrapped(text);
    ImGui::PopStyleColor();
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
    if (primary) dvr::ovl::push_primary();
    ImGui::BeginDisabled(!enabled);
    const bool hit = dvr::ovl::button(label, ImVec2(width, 0));
    ImGui::EndDisabled();
    if (primary) dvr::ovl::pop_primary();
    return hit;
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
