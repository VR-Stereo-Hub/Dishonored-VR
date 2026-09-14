# The HUD redo: a prompt for the next session

This file IS the prompt: paste the whole file into a fresh session started on
`VR-Main`. It also lives at
`git show claude/dishonored-vr-hud-cinematics-149a8e:docs/dishonored/HANDOFF-HUD-REDO.md`.
The branch it lives on (`claude/dishonored-vr-hud-cinematics-149a8e`, PR #12,
ticket VR-8) is ABANDONED: nothing on it merges. It is kept as the reference for
the HUD work below and for nothing else. Its cinematic work ([Cine] keys, the
soiree lane, the pause probe) is superseded by PR #56 and #58 on VR-Main and must
not be ported.

## The task in seven lines (the rest of this file is the detail behind each one)

1. Start a new branch off the current `VR-Main` for a HUD redo, with a new Linear
   ticket in "Dishonored VR Mod" (team VR). Follow this file.
2. Replicate the head-locked HUD window from the abandoned branch (section 1):
   the HUD draw rule is backbuffer + full viewport + depth off + alpha blend (the
   opaque scene resolve must be excluded); redirect those draws to a private
   A8R8G8B8 target, copy at Present into two shared slots (fence both ways, D3D11
   queries need Flush, time waits with QPC), alpha-repair with max(r,g,b), clear
   after the copy every present, hand the R8G8B8A8 texture to the runtime layer's
   HUD quad through `set_hud_texture_provider`. Gate on GAME STATE, never on the
   draw: the pause menu is the same draw class.
3. Bring back the 38.92 wrist HUD as a second anchor on the tracked hands VR-Main
   now has (section 2): its placement math, its shipped values (0.22 m wide,
   0.06 m above the left grip), `get_hand_pose` grip poses, a LOCAL-space quad
   layer, plus an orientation-follow option.
4. Both anchors live at once, switchable per HUD element in F10 (window / hand /
   in-frame / off). Elements are identified by the screen REGION of the redirected
   draws' pre-transformed vertices, each region routed to its own target, slot
   and quad (section 4.2).
5. Full customisation in F10, all persisted in the ini: per-element anchor,
   per-element position and scale on the window, position, size and hand on the
   hand, and the window's own distance, width, height, vertical and lateral
   offset, head-locked or world-locked-in-front. Presets in section 4.6 and a
   `hud reset`.
6. Pause menu, journal, notes, store and mission stats go INTO the window with
   the world in stereo behind (measured possible). Gate on PR #58's reflected UI
   owner, keep the 1500 ms resume stand-in so the projection never drops on
   resume (that is the "no re-render on resume" fix), keep the ghost rule, and
   make sure a screen riding the window does not also trigger its
   `[Screen] Anchor*` mono anchor. The main menu keeps the screen. Verify on the
   base that the stale-flag test no longer clears real pause menus at 1.5 s
   (VR-71). Do not port the cinematic work: it is done on VR-Main.
7. Validate on the simulator first (`dump hud`, per-eye captures, `quadLayers`,
   the pause/resume sequence, section 6), then hand over the headset list.

---

## 0. The task

Start a NEW branch off the current `VR-Main` (`claude/vr-<n>-hud-redo`, one Linear
ticket in "Dishonored VR Mod", team VR; branch and PR rules in
`docs/LINEAR_AND_GITHUB.md`). Build the game's HUD as a VR feature with two anchors:

1. a **HUD window** in front of the player (what this branch shipped as the
   head-locked panel), and
2. the **tracked hands** (what build 38.92 shipped as the wrist HUD), now possible
   because VR-Main has working tracked hands and motion aim (VR-33, PR #33 to #58).

Both anchors must be available at once, switchable in the F10 menu, ideally per
HUD element. Everything about them is customisable from F10 and persisted in the
ini: which anchor each element uses, where each element sits on the window or on
its hand, and the window's own position, width and height. Ship sensible presets
for both anchors so the first headset run has something to judge. The pause menu,
the journal, notes and every other in-game reading screen go INTO the HUD window
(the world stays in stereo behind them) and resuming the game must not re-render
or reload the stereo. The main menu keeps the screen.

Read first, in this order: `docs/STATUS.md` on VR-Main (the 2026-09-14 state:
PR #56/#57/#58 merged, "mono UI ownership and anchoring"),
`docs/dishonored/STACK_ACCEPTANCE.md`, `docs/dishonored/MONO_ANCHOR_UI_STATE.md`,
`docs/dishonored/VR-33-HANDS-AND-WEAPONS.md`, then the four ENGINE_NOTES sections
named in section 5 of this file (they are on the abandoned branch; `git show
claude/dishonored-vr-hud-cinematics-149a8e:docs/dishonored/ENGINE_NOTES.md`).
Validate on the simulator before asking for a headset (`docs/VERIFICATION.md`).

---

## 1. What this branch built: the head-locked HUD panel (replicate this)

### 1.1 The measured fact everything rests on

Dishonored draws its world into an OFFSCREEN scene target the size of the render
(2496x2688 per eye on the Quest profile) and paints the whole Scaleform HUD onto
the BACKBUFFER at the tail of the frame. The scene reaches the backbuffer by a
`StretchRect`, not a draw. So the render target alone separates HUD from world.
Measured with the draw census (`core/gfx/draw_census`, `[Draws] Census=0`,
`draws on|off|status|kill <key>|unkill`) in the sewers on `stereo reentry`:

| per present | value |
|---|---|
| draws | 1205 (DrawPrimitive 0, DrawIndexedPrimitive 382, DrawPrimitiveUP 13, DrawIndexedPrimitiveUP 810) |
| draws to the backbuffer, gameplay | 5 buckets, 15.0 per present, ordinals 1177..1221 |
| draws to the backbuffer, pause menu | 10 buckets, 95.9 per present, ordinals 1126..1223 |
| tonemap draws | 0 (the resolve is a StretchRect) |
| state blocks created | 1 per run (Scaleform does not restore state behind the setters, no Apply hook needed) |
| distinct pixel shaders | 81 |

Proven by picture (`draws kill`): killing all five backbuffer buckets removed the
HUD and left the world pixel-identical; killing any world bucket never touched the
HUD. The bucket SET drifts with what is on screen, so the redirect keys on the
RULE, never on a list of bucket keys.

### 1.2 The HUD draw rule (four terms, all required)

```
rt0 is the backbuffer (or unset)            // the world never draws there
viewport == the whole backbuffer (X=Y=0)    // every HUD draw covers the target
D3DRS_ZENABLE == D3DZB_FALSE                // depth off
D3DRS_ALPHABLENDENABLE != FALSE             // EXCLUDES the opaque scene resolve
```

The fourth term was learned the hard way: the scene resolve is a full-screen
opaque textured quad (2 primitives, depth off, full viewport) and without the
blend term the panel showed the whole world. `draw_census.h::hud_class()`
implements the rule from state SHADOWED in the setter hooks, because the device is
PURE (no `GetViewport`, no `GetRenderState`). The classifier hooks: Draw* (vtable
81-84), SetViewport 47, SetRenderState 57, SetTexture 65, SetVertexDeclaration 87,
SetVertexShader 92, SetPixelShader 107, BeginStateBlock/EndStateBlock 59/61. Draws
arrive on the presenting thread (checked every present; the census refuses if
that ever changes). Constants and their derivation:
`patterns.h` "The Scaleform HUD draw class" (`kHudFingerprintMeasured=true`,
`kHudSceneTargetIsOffscreen=1`, `kHudRequiresFullViewport=1`,
`kHudRequiresDepthOff=1`, `kHudRequiresAlphaBlend=1`, `kHudTailFractionSeen=0.92`).
The fork's old terms that are WRONG for this frame: `rt-portrait` (our eyes are
portrait), UP-entry-point only (810 world draws per present are UP here),
`samples-rt` (most HUD draws are untextured fills; requiring a texture kept 1 of
15).

### 1.3 The redirect (`core/gfx/hud_capture`, namespace `dvr::hudcap`)

Per qualifying draw: `begin(dev, vp)` binds a private `A8R8G8B8` render target at
the backbuffer's size AND multisample type (so the depth-stencil stays legal),
then re-applies the viewport (binding a target resets it), the draw runs, `end()`
restores the game's target (from the classifier's shadow, never `GetRenderTarget`)
and viewport. No reference to an engine object is kept past the call. Cost: about
28 extra `SetRenderTarget` per present against 1205 draws; the pace stayed
11.1 ms per tick at 90/s.

Per present (`end_frame`, called from `frame_hooks.cpp` BETWEEN
`dvr::stereo::end_frame` and `dvr::vr::on_present_end`, so it belongs to no stereo
method):

1. `StretchRect` the private target into one of TWO shared `A8R8G8B8` slot
   surfaces at `[Hud] SlotScale` (0.50) of the frame: 1248x1344.
2. Fence it both ways: a D3D9 `D3DQUERYTYPE_EVENT` after the blit, a D3D11 event
   query after the read. **A D3D11 event query never completes until
   `ctx->Flush()`** (the read fence spun and timed out 1033 times per run before
   the flush). **Budget the waits with `QueryPerformanceCounter`, not
   `GetTickCount`** (15 ms tick: a 10 ms budget expired on the first read).
3. Open the slot on the mod's D3D11 device (`OpenSharedResource`), `BlitQuad`
   it into an `R8G8B8A8_UNORM` panel texture with **alpha repair**: `alpha =
   max(r, g, b)` (`blit_quad.cpp`, entry `psalpha`, `draw(..., alphaRepair=true)`).
   The compositor gets premultiplied semantics (`BLEND_TEXTURE_SOURCE_ALPHA_BIT`,
   no unpremultiplied bit). Consequence: dark HUD strokes go faint (additive
   look). Deliver the PREVIOUS slot, never the one just written.
4. **Clear the private target AFTER the copy, every present, unconditionally**
   (`ColorFill`, falling back to bind + `Clear` if refused). Dishonored hides HUD
   elements when they are full, so a lazy clear left a dropped body's carry icon
   on the panel for minutes (the fork's M8.4 bug).
5. Hand the texture to the runtime layer through
   `dvr::vr::set_hud_texture_provider(provider_texture)`; null = no HUD this
   present. `panel_texture()` is the ungated one for `dump hud`.

`on_reset()` releases every default-pool object (an unreleased one makes Reset
fail forever = black screen). `shutdown()` on PreExit.

### 1.4 The gate (game state, never the draw)

```
xrGate  = dvr::hud::gate() || menuOverride     // the runtime saw a projection present with an eye tag
wantArm = g_on && xrGate && g_gameGate && g_handoffReady
```

`dvr::hud::gate()` is the runtime's own signal (`set_gate(srFrame)` in the runtime
layer, remembered by `hud_stub` instead of discarded). `g_gameGate` is published
from `DvrGameTick`:

```
set_game_gate(g_verdictLast && !g_wheelHeld && (!g_cineNow || g_cineHudPanel))
```

The pause menu is drawn by the SAME draw class, so a draw-only gate sweeps the menu
onto the panel (the original's inherited bug). The verdict carries the positive
main-menu signal (`!g_mainMenu`) the original lacked. The power wheel is also this
class: the redirect pauses while it is held.

### 1.5 In-game menus on the panel (`[Hud] MenuOnPanel=1`)

In `present_tick.cpp`'s verdict:

```
inGameMenu = (g_menuOpen || g_inMenu) && !g_mainMenu
menuTerm   = inGameMenu && !g_hudMenuOnPanel
menuGrace  = g_hudMenuOnPanel && g_menuFromLive && !g_mainMenu && pawn
             && (now - g_menuClosedMs) < 1500 ms
viewTerm   = viewLive || (g_hudMenuOnPanel && inGameMenu && g_menuFromLive) || menuGrace
verdict    = pawn && !menuTerm && !g_mainMenu && !cineTerm && viewTerm
```

and the panel's override `set_menu_override(g_hudMenuOnPanel && g_menuFromLive &&
!g_mainMenu && g_verdictLast && wants_projection() && (menu open || within the
1500 ms grace))`. Three facts behind it, all measured:

- A paused menu drops the verdict on `viewLive` (the pause silences
  `ProcessViewRotation`), NOT on the menu flags. Removing the menu term alone
  changed nothing, so an open in-game menu stands in for the view term.
- The camera upload keeps flowing while paused, so the paused world is a live
  stereo pair (`out/s=120 L/s=60 R/s=60 mono/s=0` with the pause menu open); the
  game's pause blur is a post-process on the scene target and stays in the world.
- **The resume gap**: when the menu closes the view pipeline is silent for a few
  presents until its first dispatch; without the 1500 ms grace the runtime dropped
  the projection to its screen and brought it straight back on EVERY resume, which
  the headset saw as the stereo "reloading". The grace is the requirement "resume
  must not re-render" and it is what this branch's fix looks like.
- **The ghost**: `Dis_OpenPauseMenu` fires during a LOAD with no menu on screen. A
  stand-in that trusted the flag held the projection through a loading screen
  (the loading board sat inside the projection layer). So the stand-in is granted
  only to a menu that OPENED while the view pipeline was live (`g_menuFromLive`).

The headset run of the first menus-on-panel build (run 47-04) broke: navigating
the pause menu went wrong and a new AV in game code appeared
(`Dishonored.exe+0xaf6a73`, state NO_PAWN). It was reverted, then re-applied with
the grace and the ghost guard above; that re-applied form was simulator-verified
only. Treat the headset behaviour of menus-on-panel as UNVERIFIED.

### 1.6 The runtime layer's HUD quad (the consumer, verbatim BioShock code)

`openxr_runtime.cpp` "HUD floating quad (session 19)": submitted only when a base
layer exists AND `projectionMode` AND the view space is valid. Head-locked
(`g_viewSpace`), no rotation, position `(0, g_hudUpM, -g_hudDistM)` =
`(0, -0.10, -1.30)` m, width `g_hudWidthM` = 1.25 m, height from the texture
aspect (1248x1344 -> 1.35 m tall), F10 Runtime tab sliders (distance 0.5-3.0,
width 0.3-3.0, up -1.0..1.0), `set_hud_quad(distM, widthM, upM)`. The copy is a
`CopyResource`, so the provider's texture must be on the runtime's D3D11 device,
in the swapchain family format (R8G8B8A8) and dimensionally stable (a change
rebuilds the swapchain). **The sliders are NOT persisted on this branch** (no
[Hud] keys for them); the redo must persist them. Verified: `xr: HUD quad live
(1248x1344, 1.25 m wide at 1.30 m)`. Legibility, size and placement were never
judged on a headset.

### 1.7 Keys, words, F10, files

| key | default | meaning |
|---|---|---|
| `[Hud] Panel` | 1 | the redirect + the quad; refuses if the fingerprint is unmeasured |
| `[Hud] SlotScale` | 0.50 | panel texture = frame size x this |
| `[Hud] MenuOnPanel` | 1 | in-game menus ride the panel, world in stereo behind |
| `[Draws] Census` | 0 | the table and VERDICT every 3 s (off = one bool per draw) |

Seam: `hud on|off|status|scale <f>|menu on|off`, `draws on|off|status|kill <key>|hud|unkill`,
`dump hud` (writes the panel texture; **`DumpTexturePng` swaps R and B**, read dumps
for geometry, never for colour). F10 Display: "HUD panel" tickbox, "in-game menus
on the panel too", "draw census". status.json carries `hud` and `draws` objects.

Files on the abandoned branch (`git diff --stat origin/VR-Main...HEAD`):
`src/core/gfx/draw_census.{h,cpp}` (980 lines), `src/core/gfx/hud_capture.{h,cpp}`
(491), `src/core/gfx/blit_quad.{h,cpp}` (alpha repair), `src/core/vr/hud_stub.{h,cpp}`
(gate remembered), `src/game/dishonored/patterns.h` (the fingerprint block),
`src/game/dishonored/present_tick.cpp` (verdict + gates), `src/core/framework/frame_hooks.cpp`
(the end_frame slot, reset, shutdown), `src/core/config/config.cpp`,
`src/core/ui/overlay.cpp`, `src/game/dishonored/commands.cpp`,
`src/mod/state/13_game_dishonored_game_state.inc`, `src/CMakeLists.txt`,
`tools/xrsim/hud-panel.xrs`. Commits worth reading in order: `2dee72f9` (census),
`b28eb9d5` (fingerprint), `13bc38e4` (gate + alpha), `76fb60b3` (panel),
`a33cbcee` (ships on), `7e08c38e` (menus on the panel, grace, ghost guard).
The diff against VR-Main is additive, but VR-Main has moved a lot since (see
section 3): re-derive on the new base rather than cherry-picking blindly.

---

## 2. What build 38.92 shipped: the wrist HUD on the tracked hand (replicate the anchor)

Full record: ENGINE_NOTES "The HUD and the cinematics: what the original did"
(sections 1-6). Code: `git show 4bba213e^:src/core/gfx/hud_panel.cpp` (placement and
draw), `4bba213e^:src/core/gfx/present.cpp` lines 63-160 (the eye loop) and
378-431 (the readback) and 639-698 (the gate), `4bba213e^:src/core/config/config.cpp`
lines 776-785 (keys). The fork that produced the pixels survives under the tags
`dxvk-m8.2-shipped` etc. and is NOT to be brought back; section 1 replaces it.

### 2.1 The producer (fork) - replaced by section 1, listed for the record

HUD = a full-viewport, full-size, landscape, at least 1024 wide, pre-transformed,
non-depth-tested user-pointer draw that samples a plain (non render target)
texture; redirected to an `A8R8G8B8` target the size of the whole SBS frame;
`hudskip=` per-pixel-shader exclusion list (FNV-1a of the DXBC) for world-anchored
UI such as quest markers; a census of redirected shaders once per launch. The
clear rule's final form (38.55): clear after copy, every present.

### 2.2 The consumer gate (every clause a scar)

```
dlgNow = DialogHudOff && now < dlgUntilMs
want   = WristHud && g_handMesh && CylTruthLive() && !dlgNow
         && !g_menuOpen && !g_inMenu && !g_sbsMonoNow && !g_wheelHeld
```

`g_handMesh` = the hands are actually driving ("the sword follows your hand means
you are playing"; without it the MAIN MENU got swept onto the wrist).
`CylTruthLive()` = a possessed pawn's cylinder read within 3 s. The known bug
(HANDOFF 8.4): gameplay to main menu leaves the pawn latch true for up to 3 s and
the main menu lands on the wrist. Fix = the positive `!g_mainMenu` term (VR-Main's
verdict has it, and PR #58 now reads the UI OWNER by reflection, section 3).

### 2.3 The readback and the panel

Fork RT -> `StretchRect` (LINEAR) into a 1008x568 `X8R8G8B8` default-pool
target -> `GetRenderTargetData` -> sysmem -> D3D11 `B8G8R8A8_UNORM` texture ->
drawn per eye as an ADDITIVE billboard (`SrcBlend=ONE, DestBlend=ONE`), depth test
off, mid-depth, after the game quad and the hands, before ImGui, projection mode
only. Additive because the target clears to black each frame, so black is
invisible and the elements float like a hologram with no alpha correctness needed.
Section 1's alpha repair (`max(r,g,b)`) is the same idea expressed as alpha, which
is what a compositor quad needs.

### 2.4 The placement math (head space: x right, y up, -z forward)

```
d   = controllerPos - headPos                 // world
pr  = R_head^T * d                            // controller in head axes
pr.y += PanelUp                               // lift, a pure head-space +Y
if (pr.z > -0.06) hide                        // behind or at the face
n   = normalize(-pr)                          // billboard: face the head exactly
if (|pr| < 0.05) hide                         // hand at the eye
rgt = worldUp x n ; if (|rgt| < 0.2) hide     // within ~11.5 deg of vertical
upP = normalize(n x rgt)
hw  = PanelSize/2 ; hh = hw * (568/1008)
corners TL TR BL BR = pr -+ rgt*hw +- upP*hh
```

Three properties to keep: the panel takes only the controller's POSITION, never
its orientation (the redo should ADD an orientation-following mode as an option,
since the hands now have a proper grip transform); it keeps world-up as its up
axis so it never rolls; the lift is head-space +Y. For the redo, use the runtime
layer's quad layer in LOCAL space (the head-locked quad is view space; the wrist
panel wants `XR_REFERENCE_SPACE_TYPE_LOCAL` or a per-frame pose computed from
`get_hand_pose(hand, aimPose=false, ...)`, the GRIP pose located at the same
predicted display time as the head). One quad layer per anchored element group.

### 2.5 Keys and the shipped values

| key | default | shipped/tuned | note |
|---|---|---|---|
| `[Hud] WristHud` | 1 | 1 | F10 checkbox |
| `[Hud] PanelHand` | 0 (left) | 0 | 1 = right |
| `[Hud] PanelSize` | 0.16 m | **0.22 m** (the only HUD change the tester asked for) | clamp 0.05-0.60, slider 0.06-0.40 |
| `[Hud] PanelUp` | 0.06 m | 0.06 | |
| `[Hud] DialogHudOff` | 1 | 1 | park the panel during conversations |
| `[Hud] DialogHoldMs` | 12000 | | 1000-120000 |
| `[Screen] MirrorHud` | 0 | | desktop inset, bottom-left, one fifth height |

The dialogue rule (38.47-38.53): `PlayerChoice` substring over the event family;
`Dis_PlayerChoice_RequestSkip` arms a 12 s hold (re-armed by every member),
`OnPlayerChoiceConfirm` releases after 1500 ms; a `RequestSkip` within 150 ms of
any `Versus` event is the block-key alias and is ignored; `..._Released` never
arms. PR #58 now reads the HUD choice overlay by reflection, which may make this
event rule unnecessary: check `MONO_ANCHOR_UI_STATE.md` first.

---

## 3. What VR-Main has now that the redo builds on (do not fight it)

- **Tracked hands and motion aim** (VR-33, `docs/dishonored/VR-33-HANDS-AND-WEAPONS.md`,
  `ARM_HAND_SPLIT.md`): the hand is placed by rewriting the bone palette about the
  palm; the grip transform is a reflection (parity sign beside a rotation,
  `MP_GRIP_VERSION`); numpad adjust; `[Hands] ModelScale`; the coordinate bridge
  between controller space and the draw's basis. Hand poses:
  `dvr::vr::get_hand_pose(hand, aimPose, HeadPose&)` /
  `input_get_hand_pose(hand, aimPose, pos3, quat4)` (grip = placement, aim = the
  ray), consumers in `aim_seam.cpp` and `present_tick.cpp:268`. Sim injection:
  `set_sim_hand_pose`, `vrrec`.
- **Mono anchoring and UI ownership** (PR #58, VR-107/108/74/71,
  `MONO_ANCHOR_UI_STATE.md`): the mono screen is anchored upright per context
  (`[Screen] AnchorMono=1`, `AnchorPause`, `AnchorJournal`, `AnchorNote`,
  `AnchorWheel`, `AnchorStore`, `AnchorMissionStats`, `AnchorCinematic`,
  `AnchorLoading`, `AnchorMainMenu`, `AnchorOther`; API `set_mono_anchor`,
  `mono_anchor_enabled`, `mono_anchor_contexts`, `recenter_mono_anchor`). The
  CURRENT UI owner is read by reflection: `DishonoredEngine -> GamePlayers[0] ->
  Player.Actor -> WorldInfo.Game -> DishonoredGameInfo.m_pGlobalUIManager`, whose
  movie holders identify main menu (`m_Screen`: None 0, Title 1, Async 2, Main 3),
  pause, note (`m_bNoteVisible`), journal, wheel (`m_bWheelIsOpen`), store, mission
  stats; the Bink overlay manager gives loading. **This is the positive signal
  section 1.5's event flags (`g_menuOpen`/`g_inMenu`, the ghost, the 1.5 s
  stale-flag clear) were standing in for. Use the owner, not the flags.**
- **VR-71 is Done on VR-Main** (stale-menu input recovery). Verify on the base:
  on this branch the stale-flag test (`head_track.cpp`, `[Menu] GhostClearByRate=0`)
  cleared EVERY real pause menu 1503 ms in, the pad bridge's cursor nudge then put
  Scaleform into mouse mode and the gamepad A died. Section 1.5's menus-on-panel
  also ended at that clear. If the base still clears the flag, the menu-in-window
  feature cannot work; the owner read above is the discriminator.
- **Cinematics are done** (PR #56/#58): do not port `[Cine] Mode|HudPanel|HeadLocked`,
  `cine`/`vrcine` words, or the soiree lane. One measurement from the abandoned
  branch worth knowing if a cutscene ever glues again: the intro boat ride renders
  from an `InterpTrackSoireeControl` rotator at +0x210 (moved by the mouse, not by
  the controller's rotation), VR-70.
- The frame path: `stereo reentry` default, C5Pair, PoseLag=2, HoldUntagged=3,
  the readback-owned tick, `arm-res.ps1` for the render size (the ini is the
  authority, VR-66). The HUD's `end_frame` slot sits between
  `stereo::end_frame` and `vr::on_present_end`; keep it there.

---

## 4. Requirements, with the design the measurements permit

1. **Two anchors, live-switchable in F10**: `window` (head-locked quad, section 1.6
   with persisted keys) and `hand` (a quad placed from the grip pose, section 2.4
   with an orientation-follow option). Also `frame` (leave the element in the eye
   textures) and `off`. Keys under `[Hud]`.
2. **Per-element routing.** The redirect classifies DRAWS, not elements, and the
   elements are untextured Scaleform fills that do not identify themselves. The
   honest route is by SCREEN REGION: Scaleform draws pre-transformed vertices
   (`HasPositionT`), so each redirected draw's vertex bounding box in normalized
   backbuffer space names its element by where it sits (health top-left, mana,
   ammo/equipment bottom corners, reticle centre, subtitles bottom centre,
   objective marker, the interaction prompt). Ship a region table (measured with
   the census, `draws status` prints buckets; add the bbox column) and route each
   region to its own private target -> its own slot -> its own quad. A draw that
   spans regions (a full-screen vignette) goes to the window. Log the census of
   regions once per launch so a misrouted element is a one-line fix in the ini.
   If a per-element split proves too costly for a first build, ship two groups
   (status elements to the hand, the rest to the window) and say so.
3. **Per-element placement**: on the window, an offset and scale within the panel
   (or per-region sub-quads at their own positions); on the hand, a position in
   the hand's grip frame (x/y/z metres), a size, and which hand. All F10 sliders,
   all persisted, with `hud reset` to the presets.
4. **The window itself**: distance, width, height (free aspect: letterbox the
   texture or crop the region), vertical offset, and lateral offset, plus
   head-locked versus world-locked-in-front (LOCAL space, recentred by a key).
5. **Menus and reading screens in the window**: pause, journal, notes, store,
   mission stats. The world stays in stereo behind them (measured possible,
   section 1.5). Use PR #58's UI owner as the gate, keep the resume grace
   (1500 ms stand-in) so the projection never drops on resume, and keep the ghost
   rule (a menu that opens with no live view pipeline is a load, not a menu).
   Reconcile with `[Screen] AnchorPause` etc.: when a screen rides the HUD window
   the mono anchor for that context must NOT take over (the verdict stays TRUE
   and no mono quad is submitted). The main menu keeps the screen.
6. **Presets** (starting values, headset-judged later):
   - window: 1.25 m wide at 1.30 m, 0.10 m below eye height, head-locked,
     SlotScale 0.50 (what this branch shipped; never judged).
   - hand: left hand, 0.22 m wide (the 38.92 tuned value), 0.06 m above the grip,
     billboarded to the head, world-up; status elements (health, mana, equipment)
     there; reticle and subtitles on the window; prompts on the window.
7. **Every lever default OFF except the presets**, A/B live, a `hud status` line
   that says which anchor each element is on and why any element is not shown.

---

## 5. The measured numbers and the traps (do not re-derive, do not repeat)

- ENGINE_NOTES sections on the abandoned branch: "The HUD and the cinematics: what
  the original did (archaeology)", "The Scaleform HUD draw class, measured", "The
  HUD panel: the redirect, and the draw that nearly ruined it", "The pause menu,
  measured". Bring them across to the new branch's ENGINE_NOTES as they are.
- The scene resolve is an OPAQUE full-screen draw to the backbuffer: exclude by
  alpha blend, not by shader hash.
- The device is PURE: shadow every state you need in the setters; `GetViewport`
  does not exist; `SetRenderTarget` resets the viewport.
- D3D11 event queries need `Flush()`; time waits with QPC.
- Clear after copy, every present. Deliver the previous slot.
- Alpha repair is `max(r,g,b)` premultiplied; dark strokes go faint. If the
  headset dislikes it, the fix is a real alpha capture (render the HUD class with
  `D3DRS_SEPARATEALPHABLENDENABLE` forced so the private target's alpha
  accumulates coverage), not a different blend on the quad.
- `DumpTexturePng` swaps R and B.
- The pause menu is the same draw class as the HUD (95.9 draws per present); the
  power wheel too. Gate on state, never on the draw.
- `Dis_OpenPauseMenu` ghosts during loads. A menu flag is event history.
- The resume gap needs the 1500 ms stand-in or the projection drops every resume.
- The stale-flag test cleared real pause menus at 1503 ms (VR-71); the pad
  bridge's `SendInput` cursor nudge (every 45 polls when `!g_menuOpen`) is what
  kills the gamepad A in a menu.
- `Actor::WorldInfo` resolves by name (+0x160) but `WorldInfo::Pauser` does not on
  this build (declared elsewhere or absent); `rfl props <Class>` on the abandoned
  branch lists a class's properties with offsets if a pause discriminator is
  ever needed. PR #58's owner read is the better discriminator.
- The runtime's HUD quad sliders are not persisted anywhere today.
- The simulator: `xrsim-launch -ViaSteam`, `game-key.ps1` (now with arrow keys),
  New Game = title Return, Right, Return, Return (difficulty), Return (overwrite),
  then the "press any key" board after the load holds state LOADING until a key.
  `hud-panel.xrs` and `dump hud` verify the redirect; per-eye captures via
  `xrsim-shot` verify the world has no HUD in it.

---

## 6. Acceptance, in the order the headset can judge it

1. Simulator: `hud status` reports the redirect armed in gameplay only; `dump hud`
   holds only HUD pixels; per-eye captures show a clean world; the window and the
   hand quads both appear in the compositor capture (`quadLayers` in state.json);
   switching an element's anchor in F10 moves it between quads live; the pause
   menu appears in the window with `L/s=R/s>0` behind it; Escape-resume never
   drops the projection (no `cinematic quad` enter/exit around a resume); a load
   still shows the loading screen on the mono screen.
2. Headset: legibility of both anchors at the presets; the hand panel's size and
   lift; whether status elements belong on the hand; the pause menu in the window
   navigable with the gamepad for 10+ seconds and A selecting; reading a note in
   the window; the main menu untouched.

Put measurements on the ticket, not only the PR. File a ticket for anything found
and deliberately not fixed.
