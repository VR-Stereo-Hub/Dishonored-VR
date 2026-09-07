# VR-33 step 1 results, and the plan for steps 1b-2

> **CORRECTED 2026-09-06 after review. Three of section 2's "established"
> claims overreached and are withdrawn:**
>
> * **Handedness is NOT "right carries the weapon".** The pistol's own tweaks
>   set `m_Sockets[1]="LeftHandWpn"` (`DisTweaks_WepPistol.uc:28`). The asset
>   declares left and right weapon sockets; which one a given item uses must be
>   read per item, live. Verified in the corpus.
> * **`handAttachment_*_jnt` being a CHILD of the hand chain is not measured.**
>   A socket's `BoneName` names the bone that OWNS the socket. It says nothing
>   about that bone's parent. The attachment joints could be descendants,
>   siblings, or separately driven - and the whole "weapons follow for free"
>   argument rests on an ancestry nobody has queried.
> * **`hand_R_jnt` is an inferred spelling.** Only `hand_L_jnt` appears in the
>   dump. The right-hand bone must be resolved by lookup before it is used.
>
> Also withdrawn: a zero *translation* does not establish an identity socket
> frame - rotation and scale were resolved but never printed. And `camera_jnt`
> having a socket does not prove the viewmodel's rendering frame; that was
> already recorded in ENGINE_NOTES:840 and :977 and is not new evidence.
>
> The `RefSkeleton`-against-48-bones check in section 3 is also wrong: 48 is
> the observed GPU palette size for one draw, not a skeleton count. Requiring
> them to match could reject valid data, and matching counts would not prove
> matching indices anyway.
>
> The plan of record is now `VR-33-STEP1B-PLAN.md`.

**For review before execution.** Everything in section 1 is log output from a
running build on 2026-09-06. Everything in sections 3 and 4 is a proposal.
Nothing has been written to game memory; the module that produced this is
read-only.

---

## 1. What step 1 returned

The read-only report resolved **18 of 20 names** on the first run. The two
misses were this module looking in the wrong place, not the build differing
from the decompiled corpus:

* `SkeletalMeshComponent.Sockets` - the component declares socket *queries*,
  which are functions; property reflection cannot see them. The socket LIST is
  on the asset. Route: component -> `SkeletalMesh` (+0x1D4) -> `Sockets`
  (+0x160), both of which had already resolved.
* `DishonoredPawn.Mesh` - `Mesh` is declared on `Engine.Pawn` (`Pawn.uc:187`),
  and `FindPropOffset` matches on the outer's name.

Both corrected. Resolved offsets on this build:

| Name | Offset |
|---|---|
| `DishonoredInventory.m_Slots` / `.m_pOwner` | +0x38 / +0xDC |
| `DishonoredInventoryItem.m_pOwningInventory` | +0x54 |
| `DishonoredInventoryItem.m_CurSocket` | +0x59 |
| `DishonoredInventoryItem.m_pMesh` / `.m_pPlayerMesh` | +0x5C / +0x60 |
| `DishonoredInventoryItem.m_pPostUpdateTickComponent` | +0x110 |
| `SkeletalMeshSocket.SocketName` / `.BoneName` | +0x38 / +0x40 |
| `SkeletalMeshSocket.RelativeLocation` / `.RelativeRotation` / `.RelativeScale` | +0x48 / +0x54 / +0x60 |
| `SkeletalMeshComponent.SkeletalMesh` | +0x1D4 |
| `SkeletalMesh.Sockets` / `.RefSkeleton` | +0x160 / +0xEC |
| `DishonoredPlayerPawn.m_pLookAtControl_LeftHand` / `_RightHand` / `_Camera` | +0x8A8 / +0x8AC / +0x8B0 |
| `Pawn.Mesh` | resolved |

### The socket table, read live from the player's own asset

Nine sockets, each naming its parent **bone**:

| Socket | Parent bone | Local offset (uu) |
|---|---|---|
| `RightHandWpn` | `handAttachment_R_jnt` | 0, 0, 0 |
| `LeftHandWpn` | `handAttachment_L_jnt` | 0, 0, 0 |
| `Camera_Socket` | `camera_jnt` | 0, 0, 0 |
| `Power` | `handAttachment_L_jnt` | 1.45, 2.73, 5.44 |
| `LeftHand_PowerCastA` | `handAttachment_L_jnt` | -1.60, 1.15, -0.15 |
| `Bash_BL` | `hand_L_jnt` | 3.23, -2.85, 0.00 |
| `Bash_UR` | `hand_L_jnt` | 9.29, -5.28, 0.25 |
| `Tatoo_Left` | `hand_L_jnt` | 8.02, 1.57, -1.42 |
| `Tattoo` | `hand_L_jnt` | 7.43, 0.19, -1.98 |

## 2. What this establishes, and what it does not

**Established, from the engine's own data rather than from geometry:**

1. **The wrist has a name.** `hand_L_jnt`, and by the naming convention a
   `hand_R_jnt`. This replaces both discredited heuristics - the sphere around
   an influence-weighted vertex centroid, and the maximum-degree bone in a
   co-influence graph. Neither was anatomy; this is.
2. **The weapon frame has a name, and it is a separate joint from the hand.**
   `handAttachment_L_jnt` / `_R_jnt`. Weapons hang off an attachment joint that
   is a distinct node from `hand_L_jnt`, which is what the `Bash_*` and
   `Tatoo_*` sockets use.
3. **Handedness is stated by the asset.** `RightHandWpn` -> right attachment,
   `Power` and `LeftHand_PowerCastA` -> left attachment. The left hand casts
   powers and the right carries the weapon. This directly answers the review's
   warning not to assume primary/secondary means left/right - we no longer have
   to assume anything.
4. **The camera is a BONE in this rig**: `camera_jnt`, with its own
   `Camera_Socket`. That is a concrete mechanism for the "the view model is
   drawn in camera space" finding that has shaped this whole ticket, and it
   means the camera relationship is expressed *inside* the skeleton rather than
   applied to it from outside.

**Not established, and still not to be assumed:**

* Which palette index corresponds to which named bone. The report has names;
  the split has indices. They are not yet connected.
* Whether `g_msHandBone` is `hand_*_jnt`, an attachment joint, or something up
  the arm. The earlier "shoulder" reading remains unproven either way.
* Whether a palette entry's translation is a joint position or a bind origin.
* The viewmodel's projection scale. `camera_jnt` raises the possibility that
  apparent scale error is rig-internal rather than a unit conversion.
* The order of the native attachment update relative to pose composition.
* The offset from a socket frame to a comfortable controller grip. A socket is
  an attachment frame; the grip calibration is separate and is not measured by
  anything here.

## 3. Proposed step 1b: the bone table, and the palette mapping

**Read-only, one more run.** Walk `SkeletalMesh.RefSkeleton` (+0xEC) to recover
the reference skeleton: per bone, its **name**, its **parent index**, and its
reference pose transform. Then:

1. Print the full bone list with names and parents. This is the map the whole
   ticket has been missing.
2. Locate `hand_L_jnt`, `hand_R_jnt`, `handAttachment_L_jnt`,
   `handAttachment_R_jnt` and `camera_jnt` by name, and print their indices.
3. Print the ancestor chain of each hand joint up to the root, so we know
   exactly what a delta applied at the hand would and would not affect.
4. Cross-reference against the split: print `g_msHandBone[side]` and the bones
   `g_msBoneHand[]` selects, **named**. This settles the shoulder question with
   a name instead of a distance, and it says whether the split's cut axis is
   anchored to something anatomically sensible.
5. Compare the reference-pose translation of `hand_L_jnt` with the palette
   entry at the same index, if the palette index and the skeleton index turn
   out to share an ordering. **Whether they do is itself the open question** -
   a per-LOD palette map may reorder or subset the skeleton, so this comparison
   must be able to report "these do not correspond" rather than assuming they
   do.

**Gate:** the bone table prints with credible names and a consistent parent
tree, the five named joints are found, and the palette-to-skeleton relationship
is either demonstrated or explicitly reported as unresolved.

**Risk to watch:** `RefSkeleton` is a native array of a C++ type. The
decompiled placeholder is not a reliable layout. The walk must validate stride
and count against the bone count the draw census already measured (48) and
refuse rather than print plausible garbage. If the layout cannot be validated,
that is a legitimate stopping point and the fallback is to identify bones by
their reference transforms instead of by array walking.

## 4. Proposed step 2: the native pose boundary, with a named target

Only after 1b. The review's step 2, now with a concrete target instead of a
search:

* The write target is `hand_L_jnt` / `hand_R_jnt` in the CPU skeleton, not a
  palette entry.
* **The reason to prefer this is now much stronger than it was.** The weapon
  hangs off `handAttachment_*_jnt`, which is a child of the hand chain. If the
  hand joint moves in the CPU skeleton before attachment update, the weapon,
  its muzzle, and its effects follow through the engine's own attachment path.
  That is the one thing a GPU palette edit provably cannot do, and it is the
  difference between "the hand looks right" and VR-33 being finished.
* The investigation is bounded: trace the writers and consumers around
  `TG_PostUpdateWork`, find whether there is a point after the final control
  evaluation and before attachment update and render-pose copy.
* Prove it with a small deterministic translation on one named joint, then a
  known rotation, recording the resulting socket transform and palette - not a
  write-survival counter.

If no usable boundary exists, fall back to the review's draw-scoped palette
backend (its step 4), which is sound but leaves weapons and gameplay as
separate work.

## 5. Questions for the reviewer

1. Is walking `RefSkeleton` from script-side reflection sound on UE3 build
   9099, or is there a safer route to the bone name/parent table - for example
   through the component's bone-name query functions via ProcessEvent, at the
   cost of running on the script lane only?
2. Given `camera_jnt` is inside the rig, does that change the recommendation
   about where to apply a delta - is there an argument for driving the hand
   joints in the camera joint's frame rather than converting through the
   renderer's viewmodel frame?
3. `Bash_*` and `Tatoo_*` sockets exist only on `hand_L_jnt` in this dump, with
   no right-hand equivalents. Is that expected for Corvo (left hand casts, right
   holds), or does it suggest the dump is missing sockets that live on a second
   component - for example the equipped item's own `m_pPlayerMesh`?
4. Anything in section 2's "established" list that the evidence does not
   actually support.

## 6. Current state

Branch `claude/vr-33-hands-and-weapons-at-the-controllers`, on top of the
reverted baseline: the arm/hand split with its measured ring and cap colour,
the desktop mirror fix, and the read-only pose report. No placement code is
active. `[Hands] PoseReport=1` is read-only and ships on; nothing else from the
four abandoned attempts survives.
