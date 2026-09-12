# VR-57: controller aiming guide - implementation and review handoff

2026-09-11. Step 1 is implemented, built and installed for user testing on
`claude/vr-57-crosshair-on-hand-ray`, based on `eb8f27d4`. The dot and beam are
visual diagnostics. Projectiles, Blink, powers, camera, native reticle, hand
placement and the accepted VR-76 mirror policy retain their existing behavior.
No game or simulator was launched for this work. No headset verdict yet.

## Installed candidate

Build ID: `vr33-hands-working-100-geb8f27d4-dirty`.
DLL: `C:\Program Files (x86)\Steam\steamapps\common\Dishonored\Binaries\Win32\d3d9.dll`.
SHA256: `F1164C5F354F9501DE4AD612EA37A5EB5AE126A6C1ABFD86BBA7A807CF1C9DD4`.
The installed DLL hash was compared with the release build output and matched.

Only this section was appended to the existing ini:

```ini
[Crosshair]
Dot=1
Laser=1
Hand=left
DistanceM=8.0
SizeDeg=0.5
HideGame=0
```

The original ini text was preserved exactly before this addition. MotionAim
remains explicitly disabled. Installed ini SHA256:
`D7A9BFFFDED16387EBE58D0D98C38EA45E1372811807477D00CE49809B3D42F9`.
Tree/fresh-install Dot and Laser defaults are both 0. Save As Defaults persists
all implemented crosshair controls. HideGame is reserved, unsupported and logs
that fact if set to 1; it never hides the native reticle in this step.

Original DLL, ini and both logs are copied to `build/vr57-validation/before/`.
Original DLL SHA256:
`EBE630C1E3DF42E68FA7C162B1076F0B263F21C38342E94B96E4DB9064C5EC0A`.
Original ini SHA256:
`4C6B370F28A0FEC790B3B48AC5E39DA4309189430D0333031974831249EF9707`.
Source changes remain uncommitted for review. No merge, ticket closure,
reassignment or external message was performed.

## Review answers and corrections to CROSSHAIR_PLAN

1. **Ray ownership belongs in the game adapter.** The new
   `game/dishonored/aim_ray.h/.cpp` owns the controller selection, validated aim
   pose, ray snapshot and visual consumers. It has no game-space conversion yet.
   The runtime takes explicit XR LOCAL points through `core/vr/aim_visual.h`.
   It knows how to billboard them, but cannot choose a hand, pose or direction.
   This preserves the one-ray rule without adding game logic to OpenXR.
2. **Reentry reaches projection mode, but the actual base layer decides.** A
   requested projection is insufficient during menus, cinematic fallback,
   missing texture delivery and pair-open presents. The compositor consumer
   checks the actual base layer type after the existing held-layer fallback.
   Thus a held projection keeps the guide, while a mono/cinema quad does not.
3. **The existing laser could not be wired unchanged.** It independently samples
   the aim pose and applies trims at render time. Using it beside a separately
   published dot would produce two derivations and potentially two samples.
   Instead the game adapter computes one endpoint and four beam points from the
   identical ray. Both use the existing soft-dot texture and shared billboard
   geometry. The endpoint is first in the point list and wins a tight budget.
4. **Blink is not a general trace API.** The existing trace hook operates inside
   Blink's live engine call/frame and charging state. This review did not find a
   verified standalone invocation contract. Do not call it outside that context
   by guessing. Fixed-distance rendering is a useful labelled diagnostic behind
   this lever, not an aim-on-surfaces implementation or a shipping projectile fix.
5. **Do not delete legacy pitch/flip keys in this step.** They still feed existing
   HandRelFull and Blink paths. The old weapon path is actually
   `MaimHandRel -> HandRelFull -> MaimDirFromView`; `HandRelSnap` is Blink's
   snapshot variant. The plan incorrectly named HandRelSnap as the weapon path.
   Removing those keys before migrating all consumers would change more than
   this visual build. Later migration needs explicit compatibility/release notes.
6. **The grip/40-degree diagnosis is plausible, not proven.** The bridge does
   populate legacy controller matrices from grip poses, and the old aim applies
   a fixed downward tilt plus head-relative decomposition. That can disagree
   substantially with the runtime aim pose. But the recorded up/backward shot
   direction does not isolate pose choice from basis signs, timing or repeated
   projectile steering. A near-zero aim/grip angle also does not prove the
   runtime aliased the actions, nor justify a 40-degree correction by itself.

Additional corrections:

- The hands publish placement/correction transforms, not a calibrated barrel
  axis for each weapon. A matrix column must not be labelled muzzle direction
  without measuring its local weapon axis and reference frame. This build logs
  `barrelAngle=UNMEASURED` explicitly. Static barrel alignment remains a visual
  question; moving comparisons also include the accepted hand/render pose lag.
- The old visual block runs before the hold fallback, so simply calling
  set_aim_dot/set_laser would omit the guide on HoldUntagged frames. The new
  consumer runs after that fallback, preserving it on held projections without
  altering the held world layer or stereo pairing.
- The existing soft-dot image publisher returned success even after a failed
  swapchain wait. It now requires both the wait and release to succeed. All
  quads share one texture publication per frame, including existing consumers.
- These are compositor layers, visible in the headset only. They are not drawn
  into the game's D3D9 desktop mirror and are not scene-depth-occluded. At 8 m
  the endpoint can be behind or in front of a real wall; no trace has run.

## Implementation

`aim_ray.h` validates the pose, hand, generation, timestamp, finite position and
quaternion norm. It normalizes the quaternion, then uses the existing
`xrmath::quat_rotate` to rotate XR forward `(0,0,-1)`. There is no pitch trim,
axis flip, head-relative frame, engine address or world-space reconstruction.
`visual()` derives the endpoint and beam points directly from that immutable ray.
The beam has four geometrically spaced markers from 0.25 m to 80% of the guide
range, with half the endpoint's angular size, so they do not overlap the endpoint.

`input_hand_aim_sample` is a present-thread accessor returning aim and grip from
the completed input sync, with its actual locate generation and timestamp.
Invalidation clears freshness. The producer refuses unavailable or older-than-
250-ms data, and the consumer independently checks both original sample age and
publication age. Republishing a stale pose cannot keep the guide alive.
The cached `ray()` API is present-thread-only for this step; later script-lane
consumers will need an explicit synchronized snapshot boundary.

`DvrGameTick` publishes once per present after runtime pose location. Disabled,
non-gameplay, non-projection and no-session states publish an invalid/empty guide.
Changing a setting immediately invalidates the prior publication, preventing an
old hand or distance remaining visible until a later update.

`openxr_runtime.cpp` shares the existing point-billboard function and soft-dot
texture. The new additive consumer executes just before xrEndFrame, after the
base layer and hold fallback exist. It respects both the runtime's reported
maxLayerCount and the existing 13-entry array, adds at most five layers, and
never removes a world/HUD layer to make room. It does not enter the legacy
laser pose/trim path. Successful submissions are counted only after xrEndFrame
succeeds; build attempts alone cannot produce a success counter.

F10 has an **Aim** tab with dot, beam, hand, distance and size controls, plus
ray/renderer status. Commands:

```text
crosshair status
crosshair dot on|off
crosshair laser on|off
crosshair hand left|right
crosshair distance 0.5..50
crosshair size 0.05..2
```

The command seam and F10 use the same setter. Invalid hands, nonfinite values and
out-of-range values log a refusal and keep the existing configuration. Status.json
contains a crosshair object with settings, generation, ray/renderer states and
successful frame counts. Configuration logs name the source ini, F10 or command.

## Reading the log

While armed, `crosshair:` emits a bounded one-second heartbeat containing the
selected hand, original input generation/age, aim and grip forward vectors,
aimGripDeg, fixed range, producer gate, and publication/submission/dot/beam deltas.
Angle -1 means unavailable, never agreement. `barrelAngle=UNMEASURED` is expected
until a verified per-weapon axis is added.

A companion line enumerates renderer outcomes that occurred in that window:
invalid ray, stale sample/publish, no XR frame, pair awaiting sibling, no actual
projection layer, invalid views, missing texture, insufficient budget, point
near the head/invalid, image upload failure, xrEndFrame failure, or submitted.
Pair-await is an expected present with no xrEndFrame, so equal publish and submit
counts are not required. Near-head and budget counters can coexist with a
partial successful frame; they count affected opportunities, not dropped points.
No visuals while disabled or in menus is expected and stated in the producer log.
Successful submission is evidence that layers reached xrEndFrame, not proof the
user saw them or that they line up with a weapon.

## Validation completed

- Release x86 build passed. Candidate and installed hashes match above.
- `tools/aim-ray-host.ps1`: **70,234 assertions passed**. Includes identity,
  yaw/pitch/roll, inverted and non-unit quaternion handling, invalid poses,
  stale samples, fixed-distance geometry, endpoint/beam collinearity and config
  bounds. Independent spherical directions cover 2,925 orientation combinations.
- The same harness extracts and compiles the production `quat_facing`,
  `publish_laser_image`, `build_aim_point`, `build_aim_visual` and outcome code
  against controlled OpenXR calls. Tests cover both-eye LOCAL quads, head movement,
  held projections, mono rejection, shared texture publication, budget priority,
  missing resources, failed acquire/wait/release, and successful-vs-failed counts.
  This is a host test, not a GPU or headset rendering verdict.
- Existing desktop policy regression: **79,339 assertions passed**, plus
  **72 actual desktop-copy module assertions**. Its expected negative-control
  FAIL message is followed by verification that the legacy policy was rejected.
- `frame_test.exe`: exit 0 for existing hand/weapon tests.
- Proxy export check: nine undecorated names, passed.
- Repository lint, new-file checks and diff whitespace checks passed.
- Default ini golden regenerated from the complete production literal; only the
  new Crosshair section was added. MotionAim remains default off.
- No game launch, simulator launch or headset test. Visual appearance, barrel
  alignment, tracking behavior and comfort remain the user's tests.

## Next test, one question per launch

A: Does the dot/beam appear in the headset and follow the LEFT controller while
sweeping it? F10 Aim can turn either off. Check startup resolves Dot=1, Laser=1,
Hand=left and MotionAim=0. A missing guide should have a producer gate or renderer
outcome in the log. It is normal that the desktop game window has no new dot.

B: With the controller and crossbow held still, does the guide line up with the
barrel? Turn the head without moving the controller. The guide is room-anchored;
it should not follow head aim. A mismatch is evidence to measure the actual
barrel axis and matched pose generation, not a reason to retune legacy flips.
Shots still follow the original game aim in both tests.

After this visual step is accepted: a separate trace/hit-point change, then a
single-ray projectile migration, then native-reticle suppression and the later
Blink/power consumers. Those changes were intentionally not included here.
