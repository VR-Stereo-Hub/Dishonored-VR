# Hand poses: the open empty right hand, and the engine's pose tools (2026-09-26)

Branch `claude/right-hand-open-pose`. No Linear ticket: the workspace hit its free-plan issue
limit when this was started; file one when that is lifted.

## 1. The problem

With nothing in the right hand (the sword holstered, or no item equipped) the game curls the
right hand into a loose fist while the left hand hangs open and relaxed. In a headset the two
empty hands look mismatched.

## 2. What was built

### The signal: "the right hand holds nothing"

Already published, headset-proven, and not an inference from geometry:
`reflect.cpp` reads `DishonoredInventoryItem.m_CurSocket` (None | Equipped | Holstered | Give)
for every inventory item four times a second, and `g_rflPrimaryKind` says what is EQUIPPED in the
right hand: 0 nothing, 1 the sword, 2 another item, stamped by `g_rflPrimaryKindTick` (unknown is
not empty: a failed read lets the stamp age out). A holstered sword reads 0 because its own socket
says holstered. New alongside it: `g_rflSecondaryKind` for the left hand, 0 nothing, 1 a power
(from `DishonoredInventory.m_EquipUsageInfo[Secondary]`, powers are not socketed), 2 another item,
-1 unknown.

The open pose applies while the right hand is empty, the read is under a second old, the left
hand holds nothing or a power (copying a hand that holds the Heart or a gun would look wrong), and
the state has held for 250 ms (the socket flips partway through the holster clip, and a swap
passes through empty for a few frames). Drawing the sword leaves it at once.

### The pose: the left fingers, mirrored, on the right wrist

The mod already draws both hands from its own copy of the game's bone palette (the arm/hand
split, `mesh_split.cpp`), placing each hand with a delta on the left of every bone matrix. The
open hand changes only the right hand's FINGER bones in that copy, just before the right hand's
draw:

```
P'[finger_R] = P[wrist_R] * X * inv(P[wrist_L]) * P[finger_L] * X
```

`P` are the skinning matrices (reference-pose space to world), `X` the reflection across the plane
between the two wrists in reference-pose space (the hand classes mirror across X: centroids
+52.8 / -52.8). The left finger's motion relative to its wrist is reflected into a proper motion
and put on the right wrist, so the right hand keeps its own mesh, wrist, placement and sleeve and
only the fingers change. Mirroring the whole left hand would have carried the Outsider's mark;
this does not.

Self-tests (`hand_frame_test.h`, run at startup and by `frame_test.exe`):
- `open_hand_mirror`: for a rig posed as an exact mirror image (P_R = Xw * P_L * X for an arbitrary
  world reflection Xw), the transfer reproduces the right finger's own matrix (error 0.000009).
- `open_hand_can_fail`: the same transfer without X misses by 5.76, so the first test can fail.

### Pairing the bones

Palette slots are not laid out symmetrically (the wrist log shows left bone 2 at -37.8 uu along
the limb pairing with right bone 27, not 26). So each right finger bone (ahead of the wrist along
the limb axis, the same forearm test the rigid wrist uses) is paired with the left finger bone
whose reference-pose centroid lands on its mirror image, checked both ways, within 1.5 uu. One
unmatched finger refuses the whole pose and the log says why: `hands/openright: REFUSED - ...`.
A successful pairing is logged once, as `right<-left` slot pairs.

### Levers

`[Hands] OpenEmptyRightHand=1` (default on, as requested), F10 Hands > Sleeve > "Open right hand
when it is empty". 0 restores the game's fist.

### Known limits (not verified in a headset)

- Drawing the sword switches the fingers back at once; a short pop is possible. A blend would
  need per-bone rotation interpolation; add one if the pop shows.
- It only applies where the mod draws the placed hands. During a game hand-back (mantle,
  takedowns, cinematics) the game's own pose shows, which is the intent there.
- The left hand's CURRENT pose is copied, so whatever the left does when empty (an idle breath,
  a flex) the right repeats, mirrored.

## 3. The engine's pose tools, surveyed for this and later features

From the decompiled scripts (declarations only; offsets and behaviour are derived at runtime).

| Tool | What it offers | Possible uses here |
|---|---|---|
| `AnimNodeSlot.PlayCustomAnim(name, rate, blendIn, blendOut, loop)`, `PlayCustomAnimByDuration`, `StopCustomAnim`, `GetCustomAnimNodeSeq` (the game's own `DisAnimNodeSlot`) | Play any loaded AnimSequence by name through a slot in the arms' AnimTree, with blend times | A real "hand open" or relaxed clip on the right arm, if the game ships one; custom gestures |
| `AnimNodeBlendPerBone` (`BranchStartBoneName`, `SetBlendTarget`), `DisAnimNodeBlendPerBone` (`m_BranchEndBoneNames`) | A second pose applied from a named bone down, blended | Restrict a slot clip to one hand's fingers |
| `AnimNodeMirror` (`bEnableMirroring`) with `SkeletalMesh.SkelMirrorTable`, `SkelMirrorAxis` (default X), `SkelMirrorFlipAxis` (default Z) | The engine's own animation mirroring, if the arms mesh carries a mirror table | Native left-to-right mirroring; check at runtime whether the arms mesh's table is populated |
| `SkelControlSingleBone` (`bApplyRotation`, `bAddRotation`, `BoneRotation`, `BoneRotationSpace`, translation too; `ControlStrength` blends) | Set or add a rotation/translation on one bone in a chosen space | Per-finger posing inside the engine, where attachments follow for free; the mod already drives the arms' existing SkelControls (`skelcontrol.cpp`) |
| `SkelControlLimb`, `SkelControl_CCD_IK`, `SkelControl_TwistBone`, `SkelControlLookAt` | Two-bone IK, chain IK, twist distribution, aim | Elbow and forearm IK towards the tracked controllers (a full-arm mode) |
| `AnimNodeAdditiveBlending`, `AnimNodeBlendList`, `AnimNodeBlend` | Additive layers and switched branches | Layering a grip curl on top of the game pose |
| `DisAnimNodeBlendBy*` (MeleeState, AssassinationState, PlayerStance, PlayerSneak, SpringRazorState, GrenadeState, CastPower, StepUpMantle, SwimState...) | The arms' AnimTree switches on these game states | Each node's active child IS a game-state flag the mod could read, the way `anim_state.cpp` reads the FSM |
| `SkeletalMeshComponent.GetBoneName`, `MatchRefBone`, `GetBoneMatrix`, `GetBoneMatrixLocal`, `GetRefPosePosition`, `GetBoneNames`, `GetParentBone` | Bone names and matrices at runtime | Pair left and right bones BY NAME instead of by centroid; read a finger's local rotation directly |
| `HideBone`, `HideBoneByName`, `UnHideBone` | Hide a bone, which hides its children too | Why hiding the arm bones also stopped the hands: the hands are children of the forearm. The split path is the right tool there |
| `ShowMaterialSection(materialId, show, lod)` | Hide one material section of the mesh | Sleeves or the mark as their own sections, if they are |
| `SetForceRefPose`, `FindAnimNode`, `FindSkelControl`, `UpdateAnimations`, `ForceSkelUpdate` | Force the bind pose, find tree nodes by name | Diagnostics; locating the nodes above |
| `DisSeqAct_TogglePlayerLeftHand` (Enable/Disable/Toggle), `DisSeqEvent_PlayerHolsterWeapon`, `DisUseState_Holster` | The game's own left-hand switch and holster events | A native holster event instead of polling the socket |

Animation NAMES are not in the script dump: the `DisTweaks_*Animation` classes carry their clip
lists in the packages (defaults read `AnimName="None"`). Finding a shipped open-hand clip needs a
runtime listing of the AnimSequence objects the arms' AnimSets load (a GObjects walk by class, the
same kind the property resolver already does).

## 4. Next, if the open hand is wanted closer to the game's own

1. Pair bones by NAME (`GetBoneName`) and log the names, which also documents the rig.
2. List the arms' loaded AnimSequences at runtime and look for an idle or open-hand clip; if one
   exists, `PlayCustomAnim` through the arms' slot with a per-bone mask from the right hand is the
   native answer, with the engine's own blends.
3. Check the arms mesh's `SkelMirrorTable`; if it is populated, `AnimNodeMirror` is the native
   mirror.
