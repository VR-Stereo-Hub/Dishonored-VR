# The HUD on its anchors (VR-117, VR-118, VR-119, VR-120)

## VR-129: restore configured HUD and separate wheel side panels (2026-09-16)

Build397 is now reported: verified DLL/hash/banner and unchanged full INI; both
logs preserved at build/playtest-candidates/vr129-native-reference/reported397.
NativeGameplayReference=1 ran for345 logged gameplay samples, with no toggle.
The tester could not find the control and the native layout put HUD artwork at
hard-to-see FOV edges. Objective head-coupled rotation was reported in this native
path too. This weakens a capture-only explanation, but there was no matched A/B
and no measured projection cause. Previous instructions omitted the HUD tab.
Restore reference0; F10 > HUD now displays a conspicuous restore button whenever
the comparison is enabled. Preserve the current native objective options.

The requested D-pad shortcuts and potion controls are separate quads from the
wheel capture. Two small D3D11 textures sample its SAME fenced delayed slot,
before the main circle mask. The existing D3D9 capture and D3D11 read fence cover
all three reads. No new engine object retention or memory writers. Same wheel
visual lease governs visibility and exit; no change to the accepted dial origin,
orientation, circle, closing animation, stereo poses or pair synchronization.

- Optional [Hud] WheelSidePanels=0 by default; installed test arms1.
- F10 > HUD > Weapon wheel side panels (expanded): enable toggle, D-pad shortcuts,
  Health and mana. Independent anchor, Horizontal (m), Vertical (m), Size.
- Element.wheelshortcuts and Element.wheelpotions use standard anchor/WinX/WinY/
  WinScale and HandX/HandY/HandScale persistence. Initial window offsets are
  -0.32/+0.32m horizontally and -0.22m vertically, widths0.23/0.28m.
- General HUD alpha applies, independently of the dial's transparent settings.
- Adjust captured area exposes WheelShortcuts.Crop0..3 and WheelPotions.Crop0..3:
  left UV, right UV, bottom UV, height as fraction of image WIDTH. Defaults
  (0.02,0.29,0.995,0.31) and (0.70,1.00,0.995,0.16) are candidate envelopes.
  They preserve source pixel aspect at wide and square render sizes. These are
  not measured runtime ownership rectangles; visual completeness remains open.

Offline scripts locate shortcuts_mc (sprite166) and potions_mc (sprite180) in
UI_PowerWheel_SF; both position from the bottom safe-area corners at runtime.
Shortcuts scale90%; potions slide in from+150px on opening. Source cropping keeps
runtime-loaded icons, counters and input symbols together. The expanded stage
and safe-area formula mean fixed authored1280x720 letterboxing is insufficient.
All exported assets/scripts remain ignored under build/hud-assets.

Validation:101 controls,912 wrist/crop,44 native,465 route,462 anchor,45 menu host
checks. Actual production shader on D3D11 WARP passes circle feather, hue, left/
right source crops and identity restoration. Default profile byte parity passes.
Final installed identity is recorded in STATUS/installed.json. No headset claim.
ONE launch question: are both complete side panels on the window while the
circular dial remains at its hand-origin position? Missing/clipped parts point
to source bounds/ownership; panels moving with the dial point to placement.


## VR-129: build395 result and native reference comparison (2026-09-16)

Installed397 (`vr33-hands-working-397-gd5f18400e`), clean sourced5f18400e.
Candidate build/playtest-candidates/vr129-native-reference. Release build and9
undecorated exports pass;44 native HUD,97 controls,45 menu host checks and default
profile/golden parity pass. Both395 logs, prior DLL and full INI archived at
build/playtest-candidates/installs/20260916-204922-232416.
Entire INI diff: only Hud.NativeGameplayReference=1 added; all395 values preserved.
Installed DLL/INI hashes and CRLF verified. DLL SHA256
`0561be4a84faf2a7266bc1d3d3e2204be840f808565d453e8fa61055672b8826`.
Current log remains395 until user launch;397 headset comparison pending. No game
or simulator launched, no push/PR/merge. Reference is default off in the repo.

Verified395 DLL/hash/log banner; both logs and full unchanged INI archived at
build/playtest-candidates/vr129-hud-fixes/reported395. Main reported objective
failure is movement/swivel during left-right head turning; head roll was not
specifically tested. The preceding roll correction addressed a different motion.
102 of104 rate-limited upright samples accepted the current rendered basis;
that proves execution for sampled recognized draws, not correct target tracking.
Title/distance still change planes. Current objective-row position/scale settings
are ineffective under NativeObjectiveIcons=1 because that path bypasses the panel.

Offline UI_HUD_SF identifies objectiveMarker_primary (sprite178) and
objectiveMarker_secondary (174). Both own _description_mc (160, depth1) and
_icon_mc (177/173, depth4). Description text lives in _description_mc.txt.
Description is drawn before the icon. Our current/prior-present proximity matcher
can lose that association during movement; these D3D draws do not carry the movie
instance names. No semantic runtime identity hook or target-projection fix is yet
established. Do not hard-code run-specific draw keys or widen proximity blindly.

Candidate NativeGameplayReference (default off, live F10 > Objectives) leaves all
normal gameplay HUD draws in the game image, bypassing capture, alpha, size and
upright transforms, and suppresses delayed HUD panels. Menus and wheel closing
visual ownership supersede it. Successful forwarded HUD draws supply a separate
500ms entry-health heartbeat; failed draws, device failures, missing handoff and
expired samples refuse. This avoids interpreting intentional lack of capture as
an unhealthy menu redirect. Stereo/image orientation and hand correction unchanged.
Toggling invalidates delayed panels but preserves learned marker content. Beat
logs explicitly expect empty capture during the reference. The active native size
slider is now under Objectives, and inactive objective panel controls are hidden.

ONE next launch question: does enabling the native gameplay reference stop the
objective icon/title/distance from shifting away from the target during left-right
head turns, compared with reference OFF at the same location? ON stable and OFF
unstable supports our capture/grouping/transform path; both unstable points toward
native target projection versus the VR-rendered camera. Text-only improvement
isolates an additional grouping failure. This is a diagnostic, not a claimed fix.
Keep normal-size differences out of the tracking verdict. Restore reference OFF
for ordinary play. No game/simulator or subagents were used.

Offline preview correction: wheel imports ../common_assets/lib.swf, whose cooked
movie is Startup.lib. itemIcons is sprite301; its133 frames are animation/layout,
not a populated wheel. EquipmentIcon.SetIconImage calls req_EquipmentIconImage;
15 ic_item_*/ic_pow_* textures in Startup supply the actual artwork. Exported54
shared-library textures plus15 runtime icons; the revised export helper resolves
the library import and exports its scripts. Static FFDec frames still do not run
engine callbacks or inventory population. Large journal illustration packages are
not the wheel icon source. All assets, scripts and previews remain ignored local
build/hud-assets; runtime-icon-sheet.jpg is a verified contact sheet, not a game
screenshot. wheel_mc, shortcuts_mc and potions_mc remain separate components.

Validation:44 native HUD production-scope/policy/health checks,97 HUD controls,
45 menu lifecycle checks and default profile/golden parity pass. Development
release builds. Final clean candidate identity belongs in installed.json and the
current STATUS entry; actual head-yaw result remains pending.

The game's Scaleform HUD, taken out of the eye textures and shown on quads the
runtime layer composites: a head-locked WINDOW, a WORLD-parked window, and the
LEFT and RIGHT hands (what build 38.92 shipped as the wrist HUD), per ELEMENT
(VR-120: a table of rows, each claimed by a screen rectangle or a screen's UI
owner context, an unnamed element riding `default`), with every placement value
in the ini and on the F10 HUD tab, and the quads' alpha derived by a chosen mode
(VR-119). In-game screens (pause, journal, note, wheel, store, mission stats)
ride their own row's anchor with the world in stereo behind them.

Tickets VR-117 (the redo), VR-118 (the transform), VR-119 (the alpha), VR-120
(the elements). Supersedes the abandoned PR #12 (VR-8) and VR-38. The measured
facts it rests on are in ENGINE_NOTES, "The HUD sections carried from the
abandoned PR #12 branch": the whole HUD is painted onto the BACKBUFFER at the
tail of the frame while the world goes to an offscreen scene target, so the
render target alone separates the two; the scene resolve is the one opaque
full-frame draw and alpha blending excludes it; the pause menu and the power
wheel are the same draw class; the paused world is a live stereo pair.

To measure and name one more element (the eight unmeasured rows, or a new one):
`HUD_ELEMENTS_HOWTO.md`.

## VR-129: orientation, wheel close and actual HUD assets (2026-09-16)

Installed395 (`vr33-hands-working-395-g6ccc5b079`), clean source6ccc5b079.
Candidate build/playtest-candidates/vr129-hud-fixes. Both logs, prior DLL and full
INI archived at build/playtest-candidates/installs/20260916-194857-316504.
Full INI diff: only NativeObjectiveUpright=1 and WheelCloseAnimation=1 added.
All existing values preserved; installed DLL/INI hashes and CRLF verified.
DLL SHA256 `17a2f4b476f3de0496ee7f378e38f2e7e76fefe442cb8ab2b43b5065067bfc1b`.
Release build,9 exports,32 native HUD/45 menu host checks, default profile parity,
lint and full diff checks pass. Offline asset export helper executed successfully.
No game/simulator launch. Current log remains391;395 headset verification pending.
Local branch only; no push/PR/merge. First test is objective upright during head tilt.

Installed391 verified/archived at vr129-hud-fixes/reported391 with unchanged INI.
Objective icon tracking was accepted, but artwork rotates with the head. The current
trial interprets this as head roll; gaze-position drift would require separate work.
Default-off NativeObjectiveUpright (F10 HUD grouping) transforms native icon/text
about the shared marker pivot, preserving center/depth and accepted size. World-up
comes from the current rendered perspective matrix at the camera-position upload,
with finite, unit-forward, orthogonality and near-vertical guards. Thread-local and
current-present validity prevents borrowing a newer live HMD orientation. Refusal
leaves accepted native sizing unchanged. Symmetric projection is a prerequisite;
this reuses the hand reader's documented basis validation, not the old yaw solver.
The old diagnostic yaw solver reports refusal in391 and is not evidence for this
new upright result. Logs explicitly report renderedBasis and correction angle.

Brief wheel closing flash remains reported, superseding provisional acceptance.
Default-off WheelCloseAnimation (F10 menu immersion) retains visual ownership for
at least250ms after input release and three presents after the closing flag stops.
Native closing can extend the interval. New menus/loading supersede immediately.
This uses exported animation timing, not an arbitrary frame count; at high FPS the
old three-present tail can end before the animation. It can keep gameplay HUD on
the wheel crop during that short tail; headset confirmation is required.

Offline inspection succeeded: UE Viewer exported UI_PowerWheel_SF (56 objects) and
UI_HUD_SF (12 objects); FFDec exported85 wheel scripts and410 HUD scripts plus
frame previews and XML. All outputs/tool binaries are ignored local build assets.
The first preview had red missing-texture placeholders; copying exported TGAs next
to the GFX resolved the external image references. Authored frames do not run the
scripts that populate weapon items, localize labels or reposition for safe area.

Measured native structure:1280x720 movie; wheel_mc is centered around640,354 in
authoring coordinates; shortcuts_mc is the lower-left component, potions_mc the
lower-right. Separate PC variants exist. Runtime scripts reposition the components
using safe-area/movie-space conversion (PC factor.9, console.85); authored bounds
must not become fixed live capture rectangles. Potions expose health and mana;
shortcut component has up/down/left/right slots and assignment logic. Wheel,
background, D-pad and potion close fades are250ms. Standalone quick-shortcut behavior
has additional delays, so it must not inherit a blanket wheel-close rule.

Repeatable tool: tools/hud-assets-export.ps1, using official portable UE Viewer
and FFDec. Official references: [UE Viewer export](https://www.gildor.org/projects/umodel/faq),
[UE Explorer scope](https://github.com/UE-Explorer/UE-Explorer),
[FFDec](https://www.free-decompiler.com/flash/). UE Explorer remains useful for
package/script declarations; actual movie layout is available through GFX export.
No package modified or extracted game data committed. Next auxiliary-panel work
should use these component identities and runtime placement, preserving native
potion and shortcut input; no new auxiliary panel behavior is included yet.

Validation:32 native HUD checks (basis, preserved center/depth, constant restore,
invalid perspective refusal);45 menu checks (250ms independent of FPS, native
closing, delayed presents, immediate new-menu replacement). Clean candidate installed; headset pending.

## Current:389 feedback and menu exit handoff (2026-09-16)

Installed391 (`vr33-hands-working-391-g56b422dc2`), clean source56b422dc2.
Candidate `build/playtest-candidates/hud-menu-exit`; both logs, previous DLL and
full INI archived in `build/playtest-candidates/installs/20260916-155406-323310`.
Entire INI diff: only add Hud.MenuExitHeading=1 and NativeObjectiveLabels=1.
All prior saved values retained; installed DLL/INI hashes and CRLF verified.
DLL SHA256 `7a0779f52716a029778db0511122dda7dbf08636e7c241cd00359108396b2126`.
Release build,9 exports,38 menu/23 native HUD checks, default writer/package/golden
byte parity, lint and diff checks pass. No game/simulator launch. Current log is
still389 until tester launch;391 headset acceptance pending. Local commits only.

Build389 (`vr33-hands-working-389-gdc3ffee45`, source dc3ffee45) DLL hash/banner
verified; both logs and unchanged full INI archived under
`build/playtest-candidates/hud-visual-lifecycle/reported-menu-exit`.
Headset report accepts reader grip rotation, wheel origin and native objective
icon; closing flash appears absent. Pause world remains stable. Remaining faults:
objective title/distance separate from icon; hands change apparent size during
pause yaw; head-look menu exits restore the entry direction.

- Default-off live F10 `Hud.MenuExitHeading`: preserve accumulated physical menu
  yaw in the existing ProcessViewRotation writer once. A restored render scope
  retains its native base and entry head rotation. On exit, refresh BuildLiveSet,
  revalidate camera/controller/pawn identity, possession and load generation, and
  compose with a fresh head sample. Refuse stale poses, wrong lane, second eye,
  authored cameras, dead/replaced owners and scopes older than1s. Gameplay pitch
  and roll keep their existing absolute rules. Direct fallback is unchanged;
  headset confirmation is required for the actual transition ordering.
- Default-off live F10 `Hud.NativeObjectiveLabels`: short, horizontally centered
  nearby draws can share a known marker's native route and scaling pivot, including
  text-to-icon spacing. Association expires after one actual present, rejects
  ambiguous markers and is never cached by glyph content. This is proximity,
  not a semantic font/title identity. Nearby unrelated text can be misidentified;
  batched glyphs, draw order or large layout changes can evade the candidate.
- Pause `menu/hand-depth` logs record source/delta scale, actual corrected palm
  clip-W, target depth, view translation, focal norm, animation blend and eye/pose
  identity. Read-only measurements only; hand-size correction remains open.
- Preserve all accepted389 placement, alpha, image-owned orientation, pair policy,
  PauseSceneFreshness and PaletteEyeMenuHalfStep values. New controls remain off
  in repository defaults and are enabled only for the installed trial.

Validation:38 menu checks cover one-shot yaw carry and owner/option/table refusal;
23 native HUD checks include shared-pivot math and exact shader-constant restore.
One launch question is menu-exit direction retention, defined in STATUS. Ordinary
pause turning also collects hand diagnostics. No game/simulator launched.

## Current:387 feedback, rigid readers and visual lifecycle (2026-09-16)

Installed389 (`vr33-hands-working-389-gdc3ffee45`), clean source dc3ffee45.
Candidate `build/playtest-candidates/hud-visual-lifecycle`; both previous logs,
DLL and INI archived in `build/playtest-candidates/installs/20260916-152612-084229`.
Entire INI diff is six added keys only: PauseSceneFreshness=1 and five PauseAlpha
values matching saved general alpha (repair,1,0,1,1). Every existing value retained.
Installed hashes and CRLF verified; release build,9 exports, lint and listed host
checks pass. DLL SHA256 `0488e78c3136e4dbce49015018899b87e26f2d896676a514af1410033198efc8`.
No game/simulator launch. Current log remains387 until tester launch; no headset
acceptance claimed. Local only; no push or merge.

Verified387 DLL/banner and unchanged full INI; logs archived in
`build/playtest-candidates/vr128-menu-half-step/reported-pause-scale`.
Crouch transition grouping accepted, hue correction provisionally accepted.
Wheel hand/weapon flicker improved enough to park; accepted half-step setting kept.
Pause navigation remains accepted. New pause world-scale pulsing is separate;
see newest FLICKER_REFERENCE entry for evidence, trial and launch question.

Implemented candidate, headset pending:
- Readers open at the current upright yaw/offset then store inverse(openingGrip)*
  openingPage. Each frame uses currentGrip*relativePage, so rotation AND offsets
  follow the hand like a held page. No head-driven swivel. Existing widths/right/
  distance and shared reading alpha retained.
- Separate PauseAlpha mode/gain/floor/gamma/mix and F10 section. Initially inherits
  saved general settings; general reset leaves it independent. Native frame anchor
  still uses game alpha, as with other scoped groups.
-387 wheel exit invalidation was insufficient: late draws can arrive after input
  ownership ends. Optional reflected DisGFxMoviePlayerBase.m_bIsClosing now informs
  a VISUAL-only wheel lease. Closing plus three delayed presents retain wheel crop,
  pose and alpha. Input/mono ownership still exits normally. Other menus/loading
  preempt the lease. Closing-flag runtime behavior still requires verification.
-387 later wheel context entries at13059109 through13077640 have no accompanying
  hud/dial input opens (last12863390). Retained visual state could therefore reuse
  the prior opening origin. Actual screen entry now seeds the current tracked hand
  independently of the grip-input edge, with new hud/dial visual-entry diagnostics.
  Grip selection retains its shared opening pose when active.
-387 native marker trial failed: content5fe329e6fc06bdf0 routes objective at edges
  but prompt elsewhere. It also mislabeled stationary vitals-shaped icons. A bounded
  edge-learned content set now retains marker ownership through interaction regions;
  positional grouping cannot steal it. Learning survives menus but clears on resource
  reset and expires after2400 unseen presents. Original transform sizing remains70%
  around native centers, preserving the game's own perspective/edge size variation.
  New/unseen artwork and coincident icons remain heuristic limits; no semantic movie
  identity is claimed. Isolated unlearned icons can remain native without resizing.

Validation:462 anchor checks (initial pose, grip rotation, rigid offset, reopen),
97 production alpha/input checks,16 production native-scope/ownership checks,
33 menu lifecycle/freshness checks,465 routes and2186 dial checks pass. Full default
writer/release/golden remain byte-identical. Closing-lease tests cover delayed polls,
three capture presents and loading/other-menu preemption. No headset result yet.

## Current:385 results and transition/native-icon candidate (2026-09-16)

Installed387 (`vr33-hands-working-387-g998e2ab78`), clean source998e2ab78.
Candidate `build/playtest-candidates/vr128-menu-half-step`; both previous logs,
DLL and INI archived at `build/playtest-candidates/installs/20260916-142216-456210`.
Full INI diff: add PaletteEyeMenuHalfStep=1,NativeObjectiveIcons=1,
NativeObjectiveScale=0.700; change ObjectiveScreenTracking1 to0. All other saved
values retained, CRLF verified. DLL SHA256
`097cd453ecc27c101854340f67b425c15bc84c699a757a6bb18b55e85bf08a5c`.
Clean release build,9 exports, lint and listed host/GPU checks pass. Installed
hashes verified. Game/simulator not launched; current log remains385 until launch.
Headset validation pending. Changes remain local on codex/hud-fixes.

385 banner and installed DLL hash verified. Both logs and full INI archived under
`build/playtest-candidates/vr127-opening-hud/reported-transitions-tint`; INI unchanged.
Pause navigation accepted. Wheel layout and readers accepted except closing flash,
reader opening pitch, transient interaction routing, green prompts/objectives and
left-eye hand/weapon flicker. Pause residual is hands; mono causation is unproven.

Changes prepared, headset validation pending:
- Readers retain opening yaw but discard pitch/roll; hand-following position remains.
- Wheel release keeps a separate visual pose until its context ends. Context changes
  invalidate both capture slots and request a clear before the next draw/copy, so
  old wheel pixels cannot be delivered under the next menu's layout.
- Alpha gamma previously exponentiated RGB channels separately. The shader now
  scales brightness uniformly, preserving RGB ratios and existing alpha/settings.
  Actual production shader on D3D11 WARP:180/210/180 becomes101/117/101 at gamma.25;
  ratio.863 versus input.857 within byte rounding. Old per-channel control fails.
  This removes artificial tint amplification; it does not remove native green art.
- Interaction history now counts actual draw presents, not HUD provider calls.
  Duplicate-key ambiguity expires after two frames instead of permanently poisoning
  an entry.385 arming stayed fully enabled after startup; no stance off-gate evidence.
  Semantic grouping during animation remains a hypothesis until playtested.
- NativeObjectiveIcons bypasses private HUD capture for recognized objectives and
  scales their original draw about its native center (70% width/height), retaining
  native screen-edge placement, source blend/color and depth. Shader constant edits
  restore exactly after the draw, including partial failure; no engine-memory writes.
  POSITIONT/unreadable/aliased constant layouts refuse scaling.

Failed385 objective hypothesis: zero hud/objective lines. Moving square draws in
this log have8 vertices/10 primitives, not the older2-primitive classifier. New
classification uses measured square dimensions and edge clamping plus interaction
exclusion. This is NOT semantic identity: similar icons can match, markers near
interaction text can miss, and an isolated button before its title can be ambiguous.
Native path is explicitly a test. Disable old ObjectiveScreenTracking in candidate.
No promise that every objective has been completely uncaptured yet.

Host checks:458 anchor,465 routing,2186 dial,95 controls,11 native draw-scope checks;
actual GPU circle/hue test and41 palette-eye checks pass. Default writer/release/
golden remain byte-identical. See newest FLICKER_REFERENCE for the single launch
question and measured hand hypothesis. Saved user values retained; new experimental
keys are enabled only in installed candidate, not promoted into repo defaults.

## Current: opening orientation, pause input and crouched grouping (VR-127, 2026-09-16)

**Installed385** (`vr33-hands-working-385-gd6abf293f`), clean source d6abf293f.
Candidate `build/playtest-candidates/vr127-opening-hud`; prior logs/DLL/INI archived
at `build/playtest-candidates/installs/20260916-121620-467921`. Entire INI comparison:
exactly one new key, ObjectiveScreenTracking=1; all saved values retained. DLL SHA256
`d929115f9bfc117d1d5493cefd6c2baafa2a6159b400713fca99217af9201a02`. Hashes/CRLF verified.
Release build,9 exports, lint, byte-identical default writer/golden and listed host
checks pass (also404 pair-policy and17 menu-scope checks). No game/simulator launch;
current log still382 until the tester launches385. Headset result pending.

Build382 DLL/banner verified before interpretation; both logs and newest saved INI
archived at `build/playtest-candidates/vr127-hud-fixes/reported-crouch-pause`.
Reported: standing interaction grouping improved, crouching still separates title
and action; action opacity differs from accepted title. Objectives now separate,
but their travel reaches the small panel boundary too soon. World-anchored pause
is preferred, with poor vertical stick navigation. Wheel/readers should face the
opening camera and then retain that orientation; readers still follow the hand.
Left-eye wheel flicker persists and pause also flickers; pause surface unconfirmed.

### Complete saved defaults

Generated/release/golden profiles are the full saved382 INI, byte-identical CRLF.
Ten changes versus the382 candidate: ReadingAlphaGamma1.020/Mix0.990;
InteractionAlphaGain1.090/Gamma0.250; HeadLookPause1,NoBlurPause1,WindowPause1;
Element.pause=world, pause WinScale1.520; general AlphaGamma1.000.
GroupInteractions/RouteObjectives remain1 because that is the saved tested profile.
The new ObjectiveScreenTracking lever is absent/default OFF in repository defaults;
the candidate opts in with one new key, retaining every saved value.

### Corrections and evidence limits

- Pause context3 now retains continuous deadzone-shaped left-stick axes; right
  stick is neutral and repeat clocks clear. No head-relative rotation is applied.
  The old pulse negative control emits5/120 nonzero samples over a one-second hold.
  Native navigation repeat owns the new stream. Reading/wheel/gameplay policies
  are unchanged. `pad/pause` reports raw and delivered axes.
- OpeningPlane stores a normalized LOCAL-space opening quaternion. Wheel placement
  AND hand-selection axes use it; notes/journal retain it while tracking grip
  position. Their right/depth offsets also use opening axes, so a head turn cannot
  swing the panel around the hand. Close/context change resets reader orientation;
  releasing grip resets dial. No camera writer or stereo tag is changed.
- Interaction grouping previously never adopted successful group ownership into
  StableRoutes. A draw first seen elsewhere could revert after a crouch-sized
  position step. Adopt only unambiguous matching content; retained prompt owners
  reseed the moving neighborhood. The reticle exclusion now recognizes a centered
  two-primitive draw rather than every small icon in its rectangle. The archived
  moving prompt includes8-vertex/10-primitive draws around0.039x0.038, which the
  former size-only exclusion could reject. Grouped buttons and title share the
  same InteractionAlpha sink. This is a code-supported correction, not proof
  every crouched/animated/batched interaction is identified semantically.
- ObjectiveScreenTracking (F10 HUD grouping: Objective markers follow screen)
  separates position across the rendered frustum from accepted icon pixel size.
  Window/world objectives use view-space positions; their window scale changes
  size only. Hand/frame/off policies retain their meaning. Crop bounds follow
  successful D3D9 draws into the SAME delayed slot as the pixels, including empty
  frames, failed copies and reset. Quantized padded crops avoid swapchain churn
  from subpixel extent jitter. At most6 markers; overlapping/ambiguous/overflow
  crops and insufficient layer budget fall back to the complete existing panel.
  Other HUD layers retain reserved budget. The engine's native edge indicators,
  marker visibility and heuristic shape recognition remain; no target world
  position or guessed engine field is introduced. The 103-degree rendered edge
  is still the source boundary, not the physical headset's entire peripheral view.

### Flicker remains a separate investigation

See FLICKER_REFERENCE, newest382 result. Wheel interior samples11/11 contain zero
mono; pause15/54 and note4/52 contain mono. The150ms menu hold expires during some
pause/note gaps. This cannot explain every hand-only symptom and does not justify
forcing double draws or extending hold indefinitely. Existing aggregate hand
totals mix contexts and unassociated weapon candidates. Diagnostic counters now
split by menu context and completed draw-eye bucket; rate-limited mismatch events
include actual completed identity, decision, placement/refusal and weapon-hit/miss.
No new hand correction, eye predictor, FOV, pacing or synchronization experiment.

### Verification and next launch

Production-host checks:456 HUD anchor/capture metadata,464 route ownership,
2186 dial,92 alpha/input,19 eye-decision/deferred-join. Default writer and INI golden
match. Release, exports and lint required before install. No game/simulator launch.
Headset validation is pending; local branch only, no new publication/main merge.

ONE question: while focused on the same interactable, do its title and button
prompt stay together with matching opacity as you crouch and turn your head?
Expected: one consistently styled group. Success supports retained group ownership;
continued separation means content/geometry changes need a native owner boundary
or better measured draw identity. If together but opacity still differs, compare
source coverage rather than increasing global alpha. Flicker telemetry is passive.

## Prior382 candidate: alpha, reading input and HUD grouping (VR-127)

**Installed382** (`vr33-hands-working-382-g048c1e461`), clean local source048c1e461.
Candidate `build/playtest-candidates/vr127-hud-fixes`; prior logs/DLL/INI archived at
`build/playtest-candidates/installs/20260916-032558-653882`. Complete INI comparison:
14 new keys only, no previous values changed. GroupInteractions/RouteObjectives=1
for this test (repository defaults0); new alpha values preserve the prior appearance.
DLL SHA256 `09dbbac42cbcddfde2ea5b460a8496617af1f110fb3cde05c3a46618372dc0fb`.
Hashes/CRLF verified. Release build, exports9/9, lint, default-writer bytes and the
listed host checks pass. No game/simulator launched. Existing log still shows378
until the tester launches382; do not interpret it as a new result. Follow-up branch
remains local/unmerged and headset acceptance is pending.

PR69 merged to VR-Main at0afbadc83; source branch preserved. codex/hud-fixes
starts from that merge. Main contains the complete accepted saved378 F10 profile.
New work stays local; no second main merge is authorized.

### Alpha controls

- **Notes, books and journal alpha** is one shared profile: ReadingAlphaGain/Floor/
  Gamma/Mode/Mix. Both note/book context4 and journal context5 select it.
- **Interactables alpha** uses InteractionAlphaGain/Floor/Gamma/Mode/Mix on the
  private prompt sink. Title/action/icon draws benefit only once correctly grouped.
  A frame anchor remains native game rendering and cannot use the extracted alpha.
- Wheel retains its three saved values and adds explicit WeaponDialAlphaMode/Mix.
- General **Restore original general alpha** (also `hud alpha reset`) restores
  repair, gain1, floor0, gamma1, mix1. Specialized values, modes and mix remain intact.
- D3D9 coverage forcing and the D3D11 compositor select the SAME per-sink profile.
  Leaving capture on the global mode while specializing the compositor was rejected:
  a captured-alpha reading panel would otherwise receive last-draw alpha, not coverage.
- New profiles inherit the accepted general values at migration; existing wheel
  values stay unchanged. Persist all five fields, so a later general reset cannot
  change specialized modes after a restart. Installed INI comparison is mandatory.

### Book scrolling

The previous code ran MenuStep on both axes of every ordinary menu: immediate
pulse,380ms delay,170ms repeats, zero in between. A production-function host negative
control at120 input samples/second emits only5 nonzero samples for a one-second
full-stick hold. Context4/5 vertical input now remains continuous after the existing
pad deadzone; it preserves neutral, magnitude and direction and lets the native
reader own speed/acceleration. Horizontal input remains stepped, right stick neutral,
and wheel/gameplay/other menus retain their prior policies. `pad/reading` reports the
context and delivered vertical value at a bounded rate. Headset speed not yet accepted.

### Grouping candidates and explicit limits

`GroupInteractions=0` and `RouteObjectives=0` ship OFF, with live F10 HUD grouping
checkboxes. The next candidate enables both for testing, preserving every previous
installed value. These are targeted heuristic candidates, not a solved semantic hook.

- Interaction grouping joins nearby small draws to the current/previous interaction
  bounds and outranks a stale first-position content hint. Reticle/vitals/full-screen
  exclusions remain. It expires after a draw gap/reset and bounds group growth.
  Neighbor margins0.02/0.025 are trial tolerances; they are not measured object identity.
  The first draw before a new seed is seen can still fall back for one frame, and
  nearby unrelated UI can match. A complete title/action grouping claim needs headset evidence.
- Objective candidate uses the previously measured0.033x0.032 moving quad shape,
  with trial tolerance+/-0.003, two primitives and4..8 vertices, anywhere on screen.
  It receives a private sink and the objective row's own anchor/placement instead
  of default. No fixed screen rectangle or target-specific world position is invented.
  Other same-sized icons may match; batched/edge/scaled markers can fail. This cannot
  establish native task-marker identity. `hud/owner` now includes rect/vertex/primitive
  evidence when group routing disagrees with spatial routing.
- No new engine addresses or writes. Native HUD declarations expose task-marker
  arrays and interaction groups, but their render-object mapping is not established.
  If this candidate fails, use those native owners; do not keep expanding rectangles.

Validation:457 routing checks include marker movement and negative controls,
interaction title/action travel across the original boundary, unrelated text and
expiry/reset.52 control checks compile the production alpha selector and capture-mode
predicate and the old MenuStep function; they verify specialized isolation and reading
context/input behavior. Fresh-default writer/packaged/golden bytes agree.

**One next launch question:** while keeping the same interactable focused and moving
head position/view, do its title and action prompt remain together on one plane?
Expected: stable grouping/size. If separation remains, inspect the logged draws and
continue toward semantic identification; no change in plane does not prove objectives
are correctly identified. Reading/alpha/objective controls are available but are not
additional required verdicts on this launch. Residual hand flicker remains VR-128.

## Accepted dial and saved profile (2026-09-16)

Build378 accepted for merge. Reading is stable and wheel flicker greatly reduced;
remaining left-eye hand/weapon head-motion flicker is VR-128. New saved profile is
promoted byte-for-byte to WriteDefaultIni, release and golden defaults. Latest
changes: vitals HandX0.065/HandY-0.102/HandScale0.270; Note/Journal width0.660,
distance0.020,right0.320. Both logs and full profile are in the candidate's
accepted-profile archive. Existing diagnostics/performance settings retained.

HUD plane routing improved but is incomplete: objectives still fall through to
default, and some interaction title/action draws split. VR-127 on codex/hud-fixes
will own grouped routing, reading/interaction alpha, original general-alpha reset
and slow left-stick book scrolling. The accepted content cache is not semantic
owner identification; do not mark these remaining cases fixed.

## Current: right-hand-side readers and moving HUD ownership (2026-09-16)

**Installed378** (`vr33-hands-working-378-g9bce8a13b`), clean source9bce8a13b.
Candidate `build/playtest-candidates/vr126-hud-owner`; preinstall logs/DLL/INI in
`build/playtest-candidates/installs/20260916-023823-085684`. Whole INI comparison:
exactly two new keys, NoteHandRight/JournalHandRight=0.200; every existing value
retained. DLL SHA256 `9b6135b6e14b90a9e54672d7e6ae49ad9c18eba275d726b1027a457501123e1b`.
Installed hashes/CRLF verified. Release build, exports, lint, golden INI and regression
checks pass. No game/simulator launched; current old log remains376 until the tester
launches378. Headset result pending. Local commits only.

376 reader placement accepted except horizontal alignment; camera sliding appears
fixed. Preserve saved Note width0.660m/distance+0.040m and every other F10 value.
New `NoteHandRight` and `JournalHandRight` default+0.200m, range-0.75..+0.75m.
F10 **Notes and journal on the hand > Horizontal offset (m, + right)** moves along
the camera-facing panel's right axis. Width/distance remain independent, panel
follows the left grip and stays parallel to the camera.

**Plane switching is understandable from source:** gameplay HUD routing selects a
row by draw center inside screen regions. A moving draw can become reticle/prompt/
default and inherit a different anchor/scale. Existing saved Default scale1.570
versus Prompt1.170 makes such a switch visible. In addition, measured elements on
one anchor share a crop texture; overlapping configured regions can display pixels
owned by a different row. This is not physical quad collision.

**Candidate:** initial location still seeds a row, but a bounded cache retains it
while identical local vertex content/material resources move via shader transforms.
Includes default ownership, so an unknown moving draw cannot suddenly become a
prompt simply by crossing that rectangle. Hash all bytes for eligible small draws
(max8KiB), no extra GPU read or buffer lock. Reset on menu/config/device changes;
expire after240 provider frames of absence. Duplicate content at different positions
in one frame is ambiguous and falls back, never intentionally aliases another owner.
Each measured element gets a private texture; expand around its reference region
without changing pixel scale/location, so motion outside that region is not clipped
and overlapping regions cannot see each other's pixels. Keep the12-sink ceiling and
existing overflow fallback. Unmeasured elements still share the default catch-all.

**Limits:** this is content continuity, not a named Scaleform object hook. Rebuilt,
large, animated/colour-changing or unreadable geometry may fall back to positional
routing; an initially wrong positional hint can remain wrong until expiry/reset.
Do not claim every interaction prompt is semantically identified. `hud/owner` logs
retained-vs-spatial disagreements. Headset test still required; if transitions remain,
inspect those draws and pursue semantic owner identification rather than widening
rectangles. Private textures may add copies when multiple measured elements previously
shared one anchor; do not promote a performance claim from host tests.

Validation:279 production route checks (moving/default/expiry/reset/collision/ambiguity),
38 anchor checks including expanded reference geometry. Menu world-scale investigation,
correction, host evidence and single launch question: [FLICKER_REFERENCE](FLICKER_REFERENCE.md#menu-depth-interruption-follow-up-vr-126-2026-09-16).
Local only. Installed manifest remains the build authority; preserve both logs and
entire saved INI on install. Acceptance pending.

## Current follow-up: motion stability, reading panels and alpha (2026-09-16)

374 wheel usability/appearance accepted; hands/weapons flicker and separate camera translation
are pending. Full camera findings, failed hypotheses and next launch question are
in [FLICKER_REFERENCE](FLICKER_REFERENCE.md#menu-head-motion-follow-up-vr-126-2026-09-16).
All work stays local on codex/hud-weapon-dial by explicit instruction.

Saved F10 profile archived with both logs under
build/playtest-candidates/vr126-dial-immersion/reported-head-motion.
Eight changed keys versus the prior candidate: width0.300m,travel0.080m,crop0.420x0.420,
distance+0.050m,Note NoBlur1,Journal HeadLook1/NoBlur1. Other values preserved.
The three current alpha values are gain3.000,floor0.000,gamma0.500.

- Wheel now owns WeaponDialAlphaGain/Floor/Gamma. Existing AlphaGain/Floor/Gamma
  control other HUD content. Mode and mix policy stay shared. Alpha selection is
  scoped to Wheel's active catch-all sink, independent of whether the dial placement
  toggle is on. Initial wheel values copy the saved profile, so appearance is retained.
- NoteFollowHand and JournalFollowHand use camera-parallel panels centered on the
  current left grip, with no grip rotation, wrist tilt/lift or inherited element
  offsets. They FOLLOW the hand rather than latching the opening position. A visible
  row anchor is still required; off/frame remains respected. Normal note/journal
  rectangular content is retained, without the wheel's circular mask/crop.
- Each reader has HandWidth and HandDistance keys (prefix Note or Journal), and F10
  controls under Notes and journal on the hand. Initial widths0.60/0.70m are adjustable
  starting choices, not measured ideal sizes. Distance-0.05m places the panel5cm
  toward the camera from the hand to reduce overlap. Follow controls shipoff and are
  enabled in the installed test. Existing wrist values stay saved for normal HUD.
- Dedicated placement controls replace misleading generic x/y/scale controls for
  these enabled panels and the dial. Hand-tracking loss hides the panel; existing
  near/behind-face guards remain. Selection and game menu navigation are unchanged.

33 host anchor checks cover5cm clearance, exact hand translation and zero-distance
centering. Existing route/dial checks pass; no rendered/headset claim for new readers.
User launches. Next launch focuses on hand/weapon stability during Wheel head motion; do not infer note,
journal or cinematic acceptance from that one result.

### Installed follow-up candidate

Installed build376 (`vr33-hands-working-376-g35a50573b`), clean source35a50573b.
Candidate: build/playtest-candidates/vr126-menu-motion.
DLL SHA256:32411cc1479702892e09aeb31689d2ad2aa75ce675cf378a3e51a1d4fd386f75.
Both prior logs, DLL and INI archived in
build/playtest-candidates/installs/20260916-015200-720588.
Complete INI comparison: nine new keys only; EVERY previous setting retained.
Wheel gain3/floor0/gamma0.5; Note/Journal follow enabled, distance-0.05m,
width0.60/0.70m. Installed hashes and CRLF verified. Release, nine exports,
INI golden and lint pass. No game/simulator launch. Headset verdict pending.

## Current VR-126 refinement: dial comfort and menu immersion (2026-09-16)

Build372 is headset-accepted as a usable hand dial. The tester tuned width to
0.350m, travel to0.040m, and crop to0.400x0.400. DLL hash and log banner match372;
both logs and the exact INI are archived under
build/playtest-candidates/vr126-weapon-dial/accepted-tuning. The log has33 dial
openings and460 final wheel-input samples. These do not measure selection accuracy.

Requested refinements now implemented, pending the next headset test:

- Direction-only mode outputs full analog magnitude after2mm of hand displacement,
  preserving the angle. A0.5..10mm F10 neutral radius controls jitter. Analog travel
  remains available with direction-only off. Neither stick's radial shaping changes.
- Camera-plane orientation uses the head quaternion, independent of opening hand
  location. Input uses that same right/up plane. Moving the hand lower or sideways
  no longer aims the panel's normal toward the eye position.
- Circular crop runs in the existing HUD alpha shader only on the wheel-owned sink.
  Both RGB and alpha are feathered to zero outside a pixel-aspect-correct inscribed
  circle. The tuned crop still determines its bounds; other HUD sinks are unchanged.
- F10 distance offset ranges -0.30m (closer) to+0.50m (farther), initial0. It moves
  the quad along the opening camera's forward axis. The physical hand's starting
  point stays the input origin, so changing depth does not require reaching the panel.
- F10 HUD / Menu immersion exposes independent HeadLook and NoBlur toggles for
  Pause, Note, Journal, Wheel, Store and MissionStats. New immersion controls are
  default off in repo; the installed test enables head look for Wheel/Note and
  blur suppression for Wheel only. Saved user dimensions and all other settings stay.

### Frozen world view: cause and scoped fix

UiSurfaceBlocks intentionally parks the normal script/direct camera writers even
when a menu rides a stereo HUD quad. ENGINE_NOTES' measured paused-menu rendering
already established that scene draws and camera uploads can continue with a fixed
camera. Therefore seeing an old FOV boundary while turning does not prove GPU
rendering stopped. This is a world-camera/menu issue, not the residual hand flicker.

MenuHeadBegin applies head rotation relative to the menu-entry sample to the current
camera cache only across both viewport draws. MenuHeadPublish tags both eyes with
the exact sample used, preserving image-owned orientation. MenuHeadEnd restores the
incoming rotation, location and writer provenance. Existing main-menu, cinematic,
loading, identity and stereo guards remain. Gameplay remains paused and its input
remains blocked. Head translation preserves the menu-entry offset plus subsequent
raw physical translation, avoiding a changing gameplay neck cancellation on a
camera whose animation is paused. Temporary render gaps hold the reference; a new
UI context epoch refreshes live identities even when pointers are unchanged.

### Gray blur: targeted hypothesis, not yet visually verified

The named movie flag m_bBlurGameWhileActive is not sufficient evidence for the wheel:
its class does not opt into that flag in the local declarations. Native registration
search found no callable UI blur toggle. DisPostProcessManager does expose a dedicated
m_UIPPWeight separate from Kismet and other effects. The candidate reflects
Actor.WorldInfo -> WorldInfo.Game -> DishonoredGameInfo.m_pPpManager and that weight.
On the game/draw lane, an opted-in riding menu temporarily zeros only the UI blend.
It records the latest nonzero game value and restores only its own exact zero when
ownership ends. Every writer checks IsLiveObject/current slot identity and current
owner chain; entry and context/owner changes refresh BuildLiveSet. Dead/replaced
owners are never restored. Reflection failure logs and leaves the native effect.
A successful write is not proof that this field reaches the visible effect. If gray
blur remains, use menu/blur observed weight/writes plus head-scope logs; do not broaden
to global DOF, motion blur or unrelated post-process switches without evidence.

### Installed refinement candidate

Build374 (`vr33-hands-working-374-ga51e1799f`), clean source a51e1799f;
build/playtest-candidates/vr126-dial-immersion. DLL SHA256
`5ed4b0c9a840d14aae28304cff1336d791f2035d749dd7454c8638b2d3ee977a`.
Both logs/previous DLL/INI archived in
build/playtest-candidates/installs/20260916-010814-146048. Full INI comparison adds
only16 new controls, preserving every old value and CRLF. HeadLookWheel/Note=1,
NoBlurWheel=1,direction-only/circle=1,neutral2mm,distance0. No launch performed.

### Validation and next launch

2185 dial math/input checks; native D3D11 WARP test of the actual HUD shader
(57312 transparent pixels,394 feather pixels,7830 solid for a256-square circle test;
unmasked control65536 solid);16 production menu lifecycle checks; existing cinematic
math/scope/FOV checks,30 HUD anchor and20 route checks pass. Scope tests cover dead
identity, wrong thread, external rewrites, exact restoration and nonfinite custom
translation. No game or simulator launched. GPU tests create no visible window.

One launch question: with the wheel held open, does turning the head reveal fresh
world scenery beyond the old FOV rectangle, then return normally on release?
Expected: the circular camera-parallel dial stays parked, small hand movements select,
and world head look continues without unpausing gameplay. Wheel blur suppression is
armed; Note head look is armed for normal use, but this launch's question is Wheel.
If the old rectangle remains, inspect menu/head scope/base/out/gen and fresh eye
counts to separate refused camera ownership from stale rendering. If the world moves
but the gray blur persists, head look succeeded and the UI-weight hypothesis needs
more work. If there is a close/resume jump, inspect restore/refused and context epoch
before changing synchronization. User launches; agent reads and archives both logs.

## VR-126: world-space weapon dial (2026-09-16)

Branch `codex/hud-weapon-dial` starts at VR-Main52107a094 after the accepted
combined build369. PR67 (crouch camera) and PR68 (performance) are merged;
source/release/test trees were checked against accepted merge6c3ef07b4.

**Input finding:** UpdateVirtualPad called MenuStep independently on LX/LY
whenever UiSurfaceBlocks or g_menuOpen was set. MenuStep quantizes each axis
to +/-32000 pulses, with separate380/170ms repeat timers, and zeros RX/RY.
Wheel is a blocked UI owner even when it rides a stereo HUD quad. Consequently
continuous radial directions were destroyed by the mod. PadStick also applied
an independent per-axis deadzone, distorting angles. This is a code-confirmed
fault matching the reported cardinal bias; game-side wedge reachability still
needs the headset. The old pad/rs diagnostic was BEFORE final menu shaping and
could not prove what the game received.

**Implementation:** a published Wheel-context bit exempts the wheel from list
stepping. Either stick uses an angle-preserving radial deadzone and delivers
continuous LX/LY, with the stronger stick taking priority. RX/RY stay zero to
avoid two competing navigation streams. Final pad/wheel telemetry names the
source, both raw sticks and delivered axes. Other menus retain step repeats.

With `[Hud] WeaponDial=1`, hold left grip to seed the quad center at the left
GRIP position in XR LOCAL space. Subsequent hand position does not move that
center. Both selection and rendering use a world-up billboard facing the head;
hand displacement projected on its current right/up axes supplies LX/LY.
Depth displacement does not select.15mm neutral radius;120mm full input by
default. Tracking loss neutralizes hand selection and requires release/reopen
before re-seeding. Pose reads use existing APIs; no engine memory writes added.
The new descriptor changes only the wheel quad, not scene/eye synchronization.

The wheel ring was previously measured at [0.226,0.275 -0.774,0.716] in
ENGINE_NOTES, How the Scaleform HUD identifies its elements. Initial crop is
[0.20,0.25 -0.80,0.75], with margins and original pixel aspect preserved.
This excludes the measured bottom-corner widgets and right-edge labels; their
independent display/interaction is deferred. Cropping only changes the submitted
quad's source rectangle, not draw classification. Bounds from the sewer are
not proof that every inventory/aspect fits; F10 exposes crop width/height.

F10 HUD / Weapon dial has an enable checkbox, width0.15..1.20m, hand travel
0.04..0.30m, crop width/height0.30..1.00. Initial cropped width0.42m is independent
of HandL.Width, its grip tilt, lift, element offsets, and scale. Keep the wheel's
existing handL routing enabled. New placement is default OFF in the repository,
ON in the installed candidate pending headset acceptance; continuous wheel stick
input is corrected regardless. Existing saved settings are preserved.

**Validation:**1815 production-math checks cover360 directions at two stick
magnitudes,360 hand directions, fixed center, depth motion, tilted panel, release,
tracking loss/recovery, nonfinite input, menu gating and crop bounds. Existing
HUD anchor30 and route20 checks pass; release build,9 exports, golden and lint
pass. No game or simulator launched. Headset result remains pending.

**Installed candidate:**372 (`vr33-hands-working-372-gab770282c`), clean source
ab770282c, build/playtest-candidates/vr126-weapon-dial. DLL SHA256
`eb643ede5405b0718b372720c90d44573a0ef5bede7b51aeeff86d5ce89629bc`.
Previous DLL/INI and both logs: build/playtest-candidates/installs/20260916-003048-979346.
Complete INI diff adds only WeaponDial=1, Width0.420, Radius0.120, CropX0.600,
CropY0.500. Installed hashes and CRLF verified. No launch performed.

**One launch question:** Can the left hand smoothly select every weapon wedge,
especially7 o'clock, while the enlarged cropped wheel stays at its opening
position? Hold left grip with the hand comfortably forward, leave both sticks
neutral, and slowly draw a small circle roughly12cm from the opening center.
Release on the lower-left wedge. Expected: continuous highlight, fixed center,
camera-facing wheel and selected item equipped. Success accepts the gesture;
missing wedges despite varied final logged axes points downstream to the game;
wrong/zero axes points to input gating or geometry. A clipped wheel points to
crop bounds, not input quantization. Agent reads logs after the report.

## 1. The pieces

```
game draws -> core/gfx/hud_class   the rule (rt0=backbuffer, full viewport, depth off, blend on),
                                   the census (`draws`), the region probe ([Hud] Regions: each
                                   draw's rectangle through the vertex shader's own transform,
                                   VR-118), the forced coverage equation (VR-119)
           -> core/gfx/hud_route   PURE: (context, rectangle) -> the element row (host tested)
           -> core/gfx/hud_capture N sinks, one per (anchor, crop|all): a private A8R8G8B8
                                   target each, two shared slots, the D3D11 alpha pass
                                   (repair | captured | mix, VR-119), an R8G8B8A8 texture
           -> core/gfx/hud_layout  the element TABLE, anchors, placement, the alpha: the ONE
                                   owner of [Hud]; the provider that describes the quads (one
                                   per cropped element, one per catch-all sink)
           -> openxr_runtime.cpp   "41.x (Dishonored, VR-117) HUD anchors": quad layers with a
                                   stable slot each, crop-sized swapchains, VIEW / LOCAL space
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
| `Regions` | 1 | route by each draw's rectangle (the probe, through the shader's transform); 0 = every draw rides `default` as one quad (the A/B) |
| `Element.<name>` | window | `off` / `frame` (left in the eyes) / `window` / `world` / `handL` / `handR` |
| `Element.<name>.WinX/WinY/WinScale` | 0,0,1 | placement on the window or the world window (m, m, factor) |
| `Element.<name>.HandX/HandY/HandScale` | 0,0,1 | placement on either hand panel |
| `Region.<name>` | measured for vitals, reticle, prompt | `x0,y0,x1,y1`, normalised backbuffer, y down: claims a draw whose centre lies inside; unset = unmeasured (rides `default`) |
| `WindowDistance/Width/Height` | 1.30 / 1.25 / 0 | metres, shared by `window` and `world`; Height 0 = the texture's aspect, else a centred crop |
| `WindowUp/Lateral` | -0.10 / 0 | in the window's plane |
| `HandL.X/Y/Z`, `HandR.X/Y/Z` | 0 | offset in that grip's own frame |
| `HandL.Lift`, `HandR.Lift` | 0.06 | along world up (38.92 lifted along HEAD up; differs only pitched) |
| `HandL.Width`, `HandR.Width` | 0.22 | the tuned 38.92 value |
| `HandL.Orient`, `HandR.Orient` | billboard | `billboard` faces the head, never rolls; `grip` = a watch face |
| `HandL.Tilt`, `HandR.Tilt` | 0 | grip only: degrees of nod toward the eyes |
| `AlphaMode` | repair | `repair` = max(r,g,b) (41.2); `captured` = the sink's own coverage (the redirect forces the equation); `mix` = the larger, repair scaled by `AlphaMix` |
| `AlphaGain/Floor/Gamma/Mix` | 1 / 0 / 1 / 1 | multiply the alpha; a minimum for any pixel with colour; a colour nudge; the mix weight |
| `Backdrop.window`, `Backdrop.hand` | 0,0,0,0 | `r,g,b,a`: a plate composed UNDER the quads of that anchor kind (a=0 none) |
| `MenuInWindow` | 1 | in-game screens ride their row's anchor |
| `WindowPause/Note/Journal/Wheel/Store/MissionStats` | 1 | per-context opt-in (the wheel is what the weapon scroll and the grip-hold loadout open) |
| `[Draws] Census` | 0 | the bucket table, the VERDICT and the element clusters every 3 s |

VR-117's keys (`Element.all/health/mana/menu`, `WindowAnchor`, `HandHand`,
`Hand*`) are read once, mapped onto the rows and deleted by the next save.

Elements (the rows of `hud_layout.cpp`, in `hud list` order): `default`
(every draw no row claims), `vitals` (the health and mana bars, one row: they
interleave in x), `reticle`, `prompt`, `equipment`, `subtitles`, `objective`,
`toast`, `tutorial`, `detection`, `skipgauge`, `darkvision` (the last eight
UNMEASURED: they ride `default` until `hud region` names them), `vignette` (a
draw wider and taller than 60 %), and the screens `pause`, `note`, `journal`,
`wheel`, `store`, `missionstats` (claimed by their UI owner context while they
ride; set off or frame, a screen takes the mono screen). Sinks are per (anchor,
crop|all): the rows with a region on one anchor share its crop sink and each
gets a quad that is a sub-rectangle of it; rows without a region share the
anchor's catch-all sink and one whole quad, placed by `default` (or by the
riding screen's row). On a hand an element fills the panel's width at the
grip; on the window it keeps its place on the screen. Presets: everything on
the window (the VR-117 picture) until the headset judges the split.

Seam words: `hud on|off|status|scale <f>`, `hud regions on|off`, `hud anchor
<el|all> off|frame|window|world|handL|handR`, `hud window view|world|recenter|
dist|width|height|up|lateral <v>` (`view`/`world` move every window-kind row),
`hud hand l|r billboard|grip|x|y|z|lift|width|tilt <v>`, `hud place <el>
window|hand <x> <y> [scale]`, `hud region <el> x0,y0,x1,y1`, `hud alpha mode
repair|captured|mix`, `hud alpha gain|floor|gamma|mix <f>`, `hud alpha backdrop
window|hand r,g,b,a`, `hud alpha status`, `hud menu on|off`, `hud menu
<Context> on|off`, `hud reset`, `hud layout`, `hud list`; `draws on|off|status|
regions|vsdump|kill <key>|hud|unkill`; `dump hud [sink]` (the colour PNG and
the alpha as grey). F10: the HUD tab. status.json: `draws`, `hud` (with
`layout`: the anchors, `seen`, `sinks`, the alpha), `hudQuads`, `uiBlocks`,
`uiRides`.

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
Wheel, Store, MissionStats} with its opt-in bit, `MenuInWindow=1`, the screen's own
row on a visible anchor (`screen_can_ride`, VR-120), and the redirect healthy (a
redirected draw within 500 ms, or no sink in use yet with the blit compiled)
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

## 7. Measured on the simulator (2026-09-15, `claude/vr-120-hud-elements`)

Debug builds of this branch, `dvr-xrsim` at 90 Hz, 2750x2850, `stereo reentry`, the sewer
level through the console (from the MAIN menu: the same command sent from the title screen
left the game on the loading board for eight minutes, run 1 of this branch).

- **VR-118 answered: the transform is the vertex shader's own `Transform` at c6..c9**, read
  from each shader's disassembly at first sight (ENGINE_NOTES, "How the Scaleform HUD
  identifies its elements"). The fixed-function hypothesis was tested first and died in one
  window: `HUD draws with a vertex shader 8862, without 0; SetTransform calls 0`. With the
  columns applied: `9324 probes, 0 refused, 1.0 us/probe`, every bucket rectangle inside
  [0,1], the health bar's bucket at `[0.007,0.053 - 0.148,0.220]`. The old c0/c1 rows were
  stale constants: `(0.000 0.002 0.999 1.000)` and `(0.726 0 0 0)` on every HUD draw because
  nothing uploads c0..c3 between them.
- **The element census** (`draws/cluster`, per-draw rectangles quantised to 1/40): gameplay
  idle 17 clusters, all inside the vitals block `[-0.009,0.013 - 0.172,0.253]` except the
  reticle dot `[0.497,0.497 - 0.503,0.503]`; walking up to a door: the reticle grows to
  `[0.480,0.481 - 0.520,0.519]`, the interaction prompt appears at
  `[0.524,0.481 - 0.774,0.602]` (a plate, a text run, a rule, two icons: 4 draws/present) and
  an objective marker (0.033 square) sits where its target projects. The wheel adds 30
  draws/present (the ring, the slot icons bottom-left and bottom-right, labels right, a
  full-screen fill); the pause menu and the journal are hundreds of glyph draws and overflowed
  a 256-row cluster table (1372 over), so clusters are not collected while a screen rides
  (its context is its identity). Health and mana interleave in x (fills at centres 0.076 and
  0.098, frames 0.077 and 0.103, one shared background at 0.081): one row, `vitals`.
- Cost with the parse and the clusters: `perf: tick 15.8 ms (62.0/s)` on the Debug build with
  the census, the probe and the redirect all on (the census is a measuring lever and stays
  off in play).
- **The alpha (VR-119)**: the HUD's own colour equation is `src=5 dst=6 op=1` (SRCALPHA /
  INVSRCALPHA, add) with `separateAlpha=1` and alpha `src=2 dst=1` (ONE/ZERO): the game
  writes each draw's source alpha over the sink's, so a black stroke with alpha 1 lands as
  alpha 1 there too, but the colour stays black and `repair` (max of r,g,b) reads it as
  nothing. The forced equation (ONE/INVSRCALPHA add on alpha) accumulates the coverage
  instead. `hud-alpha.xrs` 32/32: the first quad's alpha coverage `quadAlphaPct` 1.18 in
  `repair`, 1.22 in `captured` and `mix`, 100.00 with a half-opaque window backdrop, 1.18
  back at identity (the vitals block is 1.2 % of a 1375x1425 sink). `dump hud 0` in both
  modes, the vitals corner (0..0.2 x 0..0.27) of the alpha PNG: pixels at alpha >= 200 are
  7.96 % of the crop in `captured` against 2.54 % in `repair`, with `repair` spreading the
  bars over the 9..199 band (19.44 % against 14.80 %): the strokes and the bar bodies are
  solid in `captured` and mottled by the bars' own texture in `repair`. Cost: eight
  SetRenderState calls per redirected draw, about 170 per present.
- **The elements (VR-120)**, `hud-elements.xrs` 33/33 on the sewer level: with `Regions=1`
  and the preset the HUD is two quads in VIEW space (the vitals crop 275x385 of the
  1375x1425 sink, 0.25 x 0.35 m at (-0.50, 0.37, -1.30); the reticle crop 82x85, 0.075 x
  0.078 m at (0, -0.10, -1.30)); `hud anchor vitals handL; hud anchor reticle handR` with
  the sim's grips at (-0.20, 1.10, -0.40) and (0.20, 1.10, -0.40) puts two LOCAL quads at
  (-0.20, 1.16, -0.40) 0.22 x 0.31 m and (0.20, 1.16, -0.40) 0.22 x 0.23 m: the grip plus
  the 0.06 m lift, the panel's width; `hand r valid off` removes the reticle's quad alone;
  `hud anchor vitals frame` leaves its 20 draws per present in the eyes (`left in frame
  8820` per 3 s window) with the reticle's quad still up; `hud anchor vitals off` hides
  them with no quad. Before the hand rule the vitals crop came out 0.044 m wide and 0.16 m
  up-left of the grip (its screen offset scaled to the panel): a wrist HUD fills the panel
  at the hand, a window keeps the screen layout. Every sink line in the log names its
  (anchor, crop|all): `s0[window/crop]=21.0` draws per present in gameplay; a riding pause
  takes `sink 1 = window/all, for the riding screen` and releases it on resume.
  `hud-quads.xrs` 35/35 (`hud anchor all <anchor>` moves every row), `hud-panel.xrs` 24/24,
  `pause-ride.xrs` 31/31, `wheel-ride.xrs` 27/27, `hud-alpha.xrs` 32/32 on the same build.
  Host: 20 hud-route, 30 hud-anchor, 107 ui-ride checks. Cost: `perf: tick 13.5 ms
  (73.7/s)` with the probe, the routing and two quads, against 13.2 ms with `hud on` and
  one quad on VR-117 (the census off).
- A trap found on the way: sinks are acquired by the first draw routed to them, and a draw
  is routed only while the redirect is ARMED, which required a sink's hand-off to be
  ready: the first build of the table never armed (`hud/beat: ... (no sink in use)
  ... handoff=0`). The hand-off now counts as ready on the blit alone while no sink is in
  use; the first sink proves the rest or latches the failure as before.
- **The first HEADSET run of this branch** (2026-09-15, Quest 3 through VirtualDesktopXR,
  90 Hz, the Release build 287-g4c3e5aa6 with the repo default ini, log
  `vr120-headset-run1-build287`): the preset (two window quads: the vitals crop 275x385
  subtending 9.9 deg, the reticle 82x85 at 3.3 deg), the pause, a note and the wheel riding
  the window, the vitals on the LEFT hand (`hud/layout: element vitals anchor window ->
  handL`), the `default` row on the left hand, in the frame and off, and the vitals off,
  all judged good; the vitals' left-hand quad read `0.22 x 0.23 m, 0.49 m from the origin,
  subtends 25.5 deg`. Nothing in the log says the alpha mode was changed during the run
  (no `hud/alpha: mode ... (F10 HUD)` line), so `repair` is the only mode the headset has
  judged and VR-119's list is still open. The three caught first-chance exceptions at
  start-up (`EXCEPTION 0xc0000005 ... [d3d9.dll+...] (other)`, right after the reflection
  resolves) are in every log since the VR-Main base 274-g85f9ef6e, three per run, at a
  different offset per build: a guarded probe reading a page edge, not this branch's.
