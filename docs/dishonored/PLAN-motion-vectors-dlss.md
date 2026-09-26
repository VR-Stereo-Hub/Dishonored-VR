# Motion vectors, then DLSS (plan, 2026-09-26)

Branch `claude/motion-vectors` off `staging` (`fb73099aa`). Research and plan; nothing built yet.
Why: the clarity work (`PERFORMANCE.md`, Anti-aliasing and clarity) showed that a temporal pass
reprojected by head rotation alone smears while walking, and only stays sharp by giving the
history up whenever the camera moves. Motion vectors fix that, and DLSS cannot run without them.

## What a motion vector is here

For each pixel of an eye image: where that surface was in the previous image of the same eye.
Two parts:

1. **Camera motion.** From the pixel's depth, the current camera and the previous camera, the
   world point is reconstructed and projected into the previous view. Exact for everything that
   is still in the world (walls, floors, props at rest) whatever the head and body do: turning,
   walking, crouching, Blink. This is most of the image and all of the walking smear.
2. **Object motion.** Anything moving on its own (NPCs, the hands and weapons, doors, particles)
   needs its previous transform. The engine does not keep it; producing it means drawing those
   objects again with last frame's matrices. Not planned; the temporal clip handles them as it
   does now.

## What the mod already has

- Both cameras exactly: the pose record per eye image (`core/vr/pose_record`) carries the written
  rotator and position; the projection is the layer's claim.
- A per-eye history and a reprojecting pass (`core/gfx/clarity`), host-tested on the GPU.
- A frame dumper (`dump` on the seam) and the device census (every render target the game makes).

## What is missing: the depth

The game has no depth-texture path on D3D9 that the census has seen: its depth-stencils are
plain D32 surfaces (not sampleable), and the exe carries no INTZ or RAWZ format strings (one DF24
reference, most likely the shadow path). UE3 on D3D9 of this era keeps scene depth in the ALPHA
of its floating-point scene colour target for its own post effects (the `SceneDepth` shader
parameters in the exe). So the plan starts by proving where depth lives:

1. **Probe (next build).** Find the scene colour target by its identity (float format, eye size,
   bound for the opaque pass) and read its alpha at a few pixels whose distance is known (the
   hand at arm's length, a wall at a measured range from the c5 camera). A depth that does not
   move when the probe points move, or does not match the known range, fails the hypothesis.
   Log the verdict; no image changes.
2. **Copy.** Once identified, share that target (or a depth-only copy of it) to D3D11 per eye,
   the same way the colour capture does, before post-processing overwrites or tonemaps it.
3. **Reconstruct.** A D3D11 pass writes a motion-vector texture per eye from depth + the two
   cameras. Host-tested like the clarity passes: a synthetic scene with known depth, a known
   camera move, the vectors checked to a pixel.
4. **Use it.** The temporal pass switches from rotation-only to depth reprojection, keeps its
   history while walking, and the motion weighting is retired. A headset A/B decides.

## DLSS after that, and its blockers

- **32-bit.** The game and the mod are 32-bit; NVIDIA ships DLSS (the Streamline and NGX
  libraries) for 64-bit only. DLSS would run in a separate 64-bit helper process that shares the
  colour, depth and motion textures with the mod (D3D11 shared handles work across processes)
  and hands back the upscaled image. Latency: one extra cross-process fence per eye.
- **Jitter.** DLSS needs the render to move by a sub-pixel offset each frame, and to know it.
  The mod writes the camera, so a sub-pixel rotation jitter is possible; its effect on the
  projection must be measured, not assumed (a rotation is not exactly a screen shift at the edge).
- **Inputs it wants:** colour before the HUD and post effects, depth, motion vectors, exposure,
  and a reset flag on cuts. The HUD is already separate (VR-117); post effects are baked into the
  captured colour, which DLSS tolerates less well.
- **The win:** render below 100% and reconstruct to the headset size, which is the only way to
  get 200-300% clarity at today's frame cost. Licence: the DLSS SDK's terms must be read before
  anything is shipped.

## Order

Probe (1) is a single, small, measurable build. Nothing past it is worth building until it says
where the depth is.

## Result of step 1 (2026-09-26, simulator, build 15:34)

**Scene depth is in the ALPHA of the eye-size A16B16G16R16F render target (D3DFMT 113), read at
Present.** The game creates two such targets at 2750x2850 (probe candidates #6 and #7; identical
reads, most likely the ping-pong pair of scene colour). The other float targets are not it:
the half-size (1375x1425) RGBA16F targets read all zero at Present, the eye-size R16F and G16R16F
targets read zero with a single bright texel.

Evidence (5x5 grid, 10 %..90 % of each axis, alpha):
- Main-menu scene: top row about 4400 (sky), middle 2..14, bottom row about 0.8.
- In a room, head level: the top row CONSTANT across all five columns (0.2397, a ceiling: a
  plane parallel to the view's horizontal axis has one depth per row), the outer columns constant
  down the image (0.27 and 0.31, the side walls), the bottom row near-constant (0.67, the floor),
  1.1..3.4 inside. That structure is what linear view depth of a box room looks like and nothing
  else in a colour target does.
- The falsifiable prediction: head pitched 60 deg down with nothing else changed, the centre falls
  (1.17 -> 0.81, the floor along the line of sight) and the ceiling row stops being constant.
  Both happened.

Units: about 100 uu per unit (eye height over the floor at 60 deg down, ~80-90 uu, read 0.81).
Calibrate in step 2 against a measured distance before anything reprojects with it.

Next: step 2 shares this target's depth to D3D11 per eye image, at Present, beside the colour.

## Result of step 2 (2026-09-26, simulator, build 16:04)

The scene target (the first-created eye-size RGBA16F) is copied every present by StretchRect
into a D3D9 render-target texture opened on the mod's D3D11 device (`interop::create`, the same
route as the colour capture), fenced by an event query. `[Diagnostics] DepthShare`, off by
default; `depthprobe share on|off`.

Verification: every 5 s the SAME present is read on D3D9 from the game's own target and on
D3D11 from the shared copy, 25 texels each. 13 checks of 13 were bit-identical (worst difference
0) across the main-menu scene, a load and gameplay in a room; about 5200 copies, no refusal.
Simulator paced at 90 Hz, GPU per tick 7.9-8.0 ms with the copy on (a headset A/B still owes the
copy's cost; it is one full-size RGBA16F blit per present, about 62 MB at 2750x2850).

Next (step 3): pair each shared depth with its eye image and pose record (the colour capture's
tag and record, delivered on the same present), and write the motion-vector pass: world point
from depth and the current camera, projected into the previous camera of the same eye.
Host-test it against a synthetic scene with known depth and a known camera move, then feed the
temporal pass. The depth scale (~100 uu per unit) must be calibrated first: a surface at a
measured distance from the c5 camera.

## Step 3 in progress (2026-09-26, simulator) - RESUME HERE

Built: the depth ring keyed by the colour grab's serial (`depth_srv_for`), the calibration shader
(`core/gfx/motion_gpu`, host test `tools/motion-gpu-host.ps1`: 6/6, a sharp minimum at the true
scale, rotation-only 10x worse, flat with no translation) and the live instrument in
`core/gfx/clarity.cpp` (`calib_frame`, `[Diagnostics] MotionCalib=1` with `DepthShare=1`).

Measured on the simulator (head stepped 0.3 m sideways, about 150 moving frame pairs a run):
- With the translation as computed, every candidate scale (25..7000 uu per depth unit) scores
  WORSE than rotation-only, falling monotonically toward it: the predicted parallax hurts.
- SIGN TEST: the same pairs with the translation reversed give an interior minimum at 200-400
  uu per unit (0.0145 against rotation-only 0.0147). So a convention is flipped: either the
  translation's direction, or the image is mirrored left-right against the camera's right axis
  (a sideways move cannot tell the two apart).
- NEXT (built, installed, NOT yet run): the MIRROR TEST - pure head turns scored rotation-only
  with the normal convention and with the right axis mirrored; the lower error names the real
  convention. Run: sim launch, `boot.ps1 -Attach`, Space x3 to gameplay, then
  `xrsim-cmd "head rot 3 0 0"`, `"head rot -3 0 0"` repeated; read `MIRROR TEST` lines.
  If mirrored wins, the clarity temporal pass has the same fault (its yaw reprojection) - that
  may be the smear reported while moving before the motion weighting.
- Then: fix the convention in `calib_frame` (and clarity's temporal), rerun; the curve must have
  an interior minimum below rotation-only. Only then compute motion vectors for TAA.
