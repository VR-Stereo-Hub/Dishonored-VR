## Current continuation: F10 art refinement (VR-206, 2026-09-22)

Use the newest docs/STATUS.md entries. Branch f10-improvements / draft PR #109 sits on
finalized, unmerged rain-fixes / PR #108. Revised menu art/layout is prepared for review.
Build733 installed with explicit approval. No game launch; all ini settings preserved,
with existing mixed line endings normalized to CRLF. See STATUS for the archive and test.

Desktop rendering, layout bounds and native widget event checks cover normal, small,
large-text, tooltip and scrolled views. Headset/controller acceptance remains open.
The optional outdoor-rain-from-shelter feature is deferred as VR-205, separate from these
UI changes. Do not resume older menu freshness work from the historical notes below.

## Current continuation: menu freshness (2026-09-22)

Use the top entry in docs/STATUS.md and FLICKER_REFERENCE.md for VR-178.
Combined PR build650 accepted except menu cadence/mono and left-hand stepping.
The separately gated menu freshness candidate needs its headset verdict before merge.
Older handoff entries below are historical; do not roll back this combined build.

## Selective cleanup for SteamVR continuation (2026-09-19)

Latest user instruction supersedes the exact486-only handoff: remove ONLY failed
physical choke and split/attached/model health-mana experiments. Keep possession,
rain/lens, wheel blackout, crash/stability, weapon models/animations, reticle UI
and defaults, and SteamVR diagnostics. Original combined vitals remains.

Production code is c01058558 plus305f1d3dc reticle defaults and the two source
diffs from e30554204 SteamVR diagnostics. Release profile preserves all unrelated
current settings; only Choke section and VitalsMode/VitalsDebug are removed.
Research/code remains in codex/archive-hud-choke-20260919 and tracked archive.
Do not restore or revive the experiments. No branch history rewritten.

Installed vr33-hands-working-512-g3f4d323e0 from clean source commit 3f4d323e0.
Release build, lint, 908 HUD checks and 9 export checks passed. Installed DLL
and INI hashes independently match installed.json; the entire accepted486 INI
is byte-identical, with CRLF and explicit VDXR selection preserved. Prior DLL,
INI and both logs archived under build/playtest-candidates/installs/
20260919-125236-071385. Build512 has not been headset-tested.
SteamVR inversion remains open; retained diagnostics do not claim it fixed.
No game or simulator launch, no PR/merge. Continue on codex/hud-improvements.

Next: verify current build/playtest-candidates/installed.json and log banner.
Continue native SteamVR investigation from e30554204. Do not use the old
exact486-only branch-split instructions: reticle work is explicitly retained.
Do not change runtime or revive whole-image flips without a targeted test.
Read only current STATUS and relevant ENGINE_NOTES/FLICKER_REFERENCE entries.
The retained SteamVR audit compares head/eye orientations at one predicted time.
All usual log archive, full INI comparison, CRLF, liveness and no-launch rules apply.
