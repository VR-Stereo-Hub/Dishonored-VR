# Three-way stability integration: review and first comparison (VR-69)

2026-09-10. Goal: stable world pose, stable weapon pose, and weapon attachment
that survives startup, reload and equipment swaps. The combined result still
requires a headset verdict. This review starts with the supplied LOCKON_PLAN,
not the earlier writer/eye reconstruction proposals.

Update 2026-09-10: the `b3a1ff46` comparison is headset-confirmed for head-turn
stability and weapon lock through equipment swaps. Startup settling remains
brief and is accepted for now. The remaining downward-character-motion flicker,
the exact writer interaction reproduced on the host, and the isolated next
candidate are in [DOWNWARD_CLAMP_REVIEW.md](DOWNWARD_CLAMP_REVIEW.md).

Final update: the `417bfad9` downward-clamp candidate is also headset-confirmed.
The complete session and reticle-aim configuration reset are recorded in
[SESSION_HANDOFF_2026-09-10.md](SESSION_HANDOFF_2026-09-10.md).

## Provenance, verified against GitHub and local history

| Piece | PR / merge | Relevant detail |
|---|---|---|
| World pose history | [#27](https://github.com/VR-Stereo-Hub/Dishonored-VR/pull/27), `8edb4d5b` | `[Pace] Lag=2`; the selector already exists at the earlier comparison bases |
| Weapon head history | [#31](https://github.com/VR-Stereo-Hub/Dishonored-VR/pull/31), `ff55f9d9` | `[Hands] PoseLag=2`; clean port `22f275ba` excludes the automatic A/B |
| Candidate recovery and ownership | [#26](https://github.com/VR-Stereo-Hub/Dishonored-VR/pull/26), `9fe2af45` | Load/swap recollection, contract retirement, and eye-decision changes share this merge |
| Keep stowed weapon contracts | **Also #27**, commit `fc343404` | A missing candidate is not necessarily a dead component; preserve the contract while stowed |
| Contract-key diagnosis | Also #27, `d36be0a0` | Reports why an existing weapon contract missed |
| Resolution authority | [#29](https://github.com/VR-Stereo-Hub/Dishonored-VR/pull/29), `0f8f9774` | Ini/launch-file precedence changed; actual render dimensions must match between candidates |
| Older-base proposal | [#32](https://github.com/VR-Stereo-Hub/Dishonored-VR/pull/32) | CLOSED, confirmed; no merge occurred |

The fourth row is missing from the supplied plan. `weapon_attach.cpp` changes
by 111 lines in #27. `fc343404` combines pose transport and contract retention
in one commit, so separation cannot end at #26. A clean #26-based candidate
would not by itself contain the complete swap-retention behavior.

## First comparison

### Result: head-turn judder fixed, stereo weapon flicker persists

The first candidate was tested on 2026-09-10. World and weapon judder during
head turns remained corrected. Weapons did not visually settle: a brief displaced
image appeared outward in each eye, sometimes becoming a displacement lasting
several frames with an apparent size change. These are observations, not proof
of a compositor-generated duplicate or a change to model scale.

The preserved log confirms the first candidate's build ID. Its final eye summary
has L=67,510, R=67,550 and unknown=8,538 placement evaluations, with 13,502 readable
eye transitions, nine small steps and zero large-step ambiguities. The contract
summaries retain the same three adopted weapons, continue placing them, and
report zero contract refusals in those summaries. Thus visually failing to settle
is not evidence that candidate collection repeatedly lost the weapon in this run.

With eye offset enabled, an unknown eye omits its half-IPD correction. Omitting
the right-eye subtraction moves its target right; omitting the left-eye addition
moves its target left. This fits the reported direction and can change disparity.
It does not establish event-by-event visual correlation from aggregate counts.

The live game-thread mono guard can reset both the render-side eye state and
its previous sample without incrementing the large-step ambiguity counter.
`PaletteEyeAlternate=0` does not disable it. A host test of the production
function reproduces that loss when old stereo draws run after a new script-side
single-draw decision: five assertions fail before the restore, all nine pass
afterwards. The other 88 desk cases continue to pass.

Preserved run: `build/vr69-bisect/first-result/dishonored_vr.log`, SHA-256
`DE905DAAD6EF439906FDA78B621646607A9538F142FFEE0F87CEEB050B6D873E`.

### Second comparison: restore the previously working eye decision

The first candidate is now pinned as diagnostic commit `08cbb368`; its source
is the same four-file patch used for the earlier dirty build. Commit `b3a1ff46`
then restores `MpEyeForPresent` and `MpWorldTarget` exactly to their `b38519c3`
function bodies, retaining the clean pose-history consumer in `MpDriveTick`.
Removed experimental code is retained under `src/legacy/vr69` and is not selected
by the active path. The empty pass-audit log is removed too. Its zero agreement
count never established correctness because it had no comparable draw population.

This is the planned controlled restoration, not a new eye heuristic. The first
candidate's runtime, capture, camera writer, candidate collection and contract
lifecycle files are unchanged. In particular, it does not yet add #27's separate
stowed-contract improvement. Retired PaletteEyeAlternate/PaletteEyeFromPass
settings no longer select the active eye path; PaletteEyeOffset remains enabled.

Second build ID: `vr33-hands-working-59-gb3a1ff46`. Normal-build DLL SHA-256:
`C615EF48547E5A822499B819260913ABE3DA293A63BD04F19F70F943EB9FA4A0`.
The release build, 88 existing tests, nine eye regression cases and export check
passed. Installation and the next visual result are recorded in STATUS.

### Original first-candidate setup

The known comparison baseline is `b38519c3` plus the clean weapon-pose port
`22f275ba`. The latest available installed-run log reports
`vr33-hands-working-42-gb38519c3-dirty`, not a directly identifiable clean commit;
retain its DLL, ini and log hashes rather than assuming the dirty suffix names
an exact source tree.

The first candidate is exactly:

```
9fe2af45 + the changes from 22f275ba
```

The patch applied without conflicts in an isolated detached worktree at
`build/vr69-bisect/lock-merge`. It changes only the four files in the clean port:
config loading, the hand pose consumer, pose-history capture, and pose-history
state. No eye-decision changes, contract changes or additional observers are
added to this candidate.

Validation completed: release/x86 build, all 88 frame/weapon desk cases, nine
undecorated exports and lint passed. Installed build ID is
`vr33-hands-working-57-g9fe2af45-dirty`; the staged four-file patch explains the
dirty suffix. Only `d3d9.dll` was replaced. Its SHA-256 is
`B4A8BA2C79CFA8A68972907114DBD125A9BAACEBF3824A84ED686FA03A244DC4`.
The installed hash matches and the ini hash is unchanged. The game was not
launched; the headset verdict is pending. Previous DLL, configuration and logs
are preserved under `build/vr69-bisect/before-first-candidate`.

The main checkout is the integration branch
`claude/vr-69-combined-stability`, based on `ff55f9d9`. Its runtime code remains
unchanged until the comparison answers which changes to separate. VR-Main is
not modified or merged.

### Configuration is part of the comparison

The inspected installed ini has world lag 2, Stereo LagAB 0, eye offset 1,
PaletteEyeAlternate 0, PaletteEyeFromMeasured 0, and 2750x2850 with VirtualMode 1.
Hands PoseLag is absent, so the clean port's fallback of 2 applies. The first
candidate has no automatic weapon/world lag A/B. Preserve the ini and launch
file, resolution, refresh rate, SSW setting, scene and equipment between runs.
Do not compensate for a worse picture by changing settings mid-comparison.

The later mainline generated ini still writes world Lag=1, although its loader
fallback is 2; the later world-lag A/B loader also defaults on when its key is
absent. The inspected machine explicitly has Lag=2 and LagAB=0, so those defaults
are integration hazards, not evidence that they caused this particular run.
Correct them in the final integration, separately from the bisect candidate.

### Read the outcome without conflating symptoms

Record whether flicker is continuous, whether it settles, whether it returns
while holding the same weapon, and whether it returns only after a swap/load.
Record world and weapon judder separately. A short initial settling interval and
a permanent full-IPD jump are different observations.

A candidate that reproduces continuous flicker localizes a reproducing change
set to #26 plus its interaction with the common weapon-pose patch. It does not
prove a single commit or that later merges contribute nothing. A candidate that
is clean for constant flicker does not yet prove durable swap retention.

## Correct the bisect decision tree

The proposed earliest-merge-first test is useful, but it is not guaranteed to
identify four possible merge boundaries in two runs. If #26 is clean and #29 is
bad, #27 versus #29 remains unresolved. Also, applying the same weapon patch to
historical bases makes this a controlled comparison of normalized candidates,
not an untouched historical bisect. Confirm the upper endpoint under the same
settings and patch policy before drawing a binary conclusion.

After the first result:

- First candidate bad: compare a surgical restoration of the pre-#26 eye
  decision while keeping #26's candidate lifecycle unchanged. This is an
  experiment, not yet the final combined build.
- First candidate clean: next test `8edb4d5b` plus the same clean weapon patch.
  This tests #27 and also adds the missing stowed-contract retention. If that is
  bad, inspect both pose/submission and contract changes within #27.
- #27 clean: test `0f8f9774` plus the same weapon patch with matched actual
  render dimensions. Then compare the original #31 head-history implementation
  and diagnostics if necessary.
- Inconsistent repeats: stop binary elimination and repeat the endpoint/control
  in the same scene, with recorded settings and sufficient time for lock loss.

The common patch has to be checked at each base. Cherry-picking its hash is not
proof that a later preexisting implementation or A/B was removed correctly.

## Concrete eye-decision interaction to inspect after the first result

`da1d760d` adds a guard in render-side `MpEyeForPresent` that clears eye state
and history when the live game-side `g_sdDoublingNow` flag is false. A queued
stereo draw can outlive the game-side decision that produced it. Atomic access
prevents a torn flag; it does not associate that flag with the queued draw.

`PaletteEyeAlternate=0` does not disable this guard. This explains why that
negative test does not exclude the entire eye-change group. It is a source-level
candidate, not a demonstrated explanation of the visible regression.

If the first comparison implicates #26, restore the complete eye decision and
selection path from `b38519c3`, preserving the clean weapon-pose consumer in the
same file. Do not restore the entire file. Audit shared scene/state changes by
behavior rather than assuming additions cannot affect existing consumers.
Retain candidate ownership and eventual #27 stowed-contract retention separately.

## Final integration gate

The final branch must preserve all of these together:

- Lag 2 for world submission and lag 2 for weapon head normalization.
- No automatic pose-lag experiments unless explicitly requested.
- Candidate collection after load, partial collection, and weapon changes.
- Contracts invalidated on relevant lifetime changes, while live stowed weapon
  contracts survive a swap away and back.
- Correct weapon scale, depth, handedness, attachment and controller motion.
- Stable behavior while walking, turning, holding still, swapping, sheathing,
  reloading a save, and returning from menus.

Build and desk-test each candidate; verify x86 exports and record source/config
hashes. The simulator can check launch and numerical invariants but cannot
replace the perceptual result that separates these candidates. No new simulation
scenario or extra live observer is needed before this first historical comparison.
Do not declare the three-way objective complete from compilation or counters.

## Branch cleanup

`restore/pr25-known-good` is a local alias of `b38519c3`, which is reachable from
the current clean-base branch and mainline. It has no unique commit to preserve.

`claude/vr-69-weapon-eye-from-measured` has five commits beyond VR-Main and is
also present remotely. Its behavior is superseded, but those commits are not
ancestors of the current mainline. Preserve the findings before deleting its
last reference; superseded does not mean merged. Neither of the proposed branch
deletions was executed. The deletion question in the pasted plan was not an
instruction from the user to perform it.

Keep the clean-base, findings and parked investigation branches intact. No PR
was closed, reopened or merged by this work; #32 was already closed.
