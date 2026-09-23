# Expandable F10 art theme (VR-206)

The generated background and materials follow the supplied reference while preserving
native ImGui interaction. The default window is square, centered, and 46% of the shorter
eye-texture dimension (1265 pixels on a 2750 x 2850 eye texture). The panel remains
resizable. Each tab has a separate scrolling child; the top controls and footer stay
outside it. Tabs overflow by scrolling. No game or installed ini has been changed.

## Adding controls

Use dvr::ovl::section, button, checkbox, slider_float and slider_int from ovl_ui.h.
These call the corresponding native ImGui controls and decorate their backgrounds.
ImGui IDs, focus/navigation, item-activity queries and the original setters/save callbacks
remain intact. Keep OvlTip and the existing save-on-change helpers. Radio buttons, combo
boxes and input fields use the shared native palette/font. No new asset is needed for a
new toggle. Add new tabs through OvlTabBody with a unique child ID for independent scrolling.

Basic/Advanced/Debug tier logic remains unchanged. The Hands size and Sleeve sections open
initially so the default view resembles the reference; other sections remain collapsible.
Reset, save defaults, and the visible L3+R3 click/hold hint remain available.

## Artwork and lifetime

Three masters in assets/ui/f10 replace per-widget image inventories; README.md records
the prompt set. They are embedded in the DLL as resources. WIC decodes once per D3D11
device; COM initialization is balanced, failed assets fall back to flat materials, and
old textures release when the device changes. This creates no D3D9 default-pool resources.
The retained device reference prevents pointer reuse from impersonating the same device.

## Verification and limits

tools/ovl-theme-preview.ps1 -Advanced renders the real theme with representative settings,
without launching the game. It also drives production widget wrappers with native mouse
events and checks checkbox, single button activation, slider value change, collapsing
header and disabled control behavior. Preview content is representative, not a capture of
the live game. tools/ovl-theme-preview.ps1 supports -Size for square output dimensions.
Headset readability, the larger default window and controller reach still require a user
launch after explicit installation approval. No installation is authorized yet.
