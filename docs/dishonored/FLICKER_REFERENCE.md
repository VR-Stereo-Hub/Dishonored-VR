## VR-129 rune ownership and VR-87 pitch-height report (2026-09-17)

Surface routing: rune inner artwork on another HUD plane is HUD ownership;
smooth pitch-correlated vertical translation is adjacent VR-87 camera ceiling,
not eye flicker. Build407 verified and logs archived under reported407.
Objectives accepted; rune inner child remains split. Derived child size/hierarchy
and new guarded association are in ENGINE_NOTES. No image-owned orientation,
stereo synchronization, menu lifecycle or hand changes.
Camera accounting reproduces cap-related vertical loss in both stances; expose
existing EyeClamp in F10 Comfort, retain enabled policy for rune launch. Neck
retuning would not remove the cap term and is not attempted. No visual fix
claimed for either new trial. Next launch asks only if rune artwork stays united.
Then compare capsule limit on/off in open space with both stances, retaining
collision/vent regression scope before accepting any camera policy change.

# Flicker reference: symptoms, fixes, evidence, and investigation guide

## VR-129 native marker boundary trial (2026-09-16)

Surface: objective HUD ownership, following the existing build399 routing row.
Task-only parent inset candidate leaves camera, stereo, image orientation, hands
and menu lifecycle unchanged. Whole icon/text D3D ownership remains unproven;
see ENGINE_NOTES native task-parent derivation. No visual acceptance claimed.

## VR-129: build399 accepted parts, remaining objective transfer (2026-09-16)

Verified399 DLL/hash/banner, logs and saved INI archived at
build/playtest-candidates/vr129-wheel-parts-rounded-wrists/reported399.
Side-panel separation and rounded wrists accepted. Objective marker improved
but sometimes transfers onto window: classify as HUD ownership, not evidence
of a new stereo or image-orientation fault. Existing sampled logs cannot identify
the reported transition. New topology-limited miss logging records non-native
capture candidates without modifying routing; see HUD_ANCHORS. Rare wheel-exit
zoom remains open from the previous report; no new result for it this run.
Shared side-alpha controls retain the same image-owned source and existing fence.


## VR-129: build397 report and shared-image side panels (2026-09-16)

Installed399 (`vr33-hands-working-399-g489cca700`), clean source489cca700.
Release build and9 undecorated exports pass. Candidate:
build/playtest-candidates/vr129-wheel-parts-rounded-wrists.
Both397 logs, prior DLL and full INI archived before install at
build/playtest-candidates/installs/20260916-215151-048274.
Full INI comparison: NativeGameplayReference1->0; add WheelSidePanels=1,
RoundedWrist=1, RoundedWristDepth=0.350. No other byte changes. Installed DLL/INI
hashes and CRLF verified. DLL SHA256
`ecbb33f57672bc531140b7cff6a7461023c4bf10ec329f84f02d1a9a7b4921f7`.
Current log remains397 until tester launch.399 has not been headset-tested.
No game/simulator launched, no push/PR/merge; local commits only.

**Reported/measured:**397 DLL/hash/banner verified, both logs and unchanged INI
archived at build/playtest-candidates/vr129-native-reference/reported397.
NativeGameplayReference remained ON for345 gameplay log samples; no toggle event.
Objective rotation was reported in native gameplay too, but the ON/OFF comparison
did not run. Restore0 for ordinary play; no yaw/roll fix is established.

**Surface routing:** the earlier full uncropped wheel flash remains provisionally
absent. Occasional one-frame zoom remains OPEN and unlocalized: world, HUD or hands
must be identified before choosing a stereo, projection or palette hypothesis.
This candidate does not change those paths or extend the closing lease.

**Implementation:** new D-pad/potion panels sample the same fenced delayed source
as the circular wheel, before its mask, and share its visual lifetime. Two small
D3D11 crops add no D3D9 copies or independent image ages. 101 HUD control checks,
45 menu checks and the production WARP crop/mask/hue checks pass. No game or
simulator launched; visual source completeness and wrist shading are untested.
Continue from HUD_ANCHORS's one side-panel question; do not claim this resolves
the rare exit zoom or reopen settled image-owned orientation changes.


## VR-129: build395 closing result and remaining zoom (2026-09-16)

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

395 DLL/hash/banner verified; logs and unchanged full INI archived at
build/playtest-candidates/vr129-hud-fixes/reported395. Full uncropped wheel flash
is reported absent so far with WheelCloseAnimation=1; provisional acceptance,
not proof over every exit. Preserve the250ms native animation lease and delayed
tail. A separate occasional one-frame apparent zoom is reported at wheel exit.
Surface is not yet isolated to world, HUD or hands. Route by section1's surface
identification step before choosing projection, mono or palette changes. Existing
rate-limited logs cannot correlate a specific visible frame; do not call it a
measured FOV change or lengthen the visual lease as a speculative fix.

Objective complaint is predominantly head yaw/position, not a confirmed roll bug.
395 upright basis accepted102/104 logged samples; the roll-only test did not test
the main complaint. Roll acceptance remains unknown. NativeGameplayReference
isolates normal gameplay HUD routing/transforms from native target projection;
menus, existing image-owned orientation, pair synchronization and hand correction
are preserved. Standalone44 native HUD checks cover successful-draw health,
failure/refusal, expiry, menu exclusion and shader-state bypass. No new stereo or
menu-exit camera behavior is proposed. See HUD_ANCHORS newest entry for the one
launch question and next steps. The tiny exit zoom remains OPEN and separate.

## VR-129: wheel exit flash still open (2026-09-16)

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

Surface: full uncropped wheel HUD briefly appears on the general plane at exit;
route via HUD capture/lifecycle, not whole-world mono or hand palette correction.
Build391 DLL/hash/banner verified and logs archived at
build/playtest-candidates/vr129-hud-fixes/reported391. Previous apparent closing
acceptance is retracted by the new report. Existing native closing +three-present
visual lease is insufficient in at least one observed case.

Offline native Scaleform inspection establishes250ms closing fades for wheel,
background, D-pad and potions. Three presents are only25ms at120Hz or50ms at60Hz.
Candidate WheelCloseAnimation keeps visual ownership at least250ms from input
release and three presents beyond last observed native closing, while new menus
supersede immediately. The causal timing mismatch is a hypothesis, not a measured
frame-correlated cause; the old log lacks timestamps of the visible flash itself.
45 production menu tests include high-FPS early expiry as a negative control.
Potential cost: gameplay HUD can remain cropped briefly during the animation tail.
No input/scene permission extension, image retagging or hand/stereo policy change.

Objective head-roll correction is separate: transform only native HUD geometry
around its marker pivot using validated current-render perspective basis; old yaw
solver refusal is not claimed as basis validation.32 native HUD checks pass;
headset result pending. Both new controls are default off and live in F10. Next
launch asks only whether the objective stays world-upright during head tilt.

## Merge disposition (2026-09-16)

Current HUD follow-up accepted as sufficient for merge by the maintainer. This is
aggregate acceptance, not proof that pause hand-size variation is fixed.391 DLL
hash/banner verified and both logs archived locally at the merge checkpoint.
Keep measured389 pause-world acceptance and the open hand-depth distinction below.
Menu exit and objective-label candidates retain their documented limits. No further
headset launch is requested as a merge condition; no game or simulator launched.


## Build389 result: pause world accepted, hand depth still open (2026-09-16)

Installed391 (`vr33-hands-working-391-g56b422dc2`), clean source56b422dc2.
Candidate `build/playtest-candidates/hud-menu-exit`; both logs, previous DLL and
full INI archived in `build/playtest-candidates/installs/20260916-155406-323310`.
Entire INI diff: only add Hud.MenuExitHeading=1 and NativeObjectiveLabels=1.
All prior saved values retained; installed DLL/INI hashes and CRLF verified.
DLL SHA256 `7a0779f52716a029778db0511122dda7dbf08636e7c241cd00359108396b2126`.
Release build,9 exports,38 menu/23 native HUD checks, default writer/package/golden
byte parity, lint and diff checks pass. No game/simulator launch. Current log is
still389 until tester launch;391 headset acceptance pending. Local commits only.

Identity: build389/source dc3ffee45, DLL SHA256
`0488e78c3136e4dbce49015018899b87e26f2d896676a514af1410033198efc8`;
installed DLL/banner verified, full INI unchanged. Both logs archived at
`build/playtest-candidates/hud-visual-lifecycle/reported-menu-exit`.

Surface/route: prior WHOLE-WORLD pause scale pulsing belongs to section1's mono
interruption row. Reported fixed with PauseSceneFreshness=1. Seven beat intervals
more than3.1s inside pause episodes show zero mono; note5 and wheel2 likewise zero.
These are interval counts, not frame counts or synchronized perceptual events.
Keep the accepted gate exception. This does not establish all menus on all scenes.

Remaining symptom is smooth HAND/WEAPON size change during paused head yaw, not
reported world mono. Source palette scale samples0.999512 with zero logged
anisotropy/orthogonality error; sampled weapon lens ratios approximately1.000000
with no applied lens correction. Applied delta scale and actual corrected depth
were not logged, so neither model scaling nor projection depth is established.
Code inspection rejects naive linear matrix blending as an explanation: the
existing animation blend uses rotation slerp and separate scale interpolation.
No evidence to revive lens experiments or alter the accepted hand correction.

New read-only menu/hand-depth measurements distinguish those possibilities:
changing deltaScale supports transform scaling; stable scales with varying clipW
supports depth change; stable scale/depth requires image/pass correspondence next.
Samples carry hand, eye, present and pose identity and are rate limited; they are
not a complete census. Hand-size fix remains OPEN. Keep VR-128 wheel residual
parked as requested; this is a separate pause observation.

Menu exit snapback is a camera handoff boundary: scoped head look restores native
entry rotation after each draw, while blocked gameplay updates its previous head
sample. On exit it therefore has no accumulated menu delta to apply. Default-off
MenuExitHeading carries that delta once through the existing script writer, after
fresh live-table and retained-owner validation. No camera offsets or eye policy
change.38 menu host checks pass; headset transition is pending. Next one-question
launch checks whether pause exit retains current direction. Refusal/snapback and
duplicate/overshoot are both logged/testable failure outcomes. Accepted orientation,
pair synchronization, pause freshness and menu hand half-step remain unchanged.

## New387 result: accepted wheel hands, recurrent pause mono (2026-09-16)

Installed389 (`vr33-hands-working-389-gdc3ffee45`), clean source dc3ffee45.
Candidate `build/playtest-candidates/hud-visual-lifecycle`; both previous logs,
DLL and INI archived in `build/playtest-candidates/installs/20260916-152612-084229`.
Entire INI diff is six added keys only: PauseSceneFreshness=1 and five PauseAlpha
values matching saved general alpha (repair,1,0,1,1). Every existing value retained.
Installed hashes and CRLF verified; release build,9 exports, lint and listed host
checks pass. DLL SHA256 `0488e78c3136e4dbce49015018899b87e26f2d896676a514af1410033198efc8`.
No game/simulator launch. Current log remains387 until tester launch; no headset
acceptance claimed. Local only; no push or merge.

387 DLL hash/banner verified; both logs and unchanged saved INI archived at
`build/playtest-candidates/vr128-menu-half-step/reported-pause-scale`.
Reported: wheel hands/weapons sufficiently improved to park; preserve menu half-step
recognition. Two sampled wheel mismatch lines remain; unlike385's20 mixed samples,
these do not share controlled exposure, so do not infer a percentage improvement.
Pause now has recurrent WHOLE-WORLD size/depth changes, not just hand flicker.
Route via section1's mono interruption and menu stereo/projection transition rows.

Measured intervals >3.1s inside a riding episode: pause11 beat intervals,7 nonzero
mono;22 rate-limited150ms cap-expiry lines,21 sampled camera-silent gate lines and
9 present-stall gate lines. Wheel10 intervals,2 nonzero mono; note5,zero. Sampled
counts are not frame counts or timestamped perceptual events. Example12784390
returns DOUBLE after114 single ticks during pause. User's observation is compatible
with repeated mono fallback; it is not proof of FOV changes or swapped eye geometry.

Code boundary: SceneDrawDecide compares current c5 upload serial to the serial
saved at the END of the previous draw. If camera uploads happen during that draw,
with none in the idle interval, this evidence of a live scene is discarded. Prior
historical pause acceptance does not establish this gate for the current schedule.
New default-off Hud.PauseSceneFreshness trial records serial advancement during
completed gameplay-dispatch draws. Only pause context3 with head look may use it,
only for100ms, and all session/ownership/scene/present-stall/camera safety guards
still apply. Silent draws do not renew evidence; context exit clears it. No forced
scene permission, indefinite compositor hold, pixel retagging or pose-policy change.
This is a falsifiable gate hypothesis, not a confirmed correction: pause/scene logs
report prior-draw-upload age and whether the exception was used. If ages stay stale,
the hypothesis is not exercised and the upstream scene schedule remains open.

33 menu host checks include recent/stale/unseen uploads, disabled option, wrong
context, head-look off, clock rollback and context reset. Accepted image-owned
orientation, pair synchronization,150ms gap hold and hand correction unchanged.
ONE launch question: does the paused world's size/depth stay stable for about20s,
including slow head turns? Stable supports the gate exception; unchanged requires
age/mono comparison; worse rejects it. No game/simulator launch. Headset pending.

## New385 result: measured left-eye menu hand misclassification (2026-09-16)

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

Verified385 DLL hash/banner; both logs and INI preserved under
`build/playtest-candidates/vr127-opening-hud/reported-transitions-tint`.
20 rate-limited mismatch lines:15 wheel(context6),5 pause(context3), ALL handEye+1,
completed drawEye-1, decision S. Positive measured jumps2.291..2.831uu fall just
below .45*6.309=2.839uu. Sampled placements3/3, refusals0/0, weapon hits11..15,
misses0. These are sampled mismatches, not a complete visible-flicker count.
Frequent menu center/eye transitions are approximately half-IPD. The current
same-eye band can hold a right-eye correction on a completed left-eye draw.

Candidate Hands.PaletteEyeMenuHalfStep=1 lowers only menu contexts3..8 to .25IPD
(the midpoint between zero and half-IPD). Sign still selects the eye; no guessed
alternation, queued-draw retagging or camera changes. Gameplay keeps .45IPD.
Ten recorded jumps replay correctly; old threshold controls reproduce the wrong
hold. Stationary menu and gameplay controls pass;41 palette checks total pass.
This remains a heuristic susceptible to head-motion false crossings, NOT an
established flicker fix. Old VR95 predictor/alternation settings remain OFF;
accepted image-owned orientation, synchronization and gap hold are unchanged.

User confirms pause issue is minor hand flicker, not proven mono. Wheel residual
is left-eye hands/weapons only. Do not label either corrected without headset data.
ONE launch question: with wheel open and controllers still, do slow head turns
keep left-eye hands/weapons stable, relative to a still-head baseline? Stable
supports half-step recognition; unchanged requires new completed-eye comparison;
new still-head flicker rejects the threshold. Logs remain agent-inspected.

## New382 result: wheel hands and pause interruption (2026-09-16)

**Installed385** (`vr33-hands-working-385-gd6abf293f`), clean source d6abf293f.
Candidate `build/playtest-candidates/vr127-opening-hud`; prior logs/DLL/INI archived
at `build/playtest-candidates/installs/20260916-121620-467921`. Entire INI comparison:
exactly one new key, ObjectiveScreenTracking=1; all saved values retained. DLL SHA256
`d929115f9bfc117d1d5493cefd6c2baafa2a6159b400713fca99217af9201a02`. Hashes/CRLF verified.
Release build,9 exports, lint, byte-identical default writer/golden and listed host
checks pass (also404 pair-policy and17 menu-scope checks). No game/simulator launch;
current log still382 until the tester launches385. Headset result pending.

Build382-g048c1e461 DLL hash/banner verified. Both logs and full saved profile:
`build/playtest-candidates/vr127-hud-fixes/reported-crouch-pause`.
Report: residual left-eye wheel flicker, new pause flicker after enabling pause
head look/world anchoring. Pause image surface is unconfirmed; clarification asked.
Route wheel to section1's hand/weapon eye/correction rows. Pause also requires
the mono-interruption row until the visible surface is known. No perceptual fix claimed.

Measured beat intervals fully inside riding episodes (over3.1s from entry):
wheel11 samples,0 with nonzero mono; pause54,15 nonzero; note52,4 nonzero.
Sampled150ms cap-expiry lines inside those episodes: wheel0,pause29,note10.
Held-gap samples: wheel32,pause138,note156. These are rate-limited intervals,
not counts of visible flickers.378's no-mono reading result does not clear the
new pause workload. Menu restore refusals remain0 in sampled scope logs.

The150ms bound explicitly permits fallback after longer gaps. Increasing it
indefinitely would freeze world updates; forcing doubling past liveness guards
would abandon accepted synchronization. Neither is included. Global FOV/eye-tag
changes cannot be justified by this evidence. Wheel's zero-mono sampled intervals
mean mono-gap hold alone cannot be presented as its remaining hand fix.

Hand diagnostic totals at the last sample:50382 agreements,155 mismatches,
25113 unknown,65562 refused draws and589067 unassociated weapon candidates.
These were cumulative across ALL menu contexts. Missing anchors among generic
weapon candidates do not prove that a visible weapon missed correction. The
1Hz healthy sample can hide a mismatch earlier in that interval. New read-only
telemetry separates context and completed draw-eye bucket and explicitly emits
mismatch samples with placement/refusal/hit/miss fields. Existing deferred join
N hand draw to completed present N+1 is preserved and host-tested; predictions
and image normalization experiments remain off. No new flicker correction.

Recoverable next steps (VR-128): identify pause surface; collect new context/eye
populations on the next ordinary HUD playtest. If wheel has known left mismatches,
inspect their render-owned geometry/pose before changing the classifier. If known
eyes agree but placement refuses, trace that refusal; if corrections succeed,
compare the hand pose to the image-owned pose. For whole-world pause interruptions,
join cap expiries to scene gates before proposing a render-scheduling change.
HUD corrections/test question are in HUD_ANCHORS's current VR-127 section.

## Accepted menu stability; residual left-eye hands (2026-09-16)

Build378-g9bce8a13b DLL/banner verified. Logs and latest saved INI archived in
`build/playtest-candidates/vr126-hud-owner/accepted-profile`. Headset reports no
flicker while reading notes/books and most wheel flicker removed. Remaining symptom
is positional flicker on hands/weapons, apparently left eye only and only during
physical head movement. User authorized merging the accepted dial/menu work.

Measured:36 note and10 wheel beat intervals more than3.1s inside their episodes,
all with zero mono output (376:58/72 and17/26 respectively had mono).178 sampled
menu gap-hold messages, one no-recent-stereo message across the entire session.
This supports the bounded hold addressing menu mono interruptions. It does not
prove every hand draw is corrected; cumulative deferred hand totals still include
unknowns and several contexts. No new world-scale failure is reported.

PR69 merged at0afbadc83. The subsequent VR-127 HUD alpha/input/grouping candidate
preserves all accepted camera/stereo/hand settings and introduces no new hand correction.
Residual is VR-128, Backlog. Route to the hand/weapon eye-identity/correction rows,
not another FOV change or global stereo retag. Preserve the accepted menu translation,
image-owned orientation and pair synchronization. Next investigation should isolate
Wheel context and left-eye hand draws with image-owned evidence. No new hand
correction is included in this merge. HUD ownership/reading UI follow-ups are VR-127.

## Menu depth interruption follow-up (VR-126, 2026-09-16)

**Installed378** (`vr33-hands-working-378-g9bce8a13b`), clean source9bce8a13b.
Candidate `build/playtest-candidates/vr126-hud-owner`; preinstall logs/DLL/INI in
`build/playtest-candidates/installs/20260916-023823-085684`. Whole INI comparison:
exactly two new keys, NoteHandRight/JournalHandRight=0.200; every existing value
retained. DLL SHA256 `9b6135b6e14b90a9e54672d7e6ae49ad9c18eba275d726b1027a457501123e1b`.
Installed hashes/CRLF verified. Release build, exports, lint, golden INI and regression
checks pass. No game/simulator launched; current old log remains376 until the tester
launches378. Headset result pending. Local commits only.

**376 verdict:** camera sliding when turning appears fixed. Notes follow the hand
well. Residual wheel flicker includes hands/weapons and a newly reported brief
whole-world enlargement/eye misalignment. Do not classify the entire fault as hands.
Do not undo the accepted composed-yaw/coherent-position correction.

**Evidence:** installed376 DLL hash/banner verified before reading. Both logs and
current INI are archived in `build/playtest-candidates/vr126-menu-motion/reported-scale-routing`.
Across intervals fully inside a riding episode (more than3.1s after entry),17/26
wheel and58/72 note stereo beat intervals report nonzero mono output. Example:
66320046 reads61L/s,61R/s,17mono/s and40none/s. The existing count hold expires
at the fourth consecutive untagged delivery; center-eye images can then show in
both eyes even though this screen is intended to retain stereo. This is a concrete
scale/depth interruption, not proof that it explains every reported flicker.

FOV audit changes only at startup/pause transitions (108.06/103); no logged
fovMismatch=1 supports another live FOV-slider bug during Wheel. The pair-geometry
probe sometimes reads halfIPD (3.41/3.42uu vs6.81 at66674000/66678000), but joins
current render c5 to pipelined delivery, so it is NOT image-owned proof of a bad
submitted pair. No pass2 write or menu restore refusal was logged. Whole-session
hand mismatch/unknown counters mix menu contexts and cannot clear the hand classifier.
Earlier same-present hand comparisons were invalid; retain the corrected deferred join.

**Targeted correction:** while a head-tracked screen rides the HUD, suppress delivered
center/untagged images for150ms after a tagged stereo image. Submit the existing pair
instead through the established no-output/compositor hold. Never relabel pixels,
change pose records, bypass scene gates or change the accepted eye synchronization.
After150ms the existing three-present fallback applies; a genuine context exit
immediately bypasses the extra hold. Reset/disarm clears history. Logs identify held
gaps and cap expiry. A long stall may still reach mono; increasing the cap indefinitely
would freeze live head rendering and is not a solution.

**Validation:** production policy host cases cover both eyes, 149ms gaps,150ms expiry,
real context exit, clock rollback and reset. Reentry harness404 checks passes.
No headset acceptance yet. The world may stabilize while hand/weapon flicker remains.

**ONE next launch question:** with Wheel held open and controllers still, do slow
head turns still make the whole world briefly enlarge or lose eye alignment?
Expected: stable world scale/depth. If stable, short mono interruptions were a contributor.
If unchanged, read held/cap diagnostics and trace image-owned camera geometry next;
do not substitute the hand classifier or current-c5 telemetry as proof. Hand-only
flicker is a separate remaining surface. HUD routing/readers are available but are
not additional acceptance questions for this launch.

## Menu head-motion follow-up (VR-126, 2026-09-16)

**Reported:** refined wheel appearance and selection are accepted on374. Moving the
head while Wheel is open causes flicker, possibly both eyes; stationary view is
stable. Left/right head turns also appear to move the viewpoint in menus and
cinematics. Later clarification identifies hands/weapons as the likely flickering surface.
Route to the shared hand correction/eye-identity rows (3.11 and VR-116 hand follow-up),
not a claimed world or wheel-panel regression. The positional slide is separate.

**Identity/evidence:** log banner374-ga51e1799f and installed DLL SHA256
5ed4b0c9a840d14aae28304cff1336d791f2035d749dd7454c8638b2d3ee977a match.
Both logs and saved INI are preserved in
build/playtest-candidates/vr126-dial-immersion/reported-head-motion.
There are537 sampled successful menu scopes,274 sampled waits with scene=1/double=0,
and zero menu restoration refusals. These rate-limited lines are NOT tick counts
or visual correlations. Final image-orientation counters: left59508 accepted/0
fallback; right59506/1. Existing accepted image-owned orientation is active.

**Code findings and candidate:**

1. MenuHeadBegin required doubleDraw, unlike the established cinematic scope.
   Single scene draws therefore reverted to native untracked orientation while
   adjacent pairs used head look. Scope single draws too, with eye0 and their own
   exact pose record. Do not force doubling or change tag repair/hold/synchronization.
   Log double/singles counters so exercise of the corrected route is observable.
2. Scoped rotator writes did not update the native camera matrix rows. Position
   offsets were in CURRENT physical head-yaw axes but mapped through those native
   rows. At a nonzero tracked displacement, a yaw turn could rotate the offset
   despite no physical translation. Menu/cinematic scopes now map translation
   through their composed yaw. Stereo separation still uses composed full right.
   Ordinary gameplay and pitch-only scopes retain their accepted mapping.
3. Menu entry offset subtraction mixed vectors from different head-yaw frames.
   Preserve only entry neck correction, rotate it into the current yaw frame,
   then add current raw displacement. Translation and orientation now share one
   HtSample publication; cinematic scopes consume its raw position too. No new
   engine offsets or unchecked writers. Existing live identity/restore guards stay.

**Validation:** production menu module17 checks, including formerly refused single
draw;42 cinematic math checks including a361-angle fixed-position sweep and an old
native-basis negative control exceeding10uu false travel;16 production scope
restoration checks.33 HUD anchor checks,20 routing checks,2185 dial checks and30045
FOV/handback checks pass. Release build, exports, lint and golden INI checks required
before install. No game/simulator launched. Host evidence is not headset acceptance.

**Failed/limited hypotheses:** absence of restore refusals does not prove correct
pixels. Healthy same-eye image metadata does not establish correct camera translation.
Single-draw gating is a code defect with observed exercise, but the old capture hold
may reject some affected images, so it is not yet a proved cause of perceived flicker.
The saved global alpha could affect the wheel independently; separate transparency
controls are a UI change, not evidence for a world-flicker fix. No global blur/DOF,
lag, resolution, mirror or palette experiments enabled.

**Hand-specific follow-up:**374's palette eyecheck has zero agreements/disagreements
and only unknowns. Source review found it asks for present N+1 WHILE drawing at N,
before that record exists. This cannot clear the classifier. Move that read-only
comparison to N+2, joining the hand history at N to completed draw identity N+1.
Nineteen production eye/diagnostic host checks pass, including completed-record
agreement, mismatch, untagged and future-record negative controls.
Bounded menu/hands telemetry reports known/unknown, classifier decision/jump, actual
resolved draw eye, hand refusals and weapon correction misses. No phase fitting,
queue mutation, predictor activation or parked hand-normalization patch is used.
The camera single-draw correction removes an inconsistent input to the shared
hand transform; it remains a candidate for the reported hand symptom, not proof.

**Next launch question:** with Wheel held open and controllers still, does turning
the head left/right stop the hand/weapon flicker? Expected: hands stay stable while
the wheel remains usable. Improvement supports consistent menu camera inputs;
unchanged flicker calls for menu/hands known mismatch/refusal populations and pose
timing before another rendering change. Worse rejects this candidate. Positional
slide, reading panels and cinematic comfort remain separate acceptance scopes.

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

## Current menu-world investigation (VR-126, 2026-09-16)

Reported on verified build372: weapon wheel and notes expose a stationary world FOV
rectangle when the head turns. Surface is the WORLD behind a riding HUD menu, not
hand/weapon settling. Existing paused-render evidence plus explicit UiSurfaceBlocks
camera-writer gates support a frozen camera; the report alone does not establish
stopped rendering. Added a distinct symptom-routing row below.

Installed374 (`vr33-hands-working-374-ga51e1799f`), clean source a51e1799f.
Archive: build/playtest-candidates/installs/20260916-010814-146048.

Candidate implements menu-relative draw-scoped head look, exact sample publication
for both eyes, guarded restoration and entry-relative physical translation. It
preserves accepted image-owned orientation and stereo synchronization. Default-off
per-menu toggles are enabled for Wheel/Note in the installed test. No perceptual fix
is claimed yet. Lifecycle/scoped-camera host checks pass; game/simulator not launched.

Menu gray blur is separate: a reflected UI-only blend-weight candidate, not an eye
synchronization change. Full evidence, false leads, settings and next single launch
question: [HUD_ANCHORS.md](HUD_ANCHORS.md), current VR-126 refinement section.

## Current VR-50 follow-up: Display-tab FOV pulsing (2026-09-15)

Build359 fixes a code-confirmed idle F10 Display writer; headset confirmation is pending.
This is whole-view zoom-like flicker after using resolution Set, not a recurrence of the
accepted image-owned orientation fix. See the new VR-50 entry at the end of this file.
The measured resize happens once and holds its dimensions; FOV metadata alternates.


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


## HEADSET-CONFIRMED BREAKTHROUGH: world smoothness at 120 Hz

2026-09-14, VR-116. Build **271-g8cd27652** is the preserved known-good
world-smoothness checkpoint. The tester reports an exceptional improvement in
world stability during physical head movement, including during substantial
frame drops. This is the strongest reported world-smoothness result to date.
The remaining slight flicker is confined to hands/weapons and is NOT a failure
of the accepted world result. Preserve this baseline before hand changes.

Verified log banner: compiled Sep 14 2026 13:46:59; installed DLL SHA256
`d3853fb75b71d4cbbdcceea2281b17664939c306290b879918e5d6f40eca5102`.
Exact DLL/INI bundle: `build/playtest-candidates/vr-116-image-orientation`.
Both run logs and INI: `build/flicker-120/accepted-world`.
`Pace.ImageOrientation=1`, world lag 2 and hand lag 2 retained.
Final sampled counters: left accepted 38258/fallback 0; right accepted
38253/fallback 2. These are application counters, not perceptual measurements.

**Preserve the image's own head orientation, not a guessed fixed history age.**
The headset result supports this principle for rotational world reprojection;
it does not establish higher frame rate or positional reprojection correctness.
The old fixed-lag-only diagnosis is superseded for this reported world symptom.
Next: apply image-specific head normalization to shared hand/weapon corrections,
with a separate default-off toggle and the accepted world behavior unchanged.
No merge authorized. Historical pending-test statements below describe earlier work.


Compiled 2026-09-13 against committed source `6cb3f363b8dcea0799ce86e98f8753f73ebb7b18`.
This is a new reference assembled from repository documentation, implementation,
and locally available commit history, including parked branches. It does not
change the renderer or replace the historical records.

There is no single flicker bug. The project has used that word for stale eyes,
mono interruptions, black frames, desktop eye switching, uncorrected weapon
passes, missing weapon corrections, and camera-writer interference. Start by
identifying the visible symptom and the image surface before choosing a fix.

Evidence labels used here:

- **Confirmed**: a documented perceptual result plus the associated implementation
  or measurements. Confirmation applies to the recorded build/configuration/run.
- **Measured**: a code path or log population was observed; this alone does not
  prove a visible cause.
- **Open**: no later resolution found in the reviewed record.
- **Historical/parked**: an experiment or finding outside current mainline.
- **Retracted**: a conclusion was explicitly corrected by later evidence.

No new game launch, headset test, installation, or live configuration change was
performed for this document. Existing run statistics below are attributed to the
records that contain them, not presented as newly reproduced measurements.
Concurrent working-tree camera/Z-account changes were present during the review;
they are outside this committed baseline and are not assigned a flicker verdict.

## Contents

2026-09-14 performance candidate: [DESKTOP_PRESENT_PERFORMANCE.md](DESKTOP_PRESENT_PERFORMANCE.md)
records VR-115's default-off reduced/off desktop modes. This touches the desktop
alternation and delayed-tag rows below, not a new headset flicker diagnosis.
Current-draw identity still controls the Reduced pin; Off omits desktop delivery
after capture and invalidates old pin provenance. Host and standalone D3D9Ex
GPU tests pass; headset/engine pacing, menu/load and visual acceptance remain
open. Candidate is not installed; accepted build266 remains in place.

1. [Symptom routing and current status](#1-symptom-routing-and-current-status)
2. [The frame path and its identities](#2-the-frame-path-and-its-identities)
3. [Detailed issue history](#3-detailed-issue-history)
4. [Failed approaches and corrected readings](#4-failed-approaches-and-corrected-readings)
5. [Current controls and code map](#5-current-controls-and-code-map)
6. [Investigation and regression workflow](#6-investigation-and-regression-workflow)
7. [Commit and evidence index](#7-commit-and-evidence-index)
8. [Keeping this reference useful](#8-keeping-this-reference-useful)

## New reports and investigation: 2026-09-13

Two additional reports are under investigation in
[FLICKER_FRAME_DROP_AND_RESUME_PLAN.md](FLICKER_FRAME_DROP_AND_RESUME_PLAN.md).
That document contains the recoverable plan, exact source paths, preserved log
hash, measurements, implementation sequence, and restart checklist.

| New report | Findings as of this investigation | Status / next step |
|---|---|---|
| One-frame world ghost/doubled edges during fast head turns, suspected frame drops | Existing run has real timing gaps at 90 Hz/lag 2; no symptom marker joins them. Printed pose audit covers right only. Fixed-lag attribution and hold/release identity need event-local checks. | Open; two-eye capture/pose/release history before a rendering fix |
| About one second of weapon settling at startup and after mono menus, while swaps retain lock | `GameStateTick` destroyed contracts/candidates on every exit from GAMEPLAY, including MENU; the settle was a 0.5 s UI rescan hold plus 0.35 s of relearning | VR-93: B1 (`AttachKeepOnMenu`) confirmed on the headset for pauses, section 3.13; save-load negative control and B2 (the rescan hold) open |

The pause also queues a UI observer rediscovery before candidate/component
publication: a 516 ms scan coincides with a 538 ms resume gap. Plan this as a
separate lifecycle/cost correction. A temporary LOADING classification on menu
close is not proof of a real level change, and the startup scoreboard's interval
includes time the menu was deliberately open.

Caching can retain learned same-owner weapon identity; it must not retain stale
per-eye correction matrices, transforms, or object/resource pointers across real
destruction. A permanent cache used once per process is not established as safe.
The existing snapshot proves destruction/recollection, not successful post-menu
weapon re-adoption: its final contract count is zero, with no new adoption shown.
The user's reported eventual relock remains a separate perceptual observation.

No code fix, install, launch, or new visual test was performed. See the linked
plan for how to distinguish normal missed-frame delivery from incorrect image
pose metadata without reopening the disproved historical theories.

## 1. Symptom routing and current status

| Observation | First suspect / distinguishing evidence | Status in reviewed baseline |
|---|---|---|
| Hands/weapons flicker on head turns during Wheel; separate yaw-induced menu/cinematic translation | Scoped single-draw gap plus shared hand eye/pose inputs; translation-basis mismatch is a separate cause | VR-126 code/host corrections; headset pending, latest entry above |
| World FOV rectangle remains fixed while turning behind Wheel/Note | Menu blocks camera writers despite riding stereo; distinguish fixed camera from stale pair with scoped pose and capture identities | VR-126 scoped head-look candidate, headset pending |
| Desktop window alternates left/right views throughout stereo | Each eye draw reaches the game's Present; missing desktop pin | Original VR-53 pin implemented; later VR-76 correction confirmed |
| Single-frame rightward hand/weapon jump, clearest in desktop window | Current D3D9 pixels classified by a previous-present capture tag; single-draw bursts trigger raw leaks | VR-76 confirmed, `DesktopEyeSource=draw` default |
| One eye appears frozen, swapped, or behind after pause/load/rearm | Tag-ring skew, capture freshness, c5 arbitration, or one-sided tag generation | VR-80 late-tag repair confirmed; distinct reload R/0 capture repair headset-confirmed on build 215 (18:01:15), latest record below. Residual generation/timing remains open |
| Both near hands/weapons flash or lose disparity for a frame | Untagged mono image enters a stereo stream | `HoldUntagged=3` confirmed mitigation; burst generation remains open |
| Both eyes go black for one frame | Texture-less present ends an XR frame without a scene layer | Previous-layer fallback implemented and historically confirmed |
| Pause causes XR session loss | Hold overwrites saved layer with empty local structures; `XR_ERROR_HANDLE_INVALID` | VR-54 snapshot-bank correction implemented |
| Mono after loading until jump/crouch/stairs; hands precede weapon tracking | Capsule liveness depended on an event-latched pawn while the controller already possessed the player; separate startup discovery stalls | Headset-confirmed startup stereo; weapon freeze remains: [load startup](LOAD_STARTUP_IMPLEMENTATION.md); `PawnFromController` and separate `CacheNameLookups` ship off |
| Severe first-seconds flicker after loading, then stable | Startup eye starvation with asymmetric eye updates and slow ticks | Open historical startup issue (VR-16); brief settling accepted in later runs |
| Parts of both weapon models intermittently vanish only in the left eye, hands intact | Near-identity lens corrections measured on unzoomed draws; depth/colour agreement under investigation | VR-112 confirmed resolved on build264: identity bypass plus per-view inverse-lens consistency; latest acceptance below |
| Dark animated weapon copy at native position | Another render pass of the same geometry was not corrected | VR-33 pass identity/suppression fixes confirmed |
| Both weapons disappear together, hands still place | Shared correction/publication gate; overly tight snapshot age | 100 ms snapshot bound restored; rare single-frame refusal historically accepted |
| Weapon detaches or flicker returns after swap/load | Candidate list, contract lifetime/capacity, equipment roots, or config gate | Recovery/retention fixes landed; distinguish from eye-state regression |
| Persistent outward displacement in each eye after stability integration | Live script mono flag resets eye state for an older queued stereo draw | VR-69 render-side eye restoration confirmed |
| Flicker on crouch, downhill movement, or falls | Z clamp breaks ownership of an already-offset camera vector | VR-69 clamp reconciliation confirmed |
| About a second of weapon flicker after resuming from a pause, swaps fine | The menu ran the level-load transition: identity dropped and relearned, plus a UI rescan hold | VR-93, section 3.13. Relearning fixed behind `AttachKeepOnMenu`, the hold behind `UiKeepOnMenu`; both headset-confirmed for pauses, both ship OFF. Books are not covered |
| Sustained flicker after closing a note, worst crouched; the scene jumps right in the right eye and left in the left (an eye swap), or later left eye only | **A late tag**: a present shows a draw's image before that draw's tag reaches the ring, the ring runs one tag behind until a drain, and under `SharedWait=0` the unlabelled image is held out of its eye. Ledger signature `EMPTY REFUSE` then `TOOK` | **VR-80 fixed, headset-confirmed** behind `[Stereo] LateTagRepair` (the repair plus the capture-slot relabel). Section 3.15, "The solution". Why the push-to-present margin collapses after a crouched close is open (VR-99), as is an occasional single frame. Section 3.14 is the separate zero-`c5` case (VR-97) |
| Occasional single-draw bursts and held frames during gameplay | Present-progress guard and game/render scheduling | VR-77 open; VR-76 fixes its mirror consequence, not its generation |
| Object occluded in one eye vanishes from both | Stereo culling coverage | VR-79 open; adjacent visibility issue, not proven to share flicker cause |
| Doubled edges only on head turns | Cadence or pose-generation mismatch | Historical 90 Hz cadence result and later lag-2 fixes; diagnose separately |
| Arms/weapon jump sideways in ONE eye during a head roll | Palette eye classifier held the previous eye on an unreadable jump | VR-95, section 3.11. Cause measured and confirmed; the shipped correction is OFF and its own regression is open |
| Arms/weapon flicker while standing still, after enabling `PaletteEyePredictToggle` | The same correction firing on genuine repeats | VR-95 open; lever ships OFF, live A/B in F10 Hands |
| Stereo "reloads" (the world drops to the screen and comes straight back) on every pause-menu RESUME, and the same on the menu OPEN | The scene verdict falls for a few presents at both edges: on open the owner read publishes 50 ms after the menu flag, on resume the view pipeline is silent until its first dispatch; the runtime's 3-present fallback fires in the gap | VR-117: a ride stand-in (300 ms open gap, 1500 ms resume grace) and the HUD quads built after the hold path; simulator-confirmed (`pause-ride.xrs`), headset pending |
| The HUD flickers between the HUD window and the frame (both eyes, gameplay, about 10 Hz); `frame` mode does not | The HUD redirect's gate followed the per-present eye tag, and re-entry leaves 6 to 21 presents a second untagged by design (`none/s`); each one disarmed the redirect for the next present (`hud/beat presents=441 armed=400`) | VR-117: gate on the runtime's projection MODE (`dvr::hud::projection_mode`); headset-measured cause; the fix simulator-verified (`hud/beat presents=467 armed=467` in every 3 s window with `stereo: beat none/s=1`); headset-confirmed on the second run (2026-09-15): no window/frame flicker reported |
| Whole headset view repeatedly expands/contracts while F10 Display is open, noticed after live resolution Set | Legacy FOV control wrote zero every UI frame due to missing braces; raced the automatic FOV target, releasing the gameplay scope | VR-50 code cause and negative control confirmed; build359 installed, headset result pending; see latest entry |
| Whole view slides sideways when the head ROLLS (not a flicker) | Neck arc built from a rolled frame | VR-91 fixed, `[Neck] RollArc=0`. Listed here only so it is not mistaken for one of the above |

VR-78 crouched-pitch motion was fixed later with a measured zero crouched neck
pivot. VR-87 ceiling trimming and VR-91 roll-induced lateral motion are adjacent
camera issues in the latest status, not established recurrences of the downward
flicker. Do not collapse all crouch or head-motion complaints into VR-69.

**Status precedence matters.** [STATUS](../STATUS.md) and [ROADMAP](../ROADMAP.md)
record the later VR-76 success. The candidate headings in
[VR-76-CODEX-HANDOFF](VR-76-CODEX-HANDOFF.md), the tail of
[DESKTOP_MIRROR](DESKTOP_MIRROR.md), and the original architecture decision still
contain pending-test language. Those are historical stages, not the final verdict.
Likewise, [KNOWN_ISSUES](../KNOWN_ISSUES.md) and
[TROUBLESHOOTING](../TROUBLESHOOTING.md) include early settings/status that must
not override today's implementation or a later controlled result.

## 2. The frame path and its identities

The active architecture is native D3D9 through a proxy, with captured D3D11
textures submitted by the OpenXR layer. The removed DXVK/SBS implementation is
historical; its imported driver commits are not evidence about this frame path.

```text
Game/script lane
  viewport draw root -> decide this tick's gates once
    pass 1: left camera, queue draw and tag
    pass 2 if allowed: right camera, queue draw and tag
    single gameplay draw: explicit untagged entry
             |
             v  engine queues render work
Render/present lane
  native draws / palette and weapon corrections -> current D3D9 backbuffer
  reentry ring + c5 arbitration -> resolved current-draw eye
      |                                      |
      | capture slot + its eye/pose identity  | desktop current-eye publication
      v                                      |
  delivered D3D11 texture + delivered tag     |
  XR eye textures / pair / swapchain release |
  post-capture desktop hook <-----------------+
      -> snapshot left or re-blit held left into game window
  XR submission or previous-layer fallback
```

The diagram shows ownership, not a promise that every call reaches the same
tail: no-frame, first-eye pairHold, and normal runtime paths have separate
post-capture mirror hooks.

### Identities that must stay separate

| Identity | What it actually names | Common wrong substitution |
|---|---|---|
| Game tick / pass decision | Work being queued by the game lane | Latest global state used to identify an older render draw |
| Current-draw eye | Pixels currently in the D3D9 backbuffer, classified by the method | Delivered capture tag |
| Delivered tag / capture serial | Pixels returned by the capture slot | Current c5 or current backbuffer |
| Hand/weapon correction | Correction for a particular present and eye | A new, unused pose publication treated as invalidating the correction |
| Submitted pose generation | Pose associated with the image being submitted | Newest available head pose without checking image age |
| Saved XR layer | Handles, views, poses, and layer description | Immutable saved pixels or a guaranteed completed pair |
| Desktop held surface | Successful left snapshot belonging to this resource lifetime | Lifetime snapshot count used as proof a recreated surface is initialized |
| Weapon geometry contract | Identified geometry/range/pass and component association | Proof every instance using those buffers is the held weapon |

Capture timing is decisive: sync and shared with `SharedWait=1` deliver the
current frame when successful; deferred and shared with `SharedWait=0` deliver
previous-present pixels. Startup, reset, mode changes, or failures can deliver
nothing. A mode name alone is insufficient to infer latency.

c5 is downstream evidence about the rendered camera, not simply the last value
the script writer requested. Conversely, a correct c5/eye label does not prove
the final pixels or desktop selection are right. The current-draw classifier is
still a classifier, not an independent pixel identity measurement.

Sources: [capture.cpp](../../src/core/gfx/capture.cpp),
[reentry.cpp](../../src/core/gfx/reentry.cpp),
[scene_draw.cpp](../../src/game/dishonored/scene_draw.cpp),
[runtime](../../src/core/vr/openxr_runtime.cpp),
[mirror handoff](VR-76-CODEX-HANDOFF.md).

## 3. Detailed issue history

### 3.1 One-sided generation, stale delivery, and shared-slot races

Several different defects preceded the later flicker investigations:

- **Pass-2 gates changed after pass 1 had already tagged left.** Pass 1 and
  pass 2 evaluated different gate sets/times. Resume catch-up and loading state
  changes could produce left tags without a right sibling. `813807e3` moves
  the decision to depth zero before pass 1; pass 2 consumes that decision,
  retaining its exception/poison handling. Healthy game-side gate counters
  cannot exclude a fault introduced later in capture or arbitration.
- **No new capture, old tag pushed again.** Capture mode switches, Reset, and
  capture-off could leave the old texture available. `8020855a` prevents
  treating that old image as a newly delivered tagged frame. `tagNoFrame` names
  this population. A non-null texture pointer is not proof of fresh delivery.
- **A shared slot could be overwritten while D3D11 still read it.**
  `230ac120` adds the consumer read-completion fence before the next D3D9 write
  into that slot. `read_done()` and read-wait counters are the source route.
  Producer completion alone did not protect the consumer's use of the pixels.
- **The pace guard could consume an eye without a frame.** Earlier records name
  the eaten-tag owner with `eatenNoFrame` / `eaten` on stale-eye diagnostics.
  Include that owner when a tag exists upstream but no image arrives downstream.

Sources: [KNOWN_ISSUES](../KNOWN_ISSUES.md),
[RELEASE_NOTES](../RELEASE_NOTES.md), the commits above, and active capture/reentry.

### 3.2 Ring-order skew and the fragile c5 arm

Pairing solely by push/pop order broke when the game lane ran ahead, including
single-to-double transitions, rearm, pause/load, and ordinary gameplay.
`c8cfe107` compares the ring claim with the measured camera step. Between a
tick's two draws there is no intervening world tick, so the second draw's c5
step is one eye separation along right under this code's sign convention.

The later defect was trusting both directions of that inference equally:

- `inv=+1` is the within-tick pass-1 to pass-2 comparison.
- `inv=-1` reasons across a world tick and can be fooled by gentle player motion.

A mistaken left classification could overwrite left twice and leave right old
even though both game-side passes ran. `1507bafc` makes the fragile arm defer to
the ring on a single disagreement; three consecutive disagreements can still
realign. Historical measurements fell from 36 stale-eye lines in 171 seconds
to one in 238 seconds, with swapchain-target repeats falling from 80 to two.
See the September 3 three-fix chain in [STATUS](../STATUS.md).

**Important source correction:** the `1507bafc` commit body says neither arm
invents a tag over an empty/zero entry. The reviewed active code allows the
robust `+1` arm to do so and refuses invention by the fragile arm. Use the actual
`g_c5Pair` branch in [reentry.cpp](../../src/core/gfx/reentry.cpp) for current
behavior, not that sentence in the old message.

The earlier C5Pair A/B was headset-confirmed: pause/resume with it off produced
24 of 25 swapped pairs in the cited run; re-enabling it removed the observed
swap. That does not make c5 arbitration a universal explanation for later flicker.

### 3.3 Untagged mono flashes and the black-frame chain

An untagged image uses the mono route. One such image inside a stereo stream
changes disparity most visibly on nearby hands/weapons, even when distant world
geometry barely moves. After the stale-right fix, the recorded run still had
`mono/s=1..4`; 26 of 29 single-draw spells came from the present-stall guard.

`HoldUntagged=N` holds up to N consecutive untagged deliveries after recent
tagged output. The next untagged delivery reaches mono, allowing real menus,
loads, and cinematics through. At N=3 the recorded mono rate became zero.
This suppresses an output artifact; it does not make the missing second draws
happen or eliminate held-frame latency.

That fix exposed another bug: returning no texture still reached `xrEndFrame`.
With no layer assembled, both eyes went black. The claim in `12c23588` that
submitting nothing automatically holds the compositor's previous pair was wrong
for this runtime. `8441404f` explicitly resubmits the previous layer. The cited
126-second run had `held=25 black=0`, no stale-eye lines, and no target repeats.
`973d699a` then made the headset-judged N=3 the default.

The fallback cannot supply a scene layer before a valid snapshot exists. Read
`zeroLayerHeld` and `zeroLayerBlack` with startup and `shouldRender` context;
do not demand zero for every counter from process creation onward.

### 3.4 Hold banking and pause-menu session loss (VR-54)

On a hold-only present, the submitted structures are `holdProj`, `holdViews`, or
`holdQuad`. The new-layer locals remain empty. Banking based on `layerCount`
overwrote a good snapshot with those empty locals because a hold also sets the
count to one. The next hold could submit null handles/zero views, receive
`XR_ERROR_HANDLE_INVALID`, and tear the session down.

`92b04896` banks only when `builtNewLayer` was true before the fallback.
This is a separate defect from the zero-layer black frame, despite sharing the
hold path. The documented pause logs establish the failure sequence.

A valid saved structure still does **not** preserve old pixels: the runtime
uses the latest released swapchain image. A proposed partial-pair/old-pose hold
mechanism must therefore join release identity, pair identity, and poses. That
architectural limitation is real; it is not proof the mechanism caused a given
flicker. The parked tests that failed to reproduce it are in section 4.

Source: [DESKTOP_MIRROR](DESKTOP_MIRROR.md), sections 3-4, and the runtime's
`builtNewLayer` / `g_feedSnap` code.

### 3.5 Startup eye starvation (VR-16)

The September 4 session-15c run at 2750x2850, 90 Hz, Quest 3/VDXR measured:

| Phase | Eye/tick evidence |
|---|---|
| Menu/loading | L/R zero on the mono route by design |
| Starved gameplay | L about 12-19/s, R about 52-73/s; ticks 51-72/s |
| Tick budget | 17.5 ms against an 11.11 ms display slot |
| Game-side second draw | `2nd/s == draws/s`; missing pass 2 was not the measured cause |
| Tag asymmetry | 1,016 same-eye pushes, reported as repeated right |
| Recovery | About 44.5 seconds from proxy load onward: L=R=90/s, no untagged output |

The perceptual disturbance lasted roughly 25-30 seconds in that long example;
it usually settled sooner. Streaming was the documented load-related explanation
for the slow period. The exact reason the left eye specifically starved remained
unresolved; previous-slot delivery was a hypothesis, not a measured root cause.

`vrpace strict on` was proposed to show the fresh eye to both eyes instead of a
stale stereo pair. It remained an unjudged workaround in the reviewed record.
`capture sharedwait on` was proposed to test the delivery hypothesis. Neither
should be described as a confirmed fix or silently enabled for a new comparison.
Later acceptance of brief startup settling is not closure of this older issue.

Source: [ENGINE_NOTES](ENGINE_NOTES.md), "The startup eye-starvation flicker,
measured at last", and [ROADMAP](../ROADMAP.md).

### 3.6 Desktop alternation and the later one-frame leak (VR-53, VR-76)

**First defect:** each sequential eye called the game's original Present. The
runtime's desktop mirror function did not perform the required D3D9 copy, so
the window showed L, R, L, R while the headset could receive healthy pairs.
The original pin snapshots one eye and re-blits it after the other eye's capture.
Diagnosing a recording of that window as proof of broken headset pairing was wrong.

**Second defect:** the pin used the delivered texture tag to select the current
D3D9 backbuffer. In a one-present-delayed stream, it held the opposite eye.
Single-draw bursts then exposed raw frames of the other eye. This matched the
VR-76 single-frame rightward hand/weapon shift, clearest on the desktop.

The initial V-marker run had 37 post-symptom markers, each within 600 ms of a
single-tick transition; delays were 15/328/578 ms minimum/median/maximum.
The baseline prevalence was 458/1,095 sampled windows, about 42%. Histories
showed no logged placement outliers/refusals/ambiguities in the relevant samples.
Those histories overlapped, so approximately 15,800 rows are not necessarily
15,800 distinct presents, and `0.42^37` is not a valid independent-trial p-value.

`15be6fdd` adds `DesktopEyeSource=draw|tag` and current-draw publication:

- Snapshot a resolved current left draw after capture.
- Re-blit a valid left snapshot on right and up to three consecutive unknowns.
- The fourth unknown invalidates the pin and releases menus/loads.
- Reset, resize, device/source changes, disable, and failed snapshots invalidate
  held-pixel ownership. A successful new left snapshot is required before reuse.
- Right-first warmup may pass through and is counted. Failed copies are failures,
  not proof that the intended eye was displayed.
- All three runtime post-capture paths reach the hook, including zero tags and
  the early pairHold return. The runtime mirror gate is separate from host copying.

The later prologue-to-hub run reported no remaining jump: 106 diagnostic windows,
1,400 single-draw ticks, 1,397 counterfactual old-policy raw leaks, one warmup
right frame, and zero copy failures. `7324e6ac` made draw the default;
`c3d6972b` merged VR-76 (#36).

This is the confirmed visual result for that build/run. The patch changes desktop
selection, not XR eye assembly. Do not use the reported success to claim every
possible headset-only symptom has a desktop cause. VR-77 burst generation and
VR-80 note-exit flicker explicitly remain separate.

Sources: [proposal](VR-76-MIRROR-FLICKER-REVIEW.md),
[review and implementation handoff](VR-76-CODEX-HANDOFF.md),
[DESKTOP_MIRROR](DESKTOP_MIRROR.md), later [STATUS](../STATUS.md).

### 3.7 Weapon copies, shared-gate blinking, and contract lifetime

These can look like eye flicker but originate in mesh/pass handling.

**Uncorrected passes:** the same VB/IB/range can be drawn with different vertex
shaders. The dark animated native-position copy was another pass of the same
mesh. Omitting the vertex shader from contract identity made that pass appear
to be the already-known draw, then fail its transform match. `df26a49b` records
the identifying census; `f643f9da` records the shipped combined correction.

**Suppression versus rescue:** blindly suppressing lighting contributions caused
translucent weapons, black bolts, or disappearance in shadow. Adding a stand-in
draw could create another copy when the real pass appeared later. The retained
policy corrects recognized passes and suppresses recognized unplaceable weapon
draws; it does not invent a replacement draw. A residual blink can therefore be
a refused correction, not a stale stereo eye. Failed rescue paths remain in
`src/legacy/vr33/`.

**Shared publication gate:** both weapons blinked together although their
contracts and buffers differed. The common component snapshot had been tightened
from 100 ms to 20 ms for a sway theory later falsified. Restoring 100 ms removed
most blinking. An unnecessary comparison against a newly read pose generation
also discarded a still-current hand correction; present and eye are the relevant
view identities. The September 8 result confirmed controller following, no native
copies/translucency, and only rare single-frame blinking.

**Capacity:** a table of 12 contracts exactly matched three meshes times four
shaders. One extra pass could evict a live contract and bring its copy back.
`1a7e2382` raises capacity to 64 and reports eviction age/occupancy.

**Loads and swaps:** an empty list had no effective rebuild owner; a partial
non-empty list without a body anchor could block further collection. Swapping
equipment could leave a stale list and miss a crossbow's child bolt. Recovery
requires anchor-aware retries, equipment object identity, a bounded settle
window, and equipped items as collection roots. A live stowed contract must not
be retired just because it is absent from the currently held candidate list.
`fc343404` carries that retention in #27, beyond the recovery changes in #26.

**Instance identity:** a contract identifying bolt geometry must not move a fired
world bolt as though it were held. Live membership, reference freshness, and
strong STOWED/ELSEWHERE verdicts distinguish instances. Weak NO_REF cannot be
treated like a strong ownership veto without breaking re-equip recovery.

**Configuration regression:** VR-84 later found Save As Defaults writing
`AttachRigRadius=2` instead of 200. The loader clamped it to 10, still excluding
a crossbow measured around 157 units from the anchor. Hand placement counters
remained healthy while weapons stopped following. Check requested, saved, and
effective values before reopening geometry or flicker theories.

Sources: [VR-33-HANDS-AND-WEAPONS](VR-33-HANDS-AND-WEAPONS.md), especially
sections 3-5 and 8; [ENGINE_NOTES](ENGINE_NOTES.md) load/swap findings;
[LOCKON_REVIEW](LOCKON_REVIEW.md); [TRAPS](../TRAPS.md).

### 3.8 Queued stereo draws lost their weapon eye state (VR-69)

Earlier experiments tried alternating unreadable palette steps and then added a
mono guard. Alternation in genuine mono produced full-IPD oscillation. The guard
stopped that symptom but used the game lane's current `g_sdDoublingNow` to reset
render-side history. An older queued stereo draw could therefore lose its eye
correction when a newer script tick went single. Atomic access did not fix the
missing association between the flag and that draw.

The controlled #26-plus-pose candidate `08cbb368` retained stable head turns but
showed outward weapon displacement in both eyes. Its log recorded 8,538 unknown
eye evaluations out of 143,598 while three weapon contracts remained active.
Unknown eye omits the half-IPD correction, fitting the outward direction; the
aggregate alone was not an event-by-event causal proof.

`b3a1ff46` restores `MpEyeForPresent` and `MpWorldTarget` to their `b38519c3`
function bodies while retaining the working pose-history consumer and recovery
work. Production-function tests failed five assertions before and passed nine
after. The headset confirmed stable lock through swaps, leaving only the
downward-motion case. Mainline port: `6ecda3a7`.

`PaletteEyeAlternate=0` did not disable the bad mono guard, so the earlier toggle
was not an exclusion of the full regression. Those experimental eye functions
are now legacy code. The current decision holds the prior answer for a small
step, classifies a readable signed step, and reports unknown for an overly large
step. Do not reintroduce forced alternation based on the older success narrative.

Sources: [LOCKON_REVIEW](LOCKON_REVIEW.md),
[SESSION_HANDOFF_2026-09-10](SESSION_HANDOFF_2026-09-10.md),
[mesh_split.cpp](../../src/game/dishonored/hands/mesh_split.cpp).

### 3.9 Downward-motion camera ownership failure (VR-69)

After the eye restoration, crouching, descending slopes, and falling still
flickered. Moving tracked head/controllers vertically did not reproduce it;
uphill movement and jumping were reported stable.

`FovLeverApply` clamps camera Z before `camera::apply_offsets`. The offset
writer recognizes its last write by comparing the entire vector. A Z-only mod
clamp broke that match while X/Y still contained the previous eye offset. The
next write could treat those already-offset coordinates as a new engine base.
For base X=10 and eye offsets -3.155 then +3.155, the faulty sequence gives
X=10 rather than the expected 13.155 after the right write.

`417bfad9` reconciles only a same-object, same-field, exact previous-write match:
the remembered offset and written Z absorb the clamp, preserving the original
base. Fresh engine vectors, another camera/field, and inactive/test ownership
retain their normal treatment. This is not a per-axis guess or a change to the
ceiling height/easing. Mainline port: `5581ed46`.

Nineteen production-function checks passed; the legacy raw clamp failed six.
The successful headset log contained all eight bounded `camera/clamp-rebase`
entries, proving the protected sequence ran. The report then confirmed removal
of the downward flicker. Ten unknown eye evaluations and one ambiguity remained:
the acceptance criterion was stable visuals, not every diagnostic becoming zero.

The confirmed diagnostic DLL was `vr33-hands-working-60-g417bfad9`.
Its source and configuration differ from later integration binaries; the handoff
explicitly preserves that distinction. Brief startup settling was accepted.

Sources: [DOWNWARD_CLAMP_REVIEW](DOWNWARD_CLAMP_REVIEW.md),
[session handoff](SESSION_HANDOFF_2026-09-10.md),
[camera.cpp](../../src/game/dishonored/camera.cpp),
[fov_lever.cpp](../../src/game/dishonored/fov_lever.cpp).

### 3.10 Open transition issues and related motion artifacts

**VR-77:** actual single gameplay ticks remain, including bursts related to the
present-progress guard. Count the gate transition and its population before
proposing timing changes. VR-76's 1,400 single ticks during otherwise successful
play prove the mirror fix did not eliminate the trigger. Removing a liveness
guard without testing menus/loads is not a demonstrated remedy.

**VR-80:** a rare sustained both-eye flicker after closing a note was reported in
the successful VR-76 session, with a one-sided tag stream in its log. That is an
observed signature, not a proven identification of the earlier gate bug. Preserve
the opening/closing transition and determine where eye identity first diverges.

**VR-79:** an object hidden from one eye vanishes from both. This is a separate
visibility/culling investigation. Correct pair ages cannot prove correct culling.

**Cadence and pose lag:** early 120 Hz tests showed uneven display-slot cadence
and doubled edges; the same tested setup at 90 Hz was reported clean. Later
world and weapon head-turn judder required image-matched head history, with
`Pace Lag=2` and `Hands PoseLag=2`. A change to a compiled default did nothing
when the installed ini still explicitly selected lag 1. Automatic lag A/B left
enabled could recreate judder periodically. These are separate from static,
single-frame eye jumps; neither the 90 Hz result nor lag 2 proves all hardware
and runtime combinations are universally solved.

Long `xrEndFrame` stalls locate the delay at that API boundary. They do not by
themselves identify Wi-Fi, encoder, runtime, or another downstream component.
Likewise, a camera/neck error that varies smoothly with pitch is not automatically
a flicker. Sources: [STATUS](../STATUS.md), [TRAPS](../TRAPS.md),
[ENGINE_NOTES](ENGINE_NOTES.md), [VERIFICATION](../VERIFICATION.md).

## 4. Failed approaches and corrected readings

### 4.1 Desktop investigation graveyard

[BRIEF-eye-flicker](BRIEF-eye-flicker.md) is an answered investigation, not a
current fix plan. Its table actually lists five hypotheses despite some summaries
calling it four:

| Earlier explanation | Evidence/correction |
|---|---|
| Desktop monitor cadence caused the recorded alternation | The recorded comparisons did not change it; distinct from the separate headset cadence finding |
| A headset mirror combined both eyes | The artifact was visible in the game's own window |
| Pass 2 produced no Present | Measured presents tracked first plus second draws |
| Second-pass latch stayed set | Balanced latch scope and zero +1 activity in no-second-draw windows |
| Runtime never paired | Populated healthy pair/submission counts and expected ages |
| Only about 30% of gameplay ticks doubled | Retracted: the sampled window crossed pause, XR failure, and teardown; healthy surrounding windows doubled essentially every tick |
| Stand stereo down based on that low ratio | Removed; built on the retracted population |
| c5 eye-trace span measured presents | Corrected: that ring samples constant uploads; normal stereo spans an IPD |

The old centered-single proposal is historical, not a shipped universal remedy.
Real single-draw bursts measured later do not validate the earlier teardown-based
claim that most normal gameplay failed to double.

### 4.2 Parked September 9 universal-flicker investigation

Relevant material exists only in other reachable history, including `bbb0456a`,
`dac9b7be` (`FLICKER_PLAN.md`), `fad8aff8` (`HELD_LAYER_PLAN.md`), and correction
`e5d7f5ee`. `bbb0456a` and `035b2ca4` are not ancestors of the reviewed HEAD.
Do not confuse a commit titled `fix:` on that branch with a shipped solution.

That investigation described a fixed-direction leftward whole-left-eye jump,
including world geometry, while standing still. It is not automatically the same
symptom as the later outward weapon regression or VR-76 rightward desktop jump.
The branch reported no final fix. Its eliminations are useful within those runs:

| Attempt / claim | Recorded outcome and limit |
|---|---|
| Read the eye from the game-thread drawing-pass stack | Zero executions in 83,400 corrected draws; palette work ran on the render lane |
| Bare global pass audit showed 39% disagreement | Retracted; did not identify the queued view |
| Turn eye offset off | Flicker changed, but stereo depth/weapon appearance broke; not an acceptable fix |
| Use method eye publication | Large symptom improved on the experimental branch; residual remained; temporal identity still needed proof |
| Same-eye hold | Zero holds fired while the symptom continued |
| Recover an absolute origin solely from draw matrices | Input space was camera-relative; fixed-origin premise failed |
| Reject tags by absolute writer-position versus c5 distance | Walking moves the engine camera; the experiment dropped roughly a third of left tags |
| Held-layer partial pair | Holds ran while abort counters stayed zero; the proposed sequence was not established |
| Frameless present closes an open pair (`HoldKeepsPair`) | `onOpenPair=0` while frameless populations were nonzero; did not explain that run |
| Submitted pair pose separation displaced one eye | About 5,700 pairs: separation min/max/mean 0.0631 m, zero side flips/generation splits |
| c5 arbitration relabeled those gameplay eyes | Roughly 11,000 verdicts had zero disagreement; exception was a loading transition |
| Pass 1 inherited the preceding right camera | `MOVED=0`, worst move zero over the reported population |

`HoldKeepsPair` from `035b2ca4` is not an active configuration control in the
reviewed tree. The general saved-layer/pixel distinction survives; the tested
causal theory did not. A future run with an actual mid-pair release sequence
would be new evidence, not permission to relabel those historical runs.

The archived work also measured BeginScene once per present across two logs,
identifying a useful render-view boundary beneath the game-lane viewport draw.
That is a measured population for those runs, not a universal UE3 scheduling
contract. Any queued per-view identity design must validate the boundary again.

### 4.3 Instrumentation traps that recur

- Negating an eye and improving agreement cannot separate an inverted convention
  from a one-publication delay in an alternating stream.
- A regular producer sequence does not establish which publication the consumer
  actually used. Transport identity with the work or measure a valid join.
- Comparing two hands sharing one `MpDrawCtx` compares a context to itself,
  not two eyes. The historical 105,816 identical comparisons answered no stereo
  question. Consecutive draw IDs alone do not prove opposite-eye views either.
- The matrix phase-B claim that no VP-only differences meant no eye in VP was
  retracted: VP also changed in the BOTH category, with camera-relative input
  space making that expected. The XOR geometry ID could also collide.
- A pose audit after pairHold can sample only right-eye completion. Print
  per-eye populations before claiming both submitted eyes are clean.
- `L/s=R/s=0` on the mono screen is intentional. A skip total printed beside
  a three-second rate can still be lifetime cumulative. Read reset sites.
- `pushed eye TWICE` does not independently prove a stale submitted image;
  historical abort/stale counts did not support its strongest wording.
- Current `reentry: pair geom` gates on delivered tags but reads current c5.
  Under delayed capture its SWAPPED wording is not independent proof about the
  delivered/submitted pixels. The VR-76 handoff explicitly leaves this limitation.
- Equal mirror snapshot/blit counts do not identify the held eye. Shadow-policy
  counts assume successful copies; actual shown-eye values use copy provenance,
  not readback verification of the final display.
- Readable palette steps are not independently verified eye identities. Zero
  unknowns can coexist with wrong confident classifications.
- A draw census filtered by palette upload, primitive count, a budget exhausted
  before viewmodels, an overly narrow angle band, or incomplete contract identity
  can exclude the very pass being sought. Record skipped populations.
- An accepted startup settle, a stale snapshot, a menu transition, and a steady
  gameplay jump require different time windows. Do not average them together.
- (VR-80) Counters of realigns and untagged-branch entries are not counts of empty
  pops: the realign counter moves without a removal, and a 0 tag enters the same
  branch. Only a per-present ledger with draw ids separated them.
- (VR-80) A predicted self-sustaining drain loop did not reproduce in the host model,
  and the headset ledger showed the drain removing the correct tag. The fault was a
  recurring ONSET (a late tag), not a sustaining repair.
- (VR-80) With a still camera, `w2c self 0.00` cannot distinguish the right tag from
  the same eye one tick later; c5 tells ticks apart only when the camera moves.
- (VR-80) Under `SharedWait=0` a fix to the popped label is not a fix to the
  delivered image: the delivered pixels are the previous present's slot. Check the
  `deliv` column, not only `out`.

Sources: [TRAPS](../TRAPS.md), [VR-33 record](VR-33-HANDS-AND-WEAPONS.md),
[VR-76 handoff](VR-76-CODEX-HANDOFF.md), and archived corrections above.

## 5. Current controls and code map

These values describe the reviewed tree and recorded stable profile, not an
instruction to overwrite a user's ini. An explicit installed value wins over a
compiled fallback. Check loader, generated default, persistence, and consumer.

| Control | Reviewed value / purpose | Investigation caveat |
|---|---|---|
| `[Stereo] Method=reentry`, `Armed=1` | Sequential native scene redraw | Mono/arm-off removes stereo; not a neutral comparison |
| `[Stereo] C5Pair=1` | Ring versus c5 arbitration | `reentry c5pair on\|off`; an A/B can deliberately restore bad pairing |
| `[Stereo] LateTagRepair` | 0 shipped, 1 on the test PC: VR-80 late-tag repair plus capture-slot relabel | `reentry latetag on\|off`, F10 Display; needs `C5Pair=1`; the relabel refuses in sync and `SharedWait=1` |
| `[Stereo] RingLedger` | 0; per-present ring records in bounded windows, 10 s reconcile | Diagnostic only; windows are event-biased, so their distributions are not rates |
| `[Stereo] HoldUntagged=3` | Bounded suppression of brief mono delivery | `stereo hold <n>`; 0 restores mono interruptions |
| `[VR] DesktopEyeSource=draw` | Current-backbuffer pin | `desktopeye draw\|tag\|status`; source switches invalidate held pixels |
| `desktopeye on\|off` | Host desktop copy gate | Separate from `vrmirror on\|off` runtime hook gate |
| `[VR] DesktopEye` | Old documented name | Not a parsed working ini switch |
| `[Capture] SharedWait=0` | Previous-present shared delivery | `capture sharedwait on` changes timing/cost as well as delivery identity |
| `[Pace] Strict=0` | Strict stale-eye fallback off | `vrpace strict on` is unconfirmed for startup flicker |
| `[Pace] Lag=2`, `[Hands] PoseLag=2` | Recorded stable world/weapon head history | Preserve for controlled stability comparisons |
| `[Stereo] LagAB=0`, `[Hands] PoseLagAb=0` | Automatic lag experiments off | A running sweep can look like a periodic regression |
| `[Hands] PaletteEyeOffset=1` | Half-IPD placement correction | Disabling changes depth; not a clean universal cure |
| `PaletteEyeAlternate`, `PaletteEyeFromPass` | Historical experimental settings | Do not assume parsed names select the restored production path |
| `[Hands] AttachSnapshotMaxMs=100` | Component publication freshness | 20 ms caused simultaneous weapon blinking |
| `[Hands] AttachRigRadius=200` | Membership geometry gate | Diff saved ini; VR-84 wrote the wrong literal |
| `AttachDropUncorrected`, `AttachSuppressUnplaced` | Recognized unplaceable weapon-pass suppression | Diagnose missing correction before loosening gates |

The September 10 stable profile is preserved in
[known-good-2026-09-10-stability.ini](../../tests/golden/known-good-2026-09-10-stability.ini).
It is a historical comparison asset, not a current universal preset: later
aiming/model defaults changed. Config source is
[config.cpp](../../src/core/config/config.cpp), with generated text checked
against [dishonored_vr.ini](../../tests/golden/dishonored_vr.ini).

| Question | Source locations / symbols |
|---|---|
| Why was a tick single? | [scene_draw.cpp](../../src/game/dishonored/scene_draw.cpp): `SceneDrawDecide`, `SceneDrawMaybeSecond`, `SceneDrawBeat` |
| What eye did the method classify and deliver? | [reentry.cpp](../../src/core/gfx/reentry.cpp): `note_drawn_eye`, `set_pending_tag`, delivered-tag/hold branch, the ring ledger; [reentry_pair.inc](../../src/core/gfx/reentry_pair.inc): the ring, `pop_and_arbitrate`, the late-tag repair (host model: [reentry-pair-tests.cpp](../../tools/reentry-pair-tests.cpp)) |
| Which pixels did capture return? | [capture.cpp](../../src/core/gfx/capture.cpp): shared slot choice, delivered serial/tag, `read_done` |
| Did the desktop choose/copy the intended view? | [desktop_eye.cpp](../../src/core/gfx/desktop_eye.cpp), [policy](../../src/core/gfx/desktop_eye_policy.h), [frame_hooks.cpp](../../src/core/framework/frame_hooks.cpp) |
| Which XR pair/layer was submitted? | [openxr_runtime.cpp](../../src/core/vr/openxr_runtime.cpp): pairHold, mirror hook sites, zero-layer fallback, snapshot bank |
| What camera write/clamp was owned? | [camera.cpp](../../src/game/dishonored/camera.cpp): `clamp_written_z`, `clamp_location_z`, `apply_offsets`; [fov_lever.cpp](../../src/game/dishonored/fov_lever.cpp) |
| What eye/target did the hand draw use? | [mesh_split.cpp](../../src/game/dishonored/hands/mesh_split.cpp): `MpEyeForPresent`, `MpWorldTarget`, `MfMarker` |
| Why did the weapon pass miss or vanish? | [weapon_attach.cpp](../../src/game/dishonored/hands/weapon_attach.cpp), [weapon_frame.h](../../src/game/dishonored/hands/weapon_frame.h), [hands state](../../src/mod/state/57b_game_dishonored_hands_weapon_attach.inc) |
| Where are markers sampled? | [hotkeys.cpp](../../src/core/input/hotkeys.cpp), [present_tick.cpp](../../src/game/dishonored/present_tick.cpp) |
| What experiments are retired? | `src/legacy/vr69/` palette eye experiment; `src/legacy/vr33/` primitive sibling/rescue work; parked Git history |

## 6. Investigation and regression workflow

### 6.1 Preserve the reproduction before changing anything

1. Identify the surface: game window, recorded desktop, left headset eye, right
   headset eye, or both. Judge mirror and headset separately.
2. Identify what moves: world, hands, all weapons, one weapon, its loaded bolt,
   a shadow/lighting pass, or just the controller guide. Record direction,
   duration, frequency, and whether the object disappears versus shifts.
3. Record the trigger: cold load, reload, pause, note close, equip/sheathe,
   crouch/downhill/fall, head turn, controller movement, or standing still.
4. Preserve build ID, exact source/dirty patch identity if known, DLL hash,
   installed ini, resolved config lines, runtime/backend, refresh, render size,
   capture mode/SharedWait, and relevant A/B state. Do not infer machine identity
   from a drive letter or assume a dirty build hash identifies its whole source.
5. Copy the current and previous logs before a relaunch; rotation is one deep.
   Keep captures/logs/DLLs outside committed source. Use the existing data path.

### 6.2 Mark the event and join the right evidence

`V` with the game window foreground writes `MARKER #N (V)` and the preceding
2.5 seconds of hand/weapon history. It is a post-event marker, so reaction time
matters. A baseline marker and a clear note about the visible surface help.
For a sustained spell, the current F2 handler alternates begin/end fault markers;
check the build's hotkey modifiers before using it. The `mark <text>` seam and
F10 MARK also exist for timestamped faults.

| Evidence | Read it for | Do not infer |
|---|---|---|
| `reentry: gates -> SINGLE/DOUBLE` | Actual gate reason and transition | A lifetime skip total alone dates an event |
| `reentry: beat` | Draw/second-draw/present rates and p2-write refusal population | A window crossing teardown is representative gameplay |
| `stereo: eyes`, `STALE ... EYE` | Pair ages, aborts, stale submissions, named owners | Every repeated upstream tag became a stale displayed image |
| `stereo: frameid` | Image comparison at backbuffer, slot, output, swapchain stages | Sparse sampling rules out an unsampled one-frame event |
| `capture` delivery/read-wait lines | Freshness, serial/tag, slot latency and synchronization | A texture pointer identifies a new image |
| `xr: present handed in NO frame` | Held/black population and fallback coverage | A saved layer is a saved completed image pair |
| `desktopeye:` | Current/delivered identity, raw-leak shadow, successful copies, warmup/failures | All inferred shown-eye values are measured final pixels |
| Marker `T/S/A/F`, `d`, `pr`, `tR`, generation/refusals/misses | Placement decision and target continuity | An unknown-free heuristic is independently correct |
| Marker `desk=source:draw/tag/action/shown` | Mirror copy provenance joined to the draw's Present | A missing newest callback necessarily failed; it may not have presented yet |
| `camera/clamp-rebase` | Protected ownership sequence executed | It proves every perceived event had that mechanism |
| `wa/key`, instance/contract/snapshot counters | Weapon identity, lifetime, freshness, and refusal route | Healthy hand placement proves weapon attachment |
| `stereo: rate`, perf gap phase, pose-generation audits | Cadence, stall location, image/pose consistency | A mean rate proves even cadence or identifies the stalled downstream component |

The original V history searches tag alignment offsets +1/+2/+3; the VR-76 run
selected +3. This includes callback timing and delayed capture, not a universal
three-frame latency. The desktop record uses its own direct Present join.

Example existing read-only log search, with the path replaced by the preserved run:

```powershell
rg -n 'MARKER|marker #|reentry: gates|reentry: beat|stereo: eyes|STALE|frameid|desktopeye:|NO frame|clamp-rebase|wa/key|UNEVEN' 'path/to/preserved/dishonored_vr.log'
```

### 6.3 Change one cause and require an exercised negative control

Prefer the smallest comparison between a confirmed baseline and the suspected
regression. A commit containing pose, contract, and eye changes is not one
behavioral variable; inspect file/function provenance. The VR-69 normalized
build comparisons were not an untouched historical binary bisect.

Use a matching host test when the suspected defect is deterministic. Require the
old behavior to fail for the intended reason. Then obtain a separately recorded
visual result where necessary; do not replace perception with passing arithmetic.

| Existing verification route | Historical result / proper scope |
|---|---|
| [desktop-eye-host.ps1](../../tools/desktop-eye-host.ps1) | 79,339 policy assertions and 72 actual-copy-module assertions in VR-76; old delayed policy deliberately fails pinning |
| [palette-eye-host.ps1](../../tools/palette-eye-host.ps1) | Nine production eye checks after restoration; five failed before |
| [camera-clamp-host.ps1](../../tools/camera-clamp-host.ps1) | Nineteen checks; `-LegacyClamp` fails six descent/release assertions |
| Existing frame/weapon tests | Frame transforms, identity, and correction invariants; 88 cases in the VR-69 record |
| Simulator eye/pause scenarios and arming hammer | Pair ages, transition/numerical behavior; see [VERIFICATION](../VERIFICATION.md) for commands and exit codes |
| Standalone XR simulator self-test | Runtime smoke only; the 60-frame VR-76 self-test was not a successful in-game simulator run |
| Headset/desktop reproduction | Visual stability and depth on the named build/configuration; record each surface's result separately |

These are historical suite counts, not tests rerun for this document. A documentation
change does not require installing or launching a candidate renderer.

Minimum visual regression coverage for a future flicker fix:

- Stable gameplay with head/controller motion and while standing still.
- Crouch/stand, descend/ascend, fall/jump, and tracked vertical movement separately.
- All affected weapons, loaded bolt, swap away/back, sheath/draw, and save reload.
- Pause and note open/close, startup settling, and cinematic/menu transitions.
- Desktop and both headset eyes; correct depth, opaque weapon appearance, and no
  native-position copy in light or shadow.
- Real trigger population in a clean result. For VR-76, no bursts/shadow leaks
  means the leak suppression was not exercised. For the clamp, no matched clamp
  entries means the protected sequence was not demonstrated.

This is a coverage menu scaled to the changed subsystem, not a requirement to
repeat every historical experiment after every edit. Keep already-confirmed
behavior and installed configuration stable during attribution.

## 7. Commit and evidence index

Hashes are locally verified history references. Read bodies and diffs together;
subjects sometimes announce an experimental fix whose later run failed.
These references do not claim every hash is an ancestor of current mainline.

| Commit(s) | Durable meaning |
|---|---|
| `813807e3` | Decide pass gates once before the first eye tag |
| `8020855a`, `230ac120` | No stale re-push without delivery; protect shared consumer reads |
| `c8cfe107` | c5-backed ring-order correction |
| `1507bafc` | Fragile cross-tick c5 arm defers; see current-code qualification in 3.2 |
| `12c23588`, `8441404f`, `973d699a` | Untagged hold, explicit layer resubmission, then default N=3 |
| `bb7a40fd`, `6531f6e8` | Three-fix headset evidence; later startup-starvation record |
| `92b04896`, `9c36850d` | Original desktop pin / snapshot banking; VR-53 integration |
| `df26a49b`, `1a7e2382`, `f643f9da` | Shader-inclusive pass identity, contract capacity, shipped copy/blink fixes |
| `a6e00a6f`, `11cef70f`, `da1d760d` | Recovery/eye experiments; forced alternation and mono guard later superseded |
| `fc343404` | Image pose transport and live stowed-contract retention share one commit |
| `dac9b7be`, `fad8aff8`, `e5d7f5ee`, `bbb0456a` | Parked plans, corrected counterpredictions, archived eliminations |
| `035b2ca4` | Parked HoldKeepsPair attempt; not active mainline |
| `08cbb368`, `b3a1ff46`, `417bfad9` | Controlled diagnostic sequence: reproducing base, eye restore, clamp fix |
| `6ecda3a7`, `5581ed46`, `d126d039` | Mainline stability ports and integration/verification record |
| `c7f12168`, `cb3b5974` | Tested configuration promoted to fresh-install defaults (VR-72) |
| `37aab49f`, `18d39cee` | V-marker instrumentation and VR-76 diagnosis record |
| `15be6fdd`, `7324e6ac`, `c3d6972b` | Current-draw mirror fix, confirmed default, VR-76 merge |
| `7226a613` | VR-80 ring ledger with draw ids and reconcile; pairing moved to `reentry_pair.inc`; host model |
| `27f5b714`, `0641c48a`, `56411e09`, `675131d6` | VR-80 late-tag onset measured, repair lever, capture-slot relabel, headset confirmation |

To recover a parked document without changing the checkout:

```powershell
git show dac9b7be:docs/dishonored/FLICKER_PLAN.md
git show fad8aff8:docs/dishonored/HELD_LAYER_PLAN.md
git show e5d7f5ee -- docs/dishonored/FLICKER_PLAN.md docs/dishonored/HELD_LAYER_PLAN.md
git show bbb0456a -- docs/STATUS.md docs/TRAPS.md docs/dishonored/ENGINE_NOTES.md
git merge-base --is-ancestor 035b2ca4 HEAD
```

The final command returning 1 on this baseline means not an ancestor, not a
failed implementation test. Do not cherry-pick an old experiment merely to read it.

Primary reference map:

- [STATUS](../STATUS.md): dated perceptual results, open tickets, integration changes.
- [ENGINE_NOTES](ENGINE_NOTES.md): measurements, engine boundaries, startup and
  camera/weapon mechanisms. Read dates and later corrections together.
- [TRAPS](../TRAPS.md): failed inference, stale settings, invalid populations.
- [VR-33-HANDS-AND-WEAPONS](VR-33-HANDS-AND-WEAPONS.md): pass copies, blinking,
  instance ownership, and detection failures.
- [BRIEF-eye-flicker](BRIEF-eye-flicker.md), [DESKTOP_MIRROR](DESKTOP_MIRROR.md):
  original hypotheses and their corrections.
- [LOCKON_REVIEW](LOCKON_REVIEW.md), [DOWNWARD_CLAMP_REVIEW](DOWNWARD_CLAMP_REVIEW.md),
  [September 10 handoff](SESSION_HANDOFF_2026-09-10.md): controlled stability chain,
  source/config hashes, local backups, and diagnostic/integration distinction.
- [VR-76 proposal](VR-76-MIRROR-FLICKER-REVIEW.md) and
  [handoff](VR-76-CODEX-HANDOFF.md): marker evidence, review corrections, policy,
  local run paths, hashes, host coverage, and candidate-stage limits.
- [ARCHITECTURE](../ARCHITECTURE.md), [VERIFICATION](../VERIFICATION.md),
  [KNOWN_ISSUES](../KNOWN_ISSUES.md), [RELEASE_NOTES](../RELEASE_NOTES.md),
  [ROADMAP](../ROADMAP.md), [TROUBLESHOOTING](../TROUBLESHOOTING.md): supporting
  decisions, instruments, and older issue summaries.

### 3.11 The palette eye classifier holds a stale eye on an unreadable jump (VR-95)

**Ticket numbering.** This work was written and merged before its ticket existed, using the
placeholder VR-94. That number was then assigned to the world-ghost issue instead, so the
ticket for THIS is **VR-95**. Code, docs and tests are corrected; the merged commit subjects
in pull request 48 still say VR-94 and cannot be. If a commit message and this file disagree
about the number, this file is right.

**Symptom identity.** Hands and held weapon jump sideways by about one IPD for a
frame, in the LEFT eye only, during a fast head roll. The right eye is clean. The
tester also reported it may be worse rolling one way.

**Reproduction identity.** Headset, VDXR via Virtual Desktop, 2026-09-13, 2750x2850
at 90 Hz, `PoseLag=2`, `Lag=2`, `PaletteEyeOffset=1`, IPD 6.31 uu. Build
`vr33-hands-working-185-gd71968e3-dirty`. Nine V-marker episodes.

**Measured.** 90 flagged presents across the nine episodes, and **every one reads
"eye R but tag L, decision S". Not one is the reverse.** The runtime tag row
alternates cleanly through all of them, so the stream really was alternating.

The mechanism is in what `S` means. `MpEyeForPresent` classifies the eye from the
right-axis jump in the hand's `LocalToWorld` translation: a jump inside
0.45..2.0 IPD toggles and its sign names the eye; a jump under 0.45 IPD is `S`,
which **holds the previous eye**. `S` means "too small to tell apart", not "the
same eye", so the hold is a guess, and when the stream is alternating it is wrong.
Those hands then take the other eye's half-IPD inside this eye's image.

It is one-sided because the jump is not symmetric. Measured: entering a right
present the jump is about -5.60, entering a left present about +5.09, against a
2.84 uu band. Head roll adds a drift to every jump - it moves the hand AND rotates
the right axis the jump is projected on - so a roll of one sign pushes the smaller
crossing under the band well before the other. Only left-entering presents are
ever misread, and the held value is always R.

**Negative evidence collected on the way, all of it useful:**

* The classifier's own `ambiguous` counter never moved (11 for a whole run, its
  unknown-draw total frozen at 2348). The "head moved too far to judge" path is
  not involved, and `same eye` is the population instead.
* `stereo: eyes` was clean throughout: pairs equal to submits, zero aborts, zero
  stale eyes, ages 1/0. This is not a pairing or delivery fault.
* Frame rate was unchanged with the VR-78 accounting probe armed, median 86
  draws/s either way, so the probe was not the confound it appeared to be.
* Single-draw ticks during the rolling windows ran about one per three seconds.
  The `state` skip bursts that exist are menu transitions; the counter sat flat
  at 7 for the first ninety seconds of the run.

**Two instrument failures found here, both worth keeping:**

* A first cross-check against the stereo method's resolved eye joined at `pres`
  and reported the toggled row disagreeing 15311 times against 10 agreements. In
  an ALTERNATING stream a near-total inversion is exactly what a one-present phase
  error looks like and is indistinguishable from a sign convention (section 4.3).
  The correct join is `present + 1` and was already written fifteen lines away in
  `MfDump`. A cross-check on an unproven join is not evidence, whatever it prints.
* **The marker's own tag reference is fitted to maximise agreement with the
  classifier it is judging** (`mesh_split.cpp`, the `best` offset search). The
  circularity is bounded, because the candidate offsets differ by two and so share
  parity and give the same L/R, but "agrees 444/444" is a weaker claim than it
  looks and should not be quoted as an independent verdict.
* `tools/palette-eye-host.ps1` had not COMPILED since VR-76 added `MfOpen` to the
  body it extracts, so its nine checks had not run in weeks. A suite that cannot
  build is not passing, it is absent. Stubbed and restored.

**Change identity.** `[Hands] PaletteEyePredictToggle`, **default OFF**, F10 Hands
control. On an unreadable jump it predicts the toggle instead of holding, capped
at two consecutive predictions so a genuinely non-alternating stream still holds.
Six host checks replay the measured jump shape; the negative control requires the
old behaviour to produce a doublet AND that doublet to repeat the RIGHT eye.

**Results and status: OPEN.** With the lever on, tag/eye mismatches fell from 90
to 1 and the marker's agreement went from 420/425 to 444/444. **But the tester
reported a NEW hand and weapon flicker while standing completely still**, and the
counters do not see it: every flag in that run is "decision P" - the prediction
itself being flagged only because it is not `T` - with no target-jump flags and no
eye mismatches. So either the regression is real and outside what this subsystem
measures, or it pre-existed and the roll fault was masking it. The lever therefore
ships OFF and is disarmed in the tested install. The live A/B in F10 Hands is the
next measurement: does the still-flicker follow the checkbox?

**Remaining scope.** The roll symptom itself was never re-tested after the fix
landed, so the correction is unconfirmed visually in both directions. The flag
rule should stop treating `P` as an anomaly, or it floods the list. An independent
eye reference - one not fitted against the classifier - is the real missing
instrument.

### 3.12 Roll-induced lateral camera motion is NOT a flicker (VR-91, fixed)

Recorded here only to keep it out of the routing above. Rolling the head moved the
rendered camera sideways the WRONG way - 17.6 uu right for a 30 degree left roll,
19.3 uu left for a 34 degree right roll - because the neck arc is built from the
head's full rotation including roll while the yaw-only reference it is subtracted
from is not, and `[Neck] Mode=cancel` negates the difference. The engine's neck arc
is a PITCH arc; cancelling a roll arc that was never there subtracted a real motion
twice. Fixed by building the arc from a roll-free frame (`[Neck] RollArc=0`);
residual fell to 0.51 and 1.19 uu, and the neck term to exactly zero.

It is a smooth, sustained displacement, not a one-frame event, and section 3.10's
rule applies: a camera error that varies smoothly with head angle is not a flicker.
It is listed because it was reported in the same breath as VR-95 and the two were
initially conflated.

### 3.13 A menu relearned the weapons on every resume (VR-93, B1 confirmed)

**Symptom identity.** About a second of weapon flicker after resuming from a pause
or other mono menu, both eyes, held weapons only; swaps kept their lock. Distinct
from VR-80 (a sustained one-sided stream after a note) and from VR-77 (single ticks
during play). Routed from section 1's "Weapon detaches or flicker returns after
swap/load" row.

**Cause, measured.** `GameStateTick` ran the level-load transition on every exit
from GAMEPLAY, so a pause dropped the weapon contracts and the candidate list and
queued the UI observer's rescan. A pre-fix resume (build 185, `dishonored_vr.prev.log`
of that run) splits the settle into two costs in series: the rescan held the game
thread 499 ms with the view still SINGLE, then re-adoption took 15 ms and the
hand-mesh recalibration another 343 ms. Adoption was never the slow part.

**Hypothesis and counterprediction.** If relearning explains the post-stereo
flicker, keeping validated records removes it and the log shows zero adoptions and
no recalibration after resume. If instead fresh samples or pairing explained it,
the flicker survives retention. The half-second hold was predicted to SURVIVE B1,
because the rescan is untouched.

**Change.** `hands/menu_keep.h` / `menu_keep.cpp`, `[Hands] AttachKeepOnMenu`
(ships 0, F10 Hands live). A MENU over a live pawn suspends instead of dropping;
NO_PAWN, a different pawn, or leaving for anything but a menu drops as before. On
resume the script lane rebuilds the live-object table and requires the pawn, the
controller, every candidate and every contract component to be live with the class
and FName recorded at collection. Any failure drops everything. Corrections still
need a same-present snapshot under 100 ms old. Host: `tools/menu-keep-host.ps1`,
35 checks; an injected FName-blind comparison and a menu-blind suspend each fail
them.

**Results.** Build `vr33-hands-working-190-gf385bfce-dirty`, stability profile
unchanged, `AttachKeepOnMenu=1`. Four pauses, all RETAINED: every validation passed
(8 to 11 objects, table rebuild 7.7 to 9.9 ms), 0 adoptions in each resume window,
no calibration line after any resume. Headset: no weapon flicker after resume on
three deliberate pauses. As predicted, first DOUBLE still came +524 to +540 ms after
GAMEPLAY on every retained resume, matching rescans of 498 to 524 ms: that hold is
B2, not this.

**Also seen.** Three mid-level GAMEPLAY -> LOADING periods (0.75 s, 10.6 s, 7.0 s)
dropped everything under the unchanged non-menu rule, while the pawn and
controller kept the same pointer and FName across them. Not investigated; the rule
is deliberately conservative.

**Negative control, run.** Build `vr33-hands-working-191-g0ff00ebb-dirty`. A save
loaded from the pause menu read INVALIDATED ("a different pawn was latched during
the menu") before the new level reached GAMEPLAY, and the late validation failed
too (the old pawn was absent from a freshly built table). Weapons relocked after
the load. **Retracted expectation:** the new pawn at a different address carried
the SAME FName, `10783_0`, as the old one. FName does not distinguish a respawn,
so this load was caught by its new ADDRESS, and a load that reused the address
would have passed the identity check. Hardened in the same commit as this record:
the game's `LoadGameClicked` event during a suspension now drops everything. It
fires when the save browser opens, so backing out of the browser also drops.

**Not flicker, same run.** A brief mono flash during a drop takedown was a
present-progress SINGLE tick held by `HoldUntagged` (VR-77's signature), with no
menu transition anywhere near it. The run then ended in a garbage-collector crash
filed as VR-96; that signature is in the crash history on 09-11 and 09-12, before
any VR-93 code, and the last retained state had been dropped 96 s earlier.

**B2, run.** Build `vr33-hands-working-192-gff55ae1d-dirty`, `[Menu] UiKeepOnMenu=1`.
Three pauses, all retained, and the first DOUBLE came +24 to +26 ms after GAMEPLAY,
against +521 to +540 ms with the rescan on. No rescan ran on any kept resume; the
three book/note openings in the same run took the unchanged non-menu path and did
rescan (499 to 501 ms). Headset: pauses resumed without the hold.

**Status.** B1 and B2 confirmed for ordinary pauses; save-load negative control
passed. A book (note screen) reads LOADING, not MENU, so neither applies to it.

**Books, run.** `[Hands] AttachKeepOnNote=1` (build 193 built 13:49): four book
closes all SUSPENDED on the observer's note-movie bit, validated, RETAINED, with no
rescan and the first DOUBLE +14 to +41 ms. The flicker reported after the fourth
close is section 3.15, not this change.

### 3.14 With `c5` unavailable, a book exit's ring skew is not corrected for seconds (measured, open)

**Symptom identity.** After closing a book, 5 to 10 s of flicker; with dark vision
on, the hands looked misaligned until dark vision was turned off. Headset, both
eyes. Routed from section 1's "One eye appears frozen, swapped, or behind after
pause/load/rearm" and "Rare sustained both-eye flicker immediately after closing a
note" (VR-80) rows.

**Reproduction identity.** Build `vr33-hands-working-192-gff55ae1d-dirty`, stability
profile unchanged, VirtualDesktopXR, 90 Hz, shared capture (SharedWait=0). Log saved
locally under `build/vr93-logs/launch3-*` (ignored).

**Measured.** From about 391 s, when a power was used (a `PawnMaterialParam` notify
and the power wheel closing), every present's `c5` read `(0.0 0.0 0.0)` until
423.4 s, and again from 431.7 s to 441.1 s; shorter zero stretches of 1 to 3 s
appear earlier. A new shader layout (`LocalToWorld c231`) and a shader with no
readable constant table appear at the same moment. With `c5` constant,
`reentry.cpp`'s step is zero, `inv` stays 0, and pairing is ring order alone - the
configuration section 3.2 records swapping 24 of 25 resumes. Each book exit re-armed
stereo after 116 and 84 single ticks. The FIRST ring realignment of the run came at
423.2 s and the next at 440.4 s, each within one present of `c5` returning, with
`pair geom ... dot -1.000 (SWAPPED)` and a STALE L EYE at the same instants. The
skew therefore lasted 7.3 s and 5.6 s after the two book exits.

**What turned `c5` off is NOT established.** Dark vision was reported on at the
second book exit, and the second zero stretch (431.7 to 441.1 s) fits it. The first
stretch (391 to 423 s) began at a power use the log does not name, and the tester
reports that the earlier rare after-note flicker (VR-80) happened with dark vision
OFF. So dark vision is at most one of the things that stop the `c5` upload, and
VR-80 is not shown to share this cause.

**Hypothesis.** Whenever the game stops uploading `c5`, the only eye-order
measurement is gone, so a skew created by any re-arm persists until `c5` returns.
The hands follow the palette classifier's eye, not the ring, so they disagree with a
swapped world image by a full IPD.

**Counterprediction.** A book exit with `c5` present (the `stereo: frameid` lines
show non-zero `c5`) realigns within a few presents of the DOUBLE, not seconds later.
A pause and resume with `c5` at zero shows the same multi-second skew, because the
re-arm, not the book, creates it. A book exit that skews for seconds with `c5`
present falsifies this explanation.

**Also seen, not investigated.** After the third LOADING (443.5 to 444.9 s), with
`c5` back, `pushed eye +1 TWICE` climbed from 45 to 163 in ten seconds with a
realignment every ~3 s. Before 423 s the run had none.

**Not tested.** No book exit with `c5` present; the cause of the first zero stretch
is unidentified; no fix written. Candidate direction only: a fallback pairing
measurement for presents whose `c5` is unavailable, which must be marked as intent
(the position the draw wrote), not as a measured camera.

**Status.** Measured, open, filed as VR-97. **The next run falsified `c5` loss as
the explanation for after-book flicker in general** (section 3.15): the same
seconds-long skew happened with `c5` present. The zero-`c5` stretches remain a
real, separate way to lose the correction.

### 3.15 After a note closes, tag/camera skew and sustained flicker (VR-80, fixed behind `LateTagRepair`)

#### The solution (read this first; the chronology below is the evidence)

**Symptom.** After closing a note or book, most reliably crouched, the view flickers for
seconds until a pause: the scene jumps right in the right eye and left in the left eye (each
eye briefly shows the other eye's image). Standing, the same thing lasts about half a second.

**Mechanism, measured per present with the ring ledger (run 5).** The game side pushes one eye
tag per draw into the tag ring; the present side pops one per present. Normally a tag is in the
ring about 10 ms before the present that shows its image. After a crouched note close that
margin collapses to under 1 ms (VR-99 tracks why), and several times a second a present shows
draw N's image before draw N's tag arrives:

| Present | Pop | c5 says | Old behaviour | Visible |
|---|---|---|---|---|
| P | EMPTY | left | REFUSE, untagged | nothing (held) |
| P+1 | tag N (-1) | right | TOOK: override to +1 | right, but see below |
| P+2 | tag N+1 (+1) | left | defer to the ring, streak 2 | **left image in the right eye** |
| P+3 | tag N+2 (-1) | right | TOOK, drain removes N+3 | right eye pushed twice |
| P+4 | tag N+4 (-1) | left | agree | aligned |

The three-disagreement drain removed the correct tag every time: the earlier over-drain
reading (checkpoint 1 of the plan) was refuted.

**Fix, part 1: repair the late tag (`reentry_pair.inc`).** An empty pop in a tagged stream whose
c5 step names an eye OWES that eye one tag. At the next present, if that present's own c5 names
the opposite eye AND the front tag is the owed eye, the front tag is removed as a repair (its
image has already been shown) and the present pops its own tag. Requiring the next present's c5
to confirm means the fragile cross-tick arm alone can never trigger it on a moving player.
Headset run 6: 41 of 44 late tags repaired, the swap cycle gone, but the flicker moved to the
LEFT eye only.

**Fix, part 2: relabel the waiting capture slot (`capture::relabel_last_grab`).** With
`[Capture] SharedWait=0` a present delivers the PREVIOUS present's pixels. The repaired present
therefore delivered present P's image, which still carried tag 0, so `HoldUntagged` kept that
left image out of the left eye once per cycle. On a repair, the slot waiting for delivery now
receives the removed tag's eye and pose record. Only an untagged slot from the latest grab in a
pipelined mode qualifies (shared with `SharedWait=0`, or deferred); sync and `SharedWait=1`
refuse and log it. Headset run 7: reported essentially clean, 71 repairs relabelled, 0 refused.

**How it was proven.** `tools/reentry-pair-host.ps1` compiles the shipped `reentry_pair.inc`
against a producer/present schedule whose draws carry their own identity. With the lever off it
reproduces the run 5 cycle present for present; with it on, and a simulated pipelined delivery,
no image is held or reaches the wrong eye, and none of the 72 earlier fault schedules gets worse
(248 checks). The ledger's 10 s reconcile (tail movement against every counted removal, head
movement against accepted pushes) held in every headset window, so no ring mutation path is
uncounted.

**Controls.** `[Stereo] LateTagRepair` (default 0, `reentry latetag on|off`, F10 Display
checkbox); `[Stereo] RingLedger` (default 0, diagnostic: per-present records in bounded windows
plus the reconcile line). Log words: `OWE`, `LATE-REPAIR`, `late tags (...): owed N repaired N
expired N, slot relabelled N refused N`, and `reentry: late tag - ...`.

**Still open.** Why the margin collapses after a crouched close and why a pause restores it;
an occasional single-frame flicker (first suspect: the 3 to 6 owes per episode window that expire
unconfirmed); whether the lever ships on. All three are VR-99. Commits: `7226a613` (ledger and
host model), `0641c48a` (part 1), `56411e09` (part 2); records `27f5b714`, `675131d6`.

#### Investigation record

**Symptom identity.** 10 to 15 s of flicker after closing a book the fourth time in
a row, until the tester quit; the earlier three closes were reported clean or
brief. Headset, both eyes. Routed from section 1's "Rare sustained both-eye flicker
immediately after closing a note" (VR-80) row, and the counterprediction of 3.14.

**Reproduction identity.** Build `vr33-hands-working-193-gd8a1e03c-dirty` built
13:49:51, stability profile unchanged, `AttachKeepOnMenu=1`, `AttachKeepOnNote=1`,
`UiKeepOnMenu=1`, `UiFlags=1`, no powers, `c5` never read zero. Log under
`build/vr93-logs/launch4-*` (ignored).

**Measured.** Four book closes, all retained with the first DOUBLE +14 to +41 ms.
At the first stereo pair after the second close (2007.8 s) and the fourth
(2018.4 s), `pair geom ... off-right 180.0 deg (dot -1.000; SWAPPED)`: the camera
position rendered for the ring's -1 present sat on the RIGHT. After the fourth
close, for 11 s: `pushed eye +1 TWICE` rose 2 -> 127, the c5 arm `took` 78 and
`held` 37, realignments every ~3 s, runtime `L/s=74 R/s=85`, and a STALE L EYE
window of 12. The game side was healthy throughout: `draws/s=82 2nd/s=82`,
`p2write refused=0`, no stall or state skips. After the second close the stream
recovered on its own (the TWICE count stayed at 1).

**Reading.** With the passes' camera offsets inverted against the ring, a
within-tick step reads +ipd, so the fragile arm defers to a wrong ring tag (a
swapped image); a still cross-tick step reads -ipd, so the robust arm overrides a
correct ring tag to +1 (the left eye starves). Realignment pops one tag and the
next still moment breaks it again. That matches the rates: overrides only while
near still, so ~11 a second rather than one per tick.

**Not caused by VR-93.** Launch 3, where books still took the old drop path, shows
the same climbing `pushed eye +1 TWICE` with `c5` present after a book-adjacent
LOADING (445 to 455 s). VR-80 itself predates this work.

**What decides it is not established.** The direct-fallback camera writer took the
camera at books 2, 3 and 4 (`viewinject: script camera writes went stale`), not at
book 1, and book 3 did not skew, so fallback ownership alone does not predict it.
Book 4 is the only close where the DOUBLE flag never dropped during the book.

**Counterprediction for the next step.** If the inversion lives in the camera
writer, a per-present record of (ring tag, the eye the writer applied, `c5`) shows
the writer's eye disagreeing with the tag from the first resumed pair. If instead
the ring holds a stale tag across the book, the writer's eye matches `c5` and only
the ring is off by one. Nothing changes until one of those is seen.

**Instrument built, not yet run.** `[Stereo] PairTrace` (`z_account` trace mode,
independent of `ZAccount`) prints `vr80/trace:` lines, one per present, joining the
ring's eye, the eye chosen, the camera write the tag carries (eye, `P2`,
`SAME-WRITE`, age) and `c5` along right: 24 presents after every return to
gameplay or re-arm, and 8 before / 8 after any pairing override, 40 dumps at most.
Ruled out on reading the source first: the script writer's eye comes from
`eye_for_next_frame()`, which is a constant -1 under reentry, so a flipped global
cannot be the writer fault; a stale field (no writer call between a pass 2 and the
next pass 1) still can, and reads as a ring -1 carrying a `P2 SAME-WRITE`. Host:
8 new checks in `tools/zaccount-host.ps1` (50 total); dropping the SAME-WRITE mark
fails two of them.

**First trace run (build 196 built 14:11:40, same profile, `PairTrace=1`).** Nine
book closes; the flicker came after the sixth (3535.75 s) and ran until the
tester paused 24 s later. 24 dumps, 38 overrides.

- **The writer is exonerated.** Every tagged present carried the write its tag
  names: ring -1 with an eye -1 write, ring +1 with an eye +1 P2 write, no
  `SAME-WRITE` anywhere, and `write-to-c5` 0.00-0.03 uu on healthy presents.
- **The ring falls one present behind, and a present with no draw of its own
  pushes it there.** In an episode, `write-to-c5` reads 6.82 uu - the present's
  `c5` is the OTHER pass's - until a realign. Of 17 episode starts, 17 are
  immediately preceded by either an UNTAGGED present that nonetheless shows a new
  pass-1 image (14; `ring +0`, step +6.82, the ring empty when it popped) or a
  REPEAT present showing the previous image again (3; step under 0.6 uu, which
  consumed the next tag). The c5 arm then overrides the wrong tags, which is the
  `pushed eye +1 TWICE` count, until three disagreements pop a tag.
- **What changed after the book is the rate of those presents.** Untagged presents
  (`stereo: beat ... none/s`) ran 0-3 a second before it and 4-7 a second for the
  whole episode, with the left eye short (`L/s` 61-71 against `R/s` 80-84), while
  the game side stayed double on every tick (`draws/s == 2nd/s`, no stall or state
  skips, `singleTicks` 0). At 0-3 a second the episodes are brief and recover
  (dumps 2 and 3); at 4-7 a second the correction never catches up.

**Retracted reading.** Sections above described the passes' cameras as inverted
against their tags. The cameras were right; the tags were one present late.

**Open.** Where a present with no stub draw behind it comes from, and why a book
raises its rate. Next measurement: for each untagged or repeat present inside a
trace window, the Present caller and the scene-draw and `c5` serials since the last
present, so the extra present's owner is named before anything changes.

**Second instrument built, not yet run.** The draw root has three static callers besides the
gameplay site (ENGINE_NOTES, "the viewport draw root's callers"); one of them draws with
bShouldPresent TRUE and none pushes a tag, which is the scene_draw header's own ONE PUSH PER
DRAW failure. `[Stereo] DrawCallerTrace` retargets those three call sites to counting
pass-through stubs and appends to each `vr80/trace` line what drew since the previous present
(`tick`, `p2`, and per site calls with the presenting count in brackets), plus a 10 s census.
Counterprediction: if a foreign caller owns the extra presents, an UNTAGGED present follows a
non-zero presenting count on A, B or C; if every untagged present shows only `tick`/`p2`, the
extra present comes from outside the viewport draw and this lead is dead.

**Second instrument, run (build 199, `vr33-hands-working-199-ga7e7dde8`): the lead is dead.** One book
close, flicker after it until the pause 8 s later. Callers A, B and C were installed (bytes verified)
and made **0 calls** in every 10 s census and on every trace line for the whole run; the 7 untagged
presents show only the gameplay draw since the previous present (4 with nothing, 2 `tick 1`, 1
`tick 1 p2 1`). `disasm-rva.py xref 0x1fc5b0` finds no absolute reference to the root and a raw search
for its address finds none, so no vtable reaches it either: every viewport draw in play is the
gameplay tick the stub tags. The untagged presents are therefore extra **Present calls**, not extra
draws. They show a new pass-1 (left) camera with the ring empty, i.e. before the tick's -1 tag
was pushed. Next: name the Present caller (the return address into the exe) for those presents.

**Third instrument built, not yet run.** With `DrawCallerTrace=1` the Present hook stores its return
address on every present and a short backtrace, each trace line carries `Present from <addr> via
<frames>`, and the 10 s census lists every distinct Present return address with its count against the
gameplay ticks. Counterprediction: one address for tagged and untagged presents alike means the extra
present comes through the engine's normal present path (a timing or pacing cause); a second address
on the untagged presents names a second presenter.

**Third instrument, run (build 201, `vr33-hands-working-201-g5cb3e715`): one presenter.** Every present
in the run returned to `009c01a4` (1,419 to 1,596 per 10 s census, one address only), the untagged
presents included, and the other draw-root callers stayed at 0. The extra presents come through the
engine's normal present path. The untagged present's `c5` is a pass-1 (left) camera while the stub's
tick counter has not moved since the previous present, so whatever uploaded that camera was not the
stub's tick.

**Stance, reported and consistent with this log (one episode, not established).** The tester reports the
flicker persisting while crouched and ending quickly while standing. In this run four standing book
closes produced no override episode; the one episode followed a close 3.3 s after `neck: stance ->
CROUCHED` and ran until the pause with no stand in between.

**Fourth instrument built, not yet run.** Each trace line now carries what the DEVICE did since the
previous present - draw calls, BeginScene, SetRenderTarget, `c5` uploads - and the Present arguments
(source rect, dest rect, window override, dirty region). Counterprediction: an untagged present with no
draw calls is a re-show of an existing buffer; one with draws and `c5` uploads but no stub tick is a
scene render the ring never sees; a window override or rects name a present to another target.

**Fourth instrument, run (build 202, `vr33-hands-working-202-g2d8e9374`).** Untagged presents are full
scene renders (450-910 draw calls, 2 BeginScene, 51-65 SetRenderTarget, 23-34 `c5` uploads, the
same as tagged presents) with identical Present arguments: not a buffer re-show, not another target.
The tester reports the flicker persisting while crouched until a pause clears it, and about half a
second while standing; this run agrees (standing episodes of 1 and 2 realigns; a crouched episode of
11 s ended by the pause). Across the sampled interval `realigned` rose by 65 and the
c5 `untagged` branch counter by 69. The original empty-ring/no-single-ticks interpretation
is corrected below. A realign feedback loop remains a hypothesis, not a measured cause.
**Revised analysis plan: [VR-80-PLAN](VR-80-PLAN.md).**

**2026-09-13 source/log review correction (supersedes the causal readings above).**

1. **Symptom identity:** the same both-eye note-exit flicker, sustained during the
   recorded crouched episode and reported cleared by pause/resume; no new symptom.
2. **Reproduction identity:** reviewed source `c4084478` and the saved build-202 log
   at `build/vr93-logs/vr80-run4-145350/dishonored_vr.log`; existing profile retained.
   This is analysis of an existing run, not an independent headset reproduction.
3. **Hypothesis and counterprediction:** over-draining may sustain skew, but needs a
   removal ledger and a valid join to rendered draw identity. If removed tags belong
   only to already completed/cancelled draws, seek another owner. Queue serials alone
   cannot prove that a removed tag belonged to a future presented image.
4. **Change identity:** documentation only. At 5083.093 s stance is CROUCHED, so the
   5085.375 s beat (85/83/169) is not a standing baseline. At 5096.265 s the
   present-stall SINGLE path fires; by 5097.406 s the beat is 73/71/144 and stall
   rises from 51 to 55. Retract no-single-ticks for the whole episode. Note-visible
   falls at 5086.765 s; GAMEPLAY returns at 5087.765 s. Counter endpoints at
   5087.796 and 5098.968 s give +65 realign attempts and +69 c5-untagged-branch
   entries, not 69 proven empty-ring Presents. `pop_tag` can clear depth >6;
   zero tags also reach this branch. Producer depth >=8 rejects publication.
5. **Results:** printed realign geometry is rate-limited, not every event; the
   realign counter increments even without a removal. No depth-clear warning was
   found in this saved run, but a complete mutation ledger is absent. Scene-scale
   device activity argues against a no-render re-show. No new stub tick between
   Presents does not exclude an older queued draw. The earlier claims of extra
   renders without owning draws, and of a camera upload necessarily preceding its
   own tag publication, are not established. One Present address and matching
   argument flags do not establish per-view/resource identity. No instrument,
   host model, simulator run, renderer change or new visual test was performed.
6. **Status and remaining scope:** VR-80 remains open. Measure onset separately from
   repair feedback, separate empty/zero/clear/rejection cases, and trace eye plus
   pose/capture identity. The existing c5 override changes the eye while retaining
   the popped record; label recovery alone does not prove image/pose recovery.
   Pause's exact recovery mechanism and a causal crouch/timing relationship remain
   unproven. See the revised plan for competing hypotheses and regression gates.

**2026-09-13 host model and ring ledger (plan checkpoint 2).**

1. **Symptom identity:** unchanged; no new headset run yet.
2. **Reproduction identity:** the host model `tools/reentry-pair-host.ps1` compiles the
   shipped pairing from `src/core/gfx/reentry_pair.inc` (moved verbatim out of
   `reentry.cpp`); headset build pending install with `[Stereo] RingLedger=1`.
3. **Hypothesis and counterprediction:** a one-tag-ahead ring plus the three-disagreement
   drain sustains itself at a producer lead of about one tick. Counterprediction: the
   model recovers from every single seeded fault.
4. **Change identity:** diagnostic only. The ledger, default off, is one record per
   present with draw attempt ids from the producer, raw pop outcome, removed ids and a
   10 s tail/head reconcile. No pairing behaviour changed.
5. **Results:** the prediction FAILED in the model. Every single fault (repeated present,
   never-presenting draw, unpublished tag, 0-tag tick; steady or jittered lead; still or
   walking) recovers correct eye labels within a few presents. At lead 1 or more a fault
   leaves the ring a tick ahead for the rest of the run: right eye, next draw's record.
   A sustained wrong-eye episode therefore needs something outside the model (recurring
   onsets, concurrency inside the drain, capture delay, the hold, the progress guard).
6. **Status and remaining scope:** open. The ledger's first headset question compares
   standing-still and crouched-still note closes. The sustained record skew is a model
   result, not yet measured in the game.

**2026-09-13 headset run 5: the onset is a late tag (plan checkpoint 3).**

1. **Symptom identity:** the same crouched note-exit flicker, sustained until quit; the
   tester now describes the scene jumping right in the right eye and left in the left
   eye, which is an eye swap, not a mono or stale image.
2. **Reproduction identity:** build `205-g6d858c8f-dirty` (ledger code of `7226a613`),
   `[Stereo] RingLedger=1`, profile unchanged; several note closes standing and crouched,
   the last one crouched.
3. **Hypothesis and counterprediction:** the drain over-consumes (checkpoint 1). It would
   show removed draw ids that later present. Alternative: tags lost from the ring, which
   would show a reconcile failure.
4. **Change identity:** none; measurement only.
5. **Results:** both reconcile sides held (no uncounted path). The drain removed the
   correct tag (over-drain refuted). The episode is a repeating four-present cycle at
   about 3.3/s: an empty pop whose image's tag arrives a few ms later, then TOOK,
   then one present with the left image in the right eye, then TOOK plus a one-tag drain.
   Push-to-present margin for -1 tags: median 0.8 ms in the episode against 10.7 ms
   crouched and 7.6 ms standing before it; ring depth before the pop mostly 1 against 2-3.
6. **Status and remaining scope:** open. Onset mechanism measured; the cause of the margin
   collapse (and why a pause restores it) is not. Next: host-model the late push, then a
   default-off late-tag repair lever and one headset A/B. See VR-80-PLAN checkpoint 3.

**2026-09-13 candidate F-late, host-verified (not yet headset-tested).**

1. **Symptom identity:** the run 5 cycle above.
2. **Reproduction identity:** host model only (`tools/reentry-pair-host.ps1`, 240 checks),
   compiling the shipped `reentry_pair.inc`.
3. **Hypothesis and counterprediction:** removing the late tag at the next present, when
   that present's c5 confirms the other eye, removes the wrong eye and the drain. It fails
   if any earlier schedule gets worse or the late schedule still shows a wrong eye.
4. **Change identity:** `[Stereo] LateTagRepair` (default 0), `reentry latetag on|off`,
   F10 Display checkbox; ledger `OWE` / `LATE-REPAIR`.
5. **Results:** with the lever off the model reproduces run 5's cycle present for present.
   On: no wrong eye, drain or wrong record on the late schedules (still and walking); the
   refused present remains, left to `HoldUntagged`. All 72 earlier schedules are no worse.
6. **Status and remaining scope:** candidate, armed in the tester's ini for one headset
   question. It treats the onset's effect, not the margin collapse behind it.

**2026-09-13 headset run 6: F-late fires, a held left image remains (plan checkpoint 4).**

1. **Symptom identity:** still a crouched note-exit flicker, reported less frequent, sometimes
   ending on its own, and now LEFT eye only, jumping left.
2. **Reproduction identity:** build `207-g27f5b714-dirty` (F-late of `0641c48a`),
   `LateTagRepair=1`, `RingLedger=1`, profile unchanged.
3. **Hypothesis and counterprediction:** F-late removes the whole cycle. It fails if TOOK
   cycles remain or another eye fault appears.
4. **Change identity:** none during the run.
5. **Results:** 41 of 44 late tags repaired in the last 10 s; TOOK cycles nearly gone. The
   repaired present goes out `HOLD`: under `SharedWait=0` it delivers the previous slot, the
   untagged LEFT image, so the left eye misses one image per cycle (`pushed eye +1 TWICE`).
6. **Status and remaining scope:** part 2 built: the repair relabels the waiting capture slot
   with the late tag's eye and record (pipelined modes only). Host model with delivery: no
   held or wrong-eye image. Awaiting the headset. The margin collapse remains unexplained.

**2026-09-13 headset run 7: F-late with the slot relabel, headset-confirmed.**

1. **Symptom identity:** the crouched note-exit flicker.
2. **Reproduction identity:** build `209-g56411e09-dirty` (`56411e09`), `LateTagRepair=1`,
   `RingLedger=1`, profile unchanged; many note closes standing and crouched.
3. **Hypothesis and counterprediction:** relabelling the waiting slot removes the left-eye
   hold. It fails if refused relabels or held images remain in the episodes.
4. **Change identity:** none during the run.
5. **Results:** reported as essentially clean, with an occasional single-frame flicker a
   few times. Late-tag episodes still occur (29, 19 and 23 repairs in three 10 s windows)
   and every repair relabelled its slot (71 relabelled, 0 refused); every reconcile held.
6. **Status and remaining scope:** headset-confirmed behind `[Stereo] LateTagRepair`
   (default 0 until the merge decision). The occasional single frame is unattributed; the
   ledger's expired owes (3 to 6 per episode window) are the first suspect. The margin
   collapse behind the late tags is still unexplained.

**Status.** Fixed and headset-confirmed behind `[Stereo] LateTagRepair` (VR-80); the cause of
the margin collapse and the residual single frame are VR-99.
Plan and checkpoints: [VR-80-PLAN](VR-80-PLAN.md) (section 10). The earlier resume plan is
[FLICKER_FRAME_DROP_AND_RESUME_PLAN](FLICKER_FRAME_DROP_AND_RESUME_PLAN.md).

## 8. Keeping this reference useful

For each new flicker report, append an entry with:

1. **Symptom identity:** visible surface/eye, affected geometry, direction,
   duration, trigger, and distinction from an existing issue.
2. **Reproduction identity:** build/source/dirty state, DLL and config hashes,
   resolved settings, runtime, refresh, render size, and local artifact location.
3. **Hypothesis and counterprediction:** which observation would falsify it,
   with the measured population and frame/eye association made explicit.
4. **Change identity:** minimal behavior change, original and fixed commits,
   diagnostic versus integration build, and preserved configuration.
5. **Results:** old-code negative control, host/simulator evidence, desktop
   verdict, headset verdict, and what did not execute or remains untested.
6. **Status and remaining scope:** confirmed, measured, open, parked, or
   retracted; linked ticket and separate adjacent symptoms.

Update the routing/status table when a later result supersedes an earlier one.
Keep the failed prediction and the reason it failed. Never turn a clean counter,
a fix-shaped commit subject, or a desktop recording into a broader headset claim
than the evidence supports.

### VR-117: the pause menu on the HUD window, 2026-09-15

1. **Symptom:** with in-game menus on the HUD window, the projection dropped to the
   mono screen for a few presents at the menu's OPEN and again at RESUME (the
   "stereo reloading" of headset run 47 on the abandoned PR #12), and once the ride
   was granted the riding menu BLINKED at half the display rate. Whole view, both
   eyes, distinct from every eye-tag issue above.
2. **Reproduction:** simulator, `dvr-xrsim` 90 Hz, 2750x2850, `stereo reentry`, the
   sewer level via `console open L_PrsnSewer_P`; build 275-g52e2414d-dirty (the
   VR-117 tree); logs under `D:\dvr-data\logs\hud-redo-run*.log` on the dev PC.
3. **Hypothesis and counterprediction:** (a) the open gap: the owner read publishes
   50 ms after the menu flag and a health check that blinked with the per-present
   armed flag cancelled the open-gap stand-in, so the runtime's fallback fired
   first; falsified by a log that showed `standIn=open pending` surviving to the
   `ui/ride` line. (b) the blink: paused, the re-entry gates alternate SINGLE and
   DOUBLE draws and every other present hands the runtime no texture; the HUD
   block ran before the zero-layer hold and was skipped on held presents; the
   counterprediction was a shot on a held present with `projection, quad, quad`.
4. **Change:** health = the recent-redirect window alone; the open-gap stand-in holds
   its 300 ms once started; the HUD anchors block moved after the hold path
   (`openxr_runtime.cpp`, "41.x (Dishonored, VR-117) HUD anchors"). Diagnostic
   build; configuration unchanged.
5. **Results:** `pause-ride.xrs` 31/31: through a 12 s pause `stereo: beat out/s=91
   L/s=46 R/s=45 mono/s=0 none/s=45` (a live pair at 45 Hz, the other presents
   held), the shots carry `projection, quad, quad` on the open and on the held
   frame, resume keeps `projectionViews 2` with `eyeAgeL/R 0`; `hud menu off` gives
   the mono-screen pause as the A/B. Not measured: the headset. Note the sim's
   `projStaleSubmits` climbs during a ride (634 over 12 s): the held projection is
   re-submitted on every textureless present and the sim counts it as stale.
6. **Status:** simulator-confirmed, headset pending; VR-117. Adjacent: the paused
   world redraws every other present (the re-entry gate's camera-silent single
   ticks), which the mono-screen pause used to hide behind its quad.

### VR-117: the HUD flickering between the window and the frame, 2026-09-15

1. **Symptom:** in gameplay the HUD elements alternated between the HUD window and
   the frame (painted into both eyes at infinity) at roughly 10 Hz; `Element.all=frame`
   showed no flicker. Both eyes, HUD surface only, the world steady. Distinct from
   every eye-tag row above: the eyes were fresh, the HUD's ANCHOR changed.
2. **Reproduction:** first headset run, Quest 3 through VirtualDesktopXR at 90 Hz,
   build 278 of the VR-117 tree, `stereo reentry`, `[Hud] Panel=1`; the log's
   `hud/beat` read `presents=441 armed=400` (and similar in every 3 s window) while
   `stereo: beat` read `none/s=6..21`.
3. **Hypothesis and counterprediction:** the redirect's arm term followed
   `dvr::hud::gate()`, which is the per-present eye TAG; under re-entry 6 to 21
   presents a second carry no tag by design (the held presents that re-submit the
   previous projection), and each one disarmed the redirect for the next present, so
   that present's HUD draws went into the frame. Counterprediction: on the
   simulator, with the same `none/s`, `armed` must equal `presents` once the gate
   follows the projection MODE instead of the tag; it did not before the change
   (`presents=450 armed=433` on the sewer level).
4. **Change:** `dvr::hud::set_projection_mode` recorded by the runtime beside
   `set_gate`; `hudcap::end_frame` arms on `projection_mode() || menuOverride`
   (`hud_capture.cpp`); the `hud status` line reports both (`xrGate`, `eyeTag`). The
   ride predicate keeps the tag's age as its HOLD leg (unchanged). Configuration
   unchanged.
5. **Results:** simulator, sewer level, 90 Hz: `hud/beat presents=461..471
   armed=461..471 redirected=21.0/present empty-while-armed=0` in every 3 s window
   over 40 s with `stereo: beat out/s=154..156 L/s=77 R/s=77 none/s=0..1`; `hud-panel.xrs`,
   `pause-ride.xrs` 31/31 and the new `wheel-ride.xrs` 27/27 pass on the build.
   Second headset run (2026-09-15, release build 278 of the tree, the repo default
   ini): no window/frame flicker reported in gameplay, the weapon scroll and the
   grip-hold loadout stay in the window.
6. **Status:** headset-measured cause, simulator-verified fix, headset-confirmed;
   VR-117. Adjacent, same run: the weapon scroll and the grip-hold loadout
   dropping the world to the mono screen for a second were the power wheel
   (`context=Wheel blocked=1 rides=0`, 48 times); the wheel now rides the window
   (`WindowWheel=1`, `wheel-ride.xrs`), not a flicker row.

### Latest-log follow-up for the September 13 reports

A second snapshot requested during the investigation is a longer continuation
of the same process/build, not an independent reproduction. It raises the
latest-state gameplay gap count to 33 (27 at xrEndFrame), retains clean reported
pair/stale counters and a clean draw-source mirror window, and contains no V
markers. It also shows another menu candidate reset. It still has no recorded
weapon re-adoption after the first pause. Exact artifact identity and limits are
saved in the linked frame-drop/resume plan; visual cause and relock duration
remain unconfirmed by an event-local trace.


### Load startup implementation follow-up, 2026-09-13

1. **Symptom:** mono after a load until vertical movement, with early hand tracking,
   delayed weapons and a freeze. Separate from the confirmed VR-80 note-exit fix.
2. **Reproduction:** run 8, merged baseline `20cc4a98`; preserved artifact/hash and
   precise timestamps in [LOAD_STARTUP_IMPLEMENTATION](LOAD_STARTUP_IMPLEMENTATION.md).
3. **Hypothesis/counterprediction:** the controller possessed a readable player before
   the first pawn event. Event-only capsule discovery blocks startup; an independent
   validated controller source should start stereo without movement, subject to the
   existing menu/view/cinematic gates. Continued mono with valid capsule liveness
   would identify another gate. Repeated name scans contribute to discovery cost;
   total freeze elimination is not established.
4. **Change:** controller-based capsule source, independent script sampling with
   bounded table refresh, matched capsule/clamp ownership; separate bounded positive
   name-ID cache. Both levers default off with F10 A/B and ini persistence.
5. **Results:** 48 production-function startup checks pass; legacy pawn behavior
   fails the no-input regression. Cache off requires 4000 reads for the warm query,
   cache on one. Pairing 248, menu 43 and clamp 19 checks pass; release build,
   exports, ini golden and lint pass. No new headset or in-game simulator run.
6. **Status:** implemented, visual validation open. First test enables only
   PawnFromController; cache timing is a separate cold-launch comparison. Linear
   ticket pending unavailable access. No new merge or claim of instant weapon lock.


### Confirmed startup and note observer follow-up, 2026-09-13

1. **Symptom:** startup stereo no longer needs vertical movement, confirmed on the
   headset. Fast note opening/closing was reported delayed after a save load.
   This is the mono-transition surface, separate from eye-tag pairing.
2. **Reproduction:** build `vr33-hands-working-215-g20cc4a98-dirty`, banner verified
   against the install manifest before analysis. Archived in ignored
   `build/load-startup/playtest-20260913-173516/`; log SHA256
   `1d25a16094c878e93752e310b691ce26625fdb1bca126d617402c32142714172`.
   Installed ini SHA256 `5e1555d8e4e1b8bbe9e36e53715e5199ed64e4a8e192f7827c5f301e8349cdaf`.
   PawnFromController=1, CacheNameLookups=0, NoteFastMono=1; full ini diff confirms
   the prior note/pairing/menu options were preserved. Same 90 Hz profile.
3. **Hypothesis/counterprediction:** the append-only UI movie table exhausts across
   reloads, so new notes never reach the fast path. At 14397.578 s the old pNote
   is dropped; at 14400.265 s the table reports full at 48. Subsequent note close
   events occur without note flag transitions. If the replacement observer watches
   the new pNote yet the delays remain, table exhaustion is not the whole cause.
4. **Change:** the UI discovery scan replaces its population after refreshing the
   live-object table; new movies reclaim dead slots and validate class plus full
   FName against IsLiveObject. A table lock prevents partial replacement being read
   by Present, which skips a busy scan. Capacity is bounded at 128 for simultaneous
   movies; increasing capacity alone would not fix the lifetime leak. The existing
   NoteFastMono lever and loading timeout logic are unchanged. Logs identify watched
   notes and note-visible transitions. No new engine-memory writer or name-cache
   activation. Session instructions are persisted in AGENTS.md and private settings.
5. **Results:** 18 production-function checks pass, including 300 reloads, dead
   slots, address/FName reuse, full-live capacity, immediate note open/close and
   ordinary loading/observer-expiry safeguards. The old add-instance function fails
   four checks. This explains a repeatable missing-observer path; restored headset
   note timing still needs the next run. Startup is headset-confirmed; instant
   weapon tracking and total load-freeze removal are not established.
6. **Status:** startup confirmed; note reload repair awaiting headset verification.
   New Linear ticket/update is blocked in this task: no Linear connector is exposed,
   and browser startup fails with helper_sandbox_lock_failed. No invented ticket
   number, new commit, PR, or merge. The prior VR-98 issue remains the known context.


**Installed note-observer candidate:** `vr33-hands-working-215-g20cc4a98-dirty`,
compiled `Sep 13 2026 17:43:41`. The describe string is unchanged because this
working tree is uncommitted; verify the compilation timestamp as well as the tag
before interpreting the next run. DLL SHA256
`7a2dd73f97bb5fec655071198c335f70af2e2a8b74c8869f8e43ade7877c45ff`.
Backup and source snapshot: `build/note-observer/install-20260913-174444/` (ignored).
Release build and nine exports pass; pairing 248, menu 43, startup 48 and observer
18 checks pass. Ini golden and lint pass. The entire installed ini is byte-identical
to the preceding install, with CRLF verified; existing diagnostics remain armed.

**Next launch, one question:** after loading a save again through the pause menu,
do repeated note opens/closes still switch promptly between mono and stereo?
Load normally, reload the save, then open/close a nearby note several times.
Prompt transitions support restored note discovery across reloads. Delays with a
watched pNote and changing note-visible flag implicate a downstream gate; delays
without those markers implicate discovery. A flash or misplaced weapon indicates
a separate transition/retention regression. The agent reads the log; the tester
runs no commands. No game was launched during implementation.


### Reload right-eye displacement after note-observer repair, 2026-09-13

1. **Symptom:** fast notes and stereo without vertical movement are headset-confirmed,
   including notes after a save reload. A save reload caused continuous rightward
   flicker in the right eye until pause/resume. Route through section 1's one-eye
   pause/load/rearm row. This is not a repeat of the observer-capacity failure.
2. **Reproduction:** banner verified as build `215-g20cc4a98-dirty`, compiled
   `Sep 13 2026 17:43:41`, DLL `7a2dd73f97bb5fec655071198c335f70af2e2a8b74c8869f8e43ade7877c45ff`.
   Archive `build/note-observer/playtest-20260913-175403/` (ignored); log SHA256
   `fc78484e3310e9ca7eff4e5b9df60a32a118e3b3e901a6869a013b72399409ad`.
   Installed ini unchanged, SHA256 `5e1555d8e4e1b8bbe9e36e53715e5199ed64e4a8e192f7827c5f301e8349cdaf`:
   90 Hz, SharedWait=0, LateTagRepair=1, PawnFromController=1, NoteFastMono=1,
   CacheNameLookups=0. Reload returns GAMEPLAY at 15966.062 s; pause begins
   15989.109 s and GAMEPLAY resumes 15990.703 s.
3. **Hypothesis/counterprediction:** repeated SINGLE ticks expose adjacent R/0 labels
   on L/R images. Of 288 unique ledger rows sampled in the reload episode, 23
   popped explicit zero tags, all promoted to right by the within-tick invariant;
   22 had a preceding R-tagged unknown step with a 6-7 uu camera residual. Example
   P4588-4591: L matches its camera, R repeats the left camera (6.79 uu from its
   record), zero has the -6.82 uu right step, then L matches again. No ring rejects,
   clears, late owes, or repairs occur in the fully covered 15972.953-15982.968 s
   window; every mutation reconciles. Queue depth 4-6 is measured, not a cause.
   Earlier suspicion of depth-clearing is excluded. The submitted +1 doublets
   raise the runtime's LEFT-stale count even though the reported moving image is
   RIGHT: a mislabeled left view enters R before the real R, so that counter's
   eye is not the symptom's eye. The engine-side owner of the displacement is open.
4. **Change:** `[Stereo] SingleTagRepair` defaults off with a persisted F10 Display
   toggle. A one-present candidate requires a preceding measured L, an R-tagged
   camera repeat within 0.1 IPD, and a stored R position one IPD to its right. Only
   an adjacent zero tag with the robust right step and camera matching that stored
   R position confirms it. The strict tolerance is a conservative candidate guard,
   not a calibrated engine constant. A serial/record-checked buffered R capture is
   retired to untagged HOLD, and the current image gets that complete R record.
   No tag search/drain, camera write, or forced alternation. Sync/SharedWait=1 and
   identity mismatches decline; failed capture retirement leaves labels unchanged.
   Lifecycle reset clears the candidate. The log counts observed/fixed/refused.
5. **Results:** host replay of the observed geometry passes 23 checks, including
   wrong-eye suppression, full pose-record recovery, nonadjacent/moving/missing
   camera negatives, and capture serial/record/mode guards. Disabling the candidate
   reproduces wrong-eye delivery and wrong record. Pairing 248, note observer 18,
   menu retention 43, startup 48 checks pass. No simulator game launch (tester owns
   launches); headset validation of this candidate is pending. Note transitions in
   the recorded run reached the state machine in 0-16 ms, including the post-load
   note. No claim that every render surface is correct from the camera trace alone.
6. **Status/remaining:** note observer fixed and confirmed; reload right-eye repair
   is a candidate. Next launch asks whether a save reload stays stable without a
   recovery pause. If it persists with zero candidate observations, the guard/path
   missed it; fixed counts with residual flicker point beyond this sequence.
   Weapon tracking freeze remains separate: startup gaps 3341, 1811, 1266, 2141,
   2852 ms, mostly game-thread waits, with one render execution gap; discovery scan
   498 ms. Name-cache optimization remains OFF for this isolated flicker test.
   Linear remains unavailable in this task; no new number invented, commit, PR,
   or merge. Existing VR-77 SINGLE scheduling and VR-99 residual timing are related
   context, not a claim that either ticket already contains this finding.


**Reload candidate installed:** same describe tag `215-g20cc4a98-dirty`, banner
compile timestamp `Sep 13 2026 18:01:15`. DLL SHA256
`27e691b26829f852da59c998187d3139a653d7317ff7dcc28067398617d70162`;
ini SHA256 `2e14a8f0e42145e7cae96f8bd10cdd4ccd508cc4ac78b0255e4198ba9a8cd365`.
Backup, full ini diff and source snapshots are under ignored
`build/single-tag/install-20260913-180438/`. Only installed ini addition:
`[Stereo] SingleTagRepair=1`. Full section/key diff, byte verification and CRLF
checks passed. Startup cache stays off; note/pawn/late-tag settings stay enabled.
Release build, nine exports, lint and ini golden pass. Candidate headset result
pending; the agent did not launch the game.

**One launch question:** after loading normally and reloading the save once,
does the right eye remain stable for about 20 seconds without a recovery pause?
If stable, correlate with single-tag observed/fixed/refused counts. If it flickers,
let the episode run briefly, then pause/resume as before; counts distinguish a
missed/refused candidate from flicker that persists despite confirmed repairs.
Do not combine this launch with the startup-cache timing A/B.


### Reload confirmed; pause crash recurred, 2026-09-13

1. **Symptom:** the tester reports correct startup stereo, fast notes and stable
   stereo after a save reload. Opening pause at the end crashed before the menu
   appeared. The successful eye result and the GC crash are distinct observations.
2. **Reproduction:** banner verified against the install manifest before reading
   the run: `vr33-hands-working-215-g20cc4a98-dirty`, compiled
   `Sep 13 2026 18:01:15`. DLL SHA256
   `27e691b26829f852da59c998187d3139a653d7317ff7dcc28067398617d70162`;
   ini SHA256 `2e14a8f0e42145e7cae96f8bd10cdd4ccd508cc4ac78b0255e4198ba9a8cd365`.
   Logs, screenshot and dump preserved under ignored
   `build/single-tag/playtest-crash-20260913-181413/`. Current log SHA256
   `9501db99d75a730c6533a1fe8608f34069ef95556534a5ad4b54ee587482770e`;
   dump SHA256 `e78c3c3887f6ed01a6d7cd7296e628f36553ff9f2273aa6f41b8eba5db08fe4e`.
3. **Hypothesis/counterprediction:** the adjacent R/0 repair should suppress the
   previously measured wrong-R capture. A visible failure with confirmed repairs
   would put the remaining fault beyond that sequence. The pause crash matches
   historical VR-96; its timing alone cannot identify a writer or establish that
   the new stereo repair caused it. The 1.0 bit pattern alone is not attribution.
4. **Change:** no code, DLL or installed ini change during crash triage. Existing
   SingleTagRepair=1, LateTagRepair=1, NoteFastMono=1 and PawnFromController=1
   remain active; CacheNameLookups=0. Updated the evidence and priority handoff.
5. **Results:** ten note cycles, five before reload and five after, reach the
   corresponding LOADING/GAMEPLAY state in 0-16 ms. The only printed single-tag
   result is observed 1 / fixed 1 / refused 0 at 17147.625 s, for R draw/record
   2113, before reload. Reload returns GAMEPLAY at 17171.250 s and is reported
   stable through the final pause at 17246.765 s (including intervening notes).
   Do not infer that the original fault recurred after reload from this visual
   pass. At 17246.828 s the first AV reads 0x3F800008 at engine RVA 0x65894;
   its EAX is float 1.0's bits. The dump is written 125.672 s later and its
   exception is engine RVA 0xAF6A73 reading 1. The initial reference slot and
   token storage are absent, so the dump cannot identify the corrupting object.
6. **Status:** tested reload stability and fast notes confirmed for this build;
   VR-96 is the first-priority unresolved crash. Startup weapon freezing remains
   open. No crash fix or additional test is claimed. Linear updates are prepared
   but not sent: no callable Linear tools and UI runtime startup failure. No
   new ticket number invented, commit, PR or merge. See ENGINE_NOTES for the
   initial-fault evidence and the limit of the delayed dump.


### Pause crash after successful reload testing, 2026-09-13

1. **Symptom:** stereo and notes remain visually confirmed; opening pause can
   crash after a crouched reload. This is memory corruption, not an eye-tag fault.
2. **Reproduction:** build 215, compiled 18:27:13, first-fault full-memory dump;
   evidence archive `build/crash-triage/playtest-20260913-183257/`.
3. **Cause/counterprediction:** old hand-control addresses became three upgrade
   objects. Crawl release wrote strength 1.0 before stale-control validation,
   corrupting a class reference. A fresh liveness AND retained identity check
   should prevent those stores. A crash despite rejected stores would require
   examining the next owner/writer, rather than blaming the eye repair.
4. **Change:** guard both crawl strength stores before writing; log refusals and
   trigger normal control rediscovery. No note or renderer behavior change.
5. **Results:** host 13 checks pass; old writer fails three regression checks.
   Menu 43 checks and release/exports/lint pass. Fix installed: build 215 compiled
   18:43:51; whole ini byte-identical, CRLF verified. No agent game launch.
6. **Status:** identified code defect fixed, headset verification pending. One
   launch tests pause after a crouched reload. VR-96 remains the top priority;
   Linear update is prepared but unsent. Full account in ENGINE_NOTES's
   "Pause GC crash: recycled hand controls corrupted upgrade objects" record.


### Final headset verification and default promotion, 2026-09-13

The verified 18:43:51 build completed a clean run with 11 pause openings and no
exception. The crawl writer refused nine stale control updates over three edges
and performed 57 updates to validated controls over the remaining edges. Thus
the stale-pointer guard was exercised, rather than merely failing to reproduce
the trigger. The tester confirmed normal behavior. VR-96 is fixed for this
observed cause; later distinct crash signatures must be investigated separately.
Archive `build/crash-triage/playtest-pass-20260913/`; log SHA256
`a082eae06a07fb3fcbba4656f8933d82232e9b55f06d656469b6338a249bd6be`.

The maintainer explicitly approved commit, PR and merge of the current branch,
and promotion of the complete installed settings/F10 profile to repo defaults.
This supersedes the earlier default-off disposition for these tested levers.
`release/dishonored_vr.ini` is the byte copy of that installed CRLF profile;
WriteDefaultIni and the golden match, including diagnostics, calibration values,
D:\dvr-data, fast notes, late/single-tag repair and retained menu identities.
The name cache stays off. No config version bump rewrites existing settings.
The real default writer runs in a standalone x86 host and its output is compared
byte-for-byte to the installed and packaged files. Package generation now includes
that exact ini. Presence/migration sentinels remain distinct from value defaults.
No release or milestone is declared. Linear API synchronization remains pending;
the PR's Fixes link may update VR-96 through the integration, to be verified next
session. The next feature is physical head movement during cinematics.

## VR-70 cinematic pitch translation (2026-09-13)

Surface:smooth world-camera movement with head pitch,not intermittent eye flicker.
Build220-gdf783ca8 is headset-confirmed for stable boat head rotation,including
before the stick prompt. Its3210 writes restore without refusal. Opposite vertical
translation remains. Current candidate removes the CANCEL neck term only from
the authored-camera position request; normal gameplay remains compensated.
This extends the VR-78/91 lesson that an absent engine arc must not be cancelled.
Installed pivot0.321m below/0.062m behind; actual headset acceptance of this
correction is pending. Both requests are logged as requests,not render evidence.
See CINEMATIC_HEAD_TRACKING.md for archive,build manifest and one-question test.

## 2026-09-13: VR-103 cinematic/dialogue mono interruptions

Reported: both headset eyes lose stereo during a choice scene until selection;
boat/Emily handoffs briefly switch stereo to mono and back. Measured on accepted
build222 cf0d00dc20:37:01: cinematic latch alone parks the runtime; a no-lock
handoff waits2s to clear. InDialog also outlives a silent PVR interval classified
LOADING. This is a presentation eligibility hypothesis, not tag-ring skew.
Candidate separates scene permission from input permission using current script
states and scene activity. Default off; 24 x86 policy checks. Headset result open;
continuous depth is predicted, further mono intervals falsify completeness.
Full sources, archive, guards and next test: [VR-103 plan](STEREO_STATE_TRANSITIONS.md).

## VR-103 headset acceptance, 2026-09-13

Candidate225 d015b1aa21:05:03 verified against installed manifest. The reported
boat, Emily prompt, dialogue-choice and gameplay handoffs are headset-confirmed
continuous stereo. Log shows InDialog Listening0 and Choosing1 remain STEREO,
including view=0 with fresh scene uploads. Walk handoff stays STEREO despite
latch=1. Runtime quad transitions later correspond to notes and menus. Clean
unload. Both logs archived in build/stereo-state/playtest-20260913-212231.
The user explicitly approved PR55 merge. Cinematic FOV shrinking and native
hand/arm animation are separate follow-up work, not failures of this result.

## 2026-09-13: VR-50 shrinking cinematic frame

Separate from confirmed VR-103 mono interruptions: reported square shrinks during
speaker/choice framing. Accepted225 log measures FOV readback down to36.6 degrees
versus108.1 target. Default-off LockFov now requests a stable final cinematic
camera FOV and matching host claim; acceptance remains open. No eye-tag change.
See [source evidence and next test](CINEMATIC_FOV_AND_HANDS.md).

## 2026-09-13: VR-50 exit shrink isolated

Build230 confirmed in-scene FOV and native arms; exit alone shrinks then expands.
Measured override release in Walk at30273250 immediately exposes52-degree sensor,
which returns to107.6 after1125ms. Candidate bridges that same-owner native blend,
not an eye-tag fix.38 FOV/handback checks pass; exit result still pending. Exact
sources, archive and falsifiable combined test are in CINEMATIC_FOV_AND_HANDS.md.

## 2026-09-13: tilted cinematic swivel (VR-105)

Surface: scene camera, smooth axis coupling, not duplicated/stale eye imagery.
Build232 shows authored pitch -57.78 and roll32.61 becoming roll54.40 after
head composition plus pitch replacement. Upright composition and independent
roll suppression are candidates; no headset acceptance yet. FOV exit improvement
is reported successful; mantle was untested. See CINEMATIC_FOV_AND_HANDS.md for
archived identity, numeric evidence, counterpredictions and the next test.

## 2026-09-13: VR-106 steep-pitch standing roll arc candidate

Smooth scene-camera motion, not eye flicker. Standing uses a0.321/0.062m neck
pivot; crouch0/0 has no modeled arc. Two legacy branches fall back to rolled
axes at horizontal forward magnitude0.2 (about78.46 degrees pitch). The x86
control reproduces -20.633/+20.633uu lateral motion at85-degree pitch and
-/+40-degree roll; zero crouch pivot yields zero. No new headset measurement.
Default-off UprightPitchArc computes pitch-only compensation and keeps camera
position axes upright at steep pitch; true eye-right stays rolled. Ordinary
pitch parity and12 numerical regressions pass. Exact-pole position refusal is
logged; normal pitch is clamped short of it. Full plan, limits and staged
playtest sequence: [standing arc](STANDING_PITCH_ROLL_ARC.md). Parent PR56 and
this child remain unmerged pending separate testing.
## 2026-09-14: VR-109 cinematic activity regression

Build234 confirms comfort/FOV/free look but reports mono exit recurrence. At
752187 Walk is valid, pawn live, menu0 and sceneFresh1, yet view0. Quad lasts
752203-753265 (1062ms). DvrScriptViewLive was counting writes that cinematic
ownership intentionally suppresses, not the continuing PVR dispatches.
Candidate records yielded dispatch activity with the existing750ms limit;
menu/pawn gates unchanged. If a no-menu live-scene handoff still goes mono,
the correction is incomplete. Host checks pass; headset result open.
Exact archive, source changes and next test: CINEMATIC_FOV_AND_HANDS.md latest
section. The later journal/movie transition is separate, not a no-menu example.

## 2026-09-14: VR-109 mono handoff confirmed

Verified237-g563e14d6, Sep14 07:27:03: the reported cinematic mono interruption
is resolved on the headset. Logs/INI archived at cinematic-regression/20260914-073936
under build. Counting yielded PVR activity remains the accepted correction.
Character-heading drift did not resolve; it is a separate movement issue, not
stereo instability. VR-110 offers selectable native head-facing movement.


## 2026-09-14: main-menu/loading projection leakage (VR-107/108, VR-74/71)

Surface: whole view changes mono/projection while a menu/loading UI remains.
Reported main-menu stereo after several seconds, underground camera and lost
stick navigation; brief loading stereo before Continue. Historical build60
17:25:41 logs measure a -4153.935 uu clamp offset and stale-menu clearance after
1503ms; they are not results from this candidate. Hypothesis: background scene
activity overrides UI ownership. Candidate uses reflected UI/movie lifetime as
a veto across rendering, camera and input, plus configurable mono anchoring.
Counterprediction: if the same fault occurs with ui/surface blocked=1, a consumer
bypasses the veto; if blocked=0 during visible UI, the ownership classifier is
wrong. Persistent unknown or loading after Continue also fails the policy.
Status: implemented, headset test deferred; no confirmed fix. Existing eye-tag
and weapon hypotheses are not reopened. Recoverable plan and exact controls:
[mono UI state](MONO_ANCHOR_UI_STATE.md).

## 2026-09-14: final mono UI candidate updated

Build250-g78abb4fa incorporates accepted cinematic mono handoff and standing roll.
UI ownership also gates stale-cursor pad nudges. Whole-view mono/projection
leakage remains untested; no eye-tag/weapon hypothesis reopened. Installed with
complete INI/hash/CRLF evidence, host suites and standalone XR smoke passing.
See MONO_ANCHOR_UI_STATE.md for the main-menu test and subsequent loading test.

## 2026-09-14: build250 loading guard does not release

Whole view remains mono in gameplay despite tracked weapons. Verified250 log
holds Loading from5282609 until exit5313921. The persistent movie-service pointer
was mistaken for presentation lifetime. Main-menu anchoring is reported working.
Native overlay draw uses service slot1c; candidate reads its verified active field
and retains the lease only through active presentation. No hand-tracking gate.
Counterprediction: presenting0 with load mode/transition cleared must release;
presenting1 through Continue must remain mono. Unknown layout is logged/refused.
See MONO_ANCHOR_UI_STATE.md and ENGINE_NOTES for derivation and archived run.

## 2026-09-14: build252 active-field hypothesis retracted

Verified252 still holds mono in gameplay and after pause. Movie service+130
stays1 throughout; mode0/transition0/started0/hints1 cannot release the lease.
The overlay draw-enabled predicate is not presentation lifetime. Candidate
follows script Engine.WaitMovie to the manual-reset movie-completion event.
Zero-time observation is nonconsuming and tested on real host events. Expected:
completion releases mono after Continue, not before; other menu guards remain.
Full derivation, archive and failure record: MONO_ANCHOR_UI_STATE.md.

## 2026-09-14: VR-99 recurrence on build254 (open)

Route: section1 whole-view both-eye after-note/crouched symptom, detailed3.15.
Tester reports approximately one in five note closes, improved by standing.
Verified254-g43551c09 compiled08:50:11; installed DLL matched. Archive:
build/mono-ui-test/playtest-20260914-085908. LateTagRepair on, SharedWait0.
At7046281 the 10s ledger has27 owes,22 repairs,5 expired,22 slot relabels and
zero refusals; at7056281 it has107 owes,102 repairs,5 expired,102 relabels,
zero refusals. Eye+1 duplicate total rises129 at7045421 to147 at7054437.
The repair is active and not sufficient for every transition. No evidence
that mono anchoring itself causes eye swaps; UI does not reopen after these
closes. Exact visible frame is unmarked, so expired owes are still a suspect,
not proven cause. No pairing change is justified solely by those counters.
Next: use a dedicated crouched-note run, join complete ledger around expired
owes and duplicate pushes, and reproduce the failing schedule in the host
model before changing repair. Keep the accepted late-tag/relabel fix enabled.
Separate initial-load mono dip is a successful-head-write heuristic failure,
not a movie lease retrigger; MONO_ANCHOR_UI_STATE.md records the evidence and
candidate fallback. Native drop rejection is separate VR-111.

## 2026-09-14: build256 follow-up and VR-112

Verified256-gd98bcf36 compiled09:15:20; archive
build/mono-ui-test/playtest-20260914-093057. The tester reports a few brief
crouched-note whole-view flickers, each recovering quickly. VR-99 remains
residual/open; no pairing change was made in256, so improvement is reported,
not attributed to its read-only drop diagnostic. Mono transitions accepted.

Separate weapon reattachment row: after cinematic/level travel, crossbow loses
tracking while sword works, not recovered by pause. Current components exist,
but repeated crossbow scale mismatch approximately0.0468 prevents its contract
from being accepted (tolerance0.005); sword accepts. New VR-112, read-only
matrix breakdown candidate. Evidence, counterprediction and next test in
MONO_ANCHOR_UI_STATE.md latest entry. No scale tolerance relaxation.

## 2026-09-14: VR-112 diagnostic establishes differential lens

Build258 verified against installed hash and banner; archive
build/mono-ui-test/playtest-20260914-101645. Crossbow tracking failure reproduced.
Matrix trace identifies a symmetric view-plane stretch1.046635, predicting the
translation mismatch within0.0014uu; hand bridge and sword remain rigid.
Candidate AttachViewLens removes only a validated lens before strict matching
and hand correction. No arbitrary scale tolerance expansion. See latest
MONO_ANCHOR_UI_STATE.md for implementation, counterexamples and next test.
Status: measured mechanism, host-tested candidate; headset result pending.

## 2026-09-14: partial left-eye weapon surfaces after lens correction (VR-112)

Reported on verified build260-g7a0bbd46, compiled Sep14 10:23:35, DLL
SHA256 a37a0589c58697480d38253a112eccb7abfff2da5833d2576dd4b3fa4beee40b.
Both logs archived at build/mono-ui-test/playtest-20260914-104623.
Crossbow unsheath/tracking is headset-confirmed. New report: parts of both
weapon models intermittently disappear in the left eye, even while still;
hands remain intact. This is distinct from whole-view VR-99 note flicker.

Measured: all84 sampled draw/prediction matrix pairs (42 per hand) in this run
have identity relative scale within float noise, unlike the real1.046635 lens
in build258. Maximum eigenvalue deviation is9.72e-7 for sword and3.56e-7 for
crossbow. Build260 nevertheless applied the fitted near-identity correction.
Late-run no-delta stays126 while successful corrections continue; no restore
failure. Suppression also continues, but its population is not proof of the
reported partial-surface cause. Do not disable all auxiliary draws: prior
experiments lost legitimate lighting/colour contributions.

Candidate: reject identity lens fits within32 float epsilons and use the exact
original correction path. Keep real lens cancellation and existing instance,
rotation, position and scale guards. No new engine writes, stale matrix cache,
or depth-state change. This removes a measured false positive; whether it
caused the visible flicker still requires the headset. Two identity/roundoff
regressions fail before the change and pass afterward; real-lens/axis and
world-instance tests continue passing. AttachScaleTrace adds bounded per-eye,
per-hand main/auxiliary lens decisions with common-eye and depth-state values.

Next test: with both weapons drawn and head/controllers still, do their parts
remain continuously visible in the left eye? Success supports roundoff as the
cause. If it persists, inspect wa/lens-pass by eye and depth state, then capture
matched depth/colour pass geometry before changing suppression or tolerances.
PR58 remains draft; all three stacked PRs remain unmerged.

## 2026-09-14: residual crossbow-only transparency after boat travel (VR-112)

Verified build262-g9f27c514, compiled10:55:24, DLL SHA256
b8bf05fd81be9ce1ad53caf5dd2ba5937ae2c9a93c74db6d9fe4d2d5f3360f8d.
Both logs archived at build/mono-ui-test/playtest-20260914-110406.
Headset result: weapons stable before travel; both track after arrival;
sword has no transparency in either eye. Crossbow retains smaller partial
transparency after travel. This supports the identity bypass and narrows the
remaining symptom to real lens cancellation. The residual eye distribution
was not separately specified; do not claim it changed eyes.

Measured: crossbow now has a real lens around1.0356818. Of25 sampled same-eye,
same-Present main/auxiliary pairs with active correction,6 differ in fitted
ratio, maximum2.38e-7. No sampled common-eye mismatch. Sword lens remains
inactive. This is roundoff disagreement, not evidence of two different zooms.

Candidate reuses an identical inverse lens for numerically equivalent fits
of the same component, same Present and same eye. Reuse tolerance is32 float
epsilons, matching the identity arithmetic guard. A meaningful lens change
replaces the value immediately. Different eyes, Presents and components do
not borrow it. The fixed64-slot table contains only numeric lens snapshots
and component identity tokens, never retained hand deltas or engine writes.
Normal current identity/matching gates still authorize each draw. This is
not the retired stale-draw rescue or auxiliary-pass suppression experiment.

Seven host checks cover first fit, exact same-view roundoff reuse, opposite
eye, next Present, other component, real lens change and unknown eye. Existing
lens/identity/controller/world-instance tests pass. Release, exports, lint and
golden INI pass. The visual cause remains a hypothesis until the next test.
Next launch question: after the boat arrival, with weapons drawn and head/
controllers still, does the crossbow remain fully opaque? Success supports
pass consistency; failure requires joined per-pass transform/depth evidence,
not broad suppression or relaxed matching. wa/lens-pass logs reuse decisions.
No game launch. PR58 remains draft; no merge. The prior Linear update is
still blocked by automatic approval review pending explicit permission.

## 2026-09-14: weapon surface acceptance, build264

Verified banner and installed hash; both logs archived at
build/mono-ui-test/accepted-20260914-111707. The remaining crossbow partial
transparency is headset-confirmed resolved. Build262 had already confirmed
ordinary weapon stability and an opaque sword. The accepted combination is
identity-fit bypass plus numerical-equivalent per-component/Present/eye inverse
lens reuse. Both preserve real lens correction and current instance guards.
See [final stack acceptance](STACK_ACCEPTANCE.md) for the complete evidence,
promoted defaults and remaining unrelated VR-99 note flicker. Merge authorized.

## 2026-09-14: severe physical-head-turn flicker at120Hz (VR-116)

Surface: headset world image during cinematics and gameplay, exclusively on
physical head turns. Holding still and right-stick turning do not reproduce.
This routes to image/pose attribution and cadence, not weapon transparency or
static eye-swap flicker. The VR-115 desktop branch is parked, committed at
1d6558a3; its candidate was never installed. New branch starts at VR-Main255d1c91.

Verified installed266-g5aa625ae, compiled11:25:16, DLL SHA256
257ab2551fadfec93d66a1e25067143c6643257709596f8303ecb5482015c7a4.
Both logs and INI archived at build/flicker-120/20260914-133932.
Runtime period8.33ms confirms120Hz. Pace.Lag=2 and Hands.PoseLag=2; both lag
A/Bs are off; SyncHz=0. Lag was not accidentally disabled by default promotion.
In25 steady opening windows, stereo submits/s min/median/max81/90/100;
interval SD2.18..10.32ms. Later windows drop to66..82 pairs/s. Real cadence
pressure is measured, but physical-only symptoms make pose association the
first focused test. Cadence alone is not proven to explain this severity.

Steady-opening ring accounting reconciles, with no late repair/expiry and no
empty pops in those windows. Eye summaries report healthy1/0 ages without
stale eyes or pair aborts. Sampled frame identities show distinct eyes and no
swapped-side result. These summaries are evidence against the old sustained
late-tag failure, not proof that every pixel of every frame is correct.
The old posesub trace prints109 right-eye lines and zero left-eye lines.
Its final mean input/submission orientation difference is0.120deg, moving mean
0.259deg, maximum7.934deg. A large one-off disagreement is measured; its visual
correlation is not. The old posejoin camera/render comparison uses a latest
VP observation and reports large disagreement; it cannot establish an exact
cinematic image projection and is not treated as proof of a camera-write bug.

Candidate Pace.ImageOrientation defaults0, saved by F10 View's Image-linked
head orientation checkbox. When a successful stereo texture copy carries a
valid same-eye camera record, use that record's normalized head quaternion
for the copied image's submission orientation. Missing/expired, wrong-eye,
invalid camera, missing generation or invalid quaternion retains existing lag.
The record already travels with the captured texture; no new history matching
or engine-memory write is added. Camera/hand behavior, pacing, positions and
resolution are unchanged. Positional image attribution is outside this first
rotation-only candidate. This tests camera-input association; it does not
prove that the renderer honored the input. Both eyes log decisions separately.

Eight host checks cover delayed-image pose selection, wrong/unknown eye,
missing record without output damage, invalid camera/quaternion/generation,
and sign-equivalent quaternion. Existing frame/weapon/animation tests, release,
exports,lint and golden INI pass. No game launch. Headset result pending.
Next launch: keep120Hz; in the opening, turn the physical head side to side
with the stick untouched. Does the severe flicker disappear or substantially
reduce? Improvement supports image-linked orientation; unchanged or worse
requires the new per-eye decisions plus render/pose evidence. Do not declare
90Hz a fix, or silently alter numeric lag or install the desktop experiment.

## 2026-09-14: hand/weapon follow-up candidate (VR-116)

World checkpoint remains unchanged: source 8cd27652, tag
`vr-116-world-smooth-271`, acceptance documentation commit 827b2db5.
Patch branch: `codex/vr-116-flicker-fix-patch`.

`Hands.ImageOrientation` defaults off, live in F10 View and saved independently.
With both world and hand switches on, hand draws inspect the front queued
camera record on the render/present consumer lane, without popping, repairing
or searching ahead. Unknown/wrong eye, empty/skewed queue, expired record or
invalid head basis retains legacy normalization. This is the next image's
queued record, NOT the previously delivered capture record and NOT the latest
camera publication. The association remains a candidate until headset/log
validation, particularly around late tags; eye identity alone cannot prove
absence of every possible queue fault.

A coherent snapshot now carries the head rotation it was normalized against.
The draw rebases both position and orientation by
`F * transpose(R_imageHead) * R_oldHead * F`. Controller samples, head
translation, camera movement, compositor metadata and numeric lag stay intact.
Weapons consume the same hand correction through WaCommon, including depth and
other material passes. No weapon pose-generation equality gate is restored.
No engine-memory writes or native-animation ownership changes are added.

Host tests cover noncommuting head rotation with a stationary controller,
position/orientation consistency, a fixed-history negative control and invalid
inputs. Queue tests cover nonconsumption, ordered capture, missing/wrong eyes
and excessive queue depth. Per-eye `ms/image-orientation` logs report applied
and fallback counts plus camera/hand generations. Hand result is pending.

Next launch at 120 Hz, weapons drawn: hold controllers steady and physically
turn the head side to side. Does the slight hand/weapon flicker disappear
while world smoothness remains intact? Yes supports image-specific hand
normalization; unchanged/worse requires inspecting applied/fallback counts
before changing pose timing again. Never launch the game or merge this branch.

## Installed hand candidate: 2026-09-14

Build `vr-116-world-smooth-271-3-g12a974134`, compiled 14:09:46.
DLL SHA256 `dead0ade5441462c9380f43280e7e846fa9da34f6cb4a7967194aa9384b94435`.
INI SHA256 `32c998c3485f2f2ed04056011f9f1e37e729ea4a9ca3db8272a6d3a25b9f9c50`.
Bundle `build/playtest-candidates/vr-116-image-hands`; install archive
`build/playtest-candidates/installs/20260914-141029-883111`.
Entire INI comparison adds only `Hands.ImageOrientation=1`; CRLF verified.
World image orientation remains on. Release build, frame math including six
new hand tests, all 253 queue checks, exports and lint passed. No game launched.
The accepted world run still measured 75-77 submissions/s in late 120 Hz windows.
The old cadence log's causal advice is historical, not a valid explanation of
the now-confirmed smooth world in this run. Frame rate and head stability differ.
Hand playtest remains pending; world checkpoint and its DLL/INI are preserved.

## 2026-09-14: hand candidate inconclusive; exact world baseline restored

Build `vr-116-world-smooth-271-3-g12a974134`, compiled 14:09:46,
verified DLL SHA256 dead0ade5441462c9380f43280e7e846fa9da34f6cb4a7967194aa9384b94435.
Both logs and INI archived in `build/flicker-120/hand-test-left-jump`.
Reported: possible small hand improvement, but a one-frame LEFT-eye-only
leftward hand displacement persists during physical yaw while watching hands.
Possible reduced world smoothness was reported with uncertainty. Neither hand
acceptance nor a proven world regression should be inferred from this run.

Final sampled hand counters: left accepted 13365/fallback 1963 (12.81%);
right accepted 13493/fallback 110 (0.81%). Populations are hand draw contexts,
not distinct images or visually marked flicker events. Unknown-eye decisions
are counted in the left bucket by the current diagnostic, so the percentage
is not a pure left-image failure rate. Exact fallback reasons are not printed.
World submission samples end with left accepted 15479/fallback 0 and right
15468/fallback 0. Diff against 8cd27652 confirms no change to world submission
selection, only the separate hand control API. This cannot disprove a timing
or perceptual difference from additional render-thread work.

Routing: section 1's one-eye sideways hand/weapon jump (VR-95), NOT partial
weapon transparency (VR-112). Fixed-head normalization alone is insufficient.
The existing eye classifier holds its old eye on small right-axis jumps;
this can assign a right-eye offset inside a left-eye image. It also gates the
new record lookup, so a wrong classification can deny normalization correction.
Current evidence is suggestive, not event-correlated proof. Do not re-enable
the old blind toggle predictor: its still-head regression remains unresolved.
The old eyecheck reports every comparison UNKNOWN (7089 toggled, 454 SAME,
3 ambiguous). Its text claiming zero disagreement clears the classifier is
invalid when nothing was compared. The lookup asks about a future present;
classification needs a deferred join to the resolved image, not this immediate
future-record query. Fix that instrument before accepting its conclusion.

Restore the EXACT accepted DLL and INI from
`build/playtest-candidates/vr-116-image-orientation`, build 271-g8cd27652.
Keep hand candidate code committed, unaccepted and available separately.
Next launch at 120 Hz in the same scene with physical head turns:
does the exceptional world smoothness return compared with the hand candidate?
Yes isolates the difference to the hand candidate/build path; no leaves scene,
runtime variability or baseline perception unresolved. This is a baseline
comparison, not a claimed hand fix. Afterward, instrument actual per-view eye
identity and deferred classifier agreement before changing eye offsets.
Never launch the game. No merge or external publication authorized this turn.

## 2026-09-14: performance rollout preserves the world checkpoint

VR-115 now ports PR60 onto current main, retaining accepted image-linked world
orientation. This is desktop-output scheduling work, not a new hand-flicker
fix. Later perception of smoothness was uncertain; failed hand-candidate lag
attribution remains a report, not a demonstrated cause. PR63 HUD testing was
reported favorable but is not a controlled performance comparison.
No new headset result exists for this performance build. Full/Off/Full uses
actual renewed eye serials, not every-second-Present counting. A faster result
with stale eyes or broken stereo fails. See PERFORMANCE_ROLLOUT.md for the
recoverable plan, counterprediction and first test. World pose, hand correction,
image lag and scene rendering remain unchanged.

## VR-115 first desktop comparison result

Build276-g48a632e48 completed Full/Off/Full. During limited headset observation
no new visual problem was reported; sampled image-orientation fallbacks stayed
L0/R2 and Off hold diagnostics report black0. Throughput improved but p95 and
16.667ms exceedance share worsened, so no smoothness or flicker fix is claimed.
Full metrics and preserved logs are indexed in PERFORMANCE_ROLLOUT.md.

### VR-50: Display-tab legacy FOV writer causes repeated zoom-like pulses (2026-09-15)

1. **Symptom identity:** repeated apparent one-frame zoom in/out across the headset
   world after using resolution Set. Eye-specific asymmetry was not reported. This
   routes to camera-writer interference and projection-scale mismatch, not stale-eye
   transport, weapon geometry, HUD redirect or the accepted world-orientation issue.
2. **Reproduction identity:** verified installed build357,
   `vr33-hands-working-357-g847030698-dirty`; source includes6820218cd and local handoff.
   DLL SHA256 `7f1d3efa2e5c26d5f455c778c7a5542c9c07cd70cb690d545e2560cf3bae265b`.
   Preserved post-run INI SHA256 `5c273a13e6b903b64be5d9e491a21f328326616ee4155bad4b232e1674dd1143`.
   Evidence: `build/performance-results/vr50-resize-flicker-20260915-213237`, both logs,
   INI and installed manifest. Same headset setup; no new runtime/refresh comparison.
   Started3012x3122, one Set requested3135x3250. Engine call49289921, Reset49290015,
   return49290078, capture confirmation49290109. Exactly one Reset; later swapchain
   dimensions stay3135x3250. The user adjusted FOV102/103/104/105, ending at104 live.
3. **Hypothesis/counterprediction:** repeated resolution switching is contradicted by
   one Reset and stable dimensions. There are260 post-resize FOV audit changes, split
   between104.00 and108.05 degrees.212 FOV-scope releases occur in the run, many in
   ordinary walking with menu=0. At49301343 trace says animValid=1, projection=1,
   runtimeQuad=0, followed by a release. Initial150ms-expiry suspicion is superseded:
   explicit releases and source evidence identify a different writer, not expiry alone.
4. **Change identity:** the legacy FOV checkbox/slider in overlay.cpp lacked braces;
   `camera::set_fov_deg(g_fovLever)` therefore ran on idle Display frames, normally
   writing0. DvrFovHandoff restored the automatic108-degree target on Presents. A
   game-thread draw between those writes failed the target>=40 eligibility check,
   restored/released the scoped104 FOV and published the wide sensor claim. This bug
   predates the live Set button; using that Display tab exposes it. Build359 extracts
   the same control into `core/ui/legacy_fov_control.inc` and writes only on real edits.
   Slider initialization also uses the newly enabled value. Release logs now name
   scene/projection/state-valid/target/requested so a recurrence is distinguishable.
   No timeout, eye-tag, camera-scope policy or image-owned orientation change.
5. **Results:** production-control host regression passes7 checks including20,000 idle
   frames. The same harness against the previous production control fails7/7, including
   both idle enabled/disabled controls. Release build, exports, generated/package INIs,
   lint and diff checks pass. No game or simulator launched. Installed359,
   `vr33-hands-working-359-gb5e0af9dc-dirty`, requested defaults103 FOV/130% total pixels
   3135x3250/mirror-off. Both previous logs/files archived at
   `build/playtest-candidates/installs/20260915-213922-577631`. Full installed INI diff
   only ProjectionFov102->103; live Set had already saved3135x3250. CRLF verified.
6. **Status/remaining scope:** code-confirmed cause and fix, visible acceptance OPEN.
   One launch question: does the view stay stable with F10 Display open and after
   Set120% then130% in the same run? Expect one Reset per changed size, steady103 FOV
   during ordinary walking, and no repeated expansion/contraction. Continued flicker
   falsifies completeness; inspect new gate-reason logs and actual claims before a
   timeout or image-metadata change. Close Display as a discriminator if it recurs.
   Preserve the accepted image-owned orientation/stereo synchronization. Performance
   and sizing context: [PERFORMANCE.md](PERFORMANCE.md), active F11 section.

### Pair-pacing diagnostic with mirror off (2026-09-15)

1. **Symptom:** uneven fresh-pair delivery in earlier mirror-off tests, not a new
   eye swap or FOV-pulsing report. Cadence/scheduling is the boundary under test.
2. **Identity:** installed361, 103 FOV/3135x3250, mirror-off, DesktopAb=3. Detailed
   identity, archive and measurements: PERFORMANCE.md, Pair-pacing test prepared.
3. **Prediction:** modest pair-opening pacing reduces tails beyond both unpaced
   baselines; lower rate alone is not success. Zero gate delays is inconclusive.
4. **Change:** automatic unpaced/paced/unpaced driver around the existing gate,
   with adaptive target and restoration. No image-owned orientation, stereo tag,
   resource fence, camera writer or pacing algorithm changes.
5. **Result:**26 production-control host checks and build pass; installed and armed.
   No game/simulator launch or rendered/perceptual result. This is not a flicker fix.
6. **Open:** headset cadence/comfort and measured tails. Any stereo regression
   rejects the trial. Preserve prior FOV-fix acceptance as a separate open question.

### Strict mirror suppression across capture gaps (2026-09-15)

1. **Symptom/surface:** occasional desktop refresh while mirror is disabled; user
   separately reports pacing feels laggier. No new headset eye-instability claim.
2. **Identity:** verified362 pacing log archived in
   build/performance-results/pair-pacing-result-20260915-231337. Actual native desktop
   calls persist at capture gaps. Exact distributions are in PERFORMANCE.md.
3. **Prediction:** session-scoped suppression removes desktop refresh without needing
   fresh capture/callback. Any new headset instability rejects the candidate.
4. **Change:**363 opt-in DesktopMirrorStrictOff skips native Present while XR is
   begun, retaining current-work flush and all ownership/pose/eye rules. Stopped XR,
   unsupported parameters and explicit submit failures retain desktop fallback.
5. **Results:**240 real native GPU marker/pixel checks pass, including absent capture/
   callback; old guard negative control presents. Error/stop/reset paths pass.
   Installed with pacing off,103 FOV,120% pixels. No game/simulator launch.
6. **Open:** headset acceptance and actual=0 in healthy running-XR windows. Full
   evidence, build hashes via manifest, archived INI and one launch question are
   maintained in PERFORMANCE.md, Pacing result and strict mirror-off trial.

### Strict mirror-off accepted and defaults promoted (2026-09-15)

The363 headset result accepts complete desktop suppression with normal VR behavior;
621 logged strict windows have actual=0. The full saved profile is promoted at user
request, with strict suppression enabled and pacing off. Performance improvement
remains subjective. Exact acceptance identity/archive and promotion scope are in
PERFORMANCE.md, Accepted profile and publication. Earlier strict-default0 and pending
headset entries are historical. Accepted image-owned orientation remains unchanged.
