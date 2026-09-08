# VR-33 Build A review: measure the render transform, not an inferred foot origin

Reviewed 2026-09-07 against `f6f448bb` (implementation `04a192f0`), the supplied
Build A result, and current source. The tester clarified that this run tested
head turning; independent straight head translation was not tested. No game
was launched or implementation changed during this review.

**Decision:** keep absolute placement and the per-hand palette backend. Choose
option 2, implemented by recovering the actual draw's coordinate conversion.
Do not counter-rotate the whole calibrated offset, hard-code a pawn-foot
origin, or go back to relative travel. Complete the omitted shader/palette and
anchor checks before interpreting another calibration number. This is a
bounded correction to Build A, not a return to the native skeleton investigation.

## 1. The new diagnosis is conditional, not established

The arbitrary per-hand calibration is unsuitable as the final controller
target. However, the derivation `world_palm = c + R * o` assumes that palette
coordinates map back to the world through the head position. That is exactly
the origin relationship the brief now says is unknown.

Let the actual affine conversion from palette output to a common world frame
be:

```
p_world = O(t) + L(t) * q_palette
```

Here O is the palette origin in that world frame, and L includes orientation,
axis convention and unit conversion. Even granting that the current controller
displacement conversion is correct, the implemented target gives:

```
world_palm = O(t) + (c - h) + L(t) * o
error      = O(t) - h + L(t) * o
```

Equivalently, define the actual head position in palette coordinates as
`H_palette = inverse(L) * (h - O)`. Then:

```
error = L(t) * (o - H_palette(t))
```

It is the offset left AFTER accounting for the true origin that can act as a
rotating grip error. A large constant palette-coordinate camera position can
be entirely legitimate. For example, if `o == H_palette` and both are a large
constant, the error is zero. Counter-rotating that legitimate origin term
would create an error.

The current `o = q(t0) - controller_relative(t0)` also contains the difference
between the game's initial hand position and wherever the real controller was
held. It is not a measurement that separates origin, grip, scale or skinning
error. The left and right calibration vectors differ by about 51.4 uu. A
single shared palette frame does not have a different coordinate origin for
each hand; separate hand residuals are being absorbed into those values.

The result supports controller influence through the new backend. It does not
yet establish that the calculated anchor is the rendered palm or that its
absolute target is the physical controller position.

## 2. What the magnitude does and does not explain

The supplied vectors have lengths 139.45 and 131.37 uu. Dividing by the
configured 100 uu/m gives 1.39 and 1.31 m, but this is conditional on the scale
and skinning calculation being correct. It is not independent evidence of
eye height or a pawn-root origin. At 210 uu/m the same numbers would be about
0.66 and 0.63 m; that is a sensitivity example, not a recommendation to set 210.

Rotation displacement depends on distance PERPENDICULAR to the rotation axis,
not always on the vector's full length. For level yaw about palette axis 1:

| Hand | Full offset length | Yaw radius | Change over 90-degree yaw |
|---|---:|---:|---:|
| Left | 139.45 uu | 21.39 uu | 30.25 uu |
| Right | 131.37 uu | 29.56 uu | 41.80 uu |

These are mathematical predictions for the isolated offset term, not measured
game errors. The large vertical component can produce large pitch/roll effects,
but a pure level yaw does not rotate that vertical component. Thus the number
does not by itself explain all reported axes of motion.

At fixed orientation, `c + R * o` predicts no dependence on head translation.
The tester has not tested that case yet. Add it to the next acceptance run
instead of treating it as a confirmed symptom or using it to reject a model.
Ordinary head turns also include neck-pivot translation, so use the simulator
to separate those inputs first.

## 3. Build A did not implement several essential parts of the prior plan

These are source findings, not speculative alternative causes.

**The anchor is a whole-hand sample, not a selected palm patch.**
`mesh_split.cpp:1506-1521` chooses triangle corners by an index modulo rule
across the hand class. Triangle order does not guarantee spatial distribution,
and it does not exclude fingers. Vertices can be repeated. Persistent vertex
identities are useful, but their average still moves when sampled fingers
animate. Pinning that average can move the palm in response to finger motion.
Choose and verify a fixed palm patch, as the earlier plan specified.

**The CPU calculation silently normalizes weights without checking the shader.**
`MpAnchorPos`, at `mesh_split.cpp:1698-1720`, divides each skinned point by its
weight sum. The GPU matrix writer does not add an equivalent operation. Unless
the active shader normalizes in the same way, or the effective sums are one,
the CPU point is not the point the GPU renders. Match the shader's actual
weight and index arithmetic; do not repair uncertain data on only one side.
An important subcase: if the shader uses unnormalized weights, a palette
translation T moves a vertex by `weight_sum * T`, so `target - q` alone does
not guarantee the claimed displacement.

**An invalid influence can change which anchor is measured.** The function
skips an entire vertex when one of its indices is outside the cached block and
then averages the remaining vertices. It returns success if any vertex worked,
despite the comment promising refusal. A shorter or wrong palette can silently
change the anchor. Require every chosen anchor vertex and influence to be valid;
reject the whole anchor otherwise, including non-finite data.

**The residual is assigned zero.** `mesh_split.cpp:1899` writes
`g_mpResid[hIdx][i] = 0.0f`. This is not verification. Re-skin the anchor using
the actual transformed palette to measure the implemented result, and also
check projection against an independently generated target. Even a correctly
computed zero algebraic residual cannot establish the target's physical frame.

**The palette/draw contract remains necessary evidence.** The old exact-c6-only
cache and `count >= 3` acceptance are unchanged. The draw still lacks the
startIndex comparison and does not compare the stream stride. These can affect
the anchor's absolute coordinates, not just dynamic smoothness. Therefore the
brief cannot exclude them as contributors to the 139-uu number. Qualify the
shader and range, handle overlapping constant updates and reset invalidation,
and preserve exact pre-draw state before measuring the origin.

## 4. Next implementation: recover the conversion used by the draw

### A. Make one qualified draw self-describing

Capture the active vertex shader bytecode once per observed shader identity,
and snapshot the actual constants at a qualified hand draw before applying the
mod's hand transform. Include declaration, VB/IB, offsets, stride, index range,
eye, viewport, frame/pose generation and active camera/VP modifications.
Keep game-derived bytecode/captures local and ignored by git.

Direct3D provides the needed read interfaces:
[GetFunction](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3dvertexshader9-getfunction)
retrieves shader bytecode, and
[GetVertexShaderConstantF](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-getvertexshaderconstantf)
reads constant registers. Check return values; a failed read is not zero state.
Keep COM references local to the draw hook, and move disassembly/file processing
out of the recurring render work.

Read the actual shader instructions from skinning through clip-position output.
Verify matrix storage, blend-index addressing, weight handling and any c4/c5
or other post-skinning terms. Do not assume that c0 alone is the complete map
because it was labeled view-projection in an older experiment.

The existing code can alter c0 with head/positional corrections
(`vs_const_hook.cpp:307-341`, `vs_const.cpp:79-100`). A map reconstructed from
the game's requested constants before those modifications may not describe the
draw. This is another reason to use the actual qualified draw state.

### B. Derive the render camera in palette coordinates

If the verified post-skinning path is a standard perspective projective map,
write it using column-vector notation as:

```
clip = Q(t) * [q_palette, 1]
```

Q must include ALL verified post-skinning affine terms and the projection.
For the standard perspective form, the optical center in palette coordinates
can be recovered without guessing a pawn height:

```
center_homogeneous = inverse(Q) * [0, 0, 1, 0]
center_palette = center_homogeneous.xyz / center_homogeneous.w
```

This works because the camera center projects to zero clip x, y and w, with
the remaining homogeneous component fixing an arbitrary scale. Transpose the
formula for row-vector storage. Check invertibility, conditioning and a finite
nonzero homogeneous w. An orthographic or nonstandard path can fail these
conditions; do not force a plausible center out of it. This identifies a render
optical center, not an anatomical camera bone or a proof that the origin is feet.

The formula was checked during this review on 12 synthetic perspective maps
with translation, rotation, reflection/scale and off-axis projection. Recovered
centers agreed with the independently known affine camera centers to 2.9e-14.
The orthographic identity case produces homogeneous w=0, as expected. This
checks the proposed mathematics, not the game's still-uninspected shader path.

For an eye draw, this is that eye's center. Use the corresponding eye-relative
controller vector; do not add stereo eye displacement twice or substitute a
head-center pose without accounting for the difference.

Where the actual projection is independently known, also recover the affine
palette-to-view conversion from Q and that projection, checking the affine
bottom row and expected scale/basis. Factorization using an assumed projection
would simply reintroduce the unmeasured convention. The existing axis tests are
a cross-check on the recovered linear conversion, not a replacement for it.

### C. Place the anchor using that conversion

Let F map palette coordinates into the actual draw's view coordinates, and let
`controller_view` be the grip point expressed in the same view, units and pose
generation. The target is:

```
target_palette = inverse(F) * controller_view
T = target_palette - current_skinned_palm_anchor
```

Equivalently, use the recovered camera center plus the correctly converted
camera-to-controller displacement. Keep the genuine grip offset zero for this
position-only build. Remove `q(t0) - controller(t0)` calibration; absolute
alignment may legitimately jump away from the game's resting pose on activation.
The camera/origin conversion belongs to the draw, shared by both hands when
they share that draw space. Later grip calibrations belong to each controller.

Verify whether the viewmodel projection agrees with the projection through
which the user sees the world. If it differs, F alone does not establish visual
coincidence. Recover the actual projection relationship and check position,
depth and stereo explicitly. Do not tune unrelated per-axis gains to fit one
screen location.

### D. Validate what can fail, then use one headset run

Before headset testing:

1. Render a marker at the computed source point with the drive off and confirm
   it lies on the selected palm patch across several animations and poses.
2. Re-skin the patched palette to measure the corrected anchor. Compare its
   rendered location with a controller reference produced through an independent
   validated XR-to-view path, not the same assumed palette-origin formula.
3. Use simulator poses with head yaw, pitch, roll and translation isolated;
   controller movement isolated; animation changes with a stationary controller;
   and both hands sharing the same frame conversion. Check each affected pass
   and eye, plus reset and tracking-loss fallback.
4. Check scale using known controller separations/movements at two depths.
   Camera/positional tracking uses g_posScaleUU, while this target uses
   g_skcWorldScale times gain (`config.cpp:664,972`; `present_tick.cpp:297`).
   Record all active values. Neither the default 100 nor an earlier estimate
   settles effective viewmodel scale. A mismatch can also leave head-translation
   error, even with the right origin.
5. Include an absolute-on/relative-off test in the existing harness so the
   feature-flag gate regression is caught before another headset run. Run the
   reciprocal mode and tracking-invalid case too; fallback must be explicit.

Use held-out poses after deriving the conversion. A fit and its own inverse on
the same calibration point are not independent evidence. These checks directly
exercise the existing placement build, not an open-ended global object scan.

The headset run then checks natural alignment and motion. Add the previously
untested straight head movement. Orientation/grip Build B follows once absolute
position passes; weapons remain the next independent integration consumer of
the same canonical controller target.

## 5. Answers to the five questions

1. **The derivation is conditional on a head-centered origin.** The general
   error includes the unknown origin term. A large rotating residual is
   plausible, but the full 1.4 m is not established as that residual, and yaw
   uses a much shorter perpendicular radius.
2. **Do not act on the pawn-root conjecture.** Recover the actual draw mapping
   and optical center. This can distinguish camera-centered and displaced
   palette frames without interpreting one calibrated height as anatomy.
3. **Choose option 2.** Counter-rotating the whole o may rotate an origin term
   that was correct. Zeroing it blindly may remove a required origin term.
   Option 3 is not assumption-free: a head/neck surface is not the eye, and a
   camera bone is not automatically the final stereo camera. Option 4 cannot
   supply missing absolute alignment from changes alone.
4. **100 uu/m remains unvalidated for this conversion.** Establish consistent
   camera/controller units, then verify projection and known distances at more
   than one depth. Do not fit scale from the 139-uu calibration offset.
5. **Land palette/geometry/anchor correctness before trusting the origin
   measurement, as part of the next build.** Match head/hand prediction times
   and draw generation in that build too. These are required measurement
   inputs, not polish to defer until after choosing a diagnosis.

The decompiled `SkeletalMeshComponent` still exposes named camera/bone/socket
queries (`GetBoneLocation`, `GetBoneMatrix`, `GetSocketWorldLocationAndRotation`).
They are useful corroboration, but return native results whose spaces must be
validated; they do not identify the GPU palette layout or remove later camera
adjustments. No additional RefSkeleton walk or SkelControl cadence sweep is
needed for the render-based correction above.
