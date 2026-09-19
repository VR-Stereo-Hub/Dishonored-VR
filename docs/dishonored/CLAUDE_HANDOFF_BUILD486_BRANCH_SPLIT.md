# Claude handoff: accepted build486 PR, SteamVR branch, deferred HUD/choke

## Current instruction and boundaries

The user now considers the restored build working. Prepare its PR, separate only
the SteamVR work onto a new branch, and discard health/mana splitting and physical
choke from active delivery while preserving their research for later. This
supersedes CODEX_PLAN_VITALS_CHOKE.md and the old NEXT_SESSION instructions.
Do not implement vitals/choke now. Do not continue performance tuning by default.
No game/simulator launch, no merge, no deletion of finalized branches.
No subagents. Read CLAUDE.md and AGENTS.md; current user instructions win.

This handoff prepares the work; PR/branch reorganization has NOT yet been performed.
Current branch is codex/hud-improvements, containing all later experiments and
handoff documentation. It is NOT the source to use for the accepted-build PR.

## Exact accepted state

- Source commit: 3ec56e3bc8ce724270f6720ca4b1ecfe25cc1769.
- Banner: vr33-hands-working-486-g3ec56e3bc.
- Installed rebuilt Release DLL SHA256:
  34bc3cb31eb540190a4c0dd2291ac47a0be7a325e9b871622913cabd50969c3d.
- Frozen local candidate: build/playtest-candidates/hud-improvements/rollback-486.
  Its install.ps1 is copied from install-503, dry-run, hash-checked, and archives
  logs/INI. baseline.ini is the archived original install486 configuration.
- Installed settings: the whole archived486 INI, except Runtime=native and
  XrRuntimeJson=C:\Program Files\Virtual Desktop Streamer\OpenXR\virtualdesktop-openxr-32.json.
  This is the local VDXR selection, not a path to bake into public defaults.
- Do not substitute489 (c01058558, reticle customization) or a later candidate.
  489 was the immediate pre-split source but486 is the accepted/tested baseline.
- Detached local build checkout: build/pre-split-source, currently3ec56e3bc.
  Dependencies were copied locally; do not commit those copies.
- Original historical486 DLL hash was58078be3...; the installed DLL is a rebuild,
  not byte-identical to that historical artifact. Describe it accurately.
- The user reported near prior performance after reboot plus a known-good save,
  then accepted the state. Neither reboot nor save was independently isolated.
  Render3012x3122, recommended2688x2880 and120Hz match original accepted486.
  The remembered VD102/112 percent difference is not a proven resolution change.
  PERFORMANCE.md is the single performance record. No confirmed SteamVR fix.

## 1. Preserve research before changing any active branches

Local archive branch codex/archive-hud-choke-20260919 retains this handoff plus
all later commits. Verify it exists and contains this file before any cleanup.
Tracked snapshot: docs/dishonored/archive/vitals-choke-20260919/.
Read its README first: the three research documents are historical; the WIP patch
was never built/installed and has no completed binding producer or new tests.
Original code is also retained in Git history:
- 91b18ebb5 split health/mana (do not deliver).
- 3dc587932 back-of-hand experiment;7c1fc3370 attach countdown.
- 3a3643256 animation attachment;1cd485a08 drawn-palm XR attachment.
- be540542e original physical choke;7159acacf in-scene vitals.
- a1e15d41e selector, copy fix, model diagnostics and profile changes.
- e30554204 SteamVR-only source changes mixed with broad session documentation.

Carry the tracked research archive and this handoff as DOCS ONLY onto the
accepted PR branch (or a dedicated published research branch linked from that PR).
Do not rely solely on ignored build folders for preservation. Publish the
archive ref or its documented research copy before closing the mixed PR.
Never commit game logs, dumps, decompiled scripts, copied dependencies or binaries.
Do not delete the local archive branch or force-rewrite history to hide experiments.

## 2. Prepare the accepted-build PR

Existing PR75:
https://github.com/VR-Stereo-Hub/Dishonored-VR/pull/75
head codex/misc-fixes, base VR-Main.
Its current local tip674d693a8 differs from3ec56e3bc ONLY in:
docs/STATUS.md and docs/dishonored/WEAPON_MIRROR_PLAN.md.
Production source, tools, profiles and tests already match the accepted build.
Prefer updating PR75, not opening a duplicate for identical code.

Refresh remote refs and verify this remains true before acting. Do not include
changes from current codex/hud-improvements wholesale. Keep production code at
the accepted source; only add the research/handoff/current-state docs needed.
Review the complete PR diff, identify its existing tickets, and rewrite the
description around its actual scope and accepted486 evidence. Do not claim later
reticle UI, HUD splitting, attached/model vitals, choke or SteamVR are included.
Mention the rebuilt binary identity and the remaining performance uncertainty
without turning the PR into an unproven performance fix. Update current STATUS
on this branch to accepted486 and deferred features, avoiding stale505/506 plans.
Preserve appropriate Linear references already used by PR75; verify before adding
ticket IDs. Push and update the PR as requested. Do not merge.

Validate Release and lint. Run applicable baseline host tests. The choke host
script does not exist at486; do not add it simply to satisfy the superseded plan.
If historical default-profile checks fail, report the baseline issue separately;
do not silently change the accepted implementation or settings to make it pass.
No reinstall is needed for docs/PR organization; keep the accepted DLL/INI intact.

## 3. Extract SteamVR only

Use a new branch off the accepted PR75 head:
codex/vr-146-native-steamvr
Verify VR-146 before use; it was created for native SteamVR inverted orientation.
If it already exists, inspect it rather than overwrite it. Until PR75 is merged,
base a SteamVR draft PR on codex/misc-fixes so its diff shows ONLY SteamVR work.

Port only the two source-file diffs from e30554204:
src/core/vr/openxr_runtime.cpp and src/tools/ovrshim/ovrshim.h.
Do not blindly cherry-pick its STATUS/ENGINE_NOTES changes; curate the SteamVR
findings separately. The changed source parent included HUD experiments, so
review/apply the patch against486; no HUD helper or vitals dependency may sneak in.
Review the bounded audit itself (raw head/eye quaternion norms, validity, roll and
angular disagreement at the same locate time) before retaining it.

Preserve these conclusions:
- Native SteamVR/OpenXR2.17.10 successfully started the x86 game without the shim.
- Forced Meta compatibility2->0 changed the runtime name but did not fix inversion.
- Reported surface: world and menus inverted, hands upright in gameplay.
- Old log head roll near179deg lacked reliable correlation with headset wear.
- Candidate506 added diagnostics only; it did not fix orientation and was rolled
  back before its requested test was completed.
- An image Y-flip experiment was built/host-tested but NEVER installed; source was
  removed after the upright-hands clarification. It is NOT part of the extraction.
- Local scratch build/steamvr-image-flip-uninstalled is historical, not approved
  implementation. Do not ship or revive it without evidence.
- SteamVR settings remain compatibility disabled; installed game uses explicit
  VDXR. Do not switch the user's runtime or install the SteamVR branch now.

Update runtime documentation honestly:2.17+ native32 support; shim is fallback for
older/unavailable runtimes. Build/test the isolated branch, publish it as unfinished
diagnostic work if making a PR, and list next evidence needed. Separate future
SteamVR implementation from branch extraction. Never claim upside-down fixed.
Attach any PR created/updated to the task when the client supports that.

## 4. Retire the mixed delivery after preservation

Existing PR76:
https://github.com/VR-Stereo-Hub/Dishonored-VR/pull/76
head codex/hud-improvements, base codex/misc-fixes.
After verifying accepted75 and the research preservation, close76 unmerged with a
short factual explanation and links to the preserved research and SteamVR branch.
The user authorized discarding these features from active delivery, not erasing
history. Keep recovery refs. Do not delete finalized branches.
Mark VR-142 and VR-145 as deferred/backlog, not completed; link preserved research.
Keep unrelated VR-143/VR-144 findings open with their evidence in PERFORMANCE.md.
Do not smuggle reticle customization from c01058558 into the exact486 PR.
Record it as excluded/recoverable if needed; it can be selected separately later.

## Completion report

Return the accepted-build PR link, SteamVR branch/PR link, archived research
location, disposition of76 and deferred tickets, validation results, and explicit
confirmation that the installed486 build/INI remained untouched and nothing merged.
Commits use configured identity; no trailers/generated attribution. No names in
branch names. No extra user confirmation for these already authorized branch/PR
operations unless the environment's approval review specifically blocks them.
A prior push in this task was blocked before the user's present PR request; no
push was performed then. Do not bypass any new approval rejection.