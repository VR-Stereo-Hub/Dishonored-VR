# Status

## CURRENT (2026-09-06, session 20f): the cut end is CAPPED

VR-31. The clip gives a clean circular boundary and leaves it OPEN, because the
arm is a shell with nothing inside it - so at every position of the ring you
look down the inside of the sleeve and out through its far wall. The clip
already produces exactly the ring that has to be closed, one boundary segment
per straddling triangle, and a fan from that ring's centre to those segments is
the disc that fills it.

Both stumps are closed, into their own class, so hands-only closes the hand and
the inverse view closes the sleeve; in `all` the two caps are coincident, face
away from each other and sit inside a closed mesh, so the A/B still rebuilds
what the game drew.

**The colour is the ring's own.** Every cap vertex is given the SAME texture
coordinate - the MODE of the ring's UVs, counted by how many ring vertices sit
within a sixth of the ring's UV spread of each candidate. A mode and not a mean,
because a mean lands between two islands of an atlas and samples whatever is
parked there. One UV on every vertex means the disc interpolates to a single
texel, so the cap is flat in the colour that dominates the cut: sleeve on the
sleeve, skin past the cuff. It re-derives on every knob press, so it tracks the
ring by construction. Position and SKINNING come from the ring vertex instead,
so the cap deforms with the arm and cannot part from the rim.

**Two-sided by default**, and not as insurance: in hands-only mode the arm is
not drawn, so nothing stands between the eye and the BACK of the hand's cap. A
one-sided disc would be culled from precisely the angle the hole was visible
from. `[Hands] CutCapTwoSided=0` halves it and is the check for a cap that is
present but facing away; `[Hands] CutCap=0` removes it entirely and restores the
pre-cap look.

Both keys default ON with no ini edit needed. The `ms: cut end CAPPED` line
says the triangle count, which ring vertex each side took its colour from, and
whether a declaration without TEXCOORD0 forced the fallback.

Built, lint clean, installed. Untested in the headset.

## PREVIOUS (2026-09-06, session 20e): the desktop had NO eye policy, and a hold corrupted its own snapshot

Two source defects, both found in a second review of the frame path, both
fixed here. Untested.

### The "alternate-eye" flicker: the game window was never pinned to an eye

`hkPresent` calls the game's original `Present` for EVERY eye draw, and
`mirror_present()` in the runtime layer had **never implemented the D3D9 copy**
- its own comment said so and it only incremented counters. So under a healthy
sequential-stereo stream the window shows

    L(k), R(k), L(k+1), R(k+1), ...

while the headset correctly receives the pairs. A recording of the window
therefore alternates between two camera positions one IPD apart, frame by
frame, which is exactly what alternate-eye rendering looks like. The headset was
never doing AER; the desktop had no eye policy at all.

`core/gfx/desktop_eye.cpp` implements the pin: snapshot the backbuffer on the
left eye's present, re-blit it over the right eye's present **after** that eye's
XR capture. The runtime layer owns the WHEN (its call sites already sit on the
right side of each capture); the new module owns the HOW, so
`openxr_runtime.cpp` stays as close to the BioShock copy as the host allows -
it gains a hook pointer and nothing else. `D3DPOOL_DEFAULT` surface, released in
`on_reset` before the game's Reset (the hkReset LAW).

### The pause-transition session loss: a hold banked the wrong structures

On a hold-only present, `proj` / `projViews` / `quad` are the EMPTY locals - the
copies actually submitted are `holdProj` / `holdViews` / `holdQuad`. The hold
sets `layerCount = 1`, and the snapshot bank keyed off `layerCount`, so it
overwrote a good snapshot with zeroed structures and left it marked valid. The
next hold submitted null handles and a zero view count, `xrEndFrame` answered
`XR_ERROR_HANDLE_INVALID`, and the session stood down. Both logs from this date
show exactly that at the pause menu. Now banked on `builtNewLayer`.

Still open and tracked separately: a saved layer holds swapchain HANDLES, not
pixels, and OpenXR composites the most recently RELEASED image - so preserving a
completed PAIR needs retained images, not a retained structure.

### Retracted: the "30 % of ticks double" reading, and the guard built on it

The window that reading came from straddled a pause menu, an `xrEndFrame`
failure and session teardown. The windows either side read `draws/s=78
2nd/s=78`, `86/86`, `87/87`, `81/81` - the renderer doubles essentially every
gameplay tick, and the refusals in the bad window were "no XR session", after
the session was already gone. The stand-down guard built on that premise is
REMOVED, not tuned: it would have disarmed a healthy renderer every time a
session dropped.

The `camera/eyetrace` line is corrected too - its ring is appended in
`note_render_pos`, so it samples CONSTANT UPLOADS, not presents, and a full
eye-to-eye span is also what healthy sequential stereo produces. It said
otherwise and that was wrong.

### Build state - read this before trusting a log's build id

The DLL installed in the game folder **contains everything below**, but it was
built before the commit, so it stamps itself `alpha-325-g30359c05-dirty` while
HEAD is `63c1f62f` (`alpha-326`). The `-dirty` and the parent sha are the
session's uncommitted-at-build-time state, not a stale build. Rebuild before the
next headset run if an exact sha match matters.

Nothing diagnostic is armed that is not meant to ship: the draw census, the
mesh split and the eye trace are all deliberate and default ON for the test
sessions (`[Hands] DrawCensus/ArmSplit/ArmSplitAuto`, `[VR] DesktopEye`).

### The run this needs

Look at the game window: **one view, no alternation**, while the headset keeps
correct stereo depth. `desktopeye:` in the log every 15 s should show snapshot
and re-blit counts EQUAL and non-zero. Then open and close the pause menu
several times - no `XR_ERROR_HANDLE_INVALID`, no session teardown.

Performance is NOT addressed here and is the next subject: ~78 complete pairs/s
against a 90 Hz headset, ~9.7 ms of D3D9 GPU span per tick, 15.7 Mpixel per
pair at 2750x2850.

## PREVIOUS (2026-09-06, session 20d): VR-31 - a reversed winding was eating a third of the cut

The clip RAN - the log says 54 triangles cut into 108 new vertices, stream 0
ours - and the result was still ragged, with some of the boundary on one clean
line and the rest missing or floating loose. That pattern named the bug.

### The bug: one third of every clip was wound backwards

`MsClipTri` picked the three corners by ASCENDING INDEX (`in[0], out[0],
out[1]`) instead of in cyclic order. When the odd vertex out is the MIDDLE one
of the three - one case in three - that reverses the triangle, and a reversed
triangle is culled. Two thirds of the cut came out on a clean line and one
third vanished, which is exactly what was reported.

Fixed by taking the odd vertex `i` and its neighbours as `(i+1)%3` and
`(i+2)%3`, so both halves keep the source triangle's winding.

### The slant: the ring may be square to the wrong direction

"Taking too many on the other side of the forearm" is a ring that is not
perpendicular to the arm. The axis was the longest direction of the arm's
triangle cloud (PCA), which a tapered sleeve can lean off the bone.

`MsBoneAxis` derives it from the SKELETON instead: the hand bone minus the bone
it hangs off, that bone being the hand's graph neighbour whose centroid is
farthest away. No names, no hierarchy, same as everything else here. Both axes
are computed, the angle between them is logged per side, and `ms axis
bone|pca` switches live - which one is right is a question about this asset, not
about geometry, and one press settles it. Default is the bone.

### Also

`+` / `-` auto-repeat after 400 ms, because at the ultrafine step the ring moves
0.1% of the arm per press and placing it by hand would be a hundred taps.

### Still unruled-out, and the check for it

The cut is computed in the mesh's BIND pose and seen in the ANIMATED one. If
the ring's shape CHANGES as the arm moves, that is the cause and the fix is a
different one; if the slant is fixed relative to the arm whatever the pose, it
is the axis. That is a thing the tester can see in one run and nothing else can
answer.

## PREVIOUS (2026-09-06, session 20c): VR-31 - the straddling triangles are CLIPPED

Headset run on the plane cut: the steps were much better, the edge was still
jagged - spikes hanging off the cuff in the screenshot. Diagnosed and fixed
here, untested.

### Why a plane was not enough

The plane fixed the JUMPS but not the OUTLINE. A triangle straddling the cut
was still kept or dropped whole, so the boundary was a sawtooth one triangle
high; on the coarse cuff geometry that is a set of spikes. No edge RULE fixes
that - keeping only triangles entirely past the plane trades spikes for
notches. The boundary has to stop being made of original triangle edges.

### The clip

A triangle crossing the plane is split into the part on each side, with new
vertices interpolated along the two crossing edges. Both halves are emitted
into their own class, so `arms` stays the exact inverse of `hands` and `all`
still rebuilds the original mesh. The boundary is then the plane itself.

That needs a VERTEX buffer of ours as well as an index buffer, because the new
vertices do not exist in the game's. Every stream-0 element is interpolated by
its declared type; blend INDICES come whole from the nearer parent vertex,
because a bone index is a name and not a quantity, and the weights go with them
so they cannot end up naming the wrong bones. Both buffers are `D3DPOOL_MANAGED`
with room to grow, so moving the ring does not recreate them and `hkReset` has
nothing to release.

**The one precondition, checked and logged**: stream 0 must be the only stream,
because re-basing the index list onto a buffer of ours would desynchronise any
second stream the game had bound. A mesh with more streams falls back to whole
triangles with the reason named.

### Smaller steps

`Numpad .` cycles the knob's step: 2% of the arm, 0.5%, 0.1%. Default 0.5%
(`[Hands] WristStep`).

### New defaults

`WristEdge=3` (clip), `WristStep=1` (fine). `ms edge 0|1|2|3` still selects the
whole-triangle rules for comparison - 3 is the only one whose boundary is the
plane rather than a row of triangle edges.

### The run this needs

The wrist should end in a clean ring with no spikes. `ms status` says how many
triangles were cut and how many vertices were made; if `stream 0 belongs to the
game` appears, the clip was vetoed and the reason is in the read lines. Then
`Numpad .` for finer steps, `+` / `-` to place it, and the `ms/wrist` numbers
become `WristCutA` / `WristCutB`.

Compare against `ms edge 1` to see what the clip is worth - that is the best of
the whole-triangle rules and should still show a notched edge.

## PREVIOUS (2026-09-06, session 20b): VR-31 - the cut is a PLANE across the forearm

The bone-influence split works and was confirmed in the headset: both hands
present, both arms gone, at `WristScale` 0.70 on both sides (radius 21.7,
2109 hand triangles per arm). Two faults in the SHAPE of the cut, both fixed
here and neither yet tested.

### The sphere moved in whole bones

The tester's walk is in the log and it is unambiguous: from scale 0.50 to 0.67
the triangle counts did not change AT ALL, then one press flipped 156 triangles
per arm at once. A bone is inside the sphere or outside it, so every triangle
that bone dominates changes class together. What is left is the outline of a
bone's influence region, which is why moving the knob made the edge jagged
rather than moving it.

### A plane cuts a cylinder in a circle

`MsPlaneDerive` per arm:

1. **The limb axis** by power iteration on the covariance of the arm's triangle
   centroids. Two passes - a rough axis over the whole arm, then a refined one
   over a band around the rough cut, so the ring is perpendicular to the
   FOREARM and not to the whole limb with the hand's mass pulling on it. The
   refinement logs how many degrees it moved, and refuses past 40.
2. **The starting position** keeps exactly the number of triangles the sphere
   was keeping at the tester's 0.70. Changing the shape of the cut does not
   move it: the look that was settled on survives, it just stops being blobby.
3. **The knob moves the plane in LENGTH** - 2% of the arm per press - so the
   ring travels smoothly instead of waiting for a bone to flip.

The sphere is kept, not deleted: it is the seed, the fallback
(`ms shape sphere`), and the thing that proved the classification works.

### New keys and defaults

`[Hands] WristPlane=1` (plane), `WristScaleA/B=0.70` (the tester's measured
seed), `WristEdge=0`, `WristCutA/B` unset (derive). Setting `WristCutA/B` pins
the ring in mesh units from the hand bone and survives a level load - the knob
and `ms cut` both print exactly that number, so a good look becomes the default
without another walk.

`ms cut <a> [b]` places the ring, `ms shape plane|sphere`, `ms edge 0|1|2`
(kept when the centroid / all three vertices / any vertex is past the plane).

### The run this needs

Look at the hands: the starting picture should be the SAME amount of forearm as
the tester left it, but ending in a clean ring instead of a jagged edge. Then
`Numpad +` / `-`: the ring should slide smoothly, a small step each press, with
no chunk of triangles vanishing at once. When it looks right, read the
`ms/wrist` line for the two numbers and they become `WristCutA` / `WristCutB`.

If the ring comes out slanted rather than square across the forearm, the
`forearm axis refined N degree(s)` line says how far the second pass moved and
is where to look. `ms edge 1` is the alternative if the one-triangle sawtooth
is visible up close.

## PREVIOUS (2026-09-06, session 20): VR-31 - the cut is DERIVED from bone influence, per arm

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open). Installed
Release. **Untested - built and installed, not run.**

### What replaced the eyeballed mask

The 32-slice mask cut by PERCENTAGE of the triangle order, and the two arms sit
in different, non-aligned regions of that order, so a mask tuned on the left
hand took too much of the right. No amount of further eyeballing fixes a cut
that cannot express the shape.

`src/game/dishonored/hands/mesh_split.cpp` classifies each TRIANGLE by the
bones that actually move it, which is per-arm by construction:

1. **Read once.** The draw's index range and vertex window are copied out of
   the game's own buffers - the first time this project has read them - and
   VALIDATED against facts the data must satisfy (indices inside the draw's own
   vertex window, bone indices below the palette size, weights summing to 1,
   finite positions, a non-degenerate bbox). A `D3DUSAGE_WRITEONLY` buffer may
   legally hand back an uninitialised page on a read lock; that is why the
   descriptors are logged BEFORE the lock and why a failed validation refuses
   instead of carving the mesh up from noise.
2. **Two arms from the SKINNING GRAPH.** Two bones are adjacent when some vertex
   is meaningfully moved by both. Separate limbs share no vertex, so the graph
   falls into two components - no bone names, no hierarchy, no engine
   structures. A geometric widest-gap split is the fallback and says so in the
   log when it is used.
3. **The wrist from the bone SPACING.** The hand bone is the one with the most
   neighbours (a forearm joins two things; a hand joins the forearm and every
   finger). Sort the side's bones by distance from it and the biggest gap in
   that list is the wrist. Derived, not eyeballed.
4. **Per triangle**, the class holding most of its influence weight wins, so a
   triangle straddling the wrist follows the bones that move it. Stray sleeve
   shards go with the arm bones they are weighted to, which is why they should
   disappear without a special case.
5. **One index buffer of our own** (`D3DPOOL_MANAGED`, so `hkReset` has nothing
   to release), classes emitted contiguously with both hands adjacent - so
   "both hands, no arms" is ONE draw call, not a run per fragment.

### Also fixed: the stale-lock weakness

The mesh lock is now **auto-armed by the mesh's measured signature** (prims
4448, verts 2771, skinned declaration, triangle list - all ini keys), and
`DcTick` releases an automatic lock that has not been drawn for 300 frames so a
level load that recreated the buffers re-arms on the new pair. An automatic lock
**fails soft**: a mesh it cannot split is drawn exactly as the game asked, on
both the indexed and non-indexed paths. A hand-armed lock (Numpad 6) is
unchanged and still drops what it is told to.

### Defaults and controls (everything ships ON)

`[Hands] ArmSplit=1 ArmSplitAuto=1 ArmSplitMode=1` (mode 1 = HANDS).

    Numpad 0   next mode: hands -> all -> arms -> other -> off -> hands
    Numpad +   keep MORE as hand (the cut moves up the arm)
    Numpad -   keep LESS as hand (the cut moves toward the fingers)
    Numpad *   which arm + / - moves: both -> side A -> side B
    Numpad /   re-derive the whole split from the buffers
    Numpad 5   release everything, and stop the automatic re-arm

Seam: `ms status | off | hands | arms | all | other | rebuild | wrist <n> |
side <n>`. The 32-slice mask and `dc mask s19` are untouched and remain the
fallback if a driver refuses the read.

### The run this needs

Load a save with the sword and crossbow out and read `dishonored_vr.log` for
`ms:`. Expected: `ms: ib fmt=... usage=...`, then a validation line with zeros,
then two graph components, two wrist radii, and a class count with BOTH hand
classes non-zero. In the headset both hands should be present and both arms
gone, with no percentage tuning at all. `Numpad 0` once shows the whole mesh
through our buffer (the A/B - it must look exactly like stock); a second press
shows the inverse cut, arms with no hands, which is the cheapest proof the
classification is real and not a coincidence.

If a hand is too short or too long, `Numpad +` / `-` moves that wrist and the
log prints the radius - ONE number per arm instead of 32 slice bits.

### Still not done

- The acceptance list: Blink, Devouring Swarm, Windblast, weapons, head and
  stick turns, crouch, reload, checkpoint reload. Power effects are
  socket-driven, so verify the SOCKETS follow, not just the fingers.
- `HmDrawIntoEye` asks the METHOD whether it wants a projection layer, not
  whether the RUNTIME submitted one.
- `FindRefSkel` does not enforce the validation its comment claims.
- Controller wrist placement is still a separate gate; `[Hands] Enabled=0`.

## PREVIOUS (2026-09-06, session 19): VR-31 - FLOATING HANDS WORK, the cut needs per-arm ranges

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open). Installed
`alpha-316`. **No PR opened - the user asked for commit and push only.**

### The result: the game's OWN hands, floating, with their own animation

Headset-confirmed twice with screenshots: Corvo's real hand holding the sword,
and the real hand holding the crossbow, with the arm gone from the cuff. This is
Arkane's geometry, Arkane's skinning and Arkane's animation - nothing is
replaced - so the powers animate the fingers exactly as they always did, which
was the whole requirement.

### How it works, and why it needs no index buffer

The first-person arms and hands are ONE skinned mesh (`bones=48 skin=IW`, 4448
triangles, 2771 vertices) drawn by three vertex shaders. The mesh is a triangle
LIST, so any contiguous run of its triangles can be drawn on its own by shifting
`startIndex` and shrinking `primCount`. **Nothing is read, captured or
replaced** - the game's own buffers stay bound and we ask for part of the list.

The target is identified by BUFFER PAIR (stream-0 VB + IB), not by bone-palette
size, so every pass over it is caught including ones the palette-gated census
never saw. `DrawPrimitive` is hooked as well as `DrawIndexedPrimitive`.

### THE MEASURED CUT, and its limitation

The tester's mask, 12 of 32 slices: **`0x3001D237`**

    slices  1,2,3   triangles  0%.. 9%
    slices  5,6     triangles 12%..18%
    slice  10       triangles 28%..31%
    slice  13       triangles 37%..40%
    slices 15,16,17 triangles 43%..53%
    slices 29,30    triangles 87%..93%

Restore it with **`dc mask s19`** instead of repeating twelve headset presses.

**It is NOT the default and must not become one yet.** It was tuned for the
LEFT hand and takes too much of the right. The reason is structural and is the
next problem to solve: **the two arms occupy different, non-aligned regions of
the one triangle list**, so a single set of slices tuned by eye on one hand
cannot be correct for the other. The slices are also PERCENTAGES of this mesh's
primCount, so the mask means nothing on a different asset or LOD.

### Next steps

1. **Two independent masks, one per hand.** The cut has to be chosen per arm,
   because the arms are not symmetric in the triangle order. Either two masks
   the tester tunes separately, or - better - classify triangles by BONE
   INFLUENCE so the cut is derived rather than eyeballed.
2. **Finer than 32 slices** where a slice straddles hand and arm. 32 was enough
   for the left hand and not obviously enough for the right.
3. **Sleeve shards.** Small black triangles survive the cut (visible in the
   session screenshots); they are weighted to arm bones and sit outside the
   marked runs.
4. **Then the acceptance list** from review: Blink, Devouring Swarm, Windblast,
   weapons, head and stick turns, crouch, reload, checkpoint reload. Power
   effects are socket-driven, so verify the sockets follow, not just the
   fingers.
5. **Controller wrist placement is still a separate gate** and untouched;
   `[Hands] Enabled=0`.

### Known weaknesses in the instrument, deliberately not fixed

- The mesh lock is armed from a censused row, so a level load that recreates the
  buffers leaves it stale and silently drawing everything. Needs a re-arm path
  before this is a shipping lever rather than a diagnostic.
- `HmDrawIntoEye` asks the METHOD whether it wants a projection layer, not
  whether the RUNTIME submitted one.
- `FindRefSkel` does not enforce the validation its comment claims.

## PREVIOUS (2026-09-06, session 19): VR-31 - the cycler is WALKED, the hands had an uninitialised matrix

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open, so this PR takes
`Ref VR-31`). Installed build `alpha-308`.

### VR-31's question is ANSWERED: route (d) cannot give hands

The cycler was walked in the headset on `alpha-307`. Four presses of Numpad 3
removed, in order: **both arms and hands together**, sword, crossbow, bolt -
exactly the plan the engine's own `GetNumElements` produced. Attribution is now
MEASURED, not recalled.

So `Skm_Player` carries arms and hands as ONE material section, route (d) gives
**floating weapons**, and it can never give floating hands.

**Note the limit of that conclusion** (raised in review, and it is right): one
shared material section rules out a MATERIAL-only split. It does not prove our
own hands are the only possible route - preserving Dishonored's original
animated hands would need a different geometry-FILTERING approach (route (b),
the c6 bone palette, x144 = arms, is untouched and is where that would start).
Custom hands are the route being taken, not the only one that exists.

### The hands drew every triangle and showed nothing - the cause is found

First run of the rebuilt hand pass: `calls=480 draws=480 tris=120960 last=drew`,
252 triangles per present (both models, complete), nothing on screen.

**Cause, found by code review rather than another run:** the transform did
`float B[9]; RtdBuildYPR(rad, B);` and `RtdBuildYPR` is compiled ONLY under
`-DDVR_WITH_LEGACY=ON`. Every shipped build takes the empty stub in
`src/legacy/legacy_stubs.inc`, so `B` was never written and every hand vertex
was rotated by nine floats of stack garbage. The counters stayed healthy because
the geometry really was built and submitted; and a NaN passes the near-plane
reject, because `NaN > -0.03f` is false.

Fixed in this build:

- **`HmBuildYPR`, local to `hand_mesh.cpp`** - the twelve lines needed, not the
  legacy subsystem. Y-up controller convention (yaw about Y, pitch about X, roll
  about Z), so the ini keys mean what they say. Verified numerically: exact
  identity at zero trim, equal to `Ry*Rx*Rz` to 2.2e-16 over 64 combinations.
  Inert in the current configuration - every shipped trim value is 0.0.
- **`submitted` and `ON-SCREEN` are different numbers now.** The beat also counts
  `nonFinite`, `nearPlane` and `degenerate` (zero projected area draws no pixel
  and has a perfectly ordinary bounding box) and prints the NDC bounding box, so
  "invisible" resolves to an axis and a magnitude.
- **Real depth.** `pos.z = 0.5f * (-z)` over `w = -z` divides to exactly 0.5 at
  every distance, so the depth buffer could order nothing. Standard mapping now.
- **The calibration triangle** (`[VRHands] CalibTriangle`, `vrhands calib on`),
  default OFF: one fixed-NDC triangle through the same shader, buffer, states
  and target. The FALLBACK if the hands are still invisible once the geometry is
  sound - and its outcomes are not fully exclusive, so the beat's counters, not
  it, are the first read.

**How far the bug class extends, measured:** of 23 stubs in `legacy_stubs.inc`,
exactly one had an out-parameter a live caller read unconditionally, and it is
this one. `RtdSnapshot` also has out-parameters but returns false and its caller
gates every read on that. The other 21 are void no-ops, as intended.

### Still open, deliberately not fixed in this build

- **The projection guard reads the wrong thing.** `HmDrawIntoEye` asks the
  METHOD whether it wants a projection layer, not whether the RUNTIME submitted
  one. It did not misfire on this run. Fixing it means adding an accessor to
  `openxr_runtime.cpp`, the file this project keeps closest to the BioShock
  copy, so it is not being done mid-diagnosis.
- Hiding the game's arms once ours are visible should use the **proven
  material-section hide on `Skm_Player`**, not `[VRHands] HideGameArms` - that
  flag's upload-size filters also target weapons and carry static-geometry
  heuristics, so it is not equivalent.

### The requirement changed: the REAL hands, because the powers animate them

Custom meshes cannot carry Arkane's power animations, so the working custom
renderer is now the FALLBACK and route (b) is the goal. Full plan and the nine
review corrections are in ENGINE_NOTES, "route (b) starts at the DRAW".

**Step 1 is built and installed: the DRAW census** (`hands/draw_census.cpp`,
`[Hands] DrawCensus`, ships ON). Upload SIZE cannot settle an arm/hand split -
two chunks can share a size, several draws can reuse one upload, and `HideSizes`
only contains what someone already saw. So `DrawIndexedPrimitive` is hooked
(vtable 82) and every palette-fed draw is identified by what the renderer held:
stream-0 VB and stride, IB, vertex declaration, vertex shader, index range,
primitive count. **Numpad 6** next, **4** back, **5** all visible - one row
hidden per press, the log names it. `dc report|status|hide <n>|show` on the seam.

Two rules it observes: no engine D3D reference survives the detour (each `Get*`
AddRefs and is released on the next line, pointer kept as an identity token
only), and the hotkey posts a request that the tick acts on - the detour itself
runs on the RENDER thread.

### Next steps

1. Walk the draw cycler (Numpad 6). Two rows sharing a bone count but differing
   in VB/IB or index range are separate geometry a size-based hide cannot tell
   apart - that is the question. If one row is arms-without-hands, route (b) is
   a draw skip and it is nearly done.
2. If arms and hands share one draw: collapse-to-wrist as a cheap prototype -
   with the wrist point derived in the palette's OUTPUT space, NOT read off a
   translation column (skinning matrices fold in the inverse bind pose), and a
   ZERO 3x3 linear part, not identity.
3. If that seams: the index-buffer filter - a replacement index list of hand and
   cuff triangles only, original vertices, weights and animated palette kept.
   Survives arms and hands sharing both material and chunk.
4. Controller wrist placement is a SEPARATE gate; `[Hands] Enabled=0` today.
5. Read the hands beat line. `ON-SCREEN=0` with `submitted>0` is a geometry fault
   and the NDC box names the axis; `nonFinite>0` means the transform is still
   producing NaN; healthy counters with a blank screen means arm the calibration
   triangle.
2. Then hide `Skm_Player` by material section for the floating-hands verdict.
3. Floating weapons as a shipping lever (hide `Skm_Player`, default OFF, live
   A/B); check the shadow and Blink's aim.

## PREVIOUS (2026-09-06, session 19): VR-31 - our own hands are WIRED (unverified)

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open, so this PR takes
`Ref VR-31`, not `Fixes VR-31`). Installed build carries the hand wiring.

### The cycler is installed and has NEVER been run

`alpha-305-g23ef0ae5` (the cycler commit) is the installed proxy - verified by
reading the build tag out of the installed `d3d9.dll`. The newest log on disk is
`alpha-304-ge1135d25`, the run that proved route (d) with the timed sweep. So
**the walk needs no build; it needs a headset session.**

The plan the cycler builds is deterministic and already on that log, so the
recording sheet can be written now. Four positions, Numpad 3 stepping forward:

| position | component | what the engine calls it |
|---|---|---|
| 1 | `Skm_Player` | the first-person body, section 0, LOD 0 |
| 2 | `Wpn_PlySword01` | the sword |
| 3 | `crossbow_01` | the crossbow |
| 4 | `bolt_01` | the loaded bolt |

Numpad 1 steps back, Numpad 2 restores everything. Each press logs its position
and component, so the walk only needs the tester to say what vanished at each
one.

**One thing the census already settles, so the run is not spent on it:**
`Skm_Player` is the ONLY component carrying first-person body geometry. Arms and
hands cannot be on separate positions - there is nowhere else for them to be.
The question the walk actually answers is position 1: do both arms AND both
hands go, and do the weapons keep drawing? (ENGINE_NOTES, "What the cycler can
still settle".)

### Our own hands: the caller was the missing piece, and it is back

`core/gfx/hand_mesh.cpp` has drawn nothing since 41.0. `HmRenderEye` and
`HmEnsurePipeline` lost their only call site when the side-by-side pipeline was
deleted (`cc2fa936`), so `[VRHands] Enabled=1` changed nothing on screen and
logged nothing about it. Rebuilt this session:

- a `HandDrawFn` seam beside `OverlayDrawFn` in `core/gfx/stereo.h`; the active
  method calls it between the game image and the F10 panel, passing **the eye
  tag of the pixels already in the target** (not the eye the next game draw
  renders - one line apart in `reentry::end_frame`);
- the pass's own D32 depth buffer, sized to the method's output and cleared per
  draw. The painter's sort is gone; without a bound DSV the depth state is inert
  and the back of a hand paints over its front;
- **the frustum corrected to the projection layer's CLAIM** (the engine's
  rendered hfov, derived the way the runtime derives its own half-angles)
  instead of the headset's raw half-angles - 108.1 deg against the headset's
  numbers on the last measured run, and a slide against the world that grows
  with the gap;
- a refusal on the mono screen that says which rung would work, because a silent
  skip and broken hands look identical in a log;
- a 3 s beat printing calls / draws / triangles that **names the reason for any
  zero**, and `handModelTris` / `handModelWhy` in `status.json`.

**Ships ON** (`[VRHands] Enabled=1`) for the test sessions - the same deliberate
exception as `[Device] ShadowFullCopy` and `[Hands] BoneVisHide`, so a run shows
the hands without anyone sending a seam word first; it reverts to OFF when the
verdict is recorded. `vrhands on|off|status` is the live A/B.

**`[VRHands] HideGameArms` ships OFF with it, on purpose.** It collapses the
game's own view-model rigs by upload size (the 30.77 vs-const path) - a SECOND
behavioural change in the same build as the first draw of ours, against the
author's one-change-per-build rule. It also makes run 1 diagnostic instead of
pass/fail: with the game's arms still drawn, they are the reference our hands
are judged against. If our hands land right, that flag is the whole remaining
step to floating hands.

**UNVERIFIED - nothing here has been seen in a headset or on the simulator.**

### Next steps

1. Walk the cycler (4 positions above) and record position 1's answer.
2. Run 1 of the hands: nothing to switch on. Read the beat line. Expect
   the first run to need trim: `[VRHands] Scale`, the per-hand `PosX/Y/Z` and
   `Yaw/Pitch/Roll`, all already in the ini, none of them exercised since 30.x.
   The overlay panel for them was deleted in 31.5 and would have to come back if
   trimming by ini proves too slow.
3. Floating weapons as a shipping lever: hide `Skm_Player`, default OFF, live
   A/B, and check what it does to the shadow and to Blink's aim.
4. Routes (b) and (c) are not needed for either. (a) is closed.

## PREVIOUS (2026-09-06, session 18): VR-31 - route (d) WORKS, floating weapons are in reach

Branch `claude/vr-31-arm-hiding-floating-hands`, based on
`claude/vr-30-decouple-arm-hand-movement` (PR #18 still open, so this PR takes
`Ref VR-31`, not `Fixes VR-31`). Installed build `alpha-305`.

### PROVEN, headset-judged: ShowMaterialSection hides first-person geometry

Four automatic steps, geometry disappeared and returned on each. **Route (d)
works.** Full record in `ENGINE_NOTES.md`, "SOLVED: route (d) WORKS".

The plan the engine's own `GetNumElements` produced:

| step | component | sections |
|---|---|---|
| 1 | `Skm_Player` (the first-person body) | 1 |
| 2 | `Wpn_PlySword01` | 1 |
| 3 | `crossbow_01` | 1 |
| 4 | `bolt_01` | 1 |

**Every component has exactly ONE material section.** Route (d) therefore hides
per COMPONENT, not per body part.

- **This gives floating weapons now**: hide `Skm_Player`, and the sword,
  crossbow and bolt keep drawing as separate components.
- **It cannot give floating hands**: arms and hands share one section, so
  hiding the arms hides the hands too. No finer material cut exists in the
  asset, and none can be made from outside it.

**The tester's per-step attribution is NOT yet established.** The recollection
of which step removed which limb was approximate. The table above is the PLAN,
not what was seen. The numpad cycler exists to settle it deliberately.

### The cycler

`[Hands] MatCycle=1`, ships ON. **Numpad 3** next, **Numpad 1** back,
**Numpad 2** everything visible. Each press logs the component, section and LOD
it hid. The hotkey only posts a request; every dispatch runs on the SCRIPT
lane in `MatCycleTick`, because ProcessEvent from the present thread is the
lane error this project has a rule about. The timed sweep (`[Hands] MatAuto`)
is back OFF now the route is proven, so the two cannot fight.

### Next: floating hands needs a different SOURCE for the hands

Not a finer cut of the game's mesh - that split does not exist. The mod already
has its own: `core/gfx/hand_mesh.cpp` draws hand geometry and the SkelControl
drive already places hands from the controllers. Hide `Skm_Player`, draw our
own hands. That is the BioShock shape reached from the opposite direction, and
it also answers the powers requirement - our own hands can be shown for powers
and hidden otherwise without touching the game's mesh at all. Untested.

### Next steps

1. Walk the cycler and write down which component is which limb, so the
   attribution is measured instead of remembered.
2. Floating weapons as a shipping lever: hide `Skm_Player`, default OFF, live
   A/B, and check what it does to the shadow and to Blink's aim.
3. Floating hands: `hand_mesh.cpp` + the SkelControl placement, with a
   visibility policy that shows hands for powers.
4. Routes (b) and (c) are not needed for either of the above. (a) is closed.

## PREVIOUS (2026-09-06, session 18): VR-31 - route (a) closed, route (c) found

Branch **`claude/vr-31-arm-hiding-floating-hands`**, now cut from
**`claude/vr-30-decouple-arm-hand-movement`** (rebased 2026-09-06, session 18).
Linear **VR-31** is In Progress.

### The base, and what that means for the PR

This branch **carries the VR-30 fix** and is safe to build and install from. It
is based on the VR-30 branch, not on `VR-Main`, because VR-30 is still open in
**PR #18**. So:

- the PR body's first line is `Ref VR-31`, not `Fixes VR-31` - only the PR that
  reaches `VR-Main` closes the ticket, and this one will not until #18 lands;
- **PR #18 merges first.** After it does, this branch rebases onto `VR-Main` and
  its PR retargets there.
- The tester's ini carries `[Camera] ArmBodyFacing=1` (the VR-30 fix, on),
  `ArmStripMeshRot=-1` and a leftover `BodyYawLock=-1` whose key no longer
  exists in this code and is ignored.

### VR-30 is DONE (2026-09-05, session 17)

Headset-judged: head yaw leaves the arms and weapon alone, the right stick turns
view and body together, both at once works. `FaceRotation` is virtual at vtable
slot 252 (`0x00AB0D40`); the mod replaces the **Yaw it is asked for** with
`view - our own injected head contribution` and lets the engine's own function
run. Measured 3365 replacements, 0 stale, `perf: tick` unchanged at 11.3-11.8 ms
against an 11.11 ms budget. Full record in `ENGINE_NOTES.md`, "SOLVED: VR-30".

### VR-31: the question, and where the research already is

**The deliverable is a verdict, not a feature**: floating hands via the
bone-density trick, or floating weapons with hands only for the powers.

**The 30.13 result is NOT missing.** Session 18 found it. The original author
recorded it in a code comment in the state chunk beside the experiment
(`src/mod/state/40_game_dishonored_hands_arms_hide.inc`, and in the original
single file at `git show 824e08d8:src/dllmain.cpp` line 11119), which is why
three passes over the corpus called it unrecorded:

    30.14: the +0x288 byte poke was wrong - that array turned out to be some
    per-bone animation control (arms froze to the view and rode the head).

So the write was made, and it had a visible effect that was not hiding. The
probable reason: `0xFF` is not a legal `BoneVisibilityStates` value, but it is
exactly `SkelControlIndex`'s "no control on this bone", so 30.13 most likely
attached the arm bones to SkelControl #2 rather than hiding them. That also
weakens the existence proof route (a) rested on - the game may not be hiding
`spine_3_jnt` at all, it may be pointing it at a controller. Full record in
`ENGINE_NOTES.md`, "The 30.13 bone-visibility result was recorded all along".

**What session 18 built instead of repeating it.** The offsets do not have to
be guessed: this build ships UE3 reflection (`FindPropOffset` reads
`UProperty::Offset` out of GObjects, as the crouch cylinder and the graft
already do). `arms vis on|off|status|chain`, ini `[Hands] BoneVisHide`, **ships
ON for the test sessions** (a deliberate exception to "every lever ships OFF",
the same call as `[Device] ShadowFullCopy`, so a run prints the census without
anyone sending a seam word; it reverts to OFF when the verdict is recorded):

- asks the engine where `BoneVisibilityStates`, `SkelControlIndex` and
  `RequiredBones` really live and prints all three against `0x288`, naming
  which array 30.12 actually found;
- writes `BVS_ExplicitlyHidden` into the arm chain at the offset the engine
  named (never hands or fingers), saving and restoring the exact bytes;
- refuses, with numbers, if the property does not exist, if the array is empty
  (`num=0` - the engine never allocated it, which closes route (a) with a
  reason and is the same fact 30.17 saw), or if its length is not the rig's
  bone count;
- runs a write-survival census every 2 s while on - `held` / `reverted` /
  `other` - so "the write did not survive" and "the write survived and the
  renderer ignores this array" stop looking identical in a headset.

Route (b), the c6 bone palette (**x36 = sword, x144 = arms, x204 = NPC**,
machinery in `src/legacy/rtd_drive.cpp`), is untouched and is where an all-held
census sends the question next.

### VERDICT (2026-09-06, runs 1-4): route (a) is CLOSED, and route (c) is new

Four runs, all read-only, all with the arms staying visible - which was the
correct result each time: the lever refused to write and said why.

**Route (a), per-bone visibility, is closed on three independent instruments:**

1. `BoneVisibilityStates` does not resolve on `SkeletalMeshComponent`
   (`RequiredBones` resolved at `+0x23c` in the same call, so the resolver was
   working and the `0` is a real absence);
2. no `UProperty` of that name exists on **any** class in GObjects, so it is
   not hiding on a subclass;
3. of the five per-bone byte arrays on the rig, none has its values confined
   to `0..2`, which is what a visibility array must look like.

**`+0x288` is `SkelControlIndex`, measured.** 30.13 pointed the arm bones at
SkelControl #2 rather than hiding them, which is exactly the symptom 30.14
recorded. The existence proof route (a) rested on is gone with it: the game is
not hiding `spine_3_jnt`, it is pointing that bone at a controller. And 30.17
is consistent with it - `HideBoneByName` allocating nothing is what a missing
array would produce - but the native's implementation was never inspected, so
that is the likeliest reading and not a demonstrated cause.

**Route (c) appeared in the same run.** `SkelControlIndex` on the arm chain:
**8 of 10 bones read `255` (free)**, and the 2 that are taken are exactly the
two collarbones, driven by controls **0** and **1**. One byte per bone, and the
mod already drives SkelControls. What those two controls are is not yet known -
the mod's own control enumeration was off this run (`[Hands] Enabled=0`), so
naming them is one cheap run with the hand drive on.

Full record, including the scan's element-size caveat, in `ENGINE_NOTES.md`,
"VERDICT (2026-09-06, run 4)".

### The three routes, as they now stand

| route | state | cost | unknown |
|---|---|---|---|
| (a) per-bone visibility | **CLOSED**, three instruments | - | none, it is finished |
| (b) c6 bone palette | **OPEN**, proven writable | the hottest D3D9 entry point | none material: x36 = sword, x144 = arms, x204 = NPC, machinery in `src/legacy/rtd_drive.cpp` |
| (c) `SkelControlIndex` | **OPEN**, new | one byte per bone | what a free bone can be pointed AT - the index selects from the AnimTree's control lists |

**The lever is back to default OFF** now the verdict is recorded. What is kept
is the diagnostic that closed the route: `arms vis status` re-runs the whole of
it on demand, `arms vis chain` prints the arm chain.

### Next steps

1. **The route decision is the user's**, and it is the real open question. (b)
   is the safe one - proven writes, mesh separation already measured, and the
   only doubt is frame cost on the D3D9 hot path. (c) is the cheap one and the
   more interesting one, but it needs the AnimTree control list understood
   before a single byte is written, and 30.13 is the standing warning about
   pointing bones at a control somebody else owns.
2. Either way, one cheap run with `[Hands] Enabled=1` names SkelControls 0 and
   1 (and settles whether #2 is `LookAtControl_Camera`, which would close the
   30.13 story completely).
3. PR is not open yet. It takes `Ref VR-31`, not `Fixes VR-31`, while the base
   is the VR-30 branch.

### Session log

**2026-09-06, session 19 (part 3)**: floating hands WORK, using the game's own
hands. Built the draw census, then the mesh lock (buffer-pair identity, both
draw entry points), then live triangle-range slicing - the mesh is a triangle
list, so sub-ranges of the game's own index buffer can be drawn without reading
or replacing anything. Two headset screenshots confirm real hands with the arms
gone. The tester's cut is `0x3001D237`, restorable with `dc mask s19`; it is not
a default because the two arms sit in non-aligned regions and it was tuned on
the left. Also found and fixed: single-matrix draws were saturating the census
table, and a full table silently disabled suppression - which was the whole
explanation for the faint arm that survived earlier hides.

**2026-09-06, session 19 (part 2)**: the cycler walk came back and matches the
engine's plan exactly - arms+hands together, then sword, crossbow, bolt. VR-31's
route (d) question is answered. The hands showed nothing despite healthy
counters; review found `RtdBuildYPR` is a legacy-only symbol that resolves to an
empty stub in every shipped build, so the hand transform ran on an uninitialised
matrix. Replaced with a local `HmBuildYPR` (verified identity at zero and equal
to Ry*Rx*Rz to 2.2e-16), separated `submitted` from `ON-SCREEN` in the beat,
fixed the constant-0.5 depth, and added a calibration triangle as the fallback.
Swept all 23 legacy stubs: exactly one was unsafe. Corrected an error in part 1's
write-up - the stereo tagging claim was read off bring-up beats and gameplay is
properly tagged.

**2026-09-06, session 19**: no headset run. Established that the cycler build
(`alpha-305-g23ef0ae5`) is the INSTALLED proxy and has never been run - the
newest log on disk is `alpha-304`, the timed-sweep run - so the walk needs a
session, not a build, and wrote the 4-position recording sheet from the census
already on that log. Then found that `core/gfx/hand_mesh.cpp` has been dead code
since 41.0: `HmRenderEye` and `HmEnsurePipeline` lost their only call site when
the side-by-side pipeline was deleted in `cc2fa936`, so `[VRHands] Enabled=1`
changed nothing and said nothing. Rebuilt the caller as a `HandDrawFn` on the
stereo seam, gave the pass its own depth buffer, corrected its frustum to the
projection layer's CLAIM rather than the headset's half-angles, made the mono
screen refuse out loud, and added a beat that names the reason for a zero.
Builds clean, lint clean, exports OK, installed Release. **Nothing verified** -
no headset, no simulator run.

**2026-09-06, session 18**: opened. Branch cut, VR-31 moved to In Progress,
research located and extracted. Found that the 30.13 experiment's result was
recorded after all and that `+0x288` is very likely `SkelControlIndex`, not
`BoneVisibilityStates`; corrected two dead-end entries in `ENGINE_NOTES.md` and
added the record. Built the reflection-resolved route (a) lever and its
write-survival census, default off. Builds clean, lint clean, exports OK.
Rebased onto the VR-30 branch and installed Release. **Four tester runs**:
route (a) is closed on three instruments, `+0x288` is `SkelControlIndex` by
measurement, this build has no `BoneVisibilityStates` anywhere, and the arm
chain is 8-of-10 free in `SkelControlIndex` - which is route (c). Two of the
four runs were spent on instrument preconditions I got wrong (the scan needs a
live rig, and the candidate list does not exist unless the hand drive is on);
both are recorded in ENGINE_NOTES so the next instrument does not repeat them.
Lever back to default OFF, diagnostic kept behind `arms vis status`.

**2026-09-05, session 17**: VR-30 solved and closed. PR #18 open against
`VR-Main`, unmerged. Also filed **VR-52** (place the hands absolutely from the
controllers) with the anchor bug and the acceptance direction recorded, since
that is a separate problem from this one.

## PREVIOUS (2026-09-05, session 17): VR-30 IS SOLVED - FaceRotation is the seam

Branch **`claude/vr-30-decouple-arm-hand-movement`**, cut from `VR-Main`.

**Headset-judged fixed.** Head yaw leaves the arms and weapon alone, the right
stick turns view and body together, and both at once works. That is VR-30's full
acceptance, both halves.

### The fix, in one paragraph

`FaceRotation` is what faces the body to the view. It is **virtual at vtable slot
252** and resolves to **`0x00AB0D40`** on this build. The mod hooks it and
replaces the **Yaw it is asked for** with a separated body heading
(`view - our own injected head contribution`), then lets the engine's own
function run so its dependent bookkeeping still happens. Pitch, roll, delta time
and every other actor pass through untouched. `[Camera] ArmBodyFacing`, ships
**OFF**, `arms facing 1|off` is the live A/B. Measured: **3365 replacements, 0
stale**, 1014 calls on other pawns untouched.

Full derivation, the falsified attempts with their numbers, and the identity bug
are in `docs/dishonored/ENGINE_NOTES.md`, "SOLVED: VR-30".

### What was removed, and why

- **`[Camera] BodyYawLock` is gone.** It wrote the pawn's `Rotation.Yaw` every
  dispatch and was measured futile: **4 of 186 writes survived** to the next
  dispatch. Its target was correct; the engine simply re-derives the pawn's
  heading from the controller every tick. Users with the key in their ini can
  delete the line; it is ignored.
- **The per-dispatch `FaceRotation` name match is gone** from `PeHandler`. Its
  question is answered (0 dispatches across a full run - the route is
  native-to-native) and the answer is in ENGINE_NOTES.

### What is kept as instruments

- `armfollow/yaw:` - the four-way yaw census (head, controller, pawn, view with
  per-second deltas). It found the stale-controller bug and the write-survival
  number. Now on the 1-in-128 slow path.
- `armfollow/nfp:` - the one-shot native-facing probe that derived the vtable
  slot. Costs nothing after it fires.
- `[Camera] ArmStripMeshRot` - ships OFF, kept as the reproducible A/B for a
  documented dead end (304,244 writes, no effect).
- `tools/yawtest-host.ps1` - compiles the yaw bookkeeping out of `head_track.cpp`
  **verbatim** and runs its seven cases on the host. No game, no headset.

### Performance

`perf: tick` on the winning run reads 11.3-11.8 ms against an 11.11 ms budget at
90 Hz, unchanged from before the branch. The hook adds a handful of integer
compares to a function that runs about once per frame per pawn. The census's
clock read was moved to the slow path (c5ecea34's rule); the answered
per-dispatch name match was deleted.

### Next

VR-30 is done. The hands are still placed by the engine - **absolute controller
placement is a separate ticket**, and `ENGINE_NOTES` records that SkelControl
world-space translation is absolute (attempt 5's pin test) and that the world
block's anchor is wrong: `camera+0x80` is not a position (DISCARDED 120/120),
`camera+0x330` is.

## SOLVED (2026-09-05, VR-15): the black texture bug was a MIP fault, and it ships fixed

PRs #14 (the crouch fix) and #15 (the 90 Hz defaults) are **merged to `VR-Main`**. Branch
`vr-15-black-texture` carries the fix, headset-judged by the tester.

**The fault**: a surface was largely black far away and correct up close, the black
receding as the player walked in. Distance selects the MIP LEVEL, so the bad data was in
the small mips and level 0 was fine.

**The cause**: the managed-pool shadow pushed every write with `UpdateTexture`, which
**takes no level**, and writes to levels above 0 were not being carried. `shadow_unlocked()`
was not even given the level - the unlock hook had it in hand and dropped it - so no
instrument could have seen a per-level fault. The 2026-09-04 log had the corroboration
sitting in it unread: **`level>0=50189`** locks in one load with **`dirtyRects=0`**.

**The fix**: `[Device] ShadowFullCopy` pushes exactly the level the unlock wrote, with
`UpdateSurface`, which names its two surfaces and cannot be vague about which level it
copied. It **ships ON** - a deliberate exception to "every render lever ships OFF", the
same call as `[Stereo] HoldUntagged`, because OFF is a visible rendering bug.
`device shadowfullcopy off` restores the fault and is the A/B.

### The frame-rate cost, and what was done about it

The first cut made the frame rate less stable. Three causes, two of them **older than this
session's work**:

1. **The push ran on READONLY unlocks** - 12408 of them per load, each a whole-texture GPU
   copy for a lock that wrote nothing. Now skipped; the lock kind is recorded in the
   lookup the lock hook already had to do, so it costs nothing to know.
2. **A refused `UpdateSurface` was re-attempted on every unlock forever**, paying for the
   failure and the fallback both. A refusing texture is now remembered.
3. **The 60 s upload census walked all 32768 twin-map slots on the present thread** just to
   decide whether to print. Self-inflicted this session; the decision now reads counters.

`shadow_unlocked` also took its critical section twice per unlock and now takes it once.

**Unmeasured**: nobody has put a number on the improvement. `perf: tick` before and after
is the measurement, and the `device/upload` line's "work skipped" count says whether the
READONLY skip is firing at all (a 0 on a loaded level means it is not).

### Still open on this branch

`[Device] ShadowSurfaces` (ships OFF) covers a different hole: the game locking a SURFACE
taken off a texture, which the shadow's redirect cannot see by construction. **Its hooks
have still never executed** - no run has exercised them. The `device/upload` line says
whether that path is used at all.

Mechanism and the full reasoning are in `docs/dishonored/ENGINE_NOTES.md`, "SOLVED: the
black texture bug".

### What was NOT done, and why

**The simulator run did not happen.** Two launch attempts: the first died in a C++ runtime
error before the D3D9 device was created (the mod fell through to the system's Virtual
Desktop runtime rather than the simulator, `[VR] XrRuntimeJson` being empty), so none of
the new hooks executed and the run says nothing about them either way. The second attempt
was stopped. **So the new vtable patches - `GetSurfaceLevel` 18 on textures and cube
textures, `LockRect`/`UnlockRect` 13/14 on surfaces - have been compiled and installed but
never executed.** Treat the first run with this build as a bring-up test: if the game dies
at its first texture, those slots are the suspect and `device/census` will say whether the
surface hook installed at all.

## CONFIRMED ON A THIRD RUN (2026-09-04): 2750x2850 at 90 Hz is the default and it holds up

Third headset run on the shipped defaults: smooth, weapon aligned, no ghosting, and no frame
rate below 80 observed. The 90 Hz result reproduces. The defaults now
carry it (`[Screen] RenderWidth=2750 RenderHeight=2850` plus the 90 Hz note beside them).

**One open issue on that run, and it is NOT new**: the session flickered for roughly 25 seconds
at the start before locking. It normally lasts a few seconds; this run it ran long, which is
what made it measurable for the first time. Diagnosed below, **not fixed** -
the first thing to try is a lever that already exists and has never been judged.

### The startup flicker: one eye starving while the level streams

`stereo: beat` across the run tells the whole story:

| t (s) | out/s | L/s | R/s | none/s | draws/s | |
|---|---|---|---|---|---|---|
| 8.5 - 17.5 | 21 - 85 | 0 | 0 | 0 | - | menu/loading, mono by design |
| **20.5 - 38.5** | 87 - 91 | **12 - 19** | **52 - 73** | 10 - 17 | **51 - 72** | **starved: the flicker** |
| **44.5 onward** | 180 | **90** | **90** | **0** | **90** | **locked, stays locked** |
| 77.5+ | 155 - 234 | 0 | 0 | 0 | - | pause screen, mono by design |

`perf: tick` in the starved window reads **17.5 ms against the 11.11 ms budget**, split
`P1[-1] n=36` against `P2[+1] n=156` with `untagged 107`. `reentry: beat` shows pass 2 running
throughout (`2nd/s == draws/s`, all skip counters zero), so the second draw is not missing -
**the game is simply producing 51-72 ticks/s against 90 display slots/s.** Below the display
rate the pair schedule cannot land one pair per slot, the tag stream goes lopsided, and 1016
same-eye pushes accumulate (always `+1`, so LEFT is the eye that starves). One eye refreshing
at ~18 Hz beside one at ~73 Hz is the flicker, and it looks like alternate-eye rendering
because structurally that is what it is.

**Same root as the ghosting, at a different ratio.** Tick slightly over the period gives the
beat (doubled edges); tick far over gives eye starvation (flicker).

### The fix theory, in order, and NOTHING here is implemented

1. **`vrpace strict on` first.** It already shows the fresh eye to BOTH eyes when one is stale,
   which converts the starved window from alternating eyes into a briefly flat picture. It
   ships off, toggles live, and has never been judged. **Try this before any code is written.**
   It wants an A/B rather than a default flip, because it will also fire on the rare
   mid-gameplay stale eye and cost depth for that frame.
2. **If that is not enough**, the shape of a real fix is to extend the `HoldUntagged` idea from
   untagged presents to unbalanced pairs: hold the previous good pair rather than submit a
   lopsided one, bounded so a permanent hold cannot freeze the image.
3. **Measure before either**: why LEFT specifically. Hypothesis - the shared-capture deferred
   delivery (`SharedWait=0` hands over the PREVIOUS slot) repeats a tag when presents arrive
   irregularly. `capture sharedwait on` is the A/B that tests it. This is a hypothesis, not a
   measurement.

## NEXT SESSION (2026-09-05): make the startup phases hook instantly

**The goal**: a load should come up in stereo, aligned, immediately. Today it walks through
mono, then the arms hook and reposition, then stereo hooks, then the weapon and hands flicker
until they lock - **15-25 seconds of settling, every load.** The 2026-09-04 run measured 26 s.

### The measured startup timeline (from the s15c log, times from proxy load)

| t | what happens |
|---|---|
| 0.00 s | pad IAT hook; the engine command line is extended (`-ResX/-ResY`) |
| 0.6 s | config read: hands, hand render drive, crouch, crash handler |
| 0.86 s | `res` adapter-mode hooks installed |
| **3.11 s** | **device hooks** (Present/Reset/SetVSConstF/SetRenderTarget/BeginScene) + the creation census hooks |
| 4.86 s | `[game] state: NO_PAWN` |
| **5.08 s** | XR session live; `reentry: ARMED` (call site patches at the next script dispatch); **`blockhunt: walking 65821 objects`** |
| 17.1 s | `[game] state: MENU` |
| **18.4 s** | `[game] state: GAMEPLAY`; 14058 D3D creations logged at first GAMEPLAY |
| **20.5 s** | stereo tags start - but **starved** (L/s 12-19 against R/s 52-73) |
| **44.5 s** | **locked**: L/s = R/s = 90, nothing untagged, and it stays that way |

**So the settle is two separate problems and they should not be conflated:**

1. **0 -> 18.4 s is mostly the GAME loading**, not us. Our own hooks are all in by 5.1 s. The
   only clearly-ours cost in that stretch is `blockhunt` walking **65821 UObjects** at 5.08 s -
   worth timing before assuming it is free, and an obvious candidate for caching its results
   (the offsets it finds are build-constant) or deferring it off the critical path.
2. **18.4 -> 44.5 s is the eye starvation, and it is OURS to handle.** Diagnosed in the section
   above: while the level streams the tick runs 51-72/s against 90 display slots/s, so the pair
   schedule cannot land one pair per slot and one eye starves. **This is the 26 seconds the
   player actually sees**, and it is where the work is.

### Where to start, cheapest first

1. **`vrpace strict on`** - already exists, never judged, one command. It should turn the
   starved window from alternating eyes into a briefly flat picture. **Do this before writing
   any code.**
2. **Measure why LEFT starves specifically.** Every doubled push is `+1`. Hypothesis: the
   shared-capture deferred delivery (`SharedWait=0` hands over the PREVIOUS slot) repeats a tag
   when presents arrive irregularly. `capture sharedwait on` is the A/B that tests it. This is a
   hypothesis, not a measurement.
3. **Then consider holding unbalanced pairs**, extending the `HoldUntagged` idea: hold the last
   good pair rather than submit a lopsided one, bounded so it cannot freeze the image.
4. **Separately, time the startup hooks themselves.** There is no instrument that says how long
   each phase took - `blockhunt`, the census hooks, the reentry call-site patch, the first
   GAMEPLAY transition. Without that, "make startup instant" has no scoreboard. A phase-timing
   line is probably the first thing to build.

**Do not re-open**: the ghosting (solved, cadence, 90 Hz), the bbox readback (gated, prediction
falsified), motion blur (already off), a per-eye tag asymmetry (impossible - the pair shares one
locate). The 60 fps dips are the Wi-Fi encoder, not the frame path.

## SOLVED (2026-09-04): the GHOSTING was the cadence beat, and 90 Hz is the fix

**No ghosting reported at 2750x2850 on a 90 Hz headset.** Same build, same scene, same render
size at 120 Hz: ghosting still reported. One setting changed.

| | 120 Hz | 90 Hz |
|---|---|---|
| display period | 8.33 ms | 11.11 ms |
| `perf: tick` p50 (p90, max) | 9.1 ms (10.8, 12.9) | 11.3 ms (12.0, 15.1) |
| **display slots per frame** | **1.05 - 1.11** | **1.00 - 1.02** |
| EVEN / UNEVEN windows | 9 / 20 | **33 / 16** |
| MATCHED / UNDER-SUBMITTING | 11 / 26 | **38 / 22** |
| ghosting reported | yes | **no** |

At `off` slots of drift per frame, one frame in `1/off` is held for an extra display slot, and
consecutive frames shown for different durations IS the doubled edge. At 1.11 that is every 9th
frame; at 1.01 every 100th. **The fault was never the resolution and never the pose
attribution - it was the tick not dividing into the display period.**

**The defaults now carry it**: `[Screen] RenderWidth=2750 RenderHeight=2850`, with the 90 Hz
requirement written into the ini text beside it, because the pair is one setting and the refresh
half lives in Virtual Desktop where no ini can reach it.
`tests/golden/known-good-2750x2850-90hz.ini` is the byte copy of the machine that was judged.

### Session 14's falsification was WRONG, and the threshold was why

Session 14 measured 1.03-1.05 slots per frame, read `EVEN CADENCE`, and closed the cadence
hypothesis. The verdict was lying: its threshold was `|off| > 0.06`, so it called 1.05 - a beat
every twenty frames - clean. **The hypothesis was right and the instrument's threshold was
wrong.** Now 0.02, drawn at the measured edge (1.02 does not ghost, 1.05 does), and both
branches print the beat as a number so an "even" verdict shows the residual it forgives.

### Why it drops to 60 at 90 Hz when it never dropped below 90 at 120 Hz

Not a contradiction, and the hitch RATE did not change - normalised by run length it is
**27.6 gaps/min at 120 Hz and 28.4 at 90 Hz. Identical.**

- At **120 Hz** the tick never fit the 8.33 ms slot, so the app never tried to hit one. It
  free-ran and the compositor smeared over the mismatch. No cliff to fall off when you are
  already past the edge: the rate reads a smooth 100-120 and the ghosting is constant.
  **Smooth, and always wrong.**
- At **90 Hz** the tick sits right at the 11.11 ms period. Most frames make their slot - which
  is what removed the ghosting - but one that misses waits a whole period, so an 11.3 ms
  overrun displays for 22.2 ms (45 fps instantaneous) and a run of them averages toward 60.
  **Correct, with a cliff directly underneath.**

The stalls were always there; they are just visible now, standing out against a locked cadence
instead of disappearing into a permanently smeared one.

### What is actually causing the remaining drops, and it is not ours

**54 of the 71 gaps sat in `present-tail (xrEndFrame)`, blocking up to 101 ms.** On a Wi-Fi
streaming runtime a 101 ms block inside the submit call is the encoder or the link. Next steps,
cheapest first, all on the Virtual Desktop side: raise the bitrate or change codec, check the
link speed and channel, try a wired/dedicated AP. Only after that is ruled out is it worth
looking at our frame path again.

The other lever, if you want margin instead: buy ~1 ms of tick. At ~0.63 ms/MP a step to about
**2600x2700** (7.02 MP) predicts ~10.3 ms against the 11.11 ms period - real headroom under the
cliff, at a small sharpness cost. Untested.

### Falsified, honestly: the content-bbox gate was not the hitch cause

Session 15 predicted that gating the 3-second full-frame readback would cut the `perf: frame
gap` count by roughly the number of 3-second windows. **It did not.** Samples fell from one per
3 s to 2-3 per run; the gap rate was unchanged. The counter-evidence recorded next to the
prediction - the gaps sat in `xrEndFrame`, not the capture phase - was the correct read. The
gate stays because it removed a real ~30 MB present-thread stall for free, but it did not fix
what it was predicted to fix.

### The pose-lane instrument: validated, still unarmed

`xr: poseaudit SEAM CHECK ok - the script lane's yaw reads 20.67 deg and this file's own
converter reads 20.67 deg for the same head pose (0.00 apart)` fired in **both** runs. The sign
calibration is proven correct against live data, so a delta it prints would be a real
disagreement. **Nobody armed it** (`vrpace poseaudit on`), so the pose-attribution question is
still open - but it is no longer the ghosting's leading suspect, because the ghosting is
explained. Keep it for the judder/`ahead` work.

## SUPERSEDED (2026-09-04): the pose-lane instrument was built for a fault the cadence explained

Everything below is **built, linted, installed and unverified at runtime** - nothing has been
launched. What the next headset session does, in order:

1. **Set Virtual Desktop to 90 Hz.** This is not optional decoration, it is the arithmetic.
   Fitting the three measured ticks against megapixels (4.56 MP -> 8.75 ms, 6.71 -> 9.55,
   15.73 -> 15.7) gives **~0.64 ms per megapixel on a ~5.6 ms fixed floor**. The floor is the
   game's own CPU tick plus two presents and resolution does not touch it, so at 120 Hz the
   entire 8.33 ms budget leaves 2.7 ms of GPU for two full-frame scene draws - unreachable at
   any VR-useful size. **90 Hz (11.1 ms) is the honest target.** 2750x2850 is 7.84 MP and
   predicts a **~10.6 ms tick**; if it lands far off that, the fit is wrong and say so.
2. **Launch.** 2750x2850 is already armed in all four places (`tools\arm-res.ps1 -Status`
   shows them). The log must read, in order: `res: launch: command line extended ...
   -ResX=2750 -ResY=2850`, `res: handed the game our 2750x2850@<hz> mode`, `CreateDevice - the
   game asked for 2750x2850`, `capture: 2750x2850`, `res: HONOURED`, `xr: swapchain pair
   2750x2850`.
3. **Reach gameplay under `stereo reentry`, then `vrpace poseaudit on`, and turn the head.**

### The pose-lane instrument - what it answers and how to read it

The mod samples the head twice: the SCRIPT lane drives the game camera (the pose the pixels
are DRAWN with), and the PRESENT lane tags the projection layer (the pose the compositor
reprojects FROM). If they disagree the warp is wrong by the difference every frame, worst when
turning fastest and worse when a frame is slow - which is the reported percept exactly. Nobody
had ever measured it. Now:

```
xr: poseaudit SEAM CHECK ok - ...                      <- must appear FIRST
xr: poseaudit L tag .. R tag .. vs SCRIPT-lane .. -> delta L +x.xx R +x.xx deg
    | rendered sample is N locate(s) back, tag is lag=1 -> GENERATION GAP +N
    | one generation costs X.XX deg at this head speed | sample age .. ms ..
```

- **SEAM CHECK first.** The two lanes read yaw out of the same matrix with opposite sign
  conventions (`atan2(m02,m22)` vs `atan2(-m02,m22)`), so a naive comparison would read twice
  the yaw and look like a catastrophic fault that is purely convention. The seam negates once
  and then proves it against live data. **If that line says FAILED, every delta after it is
  meaningless - stop and report it.**
- **`GENERATION GAP 0` with a delta near 0.00 kills the hypothesis** and that is a real result,
  written down here in advance so it cannot be explained away later.
- **A steady nonzero gap names the fault in whole generations**, and `vrpace lag 0|1|2` is the
  live A/B: one of the three must null it.

**The written prediction: the gap is +1 and `vrpace lag 2` nulls it.** The tagging code assumes
"locate N feeds the tick that presents at N+1", one generation, which is why `lag` ships at 1.
But the game's own `DishonoredEngine.ini` carries **`OneFrameThreadLag=True`** - UE3's render
thread runs a frame behind the game thread, so the pixels in present N were drawn from locate
**N-2**. If `lag 2` fixes the ghosting, `OneFrameThreadLag=False` is the independent second
test: it removes the skew at the source instead of compensating for it, at a throughput cost.

### Also shipped: the 3-second stall nobody had looked at

`capture.cpp`'s content-bbox instrument (the `FULL`/`CROPPED` line) needs CPU pixels, and in
the shipping `shared` mode - whose whole purpose is that nothing goes to the CPU - each sample
is a full `GetRenderTargetData` + `LockRect` + row copy of the entire frame **on the present
thread**. That is the same round trip that costs 17-21 ms/present in `sync` mode, it ran every
3 seconds, and there was no lever. `[Capture] BboxMs` now defaults to 30000 with
`capture bbox off|<ms>` live; a size change still resamples immediately.

**Falsifiable prediction:** if this is behind the hitches, the `perf: frame gap` count should
fall by roughly the number of 3-second windows in the run (62-82 gaps over the last two runs is
close to one per window). **Counter-evidence already on record:** those gaps mostly reported
`sat in: present-tail (xrEndFrame)`, not the capture phase. If the count does not move, this
removed a real cost and was not the hitch cause - say that.

### Two more suspects died without a run

- **Motion blur.** Already off: `MotionBlur=False` and `MotionBlurPause=False`, with Arkane's
  own comment in the file - "Motion blur is unwanted". Not the ghosting.
- **A per-eye tag asymmetry.** Under `reentry` the LEFT present holds the XR frame open and the
  RIGHT completes it; `on_present_begin` returns at the top while a pair is open, so there is
  no second waitFrame and no re-locate between the eyes. **Both eyes of a pair share one locate
  generation.** The instrument prints per eye anyway so the invariant is checked, but do not go
  hunting this.

### Still not done from the tester's list

FXAA (`iType_AntiAlias` 1 -> 2) and the vsync A/B are **not** wired - the real keys and the
AppCompat requirement are recorded below and unchanged. Killcam is still not found in any ini.

## OPEN (2026-09-04): the GHOSTING - the cadence was NOT the cause, and that is now measured

**The report** (tester, on `alpha-264-ge2b84a80` at 2064x2208): "still ghosting flicker when turning
head and sometimes it hits harder than others".

### The resolution lever WORKS. Proven, because the tester doubted it and was right to

Every number moved, and the log names the mechanism at each step (`res: handed the game our
2064x2208@240 mode (slot 112)`, `CreateDevice - the game asked for 2064x2208`, `capture: 2064x2208`):

| | 2496x2688 | 2064x2208 |
|---|---|---|
| `perf: tick` | 9.2-9.9 ms | **8.6-8.9 ms** |
| submits/s (of 120 slots) | 88-115 | **114-117** |
| supply verdict | UNDER-SUBMITTING 0.73-0.96x | **MATCHED 0.95-0.97x** |
| slots per frame | 1.13 | **1.03-1.05** |
| interval sd | 1.3-3.8 ms | **0.75-1.36 ms** |
| cadence verdict | UNEVEN | **EVEN CADENCE** |
| capture lock | 0.5 ms | 0.0-0.1 ms |

### And the ghosting SURVIVED it. The cadence hypothesis is falsified

This is the point of having written the prediction down. The cadence is now locked - EVEN CADENCE,
1.03-1.05 slots per frame, sd under 1.4 ms, submits MATCHED to the slot rate - and **the doubled
edges are still there**. So the interference beat was real, was measured, was fixed, and **was not
what the tester is seeing.** Do not spend another session on pacing for this symptom.

What that leaves, cheapest first, all live commands with no relaunch:

1. **Virtual Desktop's Synchronous Spacewarp.** It manufactures intermediate frames by
   reprojecting, which is a literal ghost-frame generator, and it engages and disengages on its own
   - which is what "sometimes it hits harder than others" sounds like. Turn it OFF in the VD
   streamer settings and repeat the same head turn. **This is the first test and it is not ours.**
2. **`vrpace lag 0|1|2`** - which locate generation the layer's views are tagged with (ships at 1,
   "one back"). Infinite recorded the identical open suspect for the identical percept: "camera
   movement feels 'a bit jumpy' beyond the hitches - candidates: ... **the one-pair-stale
   content-pose attribution**". If the tag is a generation off the pose the frame was actually
   rendered from, the compositor reprojects by the wrong amount and the error changes every frame -
   doubled edges under rotation, worst when turning fastest.
3. **`vrpace ahead 0|1|2`** - the locate TIME (ships at 0). Already on the carried list as an
   unjudged judder item. The pair phase reads a steady **-43 ms** (we close ~5 display periods
   before the slot we asked for; on a Wi-Fi streaming runtime that is VDXR's pipeline depth, not a
   fault), so the reprojection is doing 40+ ms of extrapolation on every frame and the pose it
   extrapolates FROM has to be right.

### Not faults, checked so nobody re-checks them

- **The `CROPPED` capture bbox is a menu/loading artifact, not the resolution change.** Both runs
  show `100% x 52-53% (CROPPED)` early and then `100% x 100% (FULL)` once gameplay starts. Same
  shape at both sizes.
- **Not frame duplication or eye desync**: `mono/s=0 none/s=0-1`, `L/s == R/s == out/s / 2`,
  `ageL=1 ageR=0`, `aborts=0 staleEye 0` throughout gameplay.
- **Not Infinite's 30-second GC grid**: its spike class was
  `TimeBetweenPurgingPendingKillObjects=30`, killed A-B-A with 300. Our gaps have no 30 s
  periodicity - 0-13 s in bursts. Infinite's other signature (the streaming / level-visibility walk,
  triggered by view change and traversal) is the closer match and matches the head-turn trigger.
- **The hitches did not improve with resolution**: 62 gaps at 2496x2688, 82 at 2064x2208. They are a
  separate, later item from the ghosting.

### The 4K run: the lever is proven twice over, and SSW is OUT

**3840x4096 ran.** `perf: tick` **15.6-15.8 ms, 64 ticks/s** against 8.6-8.9 ms at 2064x2208 - the
lever moves the cost by 1.8x, exactly as pixels predict, so it is not inert and never was. Tester:
"4k does look way better". The capture lock also scales (0.0-0.1 ms -> 1.4-1.7 ms), which is the
readback and is ours.

**Virtual Desktop's SSW was already OFF** (tester confirmed), so suspect 1 of 3 is dead without a
run. The ghosting persists at every size tried, and the tester adds: **"it seems to get worse when
frame drops happen"**, and their own read is "some kind of eye submit desync, or the world geometry
isn't tracking the head tracking correctly".

**That read is now the leading hypothesis and it is testable.** The mod drives the game camera from
the head pose on the SCRIPT lane, and the compositor reprojects the submitted image using the pose
in the projection layer's views, located on the PRESENT lane. Those are two different samples of the
same head. If they disagree, the compositor's warp is wrong by the difference, and the error changes
every frame - which is doubled edges under rotation, and it grows when a frame is slow, which is
exactly "worse when frame drops happen". Nobody has ever measured that disagreement.

**The matched pair to measure it already exists in the code**: `head_track.cpp` records
`g_viewYawRad` (the yaw actually written to the engine) next to `g_injHmdYawSnap` (the HMD yaw it was
computed from) - its own comment says "Matched pair: this rotation was computed from THIS
g_hmdYaw". The instrument to build is: at submit, compare `g_injHmdYawSnap` for the frame that was
RENDERED against the yaw of the pose the layer is tagged with, and print the delta in degrees. It
can print the unwelcome answer - 0.0 deg means the two lanes agree and this hypothesis dies too.
`vrpace lag 0|1|2` and `vrpace ahead 0|1|2` are the live A/Bs that move it, and neither has been
judged.

### The tester's settings requests, with the REAL keys found (not guessed)

Asked for: killcam off, antialiasing FXAA, models high, maybe vsync on ("I just tried it and it was
maybe more smooth, but I'm not sure"). What the game's own config actually carries, in
`Documents\My Games\Dishonored\DishonoredGame\Config\`:

| ask | key | now | note |
|---|---|---|---|
| FXAA | `DishonoredEngine.ini [SystemSettings] iType_AntiAlias` | **1** (MLAA) | **2 = FXAA**, and the enum is documented in the file itself by the original devs (`EPpAa_None=0, EPpAa_Mlaa=1, EPpAa_Fxaa=2`) - measured, not guessed |
| vsync | same section, `UseVsync` | **False** | `True` is the ask; the tester is unsure it helped, so this one wants an A/B, not a default |
| models high | `SkeletalMeshLODBias`, `TextureForcedLODBias`, `DetailMode`, `Skeletal/StaticLODDistanceFactorMultiplier` | 0, 0, 2, 1, 1 | **already at the high end** (bias 0 = no reduction, DetailMode 2 = high). Nothing to change without inventing a value - do NOT write a guessed multiplier |
| killcam off | **NOT FOUND** | - | no killcam-shaped key in any of the 23 game inis (searched `Kill/Death/Assass/Slow/Cam` boolean keys). It may live in the save profile rather than an ini. Needs finding before it can be defaulted |

The AppCompat trap applies to all of these the same way it applies to the resolution: the bucket
AppCompat picks at startup overwrites `[SystemSettings]`, so anything written there must be written
to all four `AppCompatBucket*` sections too - `iType_AntiAlias` and `UseVsync` both already appear
in `DishonoredCompat.ini` with per-bucket values.

### Armed now: 3840x4096, purely to prove the lever to the eye

The tester asked to see the resolution do something visible, which is the right instinct and the
repo's own rule (confirm a lever moved something before believing its verdict). 3840x4096 keeps the
near-square eye aspect (0.9375 against 2064x2208's 0.9348) and is **3.4x the pixels** of the current
size, so if the lever were inert the tick would not move. Expect it to be slow - that IS the result.
Revert with `res 2064x2208f` (or `res 2496x2688f`) and relaunch.

## RESOLVED as a reading (2026-09-04): the submit stalls were NOT the game outrunning the headset

**The prediction written before the run held.** Submits landed at 88-115/s against 120 slots/s -
UNDER-SUBMITTING in 23 of 24 windows - and `endFrame mean` measured **0.08-1.99 ms**. xrEndFrame is
not blocking and is not throttling anything; the display slots are going unfilled. The reading that
opened this item ("the game produces frames faster than the headset can show them, and the runtime
absorbs the mismatch by blocking at submit") is **dead**, and it would still be alive if the period
had not been printed.

**What remains real:** 62 `perf: frame gap` lines, most still in `present-tail (xrEndFrame)` with
36-96 ms inside the submit call. Those are genuine, rare hitches, not pacing - and on a Wi-Fi
streaming runtime an 85 ms block in xrEndFrame is the encoder or the link, not our frame path. They
are worth a separate look AFTER the ghosting, because the ghosting is continuous and these are not.

**The tick, for whoever picks up "make it 120":** 9.2-9.9 ms, split almost evenly between the two
scene renders reentry needs - per pass roughly `present 1.8 ms + out/R 2.0 ms`, with our own capture
lock at 0.2-0.5 ms and endFrame at 0.0-0.6 ms. The mod is not the cost. Reaching 8.33 ms means
taking ~12 % off the game's own render, and the obvious dial is the 2496x2688 per-eye size.

## OPEN (2026-09-04): the submit stalls - 48 hitches of 44-156 ms, two thirds in xrEndFrame

**The report** (tester, 2026-09-04): "super laggy", and separately "this game is old enough that
it should run at hardlocked 120 fps, especially on a 4070 Ti Super". The headset is set to
**120 Hz**, so the budget is **8.33 ms** per frame.

**Throughput is NOT the problem, and that is the whole point of this item.** Measured on
`alpha-267-g4b9a0f3c-dirty`, Quest 3 via VDXR, 2496x2688 per eye, method reentry:

- **mean present interval 4.3-6.6 ms** (the heartbeat read GAME=138-233 fps across the run).
  That is comfortably inside the 8.33 ms budget, with 25-50 % headroom.
- **48 `perf: frame gap` lines**, gaps of **44-156 ms**. The worst is **36.3x the 4.3 ms mean** -
  at 120 Hz a 156 ms stall is 19 dropped frames in a row.

**Where the stalls sit**, from the `sat in:` field of those same 48 lines:

| phase | count |
|---|---|
| **`present-tail (xrEndFrame)`** | **31** |
| `out/idle (waiting for the game thread)` | 9 |
| `out/R (executing the frame)` | 3 |
| `game_tick` | 3 |
| the game's `Present` | 1 |
| `present-head (wait)` | 1 |

`flags:` on those lines read `reset=0 load=0 paceTimeouts=+0` almost throughout (one `+1`, one
`pairOpen=1`), so the pace lane is not timing out and the method is not re-arming. `vrpace ahead`
was at its shipped 0.

**Two thirds of the stalls are in the submit call.** So this is a pacing/submit problem, not a
rendering-cost one - which is the good news, because the fix is scheduling rather than cutting
quality.

### The gap that had to close first: the display period is now printed (session 13)

**It was not in the log.** `dvr::vr::display_period_ns()` existed in the runtime layer, was read by
the pace sync, and was PRINTED only inside the `PACE-BOUND` clause of the `perf: tick` line - so the
one run that most needed it (48 hitches, the wait at ~0, no `PACE-BOUND` line anywhere) is exactly
the run where it stayed invisible. "The game produces frames faster than the headset can show them,
and the runtime absorbs the mismatch by blocking at submit" was therefore an INFERENCE from a phase
NAME, and this project has spent whole sessions on inferences that read well and were wrong.

**What now prints** (built and lint-clean on `performance-fix`, **not yet run** - see below):

- **`stereo: rate`**, a new line on the 3 s stereo beat:
  `hmd=8.33 ms (120.0 Hz) slots/s=120.0 | presents/s=233 submits/s=116 (one xrEndFrame per pair) |
  endFrame mean=6.41 ms max=41.2 ms over 349 submits | <verdict>`.
  `submits/s` is a NEW counter: `xrEndFrame` calls from the present path, which under reentry is
  **one per PAIR** - it is the tick rate, not `out/s`. The endFrame mean and max are the submit's
  own cost, drained per window.
- **the verdict on that same line**, which is the whole point and can print the unwelcome answer:
  **OVER-SUBMITTING** (> 1.05x the slots) confirms the throttle reading and the lever becomes "stop
  producing frames nobody sees"; **MATCHED** (0.95-1.05x) means the endFrame MEAN is the pacing wait
  and is not a hitch, only its max is; **UNDER-SUBMITTING** (< 0.95x) says display slots are going
  unfilled, which falsifies the throttle reading outright and puts the cause upstream of the
  headset's cadence. A runtime that leaves `predictedDisplayPeriod` at 0 prints `hmd=UNKNOWN` and
  gets NO verdict - never read that as 0 Hz.
- **`perf: tick`** now opens with `[hmd 8.33 ms = 120.0 Hz, budget 8.33 ms/tick]` unconditionally,
  not only when pace-bound.
- **`perf: frame gap`** now ends its phase attribution with `= 19.0 display slots at 8.33 ms`. The
  "156 ms is 19 dropped frames" arithmetic in the table above was done by hand off the log; it is in
  the line now.
- **`status.json`**: `stereo.pair{displayPeriodMs, displayHz, endFrames, endFrameMeanMs,
  endFrameMaxMs}` and `perf{displayPeriodMs, displayHz}`. `game-cmd.ps1 "stereo status"` prints the
  same numbers live, without waiting for a beat.

**A prediction worth writing down before the run, because the arithmetic already argues against the
inference.** The report's own numbers are 4.3-6.6 ms mean PRESENT interval, and reentry submits one
frame per two presents - so submits/s should land at **76-116**, BELOW 120. If that holds, the line
reads UNDER-SUBMITTING and the "game outruns the headset" reading is dead: the headset would be
going hungry, not being over-fed, and the 31 present-tail stalls are genuine hitches inside
`xrEndFrame` rather than a throttle. The measurement is what settles it either way.

**Next step: a headset run on `performance-fix` with nothing else changed**, and the three lines
above out of the log. Nothing has been installed or launched for this change - the build is verified
only as compiling, linting clean and exporting the nine names.

### Not a fault, for the record

Stereo reads mono for roughly the first 6 seconds of a run (`2nd/s=0`), then latches to 103-108 and
holds - confirmed by the tester ("it is mono at the very beginning but it latches on and stays good
after a few seconds"). The `2nd/s=0` at the very END of a log is leaving gameplay, not a regression.

## FIXED (2026-09-03, headset, dev rig): all three flickers, in one chain

Branch `swapchain-one-picture-flicker`. Installed and judged as `alpha-253-g8441404f`. **The
tester's verdict after the third fix: every kind of flicker is gone.**

The three faults were nested - each fix exposed the next - and each was confirmed in the headset
and in the log before moving on.

### 1. The stale RIGHT eye (`1507bafc`)

`reentry.cpp`'s c5 invariant has two arms and they are **not equally trustworthy**. `inv=+1`
("pass 2 after pass 1") compares two draws with NO world tick between them: the step is exactly
`-ipd*scale` along the camera's right row by construction. `inv=-1` ("pass 1 after a still pass
2") is the **only arm that reasons across a world tick**, and holds solely while the player is
near still. A gently moving player - turning in place, decelerating, crouch-walk - parks the
tick's travel inside the `+-0.35*ipd` window (about `+-2.2 uu` at the measured 6.18) and the
fragile arm then names a genuine pass-2 present a pass 1. It did so **unconditionally, on a
streak of one**. One wrong `-1` writes the left swapchain twice and never writes the right.

The fragile arm now defers to the ring on a disagreement (a streak of three still earns the
override for either arm); neither arm invents a tag on an empty ring or over the `0` tag a
single gameplay draw pushes.

| | before | after |
|---|---|---|
| `STALE ? EYE` lines | 36 in 171 s | 1 in 238 s |
| one-picture pairs at sc | 14 of 39 (36 %) | 0-1 of 40 |
| `sc-target repeats` | 80 | 2 |
| `out/s` median | 117 | 212 |

`c5Held=36` counts the deferrals that did it, against `c5Agree=29210 c5Disagree=112`.

**Why no gate admitted to it**: `SceneDrawMaybeSecond` cannot skip pass 2 unlogged - `!doubleIt`
means pass 1 pushed no `-1` at all, the poison logs an `Error` at its one site and stands the
method down, and the forced skip IS the `forced=` field. Both tags were pushed every time; the
second `-1` was manufactured downstream. Every zero on the line was true.

### 2. The mono flick on the arms and weapon (`[Stereo] HoldUntagged`)

With the stale eye gone the tester saw a different flicker: "both arm models/weapons instead of
just the left handed crossbow", and it "felt slightly different". It was. `mono/s=1..4` in steady
gameplay, and a mono present is the same image in BOTH eyes, so its error scales with disparity -
near zero on distant geometry, **largest on the viewmodel at 30-50 cm**. Symmetric, hence both
arms, where the stale-eye fault was one-sided. 26 of 29 single-draw spells were the present-stall
guard; `c5Refused=72` was the rest.

`HoldUntagged=3` (ported from the parked branch) took `mono/s` 1-4 -> **0** and every other
flicker with it.

### 3. The black frame in both eyes (`8441404f`) - caused by the fix for 2

The hold immediately produced a new, subtler artifact: one black frame in both eyes, frequent.
**The whole layer assembly in `on_present_end` sits inside `if (backbuffer)`**, and
`backbuffer = frame`. A present that hands in no texture therefore reaches `xrEndFrame` with
`layerCount 0`, and a zero-layer frame gives the compositor nothing for that display slot -
black, both eyes, one frame. `HoldUntagged` made that path common: it holds a present by
returning false from the method, and `frame_hooks.cpp:181` passes the null to `on_present_end`
unconditionally.

The ported commit's own message claims "nothing is submitted; the compositor holds the previous
pair". **That is not true of this runtime**, and the commit was parked without a headset run that
would have caught it - it was taken as established behaviour instead of checked.

The guard now re-submits the layer the previous present used. Nothing is acquired or released on
such a present, so the swapchain images are untouched and the compositor genuinely re-shows the
previous pair; reprojecting it to the new display time is the runtime's job, and the
parked-session keepalive already re-submits that same snapshot. Measured over 126 s:
**`held=25 black=0`**, `STALE` 0, `sc-target repeats=0`, one-picture `sc=0`, `out/s` median 201.

### Levers and defaults

**`[Stereo] HoldUntagged=3` now SHIPS** (the user's call, 2026-09-03), joining the session-9
precedent of the headset-judged values being the defaults. It is a deliberate exception to the
default-OFF rule for render levers, made because the artifact it removes is a visible flicker on
the viewmodel and the black frame it used to expose is fixed at the runtime. `0` is the A/B and
the pre-41.1 behaviour; `stereo hold <n>` switches it live, `none/s` on the beat counts what it
held, and `zeroLayerHeld`/`zeroLayerBlack` in status.json say whether the guard covered them.

**Judged on ONE rig, for a few minutes.** A second headset that reports one-frame stalls or a
smeared weapon should try `stereo hold 1` and then `0`, and say which is better.

### The rig, and three traps that cost runs

Quest 3 via VDXR, 5120x1440@240 desktop, RTX 4070 Ti SUPER. `[Screen] RenderWidth/Height` in the
ini CANNOT affect the launch it is set on - DllMain reads only `dishonored_vr_launch.txt`, written
by the `res` seam word, so use `res 2496x2688` and never hand-edit the key. `bPauseOnLossOfFocus`
was TRUE in the game ini (now FALSE here): with it on, alt-tabbing to drive the command seam
pauses the thing being judged. VERIFICATION gotcha 17 bit 1 run in 3: state sticks at MENU in the
level, `2nd/s=0`, screen stays mono - open and close the pause menu.

**Check the build tag before reading any verdict out of a log.** The first "it's fixed!" here was
measured on a build that had been compiled but never installed - `build.ps1` had run, `install.ps1`
had not, so the game folder still held the previous DLL. The log banner names the build
(`build alpha-NNN-gHASH`) and `Get-FileHash` against `build\src\RelWithDebInfo\d3d9.dll` settles
it in one command. A `-dirty` tag means the tree had uncommitted changes at build time and the
log cannot be traced to a commit: rebuild from a clean tree before handing a log to anyone.

## Current state (2026-09-04, session 14: the throttle reading is dead, the cadence is the suspect)

**This branch (`performance-fix`) carries two instruments and one lever, all default OFF or
log-only.** No render path changed. Session 13 added the rate measurement, it was run, and it killed
the hypothesis the branch was opened for - the submit was never throttling. Session 14 added the
cadence measurement, which points at the ghosting instead, and `[Pace] SyncHz`, which ships at 0.

What shipped: the `stereo: rate` beat line (hmd period and Hz, slots/s, presents/s, submits/s, the
pair interval's mean and sd, the submit's own mean and max cost, and TWO verdicts - supply and
evenness - either of which can print the unwelcome answer); the display period unconditionally on
`perf: tick`; the gap converted to display slots on `perf: frame gap`; `[Pace] SyncHz` as the
persisted form of `vrpace sync <hz>`; and all of it in `status.json` and `stereo status`. The two
OPEN sections at the top carry the readings and what they mean.

**Built, lint-clean, exports OK, and INSTALLED as `alpha-260-g958ab57a`** (RelWithDebInfo,
hash-checked against `build\src\RelWithDebInfo\d3d9.dll`, clean tag - no `-dirty`). It replaces
`alpha-259-g865f1bcd`, so the game folder no longer carries the crouch fix: `crouch-fix` is still
only PR #14 and this branch is cut from `VR-Main`. **Not launched, not run** - in the simulator or a
headset. Treat every number it prints as unseen until a run produces one.

The two logs that were in the game folder are archived to `D:\dvr-data\logs\s13-pre-install-*.log`
before the run overwrites them (rotation is one deep and there were already two).

**The crouch height rise is FIXED and headset-judged** (previous session): the 38.16 deep-crouch
capsule write moved the pawn 20.00 uu on every crouch and the camera's rate-limited catch-up
integrated the remainder. `[PosTrack] DeepCrouch` now defaults to 0. That work is on `crouch-fix`
and is **open as PR #14, not merged**. Its derivation and the five falsified suspects are in
`docs/dishonored/ENGINE_NOTES.md` on that branch.

**The DLL in the game folder is `alpha-262-g61130855`** (session 14's build - the cadence instrument and
`[Pace] SyncHz`), replacing the session-13 `alpha-260-g958ab57a` that produced the run above. Both
are the tip of THIS branch, built
RelWithDebInfo, installed and hash-checked against `build\src\RelWithDebInfo\d3d9.dll`. It
replaced `alpha-259-g865f1bcd` (the tip of `crouch-fix`), so **the deployed build no longer carries
the crouch fix** - `performance-fix` is cut from `VR-Main` and PR #14 is still unmerged. Nothing
diagnostic is armed either way: `[Hands] CrouchAB`, `CrouchBurst` and `[PosTrack] DeepCrouch` are
all default OFF, and the tester's `dishonored_vr.ini` and the game's `DishonoredCamera.ini` were
restored from backups after the crouch investigation; no diagnostic keys remain in either.

**Two findings from that session are deliberately unbundled and unfixed**, because neither has been
judged in a headset:

1. `LocPropFind` and `CrouchPropFind` sit below `if (!g_handMesh) return` in `ApplyHandToMesh`, and
   `[Mode] GamepadOnly=1` (the shipped default) clears `g_handMesh` - so **they have never run on a
   shipped build**, and the 38.24 eye clamp, which needs `g_actorLocFound`, has never run either.
   Reviving it is a real behaviour change and needs a headset verdict of its own.
2. The config line reporting physical crouch as "armed" when `[Mode] GamepadOnly=1` has already
   vetoed it. Cost a session's hypothesis once already.

## FIXED (2026-09-04, headset, dev rig): the crouch height rise

**The report**: crouching then standing raises the player slightly, and spamming it rises far
enough to pass through the ceiling. Also, from the same run, "almost like noclip, I could float
around".

**The cause is ours**: the 38.16 deep-crouch write. Shrinking the crouched collision cylinder
(65 -> 45) under a grounded pawn moves the pawn's origin down by exactly the shrink, 20.00 uu,
because the engine keeps the feet planted - and sometimes leaves the pawn airborne. The camera
chases that with a rate-limited convergence that cannot finish before the next crouch, so the
remainder accumulates: ~20 uu of view per cycle, 2184 uu (22 m) in one run. Measured, filmed frame
by frame, and confirmed by an A-B-A A/B (+20.35 uu/cycle with the write on, -0.26 with it off).
Full derivation and the five falsified suspects in ENGINE_NOTES.

**The fix**: `[PosTrack] DeepCrouch` defaults to **0**. Judged in the headset by the tester: the
climb and the floating are both gone. The cost is that the player no longer fits under low
furniture; `DeepCrouch=1` restores the old behaviour and the bug with it. It cannot be made safe as
written without writing `Actor.Location`, which this mod deliberately never does.

## Previous state (2026-09-04, session 9: THE EYES ARE FIXED, root cause proven in the headset; the headset-judged values are the defaults)

**Merged to `native-stereo-rendering` (PR #7, 13 commits).** The per-eye render is correct on the
headset, at rate, with the tested configuration shipping as the defaults.

- **THE ROOT CAUSE, found and fixed and PROVEN**: the eye tags paired draws to presents by
  push/pop ORDER, and the order breaks wherever the game thread runs ahead of the render thread -
  a level load, a pause/resume, a re-arm - and spontaneously in gameplay every ~2 s on the
  tester's rig (131 skews in four minutes). Each break showed each eye the other's draw until the
  next one. That is the fault this project has chased since run 15 ("the eyes disagree", "90 % of
  the time, more at the beginning", "never correct after a load"). THE FIX: between a tick's two
  draws nothing moves the camera but the eye offset, so pass 2's camera sits exactly one IPD along
  the camera's right row from pass 1's; a present whose camera step reads that IS the second draw
  whatever the queue claims, and a disagreement streak realigns the queue. `[Stereo] C5Pair=1`.
  THE PROOF (headset, the user, 2026-09-04): unticking `c5 pairing` on the F10 EYES block and
  pausing brought the fault straight back (`swapped=24 of 25` pairs, the picture's parallax on the
  wrong side), ticking it back removed it (`swapped=0` for the rest of the run) - by eye and in the
  log, three times.
- **THE INSTRUMENT that found it ships**: the frame-identity trace (`core/gfx/frame_id`, `[Perf]
  FrameId=1 FrameIdEvery=8`, `frameid on|off|status|every N`, status.json `frameid{}`): one 64x64
  luma thumbnail per sampled present at the backbuffer as the capture found it, the shared slot,
  the eye texture and the swapchain image, read three presents later and never waited on, with the
  draw's camera position and right row on the same record. The `stereo: frameid` line prints, per
  left/right pair, the L-R difference per stage, the camera step and side check, the picture's own
  parallax shift and the first stage that reads as one picture. The F10 Display tab's **EYES
  block** shows the same numbers live and carries DUMP EYES, REARM 2, CAPTURE REINIT, PROJECTION
  OFF / AUTO, the c5-pairing tickbox and the trace tickbox.
- **PERFORMANCE: back at the headset's rate.** The trace read every present cost the tester's GPU
  1.5 ms per present (stage bb's `GetRenderTargetData` is a pipeline sync there) and the tick went
  13.9 -> 16.7 ms, 60/s under a 72 Hz headset; sampling one pair every 8 ticks removed it
  ("the fps is perfect now", 2026-09-04). The trace tickbox is off = no cost at all.
- **THE DEFAULTS ARE THE HEADSET-JUDGED VALUES** (a fresh ini now comes up where the testing left
  off): `[Stereo] Method=reentry Armed=1 C5Pair=1`, `[Camera] EyeField=0x330`, `[Neck] Mode=cancel`
  with the measured pivot (0.321 / 0.062 m), `[PosTrack] Scale=98 Lane=auto`, `[Tracking]
  HeightOffsetM=-0.090`, `[Screen] RenderWidth=2496 RenderHeight=2688 RenderFullscreen=1
  VirtualMode=1` (the Quest 3 through VDXR per-eye size, advertised so the game creates it - a
  fresh install used to render the game's own size and look soft), `[Device] Ex=1 Managed=shadow`,
  `[Capture] Mode=shared`, `[Perf] FrameId=1 FrameIdEvery=8`. Another headset: the F10 Display
  picker writes its size for the next launch; `res 0x0` asks for none.
- **Also fixed this session**: `dump eyes` writes a consecutive left/right pair and encodes it on a
  worker thread (the 640 ms present-thread stall used to re-arm the second draw, so every dump
  changed what it was taken to judge); the beat line's `presentTid` follows the presenting thread
  (it was latched at the first present, which at boot is the game thread's - a false lead in run
  17); pass-2 eye writes the camera seam refuses are counted (`p2write refused=`).

## Next steps (one paragraph per developer)

**The next developer session (the ghosting)**: build the POSE-LANE instrument and stop guessing.
Three suspects have now been killed by measurement - the submit throttle, the uneven cadence, and
Virtual Desktop's SSW - and the one the tester named has never been measured at all: the camera is
driven from a head sample on the SCRIPT lane while the compositor reprojects using a pose located on
the PRESENT lane, and nothing anywhere compares the two. `head_track.cpp` already keeps the matched
pair (`g_viewYawRad` beside the `g_injHmdYawSnap` it was computed from, its own comment says so);
publish that snap to the runtime and, at submit, print the delta in degrees against the yaw of the
pose the layer is tagged with. It must be able to print 0.0 and kill the hypothesis. Then
`vrpace lag 0|1|2` and `vrpace ahead 0|1|2` are the live A/Bs that move it, and neither has ever
been judged. The corroborating detail worth keeping in mind: the tester says the ghosting **grows
when frames drop**, which is what a lane-disagreement does and what a locked cadence does not.
Second job, small and separable: the game-settings profile (the real keys and what is already at
maximum are tabulated in the OPEN section - `iType_AntiAlias` 1 -> 2 is the only clear win, `UseVsync`
wants an A/B, model detail is already high, and no killcam key exists in any of the 23 game inis).
Anything written to `[SystemSettings]` must also go to all four `AppCompatBucket*`, or the bucket
AppCompat picks at startup overwrites it - the same trap the resolution picker already handles.

**The user (headset)**: installed as `alpha-264-ge2b84a80`, with **3840x4096 armed for the next
launch** (your call - you judged 4K much better looking). Know the trade: it measures 15.6-15.8 ms
per tick = 64 fps into a 120 Hz headset, against 8.6-8.9 ms at 2064x2208, so if the ghosting really
does track frame drops, 4K is the worst case for it and the sharpest picture at the same time. If
you want the middle, `res 2496x2688f` or `res 2064x2208f` in-game then relaunch. Nothing else is
waiting on you until the pose instrument exists - the three cheap A/Bs are spent. `crouch-fix`
(PR #14) is still ready for your merge decision, and note that the installed build does NOT carry it.
Still open from earlier sessions: (1) the PITCH PIVOT with `[Neck] Mode=cancel` against `off` and
`add`; (2) WORLD SCALE and eye height at `[PosTrack] Scale=98` / `HeightOffsetM=-0.090`.

## Blockers

- **The ini version rewrite wipes a tuned ini** (`config.cpp` carries three keys over): the
  session-8 and session-9 keys ship without a version bump. A key-preserving rewrite is a
  separate change.
- **WM_CLOSE leaves a stuck `Dishonored.exe`** (session 5): close a healthy game with
  `Stop-Process`; a menu quit is clean on the Quest (run 15).
- **The walk-in on the simulator is by hand**: Return x4 with 28 s waits reached the level on four
  of six launches this session; otherwise the mod's menu flag sticks (VERIFICATION gotcha 17) and
  an Escape pair clears it. Look at an `xrsim-shot` before trusting a state line.

## Session log

### 2026-09-04 - session 15b: the ghosting is solved, and the verdict that hid it is fixed

Two headset runs, same build, same scene, same 2750x2850 render, only the headset's refresh
changed. 120 Hz: ghosting still reported. 90 Hz: **none reported, and no jitter**. Display
slots per frame went 1.05-1.11 -> 1.00-1.02.

**The cadence hypothesis was right all along, and session 14 killed it on a lying verdict.**
The `EVEN CADENCE` threshold was `|off| > 0.06`, so 1.03-1.05 - a beat every twenty frames -
printed as a clean bill of health, and that clean bill was read as falsification. The
instrument was correctly built and correctly read; the line between pass and fail had simply
been picked before anything was measured. Threshold is now 0.02, at the measured edge, and both
branches print the beat as a number (one frame in N, and its Hz) so an "even" verdict has to
show the residual it is forgiving.

**The tester's puzzle - why it drops to 60 at 90 Hz when it never went below 90 at 120 Hz -
has an answer, and the hitch rate is the proof.** Normalised by run length: 27.6 gaps/min at
120 Hz, 28.4 at 90 Hz. The stalls did not get worse. At 120 Hz the tick never fit the slot, so
the app free-ran and the compositor smeared over the mismatch - no cliff to fall off when you
are already past the edge, and that smearing IS the ghosting. At 90 Hz the tick sits right at
the period: frames make their slots (ghosting gone) but a miss costs a whole period, which is
22.2 ms, which averages toward 60 in a run. Smooth-and-always-wrong versus correct-with-a-cliff.

**The remaining drops are not ours.** 54 of 71 gaps sat in `present-tail (xrEndFrame)`, up to
101 ms. On a Wi-Fi streaming runtime that is the encoder or the link.

**The bbox prediction failed and is recorded as failed.** Gating the 3-second readback cut
samples from one per 3 s to 2-3 per run and changed the gap rate not at all. The
counter-evidence written down beside the prediction was the correct read. The gate stays - it
removed a real unlevered stall for free - but it did not fix what it was predicted to fix.

**The pose-lane instrument validated itself and was never armed.** `SEAM CHECK ok ... 20.67 deg
and 20.67 deg (0.00 apart)` in both runs: the sign calibration is proven against live data, so
the instrument would not have lied. Nobody ran `vrpace poseaudit on`, so the pose-attribution
question stays open - it is just no longer the ghosting's suspect.

Defaults now carry the judged values (`[Screen] 2750x2850`, with the 90 Hz half written into
the ini text beside it because it lives in Virtual Desktop), and
`tests/golden/known-good-2750x2850-90hz.ini` is the byte copy of the machine that was judged.

### 2026-09-04 - session 15: the pose lanes get an instrument, and a 3-second stall is found

Session 14 falsified the cadence hypothesis and left three suspects. Two of them died at the
desk, from files already on disk, before anything was written:

- **Virtual Desktop's SSW** - the tester confirmed it was already off.
- **Motion blur** - `MotionBlur=False` in `DishonoredEngine.ini`, with the Arkane developers'
  own comment beside it: "Motion blur is unwanted".

That left the tester's own read - "eye submit desync, or the world geometry isn't tracking the
head tracking correctly" - and it turned out the instrument for it was **half-built and
unreachable**. The pose audit already sat at the right line (immediately after the projection
views are filled, before they are attached), already wrapped to +-180, already rate-limited at
500 ms. Two things were wrong: it compared the tag against the pose the present thread had just
CONSUMED, which is fresh at submit and therefore never the sample the pixels came from - so it
could not answer the question it was named for - and **`set_pose_audit` had no caller at all**.
The `fovaudit pose on` command its comment named does not exist in this repo. It was dead code
that would have printed a confidently wrong number if anyone had reached it.

**What was built.** A locate generation counter bumped once per `xrLocateViews`; the game side
stamps every head sample with the generation it came from and publishes it, with the yaw the
camera write actually used, through a new `dvr::vr::publish_script_head` seam (needed because
`g_injHmdYawSnap` is static inside the unity TU and the runtime layer is a real module). The
audit now reports, per eye, the tagged yaw against the rendered yaw, the gap in GENERATIONS
against the active `lag`, and what one generation costs in degrees at the current head speed.
`vrpace poseaudit on|off` arms it.

**The sign trap, and why it is self-checking.** The two lanes read yaw out of the same matrix
with opposite conventions - `atan2(m02,m22)` against `atan2(-m02,m22)` - so a naive subtraction
reads about twice the yaw. That is the most convincing possible way for an instrument to lie:
a large, stable, entirely fake disagreement. The seam negates once and then PROVES it against
live data, reading the same pose back through the runtime's own converter at the first publish
and logging `SEAM CHECK ok` or `FAILED`. An instrument whose calibration is only asserted in a
comment is not evidence.

**The prediction, written before the run.** `DishonoredEngine.ini` carries
`OneFrameThreadLag=True`. The tagging code assumes one generation of skew (`lag=1`); with UE3's
render thread a frame behind the game thread it should be two. So: gap +1 at `lag 1`, nulled by
`vrpace lag 2`. If the delta reads 0.00 at `lag 1`, the hypothesis is dead and that is the
result.

**Found on the way: a full-frame CPU readback every 3 seconds, on the present thread, in the
shipping capture mode, with no lever.** The content-bbox instrument needs CPU pixels, and
`shared` mode exists precisely so that nothing goes to the CPU. Each sample is the same
`GetRenderTargetData` + `LockRect` + row copy that makes `sync` mode cost 17-21 ms/present -
about 31 MB at 2750x2850. `[Capture] BboxMs` (default 30000) and `capture bbox off|<ms>` gate
it; a size change still resamples at once. The prediction and the counter-evidence are both
recorded at the top of this file.

**Performance, answered with arithmetic instead of another run.** Fitting the three tick
measurements against megapixels gives ~0.64 ms/MP on a **~5.6 ms fixed floor**. The floor is
what makes 120 Hz unreachable - it leaves 2.7 ms of GPU for two full-frame scene draws - so the
resolution question is really a refresh-rate question. 2750x2850 at 90 Hz is the coherent
combination and is what is armed. There is no render-scale, no foveation and no per-eye
resolution anywhere in the codebase; resolution is the only pixel lever that exists.

**New tool:** `tools\arm-res.ps1` arms a size with the game not running, writing the same four
places `ResRequest` does. Arming used to cost two launches (the seam command only exists while
the game is up, and its write takes effect the launch after).

**Nothing was launched.** Everything here is built, linted, exports-checked and installed, with
the format strings audited by hand; no runtime behaviour is verified.

### 2026-09-04 - session 14: the throttle reading dies, the cadence is named

The session-13 instrument was installed and run (`alpha-260-g958ab57a`, Quest 3 / VDXR, 120 Hz) and
it did its job in both directions.

**It killed the hypothesis it was built to test.** submits/s 88-115 against 120 slots/s -
UNDER-SUBMITTING in 23 of 24 windows - with `endFrame mean` at 0.08-1.99 ms. The submit is not
blocking and never was; the display slots were going unfilled. Written prediction, held.

**It pointed at the real one.** The tester's bigger complaint on that run was ghosting - doubled
edges on world geometry when turning the head. `perf: tick` reads 9.2-9.9 ms against an 8.33 ms
slot, and `pacetrace.log`'s `TRACE pairs` reads interval mean 8.6-11.8 ms with **sd 1.3-9.6 ms** and
`waitGate 3-64 ms/s` - the game free-runs at ~1.13 display slots per frame, unevenly, so consecutive
frames are held for different numbers of slots. That is the interference beat `pace_sync_gate()` was
written for in the BioShock lineage, and its own comment predicted this shape of fault.

Shipped: the pair interval mean/sd and an `UNEVEN CADENCE` / `EVEN CADENCE` verdict with
slots-per-frame on the `stereo: rate` line (the numbers existed only in `pacetrace.log` at trace
level, which is why no one had seen them), and `[Pace] SyncHz` - the persisted form of
`vrpace sync <hz>`, shipping OFF, refusing an out-of-range value with the number it read.

**The proposed fix was wrong and was corrected in the same session.** `vrpace sync 60` locks the
cadence but 60 is too low for VR, and the user said so. Infinite's own numbers on the same runtime
say why no limiter is needed: it ran 80 pairs/s == its 80 Hz refresh with sd 0.3-1.0 ms, LOCKED, on
the same two-draw method - because its render cost fit inside its period. Ours does not (9.4 ms into
8.33), and we are also at 47 % more pixels per eye than Infinite's native. So the lever is the gap
between tick and period, from either end: `res 2064x2208f` (the Quest 3 panel's own size) or a 90 Hz
headset. Still a prediction; nobody has judged either in a headset.

Installed as `alpha-262-g61130855` (clean tag); builds, lint clean, exports OK.

### 2026-09-04 - session 13: the display period is measured, not inferred

The branch's first code. **One commit, instrument only** - no lever, no default, no render path.
`dvr::vr::display_period_ns()` had existed since session 42 of the BioShock lineage and printed in
exactly one place, inside the `PACE-BOUND` clause of `perf: tick`, which is a clause the hitching
run never triggered. So the headset's rate was absent from the one log that needed it.

Added: an `xrEndFrame` counter with its own cost (count, cumulative sum, per-window max) at the
single present-path submit site, and the display period, both published through `PairProbe`; the
`stereo: rate` beat line built on them, with a three-way verdict (OVER-SUBMITTING / MATCHED /
UNDER-SUBMITTING, plus UNKNOWN when the runtime leaves the period at 0); the period unconditionally
on `perf: tick`; the gap in display slots on `perf: frame gap`; the same numbers in `status.json`
and in `stereo status`.

**A prediction is on the record before the run** (OPEN section): the report's 4.3-6.6 ms present
interval, halved by reentry's one-submit-per-pair, puts submits at 76-116/s against 120 slots/s -
UNDER-SUBMITTING, which would falsify the throttle reading outright. The line was written so it can
say that.

**Verified as: builds, `lint: clean`, `exports OK: 9 names`, installed and hash-checked
(`alpha-260-g958ab57a`).** Not launched, not run in the simulator or a headset - every number the
new lines print is still unseen.

What was already measured before this session and should not be re-derived:

- The rig has **headroom**: mean present interval 4.3-6.6 ms against an 8.33 ms budget at 120 Hz.
  The complaint is not framerate.
- **48 hitches of 44-156 ms**, and **31 of 48 sat in `present-tail (xrEndFrame)`** - the submit
  call. `paceTimeouts` and `reset` were 0 throughout, so the pace lane is not timing out.
- **The display period is not logged**, so the obvious reading (the game outruns the headset and
  blocks at submit) is an inference. Measuring it is step one.
- Mono for the first ~6 s of a run then latching to 103-108 `2nd/s` is NORMAL and tester-confirmed;
  do not chase it.

### 2026-09-05 - session 11: the work is on a board, and the flow is written down

Branch `claude/linear-github-integration-9d1a52`, docs and templates only. No code, no build,
no render lever touched.

Three people are now finding faults and recording them in three private places, and PRs #12,
#14 and #15 each name real, measured, open defects that appear in no shared list. The
Linear workspace `vr-stereo-hub` existed but was empty.

**The board now matches the code.** Team `VR`, project **Dishonored VR Mod** with a lead, a
spec and its doc links; four release-shaped milestones (41.1, 41.2, 42.0, 42.1) kept in sync
with the GitHub Releases page, each naming the ROADMAP rungs it closes; a `Type` label group,
ten `area:` labels and five flag labels; **43 tickets** (VR-6 to VR-48) covering every open PR,
every unticked ROADMAP box, every genuinely open KNOWN_ISSUES entry, the three STATUS blockers,
and the four defects PRs #14 and #15 handed over. Merged work is recorded as **one catch-up
project update**, not as retroactive tickets. The four open PRs carry `Fixes VR-<n>` and two
were retitled to conventional-commit subjects.

**The flow is `docs/LINEAR_AND_GITHUB.md`**: statuses and what each means here, priority,
labels, the ticket template, the numbered ticket-to-release flow, the PR contract, project
updates, the release ritual, and what only the Linear UI can do. `CLAUDE.md` carries the hard
rules and the session protocol now names the ticket step. `.github/` gains a PR template, two
issue forms and `config.yml`; `CONTRIBUTING.md` is the front door.

**Two rules adopted.** From PR #15: **never quote a chat verbatim** in anything published -
commit messages, PR bodies, tickets, comments, `docs/`. The observation is evidence; the
wording never is. And: **an agent never declares a release.** It may report that a milestone is
clear and ask.

**Not done, and it needs the user.** The `Released` status does not exist yet and the PR
automation rows are unset: the Linear MCP exposes no tool for either, and both are team
settings. More importantly **the magic-word link did not fire** - Linear received the PR edits
(the diff records updated) and created no link, so `linkedIssues` is still empty on all four.
The ids and magic words are correct, so this is the GitHub integration's issue-linking side
not being enabled for the repo. Until it is, the PR-to-ticket links are the plain attachments
created by hand and no status automation will work.

### 2026-09-04 - session 12: the crouch height rise, solved

- **The answer**: the 38.16 deep-crouch capsule write moves the pawn 20.00 uu on every crouch (the
  engine keeps the feet planted, so a shorter capsule means a lower origin), and the camera's
  rate-limited catch-up never converges before the next crouch. `[PosTrack] DeepCrouch` now
  defaults to 0. Headset-judged: fixed.
- **The method lesson**: naming a mechanism and flipping its lever failed FIVE times running
  (physical crouch, the engine's uncrouch arithmetic, our eye clamp, the game's bump smoother, our
  own crouch eye-drop). What worked was filming the pawn and the camera one line per frame across
  the transition and letting the shape of the curve name the moment. When levers keep coming back
  null, stop naming suspects.
- **Two A/B traps paid for**: an interleaved A/B measures a system with memory BACKWARDS (the eased
  eye clamp redistributed the effect across the cycle boundary and the per-cycle median reported
  the sawtooth, not the climb - blocks with settling cycles fixed it); and a lever that was never
  connected reads as a clean FALSIFIED (the eye clamp had never executed at all). Confirm a lever
  moved something before believing its verdict.
- **A symptom mentioned in passing was the mechanism speaking**: "I could float around" turned out
  to be the pawn genuinely airborne, filmed rising 34 uu and falling 128 uu back to the floor.

### 2026-09-04 - session 13: the stale RIGHT eye, read out of the code, then confirmed

Branch `swapchain-one-picture-flicker`, 6 commits, one headset run at the end.

The hand-off pointed at `SceneDrawMaybeSecond`'s unlogged early returns. They are a dead end,
and ruling them out is what found the fault: `!doubleIt` means pass 1 pushed no `-1` at all,
the poison logs an `Error` at its one site and stands the method down, and the forced skip IS
the `forced=` field. So the game side pushed both tags on all 20 stale submits, every zero on
the line was true, and the second `-1` had to be manufactured downstream of it.

It was, in `reentry.cpp`'s c5 pairing block, whose two arms are not equally trustworthy. Full
reasoning in the FIXED section above and two entries in ARCHITECTURE's decision log. The
asymmetry was the tell: only the `-1` arm can misfire, and only a wrong `-1` strands the right
eye. `L=0 R=20` is that, arithmetically. **Confirmed in the headset**: 36 stale lines in 171 s
became 1 in 238 s, sc one-picture 36 % became 0-1 of 40, `sc-target repeats` 80 became 2, and
`c5Held=36` counts the deferrals that did it.

The second finding is about the instrument, and it is the more expensive one. The STALE line
carried the game side's gates and the runtime's failures but **not one counter from the method
between them**, and its owner string mapped `abortLeft` straight to "the game side skipped pass
2" - a cause it never measured. Nine logged instances, three readers, all sent to the wrong
file. Worse, the block's unconditional `tagged = true` made `method untagged presents` read 0
*because* a tag had been invented: the counter did not merely miss the fault, it denied it.

**And a process trap that cost a run**: the first "it's fixed" verdict came from a run of a
build that did not contain the fix. `build.ps1` had run, `install.ps1` had not. The log banner
and `Get-FileHash` against `build\src\RelWithDebInfo\d3d9.dll` catch it in one command, and
that check now leads the rig notes.

| Change | What |
|---|---|
| `1507bafc` | the cross-tick arm defers to the ring (streak 3 still overrides); no invented tags on an empty or 0 ring; `sameEyePushed` counted at the push site; the c5 counters and a corrected owner string on the STALE line |
| `12c23588` | ported: `[Stereo] HoldUntagged`, default 0, `stereo hold <n>` - now the lever for the residual mono flick |
| `539b9391` | ported: the `pair geom` separation-angle line, every 2 s |
| `9620d437` | ported: F2 stamps the fault marker, eyes-free |
| `00b833a8` | ported: the `res` seam writes its ini path with a real separator |

**All three fixed, each confirmed in the headset before moving on.** The chain: the c5 fix
exposed a mono flick on the arms (disparity is largest on the viewmodel, so a mono present shows
there and nowhere else); `HoldUntagged=3` removed that and exposed a black frame in both eyes;
the black frame was a zero-layer `xrEndFrame`, because the layer assembly sits inside
`if (backbuffer)` and a held present hands in no texture. Final run: `held=25 black=0`, `STALE`
0, `sc-target repeats=0`, one-picture `sc=0`, `out/s` median 201.

| Change | What |
|---|---|
| `1507bafc` | the cross-tick arm defers to the ring; no invented tags; `sameEyePushed` and the c5 counters on the STALE line, and a corrected owner string |
| `8441404f` | a zero-layer xrEndFrame re-submits the previous layer; `zeroLayerHeld`/`zeroLayerBlack` counted and logged |
| `12c23588` | ported: `[Stereo] HoldUntagged`, default 0 - the lever for the mono flick |
| `539b9391` `9620d437` `00b833a8` | ported: `pair geom`, the F2 fault marker, the `res` seam separator fix |

**Two process lessons, both paid for this session.** A parked commit's message is not evidence:
`HoldUntagged`'s claim that "the compositor holds the previous pair" was false against this
runtime and had never been run in a headset, and taking it at face value is what put the black
flicker in front of the tester. And check the build tag before reading a verdict out of a log -
the first "it's fixed" here was measured on a build that was compiled but never installed.

### 2026-09-04 - session 9: the eyes - the trace, the swap, the fix, the proof

Branch `claude/dishonored-vr-both-eyes-same-659cb5` -> PR #7, 13 commits. Runs on the dev PC
(simulator lane, RTX 4060, 2496x2688 VirtualMode, the sewers, shipped defaults; logs in
`D:\dvr-data\logs\45-run*.log`) and the user's Quest 3 through VirtualDesktopXR:

| Run | What | Result |
|---|---|---|
| 01 | the trace and the words, first build | `stereo: frameid` pairs from the arming: L-R 4.1 at bb/slot/out, floor 1.5, c5 6.17, busy 0; `reentry rearm 2` -> SINGLE x2 then DOUBLE; `capture reinit` -> REBUILT, no STALE; `dump eyes` queued + written off-thread, no gap, no LOADING; the sc stage empty (an ordering bug) |
| 02 | the sc stage, the side check | sc reads (4.7-5.0); **the side flipped across `reentry rearm 2`** and within a second of the first arming; the ring overflowed 363 times in the menu |
| 03 | the 0-tag push + the c5 pairing (first form), the A/B | side ok from the first pair; `reentry c5pair off`: the side flipped on its own twice in 25 s, `untagged 16-19` per window; on: no flips |
| 04 | the invariant as the pairing, the picture shift | side ok + shift -1 px on every pair, P1 == P2, untagged 0-1; `reentry.xrs` 11/11 |
| 05 | the drain to the next expected tag | side ok from the first pair across a `stereo mono` -> `reentry` switch and a rearm; 0 ring drops |
| 06 | the F10 EYES block | the overlay renders the readout and the buttons in the headset's own view (`xrsim-shot`) |
| 07 | **HEADSET (the user)**, the session's build | the eyes RIGHT from the load and after every button; `side ok` / `SWAPPED=0` on every pair, c5 6.11, shift negative, L-R 3-14 (one picture = 1.5); the ring skewed 131 times in ~4 min - the old swaps, absorbed; the trace read every present cost 1.5 ms GPU idle per present, tick 16.7 ms (60/s under 72 Hz) |
| 08 | **HEADSET (the user)**, the sampled trace + the A/B | "the fps is perfect now"; `c5 pairing` OFF + pause/resume -> the fault returns (`swapped=24 of 25`, then 12 of 12, the picture agreeing), ON -> `swapped=0` for the rest of the run. **The root cause is proven.** |

### 2026-09-03 - session 8: performance - the tick budget, the census, the 9Ex device, the shared capture

Branch `claude/dishonored-vr-perf-9f4b10`, 20 commits. Runs on the dev PC (simulator lane, RTX 4060,
2496x2688 VirtualMode, logs in `D:\dvr-data\logs\44-run*.log`):

| Run | What | Result |
|---|---|---|
| 01 | the tick budget, sync / deferred / off | stereo sync: tick 46 ms (21/s), capture 17-21 ms per present of which lock 9-13, GPU dma 15.5-16.8 vs 3D 4.8; deferred: 36 ms (27/s), lock 0, dma 10.4; off: 93 presents/s pace-bound; the marker 1 BeginScene per present, 0 late GPU reads; `mark` and the gap line print; `reentry.xrs` 11/11 |
| 02 | the creation census | 8060 of 8120 creations MANAGED (398 MB), READONLY texture locks 10598, no AUTOGENMIPMAP; the shadow route decided |
| 03 | `[Device] Ex=1 Managed=shadow` | `CreateDeviceEx -> 0x0`, IS 9Ex, `shared surface AVAILABLE`; 5240 twins, 65552 updates, 0 failures; the sewers intact; shared (one slot) 0.2 ms per present |
| 04 | the fenced two-slot shared capture, stereo | SharedWait=1: tick 13.3 ms (75/s), lock = the 3.6 ms fence wait, dma 0.2; SharedWait=0: 11.1 ms (90/s) PACE-BOUND; `reentry.xrs` 11/11; hammer 0 stale over 5 cycles; the frame intact |
| 05 | the final build | deferred default 27.7/s; `capture mode off` with 0 STALE lines (the no-frame fix); `focus lose 2500` / `focus regain`: `eaten=0`, 0 stale; `reentry.xrs` 11/11 |
| 14a | **HEADSET (the user)**, Ex=0, deferred | 30-33 ticks/s at 2496x2688 (dma 10.9 ms per present), 50 gap lines, no attack freeze felt |
| 14b | **HEADSET (the user)**, Ex=1 | 9Ex device up, shared AVAILABLE but the capture stayed deferred (30 ticks/s); after repeated quickloads the twin map filled with tombstones and the game crashed in D3D9 (minidump `dvr_20260903_212436.dmp`) |
| 06 | the tombstone fix, Ex=1 + shared, 3 quickloads | 2324 live, 1984 tombstones reused of 32768, 0 failures, the game alive; 90 ticks/s pace-bound |
| 15 | **HEADSET (the user)**, Ex=1 + shared | "performance is pretty good"; the eyes disagree "90 % of the time, more at the beginning": 0 STALE, 0 tag mismatches, pairs one IPD apart, 32 pause/resumes each with a 1-1.5 s flat spell |
| 07 | the read fence + the menu resume, shipped defaults | `readWaits` 14 in the run (the race was real); hammer 10 cycles 0 stale, `view live at once` x11; `reentry.xrs` 11/11 |

### 2026-09-03 - session 7: the four headset faults and the picker, on the simulator

Branch `claude/dishonored-vr-stereo-polish-449d43`, 15 commits. Runs (simulator lane, logs in
`D:\dvr-data\logs\43-run*.log`; the run-40 headset log archived as `42-run40-quest3-verdict.log`):

| Run | What | Result |
|---|---|---|
| 01 | commits 0-1d | `Method=mono applied after the game side registered`; the gate decision logs its reason; `reentry.xrs` 11/11, `stale-eye.xrs` 18/18; hammer 10 cycles PASS, ages L=1 R=0 |
| 02 | `reentry skip2 120` | strict off: sim stale 0 -> 2, mono +62, `STALE R EYE` (owner first "unknown", then the game side); strict on: stale unchanged, 37 fallbacks to mono |
| 03 | the phase + ahead | runtime clock extension on the sim; phase +58 ms mean (synthetic); `vrpace ahead 1` logs and locates; hammer 5 PASS |
| 04 | pitchtest x3 | engine neck 0.321/0.062 m (cons 0.3 uu); the arc reached c5 on top of it; `neck cancel` -> travel < 0.5 uu; the picture agrees |
| 05 | Armed + console | park/re-arm on the seam correct; the first console word overflowed the game thread's stack (the hook re-entered) |
| 06 | the guard, boot | `Method=reentry Armed=1 -> active reentry` before the first present; `setres 2560x1440f`/`1600x900w` dispatch, empty reply, no Reset: INERT |
| 07-08 | the ini route | 2560x1440 fullscreen in every ini place -> `CreateDevice 1920x1080 windowed=1`: the ini is inert |
| 09 | the command line | `-ResX=2560 -ResY=1440 -FullScreen` via 3 import slots -> `CreateDevice 2560x1440 windowed=0`, capture and swapchains followed |
| 10-11 | 2496x2688, no VirtualMode | the game asked the mode list, fell back to 2560x1440 (a harness launch had restored the mod ini; the launch file carries the token now) |
| 12 | **2496x2688 with VirtualMode** | our mode handed at slot 123; `CreateDevice 2496x2688 windowed=1`; `res: HONOURED`; hfov 108 deg; both eyes 77 % non-black in the sewers; the frame complete; readback 18-20 ms/present |
| 13a | **HEADSET (the user)**: 2560x1440 fullscreen asked | Reset 2560x1440 twice then 2508x1411 windowed=1 (the game's own fallback); the run sat in menus, stereo never armed (`state` skips); readback 5.3-5.8 ms/present |
| 13b | **HEADSET (the user)**: 2496x2688 VirtualMode | `CreateDevice 2496x2688 windowed=1`, HONOURED, "pretty sharp"; `neck cancel` right; stereo L/s=R/s=16-28, ticks 28/s, readback 13-15 ms/present; one `STALE L EYE` (age 567) at a FOCUSED regain; the desync still seen on load; judder unjudgeable |

### 2026-09-03 - session 6: S2b - the capture cost, the lanes, the root, the second draw

Branch `claude/s2b-stereo-scene-draw-a341c5`, seven commits on `VR-Main` (24b22390): the
capture cost measured and the modes, the pipelined deferred capture, positional tracking on
the camera seam, the projection claim and the FOV handoff with the state-gate fixes, the
root derivation and the second draw, the docs. Runs on the dev PC (simulator lane, logs in
`D:\dvr-data\logs\42-run*.log`):

| Run | What | Result |
|---|---|---|
| 16 | capture modes | probe: shared REFUSED; sync 5.4 ms, deferred (first form) 5.2 ms: no gain; `mono.xrs` PASS both |
| 17 | user-memory surface | REFUSED (D3DERR_INVALIDCALL), fell back to sync |
| 18 | the lock split | sync: lock 2.4-3.1 ms, copy 0.7, upload 1.5; deferred first form: the lock still waits |
| 19 | deferred pipelined | lock 0, total 2.25-2.4 ms; `mono.xrs` PASS |
| 20 | postest | camera lane HONOURED on all axes - on the attract camera (the state mislabel found in run 21) |
| 21 | projection on the mono screen | two views, sensor 137, claim readback; the pictures were the title screen, then the loading screen (DISCARDED there), then the sewers: eyetest 120/120, postest +30.0 in real gameplay |
| 22-24 | the state gate | menu/cine tracking hoisted, main-menu flag, LOADING state, the cinematic latch cleared on a new pawn: title MENU -> quad, load LOADING -> quad, level GAMEPLAY -> projection |
| 25 | 1440x1440 | the game stayed 1920x1080 with ResX/ResY=1440 in both ini places; second aspect open |
| 26 | census + scrapes | PVR from one site once per present; render thread presents; the draw chain to the HUD PostRender |
| 27 | tick chain + probes | both chains under UGameEngine::Tick; the root 0x5fc5b0 named from the bytes at 0x6330da; pe-xref confirms every edge |
| 28 | first light | pulse: 3 second draws at 218-414 us, presents +1 each; `stereo reentry`: L/s 54 R/s 53, pair c5 travel 6.17 uu, no fault; the ring cleared every few seconds |
| 29 | the ring fix + soak | L/s 52 R/s 52 mono 0, ringCleared 0, 90 s clean; `reentry.xrs` 11/11 |
| 30 | **the headset (Quest 3, VDXR, the user)** | the doubling ran (draws 54 = 2nd 54, presents 108, pair 6.08 uu) but L/s=36 R/s=54 mono/s=18: left tags dropped by the position check while walking -> both frames in both eyes; lean reversed, a second motion on pitch (the head's displacement and roll not driven under the projection layer) |
| 31 | the fixes, sim | tags never dropped: L/s 53 R/s 53 mono 0; `reentry.xrs` 11/11; the lean under projection read 13.7 uu for 30 cm (the reference had crept) |
| 32 | the stable reference | 30 cm -> +29.4 uu held for 10 s, crouch/forward signs right, postest HONOURED; CINEMATIC stuck across the load (the pawn latched before the title toggle) |
| 33 | the cinematic latch | `cine: latch cleared - leaving the main menu`, LOADING -> GAMEPLAY, L/s 53 R/s 53 mono 0 |
| 34 | HEADSET (the user) | stereo good; tilt and lean reversed in all four directions, also with `stereo projection on` |
| 35 | the lane picture test | 2 m right / 2 m up on both lanes: the camera lane MIRRORED the vp lane on both axes - the field's sign |
| 36 | sign +1 | both axes match the vp lane by picture; eyetest HONOURED 119/120 (c5 -99.2 for +100), postest HONOURED both axes, L/s 52 R/s 51 |
| 37 | roll by picture | roll write lands (incoming = wrote); +20 right-ear-down leaned the verticals RIGHT - reversed |
| 38 | roll negated | +20 leans left, -20 right; forward axis matches the vp lane; pause/resume re-pairs cleanly (L/s = R/s, mono 0) |
| 39 | the verdict logger | `gameplay verdict: FALSE (menuOpen) ... -> the head-locked quad` 30 ms ahead of the runtime's own line |
| 40 | **HEADSET (the user), the verdict** | PASS: stereo depth, tilt, lean, look, crouch all correct. Open: the arming glitch (right eye), judder on fast movement, the pitch pivot behind the camera, and the F10 resolution picker + arming tickbox |

### 2026-09-02 - session 5: the state as session 5 left it (archived)

**The render is restarted on a native D3D9 game.** The DXVK fork, the side-by-side present
pipeline, the 4032x2268 window machinery, the OpenVR backend and the mod's own OpenXR
loader/pace thread/input are removed (one commit each, so `git revert` restores one piece;
history keeps the fork under the `dxvk-*` tags). The BioShock trilogy mod's OpenXR runtime
layer is the single backend (`core/vr/openxr_runtime`, verbatim behind two D3D9 seams: the
device provider and the frame texture), the static Khronos loader is linked into `d3d9.dll`,
and SteamVR rigs go through the bundled `dvr_steamvr32.dll` shim. Stereo is a SEAM with named
methods (`core/gfx/stereo.h`: `[Stereo] Method=mono|aer|reentry`, `stereo <name>` live):
the mono screen (rung 1) works, `aer` and `reentry` are registered design stubs with their
notes. The per-eye camera seam (`game/dishonored/camera`) carries rotation (measured), FOV
(measured) and the lateral eye offset (unmeasured, with the `camera eyetest` instrument).
Version 41.0.0, `[Meta] Version=10`. ARCHITECTURE and ROADMAP (S0-S3) describe it.

**Verified on the dev PC (the game IS installed here, `D:\SteamLibrary`), simulator lane**,
build `g4fb67333` and later, 2026-09-02 evening, eight runs:

- `xrsim-selftest.ps1` PASS; `xrsim-launch.ps1 -ViaSteam` reaches `xr: instance created on
  runtime 'dvr-xrsim'`, `xr: runtime "dvr-xrsim"`, `xr: pipeline READY`, session FOCUSED,
  `xr: first frame submitted to the headset (1600x900 quad)`, frames advancing at the sim's
  90 Hz (`stereo: beat method=mono out/s=90`).
- `status.json`: `state GAMEPLAY` (the game auto-continues into the last save), `stereo.method
  mono`, `framesOut` advancing, capture bbox `100% x 100%`, 97% non-black, `camera.c5ok true`.
- `stereo aer` / `stereo reentry` refuse with their note and mono keeps running; `stereo mono`
  is a no-op; `camera status` prints.
- `xrsim-shot`: a quad layer whose SOURCE reads 97.2% non-black in BOTH views with a full bbox;
  the composite reads L 37.95% / R 37.98% (world-locked quad, run 7) then L 16.4% / R 16.3%
  (head-locked quad, run 8), no `COMPOSITOR fault` / `APP fault` line. The session-4 black
  left eye did not reproduce; the simulator now attributes it if it does.
- `dump frame` writes `capture_*.bmp` (5.7 MB) and `eye_*_mono.png`; `soak.ps1 -Minutes 3`
  exit 0 (PASS, no wedge, no dumps); the crash file carries the run headers.
- `camera eyetest 100` in gameplay: run 7 wrote nothing (the lever off = no camera revalidation;
  fixed), run 8 wrote all six candidates and measured a CONSTANT offset between the draw's c5
  and each field (+6620 uu for 0x80/0x90/0xc4, +14140 uu for 0x330/0x350/0x374 along right):
  the fields are not c5's quantity in c5's frame, so the measure was redesigned around a
  per-candidate c5 baseline (commit `cf9ec6f2`). Runs 10-11 (after the stuck process cleared
  on its own, no reboot): **camera+0x330 HONOURED 119/120** (+99.2 uu of the asked +100; it
  holds -c5 exactly, so the write is negated), the other five DISCARDED. The eye-offset write
  point is measured; `[Camera] EyeField=0x330` is the default (ENGINE_NOTES has the table).
- `mono.xrs` PASS (both eyes 12.9%, equal bboxes, no fault line) and the head-lock pair on the
  fixed simulator: the composite bbox is IDENTICAL at yaw 0 and yaw 30 (run 9).

**Found on the way** (each fixed in its own commit, all measured, none guessed):

1. The game calls `Direct3DCreate9` twice; the second `init_instance` failed with
   `XR_ERROR_LIMIT_REACHED` and the fallback chain declared VR off (guarded).
2. The handoff's trap 6 is real: a DIRECT exe launch crashes at the main menu
   (`Dishonored.exe+0x60907e` reading NULL, thread "other", right after
   `DisGFxMoviePlayerMainMenu Start`); a Steam launch survives it. `xrsim-launch.ps1 -ViaSteam`
   exists for this and is the only way to run the simulator with the game here.
3. The agent's shell on this PC VIRTUALIZES writes under the user profile: files the harness
   wrote to `%LOCALAPPDATA%\DishonoredVR` (the sim manifest, `command.txt`) existed for the
   shell and a game it launched directly, and not for a game launched through Steam (its
   listing held only game-written entries; a WMI-created `dir` agreed). `[Paths] DataDir=` in the
   ini and `DVR_DATA_DIR` for the scripts point both at `D:\dvr-data`; the simulator takes its
   state dir from the manifest's directory (VERIFICATION gotcha 14).
4. The loader's property store beats the environment: `init_instance` hands `[VR]
   XrRuntimeJson` to `xrInitializeLoaderKHR` (XR_EXT_loader_init_properties) as well, and logs
   whether the manifest is readable and its library loads.
5. The config's version rewrite dropped `[VR] XrRuntimeJson`; it now carries `XrRuntimeJson`,
   `Runtime` and `DataDir` over.
6. The fresh ini armed `FovLever=130` (the side-by-side value) and wrote it 600 times per 3 s
   into a 90-deg camera; the lever ships off on the mono screen.
7. The mono quad sat in BioShock's world-locked LOCAL space; it is head-locked now
   (`[Screen] HeadLocked=1`).
8. The SIMULATOR composited quads in the wrong place (60 px outward per eye at yaw 0, 460 px
   of swing at yaw 30): the cbuffer matrix was read column-major and the view matrix's rotation
   block was transposed. Fixed (`d43eea11`), selftest PASS, and the re-measure passed (run 9:
   identical bboxes at yaw 0 and 30). BioShock's eye legs never saw it (projection layers
   rotate rays in the shader).
10. The c5 capture only caught an upload STARTING at register 5; after the two device Resets
   a level load brings, the engine batches it into a c0 x128 block and the seam saw no c5 for
   a run (run 9). Any block covering c5 feeds it now (`dd10da09`).
9. Quitting: `console exit|quit` returns -1 (the console seam does not reach a quit); WM_CLOSE
   logs `ViewportClosed` and then the process LINGERS with one thread, unkillable (no `PreExit`,
   no `proxy unloading`), which then holds `d3d9.dll` and the simulator DLL open and makes Steam
   refuse a relaunch. This ended the session's runs; a reboot clears it. Run 6 also logged an
   access violation inside `d3d9.dll+0x87c95` (VR disabled, right after a device Reset following
   a `GetRenderTargetData` failure on a multisampled backbuffer), thread "other", three times at
   page ends 5.3 MB apart; the process survived it. Unsymbolized (that build is gone); the Reset
   + AA path is the first suspect.

**Headset: verified** (run 13, 2026-09-03): Quest 3 through VirtualDesktopXR, the game on the
head-locked screen in both eyes, head tracking and the gamepad working. The quit crashed on
that run (the 38.79 class: VD's thread through a freed d3d11 pointer after PreExit with the
session still open). The handler had sat inside the motion-aim block since 38.79 and never
ran under GamepadOnly; hoisted, it closes the session from PreExit and the third quit (run
15) was clean: `shutdown: game PreExit`, `xr: session teardown`, `instance destroyed`,
`proxy unloading`, no exception.

**Not verified**: the SteamVR shim with
Dishonored; `apply_eye_offset` driving a real per-eye render (no method asks for an eye yet);
`head_track`/`pad_bridge` as real modules (deferred, S1).

#### Session 5 next steps (superseded by the list above)

**Both**: the recipe on this PC is `tools\build.ps1; tools\install.ps1;
$env:DVR_DATA_DIR='D:\dvr-data'; tools\xrsim-launch.ps1 -ViaSteam` (the game ini carries
`[Paths] DataDir=D:\dvr-data`), foreground the window, `tools\xrsim-run.ps1 -Path
tools\xrsim\mono.xrs -Dir D:\dvr-data\xrsim`. Close the game with Stop-Process while it is
healthy, never with WM_CLOSE (blocker below). Copy `dishonored_vr.log` out before every
relaunch. The eyetest is done: the eye offset writes into camera+0x330 in negated form
(`camera::apply_eye_offset`); `camera eyetest 100` re-measures it on any build.

**Developer A (AlternateEye, S2a)**: read `core/gfx/aer.cpp`. The eye field is measured
(0x330), so the method only has to alternate `eye_for_next_frame()` and tag each present; the
seam writes the offset on the script lane. Acceptance: `stereo aer`
accepted, the beat line `L/s == R/s == out/s / 2`, `stereo.xrs`, `eye-check.ps1` legs 0-5, the
runtime's pair probe clean.

**Developer B (SequentialReentry, S2b)**: read `core/gfx/reentry.cpp`. Task one is the
scene-draw root (caller census at `ApplyHeadToViewRotation`, live stack scrape, identify the
pass by making it MOVE with the eyetest as the mover); every address to `patterns.h` with its
derivation in ENGINE_NOTES; the second call deny-by-default and SEH-guarded.

**The user**: the headset run on Quest 3 via VDXR: `tools\install.ps1`, launch through Steam
with VD streaming and VDXR active (SteamVR not running), expect the game on a head-locked
screen in both eyes, head rotation turning the view, the gamepad working; F10 for the screen
size; send `dishonored_vr.log`. Quit through the game's own menu and report whether the process
lingers.

#### Session 5 blockers (superseded)

- **WM_CLOSE leaves a stuck `Dishonored.exe`** (one thread, unkillable, holds `d3d9.dll` and
  the build's `dvr_xrsim32.dll`, Steam refuses a relaunch). Pid 13452 cleared on its own after
  about an hour, no reboot. Until the quit path is understood, close a healthy game with
  `Stop-Process`, which works.
- **The quit path**: `console exit` returns -1; WM_CLOSE leaves the process lingering with no
  `PreExit`; the runtime layer's teardown therefore never runs on a graceful close. Which
  thread is stuck (the simulator's, the runtime's, a driver's) is unknown; a debugger on the
  next occurrence, or a minidump taken by hand before killing it.
- The headset run needs the user.

### 2026-09-02 - session 5: the native-stereo foundation (41.0)

The decision (docs/ARCHITECTURE.md decision log, session 5): four headset sessions showed the
DXVK side-by-side design cannot be tuned; the game renders natively again and stereo is rebuilt
as a ladder of methods on one seam, two developers taking rungs 2 and 3. One PR
(`claude/native-stereo-foundation-77e2b6` -> `VR-Main`), 27 commits: seven removals, the
static loader, the runtime layer, the shim, the stereo seam + mono screen, the camera seam +
eyetest, the stubs, the simulator instruments, the harness, the docs, then the fixes the
first runs demanded (above, "Found on the way").

Runs on the dev PC (simulator lane; logs in `D:\dvr-data\logs\41-run*.log`):

| Run | Launch | Result |
|---|---|---|
| 1 | direct exe | instance on dvr-xrsim, then init_instance twice -> VR off; the game CRASHED at the main menu (`Dishonored.exe+0x60907e`, trap 6) |
| 2 | Steam | the ini rewrite dropped XrRuntimeJson -> VDXR (no headset), flat |
| 3 | Steam | env var set but the loader answered RUNTIME_UNAVAILABLE |
| 4 | Steam | loader property override set; manifest "path not found" (err 3) - the sandbox finding |
| 5 | Steam | the path probe: the game sees 5 entries where the shell sees 7 |
| 6 | Steam (no VR) | an AV in `d3d9.dll+0x87c95` after a Reset + RTD failure, three times, survived |
| 7 | Steam, `D:\dvr-data` | **dvr-xrsim, FOCUSED, first frame submitted, GAMEPLAY, both eyes 38% non-black, soak PASS 3 min**; eyetest NOT WRITTEN (null camera) |
| 8 | Steam | head-locked quad 16% per eye but swinging with yaw (the sim's quad math); eyetest wrote, measured the field/c5 offsets; WM_CLOSE -> the stuck process |
| 9 | Steam (after the process cleared) | `mono.xrs` PASS; head-lock pair IDENTICAL bboxes on the fixed sim; no c5 (the c0 x128 block) |
| 10 | Steam | c5 back; eyetest: 0x330 reads -c5 and moves c5 by -98.7 uu (75/76), the rest discarded |
| 11 | Steam | sign-aware seam: **0x330 HONOURED 119/120 (+99.2 uu)**, five DISCARDED; `camera eyefield 0x330` |
| 12 | Steam + Quest 3 (VDXR) | flat: a stale `[VR] XrRuntimeJson` (the sim manifest) made the loader fail; fixed to warn and ignore (`21e1cb64`) |
| 14 | Steam + Quest 3 (VDXR) | quit crashed again: the PreExit handler never ran (it lived inside the motion-aim block, off under GamepadOnly) - hoisted (`cf506ba4`) |
| 15 | Steam + Quest 3 (VDXR) | **clean quit**: `shutdown: game PreExit`, session teardown, instance destroyed, proxy unloading, no exception |
| 13 | Steam + Quest 3 (VDXR) | **THE HEADSET RUN: VirtualDesktopXR, Meta Quest 3, FOCUSED, READY, the screen in both eyes following the head, the gamepad working (user's report)**; quitting through the menu crashed 2.3 s after PreExit (EIP DEDEDEDE in d3d11.dll on VD's thread, the session still open) - teardown moved to the PreExit handler |

### 2026-09-02 - session 4e: gamepad-only, and the three rendering symptoms

Headset run at 4032x2268 requested / `capture: 3840x2160` actual. The tester reported three
things and they turn out to be one geometry. Full derivation in ENGINE_NOTES, "The three
rendering symptoms, and the one geometry that ties them".

1. **"Super pixelated, but the pause menu is huge like it's at full resolution."** Both
   halves are the same fact: SBS gives the WORLD half the frame width per eye
   (`per eye 1920x2160`) while a MONO menu frame samples the whole 3840 across the same quad.
   The menu is drawn at exactly twice the world's horizontal sampling density. That is the
   cleanest confirmation of the SBS packing anyone has produced, and it is not a bug - but it
   means the frame must be at least `2 x eyeWidth = 4992` columns for a 1:1 world. At 3840 the
   world sits at 77% of the panel.
2. **The fisheye is `FovLever`.** It does not only size the quad, it WRITES the game camera's
   FOV (`fov_lever.cpp`), so `FovLever=130` makes the game render 130 deg horizontal - the log
   agrees (`MEASURED render FOV ... = 130.0 deg`). A 130 deg rectilinear frame shown across a
   94 deg frustum stretches the edges. The author already knew: `frame_hooks.cpp` disarms the
   lever on overshoot, commented *"rather than leave the user in a fisheye"*.
3. **The black bottom border cannot be tuned away at 16:9.** Filling a 99 deg vertical
   frustum needs `lever = 2*atan(tan(v/2)*aspect)`: 128.6 at 16:9, 114.6 at 4:3, 100.5 near
   square. So the fisheye and the border are the SAME setting pulled in opposite directions,
   and at 16:9 nothing satisfies both. The tester found that empirically. A taller frame is
   not a preference, it is the only way out - which is exactly what they asked for.

**Next single change: `3840x2880` (4:3) with `FovLever` ~115.** Same per-eye width as now, so
no sharpness regression, +33% pixels, and it should visibly ease the fisheye.

**Two corrections to 4c, both mine.** (a) "Must be a real display mode" was too strong -
3840x2160 WAS honoured. The real rule is narrower: **`PinBackbuffer=1` causes the crop**; a
size the game rejects merely falls back, harmlessly, as long as the pin is off. Both effects
were present at 2850x2750, which made them look like one. (b) There is no 2560x1440 cap -
that was read from the run before the pin was turned off. 4032x2268 is still not honoured
(empty `setres` replies), so **trust `capture:`, never the requested number**.

**Applied this session:**

- **`[Mode] GamepadOnly=1`, new and default ON** (`config.cpp`). Turns off SkelControl hand
  writes, hand mesh, motion aim, motion melee, motion crouch and controller Blink aim, and
  scales no hand or weapon model. Head tracking, positional tracking, the FOV lever and the
  virtual gamepad keep running - this is NOT the `XR_SAFE` bisector, which also stops the
  head. It logs loudly and `status.json` gains `gamepadOnly` so the zeroes below it read as
  BY DESIGN rather than as failures. The author's rule that motion crouch and hands "must
  never stop working" is respected: nothing is retired, it is one key, set `GamepadOnly=0`.
- **The tester's tuned values are now the repo defaults**, in both the generated ini text and
  the `IniFloat` fallbacks, so a fresh install comes up where the headset testing left off.
- **`[PosTrack] Scale` default 50 -> 98.** This closes 40.2b: the tester tuned world scale by
  feel and landed on 98, within 2% of the 100 derived from the movement constants, arrived at
  independently and without seeing the number. That is the cross-check 40.2b was waiting for.
- Hand trims and `HandSize` reset to neutral, per the tester's "no scaling or changing the
  default hand/weapon models".

Build clean, exports 9/9 undecorated, lint clean, RelWithDebInfo installed. The fork and
`dxvk_stereo.txt` are untouched.

### 2026-09-02 - session 4d: the lever is half of the resolution setting

**The 3840x2160 run was full-frame but letterboxed** - tester: "almost sort of right again,
only problem was that the resolution was rectangular so it didn't fill my view". Both halves
of that are now explained, and one of them was my error.

**My error.** Session 4's restore set `FovLever=100`, correct for the near-square 2850x2750
it was paired with. Session 4c then changed the render to 16:9 and left the lever at 100.
The frustum-fill branch takes its vertical extent as `tan(fovDeg/2)/aspect`, so at 16:9 with
lever 100 the quad clamps to **67.7 deg inside a 99 deg frustum** - letterboxed by
construction. At lever 130 the same 16:9 render fills edge to edge with ~9% black at the
bottom. Changing the render aspect without changing the lever is NOT a one-variable change:
the pair is the variable. Table in ENGINE_NOTES, "FovLever IS the vertical fill lever".

**The requested resolution is not being honoured at all.** The mod asked for 3840x2160; the
log says `capture: 2560x1440`. With `PinBackbuffer=0` that is harmless - buffer and content
agree, no crop, which is why 4c's fix worked - but **this rig cannot render above 2560x1440**
(desktop 5120x1440 caps it). Every "4032x2268" run here is really 2560x1440. Trust the
logged `capture:` number, never the requested one.

**F10 SAVE AS DEFAULTS was finally pressed** (the thing session 3c said had never happened).
The tester's tuning is now persisted and snapshotted to
`tests/golden/f10-tuned-2026-09-02.ini` and `<game>\...\dishonored_vr.ini.f10-saved-0247`:

| key | default | tuned |
|---|---|---|
| `[Tracking] HeightOffsetM` | 0.000 | 0.040 |
| `[Screen] FillScale` | 1.00 | **0.74** |
| `[Screen] DistanceMeters` | 1.60 | 1.67 |
| `[PosTrack] Scale` | 50.0 | **100.0** |
| `CrouchToggle` | 1 | 0 |
| `XrLayer` | (absent) | proj |
| `StampFix` | (absent) | 0 |

`[PosTrack] Scale=100.0` matches the measured 100 uu/m from commit 60235b86 - the tester
converged on the measured value by feel, which is a good cross-check on that measurement.

**Applied on request: GingasVR's resolution default**, as the coherent pair -
`RenderWidth/Height=4032x2268`, `SpoofDesktopW/H=4096x2304`, **`FovLever=130`** - plus
`DishonoredEngine.ini` and all four AppCompat buckets at 4032x2268. `PinBackbuffer` stays 0
(her ini has no such key) and `MenuFillScale` stays 1.00 (the 4b fix). Every F10 value above
is preserved. Backups `.pre-gingasres` / `.f10-saved-0247`. NOT YET TESTED.

**Expect this**: the render will probably clamp to 2560x1440 again (fine, still 16:9), and at
lever 130 the quad should fill horizontally and to the top with ~9% black at the bottom -
**but `FillScale=0.74` will still present it at 74% of that**. F2 raises FillScale live in
the headset; that single knob is the difference between 74% and full. Judge the lever first,
then the fill.

### 2026-09-02 - session 4c: THE RENDER WAS NEVER 2850x2750

**This is the root cause.** The tester sent a desktop-mirror screenshot of the main menu with
the picture in the top-left of the window. The mirror blits the backbuffer's LEFT HALF
pillarboxed (`frame_hooks.cpp:415-460`), so its horizontal placement is expected - but the
picture filled only the top ~52% of the window, and that is not.

**Measured, six capture dumps, non-black bounding box:**

| requested buffer | actual content | real display mode? | verdict |
|---|---|---|---|
| 1600x900 | 1600x900 | yes | FULL |
| 2560x1440 | 2560x1440 | yes | FULL |
| 3840x2160 | 3840x2160 | yes | FULL |
| 4032x2268 (GingasVR's) | 3024x1440 | no | CROPPED |
| 2750x2850 | 2750x2200 | no | CROPPED |
| **2850x2750 (our "known good")** | **2560x1440** | no | **CROPPED** |

At 2850x2750 the game draws exactly **2560x1440 into the top-left** and leaves the rest
black. So the whole session's geometry was applied to a frame that is half empty. It
explains all three symptoms at once: tiny (content covers ~90% x 52% of the quad), top-left
(it is literally there), and the eyes not fusing (the SBS halves meet at x=1280, not the
x=1425 the split assumes, so each eye gets part of the other's view plus black).

**Why nothing caught it.** `PinBackbuffer=1` forces the DEVICE to 2850x2750 while the game
renders at the size it asked for (`CreateDevice the game asked for 2560x1440`). The mod then
spoofs `GetClientRect` to 2850x2750 and the setres path reads that spoof back, concluding
`setres: the game is already at 2850x2750 - skipping the resolution script entirely`. A
check reading our own spoof cannot fail its own hypothesis, so the engine-side resize never
ran and `capture: 2850x2750` was logged for a half-empty frame.

**`PinBackbuffer` is ours, not GingasVR's.** Her tuned ini (`.pre-2750`) has no such line.
Every ini since session 2 sets it to 1.

**This is probably the central open bug.** 4032x2268 is not a standard mode either and
cropped here too. Whether an injected mode is honoured depends on the machine's GPU, driver
and desktop mode - this rig's desktop is 5120x1440, and two of the three cropped captures
came back exactly 1440 tall. It predicts affected users have a desktop shorter than the
requested render height, and it is falsifiable by asking one for their desktop resolution.

**The Documents folder was ruled out**, on the tester's suggestion: `DishonoredEngine.ini` is
vanilla plus the intended VR lines, all four AppCompat buckets were already correct, and no
file in the Config directory contains 2560 or 1440.

**Applied for testing** (one coherent change: use a resolution the display actually offers):
`RenderWidth/Height 2850x2750 -> 3840x2160`, `SpoofDesktopW/H -> 3840x2160`,
`PinBackbuffer 1 -> 0`, plus `DishonoredEngine.ini` and all four `[AppCompatBucket1..4]` to
3840x2160. Per-eye half 1920x2160 = aspect 0.889, the same per-eye aspect as GingasVR's
4032x2268. Backups: `.pre-realmode` next to each of the three files. NOT YET TESTED.
Fallback if 3840x2160 is not honoured: 2560x1440, identical per-eye aspect, and the game
asked for it itself.

**Also confirmed this session**: the 4b MenuFillScale fix works. The 02:35 run logged 28 quad
rebuilds, all at `fill=1.00` / 100.0 x 98.0 deg, none at 0.60. The size pumping is gone.

**Next instrument to build**: nothing compares the captured frame's real content extent to
the buffer size. A non-black bounding-box check on the capture, logged once per resolution
change, turns this class of bug into one line. The setres check must also stop reading the
mod's own `GetClientRect` spoof.

### 2026-09-02 - session 4b: the world size PUMPS, and MenuFillScale is why

A headset run on the restored known-good ini (02:23, VirtualDesktopXR + Quest 3) reported
"still rendering tiny and in the top left corner". Its log names the cause outright, so this
did not need a new instrument. Full derivation in ENGINE_NOTES, "MenuFillScale pumps the
world size during GAMEPLAY".

**The number.** The quad subtends **71.1 x 69.2 deg** inside a frustum of **94.0 x 99.0 deg**
- about half its solid angle. That is "tiny", fully explained, and nothing to do with
resolution, adapter, world scale or convergence. 40 of the run's 46 quad rebuilds were at
`fill=0.60`; the run ended there.

**The mechanism.** `MenuFillScale=0.60` and the `XrFrustumFill` gate are driven by the SAME
condition (`g_menuOpen || g_inMenu || g_sbsMonoNow`), and a change in it forces a rebuild.
The menu flag flaps during gameplay (the `Req_SaveSlotInfos` save-slot polls, already
documented at `present.cpp:613-616`), so the world size pumped 100 -> 71 -> 100 -> 71 -> 100
-> 71 deg across the six seconds before the crash, all after gameplay had started. The
`sbs:` line proves it is the MENU flag and not the splice counter: its last transition is
well before the pumping began.

**Why the Index never saw it.** OpenVR has one geometry path, so `MenuFillScale` only ever
dimmed a menu. `XrFrustumFill` (38.13) added a second path for the OpenXR port without making
the transition continuous, so on Quest the same flap swaps the whole quad construction
mid-gameplay. The tester's own read - Index/SteamVR was the tuned target, OpenXR/Quest a
later port - is exactly right here.

**Applied, config only, one variable, no rebuild**: `[Screen] MenuFillScale 0.60 -> 1.00`
(backup `.pre-menufill`). The menu branch now builds the same 100.0 x 98.0 deg quad as
gameplay, so a flap cannot change the world size. Cost: menu edges crop, which is what 32.4
added the key to avoid. NOT YET TESTED.

**A falsifiable prediction, and it contradicts session 3c.** Worked from the logged frustum,
the authored quad's vertical border must be SYMMETRIC, ~21% black top and ~21% bottom, with
the world in the middle 57.6%; horizontally the left eye gets 29.8% black on the temple side
and 5.6% on the nasal side (mirrored in the right eye - the rigid-screen design). Session 3c
recorded "top ~54%, bottom half black". The next `dump eyes` settles it: symmetric borders
retire that contradiction as a misread dump; a real black bottom half falsifies this model.

**The run also CRASHED** at 02:23:49, wild instruction pointer, minidump at
`%LOCALAPPDATA%\DishonoredVR\dumps\dvr_20260902_022349.dmp`. Untriaged, separate lane.

**Good news in the same log**: the fork's projection export resolved this time -
`quad/fill: world scale is set by the MEASURED render FOV (fork dxvk_vr_proj) = 100.0 deg`.
The landscape fix (session 3b) worked; world scale is no longer an assumed constant.

### 2026-09-02 - session 4: reverted to the known-good point

No launches, no code changes. Session 3c left the rig one key away from its own
confirmed-good configuration and that key was an open, unevaluated experiment; this
restores the documented point so the next run starts from a known baseline.

**What was actually different.** Exactly one line: `FovLever=130` vs `100`. Everything
else already matched - `RenderWidth/Height=2850x2750`, `SpoofDesktopW/H=2816x2880`,
`PinBackbuffer=1`, `GameFOVDeg=100`, `FillScale=1.00`, `[PosTrack] Scale=50.0`, the game
ini at 2850x2750 on both `[SystemSettings]` and `[SystemSettingsEditor]`, and all four
`[AppCompatBucket1..4]` at 2850x2750. `setup-game-ini.ps1` did not need re-running.

**The snapshot checks out.** `tests\golden\known-good-2850x2750-lever100.ini`, the game
folder's `dishonored_vr.ini.KNOWN-GOOD` and `dishonored_vr.ini.pre-lever130` (the ini as it
actually ran when the tester reported "the eyes seem to overlap correctly and provide
depth") are all identical. The snapshot is a truthful record of the run, not a
reconstruction - worth stating, because it was written at 00:54 while the live file was
already at `FovLever=130`.

**Restored** by byte copy, verified with `cmp` against both snapshots. Prior state saved as
`dishonored_vr.ini.pre-restore-known-good`.

**`FovLever=130` is untried, not disproved - and it was the well-motivated direction.**
ENGINE_NOTES "FovLever and the render size are ONE setting" has it the other way round from
how session 3c's ordering reads: at lever **100** the clamp limit is 1.91 m against a
frustum reaching 2.20 m horizontally and 2.29 m vertically, so it fires on **all four
sides**, and `dump eyes` at lever 100 confirmed the world inset with a ~9-10% border on
every side. At lever **130** the limit is 3.43 m, outside the frustum edge, so nothing
clamps and the quad fills the eye. 130 is also GingasVR's own tuned value.

**So the restore knowingly reinstates the bordered configuration.** That is the right call -
lever 100 at 2850x2750 is the only point a tester has ever confirmed fuses with depth, and
an unevaluated experiment is not a baseline - but the border is a KNOWN artifact of this
baseline, not a new symptom, and "the render window is halfway up my vision" must be judged
against that. Lever 130 stays queued as the next one-variable change once the gameplay dump
is in hand.

**Caveat that still stands**: no F10 tuning has ever been persisted (SAVE AS DEFAULTS was
never pressed), so this ini is the only reproducible configuration that exists.

**Untouched deliberately**: the proxy, the fork, `dxvk_stereo.txt`, and the stale
`command.txt` in `%LOCALAPPDATA%\DishonoredVR\` (`command.cpp:153` discards and clears a
command file older than the process, so the next run's log should show that branch firing -
a free check of the new seam diagnostics).

**Next**: unchanged from session 3c. Launch, reach GAMEPLAY with the window focused, then
`tools\game-cmd.ps1 "dump eyes"` and read the two PNGs. The unexplained contradiction is
still the lead: the world occupied only the top ~54% of the eye render target while the
measured frustum (55 deg down against 44 deg up) says the quad should overflow vertically.
That dump was a MENU frame and needs confirming in gameplay before anything is changed.

### 2026-09-02 - session 3c: the seam went deaf, and the eye texture is half black

**START HERE.** The bug is not fixed and the last measurement is incomplete.

**The one thing to do first**: launch, get into GAMEPLAY (not a menu, window focused),
then `tools\game-cmd.ps1 "dump eyes"` and look at the two PNGs in
`%LOCALAPPDATA%\DishonoredVR\dumps\`. Everything below is waiting on that image.

**The live symptom** (tester, Quest 3 + VirtualDesktopXR): eyes will not fuse, and the
image sits high - "the render window is halfway up my vision so I only see the bottom
half of it". Earlier in the session, "everything looked tiny".

**The measurement that matters.** A `dump eyes` caught mid-session shows the world
occupying only the **top ~54% of the eye render target, bottom half pure black**, in both
eyes. That is our own D3D11 pass, before the compositor. It was a MENU frame (the dump
caught a paused game), so it needs confirming in gameplay - but the vertical placement is
the lead. The measured frustum is
`eye frustums: L[-1.376 0.839 -1.428 0.966] ex=-0.0316 | R[-0.839 1.376 -1.428 0.966]`,
slots `[left, right, down, up]`: **down-biased, 55 deg down against 44 deg up**. Worked by
hand at these settings the quad should span y -2.36..+1.62 against a frustum of
-2.29..+1.55, i.e. it should OVERFLOW the eye vertically, not sit in the top half. That
contradiction is unexplained and is the next thing to chase.

Same dump showed the two eyes holding **different content** (different horizontal extents,
different fragments of the same menu text). That is the mono/stereo UV race on menu frames
- the fork stops splicing, the frame goes mono, each eye still takes its own half. Probably
menu-only: in gameplay the splice count is 5000+ and the halves are a real stereo pair.

**The command seam went deaf and blocked the session.** `status.json` stale for 18 minutes
and `command.txt` unread, while the Present hook ran at 62 fps with 251 `[present]` lines.
Two `dump eyes` commands did nothing and left no trace. `poll()` had FIVE silent returns,
so the failure was indistinguishable from the command never being written. **Fixed and
installed**: each guard now names itself with the path, the size and GetLastError. If the
seam is still deaf next session the log will say which branch refuses.

**Theories killed this session (do not re-run them):**

- The `CopyResource` size mismatch (step 0a) - eye RTs and XR eye size matched exactly.
- The pace thread as the crash victim - the faulting thread is `(other)`, not `xr-pace`.
- The black left eye - a SIMULATOR defect, not the mod; `dump eyes` shows the left eye
  texture full. See VERIFICATION "Known simulator defects".
- Eye cant - `g_eyeRot` is declared identity and the XR path correctly leaves it alone.
- The clock/rate gate - `MaimNowMs` is fine (the 5 s `depth:` line printed 50 times against
  the 3 s heartbeat's 81). Note the log uses `GetTickCount` while the gates use QPC via
  `dvr::clock`, so an advancing log proves NOTHING about the gate clock.
- A game restart clearing the seam - it did not.

**THE KNOWN-GOOD STATE, AND HOW TO GET BACK TO IT.** This rig's best result so far -
tester: *"the eyes seem to overlap correctly and provide depth, there is no freeze"* - came
from `2850x2750` + `FovLever=100` + `Scale=50`, with the main scene splicing (5202). It is
worth more than GingasVR's own values, which come from a different machine and are the
subject of the project's central open bug ("works only on her PC").

Snapshotted byte-for-byte in two places, so it survives a wiped game folder:

- `tests\golden\known-good-2850x2750-lever100.ini` (in the repo)
- `<game>\Binaries\Win32\dishonored_vr.ini.KNOWN-GOOD`

Restore = copy either over `<game>\Binaries\Win32\dishonored_vr.ini`, then
`tools\setup-game-ini.ps1 -Resolution -Width 2850 -Height 2750` for the game ini and the
four AppCompat buckets. The keys, if you ever need to rebuild it by hand:
`[Screen] RenderWidth=2850 RenderHeight=2750 SpoofDesktopW=2816 SpoofDesktopH=2880`
`PinBackbuffer=1 GameFOVDeg=100 FovLever=100 FillScale=1.00`, `[PosTrack] Scale=50.0`.

**WARNING - the snapshot does NOT contain any F10 tuning, and neither did any run.** The
overlay's sliders are live-only until someone presses **"SAVE AS DEFAULTS"**
(`overlay.cpp`, top of the panel), which calls `OverlaySaveDefaults` and writes ~90 keys -
world scale, fill, screen distance, height offset, menu fill, wrist HUD, and the whole hand
/ graft / blink block. It was never pressed, so every F10 adjustment the tester made across
this session was lost at process exit; the only thing that persisted was the hand
calibration, which `skelcontrol.cpp` writes on its own. **Procedure from now on: tune in
F10, press SAVE AS DEFAULTS, then re-snapshot the ini.** Otherwise a good configuration
cannot be reproduced, which is exactly what happened here.

**Config as left, LIVE right now**: ~~`FovLever=130`~~ **RESTORED to the known-good
snapshot (session 4)**. The live `dishonored_vr.ini` is now byte-identical to both
`tests\golden\known-good-2850x2750-lever100.ini` and the game folder's
`dishonored_vr.ini.KNOWN-GOOD` (`cmp` clean against both), i.e. `FovLever=100`. The
`FovLever=130` experiment was applied but never evaluated - the seam went deaf before a
dump could be taken - so it was discarded rather than judged; it remains untried, not
disproved. The pre-restore state is saved as `dishonored_vr.ini.pre-restore-known-good`.
Game ini + all 4 AppCompat buckets verified still at 2850x2750 (`DishonoredEngine.ini`
lines 1081/1141, `DishonoredCompat.ini` buckets 1-4), so no `setup-game-ini.ps1` re-run
was needed.
Backups in the game folder: `.pre-2750` (GingasVR's own tuned ini), `.pre-landscape`,
`.pre-scale100`, `.pre-gingas-restore`, `.pre-rollback`, `.pre-lever130`,
`.pre-restore-known-good`.

**Installed**: RelWithDebInfo proxy only, 00:50, carrying the seam diagnostics. The fork
(20:24) and `dxvk_stereo.txt` (13:52) are untouched and must stay that way - one variable.
Re-verified session 4: the installed `d3d9.dll` is md5-identical to
`build\src\RelWithDebInfo\d3d9.dll`, and the tree is clean at `112105b7`, so source,
build and install all agree. The fork and `dxvk_stereo.txt` timestamps are unchanged.

**Do not repeat these mistakes.** Restoring GingasVR's baseline I changed render size, FOV
lever and world scale in ONE step, so "misaligned" was unattributable; her values also come
from a different machine, and the project's central open bug is that her build works only
on her PC. This rig now has its own confirmed-good point (2850x2750, splices 5202, tester:
"eyes overlap correctly and provide depth") which is worth more than her numbers. Change
one thing per run, and get the gameplay dump before changing anything at all.

### 2026-09-01 - session 3b: the render was PORTRAIT, so there was no stereo

A headset run mid-session reported "the eyes are suuuuper far off and they both appear to
be zoomed in", worse than before. Its log is the **first surviving headset log** and is
archived outside the game folder. Cause found, fix applied, not yet tested.

**Session 2's resolution fix set the render to 2750x2850, which is portrait, and the DXVK
fork refuses to splice the main scene on a portrait viewport.** `d3d9_device.cpp:4381`
sets the refusal reason `"rt-portrait"`; the per-eye splice at `:4578` runs only when that
reason is `SPLICE`. So the world was drawn **mono** across the full frame while the proxy
handed each eye a different **half** of it - unrelated views that cannot fuse, each
magnified 2x by the stretch onto the quad. Exactly the report.

**The splice counter lies about it.** Light shafts, shadows and the M8.1 quarter light pass
splice under different conditions and kept working, so `splices=85` while the main scene
never spliced once - which kept `g_sbsMonoNow` false and the half-frame UVs on. The fork's
own log shows only effects splices.

**The same gate kills the FOV measurement.** `dxvk_vr_proj`'s publish (`:5996`) is also
gated on `Width > Height`, so `g_liveFovX` was 0 all session, the frustum-fill path fell
back silently to the ini constant `GameFOVDeg=100`, and an assumed number set world scale.

**This falsifies session 2's "the eyes ARE a stereo pair".** 32.7 mean-abs-diff static /
11.5 after a head turn is exactly what two different halves of one mono frame produce. That
test could not distinguish a stereo pair from two unrelated crops, so it could never have
failed its own hypothesis.

**Applied**: `2850x2750` - the same two numbers swapped. Same pixel cost, landscape by
100 px, full-frame aspect 1.036 so the quad subtends 100 x 98 deg at `FovLever=100`.
Changed in `tools/setup-game-ini.ps1` (defaults + a header section on why landscape is
mandatory), applied to `DishonoredEngine.ini` and `DishonoredCompat.ini` via the tool (both
backed up), and to the game folder's `dishonored_vr.ini` (backup `.pre-landscape`).

**Instruments added so this cannot hide again**: a portrait capture logs an Error naming
the fork's own refusal string and the fix (`present.cpp`); the frustum-fill path now says
every 10 s whether world scale comes from the MEASURED render FOV or from the assumed ini
constant (`eye_quads.cpp`).

**Installed**: RelWithDebInfo proxy only (`install.ps1 -Release -SkipDxvk`) - the fork and
`dxvk_stereo.txt` are untouched, so the resolution is the only render-path variable. The
proxy's other changes (session 3 below) are the shutdown/pace lane and logging, which
cannot confound the zoom result.

**Not yet tested.** If the fix worked the portrait Error is absent and the eyes fuse; if
the Error appears, the resolution did not take and AppCompat is overwriting it again.

### 2026-09-01 - session 3: the crash fingerprint was misread, and why

No launches: everything here comes from artifacts already on disk plus the source. The
game is installed on this PC, but nothing was run.

**Two of session 2's conclusions are instrument bugs, not engine facts.**

1. **The exit crash is an EXECUTE fault, not a freed-memory write.**
   `ExceptionInformation[0]` is three-valued (0 read, 1 write, 8 execute/DEP) and the
   fingerprinter tested it for truth, so every execute fault has printed as "writing". The
   records prove it: `ExceptionAddress == ExceptionInformation[1] == 0xDEDEDEDE` with the
   module resolving to `?`. A data write would have left `ExceptionAddress` inside
   `d3d11.dll`. **EIP landed in freed memory: a call through a poisoned code pointer.**
2. **The faulting thread is not the pace thread.** All three records say `(other)`, which
   `thread_name()` returns only for a tid in no registered slot; `present` and `xr-pace`
   both register at entry. The faulter is a third-party worker (d3d11, driver, runtime).
   The pace thread can be the cause, but instrumenting it as the victim will find nothing.

**Two evidence channels were dead and are now fixed.**

- **`dumps\` was empty by construction.** 3 `EXCEPTION` lines, 0 `minidump` lines: proof
  that `unhandled()` never ran, because UE3's own filter/SEH frame consumes the fault
  before `SetUnhandledExceptionFilter` fires. The dump is now taken from the **vectored**
  handler (which always runs), gated on the instruction pointer resolving to no loaded
  module - fatal-only by construction, and falsifiable: an ordinary in-module fault
  produces no dump and disproves the wild-EIP reading. `dbghelp.dll` is resolved at
  `install()` time so the VEH never touches the loader lock.
- **The crash file had no run identity.** `FILE_APPEND_DATA` / `OPEN_ALWAYS` with nothing
  separating runs, and `dvr-xrsim` and VDXR produce byte-identical fingerprint text, so
  the three records cannot be attributed to a backend at all. Now one header per run
  (clock, version, build id, pid, backend + runtime name).
- **Log rotation is one deep**, so two simulator runs erased both headset logs; the
  survivors contain no `EXCEPTION` and no `PreExit`. Copy the log out before each launch.

**The author read this fault correctly and session 2 inverted it.** The 38.79 comments say
"EIP dededede" and "a call through freed memory". 38.79 acted on that by standing the
**game** thread down at `PreExit`, which was right but not the whole path - it left the
pace thread running with nobody waiting for it. Closed below.

**Two pace-lane defects fixed** (steps 0b and 0c):

- **`XR_TIMEOUT_EXPIRED` is a success code.** `XrResult` is negative for failure only, so
  `XR_FAILED()` is false for it and `!XR_FAILED(xrWaitSwapchainImage(...))` ran
  `CopyResource` into an image the compositor had not finished reading - a race with the
  runtime on the one resource the headset displays, invisible because every call returns
  success. Now `== XR_SUCCESS`. In the same block `g_xrpShown` advanced **before** the
  copies, so a frame lost to a timeout was dropped permanently instead of retried; it now
  advances only once both eyes actually received the content.
- **`XrPaceStop()` joins the pace thread**, bounded at 750 ms, replacing the bare
  `g_xrRun = 0`. On expiry the thread is left running on purpose - `TerminateThread` would
  orphan `g_xrCs` and abandon an acquired swapchain image, which is worse than the race -
  and the error line is the instrument: a fault after it means the pace lane is still the
  suspect, a fault without it means the thread was already gone and it is not. The event
  pump's inner `while` now tests `g_xrRun` so an event backlog cannot hold the loop past a
  stop request.

**Changed** (8 files, uncommitted): `src/core/util/crash.cpp` and `crash.h` (three-valued
AV decode; run header; `set_context`; shared `write_dump` with the wild-EIP gate),
`src/core/vr/openxr_backend.cpp` (names the runtime in the crash context),
`src/core/vr/openxr_pace.cpp` (the wait fix, the retry fix, `XrPaceStop`),
`src/game/dishonored/ue3/process_event.cpp` (`PreExit` joins), `src/mod/fwd.h`,
`docs/dishonored/ENGINE_NOTES.md`, `docs/STATUS.md`. Verified: Debug, RelWithDebInfo and
`-Legacy` all build, and both DLLs carry the new strings; `lint.ps1` clean; exports 9/9
undecorated. **Not run in the game, in the simulator, or in a headset.**

**Next**: step 0a - the `CopyResource` size mismatch, which is still only a code-reading
hypothesis. Then the full `xrRequestExitSession` / `xrDestroySession` shutdown.

### 2026-09-02 - session 2: instrumentation, resolution, and a real headset

**First session with the game actually installed and a real Quest + Virtual Desktop
headset on the other end.** Environment: proxy and fork both built from source with
MSVC (meson + ninja + glslang 16.5.0 standalone, no Vulkan SDK); `tools\build-dxvk.ps1`
needed two fixes to run at all on a PC with VS 2026 installed next to VS 2022.

**Fixed and verified**

- **Resolution.** `ResX` in `DishonoredEngine.ini` never held: UE3 AppCompat picks an
  `[AppCompatBucketN]` at startup and writes that bucket's ResX/ResY over
  `[SystemSettings]`. Buckets 3 and 4 ship 1600x900, and any GPU newer than the 2012
  table lands in one, so every modern machine started at 1600x900 forever. Fix: set all
  four buckets; `tools\setup-game-ini.ps1 -Resolution` now does this and defaults to
  2750x2850. Measured before/after: `CreateDevice (1600x900)` -> `CreateDevice
  (2750x2850)`, `capture: 2750x2850` on the FIRST device creation, no setres needed.
- **`setres` is a dead end.** Measured `setres 2750x2850w -> "(empty reply)"` with NO
  device Reset following. New `[Screen] PinBackbuffer=1` (default OFF) sets the size in
  the present parameters at CreateDevice instead. The 32.57 "image in the corner"
  objection is answered by the GetClientRect hook that landed later.
- **World scale.** `W` is pinned to the rendered FOV and `H = W / frameAspect`, so a
  squarer render makes a taller virtual screen than the lenses can show and the player
  sees a magnified middle. 2750x2850 at FovLever=130 subtends 100x132 deg; at 100 it is
  100x102, which matches the headset. FovLever set to 100.
- **The mono/stereo UV race.** `BuildEyeQuads` BAKES the sampling UVs, but the rebuild
  only fired on an aspect change or a menu toggle. `g_sbsMonoNow` flips during gameplay
  whenever the fork's splice count dips, so a mono frame could be sampled with stereo
  UVs and each eye got a different half of one mono image. Now a change in frame kind
  forces a rebuild, exactly like the menu flag.

**Instrumentation added** (see the new "Logging" section in `CLAUDE.md`)

- Full DXGI adapter enumeration, the LUID the runtime asks for, and the adapter read
  back OUT of the finished device with an Error-level mismatch line.
- `RESOLUTION CHANGED MID-SESSION` at Warn, `quad: ... subtends AxB deg` per rebuild
  with a Warn past 110 deg vertical, `res: the game asked for WxH`, and a `skc/gate:`
  line for the hand drive.
- The hands heartbeat now names the OWNER and reports that owner's counter.

**Corrected beliefs** (all three were believed and are wrong)

1. "The hand graft never attaches." It attaches fine: `OWNER=SkelControl writes=~406/3s`
   in gameplay. The old heartbeat tracked two counters that read 0 BY DESIGN - one a
   retired subsystem, one the legacy drive that is deliberately stood down. Three
   readers including the original author concluded "the hands are dead" from a healthy
   run.
2. "The 39.3 adapter bug needs two GPUs." DXGI enumerates the SAME RTX 4070 Ti SUPER
   twice on this PC (virtual display drivers), so the default adapter is not stable on
   single-GPU machines either. **But on the real VDXR run the runtime asked for
   adapter[0], which IS the default, so the LUID mismatch is NOT the cause of the
   symptoms on this rig.** 40.1 still fixes the class of bug and makes it visible.
3. "The eyes are not a stereo pair." Measured 32.7 mean-abs-diff when static, but 11.5
   after a head turn, which is normal parallax. Stereo works; the divergence is a
   symptom of the freeze, not the disease.

**Still broken: the freeze-then-rescale.** Reported again after all of the above. What
is ruled out: the resolution (it now stays 2750x2850), the adapter (matched), the FOV
(100), the mono/stereo UV race (fixed), and the hand drive (working). What is NOT ruled
out and is where to look next:

- The **shutdown crash is a teardown race** and may share a root with the freeze: three
  runs ended with `EXCEPTION 0xc0000005 writing 0xDEDEDEDE` inside `d3d11.dll`, two
  threads at once, immediately after `PreExit` stops the pace thread. 0xDEDEDEDE is
  freed-memory poison. The detached pace thread is touching released D3D11 objects.
- The pace thread owns every runtime call while the game thread owns capture and
  UpdateSubresource on the SAME `g_ctx11`; ID3D11DeviceContext is NOT thread-safe.
  `ID3D10Multithread` is enabled but that protects the device, not a stale pointer.
- Instrument the frame path next: log around the Reset/teardown boundary and around
  every `g_ctx11` use from the pace lane, and get a minidump analysed from
  `%LOCALAPPDATA%\DishonoredVR\dumps`.

### 2026-09-02 - session 1: development framework

Explored the 22,959-line `src/dllmain.cpp` and the BioShock trilogy mod; planned the refactor
with the user (decisions: CMake+MSVC; DXVK restored in-repo and kept as the stereo path; both
backends kept behind one pipeline; retired code to `src/legacy`; a proper logging/debugging
surface). Executed: DXVK restore (52 patch commits + the M8.4 revert; `fork-patches/` removed),
submodules and vendored OpenVR, CMake scaffold and MSVC port (naked stubs, `.def`,
`_ReturnAddress`, `ID3D10Multithread`), the unity split, Phase 2 utilities, harness copy and
adaptation, simulator build + selftest PASS, debug surface, patterns.h, legacy gating, backend
probe, docs. Found: `dxvk_vr_view` is resolved by the proxy but absent from the published
patches (the handoff confirms it is the unshipped p53 commit); the hand-skin `.mtl` path used
`\v` and `\%` escapes so materials never loaded (fixed); 165 em dashes swept. Received the
author's handoff (their build 39.4) at the end of the session: version renumbered to 40.0.0,
the 39.x fixes and the adapter hypothesis folded into ROADMAP, KNOWN_ISSUES, CODE_REVIEW,
ENGINE_NOTES and XR_HANDOFF. Verification: exports 9/9, lint clean, both legacy
configurations build, `split-source.py --check` reports only the intended changes. Branch
pushed.
