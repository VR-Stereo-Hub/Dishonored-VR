# VR-33 step 2 review: proceed with corrected capture and an explicit F stage

Reviewed 2026-09-07 against `bc18dab6`, implementation `c2cd3a0b`, and
PLAN-vr33-shader-capture.md. Review only; no game or build was run and no
implementation was changed.

**Decision: proceed with this approach after incorporating the corrections
below.** The next useful work is the bounded draw capture and offline shader
analysis. There is no need for another design-only review before implementing
those corrections. Placement follows when the recovered conversion is actually
specified and validated; a finite optical center alone is not that conversion.

## 1. Close the missing link between Stages B and C

Stage B recovers a camera position in palette coordinates. Stage C requires F,
the full palette-to-view transform. Position alone does not supply its linear
part, axis convention, scale or projection relationship.

Make an explicit B2 deliverable:

```
clip = Q * [q_palette, 1]
Q = P * F
controller_view = the grip point in this draw's view, units and pose generation
target_palette = inverse(F) * controller_view
```

Identify P and F separately from the actual shader path/constant semantics if
they are separately available. If only their product Q exists, obtain
independent projection information or validated camera/geometry constraints
before declaring a factorization. Algebra alone permits
`Q = (P * inverse(G)) * (G * F)` for many G. Finding the optical center does not
resolve that ambiguity or establish metres per palette unit.

The required B2 output is the actual formula for F, the registers and operations
that supply it, how controller_view is constructed, and how units, eye and pose
generation agree. Do not fill the missing linear part with the old measured
axis signs and an unvalidated scalar merely because the center looks plausible.

Retain the earlier distinction between the viewmodel's projection and the
projection through which the user sees the world. If they differ, show how
visual position and depth are made consistent. Inverting Q against an arbitrary
screen point would define a ray or a chosen projective-depth point, not by
itself the controller's metric 3D position.

## 2. Correct the capture contract

**Capture all relevant constants.** The known 48-matrix hand palette is c6
through c149 inclusive. The proposed c0..c95 includes only 30 of its 48
matrices. It cannot reproduce all anchors or inspect all dynamically addressed
palette entries.

For initial bounded captures, read the float register bank permitted by
`D3DCAPS9.MaxVertexShaderConst`, with checked allocation bounds and HRESULTs.
For vs_3_0 this is at least 256 float4 registers, only 4 KiB at 256. Include
integer/boolean constants if they can affect branches or addressing, or take
their small banks in the initial packet. Unsupported vertex texture dependence
or other unrecorded inputs must be identified during shader analysis. Microsoft
documents the banks and relative addressing in its
[vs_3_0 register reference](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-vs-registers-vs-3-0).

**Separate code capture from state capture.** Save bytecode once per content
hash. Shader pointers are useful telemetry but can be reused after destruction;
they are not durable identities. The same bytecode is used with different
constants, poses, eyes and passes. Take a bounded set of state packets at
baseline, separate head rotations/translations, controller-only motion and both
eyes, with explicit requests to recapture. Stage C then reads current state per
qualified draw; it must not reuse the first capture's Q/F indefinitely.

Each state packet references the bytecode hash and includes a unique capture
ID, device/reset epoch, original draw contract, viewport/depth range and target
dimensions/pass context. Also include the selected anchor IDs and their input
position, blend-index and weight bytes/decoded values, coefficients, and the
declared skinning formats. Otherwise the offline side has constants but no
reproducible inputs for its claimed palm position.

**Copy on the render thread, process on the worker.** Every device query and
GetFunction/GetDeclaration call belongs to the draw's owning thread at the
capture point. Copy data into an immutable bounded packet and release temporary
COM references before returning. The worker receives owned bytes, not live D3D
objects or pointers into mutable mesh/global storage. Hashing, disassembly and
file writing can run there. Report queue-full/incomplete captures explicitly
and do not mark a failed capture as successfully completed.

Use a shared read-only geometry qualifier for capture and MsDraw. Do not invoke
MsDraw just to discover whether the contract passes, because it actually draws
and changes state. Capture before the per-hand palette modification, while
retaining the active camera/stereo state. A diagnostic capture of a known mesh
with an unqualified old palette cache is still useful: mark that uncertainty
and read the device directly. It must not enable placement, but the old cache
should not prevent collecting the very evidence needed to repair it.

## 3. Keep the real camera state; disable competing hand writers

The c0 concern is correct: device state is preferable to a shadow of game
requests before the mod's modifications. But do not disable required HMD,
stereo or positional correction merely to make the two sources agree. That
would measure a different rendering configuration from the one the hands must
work in. Disable overlapping experimental hand/palette writers. Record active
camera, positional, eye and legacy-transform settings in each packet.

There is one additional source of effective constants: shader-immediate
definitions. The decoder must honor `def`, `defi` and `defb` semantics rather
than assuming a device-register dump is the entire shader environment.
[Microsoft's def documentation](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/def---vs)
states that float immediates override values supplied through the API.

Use a mature offline disassembler for the first pass, rather than writing a
Shader Model 3 token decoder. The project already loads d3dcompiler_47.dll for
other work. Microsoft provides
[D3DDisassemble](https://learn.microsoft.com/en-us/windows/win32/api/d3dcompiler/nf-d3dcompiler-d3ddisassemble)
and recommends it over the older D3DX disassembler. Check that it accepts the
captured shader version and handle failure explicitly. The deliverable is a
verified skinning-to-position path, not a general shader emulator. Account for
the declared position output, swizzles, relative addressing and selected control
flow; reject an unsupported path instead of approximating it.

## 4. Refine the optical-center claim and its tests

Keep `inverse(Q) * [0,0,1,0]` for the verified standard perspective case, with
the documented vector/storage convention. Correct the statement that every
nonstandard path produces w=0. Orthographic affine projection produces an
infinite center in this construction; other nonstandard maps can produce a
finite result. Nonzero w is not proof that the shader is a supported perspective
camera, and some paths cannot be represented by a single Q at all.

Use a linear solve for `Q * center_h = [0,0,1,0]` if convenient. Normalize the
homogeneous result before a relative w threshold, and report finite values,
conditioning and solve residual. Homogeneous Q is only defined up to scale;
an absolute w cutoff without normalization can reject an equivalent matrix.

Revise two falsification-table entries:

- A recovered center changing numerically in palette coordinates is not alone
  a failure. Those coordinates can themselves change with component/viewmodel
  state. Test the recovered relationship against the independently known eye
  and held-out projections. For an input-isolation test, also hold buttons,
  sticks, animation state and unrelated camera changes fixed as appropriate.
- Marker stability means it stays on the selected palm patch. It is allowed
  to move with legitimate palm animation when placement is off. When placement
  is on and the controller is fixed, test the palm anchor's position; finger
  motion and animated orientation remain allowed in this position-only stage.

One eye tag and one pose-generation number in a sidecar are insufficient unless
they identify the state actually consumed by that draw. Record the associated
head and controller transforms, validity, prediction times and the active eye
transform/projection. Match the rendered generation, not whichever global pose
was sampled most recently. Head look-ahead versus hand prediction and coherent
publication remain required work from the previous review.

## 5. Step 1 improved, but several claimed guarantees are still incomplete

These corrections should accompany the capture implementation, without another
headset-only verification round.

| Current source | Remaining issue and correction |
|---|---|
| `vs_const_hook.cpp:235` | The outer condition requires `startReg <= 6`, so an update beginning inside the palette, such as c9 x3, is still ignored. Copy the intersection of every upload with the validated palette register interval. |
| `vs_const_hook.cpp:247` | A wide upload covering c6 only populates an already-existing cache. It cannot bootstrap an empty cache. Track validity per covered register, including initial wide uploads. |
| `vs_const_hook.cpp:242-245` | A short c6 x4 upload leaves the previous claimed palette length intact. Length alone still does not establish a shader's skinning contract. Track actual register state and qualify its use at the draw. |
| `vs_const_hook.cpp:238-240` | An accepted long upload copies its entire length; MpBuild then treats each triplet as another bone. Limit modifications to the shader's verified palette interval, not every register following c6. |
| `mesh_split.cpp:1881` | Per-class drawing still accepts `g_mpCacheN >= 3`. Enforce the current draw's verified interval/validity there, and invalidate state on device reset. No cache reset is present among current g_mpCacheN assignments. |
| `mesh_split.cpp:1828-1839` | A failed stream/declaration query can leave zero/null values that bypass comparison. Require successful queries and valid returned state before certifying the contract. |
| `mesh_split.cpp:1787-1789` | `x == x` checks reject NaN but accept infinity. Use a real finite check for outputs, inputs and submitted transforms. |
| `mesh_split.cpp:1985-1991` | One counter is shared by both hand draws. In the normal left-then-right sequence, every 256th call is the right hand, so the left residual never samples. Sample both once per chosen frame, or use per-hand counters. Mark failed samples invalid and publish their generation/age. |

The residual now measures a CPU-rebuilt palette, which improves on assigning
zero. It still does not prove a successful device upload or draw, and both calls
use the same CPU skinning assumptions. Check submission HRESULTs, bind the
measurement to the exact buffer submitted, and retain the independent rendered
marker check. Do not label a failed/stale sample as a current zero residual.

The new deduplicated nearest-centroid patch is a better candidate, but remains
a heuristic. Its centroid is triangle-corner weighted and can be biased by
tessellation; proximity does not prove it excludes fingers. Keep it if the
marker confirms a stable palm patch, rather than turning patch selection into
another anatomy investigation.

I checked the counter arithmetic independently: 512 normal two-hand pairs
produce zero left samples and four right samples with the current shared
counter. This is a source-level sequence check, not a game measurement.

## 6. Scale wording and the revised execution order

Replace the claim that at most one of 100 and 108 can be right. They would
conflict if both described the same metric mapping, but the palette and game
world may have different units/scales related by F. Determine that relationship;
do not force equal configuration values or fit scale from the old calibration
offset. Test known controller displacement and stereo depth against the actual
camera/world scale at multiple depths.

Execute in this order:

1. Complete the finite/state/query/residual fixes, add the absolute-on and
   relative-off harness case, and implement bounded captures using device state.
2. Capture bytecode plus a small set of state/input packets across both eyes and
   isolated simulator poses. Analyze the active shader with a standard tool.
3. Deliver Q, the optical center where applicable, and the explicit B2 formula
   for F and controller_view. Validate on separate poses and rendered markers.
4. Remove the per-hand initial-position calibration and place through F, with
   zero grip offset. Evaluate F from current qualified draw state. Validate
   position, tracking loss, reset, both hands and every relevant hand pass.
5. Use a headset run for positional alignment, including straight head movement.
   Controller orientation/grip and weapon integration remain the following
   builds. Native global scans stay parked.

This approves the measurement approach for implementation. It does not certify
the still-unknown shader path, claim optical-center recovery supplies F, or
require another permission round before carrying out the corrected capture plan.
