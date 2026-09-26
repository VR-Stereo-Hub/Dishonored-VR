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
