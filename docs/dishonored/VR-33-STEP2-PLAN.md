# VR-33: step 1b.3 results, a shipped-code bug found on the way, and the step 2 plan

For review before step 2 is implemented. Sections 1-3 are log output from
running builds on 2026-09-07. Section 4 is a proposal. No placement code
exists; nothing writes to the skeleton.

---

## 1. Step 1b re-ran clean on the corrected probe

Every chain now terminates at the verified full-FName root sentinel, every
non-terminal parent is round-tripped, and each link carries its index:

```
hand_L_jnt[54] -> lower_arm_L_jnt[52] -> upper_arm_L_jnt[51] -> shoulder_L_jnt[50]
               -> Collarbone_L_Jnt[49] -> Root_jnt[1] -> root0_jnt[0] -> ROOT
               (7 validated links, ended: ROOT)
handAttachment_L_jnt[56] -> hand_L_jnt[54] -> ...            (8 validated, ROOT)
handAttachment_R_jnt[27] -> hand_R_jnt[25] -> ...            (8 validated, ROOT)
camera_jnt[8] -> head_jnt[7] -> neck_jnt[6] -> spine_3..0[5..2]
              -> Root_jnt[1] -> root0_jnt[0] -> ROOT         (9 validated, ROOT)
```

The verdicts now read from a validated edge on a walk that reached the root,
not from a chain length. Both attachment joints are descendants of their hands.

The full class chain also printed, confirming the previous document's
correction: `DishonoredPlayerPawn -> DishonoredPawn -> GamePawn -> Pawn ->
Actor -> Object`. Pawn/Actor/Object are landmarks with intermediates between
them, not three direct links.

Skeleton shape, for the record: `root0_jnt` 0, `Root_jnt` 1, spine 2-5, neck 6,
head 7, camera 8; right arm 20-27, left arm 49-56. Parent index is below child
index throughout.

## 2. Step 1b.3 returned two negatives, and they are informative

**No `DishonoredItemSkeletalComponent` carrying an `m_pItem` was found at
all.** The scan walked GObjects for that exact class and found no instance with
a non-null item back-pointer.

**The inventory ownership cross-check DISAGREED**: the pawn's `m_pInventory`
resolved to a live object, but that inventory's `m_pOwner` read as null.

Two readings, and this probe cannot yet separate them:

* Nothing was equipped through that component at the moment the report ran. It
  fires once, early, as soon as a pawn is live - which may be before the
  inventory has been populated. `m_pOwner` being null fits that reading.
* The runtime class or field is not what the corpus names.

The probe has been widened to list every component class with "Item" in its
name when the primary scan finds nothing, so the next run distinguishes "empty
inventory" from "wrong class name". It should also be re-run **after** an equip
rather than on the first live pawn; that is a change to when it samples, not to
what it reads.

**No conclusion about handedness or attachment is drawn from this run.** The
socket defaults still predict `LeftHandWpn` for the pistol; nothing has
confirmed or refuted where a live item actually hangs.

## 3. A shipped-code bug found while reading these logs

> **CORRECTED 2026-09-07 by the next run. The mechanism below is right and my
> explanation of it was wrong.**
>
> The declaration references **stream mask 0x3** - streams 0 and 1. It does NOT
> reference stream 2. So the stream-2 binding that vetoed the clip was
> **leftover state from an earlier draw**, not data this draw consumes, and the
> pass was clippable all along.
>
> "The same mesh is drawn by more than one pass and only some of them can be
> clipped" is therefore NOT what the evidence showed. It showed one pass being
> vetoed by a binding it never reads. The review's warning - that a bound
> stream is not necessarily a used stream, and that two log excerpts do not
> identify separate passes - was correct and my reading was not.
>
> With the veto restricted to streams the declaration actually names, both
> rebuilds in the next run produced 78 clipped triangles and the caps, and the
> tester confirmed caps present with the arms still hidden.
>
> Two things survive the correction. The decline-and-wait is still sound as
> candidate selection even if it was not what fixed this. And the **draw
> contract** check is the important half regardless: `MsDraw` was binding
> re-based indices to any draw with a matching primitive count, which is a real
> hazard whether or not it fired here. It logged **zero** mismatches in the
> confirming run, so it is protection rather than a fix for an active fault.
>
> One thing to watch: the declaration names **stream 1** and this draw does not
> bind it. If a pass ever does, the veto will fire legitimately and the caps
> will go with it - that would be the multi-pass situation I claimed to have
> found here, and it has not been observed yet.



The tester reported the wrist caps missing again. The logs name the cause, and
it is not in any of the new code.

```
build 1:  ms: stream 0 is the only stream, 32 bytes a vertex, 9 element(s)
          ms: ... 78 of them CUT by the plane ... 312 triangle(s) are the CAP
build 2:  ms: stream 2 is also bound ... the index list cannot be re-based
          ms: ... 0 of them CUT ... 0 triangle(s) are the CAP
```

Both builds locked the **same** vertex and index buffers (`vb=294EFFA0
ib=30085480`) with the same signature, twenty seconds apart. **The same mesh is
drawn by more than one pass, and only some of those passes bind stream 0
alone.** Owning stream 0 is what allows the index list to be re-based onto our
own buffer, which is what the clip needs, which is what produces the caps.

The auto-lock releases when the mesh stops being drawn and re-arms on whichever
pass draws it next. That is a coin toss, and losing it silently downgrades a
clipped, capped split to a whole-triangle one. It explains the caps vanishing
between runs with no configuration change - reported at least twice.

**Fix applied:** a draw that cannot be clipped is now DECLINED rather than
accepted, and the split waits for a pass it can own - which build 1 proves
exists. The decline is distinct from a refusal, so the lock is not burned. The
skips are bounded at 120; past that the degraded split is taken anyway with a
warning, because hands with a rough edge beat no hands.

This is worth flagging beyond the fix: **the draw census's own identification
was never pass-aware.** `draw_census.cpp:224-235` already recorded that
palette-gated identification missed passes on this mesh. Any future work that
identifies "the arm draw" - including the palette-mapping question - has to
carry which PASS it means, because the passes differ in ways that change what
is possible.

## 4. Proposed step 2a: the native update boundary

Read-only tracing, no writes. The targets are now named, so the trace can watch
specific objects instead of sampling everything.

1. **The three hand controls.** Read `m_pLookAtControl_LeftHand`, `_RightHand`
   and `_Camera` off the pawn (+0x8A8/8AC/8B0, resolved). For each: identity,
   whether any two are the same object, `ControlName`, `ControlStrength`, the
   `NextControl` chain, the control space fields, and any other-bone reference
   name. **Prove which bone each one actually targets** - the variable name is
   not that proof, and ENGINE_NOTES already records that the camera control
   aims the arms at the view and went untouched for a dozen builds.
2. **Order within the tick group.** Player and weapon defaults request
   attachment updates in `TG_PostUpdateWork`, and the item's post-update
   component shares that group. Measure the actual order of: control
   evaluation, skeleton composition, attachment update, item post-update,
   render-pose copy. A shared tick group is not an ordering.
3. **Identify a writer, not a getter.** Record thread, a monotonic sequence, the
   pose generation, object identity, and the transforms of `hand_*_jnt`,
   `handAttachment_*_jnt` and `camera_jnt` at each observation point.
   Native work may bypass ProcessEvent entirely, so any native observation
   point must be byte-verified against this executable and derived from it, not
   invented from a tick-group name.
4. **Account for our own calls.** The new depth guard excludes the bone queries
   from PeHandler's evidence, but `mat_hide.cpp` and `console.cpp` make their
   own ProcessEvent calls and are not covered. A trace is not purely
   game-generated until those are excluded too.

**Gate:** name the candidate insertion point, its input frame, the last
competing writer before it, whether descendants are recomposed after it, and
the attachment and render consumers that follow it. Mark any unresolved
interval explicitly rather than inferring it from adjacency.

### What step 2b will need, stated now so 2a can collect it

* Editing a composed transform does **not** recompose descendants. If the
  chosen boundary is after composition, the mechanism for updating the subtree
  has to be named, not assumed.
* The cut boundary is a separate acceptance. `MsLerpVertex` keeps a parent's
  blend indices and weights (`mesh_split.cpp:857-859`), so clipped and cap
  vertices can still carry forearm influences. A correct wrist-only edit can
  still stretch the cap, and that must not be "solved" by moving a shared
  ancestor - `Root_jnt` is the common ancestor of both arms and the camera.
* Skeleton indices are not palette indices (54 and 56 against a 48-slot
  palette), so any render-side claim needs its own correspondence work, tied to
  a specific pass.

## 5. Questions for the reviewer

1. Is the decline-and-wait fix in section 3 right, or should the split instead
   record which pass it built from and only re-arm on that same pass? The
   second is stricter but needs a stable pass identity, which the census does
   not currently carry.
2. For 1b.3, is re-sampling after an equip event the right trigger, and is
   there a reliable equip signal to hook that does not require decoding the
   native slot array?
3. Is there a safer way to establish tick-group ordering than instrumenting
   native call sites - something already observable from the script lane?
4. Anything in section 1 or 3 that the evidence does not support.

## 6. State

Branch `claude/vr-33-hands-and-weapons-at-the-controllers`. Installed build
carries the corrected bone-query probe, the widened item scan, and the
decline-and-wait fix for the caps. `[Hands] BoneQuery=1` on the dev rig;
`[Hands] PoseReport` read-only and on. No placement code.
