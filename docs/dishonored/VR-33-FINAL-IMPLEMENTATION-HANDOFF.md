# VR-33 consolidated handoff: repair eye attribution, then rotation and weapons

Reviewed 2026-09-07 against `f65e60f3`, BRIEF 4, the current implementation,
the installed ini/log, the 28 existing packets in `D:/dvr-data/dumps`, and the
captured shaders. No implementation, installation or game state changed in
this review. This document provides conditional instructions so work can
continue without another Codex review.

**Recommendation:** retain the confirmed placement, caps and depth behavior.
Repair the measurement's unit of counting before inferring anything else about
stereo. Establish one render-view context shared by both hands, then finish
stereo/units/timing, add rotation, move one complete weapon assembly, and finally
connect gameplay. The current eye evidence has a concrete source-code defect.

## 1. The current eye instrument is comparing hands, not eyes

`MsDraw` calls `MpWorldTarget` inside its per-hand replacement loop
(`mesh_split.cpp:2313-2323`). `MpWorldTarget` increments
`g_mpEyeShaderOrd[slot]` at approximately line 1890. Thus the normal sequence is:

| Original engine draw | Replacement hand | Instrument ordinal |
|---|---|---:|
| First | Left | 0 |
| First | Right | 1 |
| Second | Left | 2 |
| Second | Right | 3 |

The supposed eye pair 0/1 is normally the two hands emitted for ONE original
draw. Their VP and LocalToWorld are naturally identical. The next original
draw increments the alleged third-draw counter. This explains why the same-frame
comparison can report zero over thousands of samples without testing eye
separation at all.

Consequences:

- The 24,376 zero pairs do not establish that LocalToWorld lacks eye information
  between real left/right views. The original midpoint classifier remains
  discredited, but its replacement did not perform the proposed falsification.
- The third-draw counter cannot establish a third engine view or third eye.
- If the vote ever became learned, it could apply opposite eye corrections to
  the left and right HANDS within the same eye view. This is an implementation
  fault independent of the numerical sign convention.
- The hunt's saved matrices are global rather than keyed to a complete
  original-draw/view identity, creating another possible comparison mismatch.

There is a second unit error: `dvr::frame::count()` increments per Present
(`frame_hooks.cpp:140`). The reentry method explicitly produces two Presents
per game tick, one per eye. Equal Present-count is not a demonstrated left/right
pair. Distinguish game tick, stereo pair, render view, original draw, replacement
hand subdraw and delivered image serial.

**Immediate code change:** move draw-state sampling and eye/view-context
acquisition above the hand loop. Assign one originalDrawId at entry and give
both hands the same immutable draw context. Keep ordinal telemetry if useful,
but never turn an ordinal into an eye label.

### The allegedly silent hunt already has output on disk

At review time the installed log contains **17 `ms/palette/hunt:` lines**,
including lines 15580 and 17056, reporting zero VP and LocalToWorld differences.
The file is:

`C:/Program Files (x86)/Steam/steamapps/common/Dishonored/Binaries/Win32/dishonored_vr.log`.

Its header identifies `vr33-scale-correct-2-gc9d7afbb-dirty`, built September 7
at 19:04:13. The installed ini has both PaletteEyeOffset and PaletteEyeHunt set
to 1. This does not establish what an earlier file/window contained; it does
establish that the present handoff's blanket absence claim is stale or based on
a different search. Record file, build header and time window before reporting
an absence. The data directory override is `D:/dvr-data`, not the default
LocalAppData directory.

The hunt also sits inside `if (g_mpEyeOffset)`. Decouple its collection from
whether correction is enabled, so a diagnostic can inspect an unmodified
baseline. Do not create a fourth feature-gate dependency.

## 2. Existing captures already constrain the optical-center proposal

I evaluated the proposed optical-center construction on all 28 saved packets,
using the shader's actual matrix convention. In those packets the VP-input
optical center is at the origin to within **0.000978 uu**; the first packet
gives exactly zero. This is nowhere near a several-uu half-IPD.

The captures comprise seven Present-count groups, each with two DB11BB81 packets
and one packet for each other shader. They have an unknown eye label, not a
verified stereo-pair identity. They cannot prove cross-eye equality. Nevertheless,
they strongly suggest that another optical-center implementation alone will
rediscover an already camera-relative coordinate origin in this path.

Equal VP does NOT imply stereo happens outside draw constants. In camera-relative
rendering, each view can rebase its coordinates so its camera is zero; the eye
then appears in the rebasing, object transforms, view metadata or c5. A projection
can be identical in both eyes while correctly positioned world geometry has
stereo disparity. Inspect the complete chain and real view identity before
concluding that the eye is absent from rendering.

Do not compare raw VP-element differences to an IPD in uu. Matrix elements mix
focal scale, depth mapping, coordinate rotation and translation contributions;
their differences are not uniformly lengths.

### Avoid a new cancellation bug in Plan A

Adding each optical center to the same HEAD-relative controller vector can
recreate zero disparity. In a simple common frame, let the eyes be at
`E_L = -IPD/2` and `E_R = +IPD/2`, with a controller straight ahead. Placing
it at `E_eye + head_relative_controller` puts it straight ahead of EACH eye,
so both images coincide. Its actual position should be one common 3D point.

The frame-correct expression is:

```
target_in_VP_input = E_in_VP_input
                   + conversion_of(controller_minus_THIS_eye)
```

For parallel eyes, with e_eye expressed in head coordinates and v_head the
controller-minus-head vector:

```
target = E + k * Basis * (v_head - e_eye)
```

E and e_eye must describe the same render view and pose generation. If the
VP-input space is a shared head-centered frame and VP already translates for
the eye, the equivalent expression is simply the shared controller point;
do not subtract the eye again. If it is independently eye-centered, E is zero
and subtracting e_eye supplies the missing disparity. Use actual eye poses for
nonparallel/asymmetric cases rather than a sign plus half-IPD shortcut.

Optical-center recovery is a useful check, not a replacement for identifying
which coordinate origin the controller vector is relative to.

## 3. Resolve render-view identity using the existing stereo boundary

The game-thread second-pass latch is intentionally thread-specific
(`camera.cpp:395-401`). Making it global/atomic without attaching it to queued
work would only replace a consistently false signal with a race.

The repository already has the producer and consumer:

- `scene_draw.cpp:315,371` pushes +1/-1 metadata before the corresponding
  viewport calls.
- `reentry.cpp:616` publishes tags into the ring.
- `SequentialReentry::end_frame` pops the tag and may correct its eye from
  camera evidence before tagging the captured image.
- Capture delivery can lag; the delivered image has its own serial/tag.

**Preferred design:** give each scene submission an immutable ticket containing
sceneId, pairId, eye, pose generation/time and camera/eye transforms. Associate
that ticket with the render-executed view, not with the producer thread's
current flag. Hand and weapon placement consume that view's ticket. Submission
uses the same ticket to identify the pixels produced.

There are two implementation routes, depending on the verified render boundary:

1. If the existing one-scene/one-Present route exposes a reliable render-side
   main-view boundary, latch a ticket once there. Refactor tag resolution into
   a shared operation whose result is retained through Present. If a ring
   candidate is previewed before drawing, it must later be committed exactly
   once, and any c5-based correction must be resolved consistently before the
   hands use it. Do not independently peek/pop per hand, shader or material
   pass. A raw peek of the next ring slot is not ground truth: this code already
   contains ring-skew and late-override recovery.
2. If that boundary cannot be established, carry the ticket through the same
   UE3 render-command ordering as the view. A verified view object/renderer
   context keyed to immutable metadata is suitable, or begin/end markers
   executed in the engine's actual render queue. Begin installs the ticket in
   render-thread scope, the draws consume it, end restores the prior scope.
   A separate mod queue plus timing assumptions is not equivalent. Verify native
   queue/context layouts before using them; never invent a UE3 command object.

For an initial read-only trace, record the original hand draws during a render
interval and label them AFTER Present using the actual resolved source tag,
then link to the delivered capture serial. This can reveal real left/right
differences without first needing an early eye signal. It is retrospective
evidence, not authorization to use a future Present tag for an earlier draw.

Validate attribution against rendered eye IDs and world-camera separation,
including a forced skipped second pass, extra untagged Present, pause/resume,
reset and capture latency. The hand-draw eye, source-frame eye and delivered-image
eye must agree by ticket/serial. Never infer the physical eye from hand-side,
odd/even global counters or a moving midpoint.

Unknown context yields a named fallback, not a guessed half-IPD. Keep the
confirmed no-offset baseline available while this is being established.

## 4. Determine size from geometry AND stereo before changing mesh scale

The too-large report is compatible with deficient disparity, especially if
monocular appearance is acceptable, but it does not establish that cause.
At IPD 6.31 uu and distance 44 uu, expected binocular convergence is about
**8.2 degrees total**. The ratio is small compared with a metre but not small
for near-field stereopsis. Zero or reversed disparity is a substantial error.

Measure these quantities separately using a known marker near the palm:

- Angular width in each eye, or equivalent projected pixel bounds with known
  viewport/FOV. This diagnoses geometry size, distance and projection.
- Left/right projected position of the SAME 3D point, with both view/projection
  matrices and principal points accounted for. This diagnoses disparity/sign.
- Palm and test-object extent/depth in consistent scene units and metre mapping.

Use a known-size reference at several distances, with stereo eye IDs carried
through the real compositor. Compare hands and a world/reference marker at the
same intended depth. Do not use a target produced by the suspect hand transform
as the only reference.

Decision branches:

- Correct monocular extent, wrong disparity: fix eye-relative placement and
  source/delivery attribution. Do not shrink the hand mesh to compensate.
- Disparity right, monocular extent wrong: inspect source mesh size, chosen
  controller-to-palm depth, viewmodel versus world FOV, and metre conversion.
- Both wrong: first establish units/projection and ticket consistency, then
  assess physical mesh size.
- Error mainly during motion: align pose generation and prediction times before
  further static calibration. Current head/hand timing remains unaddressed.

The eye camera uses `g_posScaleUU` through `present_tick.cpp:297`; the hand
target uses `g_skcWorldScale * gain`. The installed values are 108 and 100.
The old eye lever also scales IPD by the hand value, while the camera uses the
camera value. In this measured camera-relative world frame, establish one
canonical metre-to-scene conversion; if a separate component scale exists,
represent it in that component's transform. Do not maintain two unexplained
physical scales. An 8% mismatch can affect depth and angular extent; it does
not by itself prove the entire size complaint.

Only after those checks, if the authored hand is physically too large, introduce
an explicit uniform geometry-size adjustment around the palm anchor. Keep
controller translation gain and physical IPD unchanged. Check normals and caps,
and calibrate weapon grip size relationships separately. This is optional
geometry calibration, not a substitute for correct stereo.

Preserve the confirmed depth range restoration and exact state restoration.
Do not solve size by changing near/far depth or reintroducing duplicate passes.

## 5. Build rotation before calibrating weapons

Use the measured complete coordinate chain, not the earlier -I translational
basis transplanted into component-local matrices. Publish a coherent snapshot
of head, grip poses, tracking-space identity, timestamps and validity. Match
head and hands at the same prediction time, including look-ahead, and associate
it with the rendered generation. Plain float slots with a validity flag are
not a coherent cross-thread pose packet.

Define an explicit source palm frame. The 12 nearest-centroid vertices are a
candidate POSITION patch, not automatically a stable orientation frame. Prefer
three deliberately selected, well-separated palm landmarks: palm base and two
directions across the palm, excluding fingers and deforming wrist-cut vertices.
Construct axes with ordered vectors and an orthonormal cross-product basis;
check lengths, triangle area and handedness. Freeze the vertex identities.

A fit from several validated palm vertices to a fixed reference patch is also
reasonable. Use a proper rotation fit with determinant control and rejection
of ill-conditioning. Avoid per-frame PCA axes without sign/order continuity;
near-symmetry can flip axes. The acceptance condition is stability relative to
the palm during finger animation, not just non-collinearity in one bind pose.

Validated native hand-bone frames are an alternative if available in the same
render space and generation. Their presence does not require reopening the
inert SkelControl experiments. A named bone query alone does not establish its
relationship to the GPU frame.

In one canonical common frame:

```
A_target = controller_grip_pose * fixed_grip_to_palm
D_common = A_target * inverse(A_source_palm)
```

For the hand component's local palette, with C_hand mapping its palette output
to that common frame:

```
D_hand_local = inverse(C_hand) * D_common * C_hand
M_i_new = D_hand_local * M_i_original
```

Apply the same D to every influence used by that hand's isolated draw, including
cap/boundary influences. Compute it from original palette data, never previously
modified data. Keep finger animation. Rotate the matrix linear parts, and
inspect how each shader obtains normals/tangents and any inverse transforms.

Convert coordinate conventions by full basis changes, including reflection
where applicable. Do not negate Euler angles or quaternion components to mimic
translation signs. Grip offset is a fixed per-side transform expressed relative
to the controller, so its translation rotates with the controller.

Acceptance: fixed palm-on-grip through controller yaw/pitch/roll, fixed
controller under head movement, independent hands, finger/power animation,
stable caps, same pose in both eyes. Start with known synthetic orientations
and visible axes; use the headset to judge comfort/alignment after numerical
and rendering checks pass.

### Two remaining implementation assumptions to tighten

Current LocalToWorld validation checks column lengths, but unit columns alone
do not prove orthogonality. Check `R^T R` and determinant before using transpose
as inverse, or use a checked affine inverse with explicit scale handling.

MpAnchorPos skins raw positions while the actual shader first applies
MeshExtension and MeshOrigin. The inspected captures use identity extension
and zero origin, so this currently agrees there. Preserve that as a verified
condition or implement the decode using each shader's reflected constants;
do not carry the assumption into a different weapon mesh. Raw VP row
normalization also assumes a symmetric projection. For asymmetric projection,
use the actual view basis or a validated decomposition removing principal-point
terms before extracting axes.

## 6. Move a complete equipped weapon assembly in the common frame

Keep the GPU path as the near-term visual backend. A native item/component
mechanism may be preferable for functionality, but it is separate from whether
the named SkelControls tick. Do not throw away working hand placement while
investigating that mechanism.

The phrase same delta is only correct in the SAME coordinate frame. A numerical
hand-local palette delta cannot generally be copied into a weapon-local palette.
With C_weapon mapping weapon palette output into the same common frame:

```
D_weapon_local = inverse(C_weapon) * D_common * C_weapon
M_weapon_i_new = D_weapon_local * M_weapon_i_original
```

This preserves the weapon's original relationship to the animated source hand
while moving both by one common rigid transform. For a static piece whose
local-to-common matrix is C_piece, use `C_piece_new = D_common * C_piece`.
Handle units, transpose and normal transforms for each verified shader path.

D_common must be available before every dependent draw. Determine actual draw
ordering. If a weapon draws before the hand, obtain the source grip/palm frame
from a same-generation palette upload or validated native snapshot before that
weapon draw, or give the item its own verified source grip anchor targeting the
same canonical grip. Do not silently use the last eye's or previous frame's
delta. Rendering order must not define the attachment relationship.

### Identify the actual assembly

Start with one equipped crossbow and a loaded bolt. Do not call c6 x36 a weapon
identity; older routing used different counts for different items and static
components. Record mesh buffers, ranges, declaration, shader hash/layout,
materials/pass state and lifetime. Tie candidates to a live owned inventory
item and first-person component where possible. Use equip/holster transitions
and a bounded visible isolation test to confirm the association.

The decompiled scripts provide useful links:

- DishonoredInventoryItem: owning inventory, equip usage, socket category,
  m_pMesh, m_pPlayerMesh and post-update component are distinct fields.
- DishonoredItemSkeletalComponent: m_pItem backlink.
- SkeletalMeshComponent: actual attached parent and Attachments; animation
  sharing through ParentAnimComponent is a separate relationship.
- Pistol defaults include LeftHandWpn. Choose side from the live equipped
  attachment, not from a universal right-hand-weapon assumption.

Treat body, limbs/string, loaded bolt, magazine and reload parts as an assembly
with explicit membership and state. A released/fired projectile must stop
inheriting the held-weapon transform. Do not attach arbitrary nearby effects,
NPC weapons, dropped copies or subsequent draws using an ordinal window.

Acceptance: idle, aiming, head/controller motion, fire, reload, equip/holster,
death/load, both eyes and relevant render passes. No stationary duplicate,
missing bolt or double-applied native-plus-GPU motion. Preserve engine animation
inside the moved assembly.

## 7. Then connect functional aiming and interactions

GPU placement alone leaves CPU aim, collision and effects at their native
locations. Give those consumers the same canonical tracked weapon/grip pose,
with per-item muzzle/aim calibration, in game-world units. Use grip pose for
hand placement and the appropriate calibrated aim pose/weapon axis for aiming.
The displayed ray and the actual attack must consume one agreed origin/direction.

Start with one ranged weapon:

1. Trace its actual native firing path and the point where aim is cached or
   consumed. DisItemContext_ProjectileAttack exposes cached aim information and
   a camera-update firing field; changing a draw matrix does not update them.
2. Feed controller-based aim through a validated native hook or existing game
   aiming adapter at that boundary. Place muzzle effects/projectile origin
   consistently. Respect inventory ownership, ammo, reload and native timing.
3. Verify hit location against the same debug ray and visible muzzle while the
   head looks elsewhere. Check near-wall behavior and released projectile motion.

Then extend to left-hand powers, melee traces and carried objects. These are
different gameplay consumers; corpse carrying has its own state machine and
cannot be assumed to behave like an equipped weapon. Native component/attachment
updates are a legitimate separate route here. Use a demonstrated update boundary
and recomposition where required, not high-rate field writes or guessed arrays.

Native transform reads/writes must retain the established owner/ancestry,
parameter-frame, object-lifetime and recursion-guard contracts. One consumer at
a time, with an explicit fallback for unsupported items/states.

## 8. Make instruments test their own sampling, not just their answers

Every instrument should declare and log its unit: originalDrawId, hand subdraw,
renderViewId, pairId, pose generation and capture serial. This session's main
failure was a mismatch between the code's sample unit and the narrative's unit.

Implement these requirements in the harness:

- Two hand replacements of one engine draw produce ONE view sample, not a pair.
- Identical constants in that case must not be accepted as evidence about eyes.
- Two genuine view tickets can have equal VP under camera-relative rebasing.
  The instrument must still distinguish their identities.
- Synthetic swapped tickets, a skipped second pass, an unknown Present and
  reused shader pointers must fail or report uncertainty, not vote themselves
  into a plausible interpretation.
- PaletteWorld=1 with relative/absolute modes off still publishes poses; the
  diagnostic works with correction off; invalid tracking is explicitly counted.
- Zero observations, zero numerical difference and failed reads have separate
  statuses. Each stage reports entered, eligible, sampled, rejected-by-reason
  and output-written counts, even when all measurements are zero.
- State/projection tests exercise off-axis projection, non-rigid input, reset,
  tracking loss and mismatched generations. The failure must trigger without
  requiring a person to notice a bad picture.

Freeze predictions before a run. Hold all inputs except the intended stimulus
fixed in the simulator; compare held-out poses and actual rendered markers.
Log rates may limit text, not measurement accumulation. Success requires both
mechanism-level checks and the headset outcome. Record a tester-approved tag
as a recoverable visual state without declaring its unmeasured mechanism solved.

## 9. Execution order and stopping conditions

1. **Immediate:** fix per-hand eye sampling; inspect the already-present hunt
   lines with their build provenance. Preserve the confirmed placement/depth tag.
2. **Stereo/units/timing:** original-draw capture, render-view tickets, complete
   coordinate conversion, real disparity/extent checks and coherent pose timing.
   Keep the optical center as a cross-check. Do not spend another run hoping
   its magnitude will classify a rebased eye.
3. **Rotation/grip:** stable palm frame, canonical target, per-component
   conjugation and both-eye consistency.
4. **Visual weapon assembly:** one complete item and loaded/reload parts,
   then generalize verified identities and state transitions.
5. **Functional consumers:** ranged aim/muzzle first, then powers, melee and
   carry interactions.

Proceed autonomously within the authorized implementation work. No further
design approval is needed to implement the corrected diagnostic or the steps
whose stated inputs are validated. If a required view boundary cannot yet be
identified, keep that correction disabled, report the precise missing
association and continue independent source/renderer work. Do not replace the
missing association with an ordinal, timing heuristic, guessed engine offset
or a fresh interpretation of the same zero samples.
