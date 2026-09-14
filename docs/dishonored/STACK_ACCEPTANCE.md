# Accepted cinematic, standing-camera and mono/UI stack

## Final acceptance, 2026-09-14

PR56, PR57 and PR58 are headset-accepted and merged to VR-Main in order:
02a9abfa (PR56),4162a732 (PR57),f020cdb9 (PR58). All branches are retained. This is integration of tested work, not a declared release.
Final crossbow test: build264-gfe400945, compiled Sep14 11:06:45, verified by
log banner and installed DLL SHA256
8af719668288d600db9e99c30cfb76d0ccd8570b79805673224fff68aba953d3.
Both logs and the INI are archived at
build/mono-ui-test/accepted-20260914-111707. The tester reports the remaining
crossbow transparency resolved. Prior acceptance of cinematic controls,
standing roll, mono transitions and native animations remains in force.

## Findings and final behavior

| PR | Accepted behavior | Findings and rejected approaches |
|---|---|---|
| 56 | Physical cinematic free look, upright pitch/roll comfort, full-size cinematic FOV through exit blends, native visible hands/arms in cinematics and mantles, head-based movement, continuous cinematic stereo | Script declarations identify speaker zoom constraints, separate scene/arms FOV and camera-cache ownership. Compose physical look in an upright frame to avoid orbit around a tilted authored camera. Count cinematic-owned PVR dispatches as activity. Resetting the separated character heading did not fix post-cutscene drift; saved head-based mode did. |
| 57 | Standing roll at steep upward/downward pitch stays stable; crouch and ordinary head movement remain correct | The near-vertical fallback used rolled right/up axes. Analytical upright pitch compensation removes the extra arc while preserving actual stereo eye-right. The modeled old arc was +/-20.633uu at85deg pitch and40deg roll. Existing capsule-height behavior remains a separate VR-87 limitation. |
| 58 | Mono screens anchor in front of the player, main menus retain mono and controller navigation, loading stays mono through Continue and then enters stereo, native takedowns retain animation, crossbow tracking survives cinematic level travel | A live pawn, tracked hands or background gameplay rendering cannot establish gameplay. A persistent movie-service pointer and overlay-enabled bit both falsely kept gameplay mono. The native manual-reset completion event used by Engine.WaitMovie gives a non-consuming completion observation. Verified closed UI, live pawn, fresh Walk state and scene uploads cover post-load presentation gaps without granting gameplay input. |

Crossbow follow-up: build258 isolated a real weapon-specific view-plane stretch
S=s(I-ff^T)+ff^T near1.046635; the same matrix explained translation to0.0014uu.
Remove only that verified lens before strict matching and the hand delta.
Build260 restored tracking but corrected numerical identity fits unnecessarily,
causing partial weapon surfaces. Build262 excluded identity fits within32 float
epsilons and confirmed sword/ordinary weapon stability. Its remaining zoomed
crossbow passes fitted slightly different ratios:6 of25 same-eye/Present pairs,
maximum2.38e-7. Build264 shares only numerical-equivalent inverse-lens values
for the same component, Present and eye; meaningful changes replace the value.
The headset confirms the final transparency fix. No stale hand-delta reuse,
broad draw suppression, relaxed identity bands or new engine-memory writer.

The aerial attack initially became an ordinary slash, not an incorrectly placed
finisher. Decompiled context declarations and the native trajectory probe led
to read-only DropWatch diagnostics, not altered combat eligibility. Later tests
confirmed native takedown animation and handback. Do not describe this as a
measured fix to the native eligibility decision.

## Promoted defaults

The maintainer explicitly requested the full current installed INI/F10 profile,
including diagnostics, as the new repository default. This supersedes the
historical default-off statements in the investigation records. Generated
WriteDefaultIni output, tests/golden/dishonored_vr.ini and
release/dishonored_vr.ini are byte-identical CRLF copies of that profile:
SHA256 364e79997823cd18e97b398a324edc20aaae9c222377e0c8d1f01db5dc060508.
All17 changed or added missing-key fallbacks also use the tested values.
Config version11 is unchanged; explicit settings in existing INIs win.

| Section | Keys promoted to1 | Live control |
|---|---|---|
| Cine | HeadLook, HideBorders, StereoState, LockFov, LockPitch, LockRoll, Trace | F10 View and existing cinematic commands; Trace is an INI diagnostic |
| Anim | CinematicHandBack, MantleHandBack, DropWatch | F10 native-animation controls; DropWatch is an INI diagnostic |
| Camera | HeadBasedMovement | F10 head-based movement; movement head/character |
| Neck | UprightPitchArc | F10 Comfort; neckupright on/off |
| Screen | AnchorMono | F10 mono anchoring, per-context exceptions and recenter |
| Menu | SurfaceGuard | F10 current-menu ownership control |
| Hands | AttachViewLens, AttachScaleTrace | F10 weapon-specific lens checkbox; AttachScaleTrace is an INI diagnostic |
| PosTrack | ZAccount | INI accounting diagnostic |

All ten existing Anchor<context> choices remain1. Screen width2.4m and distance
1.75m, render2750x2850, hand calibration, runtime choices and remaining saved
settings are preserved exactly. Screen.HeadLocked=1 remains the fallback for
contexts where anchoring is disabled; anchoring overrides it for opted-in mono.
These are the tested machine's preferences, not universal calibration claims.

## Validation and limits

Final host rerun: cinematic math/ownership and scoped writes,38 FOV/handback,
17 handoff,15 production-facing,12 standing-arc,31 mono/loading/completion and
30 stereo-policy checks pass. Frame/weapon/animation tests include seven new
lens consistency/isolation cases and identity roundoff regressions. The real
production default writer is executed in a standalone x86 host and compared
byte-for-byte with both repository profiles. Release build, exports, lint and
golden checks passed before merge. The agent never launches the game;
no additional headset test is invented for the defaults-only integration.

VR-104/105/106/107/108/110/112 acceptance is complete. Main-menu VR-74 and menu
input VR-71 are included in accepted UI work; no new two-minute stress result
is claimed. VR-111 no longer reproduced in subsequent accepted tests.
VR-50 remains open for the broader manual-FOV and kill-cam acceptance matrix;
its tested dialogue/cinematic and exit cases are fixed. VR-109 remains open for
character-oriented movement drift; default head-based mode avoids that path.
VR-99 brief crouched-note flicker recovers quickly but remains open. VR-87
capsule-height limitation and VR-102 initial weapon startup timing remain open.

Detailed chronology and measured failures remain in CINEMATIC_FOV_AND_HANDS.md,
STANDING_PITCH_ROLL_ARC.md, MONO_ANCHOR_UI_STATE.md, ENGINE_NOTES.md and
FLICKER_REFERENCE.md. Their old candidate/default/approval statements are dated
history; this acceptance record and STATUS.md describe the final state.

## Completed integration

All three PRs report MERGED on GitHub. The merged tree equals the validated
5aa625ae candidate exactly. Installed266-g5aa625ae, compiled11:25:16, has DLL
SHA256 257ab2551fadfec93d66a1e25067143c6643257709596f8303ecb5482015c7a4.
Full INI diff is empty; install archive ends20260914-112603-417138.
All10 accepted linked tickets are verified Done; VR-50 and VR-109 remain open
for the limits above. No release or tag was created. STATUS and NEXT_SESSION
record the final installation and remaining work.
