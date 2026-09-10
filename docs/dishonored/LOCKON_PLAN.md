# Three fixes that each worked, and have never run together (VR-69)

2026-09-10. **For review before any code is written.**

The objective is one build with all three of these true at once, and nothing else
new:

1. **No world jitter.**
2. **No weapon jitter.**
3. **The weapon flicker settles**: it flickers while the correction adopts the
   weapon at startup, locks on, and does not come unlocked again.

Every one of the three has been confirmed in a headset on its own. **No build has
ever had all three.** The reason is not a technical incompatibility between them.
It is that the commits carrying (3) and the commits carrying the current
unplayable regression landed in the same merge, and one commit does both jobs.

---

## 1. The three systems, and what each one actually is

### System A - the world (head-turn) jitter: `[Pace] Lag=2`

Merged as `8edb4d5b` (12 commits). The engine renders a frame from the head
**two locate generations back**, and the runtime layer was submitting the
freshest pose with it, so the compositor reprojected every frame from a head the
image was not rendered from.

* Lever: `[Pace] Lag` (`g_poseLag` in `core/vr/openxr_runtime.cpp`), selecting
  which generation of located views the submitted pose comes from.
* Evidence: a reversing A/B/A/B in a headset. Also the negative that named the
  right pair - the first instrument compared two values derived from one pose
  consume and could only ever print zero.
* Read at the submit, in `openxr_runtime.cpp`. Touches nothing in `hands/`.

### System B - the weapon jitter: `[Hands] PoseLag=2`

Merged as `ff55f9d9` (VR-68). `MpDriveTick` normalised the hand against the
**freshest** head while the view it is planted in was built from the head two
generations back, leaving two generations of head rotation in the frame.

* Lever: `[Hands] PoseLag` (`g_mpPoseLag`), selecting from a four-deep head
  history filled in `DvrConsumePoses`.
* Evidence: `bv/lag` measured the rendered camera moving with the head at lag 2 -
  mean 0.119 deg against 1.19 at lag 0 and 2.41 at lag 1 over 4085 moving frames,
  a clean V around the minimum. A headset A/B/A/B agreed.
* Touches `hands/mesh_split.cpp` (`MpDriveTick`), `present_tick.cpp`,
  `06_game_dishonored_head_track.inc`, `config.cpp`.
* **Already ported by hand onto the clean base** as `22f275ba`, without the
  0/2/0/2 A/B that shipped with the original. Confirmed in a headset on that
  base: the weapon jitter is gone.

### System C - the weapon lock: the candidate list and the contract

Merged inside `9fe2af45`. This is the group that has never been combined with a
clean picture. It is what stops the correction losing the weapon it had adopted:

| Commit | What it fixes |
|---|---|
| `1232a144` | a contract must not outlive the level it was matched in |
| `c96a9ad0` | rebuild the candidate list on a load; double on scene liveness |
| `bb264ced` | a partial candidate list must not stick; both startup gates default off |
| `a6e00a6f` | **the candidate list gets an owner** (and, in the same commit, an eye audit) |
| `e60b5df2` | refresh the candidate list on a weapon SWAP, not only when it empties |
| `f3a2256e` | an equipped item's own component outranks the name test |
| `cff0f1b2`, `12a880f7`, `6aa5f673` | the load-phase scoreboard, the screen-up query, a ghost menu flag |

The failure they address is recorded in ENGINE_NOTES under "NOBODY OWNS THE
CANDIDATE LIST - TWICE": the list goes EMPTY after a load and STALE after a
weapon swap, every rebuild trigger sits behind a one-shot call site that has
already fired, and the recovery path was a no-op that logged as though it had
acted.

Footprint: `hands/weapon_attach.cpp`, `hands/fp_mesh.cpp`, `startup.cpp`,
`ue3/reflect.cpp`, `ue3/ui_state.cpp`, `13_game_dishonored_game_state.inc`.

---

## 2. Why they have never run together

`9fe2af45` carries System C **and** three commits that change how the weapon's
per-eye offset is decided:

| Commit | What it changes |
|---|---|
| `26f05114` | take the weapon eye from the drawing pass instead of the inference |
| `11cef70f` | an unreadable eye step ALTERNATES instead of holding the previous answer |
| `da1d760d` | a mono frame has no eye, so nothing there may be alternated |

`VR-Main` at `ff55f9d9` is unplayable in a headset: every weapon flickers
constantly, and it never settles. `b38519c3`, the commit immediately before
`9fe2af45`, is confirmed clean on that axis - the weapon flickers while it adopts
and then stops - but it lacks System C, so it comes unlocked again and again.

So the two behaviours the tester wants are on opposite sides of one merge.

### The separation is not clean

* `a6e00a6f` does **both** jobs in one commit: it gives the candidate list an
  owner AND adds the eye audit.
* `26f05114` and `da1d760d` touch `scene_draw.cpp` and
  `13_game_dishonored_game_state.inc`, which System C also touches.
* Cherry-picking System C while skipping the eye commits was tried on 2026-09-09:
  **five of nine conflicted**, because they are a contiguous sequence and removing
  the commits between them breaks the context.

`mesh_split.cpp` is the eye. `weapon_attach.cpp` and `fp_mesh.cpp` are the lock.
The shared state file and `a6e00a6f` are the tangle.

---

## 3. What is established, with its population

**Do not re-derive any of this.**

| Fact | Evidence |
|---|---|
| `b38519c3` has no constant flicker | headset, 2026-09-09 |
| `b38519c3` loses the weapon lock repeatedly | headset, same session |
| `b38519c3` + System B has no jitter and no constant flicker | headset, build `22f275ba` |
| `VR-Main` (`ff55f9d9`) flickers constantly | headset, unplayable |
| The regression is in one of four merges | `9fe2af45`, `8edb4d5b`, `0f8f9774`, `ff55f9d9`. **Not bisected.** |
| The resolution survives the older base | it reads `dishonored_vr_launch.txt`, `[Screen] RenderWidth/Height` and `VirtualMode`, all defaulting to the wanted values |
| `[Pace] Lag` is read at the older base | System A applies there |

### An observation that points at the eye group and is not proof

`[Hands] PaletteEyeOffset=1` has been in the installed ini since 2026-09-07.

* At `b38519c3` with that key at 1: weapons the right size, no constant flicker.
* At `VR-Main` with that key at 1: constant flicker.
* At `VR-Main` with that key at 0: no constant flicker, but the weapons read as
  enormous, because with no per-eye offset both eyes are handed the same weapon
  position and the disparity that should say 40 cm says much closer. **That is an
  eye misalignment, not a scale fault**, and the tester named it correctly.

The same key, the same value, opposite outcomes across the merge. That is
consistent with the eye group being the regression. It does not prove it: three
other merges sit in between and none has been excluded.

### What is already excluded, by measurement

Twelve approaches were closed on 2026-09-09, each by a counter that printed its
own population (`docs/TRAPS.md`, `docs/dishonored/ENGINE_NOTES.md`). The ones
that matter here:

* The submitted pair's geometry is exact: separation `min == max == mean =
  0.0631 m` over roughly 5,700 pairs in 25 windows, zero side flips, zero
  generation splits.
* The eye labelling is exact: `DISAGREE=0` over roughly 11,000 verdicts in 24 of
  25 windows.
* Pass 1's camera always held the left eye: `MOVED=0`, `same == population`,
  worst move `0.00 uu`.
* `PaletteEyeAlternate=0` and `PaletteEyeFromMeasured=0` both resolved correctly
  in the log **and the constant flicker continued.** So the alternate-versus-hold
  branch is NOT sufficient to explain it on a `VR-Main`-derived build.

That last row matters for this plan: it means reverting `11cef70f` alone will not
be enough, and it weakens the assumption that the eye group is the whole story.

---

## 4. The plan

### Phase 1 - bisect, two builds, no code

The regression has a known-good end and a known-bad end with four merges between.
Two headset runs settle which merge introduced it. Every build in this phase
carries System B ported on top, so the tester is never asked to judge a build with
the weapon jitter present.

| Build | Base | Question it answers |
|---|---|---|
| 1 | `9fe2af45` + System B | Is the regression in the merge that carries System C? |
| 2 | `0f8f9774` + System B, only if build 1 is clean | Is it VR-65 or VR-66? |

Build 1 is the high-value one and it is also useful whatever the answer:

* **If it flickers constantly**, the regression is inside `9fe2af45` and Phase 2
  applies. Three more commits to separate, at most two further builds.
* **If it does not**, System C is free: that build already has all three
  properties the tester asked for, and the regression is in VR-65, VR-66 or
  VR-68, which are small and individually revertible.

The instruction to the tester is one sentence per build: does the weapon settle
and stay settled, and is there any jitter.

### Phase 2 - separate the eye from the lock, only if Phase 1 says to

If `9fe2af45` is the culprit, the separation is by FILE, not by commit, because
the commits do not separate:

1. Take `9fe2af45` whole (System C intact, no conflicts).
2. Restore `hands/mesh_split.cpp`'s eye decision to its `b38519c3` form -
   specifically the function that decides the per-eye offset for the weapon
   correction, not the whole file, since `MpDriveTick` in the same file carries
   System B.
3. Leave `scene_draw.cpp` and the shared state file as `9fe2af45` has them; they
   are the lock group's context and the eye commits only add to them.

This is a surgical restore of one decision, reviewable as a diff against both
parents, and it does not ask the eye commits to revert cleanly - which they do
not.

### Phase 3 - land it

`VR-Main` currently contains the regression, so the branch cannot simply be
merged forward: it has to carry the correction. PR #32 exists and shows the shape
of the problem (returning to the older base removes 6,533 lines across 50 files);
it is open for review and should be closed in favour of whatever Phase 1 and 2
produce, because a rollback is a worse outcome than a targeted fix.

---

## 5. What would falsify this plan

* **Build 1 flickers constantly AND the eye restore in Phase 2 does not fix it.**
  Then the regression is not the eye decision, and the `PaletteEyeAlternate=0`
  result above already hints at that. The next candidate would be `a6e00a6f`'s
  audit or `12a880f7`'s screen-up query, both of which change when the correction
  runs at all.
* **Build 1 is clean but the weapon still comes unlocked.** Then System C is not
  sufficient either, and the lock failure is something the candidate-list work
  never addressed.
* **The two properties turn out to be one quantity.** If a missing per-eye offset
  reads as apparent size, an offset that is momentarily wrong reads as a jump, and
  an adoption that loses its contract also reads as a jump. It is worth checking
  whether "comes unlocked" and "flickers" are the same event at different rates
  before assuming two systems.

## 6. The rule this plan exists to honour

The 2026-09-09 session spent three headset runs and twelve instrumented
hypotheses on what turned out to be a regression with a known-good commit. Every
instrument measured clean, because the bookkeeping was never wrong.

**Ask when it was last good before building an instrument.** A bisect over four
merges is two runs. It was available the whole time.
