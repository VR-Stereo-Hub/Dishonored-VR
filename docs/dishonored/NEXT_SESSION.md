# Next session (after installed470)

## Start here
- C:/dev/Dishonored-VR, branch codex/misc-fixes (pushed; no PR open).
- Read CLAUDE.md, AGENTS.md, the top 3 STATUS sections, then this file.
- Commits under the configured identity (BioVRDev); no trailers, no names in
  branch names. No subagents, no game/simulator launches, no merge. Build and
  install yourself; archive both logs before any reinstall; diff the whole
  installed ini and keep CRLF; one question per launch with expected outcomes.
- Before reading a playtest log, check the banner. Installed now:
  `vr33-hands-working-470-gc4becc905`, DLL SHA256 572c930c...; its run is
  archived in build/playtest-candidates/wheel-blackout/run470.

## Priority 1 - VR-140: world goes black after fast weapon-wheel flicks
Reproducible by flicking the wheel open/shut fast. FLICKER_REFERENCE top entry,
items 6-8, has the measured timing: the black starts ~400 ms after the game's
`pPowerWheel` movie finally closes (bMovieIsOpen 1 -> 0) after being re-opened
mid-close; `stereo: frameid ... ONE PICTURE at stage bb` marks the onset; stereo
pairs and draw counts stay normal. NOT the UI blur weight (retracted).
Next: (1) an instrument at the ONE-PICTURE transition dumping post-process
manager entry weights, camera FOV/rotation and hudcap routing state;
(2) a config A/B isolating the mod's wheel features (NoBlurWheel=0, wheel ride
off) to learn whether the game blacks out on its own.

## Priority 2 - VR-138 weapon mirror
Pistol: nearly done, a little missing near the handle (kept 1248 of 2772,
901 skipped as already modelled, 588 straddling the plane).
Crossbow: top symmetric (x score 0.976), bottom-left missing; only 50 triangles
kept because the single plane comes from the top. Tester: lower parts of both
models are what stay missing. Idea: mirror per height band / connected component
with its own plane, or relax "other side" for triangles straddling the plane.
Back-face pass (`[Mirror] BackFaces=1`) showed no visible help. Code:
src/game/dishonored/hands/weapon_mirror.cpp; plan: WEAPON_MIRROR_PLAN.md.

## Priority 3 - rain pane (VR-136/137)
The pane is the lens effect `DisEmitterCameraLensEffect_Looping`; F10 "Lens
effects distance" brings it to the eyes (one step too far puts it behind).
Tester wants it smaller: with KeepSize off, scale DrawScale down via the native
Actor.SetDrawScale, per class; add a per-class lens hide. Code:
src/game/dishonored/lens_control.cpp. ENGINE_NOTES top section.

## Settled this session
- VR-139 periodic hitch: VD network (router restart + H.264+ fixed it).
- VR-135 possession stereo: works for rats; all DisPossessablePawn classes now.
