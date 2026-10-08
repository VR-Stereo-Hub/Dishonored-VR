## Still cutscene arms pointing behind the head: hidden behind a lever (2026-10-05, built, not headset-run)

Some cutscenes show the game's full arms in a fixed pose that points back past the head (the
opening cutscene); in a headset that pose is visible whenever the player looks around.
`[Anim] CineHideStaticArms` (default 0, live `cinehidearms on|off`, F10 Advanced > Hands) hides
them while BOTH hold: the arms are still (the between-bone speed of `arm_motion`, which already
grants the mod its own rigid write, under 8 uu/s for 500 ms) and both hand bones are behind the
camera plane (each hand bone's palette point through the draw's own LocalToWorld, which is rebased
on the view, against the camera forward from the same draw's ViewProjection). Only the qualified
arm mesh draw is skipped (`MsQualify`), and only while a cutscene runs (the FSM's cinematic states
or `bCinematicMode`). Independent of CinematicArms.

First run (2026-10-05): it never armed. The opening scene runs in `StatePlayerMasterSoiree` with the game
owning the arms and `bCinematicMode` off, which the cutscene test did not count, and under full-arm IK the
check sat on the non-IK path. It now arms whenever the game owns the arms, IK or not; still + both hands
behind the camera remains the whole test.

Second look (2026-10-06, from the logs, no new run): it could never have armed. While the game owns BOTH
arms (`native_full_arms`) the draw hook (`DcDrawIndexed`) hands every draw straight to the game at its first
line, ahead of the mesh lock and `MsDraw`; the hide sat in `MsDraw` and required `native_full_arms()`, which
that exit had already made false there. The boat logs show the consequence: `StatePlayerMasterSoiree`, the
game owning the arms (body=1), no `ik:` or `cine/hidearms` line, and 2.5 s in `dc/auto: the locked mesh has not
been drawn for 301 frames` - the lock's bookkeeping also sits after that exit, so the arm mesh was drawn and the
lock released anyway (an instrument whose population excluded the case). The hide now runs AT that exit
(`MsCineHideNative`, for draws of the locked arm buffers only) and keeps the lock alive there while the option
is on. Reported in the same session: the still arms were more visible than before; the run had the option off
(the shipped default) where the earlier ini had it on, though with the dead path that changed nothing drawn.

Boat run on that build (v1.0.3-131-ge1698b20b, 2026-10-06, option on): the hide now reached the arm mesh
(`arm-mesh draw seen at the native exit ... split's arm mesh 1`), read the arms still (`arm speed 0.0`), and
drew them anyway: every line read `hand depth ... NOT measurable this draw`, so the behind-the-camera half
never passed. The arm speed reads the same palette and hand bones, so those were present; the draw context
was not - the shader register layout it reads ViewProjection and LocalToWorld through is refreshed only in
`MsDraw` and the IK draw, both skipped at this exit. The exit now refreshes it, and the line names the failed
input (no palette, hand bones unknown, or the context's own refusal) instead of only "NOT measurable".

Boat run on v1.0.3-131-g9a1690245 (2026-10-06/07, option on): MEASURED, and the view test is the wrong test.
Facing forward both hands read behind the camera (L -36 R -30 uu), the arms were still (0.0 uu/s) and the hide
fired (`cutscene arms HIDDEN`), so the depth sign is right. Turning the head, one hand read in front (L +65 R -31)
and the arms were drawn again: they are fixed to the scene's authored camera while the view turns with the
headset, so a view-relative test releases them exactly when the player looks round at them. Reported: still
visible on the boat. The pose itself never changes, so the hide now FINGERPRINTS it: each hand bone's position
in the arm mesh's own space (the palette alone, no view). A still pose that passes the view test once is
captured into `[Anim] CineHidePoses` (up to four, persisted) and hidden from then on whenever it recurs and
holds still, whichever way the player looks; `cinehidearms forget` clears them. Tolerance 2 uu per hand.
`cine/hidearms:` logs the mesh-space hand positions, the nearest captured pose and which rule hid the arms;
`pose CAPTURED` marks a capture with its numbers, so a later build can ship the boat's pose baked in.

Boat run on v1.0.3-131-g06d345d75 (2026-10-07): HEADSET-CONFIRMED, the arms stay hidden whichever way the
player looks. The run also showed the pose is not perfectly still: it drifts about 6 uu over the ride
(hands L x 53.4 -> 55.5, z -25.0 -> -19.4 across 80 s), so at 2 uu tolerance it was captured four times (1 to
2 uu apart) and filled every slot. Tolerance is now 10 uu per hand; `CineHideStaticArms` ships ON with the
middle capture baked in as `CineHidePoses` (54.23 -104.30 -23.16 -54.24 -104.30 -23.14), and `[Meta]
DefaultsRev=3` turns it on for existing inis, setting that pose where the ini has captured none of its own.

## The game's arm re-seat targets the nominal shoulder, not the reach-shifted one (2026-10-07)

Reported (sewer weapons pickup, a short scripted scene in `StatePlayerMasterSoiree` with the game owning both
arms): the arms sat too far forward, and moving the F10 IK shoulder made no difference. The log explains the
"no difference": `ik: ACTIVE ... shoulder reach shift uu L=37.438 R=15.419` at the frame the game took the
arms, and the re-seat (`ik/gamearm`) landed the game's shoulder on `solved[h].joints.shoulder` - the IK
shoulder AFTER its reach shift. The reach shift slides a shoulder toward a wrist the arm cannot reach; in a
game animation the wrist is the game's, authored far forward for a flat screen, so the IK shoulder slid 15 to
37 uu forward and the re-seat put the game's shoulder there. The slider moves the nominal shoulder, which the
target did not use. In the takedowns of 2026-10-06 the reach shift read 0.0 at full game share, which is why
they looked right with the same code. The target is now the nominal shoulder (the F10 fit); the `ik/gamearm`
line reports the gap against it and the reach shift it excluded. Not yet run: the gap against the nominal
shoulder is larger than the logged one by the reach shift, so the stretch wanted may exceed 1.9 in that scene
(the line's `wanted` and `left over` say so; the F10 "Game arm stretch limit" is the lever).

What the first run must check, from `cine/hidearms:` (every 2 s in a cutscene): the hand depths
read POSITIVE while the arms are in front of the view and negative when they point back; if the
sign is inverted the lever hides the wrong poses and must stay off. `cine/hidearms: cutscene arms
HIDDEN` / `drawn again` mark each change. Not measured: the opening cutscene's actual depths.

## Fourth headset run: takedowns accepted, cutscene arms shelved behind the toggle (2026-10-04)

Build v1.0.3-38-ga206226b7 (banner and both config lines confirmed in the log), ini = run 3 plus
`ArmIKGameArmShoulder=1`.
- **Takedowns: reported correct.** Two front fatalities, two chokes (one into a carry). The
  re-seat ran in the fatalities and stood down in the chokes (`choke=1 ... not applied`), as
  designed. `ik/gamearm:` (54 lines): the IK shoulder and the game's shoulder were 8 to 27 uu
  apart in most samples (up to 47 on one arm mid-clip); the steadiest part is height, the IK
  shoulder 5 to 14 uu above the game's, while the forward part swings from -18 to +25 through a
  clip. So the game's shoulder is mostly LOWER, and in front or behind by turns. HEADSET-CONFIRMED:
  SmoothBlend, ArmIKGameArmInAnim, ArmIKGameArmShoulder=1.
- **Cutscene arms: not accepted; `[Anim] CinematicArms` stays an opt-in.** It ships 0, sits in
  F10 Advanced > Hands > Game arms during actions as "Your arms in cutscenes (experimental)",
  and with it off nothing of the feature runs: no unhide, no gate, no name walk, and the arm
  motion sampler does not measure. Cutscenes and conversations are then exactly the game's
  (`CinematicHandBack` and the per-state rules, as before this branch).
- **What the log says about the new gate (MEASURED, cause OPEN).** 6 openings instead of 15,
  at 30 to 183 uu/s between bones with the arms 7 to 20 uu from the reference pose, so the
  openings were not the reference pose. All 6 lasted 5.4 s: every one closed on the 5 s hold
  again. At each close the line reads motion 0.0 and reference-pose distance -1 (not
  measurable): while the game owned the arms the new instrument measured nothing. Either the
  sampler is not reached or the palette cannot be inverted in that state; not established.
  Equal-length episodes are a timer (TRAPS), so the gate still cannot tell a held pose from an
  ended clip. The reference-pose veto fired on 937 samples, so some stance in the run IS within
  1 uu of the reference pose. Player-owned seconds still show between-bones motion up to 760
  uu/s: either real game motion under the player's hands or the single-rigid-write assumption
  does not hold; not separated.
- **If this is picked up again:** first make the instrument report while the game owns the
  arms (log why `arm_motion` returned no bones there), then decide from a per-joint log whether
  the player's write is one rigid move.

## Third headset run: the gate was watching the player (2026-10-04)

Build v1.0.3-37-g01b763971, SmoothBlend=1, CinematicArms=1, ArmIK=1, ArmIKGameArmInAnim=1.
Log: 323 `cine/motion:` lines, 15 gate openings, 3 takedowns. Reported: cutscenes close to
right and the choke right; in conversations the arms were taken by the game into still default
stances (one with the arms pointing backwards) and the player's arm position reset; the arms
came back to the player while a scene still held them; in every takedown but the choke the
game's arm showed its shoulder too far forward.

**1. The motion gate measured the player's own hand. MEASURED, from the log.**
- `swing: beat peak10s` (the controller's peak speed per 10 s) against `cine/motion` in the
  same windows, player-owned: controller 0.01 m/s -> arm speed max 0.6..1.6 uu/s; 0.07 m/s ->
  7..17; 0.14 m/s -> 1..7; 0.48..0.71 m/s -> 17..340; 1.2..1.9 m/s -> 50..87. The instrument
  follows the controller. Against the cinematic camera's rotation rate the same seconds
  correlate at 0.10 (authored) and 0.25 (composed with the head), so it is not the camera.
- In every whole second the game owned the arms in a conversation, the speed is exactly 0.0 in
  16 of 21; the other 5 are real game motion (max 24..213 uu/s). The game's arms in a
  conversation are a still stance.
- All 15 openings lasted 1.78 to 2.19 s: the opening delay, a hold of 1500 ms that started at
  once because the measured speed fell to 0 the moment the game took the arms, the release.
  Not one opening was kept open by motion.
- Each change of owner is one frame of 600..2700 uu/s (the mod's write starting or stopping).
So: the mod's hand control moves palette bones behind the wrist as well as the hand (the first
design assumed it wrote hand bones only), any hand movement above 0.2 m/s for 120 ms opened the
gate, the game showed its still stance, and the hold gave the arms back 1.5 s later. Both
complaints (the reset in conversations, the short hold) are this one fault. Hypotheses checked:
"a snap is one huge spike" is true of the owner changes but they did not open the gate (it
already needed 120 ms); what opened it was sustained player motion. "The stance is the
reference pose" could NOT be measured from this log: it carries no palette. The new build logs it.

**The new instrument** (`arm_motion`, anim_policy.h; `MsSampleArmSpeed`). What the mod writes is
one rigid move of one bone and everything below it, per arm, whichever bone the control sits on
(not established which; the design does not need it). That leaves an arm's bones in at most two
groups that are rigid inside themselves. An animation bends more than one joint. The speed is
therefore measured between the bones of one arm (bone b's points in bone a's frame, which no
common rigid move changes), and the answer is the largest relative speed left once the single
fastest split is granted to the mod: the second-largest edge of the minimum spanning tree over
the pairwise speeds. Limit, by construction: a clip that moves exactly one joint reads as
still. Host checks: the mod's translation, its rotation and a whole-arm move read 0 while the
old instrument reads over 50; two joints read as motion, also under the mod's write on top.
- An opening also needs `CinematicMotionSamples` (3) DIFFERENT measurements above the
  threshold: a pose snap is one measurement, however long a stalled sampler keeps showing it.
- `CinematicRefPoseUu` (1.0): arms within that distance of the reference pose (measured the
  same way, the mod's write excluded) are never handed over, and an open gate closes at once.
  The 1.0 is a guess, not a measurement: `cine/motion` now logs the distance's range a second.
- `CinematicMotionHoldMs` 1500 -> 5000. Run 3 cannot supply this number: no opening was ever
  kept open by game motion, so no frozen-pose duration was recorded. 5 s covers the 2.5 and 4.5 s
  the BioShock Remastered mod measured. The start/stop speeds (20/8) are unchanged and are
  NOT yet measured in the new instrument's units; the only game-motion numbers run 3 has are the
  old instrument's 24..213 uu/s.
- F10 Hands > Your arms in cutscenes: hold, both speeds, duration, frame count, default-stance
  distance, live while dragged and saved on release, with a live readout (motion, distance,
  who has the arms). Seam: `anim cinegate [<start> <stop> <startMs> <holdMs> [samples] [refPoseUu]]`.
- Log: `cine/gate:` on every opening and closing with the numbers that caused it; `cine/motion`
  now carries the new motion, the old fastest point beside it, the reference-pose distance,
  the veto count and the openings.

**2. The game's arm and the IK shoulder (`[Hands] ArmIKGameArmShoulder`, default 0).**
Why, from the code: at full game ownership the arm slots are the game's palette exactly, and
the game poses that arm for its own camera and body. The IK shoulder is the tracked head plus
the F10 shoulder offsets in an upright body frame. Nothing ties the two, so the shoulder jumps
by their difference as the hand-back blends in. Run 3's log does not carry that difference
(only the IK's own reach shift: 17.8 / 20.1 uu in the choke, 0 in the drop assassination, where
the head was pitched 45 degrees down and the neck model had moved the eye 22 uu forward).
The fix (`shoulder_fit`, arm_rig.h): one transform for the game's arm, about the game's own
wrist: a stretch along the shoulder-wrist line (so the cross-section at the wrist still meets
the hand), then the smallest rotation that puts the shoulder on the IK shoulder. The hand stays
exactly where the clip put it; the elbow's bend and side stay the game's. Bounded (stretch
0.80..1.25, 45 degrees); what the bounds leave is logged. It blends in with the hand-back
weight like the arm itself, and the IK arm it blends from already has that shoulder.
1 = every game animation except the choke, which was judged right as it was and is kept
byte-for-byte (latched from the choke state until its hand-back has returned); 2 = the choke
too. `ik/gamearm:` logs, twice a second while the game has an arm, the IK shoulder minus the
game's shoulder in body axes (forward/right/up, uu) and the turn, stretch and left-over of the
re-seat, with the lever on or off. Not chosen: moving the whole arm rigidly with its hand,
which takes the hand off the clip's contact point by the same distance.

**Next run, one question each.**
1. A conversation, hands moving freely: do the arms stay with the player? `cine/gate:` should
   not appear; `cine/motion` should read motion near 0 with the fastest point high.
2. A scripted arm clip (the walk-in, the boat): does the game take the arms and keep them while
   it holds a pose? `cine/gate: OPEN` then no CLOSED until 5 s after the last motion. If the
   clip is NOT taken, read the `between bones` max in those seconds against the 20 uu/s start.
3. A default stance shown after a clip: read `reference-pose distance` there. Under 1 and
   vetoed = the stance is the reference pose; well above = it is an authored pose and the veto
   cannot see it.
4. A stab takedown and the choke: is the shoulder where the IK shoulder is, and is the choke
   unchanged? `ik/gamearm:` gives the offset the re-seat removed.

## Second headset run: choke arm, boat ride, scripted arm clips (2026-10-04)

Build v1.0.3-36-g65b278d04, two runs: the intro (boat ride, Dunwall Tower) in
dishonored_vr.prev.log, takedowns in dishonored_vr.log.
- **Choke: right hand still turned.** Open hand never ran this time (0 `hands/openright`
  lines), so that was not the whole cause. `ik: ACTIVE ... hand blend 0.000/0.000` during the
  choke: the wrist is exactly the game's choke pose, but the arm under it is IK-solved from the
  tracked shoulder, while the game's arm reaches around the neck from its own shoulder and
  elbow, so the game's wrist sits twisted on the IK forearm. New lever `[Hands]
  ArmIKGameArmInAnim` (F10 IK, "Game's own arm during game animations"): the arm slots blend
  to the game's own arm by the hand-back weight (a skin-matrix lerp; the IK slots carry the
  length scale, which the proper-rotation blend refuses). At full game ownership the arm is the
  game's exactly; it eases back to IK with the hand.
- **Boat ride: no arms.** It is `StatePlayerMasterSoiree`, cinematicMode=1, body mode 1, pawn
  hidden by the game. The first build only unhid arms-only (body 0) and re-hid it here. The game
  shows the full-body pawn itself in conversations (pawn visible, arms drawn), so body 1 is now
  allowed; HIDDEN (2) never. Not yet known: whether a cutscene camera owns the view there (the
  first-person arms draw only for their owner's view); `cine/motion` now logs the view target.
- **Scripted arm clips (picking Emily up): no hand-back.** No arm action, no matinee-node
  change, no distinct state, and it ran with cinematicMode=1 in an ordinary state. The BioShock
  Remastered mod met the same absence of a flag (its M7-S4) and used MOTION: the model-space
  movement of a bone the mod does not write, with a hold for poses a clip freezes mid-scene. Here
  the mod writes only hand bones (single-bone SkelControls), so the game's upper-arm and forearm
  bones are measured from the native palette (`MsSampleArmSpeed`, centroid plus two 10 uu
  levers so rotation counts). A gate (`MotionGate`, host-tested) opens above
  `CinematicMotionStart` (20 uu/s) for `CinematicMotionStartMs` (120) and closes below
  `CinematicMotionStop` (8) after `CinematicMotionHoldMs` (1500). A cutscene now also includes
  bCinematicMode in an ordinary state. All four are first guesses: `cine/motion:` logs the speed
  max/mean per second, the gate, the input locks and the view target, so a run sets them.
  The BioShock Infinite mod (UE3) hands the arms to the game for the whole of any cutscene camera
  or input lock; that gives no player control in cutscenes, so here both are logged, not used.

## First headset run of SmoothBlend / CinematicArms (2026-10-04)

Build banner v1.0.3-33-gdb8d3ced4-dirty (built before the commit; code = df42ff55d), IK on,
SmoothBlend=1, CinematicArms=1. Two runs (dishonored_vr.prev.log, dishonored_vr.log).
- **SmoothBlend: transitions reported as right**, entry and return, takedowns and trigger
  swings.
- **Choke: the right hand looked turned about 180 degrees at its target and the IK arm
  twisted.** The log shows the sword unequipped at the choke (`rfl/state: equipment CHANGED
  ... -> none`) and `hands/openright: right hand OPEN ... 15 finger bone(s) posed from the
  left` during it: the empty-right-hand mirroring ran on a hand the game owned, replacing
  the choke grip with the left hand's mirrored pose, and the IK arm follows that wrist.
  Fixed: `OhActive()` stands down while `hand_owned(1)` (through the return too). Open hand
  is for the player's own empty hand, never a game animation's.
- **Cutscenes: no control.** In every conversation the matinee pose blend reads
  `m_bEnabled=1` from entry to exit (`cine/arms: master=StatePlayerMasterInDialog ...
  matineeBlend=1`), so the game kept the hands the whole scene (`reason=cinematic: the game
  animates the arms`). The arms WERE drawn (`arm mesh last drawn 16 ms ago`, pawn not
  hidden, body mode 1). Fixed: the matinee flag no longer triggers a hand-back; only an
  upper/left arm action does. `cine/matinee:` now logs ActiveChildIndex, BlendTimeToGo and
  m_bDoBlend on change inside cinematics, next to the actions and the sequence, to find what
  marks an authored arm clip (a matinee-driven gesture would currently stay with the player).
  The mod's own SkelControl writes the game's hand bones while the player owns them, so
  "the native pose moves" cannot be the signal: it would detect the player.
- **Hide-player cinematic (level start): unhidden, NOT honoured.** bHidden went 1 -> 0
  through the setter, but the arm mesh did not draw in the second after. Likely cause, not
  measured: the first-person mesh is `bOnlyOwnerSee`, so it does not render while the view
  target is a cinematic camera rather than the pawn. The pawn then became unreadable
  (level transition). No visible fault reported from it.

## Smooth hand-backs, IK arms and cutscene arms (2026-10-04, built, not headset-run)

Branch `claude/anim-blend-ik`. Two levers, both default OFF with a live F10 toggle
(Advanced > Hands > Game arms during actions) and a seam word.

**Why the hand-back snapped (read from the code, not a run).**
1. *Snap out.* When the release hysteresis ended, `tick()` dropped `handMask` to 0 on the
   same tick the return blend began, and `weight_for()` answers 1 for an unmasked hand. The
   150 ms return was computed and never shown: the hand jumped to the controller.
   `ownedMask` dropped at the same moment, so the base pose under the hand changed too.
2. *Corners.* The blend was a linear 150 ms ramp both ways: full speed from the first frame
   and a dead stop at the last.
3. *Arc.* The correction's translation was interpolated about the mesh origin, so a palm far
   from it swings through an arc and overshoots. The retired HandOrigin branch measured it
   (run155: median 8.9 uu, max 98 uu off the straight path).

**`[Anim] SmoothBlend=1`** (seam `anim smooth on|off`, `anim blendms <entry> <return>`):
smootherstep easing (zero speed and acceleration at both ends), separate
`HandBackBlendInMs` (250) and `HandBackBlendOutMs` (350), a reversal mid-blend covers only
the remaining distance; the palm moves on the straight line between its native and its
controller position (`blend_transform_palm`); the hands and `ownedMask` stay with the game
until the return reaches the controller (`render_hand_mask`). From the retired branch only
the two measured defect fixes were taken (mask hold, palm path). Its HandOrigin entry
translation, never accepted in a headset, was not. Off = the original code path exactly.
The IK arm needs nothing extra: its endpoint is the wrist under the final, blended hand
correction, so a smoothed hand gives a smoothed arm.

**IK and the arm-hiding rules.** With full-arm IK active the draw already replaces the arm
mesh with the whole IK-solved arm before any split or hide is consulted (`MsDraw`), so
HideTakedownArms and the split-hand geometry choices have no visible effect. F10 now says so
and greys the takedown option while IK is on; the rules still apply if IK falls back to hands.
Per-state "Show game arms" still decides WHO poses the hand (the game or the controller), so
it stays live under IK.

**`[Anim] CinematicArms=1`** (seam `anim cinearms on|off`): in a cinematic master state the
state no longer hands the arms back by itself. The game takes them, through the same blend,
only while it animates them: an upper or left-arm action (the conversation's unequip, an item
use) or `m_pMatineeBlender.m_bEnabled` (a matinee posing the pawn). The lane-0 rule and
`Arms.0.<cinematic state>` are bypassed while it is on; CinematicHandBack is greyed.
Visibility: a hide-player cinematic hides the whole pawn (ENGINE_NOTES, "a hide-player
cinematic hides the whole pawn"), so with the lever on and the body in arms-only mode the
pawn is unhidden through the game's own setter (`kActorSetHidden`), on the script lane, after
reflection resolves `Actor.bHidden` to exactly the statically read +0x120/0x2 and the setter's
prologue matches. It is re-hidden if the lever goes off or the body leaves arms-only while the
cinematic runs; the game unhides it itself when the cinematic ends.
With the lever on, `cine/arms:` logs every cinematic transition (bCinematicMode, pawn bHidden,
body mode, matinee blend, how long since the arm mesh last drew; off, nothing runs at all) and reports one
second after an unhide whether the arm mesh actually drew (HONOURED / NOT honoured).

**Not established.** Whether conversations (`InDialog`) hide the pawn or only lower the arms
out of view: the dev-PC log shows no arm draw in dialogue, nothing more. If a conversation
does not set bHidden, CinematicArms gives the hands to the player but there may be nothing
drawn; the first `cine/arms:` line answers it. Body mode FULL_BODY (seen in dialogue) is
never unhidden: that would show the third-person body at the camera.

Host: 138 animation checks (24 new: easing ends and monotonicity, entry/return durations,
reversal share, shape survives a reset, palm on the straight line within 0.001 uu with a
negative control in which the old blend leaves it by more than 5 uu, mask held through the
return). Default writer, packaged profile and golden ini byte-identical; 11 exports; lint.
Release builds. No game launched.

**Next launch, one question each:**
1. SmoothBlend on (IK on): does a trigger sword attack and a takedown now ease in and come
   back to the controller without a jump? A jump at the END is the mask/owner path
   (`anim: ... reason=returning to tracked hands` should appear between release and
   controller); a jump at the START is the entry, judged with the In slider.
2. CinematicArms on, in the first conversation or cutscene: read `cine/arms:`. pawnHidden=1
   then an UNHIDDEN line and HONOURED = the hide was the cause and the arms are back;
   pawnHidden=0 with no recent arm draw = the arms are lowered or culled, a different cause.

## The player's sequence vocabulary, by name (2026-10-04)

Source: `tools\model-export.ps1` (UModel) over the cooked packages, plus the `anim: gen=...
master=... seq=...` lines already in the ten rotated logs on the dev PC. Sequence NAMES only:
the animation data cannot be decoded (Sony Edge Animation, see MODEL_WORKFLOW.md), and the
full name list stays local (game-derived). Rules still key on the master state; these are
leads for refining them by `m_AnimSeqHistory`, which `anim_state.cpp` already reads.

**What exists.** All 34 player AnimSets are in `Startup.upk` (always loaded), 422 sequences.
Largest: `Ply_Head_Locomotion_as` 43, `Ply_Guns_as` 36, `Ply_Sword_Assassination_as` 31,
`Ply_Generic_as` 27, `Ply_Empty_Locomotion_as` 27, `Ply_Powers_as` 22. Families that matter
here:

| family | names | why it matters |
|---|---|---|
| takedowns | `Sword_Ready_Assassination_{Front,Back,Left,Right,Fast*,Drop*}[_CarryCorpse]_Master`, `Sword_Ready_Fatality_*_Master` | one `_Master`-suffixed family; a prefix rule covers every variant |
| boss kills | `Sword_DramaticDeath_Front_{Campbell,Daud,Havelock,LadyBoyle,LordRegent,Martin,PendletonA,PendletonB}_Master`, `..._Back_A_Master` | per-target scripted kills; whether each runs under `StatePlayerMasterAssassinate` is NOT measured |
| chokes | `Sword_Choke_{In,Loop,Win,Lose,Cancel,CarryCorpse}_Master` | matches `DisItemContext_Choke.m_State` one to one |
| mantles | `{Empty,Sword_Ready,Sword_Sneak,Empty_Sneak}_Mantle{Low,Medium,High}`, `Empty_CrouchMantle*`, `Empty_MantleImpact` | in the locomotion sets |
| window vault | `Empty_VaultOverWindow` | in `Ply_Empty_FullBody`, NOT with the mantles (open question 1) |
| forced grabs | `Sword_Weeper_ArmGrab_{In,Loop,Out}`, `Empty_ArmGrab_{Loop,Out}` (`Ply_WeeperAttack_as`) | the game owns the arms; no current rule names them (open question 2) |
| powers | `Powers_Cast_Blink_{In,Loop,Out}`, `Generic_Powers_Cast_Blink_Travel`, `Powers_Cast_Possession_{In,Loop,Out,Cancel}`, `Powers_Cast_{BendTime,Swarm,Windblast}` | cast phases are visible by name |
| carry body | `Empty_CarryCorpse_{In,Idle,Walk,Drop*}_Master` | pairs with `StatePlayerCarryCorpseIdle` |

UModel also reports Arkane-specific `m_NotifiesAtAnimStart` / `m_NotifiesAtAnimEnd` arrays on
the sequences: possible start/end events, not yet looked at.

**Measured from existing logs (no new run):**
- Blink casts play while master is `StatePlayerMasterWalk` (or `Falling`): `seq=Powers_Cast_
  Blink_In/Loop/Out` 45/45/101 times under Walk. A master-state rule cannot see a cast; the
  sequence name can. Possession In/Loop/Cancel also run under Walk; only `Possession_Out`
  appears under `PrePossess`/`Possess` (body=2).
- `StatePlayerMasterClimb` logged `seq=unavailable` all 5 times: climbing records no sequence
  in the history, so climb rules must stay state-only.
- **The history lags the state.** Under `StatePlayerMasterAssassinate` the logged seq was a
  takedown sequence most times, but also `Sword_Sneak_JumpLandSmall` (2) and
  `Powers_Ready_Equip` (1): the newest entry at the state change is still the previous
  animation. A rule may refine by name only after the new sequence lands, never at entry.
- Chokes and mantles logged their own family names under their own states.

**Open questions (each answered by grepping the next log, no extra instrument):**
1. Does `Empty_VaultOverWindow` run under `StatePlayerMasterMantle` or under
   `StatePlayerMasterAction` ("Full-body action")? If the latter, `MantleHandBack` and the
   mantle pose rule do not cover window vaults. Not in any of the ten logs on disk yet.
   Grep: `seq=Empty_VaultOverWindow`.
2. Which master state runs the Weeper arm grab? Grep: `seq=.*ArmGrab`.
3. Do all eight boss `DramaticDeath` kills run under `StatePlayerMasterAssassinate`
   (VR-283 hides arms there)? Grep: `seq=Sword_DramaticDeath`.

## The sword hand-back is for trigger attacks (VR-220, 2026-09-25)

`HandAnimMelee` matched every `StatePlayerMeleeAttack`; it now matches an attack whose source is
the trigger, read from the motion sword's fire record (`swing.h`: `last_fire_tick`,
`last_pulse_close_tick`, `fires`, `last_fire_real_trigger`). A physical swing keeps the tracked
hand; `HandAnimMeleeSwing=1` restores the old behaviour. The verdict is latched per attack and
per combo clip and logged as `anim/melee: attack source=...`. The melee body gate reads
`cameraAction` instead of `game` so a trigger hand-back does not close the swing gate. Default
`HandAnimMelee` is 1 from this change, moved by a one-time `HandAnimMeleeRev` migration.
Simulator: `tools\xrsim\swing-anim.xrs`. Headset: owed. Detail: PHYSICAL_SWING.md, "The
animation belongs to the trigger, not the swing".

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

## VR-134: arm visibility must not select pose ownership (2026-09-17)

Build433 mantle report confirmed in verified logs: Arms.0.StatePlayerMasterMantle=0
produces PLAYER while the master FSM remains Mantle and reports a mantle sequence.
Arms=1 produces GAME. Thus the visible animation was overridden by tracked hands;
this is not evidence that the native mantle action was cancelled. Both logs/latestINI
are preserved in animation-action-controls/reported433. Native block requests were
also logged as rejected, but no general cancellation acceptance is inferred.

Correction: native action pose ownership derives from voluntary action states and
existing scripted-action defaults independently of Arms.*. Visibility selects full
arms versus the existing clipped/rounded hands under the native animated palette.
Weapon native ownership remains intact. The split bypasses controller palette and
depth overrides for native hands. Normal unchecked walking remains controller-driven.
Whole-body action visibility takes priority over upper/left states; idle/walking
do not override a real upper-body action. Visibility is frozen with pose weight for
a render frame, preserving the stereo pair. No engine state or camera change.

103 catalog/policy checks plus22 handoff checks pass. New cases cover hidden mantle
retaining its pose, unchanged checked mantle, tracked walking and split/full geometry
routing. Release builds. Headset result pending; existing split qualification still
fails open if geometry is unavailable. Earlier433 documentation calling Arms.*
independent was incomplete: it was independent of action rejection, not native pose.

Next launch: with Enable action on and Show game arms off for Mantling, do the hands
and weapon still animate through the climb while forearms remain hidden? Normal
animation supports separation; tracked/static hands mean another pose override;
visible forearms mean the split route failed. Enable action stays on for this test.

## VR-134 independent action requests and arms (2026-09-17)

F10 Animations now has separate Enable action and Show game arms controls.
Action.<lane>.<state>=0 rejects the next native RequestState before it modifies
pending state or calls entry handlers. An existing action may finish. All actions
default enabled; Arms.* overrides keep their previous meaning and values.
18 voluntary state entries expose cancellation; the other22 automatic/recovery/
story states expose arms only. This is FSM state control, not per-clip playback.
Generic full-body/item states group more than one action. Native callers which
perform work before requesting a state remain a headset-validation limitation.

The hook verifies its exact entry bytes and fails open for unknown/stale player
identity, new level objects awaiting a live-table refresh, unmatched state names,
or a missing hook. It rechecks the current pawn/FSM chain and current GObjects
membership for a disabled request, retains no engine object identity across a
menu, and writes no engine state fields. See ENGINE_NOTES for the native ABI.

ViewRightCm (default0, range-20..20cm) provides manual native-animation viewpoint
alignment. Positive moves the view right toward arms reported to the right.
Only existing validated native camera scopes with native handback active use it;
menus do not. The scope freezes one value for both eyes and restores native fields.
It does not measure a root cause or automatically move the body. Build431 contains
large lateral tracked-head offsets, but those are requested offsets, not proof of
misalignment. No guessed nonzero correction ships.

Validation:96 catalog/policy checks plus22 existing handoff checks;1000 calls
through the extracted production x86 stub verify pass/reject return, stack cleanup,
this pointer and request parameter. Camera math and extracted scope checks include
alignment unit conversion, pair freezing and exact restoration. Headset pending.
First launch isolates cancellation: Jumping disabled in the candidate INI; enabling
it live should restore jumping. Arms/alignment remain at saved settings/zero.

## VR-134 F10 Animations (2026-09-17)

The new Animations tab exposes all40 shipped FSM lane/state entries listed in
section2.1. A checked active state hands arms/weapons back to native animation;
any checked lane can request it. Unchecking a state removes that trigger only.
The master enable applies to all choices. Existing250ms release/150ms blend and
stale-state fail-soft remain. No changes to animation playback or engine writes.

Persistent overrides use [Anim] Arms.<lane>.<state>=0|1. Missing keys inherit
HandBackMaster/HandBackUpper and mantle/cinematic defaults. Checkboxes save live;
reset deletes only these40 overrides. Default profile need not materialize every
inherited value. Active state labels and filter help find an action. Clip history
is not playback state; individual clips within one action share its checkbox.
85 catalog checks plus existing blend/freshness tests pass. Headset pending.

# VR-88 plan: know when a scripted animation owns the body, and hand it back

**Status: phases 1 and 2 implemented together, enabled by default at user request. Headset validation pending: the first playtest after implementation ran the previous installed build, so it is not evidence for this code.** Branch `claude/vr-88-anim-handback`, off
`VR-Main` at `5e076813`. Ticket VR-88; the long-term layer is VR-89.
Sections 2 and 3 are the research and the claims a run must prove; sections 4 to 6 are the design, and the implementation notes at the end record what landed.

## 1. The problem and the goal

During a takedown, a choke, a drop assassination or a fatality, the game animates the
arms and the held weapon through the move. The mod keeps doing its own thing on every
draw: the bone palette correction moves the hands and the weapon to the controllers,
the arm split draws only the hand triangles, and the weapon attachment suppresses the
copies it did not place. So the move plays with the player's hands wherever the
controllers happen to be, or with nothing visible at all.

**Goal of VR-88, in two phases:**

1. **A flag.** Read, not infer, whether a scripted full-body action owns the player's
   body right now, and which one. Read-only, log changes, publish a snapshot.
2. **The hand-back.** While the flag says the game owns the body, stop the hand and
   weapon overrides and let the game's own arms and weapon draw; take them back
   afterwards without a pop. Behind a default-on lever with a live A/B.

**The long-term goal (VR-89)** is an animation control layer: know exactly which
animation plays, allow or suppress specific ones, trigger animations on demand, and pose
the hands freely. Section 8 sketches how the pieces found here serve that, so phase 1 is
built as its first layer and not as a one-off.

## 2. What the decompiled scripts say

Declarations only; the dump has no function bodies. Names are the claims; **every offset
is derived at runtime by name**, never taken from here. Reviewer: please spot-check the
class and property names against `tools/uscript/dishonored/`.

### 2.1 The player runs three native state machines, and the master one is the flag

`DishonoredPlayerPawn` declares three `DishonoredNativeStateMachine` members:

| Member | Role, from its default object |
|---|---|
| `m_pPlayerMasterFSM` | the whole body: 23 state templates (below) and a transition table |
| `m_pPlayerUpperFSM` | a `DisNativeStateMachine_PlayerAction`, `m_ActionUsage = Upperbody`, bound to the `ANIMSTATE_UPPER_BODY` picker |
| `m_pPlayerLeftArmFSM` | a `DisNativeStateMachine_PlayerAction`, `m_ActionUsage = LeftHand`, bound to the `ANIMSTATE_SPECIAL` picker |

`DishonoredNativeStateMachine` declares `m_pCurrentState` (a `DishonoredNativeState`
object), `m_pCurrentStateID` (a `Class`), `m_pPendingStateID`, `m_pPendingState`,
`m_bIsLocked`, the template array `m_NativeStates`, and `m_pTransitionLogic`.

**So "what is the body doing" is the CLASS NAME of the master machine's current state.**
One pointer chain and one class-name fetch: pawn, `m_pPlayerMasterFSM`,
`m_pCurrentState`, `ObjClassName`. `m_pCurrentStateID` is the same answer by another
route and is the cross-check.

Master FSM states (template classes): `StatePlayerMasterWalk`, `Leaning`, `Swim`,
`Jump`, `Falling`, `MasterAction`, `Versus`, `ChangeReadyStance`, `Mantle`, `Stunned`,
`Dead`, `InDialog`, `Soiree`, `InScriptedChoice`, `Assassinate`, `HolePeeking`, `Climb`,
`PrePossess`, `Possess`, `Slide`, `Minigame`, `Choke`, `InStore` (every class is
`StatePlayerMaster<name>`; a few template OBJECTS are named `StatePlayerWalk_Template`
and similar, which does not matter because the reader uses the class).

Upper FSM states: `StatePlayerUpperIdle`, `StatePlayerMeleeAttack`, `StatePlayerBlock`,
`StatePlayerGenericFatality`, `StatePlayerAction`, `StatePlayerTransitionItemIn`,
`StatePlayerEquipChange`, `StatePlayerChangeReadyStance`, `StatePlayerGrabMovable`,
`StatePlayerGrabCorpse`, `StatePlayerCarryCorpseIdle`.

Left arm FSM states: `StatePlayerUpperIdle`, `StatePlayerAction`,
`StatePlayerTransitionItemIn`, `StatePlayerEquipChange`, `StatePlayerUpperNav`,
`StatePlayerGrabMovable`.

Supporting evidence that `Assassinate` and `Choke` are exclusive, long-lived states: the
master transition table marks leaving either for any `DishonoredNativeState` as
NotAllowed, with explicit exceptions only to `Dead`, `Swim`, `Soiree` and
`Choice_Base`. The table gives the same locked-in shape (leave for anything: NotAllowed,
then a short allow list) to `Versus`, `Minigame`, `Stunned`, `Soiree`,
`InScriptedChoice`, `InStore` and `InDialog`: nine states in all where the engine itself
refuses to let ordinary movement interrupt. That list is the natural first draft of
"the game owns the body", before any run. `StatePlayerMasterAssassinate` also defaults `m_bAllowIncomingAttacks`
to false. `StatePlayerMasterChoke` and `StatePlayerMasterMantle` both derive from
`StatePlayerMasterAction`.

### 2.2 The earlier candidate is a config value, not a live flag

`GAMEPLAY_STATE.md` and ENGINE_NOTES name `eDisPlayerActionUsage_Fullbody` as "the
takedown and choke discriminator". The dump does not support reading it that way:
`eDisPlayerActionUsage` is the TYPE of `DisNativeStateMachine_PlayerAction.m_ActionUsage`,
set once per machine in its default object (Upperbody, LeftHand). Nothing declared holds a
changing Fullbody value. **Those two docs should be corrected in the same commit as the
code**, with this plan as the reference. If the reviewer finds a live holder, this
section is wrong and says so.

### 2.3 Secondary evidence, readable by name

| Where | What it adds |
|---|---|
| `DishonoredPlayerPawn.m_BodyMode` (`ePlayerBodyMode`: `ARMS_ONLY`, `FULL_BODY`, `HIDDEN`) | whether the game has switched to the full-body mesh. ENGINE_NOTES: it swaps meshes on the one component. A likely co-signal for takedowns; to be MEASURED, not assumed |
| `DishonoredPlayerPawn.m_pAnimStateComp` (`DisAnimStateComponent`) | `m_StatePickers` (one per picker: current state pointer, `m_bLockedOut`, `m_bLockedOut_Children`), and **`m_AnimSeqHistory[12]`**: a ring of (sequence FName, picker index) with `m_iCurNewestSeqInHistory`. The names of the last twelve sequences played, per picker. This is "which animation", readable with no hook |
| `DishonoredPlayerPawn.m_AnimStates_FullBody_Navigation / _Assassination / _Fatality / _Misc`, `m_AnimStates_UpperBody_Items(_Melee/_Ranged) / _Misc`, `m_AnimStates_LeftHand_Items`, `m_AnimStates_Special_Misc` | arrays of `DisPawnAnimState` (state name, blend time, tree name, non-looping, custom sequences, root motion mode). The vocabulary the history's names come from |
| `DisItemContext.m_ContextStatus` (`Idle / Failed / InProgress / Finished`) | per weapon action; `DisItemContext_Choke.m_State` (`In / Loop / Win / Win_HoistCorpse / Lose / Cancel`), `DisItemContext_Fatality.m_CachedFatalityType` (`Synced / Generic`) |
| `DishonoredPlayerPawn.m_pMatineeBlender` (`ArkAnimNodeBlendPose`) | the cinematic pose blend; `m_bEnabled`, `ActiveChildIndex` |
| `DishonoredPlayerPawn.m_pLookAtControl_LeftHand / _RightHand / _Camera` | `SkelControlSingleBone`s already on the rig (VR-30 notes the camera one) |
| `DisTweaks_Assassinate` move sets (front, back, left, right, fast, bend-time-frozen, special) and `DisItemAction.m_AnimStates_NonReady` | the tuning names a takedown picks its animation from |

### 2.4 The callable surface, from the native registration table

`python tools/ue3-natives.py --verify <exe> natives --grep <term>` (the crossbow
re-derivation passed before answering). Registered execs relevant here:

* `AnimNodeSlot`: `PlayCustomAnim`, `PlayCustomAnimByDuration`, `StopCustomAnim`,
  `SetCustomAnim`, `GetCustomAnimNodeSeq`; `AnimNodePlayCustomAnim`: the same three.
* `SkeletalMeshComponent`: `FindAnimNode`, `FindAnimSequence`, `FindSkelControl`,
  `UpdateAnimations`, `SetAnimTreeTemplate`.
* `AnimNode`: `PlayAnim`, `StopAnim`, `ReplayAnim`, `FindAnimNode`;
  `AnimNodeBlend` / `AnimNodeBlendPerBone`: `SetBlendTarget`;
  `ArkAnimNodeBlendPose`: `SetActiveChild`.
* `SkelControlBase`: `SetSkelControlStrength`, `SetSkelControlActive`;
  `SkelControlLookAt`: `SetTargetLocation`, `SetLookAtAlpha`.
* Cheats, useful as TEST instruments: `PlayerSetFixedFatality`, `PlayerDisableFatality`,
  `PlayerDisableAssassinate`, `TogglePlayerBodyMode_Native`, `NPCForceFatality`.
* `DishonoredPlayerPawn.OnToggleChoke` (a Kismet action handler).

**Negative, which matters for VR-89:** no exec touches the player state machines. Their
transitions are native only, so from outside they are READ-ONLY; suppressing a state is a
native-hook problem, not a function call.

The call-by-name lane already exists: `FindFunctionObj` plus a direct `ProcessEvent`
call, used by `console.cpp` and `mat_hide.cpp`. `FindFunctionObj` matches the function
NAME only, and `PlayCustomAnim` exists on two classes, so any use of it must also match
the function's Outer.

## 3. Claims the first run must prove or kill

Rule 3 of GAMEPLAY_STATE: a flag is not a flag until a run shows it CHANGING.

| Claim | Prediction | Killed by |
|---|---|---|
| C1: the master state names the action | walk, jump, mantle, slide, choke, takedown each show their own `StatePlayerMaster*` class, entering at the move's start and leaving at its end | a takedown that leaves the master state at `Walk`, or a state that never changes |
| C2: front/back takedowns and drop assassinations are all `Assassinate` | one class for all three | a different class per variant (then the classifier grows, the design holds) |
| C3: fatalities live in the UPPER machine | `StatePlayerGenericFatality` in upper while master stays in a combat-capable state | a fatality with no upper change |
| C4: `m_BodyMode` moves with full-body moves | `FULL_BODY` during takedowns and chokes, `ARMS_ONLY` otherwise | no change in any move (then it is only for cinematics, and it drops out of the classifier) |
| C5: the sequence history identifies the animation | a new FName at the start of each move, different per move | a history that never advances |
| C6: the state pointer is stable within a state | the same `m_pCurrentState` pointer for the whole move | a pointer that changes every tick (templates cloned per entry would still work, but freshness rules change) |

## 4. Phase 1: the flag (read-only)

### 4.1 Where and how it reads

New module `game/dishonored/anim_state.cpp`, script lane, ticked from the ProcessEvent
camera pass beside `RflTick`.

* **Resolve by name, once, lazily**, through `ue3/reflect.cpp`'s cached `RflOffsetOf`,
  behind its GNames gate: `DishonoredPlayerPawn.m_pPlayerMasterFSM / m_pPlayerUpperFSM /
  m_pPlayerLeftArmFSM / m_BodyMode / m_pAnimStateComp`,
  `DishonoredNativeStateMachine.m_pCurrentState / m_pCurrentStateID / m_pPendingStateID`,
  `DisAnimStateComponent.m_AnimSeqHistory / m_iCurNewestSeqInHistory`. Every miss logs the
  class and property and leaves the flag UNKNOWN.
* **The pawn** comes from the existing controller-to-pawn path (`crouch.cpp` already
  resolves it). `IsLiveObject` gates the pawn when its pointer changes, and each FSM
  object when its pointer changes. These are reads, but a stale pawn after a level load
  is exactly the case the class-name guard failed on in VR-85, so the stronger test is
  used from the start.
* **Per tick** it reads three pointers and compares them with the last ones. Only a
  CHANGED state pointer pays for `ObjClassName`; the name is cached against the pointer.
  The sequence history is read as one index plus one FName when the index moves.
  Cost per tick: a handful of dword reads. No per-property validation (TRAPS, the
  PropWatch frame-budget entry).
* **No writes of any kind.**

### 4.2 What it publishes

A snapshot under an SRW lock, with a generation and a script-lane timestamp:

```
master, upper, left   state class name (interned), state pointer, entered-at ms
pending master        class name or none
bodyMode              0/1/2 or unknown
lastSeq               newest history FName and its picker, and when it changed
owner                 PLAYER | GAME | UNKNOWN, and the rule that decided it
```

Consumers on the present thread take a copy and check the generation's age; a snapshot
older than 150 ms reads as UNKNOWN (fail inert: today's behaviour).

### 4.3 The classifier

Data, not code: `[Anim] HandBackMaster=` and `[Anim] HandBackUpper=`, comma lists of
class names, with a compiled default that is a HYPOTHESIS to be confirmed by the phase 1
run:

* master: `StatePlayerMasterAssassinate, StatePlayerMasterChoke, StatePlayerMasterClimb,
  StatePlayerMasterStunned, StatePlayerMasterDead, StatePlayerMasterPrePossess,
  StatePlayerMasterPossess, StatePlayerMasterMinigame` (Mantle was in the first draft and was
  removed after the headset run: see the results below)
* upper: `StatePlayerGenericFatality, StatePlayerGrabCorpse`

Open for the run to decide, deliberately NOT in the default: `Slide`, `Versus`,
`InDialog`, `Soiree`, `InScriptedChoice`, `InStore`, `HolePeeking`, `CarryCorpseIdle`,
`MeleeAttack`.

Owner = GAME when any listed state is current. **Hysteresis:** GAME is entered
immediately; PLAYER returns only after the machine has been out of every listed state for
`[Anim] ReleaseMs` (default 250, not measured), so a state that flickers through `Walk`
between two scripted states does not bounce the hands.

### 4.4 Instruments

* The log, changes only:
  `anim: master StatePlayerMasterWalk -> StatePlayerMasterAssassinate (pending none) | upper StatePlayerUpperIdle | left StatePlayerUpperIdle | body ARMS_ONLY -> FULL_BODY | seq <name> (picker 0) | owner PLAYER -> GAME by master rule`.
* A 5 s beat that prints even when nothing changed, with the resolve status and the
  snapshot age, so "never resolved" and "nothing happened" read differently.
* `anim status`, the F10 Debug line, and `status.json`.
* `[Anim] StateWatch` gates the whole reader: **ships 1 (user-requested default)**, armed in the tester's
  installed ini for the run.

### 4.5 Host tests

A pure classifier with injected snapshots: entry is immediate, release honours ReleaseMs,
a stale snapshot reads UNKNOWN, an unresolved property reads UNKNOWN and never GAME, the
ini lists parse with spaces and unknown names, and a name not in any list is PLAYER.

## 5. Phase 2: the hand-back

Built together with phase 1 at the user's request; C1 remains to be checked in the combined headset run. One lever, `[Anim] HandBack`, **ships 1 (user-requested default)**,
live `anim handback on|off`, an F10 Hands checkbox, effective value logged with its source.

### 5.1 What stops while the owner is GAME

Named by module, so the reviewer can check nothing is missed:

| Override | Module | While GAME |
|---|---|---|
| hand placement (the palette correction D on the hand draws) | `mesh_split.cpp` `MsDraw` | D blends to identity (5.2), then the draw passes through |
| arm suppression (drawing only the hand classes) | `mesh_split.cpp` `MsDraw` | the full player mesh draws: the game's own arms |
| our D3D11 hands over the image (VR-31) | `core` `HandDrawFn` from `present_tick.cpp` | not drawn |
| weapon placement and duplicate suppression | `weapon_attach.cpp` `WaDraw` | pass through, suppress nothing |
| SkelControl rotation drives | `skelcontrol.cpp` `SkcRotApply` | not applied |
| arm hiding | `arms_hide.cpp` | not applied |
| the aim laser and dot (VR-57) | `aim_visual` | hidden |

Unchanged: head tracking, the camera seam, the neck term, fire seams (the game does not
fire during these moves; logged if it does).

### 5.2 No pop in either direction

The palette route already composes one rigid D per draw. On entry D slerps from its last
target to identity over `[Anim] HandBackBlendMs` (default 150, not measured), and only
then do the arm split and weapon suppression release; on exit the split and suppression
resume at identity and D slerps back to the controller target. The arm cut cannot blend,
so it switches at the identity end of each blend, where the hands are exactly where the
game draws them.

### 5.3 Lanes

The owner is read on the script lane; draws happen on the render thread up to a frame
later (UE3's one-frame thread lag, the reentry ring's comment). A handback that lands one
frame late shows one frame of our hands at the start of a move, which the blend hides.
Reviewer: is a per-draw generation match worth the complexity here, or is the blend
enough?

### 5.4 Fail inert

Unresolved, stale, lever off, or pawn not live: the owner is PLAYER and every module
behaves exactly as today. A module that cannot identify identity for its D refuses the
blend and passes the draw through untouched, logging why.

## 6. Launches

**Launch 1 (phase 1 build, `StateWatch=1`): which states do these moves enter?**
Load a save with a guard nearby. In order, with a few seconds of normal walking between
each: jump; mantle onto a ledge; sprint and slide; choke a guard from behind; a stealth
takedown from behind; a takedown from the front (or a drop assassination if easier); a
combat fatality; pick up and drop a body. Quit through the menu.
Answers C1 to C6 from the log. Outcomes: C1 holds, go to phase 2 with the measured lists;
C1 fails, the reader is wrong or the flag lives elsewhere, and the next step is the
sequence history (C5) or `propwatch` on the pawn during a takedown.

**Launch 2 (phase 2 build, `HandBack=1`): do takedowns show the game's arms, and do the
hands come back cleanly?** The same moves; judge the start and end of each. F10 toggles
the lever for an A/B inside the run.

## 7. Rules carried

No hardcoded offsets; property names resolved and logged. `IsLiveObject` for any object
whose pointer is retained across ticks. Log changes, not state. The VR-88 levers default on at user request,
with a live A/B. The installed ini is diffed in full on every install. No game content
committed: this document lists names only.

## 8. Toward VR-89, the animation control layer

What phase 1 builds is layer one; each later layer is its own ticket.

1. **Which animation.** The sequence history and item-context states, joined to the FSM
   state, give (state, sequence name, context phase). That tuple is the key every later
   rule is written against.
2. **Rules per animation.** Allow, hand back, or suppress, keyed on that tuple, loaded
   from data.
3. **Trigger.** `SkeletalMeshComponent.FindAnimNode(slotName)` on the player mesh, then
   `AnimNodeSlot.PlayCustomAnim(seq, rate, blendIn, blendOut, loop, override)` through the
   existing ProcessEvent lane, the parameter frame resolved from the UFunction's own
   properties by name, the function matched by Outer. First target: a harmless idle
   variant, proved by the sequence history showing it.
4. **Suppress.** No script surface exists for the state machines. Candidates, cheapest
   first: neutralise at the node (slot weights, `SetBlendTarget` on the per-bone filters
   the pickers hold); find the native transition request with the natives table and a
   caller census (`tools/pe-xref.ps1`) and gate it. The transition table in 2.1 is the
   engine's own allow list and a natural place for such a gate.
5. **Pose.** Per-bone local rotations composed into the bone palette the hands already
   rewrite: no engine writes, and the bone map comes from the arm split's bone analysis.
   `SkelControlBase.SetSkelControlStrength` on the rig's own controls is the engine-side
   alternative, with the 32.6 freed-AnimTree hazard and `IsLiveObject` on every write.

## 9. Open questions for the reviewer

1. Is the master FSM's `m_pCurrentState` class name the right primary signal, or should
   `m_pCurrentStateID` (a Class, no instance) be primary because it cannot dangle?
2. Section 2.2 contradicts two existing docs. Agree, or is there a live Fullbody holder?
3. The default hand-back lists in 4.3: anything that must be in or out before the run?
4. Section 5.3: generation-matched hand-back per draw, or blend only?
5. Should phase 1 also read the item contexts (choke `m_State`, context status), or wait
   for layer 1 of VR-89?
6. Mantle and climb animate the arms too. Handing them back loses the controllers during
   every ledge grab. Include them by default, or leave them to a separate lever?


## Implementation notes for the combined build

`anim_state.cpp` samples the three FSMs on the script lane at most once per
10 ms, using the controller's reflected Pawn link. The state instance and class
ID must agree. Required read failures, watch off, or a snapshot older than 150 ms
mean UNKNOWN and retain the existing controller behavior. Optional body/history
failures do not invalidate an otherwise readable ownership signal. Pending state
and sequence names are resolved on changes; a five-second heartbeat exposes an
unresolved reader. The SRW-locked snapshot and status.json report state names,
owner, sequence, body mode and controller blend weight.

`StateWatch=1` and `HandBack=1` are both the compiled fallback and generated ini
default. Commands: `anim status`, `anim watch on|off`, `anim handback on|off`.
F10 Hands exposes the handback checkbox and current state. Master/upper comma
lists remain configurable through `HandBackMaster` and `HandBackUpper`; mantle
was included at first and is now excluded; climb (ladders) stays (results below). VR-89 suppression and on-demand playback remain deferred.

The correction uses shortest-path rotation interpolation, with uniform scale
and translation interpolated to identity. It is blended ONCE in the hand path
before publication to weapons. Weapon copies inherit that same correction
through their existing coordinate transforms. Re-blending in each weapon's
local frame is incorrect. A present consumes a fixed blend weight. Animated
source and controller target continue updating during the blend. This is
present-level consistency, not a claim of exact FSM/draw pairing.

At identity the draw router bypasses split and duplicate suppression, including
non-indexed draws. D3D11 hands and aim visuals are hidden during ownership and
blend-back. The script path releases hand SkelControl application flags and model
scale, detaches active graft donors, restores mesh rotation and mod-hidden arms,
and suspends further hand writes. Camera look-at and fire aiming remain
independent. Controller writes resume on return and the palette blends back.
Stale snapshots and a disabled lever return to existing behavior.

The optional sequence record validates as FName plus picker int, with stride
derived from the reflected picker offset. The property resolver gained a checked
form so a real member at offset zero is distinguishable from a lookup failure.
Property scans are initialization work, not repeated sample work.

One combined headset run remains: walking, jump, mantle/climb, slide, choke,
back/front/drop assassination, fatality, and pick up/drop a body. Compare
HandBack on/off in F10, check both eyes and action entry/exit, then reload a save.
The state labels and 150/250 ms blend/release settings remain hypotheses until
that run. A successful build does not confirm mappings or visual comfort.

## Headset results (2026-09-13)

Run on build `a9a138d1` plus the stale live-object table fix (`a59cfbd2`). The reader rebuilt
the table twice after the level loaded, then tracked every move tried. Observed master states:
`Walk`, `Falling`, `Jump`, `Swim`, `Mantle` (2), `Assassinate` (4, with `m_BodyMode` going to
FULL_BODY during the move); upper: `GenericFatality` (2), `GrabCorpse`, `CarryCorpseIdle`,
`EquipChange`, `TransitionItemIn`. Each listed state entered GAME at the move's start and
returned to PLAYER after the release interval. Claims C1, C3 and C4 held for the moves tried;
`Climb`, `Choke`, slides and drop assassinations were not observed in this run.

The takedowns, fatalities and body pickup were judged right in the headset. **Ledge climbing
(`Mantle`) was judged better WITHOUT the hand-back**: the controller hands should stay, so
`Mantle` is removed from the default `HandBackMaster` list. Ladder climbing (`Climb`) keeps
the hand-back by request, though it was not observed in this run. Not every action
variant was tried.

## Swing and shot on the tracked hands (2026-09-19)

Run483 measured where a shot lives: `Pistol_Fire#0` plays inside
`StatePlayerAction` (the upper lane, the left lane, or both). That state also
carries reloads and the sword sneak in/out, so the shot is matched on the CLIP
name (any sequence containing "fire", case-insensitive), not on the state. The
sword swing is its own state, `StatePlayerMeleeAttack`. Both now use the mantle
path: native pose on the hands with the forearms hidden (`mantleSplit`), unless
that state's "Show game arms" is also checked. The camera classifier is NOT
widened, so neither takes the camera. Levers: `[Anim] HandAnimMelee`,
`[Anim] HandAnimFire`, default 0, in the F10 Animations tab (saved on change).
The crossbow's fire clip name is not yet measured; the next run's `anim:` lines
say whether it contains "fire".
