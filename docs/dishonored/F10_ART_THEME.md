# Expandable F10 art theme (VR-206)

The generated background and materials follow the supplied reference while preserving
native ImGui interaction. The default window is square, centered, and 46% of the shorter
eye-texture dimension (1265 pixels on a 2750 x 2850 eye texture). The panel remains
resizable, with a text-dependent minimum to keep action labels inside their buttons.
Automatic text scale follows the square reference dimensions; a saved UiScale wins.
Each tab has a separate scrolling child; the top controls and footer stay
outside it. Tabs overflow by scrolling. No game or installed ini has been changed.

## Adding controls

Use dvr::ovl::section, button, checkbox, slider_float and slider_int from ovl_ui.h.
These call the corresponding native ImGui controls and decorate their backgrounds.
ImGui IDs, focus/navigation, item-activity queries and the original setters/save callbacks
remain intact. Keep OvlTip and the existing save-on-change helpers. Radio buttons use the native interaction with restrained brass rings; combo
boxes and input fields use the shared native palette/font. No new asset is needed for a
new toggle. Add new tabs through OvlTabBody with a unique child ID for independent scrolling.

Basic/Advanced/Debug tier logic remains unchanged. The Hands size and Sleeve sections open
initially so the default view resembles the reference; other sections remain collapsible.
Reset, save defaults, and the visible L3+R3 click/hold hint remain available.

## Artwork and lifetime

Five masters in assets/ui/f10 replace per-widget image inventories; README.md records
the prompt set. They are embedded in the DLL as resources. WIC decodes once per D3D11
device; COM initialization is balanced, failed assets fall back to flat materials, and
old textures release when the device changes. This creates no D3D9 default-pool resources.
The retained device reference prevents pointer reuse from impersonating the same device.

## Verification and limits

tools/ovl-theme-preview.ps1 -Advanced renders the real theme with representative settings,
without launching the game. It also drives production widget wrappers with native mouse
events and checks checkbox, single button activation, slider value change, collapsing
header, radio selection and disabled control behavior. Computed bounds check action
label fit, scrolling-child/footer separation and footer containment. Preview content is representative, not a capture of
the live game. tools/ovl-theme-preview.ps1 supports -Size, -TextScale, -Tooltip and -Bottom.
The fixture includes all hand adjustment axes (extra axes in More adjustments), advanced
preferences and sleeve detail. It uses the production scrolling-child/footer helpers.
Visual QA covers Basic/Advanced at 1254 square, Advanced at 900 square with 1.54 text,
1254 square with 2.0 text, a real hover note, and the bottom of the scrolled settings.
Headset readability, the larger default window and controller reach still require a user
launch after explicit installation approval. No installation is authorized yet.

## Reference refinement

The revised pass replaces the busy skyline/brass frame with a muted painted backdrop,
a light Perpetua title, Times body/italic notes, torn parchment sections with diamond
markers, and quiet framed slate controls. The shortcut is one visible line. Recenter,
Save and Reset share a row. Left/right, up/down and pitch stay visible; depth/yaw/roll
and the advanced numpad preference are available through More adjustments. No setting
or adjustment axis was removed.

The initial screenshot fixture put a note over a slider and omitted half the adjustment
rows. That fixture was replaced with the actual hover-note path and a complete control
inventory, using the same scrolling layout as the game. Scrolled content clips at its
child boundary and cannot paint into the fixed footer. These are desktop checks; headset
interaction and persistence remain a separate acceptance step.
