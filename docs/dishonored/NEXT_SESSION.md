# Next session: possession stereo and rain (installed458)

## Start here
- Work in C:/dev/Dishonored-VR on codex/misc-fixes. Keep existing branch.
- Read CLAUDE.md, AGENTS.md, only the top 3 STATUS sections, then this file.
- Commits are credited to the configured GitHub identity (BioVRDev). No trailers,
  no names in branch names. No subagents, game/simulator launches, or merges.
- Before reading a playtest log: banner must say
  `vr33-hands-working-458-ge5246ba2f`, installed DLL SHA256 8aef77f4...
  Archive dishonored_vr.log and dishonored_vr.prev.log before any reinstall.

## Installed baseline (preserve)
- Build458 = 452's accepted camera/controller/wheel behavior plus two new
  levers. Manifest: build/playtest-candidates/possession-rain/manifest.json;
  pre-install 452 logs, ini and crash txt in its preinstall/.
- Installed ini vs 452: `[Cine] PossessionStereo=1` added; `[Rain] Hide=0
  Trace=1` added. Nothing else. CRLF, 1236 lines, SHA a55a9106...
- Texture streaming config unchanged: 13 NumStreamedMips=-1 in the user
  DishonoredEngine.ini and the installed DefaultEngine.ini.
- Memory watcher: D:/dvr-data/support-watch/20260918-082952-438 (Memory mode,
  3000 MB, build/diagnostics/procdump/procdump.exe, signed x86). It was started
  from a Claude session; confirm it is alive (procdump or powershell waiting)
  before the launch, re-arm with tools/watch-crashes.ps1 if not.

## Launch question 1 (VR-135): is a rat possession stereo?
Load a save, possess a rat (or a person), move around for ~10 s, release.
- Expected: depth during the possession; entry zoom and exit as before.
  Log: `possession/stereo: VALIDATED ... (DisPossessionProxyPawn)`, then
  `stereo/state: STEREO ... possessed=1`, beat `L/s` = `R/s`, no `mono/s`.
- Still flat, log shows VALIDATED and STEREO: the verdict passed but something
  downstream (method/runtime) refuses; read `reentry: gates` for the reason.
- Still flat, `possession/stereo: not validated reason=...`: the reason names
  the failing check (layout unresolved, back-pointer mismatch, liveness).
- Stereo but wrong (eyes swapped, doubled, scale odd, entry/exit flash): the
  gate works; the camera seam on a possessed pawn is the next question.
- Any menu or load during possession must stay as before (mono screen where it
  was mono). Regression there = revert with `possessionstereo off` or F10.

## Launch question 2 (VR-136): rain, only after Q1 is answered
Two steps; do not combine with Q1.
1. From the Q1 log (if it rained) or a rainy area run: read the `rain/box` lines.
   `extent` is the box around the view, `fwd/right/up/dist` the emitter's
   position in the camera frame (100 uu = 1 m). A near-eye design (shrinking
   the extent or moving the emitter toward the eyes) is derived from these; no
   near-eye code exists yet.
2. If near-eye is not feasible or not wanted: set `[Rain] Hide=1` (byte-aware,
   CRLF) or tick "Hide camera rain" in F10. Question: is the rain pane gone
   while other particles (fire, blood, impacts) remain? Log: `rain: HID ...
   HiddenGame 0 -> 1`. If the pane stays with HiddenGame 1, the pane is not this
   emitter: say so, do not widen the hide.

## Code map
- src/game/dishonored/possession_state.cpp: validator (script lane, 50 ms).
  stereo_state.cpp consumes `PossessionStereoLive()`; policy in
  stereo_state_policy.h `possession_eligible`.
- src/game/dishonored/rain_control.cpp: rain measurement + native hide.
- ENGINE_NOTES top section: field derivations. FLICKER_REFERENCE top entry:
  the measured possession gate and counterprediction.

## Keep scope controlled
Health vignette remains invisible and unresolved (separate item). Do not reopen
performance experiments or animation pose broadening. Put measurements and
verdicts on VR-135 / VR-136 in Linear, update STATUS, FLICKER_REFERENCE and
ENGINE_NOTES with actual results.
