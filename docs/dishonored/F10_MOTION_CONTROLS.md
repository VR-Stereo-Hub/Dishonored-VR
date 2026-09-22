# F10 menu motion controls (VR-174)

**Status: HEADSET-CONFIRMED (build 602, 2026-09-21).** Opening, pointing, clicking,
scrolling, slider nudging and the recenter hold all worked. The default placement is
where the tester put the panel (see "As built"). This is a port of the implementation the BioShock trilogy
VR mod ships and has headset-verified. It uses the same ImGui (1.92.8), the same OpenXR
runtime layer, and nearly the same F10 panel. The source files, read-only, are in the
trilogy repo:

* `src/core/ui/overlay.cpp` - pointer, click, scroll, slider tweak, display normalisation,
  geometry probe
* `src/core/vr/openxr_input.cpp` - the stick-click chord split into tap and hold
* `src/core/input/xinput_bridge.cpp` - the game-side suppression while the panel is up
* its `docs/STATUS.md`, "F10 is now usable from the controllers (2026-08-22, part 3)", and its
  ARCHITECTURE decision log, session 63

The four lessons it paid a headset session for are the reason to port rather than
re-derive. Each one is already a live bug or a trap in this repo:

| Trilogy lesson | State here |
|---|---|
| ImGui input has been EVENT-DRIVEN since 1.87. `io.MousePos` / `io.MouseDown[]` are rebuilt from the queue inside `NewFrame`, so direct writes are silently lost | `OverlayFrame` writes both directly (build 31.0 code). The click cannot land, and the cursor survives only while the real mouse is still |
| The viewport was the WINDOW CLIENT RECT while the panel draws into the BACKBUFFER, so the panel was guillotined or mis-scaled | `DvrOverlayDraw` sets a viewport of the eye texture size, but `ImGui_ImplDX11_RenderDrawData` resets it to `DisplaySize` (the client rect) |
| Event trickling deferred every wheel event, because a ray cursor moves every frame, so scrolling ran on after release | Trickling is on (the default); there is no scroll yet |
| Sliders position ABSOLUTELY on click, so a synthesised click-drag jumps the value to the aim point | No stick tweak yet |

## What is here today

* `OverlayFrame` (`core/ui/overlay.cpp`) runs inside the stereo method's overlay callback
  (`DvrOverlayDraw`, `game/dishonored/present_tick.cpp`). It runs once per eye texture
  (`reentry.cpp:471`, `mono_screen.cpp:63`), drawn over our hands. The target is
  `dvr::capture::width() x height()`, the game's render size.
* It is initialised with a fixed `FontGlobalScale = 1.6` and `ScaleAllSizes(1.6)`, and a fixed
  width of 560 px, whatever the resolution.
* The pointer: `pad_bridge.cpp:183-197` computes `g_ovlRayX/Y = rel.x/rel.z` from
  `HandRelFull`. `OverlayFrame` maps that to pixels as `centre + ray * PointerSpeed(2.2) *
  half-size`. That is a guessed gain, not the eye FOV, so the cursor can only sit on the ray
  at one resolution and one FOV. It is off by default (`[Overlay] ControllerPointer=0`).
* The recenter chord: both stick clicks, one edge per chord (`openxr_input.cpp:538-549`).
* The window subclass already swallows mouse and key messages ImGui wants
  (`game_window.cpp:30`), the same as the trilogy.
* There is no suppression of the game's trigger or stick while the panel is up, and the aim
  laser, dot and reticle keep drawing.

## The plan

Each step is one commit and is revertable on its own. Every step fails soft: without a
tracked controller, the panel is mouse and keyboard exactly as it is today.

### 1. Normalise the panel to the eye texture

* `OverlayFrame(w, h)` gets the target size from `DvrOverlayDraw`.
* After `ImGui_ImplWin32_NewFrame`, set `io.DisplaySize = (w, h)`. Rescale the real OS cursor
  from client pixels to target pixels with `AddMousePosEvent`, and only while no controller
  owns the cursor. The DX11 backend then sets the right viewport by itself.
* Defaults are fractions of the target, not pixels: size `0.42w x 0.45h`, centred,
  `FirstUseEver`. The trilogy measured these in its headset.
* Text scale `fs = 1 + (h/1080 - 1) * 0.5`, clamped to `[1, 2]`, applied as
  `style.FontScaleMain` (1.92 moved `io.FontGlobalScale` there). A **"UI text scale"** slider
  is added with an ini key, because this is a perceptual number. The current fixed
  `ScaleAllSizes(1.6)` padding is kept or rederived from the same factor; decide in the
  headset.
* Port `ProbeWindowGeometry`, which logs the window pos and size as FRACTIONS of the target
  (debounced), and the one-time client-rect-vs-target line. Whatever looks right in the
  headset can then be read back out of the log and baked in.

### 2. The pointer from the FOV projection

This ports `InjectControllerPointer` verbatim, except where the D3D9 host differs. It runs
on the overlay's own thread, between `ImGui_ImplWin32_NewFrame` and `ImGui::NewFrame`,
which is the only window in which an injected cursor survives. It never runs on the pad
thread, per the 31.1 lesson already commented in `pad_bridge.cpp`.

* Take `peek_head_pose` and the aim pose `get_hand_pose(hand, true)`, rotate the aim forward
  into the head frame, divide by the forward component, and divide by `fov_audit`'s tangents.
  That gives NDC, then target pixels. All three functions exist in our copy of the runtime
  layer.
* Hold the last good position, so both eye textures draw the cursor at the same pixel.
  Pointer ownership expires 500 ms after the last ray, so the real mouse comes back when the
  controller sleeps.
* Send input only through the event API (`AddMousePosEvent`, `AddMouseButtonEvent`,
  `AddMouseWheelEvent`, `AddKeyEvent`). Set `io.ConfigInputTrickleEventQueue = false`.
* `MouseDrawCursor` stays on, so ImGui's own cursor is drawn at the injected position in both
  eyes.
* Retire `g_ovlRayX/Y`, `g_ovlPtrValid/Down` and `[Overlay] PointerSpeed`, and list the
  removed key in RELEASE_NOTES "Upgrading". `[Overlay] PointerHand` stays.
  `[Overlay] ControllerPointer` stays as the lever and moves to default 1 once step 6
  passes. The trilogy gated the same behaviour until it was headset-tested.
* **Known approximation (trilogy note, still true here):** this assumes the eye texture
  holds the image at the FOV the layer claims. If the cursor sits a constant factor off the
  ray, that factor is the mismatch. The simulator measures it before the headset does
  (step 6).

### 3. Trigger, scroll and slider tweak

* The right trigger at more than half travel clicks (`AddMouseButtonEvent(0, ...)`). The
  overlay thread needs the XR pad state: publish the trigger and right stick from
  `openxr_input`'s snapshot, the equivalent of the trilogy's `last_xr_pad`.
* The right stick uses the DOMINANT axis only, with a dead zone of 0.25, so a scroll never
  nudges a value on the way past.
  * **Y scrolls:** `AddMouseWheelEvent(0, ry * 2.5 * dt)`, per second rather than per frame.
    This matters doubly here, because `OverlayFrame` runs once per eye texture.
  * **X tweaks the slider under the cursor** through ImGui's keyboard path. `UpdateSliderTweak`,
    right after `NewFrame`, does `SetActiveID(HoveredIdPreviousFrame)` and sets `ActiveIdSource`
    and `NavInputSource` to Keyboard. The stick then pulses Left/Right arrow key events at
    `2 + mag^2 * 28` steps per second. That is 1% of the slider's range per step, relative
    to its current value; it never jumps. The accumulator is capped at 4 so no burst is
    banked. Whether anything is hovered is sampled at the end of the draw
    (`IsAnyItemHovered() || IsAnyItemActive()`). While tweaking, the trigger is forced up
    so it cannot steal the item. All the internals used exist in our `imgui_internal.h`
    1.92.8.

### 4. Open and close from the controllers

The chord in `openxr_input.cpp` gets the trilogy's split:
* a **tap** (released within about 350 ms) toggles the panel;
* a **hold** (about 0.6 s) recenters once, and does not also toggle on release.

The panel's own close button (the X on its title bar) is clickable with the trigger too. The
change to the runtime-layer file is small and marked `(Dishonored)`, the same as the other
seams in that file.

**Trade-off to accept:** recenter stops being instant and waits out the hold. This is the
trilogy's call, and it was fine there.

### 5. The game stops hearing what the panel uses

While the panel is up, in `pad_bridge.cpp`, the last point before the virtual gamepad
reaches the game:
* the right trigger reaches the game as 0;
* the right stick is zeroed, so a scroll does not turn the view;
* the motion swing's attack pulse is suppressed, so a click cannot attack;
* the left stick stays live, so you can still walk with the panel open.

The aim laser, the aim dot and the HUD reticle are hidden while it is up, because they
land on the panel and fight the cursor for the same pixels.

### 6. Verify

The simulator comes first (`docs/VERIFICATION.md`). Everything in this list is
non-perceptual:
* **Cursor on the ray:** set the simulated hand at known yaw and pitch offsets
  (`xrsim-cmd hand ...`). The log line gives cursor pixel against predicted pixel. Take an
  `xrsim-shot` capture with the cursor visible in both eyes. Repeat at two render sizes
  (derive at more than one aspect).
* **Click:** press the simulated trigger over a checkbox. The ini key flips.
* **Scroll and tweak:** push the simulated right stick. The log shows the tweak's item id,
  its steps per second and the value change.

The headset then judges only the perceptual parts: text size, tweak pace, scroll speed and
where the panel sits.

## Logging (per the repo's logging rules)

* `overlay: pointer owner CONTROLLER|MOUSE` on each change, with the hand, the NDC, the pixel
  and the target size. The FOV tangents are logged once and again on any change.
* The window geometry as fractions (debounced). The client rect against the target, once.
* `overlay: tweak START item 0x... | STOP (N steps)`. Scroll rate, rate-limited.
* `input: chord TAP -> panel open|closed` and `input: chord HOLD -> recenter`.
* Every refusal gives its reason: no head pose, hand behind the head, `fov_audit` empty.

## Not in scope

* A world-anchored panel on its own quad layer. The trilogy asked for this and dropped it as
  too big: it needs an offscreen render target, a swapchain, and a ray-plane pointer.
* Driving the panel with the left hand, beyond the existing `PointerHand` key.

## As built (2026-09-21)

**The reticle while the panel is up (2026-09-22).** The panel used to switch the aim
ray off, because the dot landed on the panel and fought the cursor. With `[Overlay]
ReticleWhileOpen=1` the ray stays on. `OverlayFrame` publishes the panel's rectangle
(fractions of the eye texture) through `dvr::vr::set_aim_occluder`. The runtime
projects each reticle point into both eyes with the projection layer's own pose and
fov, and drops any point inside that rectangle. A quad layer always composites over
the projection, and the panel is drawn into the projection image, so the dot can only
look behind the panel by not being drawn there. The aim-visual outcome
`behind the F10 panel` counts those frames. `crosshair/panel:` logs the switch.

The game's arms are part of the game image, so they are always behind the panel too.
A hand is visible beside the panel, not through it.

**The cursor only over the panel (2026-09-22, first headset run of the above).** The
reticle was hidden correctly: it sat about 14 deg right and 13 deg up, inside the
panel. But ImGui drew its software cursor wherever the pointer ray landed, on the panel
or off it, so off the panel a cursor sat where the reticle should read. `io.MouseDrawCursor`
is now set per frame to "a panel window is hovered, or a drag that started on it is
active", so the cursor exists only on the panel and the reticle only off it.

* `core/ui/overlay.cpp` holds `OvlInjectControllerPointer`, `OvlUpdateSliderTweak` and
  `OvlProbeWindowGeometry`. `OverlayFrame(w, h)` sets `DisplaySize` to the eye texture and
  uses `style.FontScaleMain`. There is a "UI text scale" slider at the bottom of the panel.
* `core/vr/openxr_input.cpp` handles the chord: TAP (<350 ms) -> `take_panel_chord`, HOLD
  (600 ms) -> `take_recenter_chord`. It is gated by `set_chord_tap_opens_panel`, which
  follows `[Overlay] ControllerPointer`. Off is the original instant recenter.
* `core/input/pad_bridge.cpp`:
  * The 31.0 ray code is removed.
  * It toggles the panel on a tap.
  * While the panel is up it zeroes the pointing hand's trigger (and the right stick, for
    the right hand), plus the swing's RB pulse when `[Melee] Output=rb`.
* `present_tick.cpp`: the aim ray ticks as "not gameplay" while the panel is up, which
  hides the laser and dot. The HUD reticle is NOT hidden; it is the game's own element.
* Removed: `g_ovlRayX/Y`, `g_ovlPtrValid/Down`, `g_ovlPtrGain`, `[Overlay] PointerSpeed`.

**Side effect, accepted:** while the panel is up, Blink (left trigger) falls back to head
aim, because the ray is off.

**What to read in the log after a headset run:**
* `overlay: pointer owner CONTROLLER|MOUSE` - who owns the cursor, and why it changed.
* `overlay: pointer on the ray - ndc (...) -> px (...) of WxH, tangents a/b` (every 5 s).
  A cursor that sits off the ray by a constant factor is a rendered-vs-claimed FOV
  mismatch. The ndc and tangents give the factor.
* `overlay: window pos ... fractions ...` and the one-time `client rect vs eye texture` line.
* `overlay: text scale X from the eye texture height H`.
* `overlay: tweak START/STOP/LOST item 0x...`.
* `input: chord TAP (N ms) -> F10 panel toggle`, `input: chord HOLD -> recenter`, and the
  between-the-two release line.
* `pad/overlay: the F10 panel is up - ... reach the panel, not the game`.

**Headset result (build 602, 2750x2850 eye texture):** everything worked as intended. The
tester resized and moved the panel, and the geometry probe read the final place back out:
top-left at 0.3149,0.3596 and size 0.3855 x 0.2302 of the eye texture. That is now the
default, replacing the trilogy's 0.42 x 0.45 centred. The text scale was left at the
derived 1.54 (`1 + (2850/1080 - 1) * 0.5`).
