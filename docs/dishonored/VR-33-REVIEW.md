# VR-33 review brief: putting the hands and weapons on the VR controllers

**Status: four approaches attempted, none shipped. Reverted to the branch's
starting tree. Seeking review before a fifth.**

Prepared 2026-09-06 for external review. Everything below is either measured
off a running build or explicitly flagged as an assumption. The reviewer's job
is to attack the plan in section 6 before it costs another headset run - and to
say plainly if section 5's conclusion is wrong.

---

## 1. The goal, and the one constraint that shapes it

Corvo's hands and weapon should sit where the player's VR controllers are, and
turn as the controllers turn.

The constraint that killed three of the four attempts: **Arkane draws the
first-person view model in camera space.** There is no world matrix for the
arms anywhere in the constant map, so the rig's placement is defined by the
camera. This is not new - it is recorded in `docs/dishonored/ENGINE_NOTES.md`
under "Head coupling of the arms", derived by the mod's original author across
six attempts of their own.

## 2. What already works, and is not in question

These shipped and were confirmed in the headset. They are the foundation
anything new builds on, and the revert preserved all of them.

* **VR-30, head/body decoupling.** `FaceRotation` is intercepted so head yaw no
  longer turns the pawn. Merged in PR #19.
* **VR-31, the arm/hand split.** The arms and hands are ONE skinned triangle
  list with one material - no per-bone visibility flag and no material section
  separates them, each ruled out on its own instrument. So the geometry is cut
  by us: classified per triangle by BONE INFLUENCE, cut at a plane
  perpendicular to the forearm, with straddling triangles CLIPPED at that plane
  and the open end CAPPED. Ring at `[Hands] WristCutA/B = -4.9`, measured.
  PR #19. Full reference: `docs/dishonored/ARM_HAND_SPLIT.md`.
* **VR-53 / VR-51**, the desktop mirror eye pin and the pause-menu session
  loss. PR #20. Reference: `docs/dishonored/DESKTOP_MIRROR.md`.

**The split matters to this ticket for a reason beyond the cut.** Blend indices
in the vertex data name the bone palette directly, so the split's
classification gives palette slots by construction: `g_msHandBone[side]`,
`g_msBoneSide[]`, `g_msBoneHand[]`. Any approach that needs to know "which
bones are the hand" already has the answer without a name lookup.

## 3. What was measured this session

Numbers, from `dishonored_vr.log` on the dev rig (Quest 3 over Virtual Desktop,
VDXR, 90 Hz).

| Fact | Value | How |
|---|---|---|
| Arm mesh signature | 4448 prims, 2771 verts, 48 bones | draw census auto-arm |
| Bone palette | `c6`, 3 float4 per bone, **x144** for the arms | constant hook |
| Palette space | **camera-relative** | c5 moved thousands of units while bone origins held within ~1 uu |
| Palette axis order | **(X right, Y up, Z forward)** | in a menu the two hand bones are exactly symmetric at (+-31.07, 8.99, -12.50), so component 0 separates the hands |
| Palette scale | **~210 uu/m**, not the world's 108 | at 108 a resting hand would be 0.6 m right / 0.8 m down / 0.7 m forward |
| Vertex declaration | 32 B, 9 elements, stream 0 only; TEXCOORD0 is FLOAT16_2 | decl dump |
| `g_msHandBone` position | **66 uu (L) / 108 uu (R) from the hand bones** | probe |
| "Hand" bone cluster | 19 bones, spread **176 / 215 uu** | probe |

**The last two rows are the important ones and they were a surprise.** The bone
the split calls the hand bone sits a third to half a metre from the bones that
actually carry hand geometry, and the set of bones it calls "hand" spans nearly
a metre. Both are consequences of `MsClassify` computing `g_msBoneHand[]` from
the SPHERE test - the fallback shape the plane cut replaced - rather than from
the plane. **The tester independently guessed "the pivot is at the shoulder"
from the feel of it, and the numbers agree.**

That is a live defect in the split's bone classification. It does not affect
the cut, because the cut is done per triangle against the plane, but it makes
those two arrays unsafe for anyone who assumes they mean what they are named.

## 4. The four attempts, and why each failed

### 4.1 SkelControl, engine-side (`SkelControlSingleBone`)

Write the controller pose into `BoneTranslation` / `BoneRotation` on the
engine's own per-hand skeletal controls.

**Outcome: position works, rotation does not.** This reproduces exactly what
ENGINE_NOTES already recorded from the original author's build 38.x - writes
into `BoneRotation` lose to the engine's recompute, measured there at 9,000
writes a second. Confirmed again in the headset on 2026-09-06.

Position that did work was still head-coupled: the hands traced an arc on a
horizontal head turn, from a residual between the yaw this mod injects into the
game camera and the basis that camera reports back. Not chased further.

**Two errors of mine on this lane, recorded because they were process errors,
not just bugs:**

1. A grip-pivot correction was written and shipped before checking that the
   drive ran at all. It never executed once - `[Mode] GamepadOnly=1` vetoes the
   entire hands subsystem - and its own instrument said so (`0 write(s) carried
   the pivot offset, 0 could not`). A whole headset run was spent reading a
   "pivot fault" that had never had the chance to be wrong.
2. A "fix" was then written for the head coupling based on a claim that the
   offset was a raw tracking-space delta from a stale capture. **That claim was
   false** - `ph` was already live-head-relative and yaw-de-rotated, twenty
   lines above where I was reading. The "fix" recomputed what already existed,
   in the wrong axes, and broke what was working. Reverted.

### 4.2 Moving vertices in our own buffer

The split already owns a validated copy of the vertex data, so transform the
hand vertices directly.

**Outcome: rejected before implementation, and it is worth stating why**,
because it is the intuitive approach and it cannot work:

* Our copy is the BIND pose. The animation happens in the vertex shader from
  the bone palette, so anything written in bind space is skinned AGAIN on top.
* Even if it rendered correctly, it moves pixels only. The weapon attachment,
  muzzle flash, projectile spawn, lights, shadows and the aim ray all hang off
  the engine's bone transform. The project's own rule: engine-side writes let
  attachments follow for free, matrix patches do not.

The tester accepted the second point explicitly, on the basis that crosshair
placement can be faked separately. The first point still stands regardless.

### 4.3 The bone palette at the constant upload

Rewrite the hand bones' matrices in the `c6` upload. This is the last word
before the GPU - the draw consumes the constants immediately and nothing
recomputes after - so it is downstream of everything that killed 4.1. The
legacy `rtd_drive` lane already proved geometry moves from here.

**Outcome: geometry moved, and every attempt to place it correctly failed.**
Four separate faults, found in this order:

1. **Axis order assumed** (UE3 X-forward). Wrong - see section 3. Hands went
   under the floor.
2. **Scale assumed** (108 uu/m). Wrong - it is ~210.
3. **The basis was applied transposed.** A D3D skinning palette stores the
   transpose of a 4x3: row `r` is matrix COLUMN `r`, with the translation in
   each row's `w`. So the translation reads like an ordinary vector and the
   basis does not. The signature was unmistakable once seen: position landing
   correctly, one controller angle where the hand sat exactly right, and a
   rotation lever several feet long.
4. **The transform was not rigid.** The delta was applied only to bones
   classified as hand, but a skinned vertex blends across several bones, so
   every vertex on the hand/forearm boundary was stretched between a moved bone
   and an unmoved one. This is the mangled, spiked mesh in the tester's
   screenshots. Applying the delta to every bone on the side was the intended
   fix and was not confirmed before the revert.

**Process failure worth naming: 1, 2 and 3 shipped together, in one lever, with
all three unmeasured.** The project has an explicit rule against exactly that.
The correction - defaulting the lever OFF and shipping a read-only probe that
answered space, scale and axis order from a single run - should have been the
first move, not the fourth.

### 4.4 The original mod's shipped answer, for completeness

Build 38.92 shipped hands by hiding the engine's arms and drawing **our own OBJ
meshes** at the controller pose - "tracking is 1:1 by construction". The
machinery still exists (`core/gfx/hand_mesh`, `vrhands/*.obj`, and the caller
was rebuilt during VR-31). Not pursued because the models it draws are
placeholders and the real hand geometry is what the split now owns.

## 5. What I believe the state of the problem is

Stated as falsifiable claims, so a reviewer can attack them individually.

1. **The palette lane is the right lane.** It is the only write that nothing
   downstream recomputes. 4.3's failures were all in the transform, not in the
   approach, and each has been identified.
2. **All three unknowns are now measured**: space (camera-relative), axis order
   (right, up, forward), scale (~210 uu/m). These came from one probe run and
   should be re-confirmed rather than trusted.
3. **The remaining blocker is the reference frame, not the mechanism.** The
   pivot must be the wrist and the transform must be rigid across every bone
   that influences the drawn geometry. Both are understood; neither has been
   confirmed in a run.
4. **`g_msBoneHand[]` and `g_msHandBone[]` are unsafe as named.** They come
   from the retired sphere test. Anything that needs "the hand bones" should
   derive them from the plane, or from the classified triangles' blend indices,
   not from these.
5. **The static hand is a stepping stone, not the destination.** Replacing the
   bone matrices rigidly gives a hand with no finger animation and no grip. The
   real answer probably drives the wrist bone and lets the engine's own
   animation play through the children - which is closer to 4.1 in spirit but
   at a stage the engine does not recompute.

## 6. The plan I would follow next, for review

**Step 1 - fix the split's bone classification.** Derive `g_msBoneHand[]` from
the same plane the triangles are cut by, not from the sphere. Verify with the
existing probe: the reference-to-hand-cluster lever should collapse from 66-108
uu to a hand's own dimensions, and the cluster spread from ~200 uu to ~40. This
is a defect in shipped code regardless of VR-33 and should land on its own.

**Step 2 - confirm the rigid transform with rotation disabled.** Apply
translation only, to every bone on the side, with the pivot from step 1. A
translation-only drive cannot tear geometry and cannot be wrong about the
basis, so if the hand moves cleanly to the controller, the reference frame and
the scale are right and the remaining risk is isolated to the rotation.

**Step 3 - add rotation, with the basis convention as a live switch.** The
transposed reading is the D3D convention and the strong favourite, but it has
never been seen working, so both stay one press apart until one is confirmed.

**Step 4 - the grip pivot.** Rotation about the palm rather than the wrist,
seeded from the split's own measurement (12.26 uu from the wrist ring to the
hand's centroid) and tunable in the headset.

**Step 5 - only then, the weapon.** It should follow the hand for free through
the engine's attachment if the hand's own bone is what moved. If it does not,
that is a separate ticket and a separate mechanism.

### Questions for the reviewer

1. Is the palette lane right, or does replacing bone matrices at the constant
   upload have a failure mode not hit yet - a second pass over the same bones,
   shadow or depth draws with their own palette, or per-eye divergence under
   the re-entry stereo method?
2. Is there a better source for "the hand's bones" than either the sphere test
   or the plane - something the asset states directly?
3. Is the static-hand stepping stone worth it, or should step 2 go straight to
   driving the wrist bone and letting the engine animate the children?
4. Anything in sections 3 or 5 that is not actually supported by the evidence
   given.

## 7. Current tree state

Reverted to `1309c3eb`, which is the arm/hand split plus the desktop mirror
fix, with the measured ring position and the cap colour. This is the last state
the tester confirmed good.

One deliberate exception is kept: the fix that stops a mode veto writing itself
into the ini. `GamepadOnly=1` used to bake `[Hands] Enabled=0` into the config
file permanently, so clearing the mode later did nothing and the hands stayed
dead with no line saying why. Unrelated to this lane and expensive to rediscover.

**Open, and to be checked on the next run:** the tester reported missing wrist
caps and an eye height that felt too tall while the palette drive was active.
Both are expected to be gone with the revert - the caps because they were part
of the non-rigid tearing in 4.3, the height because `GamepadOnly=1` restores
the fixed eye height - but neither is confirmed.
