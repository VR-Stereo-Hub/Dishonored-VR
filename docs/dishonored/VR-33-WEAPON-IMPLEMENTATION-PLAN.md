# VR-33: put the weapon assemblies into the tracked hands

Plan written 2026-09-07 against `82abe020` on
`claude/vr-33-rotation-grip-and-weapons`, with the subsequent headset report
confirming good position and rotation after grip capture. This is an
implementation handoff for Claude; this review did not build, install or alter
the running game.

## Outcome and chosen implementation

Keep the working hand placement, rotation, caps, stereo and depth behavior.
Place the equipped crossbow and its loaded bolt in the tracked palm, then add
the sword and pistol using the same implementation with item-specific profiles.
Support small hand and weapon grip adjustments without changing world scale.

Use the existing draw-scoped rendering path for this phase. Extract reusable
target-pose and draw-context helpers from the hand implementation; keep weapon
ownership and assembly state in a separate module. The weapon renderer consumes
the same target palm as the hand renderer, but does not wait for a hand draw.

CPU attachment and native firing work are separate consumers. Do not reopen the
SkelControl experiments or enable the old broad component writer to obtain the
first visible weapon. The legacy component code is evidence about discovery and
possible later native mechanisms, not a drop-in six-degree-of-freedom backend.

The first complete visual milestone is a crossbow AND loaded bolt correctly
held through controller/head motion, with native internal animation preserved
and no unmoved duplicate. A body-only crossbow is an intermediate check.

## W0. Preserve and persist the working grip before depending on it

Record `82abe020` and the current settings as the confirmed post-capture hand
baseline. Update the status to distinguish the new headset confirmation from
the earlier unverified build. Preserve a recoverable build before editing.

### The reflection fix is correct; persistence still drops the reflection

The actual measured draw basis has determinant +1. Consequently the current
`O_C = B * F * R_H^T * R_C` is improper, while the captured G is also improper.
Their product and the applied palette delta are proper. Keep this measured
behavior. The earlier expectation that B*F must itself be proper was too narrow.

There is a concrete problem in the current save/load path:

- `mesh_split.cpp:2254-2273` captures the full G, then reduces it to three Euler
  angles and tells the tester to save those angles.
- `config.cpp:1182-1199` reconstructs G as a product of three proper rotations.
- A proper rotation cannot reconstruct a matrix with determinant -1. The current
  Euler round-trip test exercises proper rotations, not saved mirrored grips.

This also explains the unsafe startup combination: identity G with improper O_C
can mirror the hand until capture supplies matching parity. Do not serialize an
improper matrix directly as Euler angles or a quaternion.

Implement a versioned calibration record with an explicit fixed parity factor.
For validated parity p = sign(det(O_C)), choose P = diag(1,1,p), so P*P = I.
Do not put a numerically approximate determinant into P. Define:

```
R_saved = P * G                 // proper rotation
G_loaded = P * R_saved
```

Persist p and R_saved, using a normalized quaternion or documented Euler order,
with sufficient precision. Include side, controller/profile convention,
source-palm convention fingerprint, units and format version. A full validated
orthogonal matrix plus explicit parity is also acceptable. Do not silently
interpret the old three-angle records as a complete mirrored calibration.

Save a successful capture automatically on the appropriate config/worker lane,
not through file I/O in a draw detour. Report saved, rejected or pending with
the path/reason. If the full in-memory G is still available, preserve it before
restarting. Otherwise request one fresh capture in the configured build and
save it correctly; the old printed angles alone are not the new record.

With no valid record, keep translation-only tracking until capture, or use an
explicitly parity-correct default. Never submit a reflected hand as the default
fallback. Validate properness of the final target orientation and D, rather
than rejecting the intermediate orthogonal coordinate conversion.

Tests: capture -> save -> load -> identical output pose for p = +1 and -1;
fresh startup without calibration; changed source convention; tracking loss;
and restart with no new key press. Compare actual matrices/points as well as
rotation errors, since an angular metric for proper rotations is not a valid
test of a reflected matrix.

### Small alignment adjustments

Add an independent rigid HandTrim per side, expressed in the calibrated palm
frame. Translation is in metres at the configuration boundary and converted
once through the existing effective scale. Rotation is a proper rotation with
a declared axes convention. Suggested control steps: 1 mm and 1 degree, with a
coarser modifier. Use an existing settings/debug UI or collision-checked live
commands/hotkeys; the installed build must require no manual ini editing.

HandTrim moves the hand AND its held weapon target. A separate WeaponTrim moves
only that item's grip alignment. This prevents a later hand adjustment from
requiring every weapon to be calibrated again. Save changes and provide reset
for each trim independently. Do not alter the approved 100/108 effective unit
behavior during this stage.

## W1. Resolve the equipped item and its complete visible assembly

### Start from evidence already available

The tester has confirmed that each weapon model and the bolt were previously
suppressed individually. Treat this as established separability and reuse the
actual successful component selections/draw filters. Do not spend another
headset run rediscovering whether the weapon and bolt are separate geometry.

The existing engine notes and material-component work identify assets
`Wpn_PlySword01`, `crossbow_01` and `bolt_01`, each with its own component and
section. Start from those recorded identities, the material cycler's component
records in `hands/mat_hide.cpp`, the property resolver and the corrected object
census. Reacquire the current live instances rather than reusing old addresses.
Asset names alone are not permission to move every matching copy.

The remaining qualification is transform space, live ownership, pass coverage
and animated membership. Where the old suppression filter already identifies
the original draw, put the scoped palette/component transform at that same
selection point. This is the shortest route from an individually hidden model
to an individually moved model.

For an in-place draw transformation, restore the selected native model's
visibility and execute its original draw ONCE under the replacement constants.
Leaving ShowMaterialSection suppression active may prevent the draw from being
submitted at all. Do not hide the native component and then expect its absent
draw to reach the palette hook; nor draw a second transformed copy alongside
the original. Suppression is useful for qualification and explicit fallback,
not an additional permanent step in this backend.

The decompiled declarations add these concrete links:

| Declaration | Useful role |
|---|---|
| DishonoredInventoryItem.m_pPlayerMesh and m_pMesh | Separate first-person and other item meshes. |
| DishonoredInventoryItem.m_pOwningInventory, m_EquipUsage, m_CurSocket | Ownership and equipped usage/socket. |
| DishonoredItemSkeletalComponent.m_pItem | Component-to-item backlink. |
| DisWepCrossbow.m_pArrowMesh_HighRes | Explicit separate high-resolution arrow component. |
| DisWepCrossbow.m_pCrossbowTweaks -> DisTweaks_WepCrossbow.m_ArrowSocketName | Authored arrow socket name to inspect on the live asset. |
| DisTweaks_WepCrossbow.m_NoAmmoLoadedAnimState | Empty versus loaded animation clue. |
| SkeletalMeshComponent.AttachedToSkelComponent and Attachments | Actual attachment relationship. |
| SkeletalMeshComponent.ParentAnimComponent and ParentBoneMap | Animation sharing, distinct from spatial attachment. |

The high-resolution arrow's defaults request foreground rendering, attachment
updates in tick and TG_PostUpdateWork. This supports treating it as a separate
first-person assembly member; it does not establish the live parent or which
state makes it visible. Do not guess the arrow socket's string from its field
name. Read the live setting and validate the socket on the correct component.

Pistol tweaks separately declare magazine/reload meshes and a bullet socket.
They also default an equipped socket to LeftHandWpn. Determine hand from the
current owned attachment and usage; do not route by a generic right-hand-weapon
assumption. Crossbow in one hand and sword in the other must coexist.

### Build a small live resolver, not another one-shot startup report

Run ownership and attachment discovery on the established game/script lane
after the pawn/inventory becomes available. Refresh on equip/holster, load,
death and relevant changes, with bounded retries while readiness is unknown.
Use class ancestry, not exact class-name equality. Resolve scalar/object
properties through the validated reflection path, and preserve the established
owner-checked ProcessEvent call contract for any required function calls.

Do not reuse `BqItems` blindly: it still performs exact component class matching
and explicitly labels its byte-search of Attachments as UNVALIDATED. Likewise,
do not call `FpCollect` as a harmless reader: it invokes rotation restoration
and uses broad pointer scans. Reuse validated facilities, not their historical
side effects or guessed layouts. Inventory slots are native records, not an
array of item pointers. A validated inverse backlink is the preferable shortcut.

Publish plain immutable metadata to rendering. Include live item/component
identity with a lifetime generation, owning pawn, side, asset/profile identity,
parent relationship, member role and eligibility state. No GObjects walks,
native engine dispatches, disk writes or unbounded allocations inside draws.

### Associate that instance with its draws

Extend the existing draw census/capture for bounded candidate recording. Capture
the declaration, streams/offsets, index range/base vertex, primitive type,
shader hash/layout, material/pass evidence and original transforms/palette.
Reflect BoneMatrices, LocalToWorld, VP and vertex decode constants per shader.
Capture each new qualified shader once for inspection.

Validate association using owned component/asset information, bounded component
isolation if needed, and equip transitions. A draw signature identifies a
rendered geometry/pass combination, not a unique game instance. Different
shaders can draw the same mesh; shared buffers can draw different instances.
If owner association remains ambiguous, qualify a component-to-render-context
link before modifying that draw. Palette count, proximity and draw order are
not sufficient substitutes. Report the missing link explicitly while continuing
work on already identified members.

The output of W1 is one profile/assembly record for the equipped crossbow and
its loaded arrow, plus the source-frame recipe described below. Re-equipping
can allocate different pointers; reacquire semantics rather than requiring raw
pointer identities to return unchanged. A holstered resource may remain cached
without remaining eligible for the tracked held-weapon renderer.

## W2. Share the target palm, not the last hand draw's delta

Use column-vector notation. C denotes one qualified draw's camera-relative
world frame, with the same eye origin and units used by the working hand path.

Expose a pure helper that constructs the target palm from:

- The immutable pose snapshot already used by that render interval/view.
- The validated draw conversion and existing eye correction.
- The loaded hand calibration and side-specific HandTrim.

It must work before either hand is drawn. It must not read g_mpSrcR or a cached
last-hand delta as if those values belonged to the current draw generation.
The calibrated target needs no current hand source palette. The source palette
is needed to move the HAND mesh to that target, not to define the target itself.

### Preserve the eye classifier's sample identity

There is an additional ordering dependency in the current code:
`MpEyeForPresent` is reached through the HAND draw's `MpDrawCompare`, and its
previous translation sample belongs to that hand component. An early weapon
draw cannot blindly read g_mpEyeState from the previous Present. Feeding a
weapon's LocalToWorld into the same classifier would also compare different
component origins and could undo the scale fix.

Record actual ordering using the existing draw recorder. If all selected
weapon draws already have a valid current-view eye decision available, reuse
that decision with an explicit matching Present/view check. Keep classifier
input restricted to its qualified reference component; weapon draws are
consumers of the decision, not extra samples for it.

If a member appears before that decision, supply a validated early render-view
identity from the stereo submission/render boundary before enabling that case.
The existing reentry producer tags are a starting point, but the Present path
can revise tags and capture delivery can lag. A raw ring peek or the most recent
delivered-image tag is not an early current-draw identity. Resolve one consistent
view ticket and retain its decision through submission, or establish an equally
verified early reference for this view. This is a specific input needed by an
early weapon, not a reason to redesign the working hand placement.

Latch pose, eye identity and view generation once and pass them explicitly to
target construction. Unknown or stale eye context is a named readiness failure,
not a guessed sign. Include weapon-before-first-hand and skipped-Present cases
in the ordering test. Removing the source-hand-palette dependency does not make
missing current-view metadata available automatically.

An optional proper-frame interface can factor the measured parity without
changing the working result:

```
Controller_C = [ O_C * P | d_camera ]       // proper orthonormal orientation
HandBase    = [ P * G   | 0 ]              // proper stored hand calibration
PalmTarget_C = Controller_C * HandBase * HandTrim
```

Since P*P=I, this retains the successful O_C*G orientation. HandTrim's local
translation rotates with the calibrated palm. Keep this an algebraically tested
adapter around the working code, not a rewrite of its measured conventions.

### A full palm-to-weapon grip profile

For each item and side, store a proper rigid transform H from the calibrated
palm frame to the weapon's stable grip/root frame. It contains rotation AND a
small, explicitly scaled translation:

```
WeaponGripTarget_C = PalmTarget_C * H * WeaponTrim
```

Seed H from the original native hand/weapon relationship in a stable equipped
pose, rather than asking the tester to invent six offsets. Preferred source:
validated socket/attachment and component/hand-frame data in one known frame
and pose generation. Alternatively capture both original source frames in a
matched renderer sample:

```
H = inverse(PalmSource_C) * WeaponGripSource_C
```

That capture uses ORIGINAL source transforms, including the hand's current
source palm convention, never its already tracked output against a native
unmoved weapon. Do not combine first-seen samples from different eyes/times.
The captured H is a deliberately fixed profile; applying that calibration in
later frames is valid. An unlabelled stale animated source transform is not.

Collect or refresh the default profile only while the native item is stably
equipped. Reloading, recoil and holstering are unsuitable calibration poses.
Store the asset/source convention so the profile survives object recreation,
and invalidate it when that convention actually changes. Expose small live
WeaponTrim edits with preview, save and reset. Do not recapture the hand grip
when switching weapons.

### Obtain the weapon source grip frame

Prefer an authored grip/root/attachment frame with a validated mapping into
the render component's palette-output space. If the asset has no suitable
named frame, define one on the rigid handle using fixed handle landmarks and
a validated rigid palette slot, with a fixed reference-point orientation.
Use decoded/skinned handle positions for the origin, not a skinning matrix's
translation column or the whole weapon's centroid. Record how the frame was
obtained. A root slot must follow the handle, not an animated limb or bolt.

For one skeletal component with LocalToWorld L:

```
S_local = current original weapon grip frame in palette-output space
D_local = inverse(L) * WeaponGripTarget_C * inverse(S_local)
M_i_new = D_local * M_i_original
```

Equivalently, with S_C=L*S_local:

```
D_assembly_C = WeaponGripTarget_C * inverse(S_C)
D_local = inverse(L) * D_assembly_C * L
```

Apply that one correction to every palette influence of the weapon component.
This keeps its internal animation while anchoring the handle. Native global
sway/recoil at the handle is intentionally removed by grip tracking; retained
internal mechanical animation is a different quantity. Do not add new recoil
behavior during this placement stage.

Read source palettes and other constants from the actual qualified draw state.
Do not reuse the hand's c6 cache as a weapon cache or assume a 12-bone layout.
Introduce per-draw source buffers and explicit reflected register ranges.
Keep observed mesh scale; use checked affine inversion if a qualified component
has scale, or decline unsupported scale rather than pretending transpose is
its inverse. Canonical grip frames and final common-space correction are rigid;
local matrices must faithfully retain the component's coordinate conversion.

## W3. Carry the loaded arrow and mechanical parts as an assembly

For a separate member j with its own L_j, the SAME common-space assembly
correction is:

```
D_j_local = inverse(L_j) * D_assembly_C * L_j
M_j_new   = D_j_local * M_j_original
```

For a qualified static component, compose its local-to-common transform instead
and update every dependent inverse/normal constant consistently. Do not require
a bone palette for an unskinned piece.

Do not independently pin each animated member's own anchor to a fixed target.
That would cancel arrow seating, string/limb movement, or magazine travel.
Preserve the member's current native transform relative to the stable assembly
root. The arrow socket and actual attachment relationship provide the asset's
intended relation; do not replace them with a guessed fixed offset.

### Make source availability explicit

Sharing the target removes the dependency on a HAND draw. A separate bolt that
draws before the crossbow still needs the current ASSEMBLY source frame. Choose
one demonstrated route:

1. Reconstruct the same assembly root at each member draw from its original
   component transform and validated current root-relative attachment data.
   Those data must include animated socket movement and belong to the render
   pose generation. A fixed child-to-root transform is valid only for a truly
   rigid attachment with no intervening animated motion.
2. Publish the original assembly root and current member relations before the
   first member draw from a verified component/render update boundary, associated
   with the queued render generation. This may be a native read/metadata hook;
   it does not require native pose writing or reopening SkelControls.
3. Reuse a source root from another original draw only when its identity,
   coordinate frame and generation are demonstrably identical and it already
   exists before every consumer. Do not assume the body always draws first.

Test reversed body/bolt ordering and multiple passes in the harness. No last-eye
or previous-frame delta, ordinal window or deferred replay that changes the
game's depth/blend ordering. If the early member's source is unavailable, name
that exact deficiency and complete the needed source path; do not mark the
assembly milestone complete with an unmoved loaded bolt.

### Ownership during reload and release

Track eligibility as an explicit state, for example:

```
held assembly -> loaded member / animated reload member
             -> attached to another hand, if the live attachment changes
             -> released projectile or dropped item
             -> holstered/inactive
```

An offhand-held reload piece follows the appropriate tracked hand through its
verified grip relationship, not automatically the crossbow root. Do not infer
this state from screen proximity. A released projectile stops receiving the
held transform; ownership/backlinks alone may remain after release, so active
attachment and state matter too. Native reload events still own ammo and the
mechanical animation. Physically interactive manual reloading is a later task.

## W4. Rendering integration and supported-item expansion

Keep weapon routing separate from the split mesh's hand-only identity lock.
Share the math and context types, not mutable global hand state. Suggested
implementation boundaries, adapted to the existing unity build:

| Area | Responsibility |
|---|---|
| hand_frame.h and its tests | Proper/improper frame factorization, target and per-component delta math. |
| mesh_split.cpp | Existing hands, extracting the common target helper without changing their behavior. |
| new hands/weapon_pose.cpp | Owned item resolver, assembly graph, source recipes, profiles and readiness. |
| new hands/weapon_draw.cpp | Qualified weapon draw routing, scoped state capture/restore and per-member correction. |
| palette_capture.cpp / draw_census.cpp | Reusable shader layout and bounded candidate capture; no global hand-cache contamination. |
| config/hotkeys or existing settings UI | Versioned calibration and trims, explicit reset/save, installed stage levers. |

Every draw mutation must be scoped and restored on every exit. Read original
constants before replacement. Audit weapon normals/tangents and vertex decode
from its shaders; the three inspected hand shaders do not qualify a weapon
shader. Keep all relevant depth/color/lighting passes aligned. Identify and
handle foreground viewport depth as for the hands, without changing unrelated
world draws. If changing component matrices instead of palettes, keep all
dependent inverse and lighting transforms coherent.

Avoid partial-pass or half-assembly activation: qualify a pass family and its
required members before enabling the supported state. Unknown new states or
shader paths get a named fallback, not a mix of moved and native duplicates
silently reported as success. A weapon failure must not disable the working
hands. Native and GPU writers may never both transform the same object.

After the crossbow assembly passes, add:

1. Sword profile and verified controlling hand. Preserve any internal blade
   animation; verify simultaneous sword/crossbow holding and independent motion.
2. Pistol profile, magazine/reload components and active attachment states from
   its own live item/tweaks. Reuse the assembly code with verified profiles.

Other holdables can then gain profiles. Do not promise universal support based
on one palette size or an asset-name substring. NPC copies, dropped items and
released projectiles remain outside the held-item renderer.

## W5. Validation and what the tester does

Claude runs deterministic tests before installing each behavioral stage. Add
meaningful cases to the existing frame test or a focused weapon test:

- Mirrored calibration persistence and safe uncalibrated startup.
- Weapon target equals palm target times profile, with independent hand and
  weapon trims; a hand trim moves both by the same common transform.
- Nontrivial rotated/translated component matrices: different local deltas give
  one common assembly transform and preserve member-relative transforms.
- Animated arrow/socket relation remains animated after placement.
- Body-before-bolt and bolt-before-body produce the same result; wrong pose
  generation is refused rather than reused.
- Left/right eye origin changes, head motion, two simultaneous items, resource
  recreation, unknown shader/state, release and reacquisition.
- Original constants/depth restored after success and forced failure; the next
  world or NPC draw is untouched. Hands remain on the working path.

Use algebraic residuals with declared tolerances plus rendered geometry checks.
No single self-derived target residual is evidence of physical alignment.
Automatic logs report item/side/member, asset/profile, pose and draw generation,
target/source/correction validity, active/refused state and reason. Print counts
for eligibility, processed draws and restored state; rate-limit text rather
than measurement accumulation. Do not label attempted draws as placed.

### Short headset tests, with stages already enabled

| Stage | Tester action | Expected outcome |
|---|---|---|
| W0 | Capture once if needed, close and relaunch | Same properly oriented hands without another capture; no inside-out startup. |
| W0 | Apply a small hand trim | Palm alignment changes slightly; position/rotation tracking and head independence remain. |
| W1/W2 | Equip crossbow, then move and rotate its controller | Grip stays seated in the corresponding palm through roll/pitch/yaw; weapon has no native stationary duplicate. |
| W3 | Inspect the loaded arrow and turn the weapon | Arrow stays seated in its authored relationship to the crossbow, including both eyes and occlusion. |
| W3 | Fire/reload | Loaded arrow is released from held ownership; native reload animation survives. Native projectile aiming remains the explicit limit below. |
| W4 | Hold sword plus crossbow, move one controller | Only that hand's weapon assembly follows; the other is stationary. |
| W4 | Switch crossbow/pistol, holster, die/load, restart | Correct profiles and membership reacquire; no floating cached copy or cross-item calibration. |
| All | Keep controllers still and turn/move the head | Hand and held-weapon relationship stays fixed; no eye-dependent twist or large drift. |
| All | Use live disable for weapon placement | Original weapon rendering returns and tracked hands continue working. |

Build Release, preserve the last working build/config and copy the log before
relaunching. Enable each stage's installed levers and automatic diagnostics;
keep repository defaults per policy. Give the tester the short current-stage
table and one explicit live fallback. Do not require manual config edits, log
arithmetic or developer harness commands from the tester.

## Boundary: displayed weapons versus native attacks

This phase positions the held assembly. It does not make CPU projectile origin,
firing direction, aim assist or melee contacts follow a palette edit. A fired
projectile can visibly depart from the native firing location until the next
gameplay change. State that limitation; do not require a correct muzzle launch
as a test for a build that only moves held meshes, or visually drag a projectile
away from its collision trajectory to hide the disagreement.

The next ranged stage has concrete script entry points: DisItemContext_FireCrossbow
derives from DisItemContext_ProjectileAttack, which stores cached aim and a
camera-update firing field. Crossbow firing tweaks also expose arrow spawn
distance and tracer socket settings. Trace the real native consumption point
and feed it the same calibrated weapon grip/muzzle pose in game-world units.
Keep ammo, reload timing and attack logic native. This is subsequent functional
work, not a prerequisite to getting the weapon held correctly.

## Execution order

W0 persistence/trim and reusable target helper -> W1 live crossbow/arrow
qualification and source recipe -> W2 root placement -> W3 complete animated
assembly -> W4 sword/pistol profiles. Finish each behavioral stage on the
working baseline, with automated checks and the stated headset expectations.

Do the source reads, existing-capture analysis and deterministic math work in
the development session. Request a new headset observation only when it tests
something those tools cannot establish. This is an implementation sequence;
do not replace it with successive general-purpose probes or restart the solved
hand-coordinate investigation.
