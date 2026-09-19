# Next session (after Codex implements docs/dishonored/CODEX_PLAN_VITALS_CHOKE.md)

## Start here
- C:/dev/Dishonored-VR, branch `codex/hud-improvements` (draft PR #76, stacked on
  #75 = `codex/misc-fixes`, open, NOT merged). Never merge.
- Read CLAUDE.md, AGENTS.md, the top 3 sections of docs/STATUS.md, this file, then
  `docs/dishonored/CODEX_PLAN_VITALS_CHOKE.md` and `git log --oneline -15` to see
  what Codex actually landed against that plan.
- The tester runs the game; you never launch it or the simulator, never use
  subagents. Build Release and freeze each candidate with an install.ps1 (pattern:
  build/playtest-candidates/hud-improvements/install-503/install.ps1, which has a
  `-TestIni` dry run). The tester is usually PLAYING while you work: do not install
  over a running game; hand over the one-line install command. Archive both logs
  before reading them (the live log may already be a NEW launch; the run you want
  is then in the .prev log - check both banners).
- Diff the whole ini on every install, keep CRLF, one question per launch with
  expected outcomes. Commits under the configured identity, no trailers, no names.

## State at handoff (2026-09-19)
- Last installed: candidate 503 (`vr33-hands-working-503-g7159acacf`); its run is
  archived in build/playtest-candidates/hud-improvements/run503.
- Accepted: wheel blackout fix (VR-140), reticle look (VR-141, white 0.69 deg),
  split health/mana (VR-142 base), rain lens defaults (distance 18, follow on).
- Failed in the headset: every attempt to keep the vitals ON the hand model
  (controller-relative attach, animation offset, drawn-palm XR quad, in-scene
  draw - the last never drew: see the plan's section 0); the physical choke
  (fired RB, the game never choked).
- Open tickets: VR-142, VR-145, VR-143 (stall on the first stand-up after
  loading a crouched save), VR-144 (wheel stutter, ReduceDesktopPresent A/B).

## First job
Verify Codex's implementation against the plan, section by section: build it,
read the diff, run the host tests (`tools/hud-anchor-host.ps1`,
`tools/choke-gesture-host.ps1`, `tools/default-profile-host.ps1`, lint). Then
freeze the plan's first candidate (settings selector + model-mode diagnostics,
magenta square) and hand the tester one question.
