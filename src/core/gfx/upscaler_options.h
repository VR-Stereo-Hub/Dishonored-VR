// Shared persisted upscaler choices for F10 and the launcher.
#pragma once
namespace dvr::dlss {
struct ModelChoice { const char* name; int model; int preset; const char* tip; };
inline constexpr int kPresetPerMode = 16;
inline constexpr ModelChoice kModelChoices[] = {
    {"Transformer K (default)", 0, 0,
     "NVIDIA's best all-round model: sharp, stable, little ghosting. About 2 ms per eye at 2750x2850, "
     "the same in every mode."},
    {"Transformer J", 0, 10,
     "A sibling of K: slightly less ghosting behind moving things, slightly more flicker on fine "
     "detail. Same cost as K. NVIDIA recommends K over J."},
    {"Transformer 2 M", 0, 13,
     "The newer (DLSS 4.5) model NVIDIA uses for Performance: the least ghosting and cleaner fine "
     "patterns, best when the render is small. Heavy at high resolution: about 3 ms per eye in "
     "Performance, 4 in Quality, 8 in DLAA."},
    {"Transformer 2 L", 0, 12,
     "The newer model NVIDIA uses for Ultra Performance: rebuilds the most from a very small render. "
     "The heaviest: about 3 ms per eye in Performance, 5 in Quality, 10 in DLAA."},
    {"NVIDIA recommended per mode", 0, kPresetPerMode,
     "What NVIDIA picks for each mode: K for DLAA, Ultra Quality, Quality and Balanced; M for "
     "Performance; L for Ultra Performance."},
    {"Fast (older CNN)", 1, 0,
     "The older, lighter models (E for the smaller modes, F for DLAA): under 1 ms per eye, a little "
     "softer, more ghosting and shimmer. Pick it when frame rate matters more than detail."},
};
inline constexpr int kModelChoiceCount = (int)(sizeof(kModelChoices) / sizeof(kModelChoices[0]));

}
