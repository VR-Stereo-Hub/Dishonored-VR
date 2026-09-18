## Installed441: hand-origin wheel facing (2026-09-17)

Installed vr33-hands-working-441-g3ffbee221. Release,9 exports and2213 wheel
checks pass. Entire INI byte-identical; CRLF and installed hashes verified.
Both logs/prior DLL/INI archived in installs/20260917-213123-337079.
No game launched; headset opening-facing test below remains pending.

## Wheel opening faces eye position (2026-09-17)

Build439 accepted for mantle and upright wheel. Verified installed DLL/banner;
both logs and latest INI archived under mantle-upright/accepted439.
Remaining wheel issue: opening while looking away leaves an awkward yaw.
Keep its hand-origin position and face the opening eye position in the horizontal
plane, rather than inherit head rotation. Freeze orientation afterward.
Visual, gesture axes and distance offset share the same opening normal.
No mantle, camera, blur or settings changes. Existing wheel tests now measure
movement along the actual panel right axis, not world X for an off-center panel.
Next launch: does opening the wheel while looking left/right leave it at the hand,
upright and squarely facing your position? A tilted/edge-on wheel fails facing;
movement after opening fails the frozen anchor.

## Installed439: mantle-only visibility candidate (2026-09-17)

Clean source59cc23a01 installed as vr33-hands-working-439-g59cc23a01.
Release build,9 exports and lint pass in addition to the host checks below.
Both logs/prior DLL/full INI archived at
build/playtest-candidates/installs/20260917-212054-067201.
Complete INI comparison changes only NoBlurWheel0->1; reversing that replacement
reproduces every prior byte. Installed DLL/INI hashes and CRLF independently pass.
No game launched. Mantle visual correctness and exit remain headset-unverified.

## Mantle-only pose/visibility correction and upright wheel (2026-09-17)

Build437 rollback accepted: normal movement restored. Dark Vision test with
NoBlurWheel0 also reported successful; pause with blur suppression was successful.
That does not eliminate a wheel-specific suppression conflict. Restore NoBlurWheel1
at the user's request; Dark Vision remains an open, reproducible-risk hypothesis.

VR-134 now adds native hand pose ownership only for explicit master Mantle when
MantleHandBack is enabled. The mantle Arms checkbox selects full native geometry
versus existing split hand geometry, preserving the native palette/depth once
handoff reaches native. Other master/upper/left states keep build437 pose policy.
No cancellation-eligibility pose trigger, camera edit or engine-memory writer added.
Hidden-mantle geometry policy is frozen with pose weight per render frame and held
through the existing release hysteresis. Existing action cancellation is unchanged.
Other states' Arms controls still choose native versus tracked poses; independent
geometry for those states is unfinished.

Wheel opening uses the existing yaw-only upright capture for both its visual plane
and gesture axes. Opening position, distance offset, crop and later fixed anchoring
are unchanged. Pitch/roll at entry no longer tilt the wheel.

Host checks:138 animation catalog/policy checks plus22 handoff checks,2204 wheel
checks,908 HUD anchor checks pass. These establish policy/math, not visual comfort.
Next launch question: with Mantling enabled and Show game arms unchecked, does a
mantle retain animated hands with forearms hidden and return to normal tracking?
Tracked/frozen hands or full forearms fail the separation; a bad exit fails release.
Do not interpret these checks as headset acceptance or a fix to Dark Vision.

## Dark Vision plus weapon wheel blackout isolation (2026-09-17)

Build437 rollback headset-confirmed normal for movement. New report: Dark Vision
works until opening the weapon wheel; world turns black while highlighted people
remain visible. Menu still appears to affect color/bloom despite blur suppression.
Verified437 DLL and log banner; logs/profile archived under
build/playtest-candidates/darkvision-menu/reported437.

MenuEffectsTick suppresses only reflected DisPostProcessManager.m_UIPPWeight.
Log confirms suppression during wheel context6. This is not proof that every UI
post-process effect is disabled, nor that this write causes the blackout.
Surface is the world scene with surviving Dark Vision silhouettes, not an
established eye-pair synchronization fault.

Prepared config-only A/B on the same437 DLL: NoBlurWheel1->0, every other INI byte
preserved, full diff and CRLF verified. Both logs/prior DLL/INI archived under
build/playtest-candidates/installs/20260917-211007-808112.
Question: with Dark Vision active, does opening the wheel still black out the world?
Expected if suppression conflicts: native menu background returns and scenery stays
visible. If black persists, suppression alone is insufficient; inspect native menu
post-process composition and head-look rendering separately. No headset result yet.
No code fix claimed and no game launched. Other menus and repo defaults unchanged.

## Installed437: recovery rollback to build433 (2026-09-17)

Installed vr33-hands-working-437-g0d415c741 on codex/misc-fixes.
All production source matches build433 commit5dee90153 exactly. Release build,
9 export checks,96 catalog/policy checks and22 handoff checks pass.
Both logs, prior DLL and full INI archived at
build/playtest-candidates/installs/20260917-210002-856329.
Independent installed DLL/INI hash checks pass. Entire installed INI is
byte-identical to the previous one, including latest F10 values; CRLF verified.
No game or simulator launched. Headset recovery test pending.

One launch question: are normal crouching, jumping and looking around restored,
including the same window exit? Success supports the build435 regression being
removed. Remaining disruption requires investigating the earlier path or session
state. Independent arm visibility is still unfinished; do not test that here.

## Build435 rejected: restore build433 pose policy (2026-09-17)

Tester reports unwanted crouch animation, native animation close to the face and
loss of normal control/view after jumping through a window. Installed435 DLL hash
and banner verified; both logs and latestINI archived in
build/playtest-candidates/animation-visible-hands/reported435. Run ends in normal
PreExit; this is not evidence of a crash.

Confirmed design error: native_pose_requested used cancellable_action as a native
pose trigger. Generic upper/left StatePlayerAction also covers movement transitions,
not only deliberate item interactions. Logs show repeated GAME ownership during
Jump/Falling/Walk with JumpIn/JumpLandSmall sequence history and native split-hands
reason, despite saved Jump/Falling/Walk arms being0. Sequence history is supporting
context, not authoritative playback identity. This broadens hand ownership beyond
the requested mantle fix. Exact close-face and view-disruption causes remain open.

Revert all435 production changes and their policy tests to exact433 source: original
arm-driven classifier, draw bypasses, mesh palette/depth path and F10 wording.
Keep433 action-cancellation controls, camera/keyhole fixes and latest saved settings.
No new camera compensation or guessed offset. The original limitation returns:
unchecking Show game arms also restores tracked hands, overriding native hand poses.
Do not describe that option as independently controlling geometry in this rollback.

Future work must explicitly distinguish pose choice from forearm geometry, preserve
ordinary movement ownership, and first validate one named mantle path. A generic
FSM StatePlayerAction match or cancellation eligibility is not a native-pose policy.
The435 test proved that helper-level policy checks cannot establish comfortable
native rendering or correct movement integration.435 is rejected, not accepted.

Next launch is recovery only: are normal crouching, jumping and looking around
restored, including the same window exit? Normal behavior supports435 as the
regression; a remaining fault requires tracing433 or persistent session state.
No new animation-visibility test in that launch.

## Installed435: retain animation with hidden arms (2026-09-17)

Clean source bdd7d207a installed as vr33-hands-working-435-gbdd7d207a.
Release,9 exports,103 catalog/policy checks plus22 handoff checks, lint and
default-profile parity pass. Both logs/priorDLL/fullINI preserved at
build/playtest-candidates/installs/20260917-204023-021893. Whole INI comparison
changes only Jump action0->1, adds Mantle action1 and Mantle Arms1->0.
Reversing those changes reproduces every prior byte. CRLF and installed DLL/INI
hashes independently verified. No game/simulator launched. Headset test pending.

Build433 mantle regression confirmed: Arms off returns hands to controller tracking
while the native FSM remains Mantle. Logs/latestINI archived in
animation-action-controls/reported433 after DLL/banner verification.
VR-134 now separates native pose ownership from full-arm visibility. Hidden arms
draw clipped/rounded animated hands with the original native palette; weapons retain
native animation. Camera and cancellation hook unchanged.103 policy/catalog checks
plus22 handoff checks pass. See ANIM-HANDOFF-PLAN.md current section.

Next launch isolates mantle: action enabled, forearms hidden, native hands/weapon
should still animate through the climb. Static/tracked hands falsify pose separation;
full forearms indicate split routing failure. Restore the prior jump-disable test to
enabled so it cannot interfere with reaching the ledge. Keep all other saved settings.

## Installed433: independent action controls (2026-09-17)

Installed vr33-hands-working-433-g5dee90153 from clean source. Release build,
9 exports,96 catalog checks,22 handoff checks,1000 production hook ABI calls,
cinematic math/scope restoration tests, default-profile parity and lint pass.
Archive: build/playtest-candidates/installs/20260917-202500-036762.
Both logs preserved. Full installedINI diff contains exactly one added setting:
Anim.Action.0.StatePlayerMasterJump=0. Removing it reproduces every prior byte;
CRLF and installed DLL/INI hashes independently verified. No game launched.

codex/misc-fixes, VR-134. Build431 door/keyhole fix reported accepted; both logs,
latestINI and verified DLL identity preserved in misc-special-camera/reported431.
Lean and texture allocation crash remain separate open items in VR-133.

F10 Animations:18 Enable action toggles reject native FSM requests before entry;
all40 Show game arms choices remain independent of cancellation and camera ownership.
Automatic/recovery/story states cannot be cancelled by this UI. Existing saved arm
values are retained. Native animation view left/right is a default-zero manual trim,
not a proven automatic correction. Both eyes share a frozen scope value; menus skip it.
Details and limitations: ANIM-HANDOFF-PLAN.md and ENGINE_NOTES.md current sections.

Next launch: does Jumping disabled prevent the jump, with Enable action checked
live restoring it? No jump followed by normal jump supports the new boundary.
Jumping while disabled means missed ownership/request path; inability to jump after
enabling means a cancellation regression. Check build banner and read the log.
Do not combine this with the later arm/alignment perceptual test.

## Installed misc candidate431 (2026-09-17)

Installed vr33-hands-working-431-g2591ebc5c from clean source on codex/misc-fixes.
Release,9 exports,908 HUD checks,85 animation catalog checks plus existing
animation blend checks, cinematic math and extracted production camera scope
checks pass. Default-profile byte parity and lint pass. Both logs, prior DLL
and complete INI archived at build/playtest-candidates/installs/20260917-125202-935413.
Whole INI comparison: only ReadingTilt -31.000->0.000, ReadingTiltReference=1,
and Cine.SpecialHeadLook=1 added. Reversing those changes yields identical prior
bytes; CRLF and installed DLL/INI hashes independently verified. No game launch.
F10 Animations exposes arm-state choices; F10 View has the armed special-camera
option. One question this launch: natural head look during Y-lean and normal
camera movement after release? Keyholes and animation checkbox perception remain
separate follow-up validation. Texture allocation crash is diagnosed, not fixed.

## Current: misc fixes after merged PR73 (2026-09-17)

Controller/reading work merged to VR-Main in PR73 (374440668); preserved its
branch. Active branch codex/misc-fixes. Accepted427 ReadingTilt=-31 becomes0,
with versioned migration preserving the same physical angle. Latest427 logs,
INI and crash dump archived under reading-fixed-grip/accepted427.

VR-134: F10 Animations lists all40 shipped player FSM entries (23 master,11
upper,6 left) with independent live/saved arm checkboxes, active state display,
filter and reset. Existing handback defaults remain until overridden. States,
not individual clips: sequence history can be stale. Existing body correction,
blend and liveness rules retained. Camera classification ignores these new
visibility overrides, so checking walking does not claim special head look.

VR-133:119 lean trace samples retain player influence0/1/0 and PVR head writes;
native lean adds about10 degrees of camera roll after that write. Decompiled
lean limits pitch/yaw; keyholes relocate the controller. Candidate extends the
existing draw-scoped head owner only to these two explicit states, restores
native fields after both eyes, and carries physical yaw once on return to Walk
with refreshed identity validation. Native translation remains; special views
use raw HMD position rather than gameplay neck cancellation. Default-off
Cine.SpecialHeadLook has F10 View toggle; arm it for the next lean test.

Crash is a separate unresolved allocation failure: SYSTEMMEM DXT5 twin
1920x2048 lv1 returned0x8007000e before LockRect INVALIDCALL. Installed exe is
already large-address-aware. Preserved dump lacks memory-info stream; do not
claim leak, texture-pack causality or camera causality. Add failure-only virtual
memory/commit diagnostics and memory-info metadata to ordinary crash dumps.
No destructive shadow eviction or device-policy change. No game/simulator run.

Next launch question: while holding Y and leaning, can the head turn naturally
and return to normal movement after release? Stable hold/exit supports scoped
ownership; hold-only failure points at native modifiers, exit-only failure at
handoff. Any repeat crash requires allocation diagnostics. Keyhole validation
and animation checkbox perception follow separately, not in this launch.

## Current: accepted reading attachment and controller merge (2026-09-17)

Build427 fixed reading attachment accepted after ReadingTilt=-31.000 adjustment.
Rebase that exact physical angle to zero; ReadingTiltReference=1 versions the
trim so older saved angles migrate without changing appearance. Both427 logs
and latest INI archived in reading-fixed-grip/accepted427; DLL/banner verified.
VR-132 tracks accepted controller and reading work. New lean/keyhole camera
instability and subsequent texture allocation failure are VR-133; animation
checkboxes are VR-134 under VR-89. These follow on codex/misc-fixes after the
explicitly authorized controller PR/merge. Crash log reports SYSTEMMEM twin
allocation 0x8007000e before texture LockRect failure; camera causality unproven.

## Current: fixed captured reading attachment (2026-09-17)

425 automatic entry tilt was active in logs but did not resolve the reported
inconsistency. Its per-opening grip reference remained variable. Verified425
DLL/banner and archived both logs/latest profile in reading-auto-tilt/reported425.
The last full Note pose at16752906 supplies gripQ=(.672240,-.104614,-.336110,.651290)
and pageQ=(-.049763,-.026783,.011967,.998330), autoTilt=-45.267, manual=0.
This is the last recorded book sample, not a claimed exact shutdown sample.
Replace entry fitting with fixed inverse(gripQ)*pageQ reference. Preserve the
placement basis separately by removing the old pitch from pageQ before deriving
its grip-relative rotation. This reproduces both orientation and the existing
position offsets at that sample; both then follow the current hand rigidly.
No head/opening pose participates. Keep one additive ReadingTilt slider for
notes/books/journal. Remove the failed automatic pitch helper; history remains
below. Headset confirmation pending. Test: open the same book with different
initial hand poses, then return to the comfortable pose. Does its angle remain
correct and identical? Consistent but wrong means trim/reference needs adjustment;
variable means another attachment path remains. No game or simulator launch.

Installed vr33-hands-working-427-g15503fbcc from clean source.900 HUD anchor
checks, default-profile parity, release build,9 exports and lint pass. Both logs
and prior DLL/INI archived at build/playtest-candidates/installs/20260917-122258-812613.
Full installed INI is byte-identical to prior saved profile; CRLF and DLL/INI
hashes independently verified. Headset result pending.


## Current: automatic reading entry tilt after423 (2026-09-17)

423 DLL/banner verified; both logs/latest INI archived in reading-tilt/reported423.
Single manual tilt still depended on opening hand position because initial page
was forced upright at every height. Compute initial pitch from actual panel
center to eye height in the opening yaw plane, then latch it for this opening.
Keep hand-follow rotation and position offsets; do not swivel as the head moves.
Single ReadingTilt remains additive trim. Behind-head/degenerate fit falls back
to upright. No calibration UI, additional axes, camera or stereo changes.
The exit log again lacks complete left-grip pose; do not claim the preferred
pose was recovered. Latest saved trim is0. Added bounded entry/full-pose logs
for subsequent reports. Standalone tests vary entry wrist rotation and hand
height/yaw. Installed vr33-hands-working-425-g17b228d64.564 anchor checks, release,9 exports
and lint pass. Both logs/full prior DLL/INI archived at
build/playtest-candidates/installs/20260917-115527-386246. Entire installed INI
byte-identical; CRLF and DLL/INI hashes verified. No game/simulator launch. One launch question: opening the same book
with your hand low versus near eye level, does it face you comfortably in both
cases without changing the slider? Yes supports geometry-based entry tilt;
wrong/inverted or unchanged pitch falsifies the fit/sign/context path.

## Current: single reading tilt slider (2026-09-17)

User replaced calibration request with one rotation axis; existing attachment
is satisfactory. Remove calibration/timer/quaternion configuration UI and path.
Retain419 opening/hand-follow attachment, with shared ReadingTilt in degrees
(default0, range-180..180) for notes/books/journal. F10 HUD > Notes and journal
on the hand exposes only Reading tilt, saves live. Rotation is local pitch
around the existing panel center; hand-relative center and all position offsets
remain unchanged. Retain per-panel width/right/up/depth controls. Old calibration
keys ignored. Three-axis draft never built or installed. No guessed comfortable
pose.469 anchor tests pass, including identity at0 and signed local pitch.
Installed vr33-hands-working-423-g4770f6d8d;469 anchor checks, release,9 exports,
lint and production default-profile parity pass. Both logs/full DLL/INI archived
at build/playtest-candidates/installs/20260917-114630-645045. Entire installed
INI byte-identical; CRLF and DLL/INI hashes verified. No game/simulator launch. One launch question: does Reading tilt adjust the book
angle without moving its attachment point? Yes supports center-preserving tilt;
position movement or extra rotation means the placement composition is wrong.

## Current: saved reading attachment after accepted419 (2026-09-17)

419 controller changes headset-accepted. Verified DLL/banner and archived both
logs/latest profile in controller-cross-hand/accepted419. User ended the run
with a comfortable book pose and requested a persistent default attachment.
The log does not contain the complete reading grip quaternion/relative pose;
scalar hand-angle/depth diagnostics cannot reconstruct it. No pose was invented.

Current readers recapture upright orientation relative to the entry grip on
each menu opening. Add shared ReadingSavedGrip (default1), validated relative
quaternion ReadingGripQx/Qy/Qz/Qw and ReadingGripValid (default0). Without valid
calibration retain existing behavior; no arbitrary new angle is installed.
F10 HUD > Notes and journal on the hand offers five-second calibration: close
F10, hold left hand comfortably, look where the page should face. Capture stores
inverse(grip)*head orientation, preserving offsets/size, applies to notes/books/
journal and persists automatically. Three-second tracking retry then fail-soft;
invalid capture retains prior saved grip. Toggle restores old per-opening mode.
New NoteHandUp/JournalHandUp sliders complement existing right/depth offsets.
No weapon-dial lifecycle, camera, image orientation or stereo policy changes.
472 anchor checks pass, including saved reload, changed opening gaze, rotated
hand, invalid quaternion refusal and preservation of previous calibration.
Installed vr33-hands-working-421-gddc7000cd from clean sourceddc7000cd.
Release build/9 exports/lint and472 anchor checks pass. Both logs/full previous
DLL/INI archived at build/playtest-candidates/installs/20260917-114102-728633.
Entire installed INI byte-identical to latest save; zero settings changes,
CRLF and DLL/INI hashes independently verified. No game/simulator launch.
Exact preferred attachment awaits the one-time F10 capture; no guessed pose
promoted into repository defaults. Branch local; no PR/main merge requested.
One launch question: after using the five-second capture in a comfortable pose,
does reopening a book from a different wrist position preserve the same fit in
your hand? Stable fit supports the saved relative transform; changed fit means
an entry path still replaces it. Check hud/reading-grip and installed saved keys.

## Current: cross-hand controls and powers-menu scroll (2026-09-17)

417 verified against installed manifest/hash/log, both logs/latest INI archived
at build/playtest-candidates/controller-emulation/reported417. Report: Y lean
needs the right stick; left thumbrest modifier did not work; remove grip choice;
right-stick scroll absent in journal powers menu. Logs prove both thumbrests
reported. Left modifier4 was paired with left stick (flip0); no direction bits,
whereas R3 produced all four D-pad directions. Same-hand pairing is the measured
configuration mistake, not missing left touch. Updated policy automatically
pairs either thumbrest with its opposite stick, including old INI combinations.

Remove grip mode from UI and composer; keep left-rest numeric value4. Old mode3
normalizes toOff and neither grip is consumed. Gameplay-only Y sends physical
right-stick axes to native left-stick lean axes at the final pad boundary and
zeros right axes; it outranks D-pad and excludes menus/wheel/cinematics/F10.
No engine-memory changes or camera writes. Lean behavior still needs headset
confirmation; it uses the existing native Y action. Native non-wheel menus now
retain continuous right-stick vertical input after menu shaping. Horizontal
menu shaping, wheel input and modifier ownership retain precedence.

196 host checks (removed grip tests, added cross-hand/lean/menu axis checks),
default writer/package/golden byte parity pass.
Installed vr33-hands-working-419-g1f10404e6 from clean source1f10404e6.
Release/9 exports/lint pass;196 controller checks and default-profile parity pass.
Both logs/full prior DLL/INI archived in
build/playtest-candidates/installs/20260917-104459-011878. Entire installed INI
byte-identical to latest save; zero settings changes. DLL/INI hashes and CRLF
independently verified. Headset result pending; source local, no new PR/merge.
Ticket publication was explicitly requested, but automatic review rejected even
the minimal summary to Linear; exact-text approval question pending. No issue ID.
One launch question: in the journal powers menu, does the right stick now scroll
up/down normally? Success confirms final-axis restoration; no response with
nonzero pad/axes RY means the menu needs another native input path; zero RY
means a context/modifier gate is still consuming it. Do not launch game/simulator.

## Current: controller emulation improvements (2026-09-17)

PR72 merged accepted marker/startup work into VR-Main at6a403c600; preserved
feature branch. New branch codex/controller-emulation-improvements starts there.
Installed413 DLL/banner verified and both logs/latest profile archived in
build/playtest-candidates/vr129-rune-icon-continuity/accepted413 before new work.

New F10 Controls tab: DpadModifier0..4 matches BioShock1's explicit choices,
DpadFlip0 selects left stick,1 right; flip automatically swaps thumbrest choice.
PauseChord1 enables X+Y as menu. Default modifier1/right thumbrest, flip0/left.
Menu tap pulses Start150ms on release; modifier+menu immediately holds Back;
500ms unmodified hold also yields Back. Back ownership survives releasing the
modifier first. Chord consumes X/Y until both release. Y now forwards native Y.

Pure composer precedes Dishonored pad shaping. All four D-pad directions are
HELD: game Select on press/Use on release, not BioShock ammo pulses. Consume the
selected stick, R3 health input or left-grip wheel input only when assigned.
Wheel can still use hand direction with D-pad shortcut assignment. Recenter and
accepted menu navigation retained. No camera/stereo/engine-memory writes added.
221 host checks and production default-writer/package/golden byte parity pass.
Linear ticket creation pending explicit external-publication permission after
approval review rejection; no ticket identifier invented.
Installed vr33-hands-working-417-gdc6751a0f from clean sourcedc6751a0f.
Release/9 exports/lint/221 controller checks and default-profile byte parity pass.
Both logs, previous DLL and full INI archived at
build/playtest-candidates/installs/20260917-092418-488098.
Entire installed INI differs only by three new Controllers keys:
DpadModifier=1,DpadFlip=0,PauseChord=1. Removing those lines reproduces
prior INI byte-for-byte; CRLF and installed hashes independently verified.
No game/simulator launch. Controller source remains local, no new PR/merge.

One launch question: does right-thumbrest + left stick select/use all four
shortcuts without walking? Correct selection supports runtime-to-pad mapping;
no response implies binding/threshold gap, walking implies consumption failure.

## Current: HUD improvements merged in PR71 (2026-09-16)

Build401 (vr33-hands-working-401-g4f974a5da) accepted for publication/merge.
DLL hash and log banner verified; both logs and latest full F10 INI archived at
build/playtest-candidates/vr129-side-alpha/accepted401. User explicitly authorized
PR and merge of codex/vr-129-hud-fixes into VR-Main. PR71 merges this work;
the local and remote feature branch are preserved.

Latest profile captured byte-for-byte in release INI, golden and production
WriteDefaultIni. Only new tuning since previous defaults: WheelPartsAlphaMode=mix,
Gain0.890,Gamma0.660,Mix2.090 (Floor remains0). Accepted INI SHA256
9c429cb77e9bc730d696357b46ed7640f5d6a4f33b8a25f25084ae8fe91a68af.
Installed401 remains the tested binary; no reinstall is required for a defaults
literal update. No game/simulator launch. Production writer/package/golden parity,
lint and diff checks pass; clean release build and all9 exports pass.
PR: https://github.com/VR-Stereo-Hub/Dishonored-VR/pull/71

Accepted scope: wheel closing visual ownership; independent D-pad/potion window
panels with shared alpha; rounded wrists; accessible native objective controls;
complete tuned defaults. Host evidence:104 HUD controls,912 wrist/crop,44 native,
465 route,462 anchor,45 menu checks and actual D3D11 WARP mask/hue/crop checks.

VR-129 remains open for occasional objective/window transfers, incomplete native
label association and the earlier rare wheel-exit zoom. Capture-miss logging is
diagnostic, not proof of semantic ownership. VR-130 rounded wrists are accepted.
No new test requested for this merge. Next work should start from updated VR-Main,
read current HUD_ANCHORS/FLICKER_REFERENCE, and verify installed.json/log identity.

## Current handoff: VR-126 refined dial and menu immersion (2026-09-16)

- Branch codex/hud-weapon-dial; PR67/68 already merged, no HUD merge authorized.
- Build372 dial accepted and tuned: width0.350m,travel0.040m,crop0.400x0.400.
  Verified logs/INI archived in build/playtest-candidates/vr126-weapon-dial/accepted-tuning.
- New work: tiny-motion direction selection, camera-parallel panel, true circular
  feathered crop, closer/farther slider, per-menu live head look and UI blur controls.
- Installed374 (`vr33-hands-working-374-ga51e1799f`), clean source a51e1799f.
  Candidate build/playtest-candidates/vr126-dial-immersion; both logs and previous
  DLL/INI archived in build/playtest-candidates/installs/20260916-010814-146048.
  Full INI diff:16 new controls only; every previous value retained. Hashes/CRLF
  verified. HeadLookWheel/Note=1,NoBlurWheel=1,direction-only/circle=1,deadzone2mm,
  distance0; tuned width0.350m/crop0.400x0.400 retained. Not headset-tested yet.
- Current candidate identity is in build/playtest-candidates/installed.json.
  Preserve all installed settings/CRLF; compare the entire INI and archive both logs.
- Read only the current VR-126 refinement section in [HUD_ANCHORS](HUD_ANCHORS.md) for
  details, evidence, failed hypotheses, validation and the single launch question.
- Next question: with Wheel open, does turning reveal fresh world scenery beyond
  the old FOV rectangle and return normally on release? Camera and UI-blur fixes
  remain headset-unverified. Scope and effect diagnostics distinguish the outcomes.
- Standalone2185 dial checks, actual GPU mask test,16 production menu lifecycle
  checks, existing camera/FOV and HUD tests pass. No game/simulator launched.
- No subagents, source branch deletion or new main merge. User launches only.

### Minimal next-chat prompt

Continue VR-126 on codex/hud-weapon-dial. Read AGENTS/CLAUDE, current STATUS and
HUD_ANCHORS current refinement section; FLICKER_REFERENCE current menu-world entry
for camera work.372 dial accepted; preserve tuned35cm width and0.4x0.4 crop. New
candidate adds2mm direction-only selection,camera-plane orientation,circle mask,
distance offset,and per-menu head-look/UI-blur controls. Check installed.json and
verify log banner before analysis; archive both logs and preserve full INI/CRLF.
User checks head turns beyond old FOV while Wheel open. Inspect menu/head and
menu/blur; a write is not visual acceptance. No subagents or game/simulator launches.

## Performance research shelved, 2026-09-15

Historical shelving checkpoint; the VR-50 exception above supersedes installation and
playtest status. Unfinished performance tickets
are Backlog and unassigned. [PERFORMANCE.md](PERFORMANCE.md) is the compact canonical
record of measured results, rejected routes, useful tools and resumption options.
Ordinary per-eye preparation measured 1.322 ms/pair including 1.082 ms culling;
no safe work-sharing optimization is established. Default-off profilers and the
measured desktop throughput/tail experiment are retained. Failed nonblocking
Present and invalid coarse CPU-ms instrumentation are removed.
Exact installed307 remains unchanged. Research cleanup is build/host-validated;
no new install or headset acceptance is claimed. Source branches are preserved.
The user authorized a consolidation PR and merge to VR-Main; this is research
preservation, not promotion of experimental rendering settings.

## Completed merge and installed state: 2026-09-14

World-only PR #61 is integrated into VR-Main at a60516c4b.
Installed build 272-ge3bb7ac2a, compiled 14:27:16, has source identical to
that merge apart from commit/build metadata. DLL SHA256
473bca2b298a988d48307ac23a2a2b257d53f00ac2f8df83dcfa95149c3d373b.
The installed, release and golden INIs are byte-identical CRLF, SHA256
de20bd794ab0917ca5450178207a333bd2a28066f3edfac253a754b153ac5ce5.
The full installation comparison has ZERO INI changes. Both prior logs archived
at build/playtest-candidates/installs/20260914-142912-415128.

Only default promotion differs in source from the repeatedly accepted original
world candidate: no hand-normalization experiment is included. Release build,
frame tests, exports, lint and golden checks passed. No game launched.
Retain both feature branches and the exact original world checkpoint.
Next work is deferred: investigate left-eye hand jumps after opening-cutscene
exit (VR-95); sewer-save loading was reported essentially clean. Do not infer
that all save loads or all cutscenes reproduce it.

## Earlier acceptance and scope

## Current acceptance: world smoothness approved for main, 2026-09-14

The exact original world candidate 271-g8cd27652 was restored and independently
reconfirmed on the headset. Exceptional world stability returned. Its installed
INI is now copied byte-for-byte to the release and golden INIs, and the default
writer matches it. `Pace.ImageOrientation=1` is the generated and missing-key
default by explicit user request after acceptance; explicit 0 remains respected.

This merge contains the WORLD fix from 8cd27652 only. The unaccepted hand
normalization code from 12a974134 is absent. The branch
`codex/vr-116-flicker-fix-patch` preserves that experiment for later investigation.
The accepted DLL/INI bundle and `vr-116-world-smooth-271` tag remain recoverable.
The user explicitly approved publishing this accepted state to VR-Main.

Latest symptom refinement: residual left-eye hand flicker was observed after
exiting the opening cutscene, while loading a sewer save was essentially clean.
This makes transition state relevant; it does not prove the precise cause.
Track the deferred hand issue with VR-95 and preserve its historical predictor
regression. Do not resume or include hand changes in this merge.

Reconfirmation logs/INI: `build/flicker-120/reconfirmed-world`; banner and DLL
hash match 271-g8cd27652, compiled 13:46:59. Source changes beyond that checkpoint
are documentation and INI/default promotion only. No engine-memory writer,
camera transform, hand pose or frame pacing change is introduced by promotion.

Next: complete and verify the authorized main merge; hand/cutscene work waits.
Historical pending, publication-blocked and candidate text below records earlier
stages and does not override this scope or the new explicit merge authorization.

## Earlier records

## Current state: VR-116 120Hz physical-head-turn flicker investigation

VR-115 desktop branch is parked at committed1d6558a3; no uncommitted changes
needed a stash. Its build271 was never installed. Do not mix that experiment
into this investigation. New branch codex/vr-116-120hz-head-motion from main255d1c91.
Verified main build266 run shows120Hz, world/hand lag2, automatic A/Bs off.
Physical head turns alone trigger severe world flicker; stick turns and still
head do not. Both cinematic and gameplay scenes affected. Both logs archived
at build/flicker-120/20260914-133932. Ring/eye summaries remain healthy; uneven
cadence and occasional pose disagreement are measured, causality unresolved.

Default-off Pace.ImageOrientation candidate uses same-eye captured camera-record
orientation for submission, with numeric lag fallback; positions and game/hand
behavior unchanged. F10 View provides live A/B and Save. Eight new host checks
and existing frame suites pass; release,exports,lint,golden pass. No game launch.
Exact findings, rejected claims and one-question120Hz test:
[HEAD_MOTION_120HZ.md](HEAD_MOTION_120HZ.md).
Build/install identity will be recorded after packaging. No merge authorized.

## Earlier records

## Current state: standing roll accepted; VR-87 remains open

Build245 confirms roll while looking up/down. Remaining inverse vertical motion
is the existing eye-ceiling limitation VR-87, supported by joined log accounting.
See STANDING_PITCH_ROLL_ARC.md for evidence. PR57 ready, unmerged; PR58 next.
Installed build remains245. No new install or INI change. Preserve all three PRs
unmerged until remaining tests complete. Never launch the game; no subagents.

## Earlier records

## Current state: standing camera-roll candidate after accepted PR56

Installed245-g0cd7b263 (Sep14 07:57:48), standing-roll fix and ZAccount ON.
Full INI diff contains only those two changes; hashes/CRLF verified.

PR56 is ready for review and unmerged. Build239-gadebc947 confirmed head-based
movement, alongside the accepted cinematic comfort/FOV and mono handoff fixes.
PR57 now includes that parent through cb77328d. Its next build retains all
accepted settings and enables Neck.UprightPitchArc=1 plus PosTrack.ZAccount=1.
Read docs/dishonored/STANDING_PITCH_ROLL_ARC.md for the one-question test.
PR58 mono UI work remains a separate later candidate. Merge all three only after
the remaining tests finish. Character-mode drift (VR-109) remains open.
Never launch the game, no subagents. Agent installs and reads/archives both logs.
Canonical installed identity: build/playtest-candidates/installed.json.

## Earlier records

## Current state: three stacked candidates, no merge approval

The new branch codex/vr-107-mono-anchors-and-ui-state starts at d27fdf6c on
PR57's standing-camera branch. VR-107 adds configurable mono anchoring;
VR-108 and existing VR-74/VR-71 cover loading/menu stereo and input ownership.
Read docs/dishonored/MONO_ANCHOR_UI_STATE.md for implementation and test plan.
Both new levers default off; preserved build240 enables them together.

PR56 (cinematic build234) and PR57 (standing build237) remain draft/unmerged.
Keep build234 installed first; build237 and the new cumulative UI candidate
remain separately archived for agent-managed swaps. No headset testing has
occurred on these candidates. One question per launch; never launch the game.
No subagents. Build240 was installed and hash-verified, then build234 restored and verified.
The next step is deferred headset testing in order234,237,240. The agent archives both
logs and checks full INI diffs/CRLF every swap. installed.json under
build/playtest-candidates is the active installation authority.

## Earlier session records

## Current state: two candidates awaiting separate headset tests
## Current state: PR56 accepted; next is standing-roll PR57

Verified239-gadebc947 confirms head-based movement. PR56 finalized for review;
keep all three PRs unmerged until the remaining tests finish. Character-oriented
drift remains open under VR-109. Next rebuild PR57 with this accepted parent,
enable UprightPitchArc and ZAccount, and test standing pitched-head roll versus
crouching. Preserve HeadBasedMovement=1. Agent handles installs/logs, no game
launch, no subagents, one question per launch. See the latest cinematic notes.

## Previous records

## Current state: head-based movement239 installed

Installed239-gadebc947 (Sep14 07:44:40), HeadBasedMovement=1. Full INI
diff adds that key only; hashes/CRLF verified and both logs archived.

Cinematic237-g563e14d6 confirms the mono handoff fix. Character movement drift
persists; VR-109 remains open for that part. VR-110 adds a selectable, saved
head-based mode that uses native view facing, preserving the character option.
Read CINEMATIC_FOV_AND_HANDS.md latest section for evidence and the single test.
Current branch codex/vr-50-cinematic-fov-and-hands, draft PR56. No merge approval.
PR57/58 are preserved and need the accepted parent correction before new tests.
Never launch the game; no subagents. Agent builds/installs and archives logs.
Canonical active build: build/playtest-candidates/installed.json.

## Earlier records

## Current state: VR-109 correction installed, headset test pending

Installed: 237-g563e14d6, Sep14 07:27:03 (cinematic correction, NOT the
standing candidate237-gfd7a830d). Full INI diff empty; both logs archived.

2026-09-14. Verified build234 (23:11:43 Sep13) playtest confirms cinematic roll/
pitch comfort, FOV and free look. New VR-109 tracks brief mono exits and diagonal
movement after cinematic ownership. Fix extends draft PR56 on
codex/vr-50-cinematic-fov-and-hands. No merge approval. Read the latest section
of docs/dishonored/CINEMATIC_FOV_AND_HANDS.md before continuing.

PR57/build237 and PR58/build240 remain preserved, unmerged and untested. They
predate this correction: propagate an accepted parent fix before their next
builds/tests. Never launch the game. No subagents. One question per launch.
Agent archives both logs, verifies the banner and installed hash, builds/installs
and checks the full INI diff and CRLF. Canonical install manifest is
build/playtest-candidates/installed.json. Mantling still lacks explicit acceptance.

## Previous session records

## Current candidate: upright cinematic tracking

Cinematic PR56 is draft/unmerged; build234 is preserved for the first test.
Draft PR57 targets the cinematic branch (PR56). Both remain unmerged.
Current branch codex/vr-106-standing-pitch-roll-arc was created from parent
codex/vr-50-cinematic-fov-and-hands at6f85417a. VR-106 adds a default-off fix
for the standing steep-pitch roll arc. Read STANDING_PITCH_ROLL_ARC.md in
docs/dishonored for evidence, candidate, controls and the two-build test order.
The agent manages installs and archives both logs, with full INI diffs/CRLF.
Canonical active installation: build/playtest-candidates/installed.json once
swaps run; preserved bundles are under build/playtest-candidates. Build234
must be left installed first for tomorrow. No test tonight, no game launch,
no subagents, no merge approval. Mantling still untested.

## Previous records (latest candidate above supersedes earlier plans)

## Combined build steering, 2026-09-13

The user explicitly requests both FOV suppression and native cinematic hands/arms
in the same build. LockFov and CinematicHandBack are implemented and installed
ON together for the next test. The earlier separate-build plan below is superseded.
Current branch codex/vr-50-cinematic-fov-and-hands; VR-50 and VR-104 In Progress.
Read CINEMATIC_FOV_AND_HANDS.md (under docs/dishonored) for current scope and test.
No new merge approval. Never launch the game; no subagents.

## Latest continuation: cinematic FOV and hands, 2026-09-13

PR55 merged823c53d5; VR-103 Done and headset-confirmed. VR-50 branch is
codex/vr-50-cinematic-fov-and-hands. Read CINEMATIC_FOV_AND_HANDS.md first for the
FOV candidate and queued VR-104 native handback. FOV is the only first-test
behavior change. Latest install: build/cinematic-fov/latest-install.json.
No subagents, never launch the game, no merge approval for new work.

## Current state (2026-09-13): PR54 merged; VR-103 candidate

Cinematic head look, pitch and black borders are headset-confirmed and merged
in PR54 (39a68a50); VR-70 is Done. New VR-103 is In Progress on
codex/vr-103-stereo-state-transitions from that merge. The candidate separates
stereo scene eligibility from gameplay/input locks using decompiled state
names and live scene activity. It is default-off and awaiting headset testing.
See STEREO_STATE_TRANSITIONS.md for evidence and the one-question
dialogue transition test. Latest install manifest: build/stereo-state/latest-install.json.
Never launch the game; read/archive logs yourself. No subagents this session.
No merge permission for VR-103. PR54 approval does not extend to this work.

## Historical handoff (superseded by current state above)

## Latest steering (2026-09-13)

PR54 head look,pitch and border removal are accepted and explicitly authorized
for merge. VR-103 is the new In Progress ticket for dialogue-choice mono and
stereo/mono handoffs. Start its new branch from the merged VR-Main,read decompiled
state declarations,retain confirmed camera behavior. No subagents this session.
Approval to merge PR54 does not authorize merging the new state work.

## Continuation update (2026-09-13, VR-70)

Linear reconciliation below is now complete. VR-96 is Done/High; VR-98 remains
Done; completed follow-ups VR-100/VR-101 and open freeze VR-102 are recorded.
VR-70 is In Progress on codex/vr-70-cinematic-head-tracking from cccb1815.
Read CINEMATIC_HEAD_TRACKING.md for the current border-removal test.
Build221 natural pitch is headset-confirmed,2770 restores,zero refusals. The
new HideBorders control intercepts only the verified native stripe query and
preserves all HUD mask values.136 x86 checks pass. Check latest-install.json
for exact installed identity. No more subagents this session. No merge
permission; PR54 remains draft. The older handoff follows for history.

# Next session: physical head movement during cinematics

Read ../../AGENTS.md and ../../CLAUDE.md first, then STATUS and the latest
FLICKER_REFERENCE. Personal rules also live in the global Codex AGENTS.md. The
tester launches the game and reports; the agent builds, installs, arms diagnostics,
verifies the banner and reads/archives the logs. Never launch the game yourself.
Use one question per launch. Preserve full installed INI bytes/CRLF and compare
all settings on every install. Engine writers require current IsLiveObject plus
retained identity checks. Commit/PR/merge text has no trailers or attribution.

## Starting state

The prior branch was merged as PR #53 (branch commit dbf61fa7). Start from the
updated VR-Main; confirm its merge commit from git/GitHub before beginning. VR-96's crawl-release corruptor is fixed
and headset-confirmed: nine stale updates refused, 57 valid updates, 11 pauses,
zero exceptions, clean exit. Startup stereo, fast notes and reload-eye repair
are also confirmed. Do not reopen the disproved tag-drain theory.

The complete installed profile is now release/dishonored_vr.ini and generated
by WriteDefaultIni, including saved F10 controls, calibration, diagnostics and
D:\dvr-data. Keep installed overrides. Name cache remains OFF. The first-fault
full-memory capture stays armed in case another GC signature recurs. Latest
install manifest is ignored build/crash-triage/latest-install.json; read its
actual build/hash before interpreting the next log. Full dumps are local only.

## Feature to implement

During cinematics the view stays at a fixed position when the tester physically
moves their head. Right-stick turning works. First make physical HMD movement
work during cinematics, including identifying whether position, orientation or
both are suppressed. Preserve the game's authored camera motion and existing
right-stick behavior; do not substitute an unrelated free-camera mode. No
cinematic implementation was made in the prior session.

Use the existing game-state instrumentation to establish which flags identify
the cinematic and which writer/layer owns the view. Trace game state -> authored
camera -> HMD offset -> capture/runtime pose. Existing cinematic policy may
already intentionally suppress the missing movement; measure before removing
safety gates. Do not treat pause menus, notes and loading as cinematics.

Relevant starting points:

- game/dishonored/commands.cpp: GameStateTick and CINEMATIC verdict.
- game/dishonored/head_track.cpp: cinematic hold / head tracking ownership.
- game/dishonored/anim_state.cpp and anim_state.h: current script state snapshots.
- core/vr/openxr_runtime.cpp: g_cineDrive, CineDrive::Authored, cinematic quad/
  stereo choice and submitted pose. Paths above are under src/.
- GAMEPLAY_STATE.md, ENGINE_NOTES.md, FLICKER_REFERENCE.md and DESKTOP_MIRROR.md.
- Existing GitHub PR #12 concerns HUD/cinematics. Inspect for overlapping work;
  do not merge it or reuse its older assumptions automatically.

Create or verify a Linear issue BEFORE assigning its number to code or commits.
Create a new codex/vr-<number>-<description> branch from updated VR-Main. The
prior permission to merge covered the finished load/reload branch only; it is
not permission to merge this future cinematic feature. No new milestone/release.

## Linear synchronization, first task after app restart

Linear was not exposed to the preceding task despite being connected by the
maintainer. Browser/native UI runtimes also failed. No board updates were claimed.
Read live issues and preserve existing metadata; search before creating anything.

1. VR-96: verify the merge integration closed it; otherwise mark Done after
   verifying the PR is merged. Keep the recurrence/history and High priority.
   Add the initial-fault owner evidence and successful guard test from ENGINE_NOTES.
2. VR-98: preserve its already merged status; record prompt notes across reload
   after observer-table reuse repair (five cycles before/five after, 0-16 ms).
3. Find/map startup stereo and reload right-eye follow-ups. Create missing records
   with project/milestone/Type/priority, linking the completed PR and measurements;
   do not invent new IDs or reopen VR-80's resolved cause. VR-77 and VR-99 remain
   context for underlying generation/timing, not proof of ownership of this fix.
4. Keep weapon startup freezing open. Name-cache optimization is implemented but
   untested on the headset and OFF; do not mark the freeze fixed.
5. Create/verify the cinematic physical-head-tracking issue, then branch and work.
6. Post one batch project update after reconciliation: stability fixes merged,
   complete tested defaults promoted, cinematic head motion next, startup freeze
   still open. Do not declare a release or invent a milestone.

Team VR, project Dishonored VR Mod, existing milestone
Stable - 6DOF, motion controls and alpha parity. Follow docs/LINEAR_AND_GITHUB.md.
The local prepared crash updates are under build/crash-triage; the committed
ENGINE_NOTES/FLICKER_REFERENCE contain the durable findings and hashes.

## Deferred headset queue, 2026-09-13

Testing resumes in the next session. PR56 stays draft and unmerged. Candidate234
is preserved independently under build/playtest-candidates/cinematic-234 with
DLL, exact CRLF INI and manifest. Test this candidate first: tilted Empress scene
and free head look. Mantle remains a separate untested acceptance item.
VR-106 tracks a longstanding standing-only pitched-head roll arc on a child
branch from this branch. It must not be confused with authored cinematic roll.
The agent swaps builds, compares the entire installed INI and reads/archives
both logs; the tester only launches and reports observations. One question per
launch. No game launch or merge is authorized. Both candidates await testing.

## Final artifacts and installation, 2026-09-13

Standing candidate237: vr33-hands-working-237-gfd7a830d, compile23:34:10.
DLL SHA256 b9cca3b0e0b77212424d25158807df66178a689f617f4d3d067e5efb975671a5.
Bundle: build/playtest-candidates/standing-arc-237 (DLL, CRLF INI, manifest).
The candidate was installed and hash-verified; full INI diff only added
Neck.UprightPitchArc=1 and changed PosTrack.ZAccount=0 to1. Both logs and prior
files archived at build/playtest-candidates/installs/20260913-233449-048351.
Then cinematic234 was restored for the first deferred test, with both hashes
verified and the exact inverse full INI diff. Restore archive ends233449-390590.
Active install is234, NOT237. Canonical record: build/playtest-candidates/installed.json.
No new game launch or game log exists for either pending candidate.

Final checks:12 x86 standing-arc regressions,39 parent math/ownership checks,
13 camera-scope checks, Release build, lint, nine exports, golden INI and standalone
XR60 frames FOCUSED/zero errors pass. Headset acceptance remains pending.
