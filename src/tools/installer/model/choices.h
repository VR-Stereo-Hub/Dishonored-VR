// tools/installer/model/choices.h - the choices a player picks, and what
// each one means in dishonored_vr.ini. Everything else is the F10 panel's.
#pragma once
#include <stdint.h>
#include <string>

namespace dvr::setup {

// [VR] Runtime / XrRuntimeJson, the exact mapping of tools/vr-runtime.ps1:
//   Vdxr    Runtime=native  XrRuntimeJson=<Virtual Desktop's 32-bit manifest>
//   SteamVr Runtime=steamvr XrRuntimeJson=          (the bundled shim; start SteamVR first)
//   Auto    Runtime=auto    XrRuntimeJson=          (the system's 32-bit runtime, shim as fallback)
enum class Runtime { Vdxr = 0, SteamVr = 1, Auto = 2 };

// [Screen] RenderWidth/RenderHeight as a share of the tested 2750x2850 pixel
// count, both axes scaled together, the F10 Display picker's own arithmetic.
enum class Quality { Performance = 0, Balanced = 1, Quality = 2, Custom = 3 };

struct Size { uint32_t w = 0, h = 0; bool operator==(const Size& o) const { return w == o.w && h == o.h; } };

constexpr uint32_t kBaseWidth = 2750, kBaseHeight = 2850;   // 100 %, Balanced per-eye resolution
constexpr float kPerformancePercent = 75.0f, kBalancedPercent = 100.0f, kQualityPercent = 120.0f;

// Optional front-page preferences. -1 preserves the existing/shipped key.
// Values use INI semantics, including DesktopMirrorOff's inverted meaning.
struct Preference {
    const wchar_t* section;
    const wchar_t* key;
    const wchar_t* flag;
    const char* label;
    int fallback;
    bool inverted;
};
enum PreferenceId { Mirror, Crouch, Rain, Modifier, DpadFlip, PauseChord, PreferenceCount };
inline constexpr Preference kPreferences[] = {
    { L"VR", L"DesktopMirrorOff", L"--mirror", "Desktop mirror", 1, true },
    { L"Tracking", L"PhysicalCrouch", L"--physical-crouch", "Physical crouching", 1, false },
    { L"Rain", L"Hide", L"--hide-rain-overlay", "Hide close rain overlay", 0, false },
    { L"Controllers", L"DpadModifier", L"--dpad-modifier", "D-pad modifier", 1, false },
    { L"Controllers", L"DpadFlip", L"--dpad-flip", "Use right stick for D-pad", 0, false },
    { L"Controllers", L"PauseChord", L"--pause-chord", "X + Y pause shortcut", 1, false },
};

struct Choices {
    int preferences[PreferenceCount] = { -1, -1, -1, -1, -1, -1 };
    bool overwriteSettings = true; // recommended defaults, backed up before replacement
    Runtime runtime = Runtime::Auto;
    Quality quality = Quality::Balanced;
    float pixelPercent = kBalancedPercent;   // the Advanced slider; authoritative when quality == Custom
    // An exact size (the ini's current one, or --size): kept as it is until a pill or
    // the slider is touched, because a size off the picker's curve (a headset's own
    // 2064x2208, say) must not be re-rounded onto it behind the player's back.
    Size exact;
    std::wstring vdxrJson;                   // the manifest Vdxr pins; empty = the default location
    Size size() const;
    void choose(Quality q);                  // a pill: clears the exact size
    void choose_percent(float percent);      // the slider: clears the exact size
    void keep(Size s);                       // an exact size, shown as Custom
};

Size  size_for_percent(float percent);       // (uint32_t)(axis * sqrt(p/100) + 0.5f), like the picker
float percent_for_size(Size s);
float percent_for_quality(Quality q, float customPercent);
Quality quality_for_percent(float percent);  // Custom unless it is one of the three presets

const char* runtime_token(Runtime r);        // "vdxr" | "steamvr" | "auto"
bool parse_runtime(const std::wstring& token, Runtime* out);
const char* quality_token(Quality q);
bool parse_quality(const std::wstring& token, Quality* out);
const char* runtime_label(Runtime r);        // for the screens
const char* quality_label(Quality q);
// The ini's own words for a runtime choice.
const wchar_t* runtime_ini_value(Runtime r); // "native" | "steamvr" | "auto"
bool runtime_from_ini(const std::wstring& runtimeValue, const std::wstring& json, Runtime* out);

// VR-223: the headset the player says they have. Recorded for diagnostics only
// (the launcher log, the mod's startup log, the support bundle); it changes no
// setting. The list is the BioShock Remastered VR mod's Setup.bat question, in
// its order, so reports from both mods group the same way. "Something else"
// takes a typed name. Persisted in launcher.ini [Headset] Model as the label
// itself, so a name that is not in the list reads back as "Something else".
inline constexpr const char* kHeadsets[] = {
    "Meta Quest 3 / 3S", "Meta Quest Pro", "Meta Quest 2", "Meta Quest 1",
    "Meta Rift S / Rift CV1", "Valve Index", "HTC Vive / Vive Pro", "Vive Pro 2 / XR Elite",
    "Bigscreen Beyond 1 / 2", "Pimax Crystal / Light", "Pimax 5K / 8K", "Reverb G2 / other WMR",
    "Varjo Aero / XR-3", "Pico 4 / 4 Ultra", "Somnium VR1", "PSVR2",
};
constexpr int kHeadsetCount = (int)(sizeof(kHeadsets) / sizeof(kHeadsets[0]));
constexpr int kHeadsetOther = kHeadsetCount;         // the "Something else" row
constexpr size_t kHeadsetNameMax = 48;
// The list index of a stored name: 0..kHeadsetCount-1, kHeadsetOther for a typed
// name, -1 for none recorded.
int headset_index(const std::string& name);
// A typed name made safe for an ini value and a log line: printable ASCII,
// spaces collapsed, trimmed, capped at kHeadsetNameMax. Empty when nothing is left.
std::string clean_headset_name(const std::string& typed);

std::wstring default_vdxr_json();            // %ProgramW6432%\Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr-32.json

} // namespace dvr::setup
