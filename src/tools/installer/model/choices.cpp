// tools/installer/model/choices.cpp - see choices.h.
#include "model/choices.h"
#include "sys/fs.h"
#include <math.h>

namespace dvr::setup {

Size size_for_percent(float percent)
{
    // src/core/ui/overlay_tabs.inc, OvlTabDisplay: both axes by sqrt, rounded
    // half up. 110 % -> 2884x2989 and 120 % -> 3012x3122, the numbers
    // PERFORMANCE.md records, fall out of exactly this.
    const float axis = sqrtf(percent * 0.01f);
    Size s;
    s.w = (uint32_t)(kBaseWidth * axis + 0.5f);
    s.h = (uint32_t)(kBaseHeight * axis + 0.5f);
    return s;
}
float percent_for_size(Size s)
{
    if (!s.w || !s.h) return kBalancedPercent;
    return 100.0f * ((float)s.w / kBaseWidth) * ((float)s.h / kBaseHeight);
}
float percent_for_quality(Quality q, float customPercent)
{
    switch (q) {
    case Quality::Performance: return kPerformancePercent;
    case Quality::Balanced: return kBalancedPercent;
    case Quality::Quality: return kQualityPercent;
    default: return customPercent;
    }
}
Quality quality_for_percent(float p)
{
    if (fabsf(p - kPerformancePercent) < 0.5f) return Quality::Performance;
    if (fabsf(p - kBalancedPercent) < 0.5f) return Quality::Balanced;
    if (fabsf(p - kQualityPercent) < 0.5f) return Quality::Quality;
    return Quality::Custom;
}
Size Choices::size() const
{
    if (exact.w && exact.h) return exact;
    return size_for_percent(percent_for_quality(quality, pixelPercent));
}
void Choices::choose(Quality q) { quality = q; pixelPercent = percent_for_quality(q, pixelPercent); exact = Size(); }
void Choices::choose_percent(float percent) { pixelPercent = percent; quality = quality_for_percent(percent); exact = Size(); }
void Choices::keep(Size s)
{
    exact = s;
    pixelPercent = percent_for_size(s);
    quality = quality_for_percent(pixelPercent);
    if (!(size_for_percent(pixelPercent) == s)) quality = Quality::Custom;
}

const char* runtime_token(Runtime r) { return r == Runtime::Vdxr ? "vdxr" : r == Runtime::SteamVr ? "steamvr" : "auto"; }
bool parse_runtime(const std::wstring& t, Runtime* out)
{
    if (fs::iequals(t, L"vdxr")) { *out = Runtime::Vdxr; return true; }
    if (fs::iequals(t, L"steamvr") || fs::iequals(t, L"shim")) { *out = Runtime::SteamVr; return true; }
    if (fs::iequals(t, L"auto")) { *out = Runtime::Auto; return true; }
    return false;
}
const char* quality_token(Quality q)
{
    switch (q) {
    case Quality::Performance: return "performance";
    case Quality::Balanced: return "balanced";
    case Quality::Quality: return "quality";
    default: return "custom";
    }
}
bool parse_quality(const std::wstring& t, Quality* out)
{
    if (fs::iequals(t, L"performance")) { *out = Quality::Performance; return true; }
    if (fs::iequals(t, L"balanced")) { *out = Quality::Balanced; return true; }
    if (fs::iequals(t, L"quality")) { *out = Quality::Quality; return true; }
    if (fs::iequals(t, L"custom")) { *out = Quality::Custom; return true; }
    return false;
}
const char* runtime_label(Runtime r)
{
    return r == Runtime::Vdxr ? "Quest via Virtual Desktop" : r == Runtime::SteamVr ? "SteamVR headset" : "Let the mod choose";
}
const char* quality_label(Quality q)
{
    switch (q) {
    case Quality::Performance: return "Performance";
    case Quality::Balanced: return "Balanced (tested)";
    case Quality::Quality: return "Quality";
    default: return "Custom";
    }
}
const wchar_t* runtime_ini_value(Runtime r) { return r == Runtime::Vdxr ? L"native" : r == Runtime::SteamVr ? L"steamvr" : L"auto"; }
bool runtime_from_ini(const std::wstring& v, const std::wstring& json, Runtime* out)
{
    if (fs::iequals(v, L"steamvr")) { *out = Runtime::SteamVr; return true; }
    if (fs::iequals(v, L"native")) { *out = json.empty() ? Runtime::Auto : Runtime::Vdxr; return true; }
    if (fs::iequals(v, L"auto")) { *out = Runtime::Auto; return true; }
    return false;
}

std::wstring default_vdxr_json()
{
    // %ProgramFiles% from a 32-bit process is Program Files (x86); the streamer
    // is a 64-bit install under the real Program Files.
    std::wstring pf = fs::env(L"ProgramW6432");
    if (pf.empty()) pf = L"C:\\Program Files";
    return fs::join(pf, L"Virtual Desktop Streamer\\OpenXR\\virtualdesktop-openxr-32.json");
}

} // namespace dvr::setup
