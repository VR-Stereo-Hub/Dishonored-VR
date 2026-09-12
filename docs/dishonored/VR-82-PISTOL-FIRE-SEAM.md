# The pistol's fire seam (VR-82)

2026-09-12. The derivation, the plan, and the one place the pistol does NOT behave like
the crossbow. Nothing here has been run yet: this is an offline trace of the installed PE,
written before the code so that the reasoning survives if the session does not.

The pistol's GUIDE is already correct - it shares the latched bolt axis from VR-57. Only
the SHOT is unhooked, because the native fire hook installs at the crossbow's pre-spawn
join and announces `player crossbow firing context only`.

---

## 1. How it was found

The same route ENGINE_NOTES records for the crossbow, re-walked for the pistol. Every step
reproduced the crossbow's published numbers before being trusted for the pistol, which is
the only reason to believe the pistol's numbers at all.

| Step | Crossbow (published) | Crossbow (re-derived) | Pistol |
|---|---|---|---|
| class metadata | `0x01361928` | `0x01361928` OK | `0x01361AE0` |
| constructor (metadata +0x14) | `0x00C29DB0` | `0x00C29DB0` OK | `0x00C29E00` |
| context vtable | `0x01172C80` | `0x01172C80` OK | `0x01172E60` |
| firing routine (vtable +0x1B0) | `0x00C38230` | `0x00C38230` OK | `0x00C2A3E0` |

The class names live as UTF-16 in `.rdata`; the metadata struct is the dword that points at
one. The two constructors are 0x50 apart and byte-for-byte the same shape.

The exe this was read from is SHA256
`66443f3d68a6c658b0ed943260c3eb2f4cc9e3400d5809a94d6ec41eb725e17e`, the same image the
crossbow seam was derived against.

**Two independent confirmations that `0x00C2A3E0` is the firing routine**, before any
byte was planned: it calls `0x00BFF440`, the source-pawn resolver the crossbow uses, and
`0x00C14640`, the native aim-cache accessor the crossbow uses - in the same order.

## 2. The shape of the routine

`0x00C2A3E0` .. `0x00C2AA77`, 468 instructions, `ret 4`. It runs the same three helpers as
the crossbow in the same order, which is what makes the seams comparable at all:

| | crossbow | pistol |
|---|---|---|
| source pawn | `0x00C38276` -> local `ebp-0x54` | `0x00C2A417` -> local `ebp-0x1C` |
| aim cache | `0x00C3832A` | `0x00C2A443` |
| vector -> rotator (`0x0040D260`) | `0x00C38BD0` | `0x00C2A543` |
| SpawnActor (`0x00C66070`) | `0x00C38BF4` | `0x00C2A57A` |
| projectile initializer, vtable `+0x3A4` | `0x00C38DB6` | `0x00C2A611` |

The pistol is the shorter of the two because it has no second spawn-position branch: the
crossbow's `0x00C38BBB` exists precisely because two branches join there, and the pistol
computes its position inline.

**The initializer is the same slot.** The pistol's projectile is `DisBullet` and the
crossbow's is an Arrow, but both are `DisProjectile`, and both call the `+0x3A4` slot with
the direction local passed twice by address. So the direction a hook writes reaches
velocity and actor orientation by the path already traced for the crossbow; that half needs
no new work.

## 3. The locals at the join

| Role | Local | Written by |
|---|---|---|
| spawn position (float3) | `ebp-0x54` | once, at `0x00C2A4A1`..`0x00C2A4BF` |
| direction (float3) | `ebp-0x48` | from the aim cache, then possibly by `0x00BFFBA0` |
| ORIGINAL unit aim direction (float3) | `ebp-0x60` | the aim-cache out-param; never rewritten |
| the tweaks object | `ebp-0x18` | `[esi+0xA4]`, once |
| the source pawn | `ebp-0x1C` | the `0x00BFF440` return, once |
| the rotator out | `ebp-0x6C` | by the vec->rotator call |

`esi` is the context; `ebp` is the routine's aligned frame. Same contract as the crossbow's
bridge.

## 4. The one real difference: the spawn position is derived from the direction

This is the finding that changes the design, and it is why the crossbow's hook cannot
simply be pointed at a second address.

`DisTweaks_FirePistol` carries `m_fBulletSpawnDistance=150.0`. The routine spends it like
this, at `0x00C2A468`..`0x00C2A4BF`:

```
dir   = [ebp-0x60]            ; unit aim direction from the cache
dist  = [tweaks + 0x420]      ; m_fBulletSpawnDistance
spawn = origin + dir * dist   ; origin is the arg float3 at [edi+8]
      -> [ebp-0x54]
```

The crossbow's spawn position does not depend on its direction. The pistol's does.

**So replacing only the direction is wrong here.** The bullet would be spawned 150 units
along the OLD head direction and then aimed from there. The solver would still converge on
the endpoint - it aims from the actual spawn - but the bullet would begin its flight up to
150 units off the line the player is pointing along, which can put its first frame inside
geometry that the aim line never crossed.

**The fix is to move both together**, reconstructing the origin the engine used:

```
origin    = spawn - dir * dist          ; dir and dist both still live at the join
newDir    = normalize(endpoint - origin)
newSpawn  = origin + newDir * dist
```

This puts the bullet at the same 150-unit standoff the engine intended, on the line the
player is actually pointing along. It is the engine's own formula with our direction
substituted, not a new one.

## 5. Where the hook goes, and why not earlier

**`0x00C2A53C`, displacing 7 bytes** `8D 4D 94 51 8D 4D B8`
(`lea ecx,[ebp-0x6c]; push ecx; lea ecx,[ebp-0x48]`), resuming into the vector->rotator
call at `0x00C2A543`. No relative branch in the displaced range, so the bridge replays
them verbatim, exactly as the crossbow's bridge replays its six.

**It cannot go earlier.** `[ebp-0x48]` is passed BY ADDRESS to `0x00BFFBA0` at
`0x00C2A51A`, so that call can still write the direction. A hook placed before it - which
is the natural-looking spot, right after the aim cache returns - would have its write
silently overwritten and would present as "the hook fires, the counter moves, nothing
changes". That is this project's most expensive failure shape, so it is written down here
rather than discovered in a headset.

At `0x00C2A53C` everything the solver needs is settled and is a stack local: the final
direction, the final spawn position, the untouched original direction, the tweaks pointer
and the pawn.

Contract bytes to verify before installing, both of which must match or the hook refuses:

| What | Address | Bytes |
|---|---|---|
| the join | `0x00C2A53C` | `8D 4D 94 51 8D 4D B8` |
| the initializer call | `0x00C2A611` | `FF D2 8B 06 8B 90 48 01 00 00` |

## 6. Plan

1. `patterns.h`: the pistol's join, displaced bytes, context vtable, initializer call and
   its bytes, the four local offsets, and the tweaks field offset `0x420`. Every constant
   named for the pistol, never shared with the crossbow's.
2. `fire_aim_math.h`: one new entry point beside `solve()` that takes the spawn position,
   the original direction and the spawn distance, and returns the corrected PAIR. The
   existing `solve()` is not touched - the crossbow's behaviour must not move.
3. `fire_aim.cpp`: a second handler and bridge for the pistol context, gated on the pistol
   vtable, reusing every existing guard (possession, gameplay state, sample age, unit
   direction) verbatim. One shared `[Aim] FireFromHand` lever; the log line names which
   weapon it fired for.
4. `tools/fire-aim-host.ps1`: extend the offline checks to the pistol pair - that the
   corrected spawn stays on the aim line, that the standoff distance is preserved, that a
   zero or non-finite distance refuses rather than dividing, and that the crossbow rows
   still pass unchanged.
5. ENGINE_NOTES gains section 1-5 above, condensed, in the same commit as the code.

## 7. What would falsify this

The trace is offline and the routine has never been observed executing. The instrument
that can fail its own hypothesis is the per-shot log line: it prints the resolved spawn
distance read from the tweaks object, the reconstructed origin, and the angle between the
old and new directions. If `0x00C2A3E0` is not the player's pistol fire, the hook never
fires and the counter stays 0 with no refusals - which is a different and distinguishable
reading from "fires and refuses". If `[ebp-0x18]+0x420` is not
`m_fBulletSpawnDistance`, the printed distance will not read 150 and the guard refuses.
