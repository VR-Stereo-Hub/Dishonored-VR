# VR-57: native crossbow launch handoff (2026-09-12)

## Result and evidence boundary

Implemented a native pre-spawn crossbow aiming hook on
`claude/vr-57-crosshair-on-hand-ray`. It aims from the game's selected spawn
position toward the existing controller dot's fixed-distance endpoint. The game
uses the replaced direction for spawn rotation and initial velocity. This is an
installed candidate, not a verified headset result. No game or simulator was
launched, and no repeat diagnostic run is required to finish this implementation.

## What changed

- `fire_aim.cpp` validates the native firing context and current player possession,
  then replaces one stack-local direction before spawning. No projectile pointer
  survives the callback. Camera, controller and pawn rotations stay untouched.
- `fire_aim_math.h` converts the existing AIM-pose ray and its configured endpoint
  into world space, then computes `normalize(endpoint - nativeSpawn)`. This
  accounts for muzzle displacement instead of copying a parallel direction.
- `aim_ray.cpp` publishes the ray, head pose and endpoint distance together under
  a mutex. Firing keeps ray sampling active even when the visual guide is hidden.
  Only the Present lane calls the XR pose API.
- `fire_aim_stub.h` preserves integer registers, flags, x87, MXCSR and XMM state.
  Production and the offline ABI test compile the identical bridge.
- `[Aim] FireFromHand` defaults off in new configurations. It is enabled in this
  installation. F10 Aim offers **Aim crossbow from controller**; command seam
  `fireaim on|off` switches it live. `DriveFromHand` and legacy MotionAim stay off.
- Claude's read-only FireWatch and ShotProbe remain available, but are disabled
  in the installed configuration along with SeamProbe. The new per-shot log names
  the launch input, endpoint, sample age and refusals; it does not claim an impact.

## Verified native contract

See ENGINE_NOTES, "VR-57 native crossbow fire seam", for the derivation. The
installed executable SHA256 is
`66443f3d68a6c658b0ed943260c3eb2f4cc9e3400d5809a94d6ec41eb725e17e`.
The FireCrossbow vtable leads to `0x00C38230`; the common pre-spawn join is
`0x00C38BBB`. Its six displaced bytes and the initializer call at `0x00C38DB6`
are checked before the hook installs. Address constants live in `patterns.h`.

Offline analysis follows the same direction local through spawn rotation and the
Arrow initializer into its velocity writes. It also establishes that the native
aim cache CAN be consumed during firing: `0x00C14460` checks/refills its tick tag.
Earlier failed cache writes do not prove that cache is HUD-only. This fix bypasses
that timing dependency without experimenting with the tick tag.

## Validation

- x86 RelWithDebInfo build passed.
- `tools/fire-aim-host.ps1`: 2,851 geometry, refusal and actual bridge checks.
  Covers left/right and vertical signs, head turns, scale, muzzle convergence,
  stale/invalid data, register preservation, replayed stack arguments and both
  writing/refusing callbacks.
- `tools/aim-ray-host.ps1`: 70,234 ray and production compositor assertions.
  Its extraction harness needed a no-op logging macro for an existing renderer
  diagnostic; production rendering was not changed.
- Native hook bytes checked against the installed PE. No game-derived source or
  disassembly is committed; only findings and the required hook signatures.
- Repository lint and whitespace checks passed. Installed `d3d9.dll` matches the
  built x86 proxy byte for byte, SHA256
  `7267c5e6b0b9be28219fb6801803054eef16946933e6525459a280805c809f47`.
  Previous DLL and INI were copied to the ignored build backup directory before
  installation; nothing was restored. Only the proxy and the named INI switches
  were changed in the game installation.

## Limits and next work

The hook is gated by the player's FireCrossbow context, not by an ammunition
subtype. The ordinary Arrow initializer is the path traced offline; other ammo
behavior has not been verified. Native obstruction/target selection occurs before
the hook, and assist/homing may act afterward. Speed and gravity are unchanged.
The fixed-distance dot is a launch target, not ballistic impact prediction.
Weapon model alignment, controller-based traces and assist changes remain separate.

Tracking older than 100 ms, near-vertical head basis, invalid geometry, unavailable
gameplay state or unverified player possession retain the native direction and
log a refusal. Runtime acceptance and visual accuracy still need ordinary user
play, but this build performs the write rather than requesting another probe run.
