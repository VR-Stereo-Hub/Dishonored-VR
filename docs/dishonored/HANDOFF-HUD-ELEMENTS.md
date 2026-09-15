# Handoff: every HUD element on its own anchor, and a real alpha capture

Session prompt for the follow-up to VR-117 (the HUD redo). Read this whole file, then
`docs/STATUS.md`, `docs/dishonored/HUD_ANCHORS.md` (the design you are extending; section 6
is every measurement) and `CLAUDE.md`. This file is the intent; HUD_ANCHORS is the state.

## 0. Where you start

- **Branch off `claude/vr-117-hud-redo`, NOT `VR-Main`.** VR-117 is on PR #63, reviewed but
  held while VR-Main gets tested; the redirect, the sinks, the layout model, the runtime's
  HUD anchors block and the ride policy all live only there. Name the branch
  `claude/vr-<n>-hud-elements`. The PR's base is `claude/vr-117-hud-redo` and its body's
  first line is `Ref VR-<n>` until VR-117 merges; retarget to `VR-Main` and change it to
  `Fixes VR-<n>` then. Never merge to `VR-Main` without explicit permission.
- **Tickets.** VR-118 exists (Backlog, Research: find the Scaleform 2D transform so the
  region probe's rectangles land on screen). Keep it as the measurement ticket and move it
  to In Progress. Create two feature tickets from the template in
  `docs/LINEAR_AND_GITHUB.md`, milestone 42.0, project "Dishonored VR Mod": one for the
  per-element routing (this file's section 3; it depends on VR-118 and blocks nothing) and
  one for the alpha capture (section 2). One branch and one PR can carry both features
  if the commits stay one behaviour each; say which ticket each commit serves in the PR.
- **This PC.** The game is installed at `D:\SteamLibrary\steamapps\common\Dishonored`;
  launch through Steam only (`tools\xrsim-launch.ps1 -ViaSteam`). `$env:DVR_DATA_DIR=
  'D:\dvr-data'` for every harness call. The newest save is a death loop at the intro boat:
  reach gameplay with `tools\game-cmd.ps1 "console open L_PrsnSewer_P"` from the main menu,
  then one Return. `xrsim-cmd.ps1` needs `-Dir D:\dvr-data\xrsim`; `crosshair dot off`
  before counting quad layers; the sim's hand validity word is `hand l valid off|on`.
  The installed ini is a byte copy of `release/dishonored_vr.ini`; keep it that way and
  regenerate golden + release (`tools\ini-golden.py`, CRLF) whenever `WriteDefaultIni`
  changes. Copy the log out before every relaunch (rotation is one deep).
- **Simulator before headset**, one behavioural change per commit, every render lever
  default OFF with a live A/B (the per-element defaults are the one exception, see 3.6),
  log generously, everything measured goes to HUD_ANCHORS and ENGINE_NOTES in the same
  commit, STATUS rewritten and pushed at the end. No em dashes, no names, no chat quotes.

## 1. The two goals

1. **A real alpha capture** for the HUD sinks, with F10 controls, so dark strokes (text
   outlines, the black edges of the bars) survive instead of going faint under the
   `max(r,g,b)` repair.
2. **Every HUD element on the anchor the player chooses**: off, the frame (in the eyes, as
   the game draws it), the head-locked window, the world-parked window, a panel on the
   LEFT hand, a panel on the RIGHT hand. Per element, live from F10, persisted in the ini.
   The list of elements must be complete (every piece of UI the game draws in gameplay and
   in the riding screens), and the design must scale: a new element found later is one
   table row, not a new code path. Customisability and scalability come before cost; the
   HUD is a few dozen draws per present, so the budget is real but generous (VR-117
   measured 21 HUD draws per present in gameplay, 35 in the pause menu, 0.3 ms per tick
   for the whole redirect).

## 2. The alpha capture

### 2.1 What exists

`hud_capture.cpp` clears each sink to transparent black, the game's HUD draws land on it
with the game's own blend state (`SRCALPHA / INVSRCALPHA` on colour; whatever the game
left for alpha, which without separate alpha blending is the same equation and yields
`srcA*srcA + dstA*(1-srcA)`, too transparent for every semi-transparent pixel and exactly
zero for a black stroke), and `BlitQuad::draw(..., alphaRepair=true)` (`blit_quad.cpp`,
`psAlpha_`) writes `alpha = max(r,g,b)` into the R8G8B8A8 texture the quad layer shows.
The colour is premultiplied by construction (drawn over black), which is what the OpenXR
compositor assumes for `BLEND_TEXTURE_SOURCE_ALPHA_BIT` without the unpremultiplied flag.

### 2.2 What to build

- **Captured alpha.** For every redirected draw set `D3DRS_SEPARATEALPHABLENDENABLE=TRUE`,
  `D3DRS_SRCBLENDALPHA=D3DBLEND_ONE`, `D3DRS_DESTBLENDALPHA=D3DBLEND_INVSRCALPHA` before
  forwarding and restore the shadowed values after (the device is PURE: the shadow in
  `hud_class` is the only truth, and `hud_class` already hooks SetRenderState slot 57).
  That accumulates coverage `dstA = srcA + dstA*(1-srcA)`, the correct "over" alpha, and
  the colour stays premultiplied. Draws the game issues with additive colour (`ONE/ONE`;
  glows, the detection arrows: check the census) need the same alpha equation, not
  `ONE/ONE` on alpha; log the first draw of each blend mode seen inside the class.
- **The blit's alpha modes**, one pixel shader with a constant buffer (or three shaders):
  `repair` (today), `captured` (alpha from the sink's A channel), `mix` (alpha =
  max(captured, repair * k)) as the safe middle while the headset judges.
- **Legibility controls** the player can move live: alpha gain (multiply, clamp), an alpha
  floor for pixels with any colour (keeps thin strokes), a backdrop (a plate behind the
  window at a chosen colour and opacity, drawn into the out texture before the HUD so
  dark text reads on a bright room; per anchor, since the hand panel wants none), and a
  gamma nudge. Each is an `[Hud]` key, an F10 slider on the HUD tab and a `hud alpha ...`
  word. Ship `AlphaMode=repair` (today's headset-judged picture) and every slider at its
  identity value; the headset picks the default and the PR flips it with the verdict on
  the ticket.
- **Instruments.** `dump hud <sink>` already writes the out texture (R/B swapped, VR-13);
  add the alpha channel as a second grey PNG so a dump can fail the hypothesis "the
  strokes are in the alpha now". `hud status` gains the alpha mode and the sliders.

### 2.3 Verify

Simulator: `hud-panel.xrs` and `pause-ride.xrs` unchanged; a new `hud-alpha.xrs` that
flips the mode live (`hud alpha mode repair|captured|mix`) and asserts the quad layer
stays and the shot's HUD pixel count does not collapse (a mode that produces alpha 0
everywhere must FAIL it). `dump hud` alpha PNG: the health bar's outline present in
`captured`, absent in `repair`. Headset: dark strokes readable, no halo, no double edge.

## 3. Every element on its own anchor

### 3.1 What exists and what was measured

- `hud_layout` (`core/gfx/hud_layout.{h,cpp}`) owns elements, anchors, placement, presets,
  the provider that turns sinks into `HudQuadDesc`s, the `hud` word and the F10 tab.
  Elements today: `all, health, mana, equipment, reticle, subtitles, prompt, objective,
  vignette, menu`; anchors: `off, frame, window, hand`; ONE hand (`HandCfg.hand` picks L
  or R for the whole panel). `sink_for(bbox, &element)` routes a draw by its screen
  rectangle; with `[Hud] Regions=0` every draw is `all` and rides one sink.
- `hud_class` classifies a draw as HUD by four shadowed terms (rt0 = backbuffer, full
  viewport, depth off, alpha blend on; the scene resolve is excluded by the blend term)
  and, with `[Hud] Regions=1`, runs `probe_draw`: it reads the vertices (every HUD draw is
  a `DrawIndexedPrimitiveUP` with SHORT2 positions in Scaleform's shape space, ranges
  around -70..23000; a few `DrawPrimitiveUP` with FLOAT2 in the menus) at 0.5 us per draw.
  The transform hypothesis FAILED: `vs_const_shadow_row(0..3)` reads the same
  `(0, 0.002, 0.999, 1)` and `(0.726, 0, 0, 0)` for every HUD draw, and the rectangles it
  yields are nonsense. Everything else about the probe works. HUD_ANCHORS section 6 and
  VR-118 carry the numbers.
- The census (`draws on`, `draws status`) records per bucket: pixel shader hash, vertex
  declaration, target size and class, viewport, z, blend, `tex0`, primitive band. In
  gameplay the backbuffer population is 5 buckets (4 HUD candidates + the resolve), in the
  pause menu 11. The HUD is Scaleform GFx (UE3 build 9099's GFx 3.x integration):
  `DisGFxMoviePlayerHUD` is the gameplay HUD movie; `DisGFxMoviePlayerPowerWheel`, `..Note`,
  `..PauseMenu`, `..MainMenu`, `..MenuBase` are the screens (`ue3/ui_surface.cpp` resolves
  their properties by name; `docs/dishonored/GAMEPLAY_STATE.md` lists more of their flags).
  The UI owner (`ui_surface`) already names which SCREEN is up.
- The runtime block ("41.x (Dishonored, VR-117) HUD anchors", `openxr_runtime.cpp`)
  submits up to `kMaxHudQuads=6` quads from the provider, anchors `Window` (VIEW),
  `WindowWorld` (LOCAL, seeded at recenter), `Hand` (LOCAL at the grip pose, hand L or R
  per descriptor, billboard or follow-grip, the 38.92 hide rules). `hud_anchor.h` is the
  pure math with 30 host checks.

### 3.2 The goal, precisely

A table the player edits: one row per element, one anchor per row from {off, frame,
window, world window, left hand, right hand}, plus placement within that anchor (offset,
scale). Two hand panels with their own size, lift, orientation and tilt. The riding
screens (pause, note, journal, wheel, store, mission stats) are rows too. An element the
mod has not named yet still gets routed (to `default`) and shows up in the list so it
can be named. Everything in `[Hud]`, on the F10 HUD tab, in `hud` words, in `hud status`.

### 3.3 Identify elements by WHAT they are, not only WHERE they are

Screen regions (VR-118's route) are the cheap identity and they break exactly where a
player would notice: subtitles and prompts share the lower middle, the pickup toast slides,
the detection arrows orbit the reticle, the tutorial window covers the health bar. The
scalable identity is the Scaleform DISPLAY OBJECT each draw belongs to (the instance path
inside the HUD movie, `_root.hud.health` or whatever the SWF names it). This session's
first job is to MEASURE which of these identities is reachable on this build, in this
order, and stop at the first one that works well enough for a complete element list:

1. **Movie identity, engine side.** Every `UGFxMoviePlayer` renders its movie through the
   engine's GFx integration once per frame; a hook at that call (the UE3 side: find the
   render entry through `tools/ue3-natives.py <exe> natives --grep GFx` and the
   `UGFxMoviePlayer` vtable, `tools/pe-xref.ps1` for its callers from the viewport draw
   root the HUD tail already uses, `scene_draw.cpp` names it) tags every draw between
   entry and return with the movie player object, and `ObjClassName` names it. This
   separates the HUD movie from the wheel, the note, the pause menu, the subtitles if they
   are their own movie, at zero per-draw cost. It is also the right place to read the
   movie's own viewport and scale, which is probably where the 2D transform VR-118 looks
   for lives (GFx sets it per movie, not per draw).
2. **Display-object identity, Scaleform side.** GFx 3.x renders a sprite tree; each
   character's `Display` walks its children and issues the draws through the renderer
   (`GRendererD3D9::DrawIndexedTriList` and friends: this is what our D3D9 hooks see).
   Hooking the character display call and reading the character's instance name gives the
   per-element identity directly and survives every layout change. Find it by strings: the
   exe carries GFx's class and error strings; the AS instance names live in the SWF
   (game content, gitignored, read them with `draws` and a name dump at runtime, never
   commit them). Byte-verify every hook target, refuse on mismatch, fail soft.
3. **Region identity** (VR-118's measurement) as the fallback and the placement tool:
   fix `probe_draw` once the transform is found (dump the HUD vertex shader through
   `GetFunction` like the pixel shader hash does, read its `dcl`/`def` and which `c#` the
   position multiplies; check constants beyond c3, per-vertex components, and the movie
   viewport from route 1), then the bucket rectangles fall out of `draws regions` and the
   region table is written from a measurement, not a guess.
4. **Texture identity** (`tex0` in the census signature: the glyph atlas, the icon sheet,
   the bar gradients) as a tie-breaker inside one movie when 2 is out of reach.

Decide by measurement on the simulator in the first third of the session, write the
result in ENGINE_NOTES ("How the Scaleform HUD identifies its elements"), and say on the
ticket which identity ships and why the others were not taken. A route that cannot fail
its own hypothesis is not evidence: for route 2 the counterprediction is "the name dump
lists the health bar's instance while the bar is drawn and nothing while it is hidden".

### 3.4 The element list is measured, then named

Walk the game with the identity instrument on and enumerate: gameplay idle, health and
mana changing, a weapon switch, the wheel, an interaction prompt, a pickup toast, an
objective update, subtitles in a conversation, a dialogue choice, a tutorial window, the
detection indicators in combat, the hold-to-skip gauge, dark vision, a note, the journal,
the store, mission stats, the pause menu, the death screen. Every identity seen becomes a
row with a human name (the table in `hud_layout.cpp`); anything unnamed routes to
`default` and is listed by its raw identity in `hud status` and in the log
(`hud: new element <identity> -> default`), so a tester can name it from the log. Put the
list in HUD_ANCHORS with what each element is and when it draws.

### 3.5 The layout model, extended

- `Anchor` becomes `{Off, Frame, Window, WindowWorld, HandLeft, HandRight}`; `HandCfg`
  becomes two (`hand[2]`, each with x/y/z, lift, width, orient, tilt); `ElementCfg` keeps
  one placement per anchor kind (window, hand); the provider emits one quad per
  (anchor, sink) in use, so two elements on the left hand share one sink and one quad,
  and an element alone on the right hand gets its own. `kMaxSinks` and `kMaxHudQuads`
  rise to what the table needs (count the anchors in use, not the elements; the Quest 3
  layer budget is 16 and the runtime block already counts `hidden[budget]`).
- The per-element sub-rect within a shared sink (`subrect` in `HudQuadDesc`) comes from
  the identity route's rectangle when it has one; without one the element takes the whole
  sink and its placement offsets the quad (still correct: only that element's draws are
  in that sink). Draws without a rect never break routing.
- The riding screens are elements too (`ElMenu` today): keep the ride policy as it is and
  route the screen's draws by the same table, so a player can put the pause menu on the
  world window and the wheel on the hand.
- ini: `Element.<name>=<anchor>`, `Element.<name>.Win/Hand{X,Y,Scale}`,
  `Element.default=window`, `HandL.*` and `HandR.*`, `Region.<name>` only if route 3
  ships. `hud anchor <element> <anchor>`, `hud place ...`, `hud hand l|r ...`, `hud reset`.
  `kConfigVersion` stays; a missing key reads the compiled default.
- F10 HUD tab: the element table (name, anchor combo, two placement rows, "seen this
  session" tick), two hand blocks, the alpha block, the ride tickboxes, `Reset presets`.

### 3.6 Defaults

The shipped preset is the user's call at the end: propose "status elements (health, mana,
equipment) on the LEFT hand, prompts, subtitles, objective and toasts on the window, the
reticle in the frame, screens on the window" and let the headset decide. Until then ship
the VR-117 picture (everything on the window) so a tester who updates sees no change.

### 3.7 Verify

- Host: extend `hud-anchor-tests.cpp` (two hands, the tilt sign per hand) and add a
  routing test over the pure table (`hudlayout::route(identity) -> sink`, unknown ->
  default, a renamed element keeps its row).
- Simulator: `hud-elements.xrs`: `hud anchor health handL; hud anchor mana handR; hud
  anchor prompt window` then shots with `quadLayers` 3 and the sim's per-quad
  `space`/`pose` within 0.15 m of each posed hand; an untracked right hand hides only
  its quad; `hud anchor health frame` returns its draws to the eyes (`dump hud` of that
  sink empty, the frame shot with the bar). `pause-ride.xrs`, `wheel-ride.xrs`,
  `hud-panel.xrs`, `hud-quads.xrs`, `reentry.xrs`, `mono.xrs` unchanged.
- Cost: `perf: tick` before and after with the full table populated; log the per-present
  identity cost the way the probe logs its 0.5 us.
- Headset list on the ticket: legibility per anchor, the two hands' tilt signs, whether
  the reticle belongs in the frame, the alpha mode.

## 4. Traps already paid for (do not re-derive)

- The device is PURE: never `Get*` state, the shadows in `hud_class` are the truth;
  `SetRenderTarget` resets the viewport; bind sinks through
  `dvr::frame::orig_set_render_target`, never the vtable.
- The redirect arms on the runtime's projection MODE, not the eye tag (re-entry leaves
  6 to 21 untagged presents a second by design; a tag-following gate flickered at 10 Hz).
- The HUD quads are built AFTER the runtime's zero-layer hold; a riding menu blinks
  otherwise. `projStaleSubmits` climbs during a ride; do not assert it across a pause.
- A focus loss makes the game present from its game thread; log and count, never latch a
  refusal.
- D3D11 event queries need `Flush`; waits use QPC; clear after copy; deliver the previous
  slot; `DumpTexturePng` swaps R and B.
- A vertex buffer created write-only cannot be read; every HUD draw so far is a UP draw,
  so the probe reads from the pointer.
- The census's own zero: `draws status` with `Census=0` prints nothing by design.

## 5. Hand back

STATUS "Current state" and "Next steps" rewritten, a session-log entry, HUD_ANCHORS
extended (the element list, the identity route, the alpha modes, new measurements in
section 6), ENGINE_NOTES (the GFx render path and hook targets, derived and byte-verified;
addresses in `patterns.h`), VERIFICATION rows for the new sequences, TRAPS for anything
that bit, RELEASE_NOTES "Unreleased", the PR with the template filled and "what is
deliberately not here" naming tickets for whatever was found and not fixed, verdicts and
numbers on the tickets, and a numbered headset test list with the F10 fields to change.
