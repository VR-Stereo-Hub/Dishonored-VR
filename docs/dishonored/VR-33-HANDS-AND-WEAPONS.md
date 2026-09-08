# VR-33: the hands and the weapons

How the first-person hands and the held weapons are placed on the tracked
controllers. Written 2026-09-08, when the weapons landed and the duplicate
copies were finally cleared, replacing twenty-nine plan, review, brief and
handoff files written across the sessions that got there.

Read this before touching `mesh_split.cpp`, `weapon_attach.cpp`,
`palette_capture.cpp` or `hand_frame.h`. Section 8 is the graveyard: every
approach in it was tried, cost a headset run, and failed. Several of them
failed the SAME way.

---

## 1. The one idea

Every skinned draw carries a bone palette. The engine animates those matrices;
we compose ONE rigid transform onto all of them. The weighted skinning blend
commutes with a common rigid transform, so the animation survives and only the
result moves. That is the whole mechanism, for the hands and for the weapons.

```
newPalette[i] = D * originalPalette[i]      for every i
```

`compose_commutes` in `hand_frame_test.h` pins the property. Because D is
applied to the full 3x3, normals and tangents come with it - two of the three
hand shaders run them through these same rows, so a rigid D carries the tangent
frame correctly. `WorldToLocal` converts view and light vectors INTO component
space and must NOT be touched.

---

## 2. The hands

`mesh_split.cpp`. The split classifies each triangle of the player mesh by the
bones that move it, draws the hand classes through its own index buffer and
suppresses the rest. Placement is measured every frame rather than calibrated:

```
target = palm_target(O_C, G, d_cam, trimR, trimT)
D      = delta_from_target(R_L, t, target, R_src, q_local)
```

`palm_target` needs only the controller pose, the draw's basis and the
calibration - no hand source palette - which is what lets a weapon be placed
before either hand has drawn.

### The grip transform G is a REFLECTION

The single most expensive fact on this ticket. The draw's camera basis is
right-handed and the engine is left-handed, so the XR-to-game pose mapping is a
MIRROR: `det(B*F) = -1`. The first rotation build demanded a proper rotation and
refused all 116,908 draws.

A reflection is a coordinate convention. It carries through by full basis change
and CANCELS between the controller orientation and the grip transform, so what
reaches the palette is a proper rotation either way. The guard requires
ORTHONORMALITY only and records the parity.

Consequences that are easy to get wrong:

* **Three Euler angles cannot represent a reflection.** The grip record stores a
  parity sign beside a proper rotation, is versioned (`MP_GRIP_VERSION`), and
  REFUSES a pre-version record - loading one puts the hand inside out while
  looking like a valid calibration.
* **Identity is the wrong uncalibrated default.** With an improper mapping,
  identity makes `O_C * G` improper and the hand is drawn inside out. The
  default is the parity-matched factor.

SHIFT+F7 solves G for both hands from one pose snapshot and saves it. A
calibration survives a restart with no key press.

### The numpad adjust

`[Hands] Adjust=1`. Adopted key for key from the maintainer's BioShock
Remastered VR mod, because those are the keys the tester already has.

| Key | Position mode | Rotation mode |
|---|---|---|
| Numpad 9 | cycle mode, and NAME it in the log | |
| Numpad 8 / 2 | along the fingers | pitch |
| Numpad 6 / 4 | across the palm | yaw |
| Numpad 0 / 5 | out of the palm | roll |
| Numpad 7 | cycle the step | cycle the step |

Four modes: LEFT position, LEFT rotation, RIGHT position, RIGHT rotation. The
trim is PER HAND - one shared trim assumed the residual after calibration is
common to both palms, and it is not, because the two grips are solved from two
separate poses.

Every numpad key was already claimed, so `Adjust=1` makes an EXPLICIT claim: the
census gives up 4-9, the material cycler gives up 2, the mesh split gives up
Numpad 0 and its mode cycle moves to Numpad 1. One startup line names all of it.
**F5 was the first binding and was unusable** - it is the game's quicksave and
`head_track.cpp` reads it twice, so one press fired three features.

### The model scale

`[Hands] ModelScale`, one uniform factor about the target palm, applied to the
hands AND anything held in them.

**PageUp/PageDown were never a size knob.** `g_posScaleUU` sets the stereo
separation, which is a property of the projection, so it resizes the whole frame
at once. The hands were not scaling with the world, they were scaling because of
it. Hand TRAVEL is separate again (`WorldScaleUU`, `PaletteDriveGain`), which is
why tracking felt right while the models were too big.

Scaling about the PALM keeps the grip where tracking put it. The weapon takes
the same factor about the same palm, so the two cannot drift apart.

---

## 3. The weapons

`weapon_attach.cpp` + state chunk `57b`.

### The correction

The hand publishes its own correction conjugated out of its local space into the
draw's camera-relative world:

```
D_common  = L_hand * D_hand_local * inverse(L_hand)
D_member  = inverse(L_member) * D_common * L_member
```

Both `L` are DRAW transforms, so both sides are in one space by construction.

**There is no weapon grip and no weapon pivot, deliberately.** The game already
authors the hand-to-crossbow and crossbow-to-bolt relationships; carrying the
whole assembly through one transform preserves them for free, including the
bolt's animated relationship to the stock. Two members each solved onto the same
palm do NOT stay attached to each other - that was the first design and it would
have pulled the bolt out of the crossbow.

`D_common` carries its Present, pose generation and EYE, and the match on all
three is exact. A correction from the previous eye is a whole IPD wrong.

The model scale is folded into `D_common` exactly once, on the hand side.

### Identification: the coordinate bridge

A component's own `LocalToWorld` (native, `+0x60` rows and `+0x90` translation)
and the shader's `c231` describe the same placement, but the shader's is rebased
on the view. The offset is taken from a component identified by OTHER means -
the body mesh the split already locks and draws:

```
K                       = L_draw_ref * inverse(L_native_ref)
predicted L_draw_member = K * L_native_member
```

A candidate's own residual can never validate that candidate; the reference must
be independent. Both matrices are read by the SAME extraction so the row/column
convention cancels rather than being asserted.

### Two jobs, two instruments

Do not conflate these. Conflating them cost six builds.

**Is this draw on the view model?** PROXIMITY. Every view-model draw measures
within ~170 uu of the camera; the nearest world draw measured 1880 and most were
3000-9700. A better than tenfold gap, and a far stronger instance test than any
angle. This is what keeps a fired bolt standing in the ground out of the hand.

**Which member is it?** A relaxed band inside the view model. The held crossbow
matches at 0.000 deg, but the sword and the loaded bolt are SOCKET-MOUNTED and
their component transform does not track their render transform - they sit at
11-12 degrees. No band tight enough to exclude world geometry was ever going to
admit them. The relaxed band is still narrow enough to refuse the body mesh,
which named a member at 175 deg and 143 uu.

**The margin must shrink when the band widens.** At 20 degrees several members
qualify at once, and a margin of 4x calls anything within four times the
winner's score a tie - so the relaxed path accepted NOTHING while the census
showed the winner was correct every time. It has its own margin of 1.5.

### The instance gates

| Gate | Key | What it stops |
|---|---|---|
| View-model proximity | `AttachViewModelUU` | a world copy of a weapon mesh |
| Rig radius | `AttachRigRadius` | a fired bolt that keeps its name and mesh |
| Pass radius | `AttachPassRadius` | another INSTANCE mistaken for another PASS |

The rig radius exists because a component that has left the view model is still
called `bolt_01` and still draws from the same buffers. Rig components sit
together - the body, sword, crossbow and bolt spanned about sixty units.

---

## 4. The duplicate copies, and why they took eight builds

A dark copy of each weapon stood where the engine drew it, animating correctly,
flickering. It was **another render pass of the same mesh**, uncorrected.

The census found it in one run:

```
[0] vb 1E6DAD80 ib 109EC4A0 vs 2FDE3820 prim 1957 verts 1961
    nearest 'crossbow_01' at 9.557 deg | left where the engine drew it
[5] vb 1E6DAD80 ib 109EC4A0 vs 31613300 prim 1957 verts 1961
    nearest 'crossbow_01' at 0.000 deg | CORRECTED
```

Same vertex buffer, same index buffer, same range, same primitive count.
**Differing only in the vertex shader.** The buffer-identity check compared ten
fields and not the shader, so the uncorrected pass looked like the contract's
OWN draw, fell through to the transform match, missed by 9.5 degrees and was
counted as an ordinary miss.

Three further things were needed:

1. **The uncorrected pass draws FIRST in the frame** (census order proves it), so
   its twin's delta for the current Present does not exist yet. It is corrected
   from the current view and its own transform instead.
2. **The contract records which prediction won** (rebased or world space). A
   member is offered in both and its other passes belong to the same one.
3. **A pass we recognise but cannot place is DROPPED, not drawn.** It is a
   duplicate of geometry the same frame draws correctly, so suppressing costs at
   most one pass of depth or shadow and drawing it costs the copy. This is the
   arm split's own rule: drop what you cannot place.

### The contract table was the size of the answer

`WA_MAX_MESH` was 12. Three meshes drawn by four shaders each is twelve
contracts, and every healthy run reported `contracts 12` - pegged. One more
distinct draw evicted a contract still in use, that mesh stopped being
recognised until it was identified again, and its copy came back. It is 64 now,
and an eviction logs the age of what it threw out, because an eviction of a
contract that drew recently is thrashing and an eviction of a stale one is not.

---

## 5. Lanes and device state

* The present thread owns every runtime call. The pose tick and the hand draws
  are ONE thread (measured: 0 stale snapshots over 11,881 publications).
* Component transforms are snapshotted on the SCRIPT lane under an SRW lock and
  published with a generation; a snapshot older than 100 ms refuses publication.
* **A constant is CURRENT DEVICE STATE, not a one-shot.** The game's own palette
  block goes back immediately after the draw or every later draw inherits the
  correction. `draw_census.cpp` paid for that once already.
* Never take a reference to an engine D3D object inside a detour. Every
  `Get*` here releases inside the call and keeps only the pointer VALUE as an
  identity token.
* The bone register comes from the shader's own reflected constant table, never
  a hardcoded `c6`. Several shaders draw the same mesh and they do not agree.
* The view model is drawn into a compressed depth range. Once moved out into the
  world it needs the FULL range or it renders in front of geometry it is now
  behind - this is what fixed both the hands' occlusion and their duplicate.

---

## 6. The levers

`[Hands]` unless noted. Everything ships ON except the sweep.

| Key | Default | What |
|---|---|---|
| `PaletteRotate` | 1 | full rigid correction, not translation only |
| `Adjust` | 1 | the numpad hand adjust, and its claim on the numpad |
| `ModelScale` | 1.00 | hands and held weapons, about the palm |
| `AttachWeapons` | 1 | the weapon attachment |
| `AttachSwordHand` / `AttachCrossbowHand` | 1 / 0 | an ASSUMPTION, not measured |
| `AttachViewModelUU` | 500 | the proximity instance gate |
| `AttachNearAngle` / `AttachNearPos` / `AttachNearMargin` | 20 / 30 / 1.5 | the relaxed band |
| `AttachRigRadius` / `AttachPassRadius` | 200 / 60 | the other instance gates |
| `AttachDropUncorrected` | 1 | suppress a recognised pass we cannot place |
| `AttachCensus` | 1 | one line per distinct view-model geometry |
| `WeaponId` | 0 | the whole-scene hide sweep, retired to a confirmation tool |

### Reading the log

`wa: beat` prints every five seconds whether or not anything matched, because
"nothing attached" and "never entered" looked identical for three runs.

* `contract table N of 64` climbing to the cap means thrashing.
* `routed` rising with `compared` at 0 means no hand correction reaches the
  weapon's view - an ordering problem.
* `compared` rising with `matched` at 0 means the bridge or the bands are wrong.
* `wa: NOTHING TO ATTACH` means the weapons were never resolved as components -
  a different problem from failing to attach them.
* `wa/census` accounts for every piece of the view model, corrected or not.

---

## 7. What is still open

* **The lock takes 20-90 s** (VR-49, urgent). Slow to acquire, stable
  afterwards. The asset-to-hand and asset-to-space decisions do not change
  between runs even though the buffers do, so they are cacheable.
* **The crosshair is still head-locked** (VR-57, urgent). One ray.
* **The weapon models have no back faces** (VR-56). The asset was authored to be
  seen from one side; a tracked controller lets the player look at the side the
  artists left open.
* The hand sides are an assumption, not measured attachment data.

---

## 8. The graveyard

Every one of these was built, run in a headset, and failed.

**A filter that can only find what it already assumed is not evidence.** Five
separate ones hid the target in this investigation, each producing a confident
zero:

1. **The palette gate.** The sweep recorded only draws following a fresh `c6`
   upload. A weapon drawn as a static or single-bone attachment was invisible.
2. **The primitive-count pre-filter.** The first ghost detector keyed on
   triangle count, so a pass drawing a different number from the same mesh was
   rejected before anything looked at it. Reported `ghost passes seen 0` for
   three builds.
3. **The per-Present probe budget.** 400 draws, spent on world geometry long
   before the frame reached the view model, which is drawn LAST. Reported 0 of
   2.8 million and skipped 3.7 million more.
4. **The tolerance band.** 0.25 degrees could not admit a socket-mounted member
   at 11 degrees, and the assignment was correct the whole time.
5. **The contract key without the vertex shader.** The copy looked like the
   contract's own draw.

An ENUMERATION cannot exclude what it is looking for. The census solved in one
run what five filters had hidden for eight builds. Reach for it earlier.

### The two frame bugs that looked like each other

Both reported as "the hands drift and swivel when I turn my head", and they are
different faults. Three rounds were spent because the question was put to a
perceptual judgement instead of a number.

**The yaw residual is a COORDINATE OFFSET, not a body yaw.** `phi` held
144.0-145.6 degrees across a run in which the camera swung 73.6 to 189.1 and the
head swung -71.2 to +44.8. The camera tracks the head 1:1 and `phi` is simply
the fixed offset between UE's yaw origin and XR's. Rotating the delta by it was
wrong, and "both hands swivel" was the report of doing so. `PaletteYawFix`
defaults to 0.

**The drive's neutral was stored in HEAD SPACE, and that was the whole drift.**
The zero point rotated with the head, so a still controller produced a moving
difference and the hand swung to chase it, while the head yaw at capture became
a fixed rotation of the mapping. One error, both reported symptoms, and it
predicted that vertical would be unaffected - which is what was reported.

Subtract in WORLD, then rotate the difference into the head frame:
`R_head * R_head^T * (w - w0) = w - w0`, constant while the controller is still
however the head moves, while a stick turn rotates the frame without rotating
the offset.

The lesson worth keeping: **a frame question cannot be settled by asking whether
a hand looks stable.** Print all the candidate frames as raw numbers next to the
head yaw; held still, the correct frame is the one whose numbers do not move
while the head turns, and one that tracks the head yaw is head-coupled and wrong
however it looks.

**Other dead ends:**

* **Palette size as an identity.** "c6 x36 = the sword" is inherited lore. A
  size is not an identity: several components share a bone count and one upload
  can feed several draws. Worse, `g_dcPendingBones` was itself stale for three
  reports because its age counter only advanced when the census was reporting.
* **A vertex/index buffer pair as one geometry.** Shared and dynamic buffers
  hold several ranges and several instances.
* **One disappearance as evidence.** A camera move, an NPC leaving frame or an
  LOD switch all produce "this draw stopped during a 1.5 s window". The sweep
  requires vanish-on-both-hides AND return-on-both-shows for this reason.
* **Uniform reject counts across every component INCLUDING the player body.**
  146/154/151/153, then 51/50/51/48. Counts that uniform are not a fact about
  components; they are what "the thing being measured was never in the
  population" looks like. It was read as a signature problem twice.
* **Palette bone 0 as the weapon's grip.** A skinning matrix can carry
  inverse-bind effects and was never a grip.
* **A report that needs one more tick of a lane that can stop.** A sweep ran
  perfectly end to end and printed nothing, because the script lane went quiet
  between the last phase and the report.
* **A zero that is expected but does not say so.** The stance line read
  `standing (eye 0.0 uu)` through a run deliberately spent half crouched: the
  eye height was never resolved and the flag was 0 by design. It cost the
  correlation the run was made to capture.
* **An instrument that reports its own CAP as a census.** The GObjects sweep
  exited on `curN < 64`, the size of its comparison table, so it never walked
  past the 64th SkelControl. "Zero of 64 advanced" meant "zero of the FIRST
  64", and the native SkelControl lane was recorded as closed on that basis -
  a retraction that also withdrew the 38.x write-race retirement resting on it.
  A counter is not evidence until you know its population.
* **A printf whose arguments stopped matching its format.** Two edits each added
  the same segment; every value after that point was shifted, and decisions were
  read from numbers that were not what they were labelled.
