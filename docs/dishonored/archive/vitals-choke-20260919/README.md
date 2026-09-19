# Deferred vitals/choke research snapshot (2026-09-19)

Historical research only. The user has withdrawn the health/mana split and choke
feature from the active delivery. These files are not instructions to resume them.
Current task: ../../CLAUDE_HANDOFF_BUILD486_BRANCH_SPLIT.md.

- HUD_ANCHORS.md: full ownership audit, failed attachment attempts, model-draw
  refusals and copy-invalidation finding. Later source reference: a1e15d41e.
- CHOKE_GESTURE.md: original gesture research and correction that holding RB did
  not prove an actual choke. The active pad binding still needs to be established.
- CODEX_PLAN_VITALS_CHOKE.md: original design and gated implementation order,
  retained for a future explicitly authorized implementation.
- choke-calibration-wip.patch: unbuilt, untested, incomplete draft against the
  choke/pad files at e30554204. It was never installed. The draft header's
  "host-tested" wording refers to the old implementation, NOT this draft.

Draft calibration ideas: three countdown/move/one-second-hold trials, yaw-only
shoulder coordinates, mean fitted point, enter radius=max(0.08,max deviation+0.05),
exit=enter+0.08, speed=max(0.4,0.6*slowest peak). Runtime binding selection and
calibration tests are unfinished. Auto output depends on binding atomics with no
completed producer. Do not apply or ship this patch as a finished feature.

Preserve measured failures:
- Run503 allocated vitals textures but did not log a successful model draw.
- Later sink copies erased vitals-copy validity; a1e15d41e addresses this statically.
- Both-eye draw proof is required before suppressing XR fallback.
- Magenta palm diagnostic and selector candidate505 passed host checks but were
  not validated in the headset. Game-space attach capture was not implemented.
- The right-palm logger needed a per-hand timer; a shared timer hid one side.
- Gesture held RB twice without StatePlayerMasterChoke. Binding set2 maps RB to
  attack; identify live GBA_Block routing before choosing output.
- Colour/tone-map correctness, native game-space capture, calibrated choke output
  and the ReduceDesktopPresent wheel comparison remain unproven.

Do not ship any of these features, tests or INI defaults in the build486 PR or
SteamVR-only branch. Keep this archive as documentation; code remains recoverable
from Git history and the local archive branch named in the handoff.