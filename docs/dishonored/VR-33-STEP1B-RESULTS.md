# VR-33 step 1b results: the skeleton, from the engine

Log output from a running build, 2026-09-07. Every number below was returned by
an engine call, not derived from geometry. No placement code exists.

---

## 1. The verdict

**`handAttachment_L_jnt` and `handAttachment_R_jnt` are BOTH descendants of
their respective hand joints.** The chains, as the engine reports them:

```
hand_L_jnt[54]           -> lower_arm_L_jnt -> upper_arm_L_jnt -> shoulder_L_jnt
                            -> Collarbone_L_Jnt -> Root_jnt -> root0_jnt -> ROOT
hand_R_jnt[25]           -> lower_arm_R_jnt -> upper_arm_R_jnt -> shoulder_R_jnt
                            -> Collarbone_R_Jnt -> Root_jnt -> root0_jnt -> ROOT
handAttachment_L_jnt[56] -> hand_L_jnt -> lower_arm_L_jnt -> ... -> ROOT
handAttachment_R_jnt[27] -> hand_R_jnt -> lower_arm_R_jnt -> ... -> ROOT
camera_jnt[8]            -> head_jnt -> neck_jnt -> spine_3_jnt -> spine_2_jnt
                            -> spine_1_jnt -> spine_0_jnt -> Root_jnt -> root0_jnt -> ROOT
```

So a pose applied at a hand joint has the weapon attachment beneath it. That is
the necessary condition for the native route, and it is now measured rather
than assumed. It is **not sufficient** - see section 4.

**`hand_R_jnt` exists.** It was an inferred spelling in the previous document
and is now confirmed by lookup, at index 25.

**The camera is on a different branch entirely.** `camera_jnt` descends through
`head_jnt` and the spine, not through either arm. The nearest common ancestor
of the camera and either hand is `Root_jnt`. Moving a hand cannot move the
camera, and the two arms only meet each other at `Root_jnt` as well - so a
per-side edit at or below a hand joint cannot disturb the other side or the
view. That answers a specific worry from the previous review about touching a
shared ancestor.

## 2. Skeleton indices are NOT palette indices, and now there is proof

`hand_L_jnt` is bone **54** and `handAttachment_L_jnt` is bone **56**. The arm
draw's GPU palette is **48 entries** (144 registers at c6, measured by the draw
census).

54 and 56 are both larger than 48. **The reference skeleton cannot be indexed by
the palette's index, and the palette cannot be indexed by the skeleton's.** The
previous review warned that a per-LOD palette map may reorder or subset the
skeleton; this is direct evidence that it does at least one of those, and it
retires any plan that assumed the two orderings agreed.

It also means every earlier statement of the form "bone N of the palette" is a
statement about a render slot and says nothing anatomical. That includes the
retracted shoulder-pivot reading, which is now doubly unfounded.

## 3. How each gate went

| Gate | Result |
|---|---|
| Class `Super` offset, derived not guessed | **+0x44**, by requiring `DishonoredPlayerPawn -> Pawn -> Actor -> Object` to resolve by name at one offset |
| Receiver ancestry | the pawn's Mesh is a `DishonoredPlayerSkeletalComponent`, which derives from `SkeletalMeshComponent` |
| Function resolution, on the declaring class | `GetNumElements`, `MatchRefBone`, `GetBoneName`, `GetParentBone` all resolved |
| Smoke call, no inputs | `GetNumElements() = 1`, frame written, guard depth back to 0 |
| FName round trip, both words | passed for every target; no chain was walked without it |
| Parent walk | terminated at `ROOT` for all five targets, no cycles, no truncation |

The receiver being a `DishonoredPlayerSkeletalComponent` rather than a plain
`SkeletalMeshComponent` is worth noting: the ancestry check was load-bearing,
not ceremony. A name-equality check would have refused a valid receiver.

## 4. What this does NOT establish

Unchanged from before, and none of it is weakened by the verdict:

* **Whether a later writer overwrites a pose we set.** Ancestry is necessary
  for inheritance, not proof that the attachment update or render-pose copy
  honours a change made earlier in the frame.
* **Where the attachment update sits** relative to skeleton composition. The
  tick group is shared; ordering within it is unmeasured.
* **Whether the equipped item's mesh is actually parented where the socket
  says.** The socket declares an attachment frame; the live item's component
  may be attached elsewhere. `DisTweaks_WepPistol.uc:28` defaults the pistol to
  `LeftHandWpn`, so handedness must still be read per item.
* **That editing a composed transform recomposes descendants.** It does not.
  Changing a wrist entry in an already-composed array leaves its children where
  they were; that is a separate mechanism to solve.
* **The cut boundary.** `MsLerpVertex` keeps a parent's blend indices and
  weights (`mesh_split.cpp:857-859`), so clipped and cap vertices can still
  carry forearm influences. A correct wrist-only edit can therefore still
  stretch the cap. Hiding the arm triangles did not remove those weights.
* **The palette mapping**, which section 2 has now shown to be a real and
  non-trivial problem rather than an identity.

## 5. Next, per the review's sequence

Step 1b.3 - the live equipment report: follow the local inventory to the
equipped item, its `m_pPlayerMesh`, and its ACTUAL attachment parent and bone,
rather than trusting the socket defaults. Then step 2a, tracing the native
update order around the tick group, with the named joints as the objects to
watch.

No placement build should be produced before 2a names the boundary and 2b
proves one deterministic translation and rotation on one named joint.

## 6. Reproduction

`[Hands] BoneQuery=1` in `dishonored_vr.ini`. The report runs once
automatically when a pawn is live, on the script lane, and every stage refuses
with its numbers rather than proceeding on a partial result. It ships default
OFF because it makes engine calls; the read-only socket report
(`[Hands] PoseReport`) is independent of it and stays on.
