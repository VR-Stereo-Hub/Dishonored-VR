// core/ui/ovl_ui.cpp - see ovl_ui.h (VR-196).
#include "core/ui/ovl_ui.h"
#include <atomic>
#include <string.h>
#include "imgui.h"

namespace dvr::ovl {
namespace { std::atomic<int> g_level{Basic}; }

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

void tip(const char* text)
{
    if (!text || !*text || !ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) return;
    ImGui::BeginTooltip();
    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 32.0f);
    ImGui::TextUnformatted(text);
    ImGui::PopTextWrapPos();
    ImGui::EndTooltip();
}

bool section(const char* name, int tier, const char* tipText, bool defaultOpen)
{
    if (!show(tier)) return false;
    const bool open = ImGui::CollapsingHeader(name, defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    tip(tipText);
    return open;
}
} // namespace dvr::ovl
