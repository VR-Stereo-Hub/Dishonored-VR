# The HUD on its anchors (VR-117)

The game's Scaleform HUD, taken out of the eye textures and shown on quads the
runtime layer composites: a WINDOW in front of the player and the tracked HAND
(what build 38.92 shipped as the wrist HUD), per element, with every placement
value in the ini and on the F10 HUD tab. In-game screens (pause, journal, note,
store, mission stats) ride the window with the world in stereo behind them.

Ticket VR-117. Supersedes the abandoned PR #12 (VR-8) and VR-38. The measured
facts it rests on are in ENGINE_NOTES, "The HUD sections carried from the
abandoned PR #12 branch": the whole HUD is painted onto the BACKBUFFER at the
tail of the frame while the world goes to an offscreen scene target, so the
render target alone separates the two; the scene resolve is the one opaque
full-frame draw and alpha blending excludes it; the pause menu and the power
wheel are the same draw class; the paused world is a live stereo pair.

## 1. The pieces

```
game draws -> core/gfx/hud_class   the rule (rt0=backbuffer, full viewport, depth off, blend on),
                                   the census (`draws`), the region probe ([Hud] Regions)
           -> core/gfx/hud_capture N sinks: a private A8R8G8B8 target each, two shared slots,
                                   D3D11 alpha repair (alpha = max(r,g,b)), an R8G8B8A8 texture
           -> core/gfx/hud_layout  elements, regions, anchors, placement: the ONE owner of
                                   [Hud]; the provider that describes the quads
           -> openxr_runtime.cpp   "41.x (Dishonored, VR-117) HUD anchors": up to 6 quad
                                   layers, one swapchain slot each, VIEW / LOCAL space
game state -> ue3/ui_surface.cpp   the ride predicate (ui_ride_policy.h)
           -> stereo_state.cpp     the scene verdict's stand-in while a screen rides
           -> present_tick.cpp     hudcap::set_game_gate(sceneVerdict && !wheel, rides)
```

The draw-hook chain is fixed by construction (`frame_hooks.h`): game -> the
frame hooks -> the hand census (which may DROP a draw or re-issue it) ->
`orig_draw_*` -> the inner hook (the HUD redirect) -> `raw_draw_*` -> D3D. A
dropped draw never reaches the redirect, and the redirect binds and restores
its sink through the raw SetRenderTarget, so no backbuffer detector sees it.

## 2. The keys (all `[Hud]` unless stated; every one has an F10 control)

| key | default | meaning |
|---|---|---|
| `Panel` | 1 | the redirect and the quads; 0 = the game draws the HUD into the frame |
| `SlotScale` | 0.50 | a sink's texture is the render's size times this |
| `Regions` | 0 | route elements by the screen rectangle of each draw (the probe); 0 = one element, `all` |
| `Element.<name>` | preset | `off` / `frame` (left in the eyes) / `window` / `hand` |
| `Element.<name>.WinX/WinY/WinScale` | 0,0,1 | placement on the window (m, m, factor) |
| `Element.<name>.HandX/HandY/HandScale` | 0,0,1 | placement on the hand panel |
| `Region.<name>` | unset | `x0,y0,x1,y1`, normalised backbuffer, y down; unset = unmeasured |
| `WindowAnchor` | view | `view` head-locked, `world` parked at the last recenter (LOCAL space) |
| `WindowDistance/Width/Height` | 1.30 / 1.25 / 0 | metres; Height 0 = the texture's aspect, else a centred crop |
| `WindowUp/Lateral` | -0.10 / 0 | in the window's plane |
| `HandHand` | 0 | 0 left, 1 right |
| `HandX/Y/Z` | 0 | offset in the grip's own frame |
| `HandLift` | 0.06 | along world up (38.92 lifted along HEAD up; differs only pitched) |
| `HandWidth` | 0.22 | the tuned 38.92 value |
| `HandOrient` | billboard | `billboard` faces the head, never rolls; `grip` = a watch face |
| `HandTilt` | 0 | grip only: degrees of nod toward the eyes |
| `MenuInWindow` | 1 | in-game screens ride the window |
| `WindowPause/Note/Journal/Wheel/Store/MissionStats` | 1 | per-context opt-in (the wheel is what the weapon scroll and the grip-hold loadout open) |
| `[Draws] Census` | 0 | the bucket table and VERDICT every 3 s |

Elements: `all` (every draw without a readable region), `health`, `mana`,
`equipment`, `reticle`, `subtitles`, `prompt`, `objective`, `vignette` (a
draw spanning more than 60 % of both axes), `menu` (the riding screens; never
the hand). Presets: status elements on the hand, text on the window.

Seam words: `hud on|off|status|scale <f>`, `hud regions on|off`, `hud anchor
<el> off|frame|window|hand`, `hud window view|world|recenter|dist|width|height|
up|lateral <v>`, `hud hand left|right|billboard|grip|x|y|z|lift|width|tilt <v>`,
`hud place <el> window|hand <x> <y> [scale]`, `hud region <el> x0,y0,x1,y1`,
`hud menu on|off`, `hud menu <Context> on|off`, `hud reset`, `hud layout`;
`draws on|off|status|regions|kill <key>|hud|unkill`; `dump hud [sink]`.
F10: the HUD tab. status.json: `draws`, `hud` (with `layout`), `hudQuads`,
`uiBlocks`, `uiRides`.

## 3. The gate

The redirect arms when ALL hold: `[Hud] Panel`, the hand-off to D3D11 is up
(sinks at the backbuffer size, the repair pass compiled), the runtime's
presentation MODE is a projection layer (`dvr::hud::projection_mode()`; the
mono screen, a load and the cinematic quad drop it) OR a screen is riding the
window, and the game side's gate: the SCENE verdict (`DvrSceneVerdict`, the
presentation class, which a riding screen keeps true). Game state, never the
draw, and never the per-present eye tag: re-entry leaves 6 to 21 presents a
second untagged by design (`none/s` in the stereo beat), and a gate on the tag
drew the HUD into the frame on each of them, a 10 Hz window/frame flicker on
the first headset run.

The ride (`ui_ride_policy.h`): a blocked UI owner in {Pause, Note, Journal,
Wheel, Store, MissionStats} with its opt-in bit, `MenuInWindow=1`, the menu element on
the window, and the redirect healthy (armed, a redirected draw within 500 ms)
RIDES. Decided once per blocked interval (a health flap mid-menu cannot flip
the picture); only a latched D3D failure drops it (to the mono screen: fail
soft). While riding: the runtime is told the context does NOT force mono, so
the projection stays up and no `[Screen] Anchor*` placement happens; the
scene verdict uses the stand-in `pawn && (raw camera-upload clock fresh ||
tagged projection present within 250 ms)` because the pause silences the view
dispatches and the FSM snapshot expires; the INPUT class (`UiSurfaceBlocks`)
is unchanged, so the head-mouse stays off, the pad keeps its menu shaping and
VR-71's stale-flag guard still sees the owner. The stand-in also covers the
300 ms between a menu flag rising and the owner read publishing (the open
gap) and 1500 ms after the screen closes while the view pipeline is silent
(the resume gap: without it the projection dropped to the screen and came back
on every resume, the "stereo reloading" of headset run 47).

Readers of `UiSurfaceBlocks()` that decide what the headset SHOWS use
`UiSurfaceOwnsPresentation()` instead (`stereo_state.cpp`, `scene_draw.cpp`,
`fov_lever.cpp`); every reader that decides what the PLAYER MAY DO keeps
`UiSurfaceBlocks()`.

## 4. How to read the log

- `draws: hooks installed ...` once; `draws: the draw thread IS the present thread` once.
- `hud: sink 0's target is WxH A8R8G8B8 ...`, `hud: sink 0's hand-off is live ...`.
- `hud/beat: presents=N armed=N redirected=x/present (s0[all]=x) delivered=N
  empty-while-armed=N (even a, odd b) ... -> ARMED`. The line states its
  prediction: while armed, empty=0. empty==armed/2 on one parity = the HUD
  tail lands in ONE re-entry pass; empty==armed = the rule matched nothing.
- `hud/layout: window view 1.25m@1.30m hand L 0.22m | all=window health=hand(no
  region: rides all) ...` and the routed counts per element every 3 s.
- `xr: HUD quad[i] live (<anchor> anchor, element e, WxH crop ..., w x h m, d m
  from the eyes, subtends N deg)` once per slot; `xr: HUD quads n of m
  submitted (hidden: ...)` every 5 s.
- `ui/surface: context=Pause blocked=1 known=1 rides=1 ...` and `ui/ride: Pause
  -> RIDING the HUD window` / `refused - ... (the mono screen takes it)`.
- `stereo/state: STEREO ... standIn=riding|resume grace|open pending rawAge=..
  gateAge=..`.
- `draws/regions: ...` the probe's per-bucket rectangles (`draws on`, `hud regions on`).

## 5. Traps (each one paid for)

- The device is PURE: no `GetViewport`, no `GetRenderState`; every value the
  rule needs is shadowed from its setter, and `SetRenderTarget` resets the
  viewport, so the redirect re-applies the shadowed one.
- A D3D11 event query never completes until the context is FLUSHED; budget the
  waits with QueryPerformanceCounter (a 15 ms tick expires a 10 ms budget).
- Clear the sink AFTER the copy, every present, unconditionally (a lazy clear
  left a dropped body's icon on the wrist for minutes). Deliver the PREVIOUS
  slot.
- Alpha repair is `max(r,g,b)` premultiplied: dark strokes go faint. The fix,
  if the headset dislikes it, is a real alpha capture
  (`D3DRS_SEPARATEALPHABLENDENABLE` forced so the sink's alpha accumulates
  coverage), not a different blend on the quad.
- `DumpTexturePng` swaps R and B (VR-13): read `dump hud` for geometry only.
- The pause menu and the wheel are the same draw class: gate on state.
- `Dis_OpenPauseMenu` ghosts during loads; the ride needs the OWNER read, and
  the open-gap stand-in lasts 300 ms at most.
- A vertex buffer created write-only cannot be read: the region probe refuses
  it (counted, `draws/regions` says `write-only VB`); UP draws read the pointer.
- The runtime accepts 16 layers; the layer array holds 20. The HUD block stops
  at the runtime's cap and counts `hiddenBudget`; the aim visuals budget after it.
- The simulator composites quads with culling off, so a back-facing watch-face
  tilt cannot fail there: the headset judges the tilt sign once.
- Views are located at `predictedDisplayTime + Ahead*period`, hands at
  `predictedDisplayTime` (Ahead default 0); a wrist panel that trails is that.

## 6. Measured on the simulator (2026-09-14/15, this branch)

Build `vr33-hands-working-274-g85f9ef6e-dirty` (the VR-117 tree on VR-Main 85f9ef6e),
`dvr-xrsim` at 90 Hz, 2750x2850, `stereo reentry`, the prison sewer level opened
through the console (`console open L_PrsnSewer_P`; the newest save on this PC is a
death loop at the intro boat and cannot be used).

- The rule holds at the shipped size. In gameplay `hud/beat: armed=421 of 425
  presents, redirected=20.8 draws/present (s0[all]=20.8), delivered=850,
  empty-while-armed=0 (even 0, odd 0)` while `stereo: beat ... L/s=71 R/s=71
  mono/s=0`: every armed present carried HUD draws on BOTH re-entry passes (the
  prediction the line states), none matched nothing.
- The picture: with `hud off` the health and mana bars are in the game window's
  top-left; with `hud on` that corner is bare world, and `dump hud 0` holds the two
  bars alone on a transparent ground (bbox 0,18-691,716 of 1375x1425, alpha up to
  247, 37719 covered pixels; R and B swapped as VR-13 says).
- The runtime: `xr: HUD quads 2 of 2 submitted ... layers 3 of 16` (sink 0 and, on
  this build, the eagerly allocated menu sink; the source now takes that sink only
  while a menu rides).
- The anchors (`hud-quads.xrs`): with the whole HUD on the window the shot holds a
  quad in `view` space at (0, -0.10, -1.30), 1.25 x 1.30 m; with `hud anchor all
  hand` and the sim's left hand posed at (-0.20, 1.10, -0.40) the quad is in
  `local` space at (-0.20, 1.16, -0.40), 0.22 x 0.23 m: the grip plus the 0.06 m
  lift, at 38.92's width. `hand l valid off` removed that quad on the next presents
  (no stale quad); the sim's syntax is `valid off|on`, not 0/1.
- The first pause fell to the MONO screen, and the log said why in one line each:
  `stereo/state: STEREO ... standIn=open pending` at the menu flag, `FALLBACK
  standIn=none` 46 ms later, `xr: cinematic quad ON` 32 ms after that, then
  `ui/ride: Pause refused - healthy=0`. The redirect's health check required the
  per-present `armed` flag, which drops on any untagged present (the beat's
  `none/s=1`), and a health blink cancelled the open-gap stand-in before the owner
  read published. Fixed twice: health is the recent-redirect window alone, and the
  stand-in holds its 300 ms once started. Re-measured below.
- Re-measured after those two fixes: `ui/ride: Pause -> RIDING the HUD window`,
  `stereo/state: STEREO ... standIn=riding rawAge=0 gateAge=15`, the menu element
  took sink 1 on its first draw (`hud: sink 1's hand-off is live`), `xr: HUD quad[1]
  live (window anchor, element 9 ...)` and the menu measured 94.9 draws/present on
  that sink (PR #12 measured 95.9): the pause menu reaches the window with the
  projection up. But the window BLINKED: paused, the re-entry gates flip between
  SINGLE and DOUBLE draw (`camera silent (no c5 upload since the previous draw)`)
  and about every other present hands the runtime no texture, which re-submits the
  held projection ALONE - the HUD block ran before that hold path. Moved after it,
  so a held present carries the HUD quads on top of the held projection.
- The pause, measured on the final build (`pause-ride.xrs`, build 275-g52e2414d-dirty):
  Escape -> `standIn=open pending` on the menu flag, `ui/ride: Pause -> RIDING` 62 ms
  later, `standIn=riding`; through the pause `stereo: beat out/s=91 L/s=46 R/s=45
  mono/s=0 none/s=45` (a live stereo pair at 45 Hz with the other 45 presents
  untagged and HELD; never the mono quad), `hud/beat: redirected=94.9/present
  (s1[menu]=94.9) empty-while-armed=0`, the shot holds `projection, quad, quad`;
  held 10 s with no `menu: stale flag cleared` (VR-71's guard holds); resume via
  `OnResumeGameClicked` keeps `projectionViews 2` with both eyes fresh (`eyeAgeL/R 0`).
  The sim's `projStaleSubmits` climbs during a ride (634 over 12 s): every held
  present re-submits the previous projection, which the sim counts as stale. The
  mono-screen pause hid the same holds behind its quad; the sequence no longer
  asserts that counter across a pause.
- The world-locked window: `hud window world; hud window recenter` parks the quad
  in LOCAL space at (0.00, 1.50, -1.30) (the sim's head at 1.60 m, 1.30 m ahead,
  0.10 m down); after `head rot 0 30 0` the shot still carries that LOCAL pose.
- Cost: `perf: tick` 12.9 ms (76.0/s) with `hud off`, 13.2 ms (74.3/s) with `hud on`
  at 2750x2850 on the simulator: about 0.3 ms per tick for one sink.
- The region probe: 0.6 us per probe (about 9000 probes per 3 s in gameplay, 20 per
  present), 19 % refused; in the pause menu 0.8 us per probe over 37000 probes per
  3 s. With `[Hud] Regions=1` the routing already splits the gameplay HUD into
  `all` (8.6 draws/present) and `vignette` (12.3/present: draws wider AND taller
  than 60 % of the screen), before any region is named.
- A focus loss (`OnLostFocusPause`, from the harness foregrounding another window)
  made the game present from its game thread while the render thread was parked:
  the first build's classifier latched a permanent refusal on that hand-off and
  switched the redirect off. It now logs the topology change with both thread ids
  and keeps running (`status.json` `draws.threadMismatchPresents`).
- The census at 2750x2850 (`draws on`, final build): 1239 draws/present, the
  BACKBUFFER population 5 buckets / 22.0 draws per present in gameplay (4 HUD
  candidates, 21.0/present, the resolve 1.0/present at ordinal 1218 of 1239) and 11
  buckets / 36.4 in the pause menu (10 candidates, 35.4/present); the VERDICT names
  `vdecl` as the separator with no overlap. Every HUD candidate is a
  DrawIndexedPrimitiveUP with a SHORT2 position (16-bit integer pairs, the
  vertices' own ranges around -70..23000: Scaleform's shape space), plus a few
  UP draws with FLOAT2 positions in the menu.
- The region probe's transform HYPOTHESIS IS WRONG for this build: the vertex
  shader's rows c0 and c1 read (0.000 0.002 0.999 1.000) and (0.726 0 0 0) for every
  HUD bucket and every present, and multiplying the shape coordinates by them gives
  rectangles like [-127,-903670 - 139,907972]: not a screen position. Whatever
  carries the 2D transform in this GFx build is not c0/c1 as a 2x4 matrix. So
  `[Hud] Regions` stays OFF, per-element routing is NOT delivered by this branch,
  and the next step (VR-118) is to disassemble the HUD vertex shader (pixel shader
  d50ac0b5's partner; the census can hash and dump it like the pixel shader) and
  find which constant registers, or which vertex components, carry the transform.
  The probe itself is ready: 0.5 us per draw, the vertices read correctly.
- The first HEADSET run (Quest 3 through VirtualDesktopXR, 90 Hz, 2026-09-15): the
  window, the hand panel, the pause and a note all judged good. Two reports, both
  answered by its log: (1) the HUD flickered between the window and the frame in
  gameplay: `hud/beat` read `presents=441 armed=400` and similar in every 3 s window
  while `stereo: beat` read `none/s=6..21`; the redirect's gate followed the
  per-present eye tag, so every untagged present disarmed it and the next present's
  HUD draws went into the frame. Fixed by gating on the runtime's projection MODE.
  (2) a weapon switch by mouse scroll and the grip-hold loadout dropped the world
  flat: both are the power wheel (`Dis_WheelShortcuts_MouseNext` ->
  `DisGFxMoviePlayerPowerWheel`, `ui/surface: context=Wheel blocked=1 rides=0`, 48
  times), which was excluded from riding and which parked the redirect through
  `g_wheelHeld`. Fixed by letting the wheel ride (`WindowWheel=1`) and dropping the
  wheel term from the gate. Both re-verified on the simulator before the second
  headset run: `hud/beat presents=467 armed=467 empty-while-armed=0` in every 3 s
  window with `stereo: beat none/s=0..1` (before the change the same level read
  `presents=450 armed=433`), `wheel-ride.xrs` 27/27 (`ui/ride: Wheel -> RIDING`,
  the projection and a quad layer through the hold, both eyes fresh on release,
  the mono screen with `hud menu Wheel off`), `pause-ride.xrs` 31/31 again.
- The second HEADSET run (2026-09-15, the release build with the repo default ini):
  no window/frame flicker in gameplay, the weapon scroll and the grip-hold loadout
  stay in the window, the pause, journal, note and the rest judged good. VR-117's
  pass criteria met as far as one run judges them; the alpha repair's faint dark
  strokes drew no complaint.
- The cost: two sinks at SlotScale 0.50 = 7.5 MB StretchRect each per present;
  fences `blit waits 5357 timeouts 0, read waits 0 timeouts 0` over the run.
