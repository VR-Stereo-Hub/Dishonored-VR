# Next session: near-eye rain, lens effects, weapon mirror (installed462)

## Start here
- Work in C:/dev/Dishonored-VR on codex/misc-fixes. Keep existing branch.
- Read CLAUDE.md, AGENTS.md, only the top 3 STATUS sections, then this file.
- Commits are credited to the configured GitHub identity (BioVRDev). No trailers,
  no names in branch names. No subagents, game/simulator launches, or merges.
- Before reading a playtest log: banner must say
  `vr33-hands-working-462-gdd19939ac`, installed DLL SHA256 b212d154...
  Archive dishonored_vr.log and dishonored_vr.prev.log before any reinstall.

## Installed baseline (preserve)
- Build462 = `vr33-hands-working-462-gdd19939ac`, DLL b212d154... Manifest:
  build/playtest-candidates/near-eye-effects/manifest.json; the 458 run's logs
  are in its preinstall/ (and in possession-rain/support-20260918-085448-848.zip).
- Installed ini SHA a55a9106... (458's, unchanged): `[Cine] PossessionStereo=1`,
  `[Rain] Hide=0 Trace=1`. Rain Distance and the [Lens] keys are not in it; code
  defaults are native (Rain -1, Lens 0, KeepSize 1, Trace 1).
- Streaming: 13 NumStreamedMips=-1 in both game INIs. Unchanged.
- Possession stereo headset-CONFIRMED on 458 (rat). 462 widens it to every
  DisPossessablePawn class; watch for `possession/stereo: SuperField chain
  check ... TRUSTED` and a VALIDATED line naming a DishonoredNPCPawn.
- The memory watcher was stopped by the tester. Ask before re-arming.

## Launch question NOW (build470, VR-140): can a fast wheel flick still black the world?
Banner `vr33-hands-working-470-gc4becc905`, DLL 572c930c.... Tap the wheel open
and shut as fast as possible, many times (with and without Dark Vision).
- World never goes black: the restore was the cause (FLICKER_REFERENCE VR-140).
- Black again: read for `menu/blur: UI post-process weight ... held` (WARN). If it
  fired, the game itself left the weight up - find the other owner; if it did
  not, the black is not this weight.
Passive: `mirror/build` kept counts and `mirror/beat` back-face passes; the
pistol/crossbow look is an observation, not this launch's question.

## Previous question (build467, VR-138): answered - pistol mostly filled, crossbow no change
Banner `vr33-hands-working-467-g8f79caeae`, DLL e939e54b.... Play normally for
5+ minutes (walking included) so the hitch instruments collect, and look at the
far side of the pistol and the crossbow.
- Filled in: read `mirror/plane` + `mirror/build` (MEASURED plane, kept K).
- Still hollow: `mirror/build ... REFUSED` names why; `mirror/plane` gives every
  axis's score for a `mirror plane <asset> <axis> <offset> <sign>` override.
- Floating copy: wrong axis/offset; override as above.
Passive, no question: at every frame gap read `device/stream (gap)` (MB uploaded
/created in the 2 s before) and `gpumem (gap)` (VRAM vs budget, NON_LOCAL,
free address range). PERFORMANCE.md top section says what each reading kills.

## Previous launch question (VR-138, build464): answered - no change, both REFUSED
Build464 (banner `vr33-hands-working-464-g0b7171bd1`, DLL affccb00...), the
installed ini arms `[Mirror] Enabled=1`. Hold the pistol, turn it to see the
side that used to be hollow; then the crossbow.
- Solid, no flicker on parts that were already modelled: keep it.
- A copy floating beside the gun: the plane is wrong. Read `mirror/build`
  (face counts, plane) and fix live: `mirror plane <asset> <x|y|z> <offset> <+|->`.
- Still hollow: `mirror/build` says REFUSED (reason given) or kept 0; or
  `mirror/beat` drawn stays 0 (the asset name differs: `[Mirror] Assets`).
- Flicker/z-fight on the grip or stock: raise `[Mirror] FillRadius`.
F10 "Mirror pistol/crossbow" toggles it live for an A/B.

## Launch question after that (VR-136): does moving the rain slab fix the pane?
In a rainy area, open F10, set "Rain distance uu" to 0, look around.
- Rain around you, falling past, no sheet: keep it; next build writes
  `[Rain] Distance=0` into the installed ini (byte-aware, CRLF).
- Still a sheet, just nearer: the drop module's own spawn volume is the sheet;
  try 100-200, then the hide checkbox is the fallback the user authorised.
- No change at all: the log must show `rain: distance lever took camera ...`;
  if it does, the engine overrides the extent (look for `extent was reset`).
Passive in the same run: `lens/fx` lines when hurt (the health lens position),
and any person possession (`VALIDATED ... DishonoredNPCPawn`).

## Next question after that (VR-137): the low-health vignette
Get hurt below the vignette threshold. Read `lens/fx ... HEALTH lens ... fwd=`
first: fwd vs DistFromCamera 90 says whether the distance is FOV-scaled. Then
F10 "Lens effects distance" 10..30 with "keep their size" on: does the red
read as near-eye rather than a pane? KeepSize off spreads it outward.

## VR-138: implement the weapon mirror
docs/dishonored/WEAPON_MIRROR_PLAN.md is written to be implemented as is
(new weapon_mirror.cpp, three call sites in weapon_attach.cpp, WaMesh field,
[Mirror] keys, frame_test additions, the build/beat log lines).

## Code map
- src/game/dishonored/possession_state.cpp: validator (script lane, 50 ms).
  stereo_state.cpp consumes `PossessionStereoLive()`; policy in
  stereo_state_policy.h `possession_eligible`.
- src/game/dishonored/rain_control.cpp: rain measurement, native hide, distance.
- src/game/dishonored/lens_control.cpp: lens effect measurement and distance.
- ENGINE_NOTES top section: field derivations. FLICKER_REFERENCE top entry:
  the measured possession gate and counterprediction.

## Keep scope controlled
Health vignette remains invisible and unresolved (separate item). Do not reopen
performance experiments or animation pose broadening. Put measurements and
verdicts on VR-135 / VR-136 in Linear, update STATUS, FLICKER_REFERENCE and
ENGINE_NOTES with actual results.
