# VR-70: cinematic physical head tracking

Branch: codex/vr-70-cinematic-head-tracking, based on current VR-Main cccb1815
(PR #53). Ticket verified In Progress; VR-43 and open PR #12 are related.
No permission to merge this work.

## Completed before the first boat test

- Reconciled Linear: VR-96 already Done with High priority retained; initial
  fault owner and nine refused/57 valid updates/11 clean pauses recorded.
- VR-98 remains Done; ten note cycles across reload, 0-16 ms, recorded.
- Created completed follow-ups VR-100 (startup mono gate) and VR-101 (reload
  right-eye R/0 repair), linked to merged PR #53. These fixes are not reopened.
- VR-102 tracks the still-open weapon startup freeze. CacheNameLookups stays off.
- Posted one batch project update. No release or milestone created.
- Read PR #12 body and history; no comments/reviews exist. Its presentation
  controls do not implement head tracking, and its later menus-on-panel
  commit is not part of this work.
- Derived native final camera cache fields independently through both getters.
  Added a read-only trace. No new camera writer or change of gameplay policy.

## Current test: identify which head components are lost on the boat

The agent builds/installs and arms [Cine] Trace=1. Install identity and full INI
diff are under ignored build/cinematic/latest-install.json and its archive.
Before interpreting a run, match the log banner and compile timestamp to that
manifest, then archive current and previous logs before another launch.

One question: during the opening boat ride, which physical head movements
change the view: turning, leaning, both, or neither?

Launch normally and start the opening boat ride. With the right stick untouched,
slowly turn left/right, look up/down, then lean sideways and back. Finally,
keep the head still and turn briefly with the right stick as the comparison.
Quit normally when convenient. The agent reads the log; no tester commands.

- Both physical motions work: the reported lock was not reproduced; inspect
  the state boundaries before choosing an override.
- Turning works but lean fails: investigate position ownership only.
- Lean works but turning fails: investigate rotation ownership only.
- Neither works while the stick works: identify the authored cache/input
  path that bypasses the existing script writer.
- Head pose changes but no valid cache samples: repair observation before
  claiming a camera mechanism.
- Cache responds but view does not: downstream camera/presentation ownership
  remains unresolved; do not call cache stores honored.

## Implementation after the observation

Use the positive boat/scripted camera state and verified owner. Exclude pause,
notes, load and attract/menu scenes; runtime quad fallback is not that identity.
Compose physical head motion over the authored camera and preserve stick input,
authored pitch/roll/translation and cuts. A draw-scoped cache overlay is the
candidate; restore before the next authored update, use one coherent sample
for both eyes and preserve pose-record provenance. Validate current IsLiveObject
and retained identity on every engine writer, refreshing for new load objects.
Do not lift the direct controller fallback guard or modify pawn locomotion.

Ship the behavioral lever off with a live A/B. Test production composition
math/policy and identity/restore cases offline, then build/install and ask one
focused headset question. Normal gameplay and transition checks follow as
separate launches. Keep VR-70 In Progress until behavior is confirmed. Never
merge without explicit approval.
