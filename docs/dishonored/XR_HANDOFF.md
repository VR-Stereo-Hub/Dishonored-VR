# Handoff: the OpenXR / Quest presentation

## What it is

On Quest via Virtual Desktop (VDXR), the picture warps as the head turns and, on some
machines, the view locks a few seconds after a load. On SteamVR-native headsets the same
pipeline is fine. The original author could not reproduce it on their rig and shipped
several mutually exclusive switches for users to A/B (`[VR] StampLive`, `StampFix`,
`XrPoseDelay`, `XrLayer`, `[Screen] RigidScreen`, `WorldScreen`, `EyeCant`, `XrCylinder`).

## What was tried (build, verdict)

- 37.6 RigidScreen (per-eye screen placement keyed to the HMD model): Quest lenses are canted.
- 37.7 `Submit_TextureWithPose` with the render pose: only the true delta is reprojected.
- 37.9 OverlayScene (compositor overlay, reprojection-exempt): stable but cannot be motion
  smoothed; rejected 38.0.
- 38.1 EyeCant: measured identity on Quest/Steam Link; not the warp.
- 38.2 WorldScreen (room-anchored screen, plain submit): geometrically correct, experientially
  wrong (a cinema screen); auto killed 38.3.
- 38.3 XR-3: detached pace thread + head-locked quad layers in VIEW space.
- 38.7 cylinder layer: "weird wrapped around".
- 38.8 projection layer stamped with the render pose (Vive parity): never had a fair test.
- 38.9 XrPoseDelay: stamp the pose matching the content's age.
- 38.13 XrFrustumFill; 38.14 FpsCap (even cadence; measured stutter cause was fps 66-80 vs
  72/90 Hz).
- 38.84 StampFix (correct the stamp by the rendered pitch from the fork's `dxvk_vr_view`):
  inert on this repo's builds, the export is missing.
- 38.89 StampLive (stamp the pose located THIS submit): the current default, with the
  clearest mechanism statement: the game camera already carries the head rotation, so any
  stamp that disagrees with the current head makes the compositor warp the image back by the
  difference.

## Reproduction (simulator first)

`tools\xrsim-launch.ps1`, `boot.ps1 -Attach`, then `world-6dof.xrs`: `ClaimRatioH` must be
~1.0 at every yaw/pitch and `EyeSeparationM` constant. `headlook.xrs` with `pace step`:
compare the eye image at head yaw 0 and 35 against the projection-view pose stamped in the
capture JSON (`layers[].pose`). A stamp that lags the rendered content by N submits shows as
a constant offset between the image motion and the pose motion.

## Next experiment

1. In the simulator, log per submit: the pose the content was rendered from (what
   `ApplyHeadToViewRotation` wrote that tick), the pose stamped on the layer, and the pose
   located at the predicted display time. The spec wants the stamp == the rendered pose.
   Measure the lag between "written to the rotator" and "visible in the capture" in submits.
2. Pick ONE stamping policy from that number (`XrPoseDelay` = the measured lag, `StampLive`
   off), retire the others, and re-derive `dxvk_vr_view` in the fork only if the rotator
   write is proven to be ignored on some machines (the 38.84 theory).
3. Then bring the hands, the wrist HUD and the overlay into the projection-layer mode only,
   and drop the quad/cylinder modes.

## Do not retry

OverlayScene (38.0 verdict), WorldScreen as a default (38.3 verdict), EyeCant as the cause.
