# VR-57: the model ray for every loaded projectile, not just one bolt

2026-09-12 working draft, saved before implementation so another session can
resume. No game launch.

Baseline: `56ad9673`. ModelRay confirmed working in a headset on the crossbow with
the regular bolt and with the explosive bolt: the laser sits on the bolt line and
bolts land on it. Two reports remain.

* **The pistol has no laser and fires to head aim.**
* **The poison bolt behaves the same as the pistol.**

## Why, and it is one line

`BrMeasure` gates on `strcmp(w->asset, "bolt_01")`. Only that one asset is ever
measured. Everything else returns before the geometry is looked at, so no snapshot
is published, the ray goes stale after 250 ms, and the native fire hook falls back
to the engine's own aim. That fallback is working as designed; the gate is the bug.

The asset names the renderer reported in the confirmed run:

| Component | Asset | What it is |
|---|---|---|
| `pArrowMesh_HighRes` | `bolt_01` | the regular bolt, the only one measured today |
| `pArrowMesh_HighRes` | `Bolt_Flare` | a bolt variant |
| `pBulletMesh` | `Gun_bullet_regular` | **the pistol's loaded bullet** |
| `pPlayerMesh` | `crossbow_01`, `Wpn_PlyGunElite`, `Wpn_PlySword01` | the weapon bodies |
| `pMesh` | `EliteGun`, `Skm_Player` | the gun, the player body |

So the pistol already has exactly the thing the crossbow has: a loaded projectile
mesh, drawn in the view model. It needs no new idea, only the name test widened.

## On using the weapon model instead, and lining it up with its top

Asked directly, and the answer is no - for a reason worth recording rather than a
preference.

**The bolt works BECAUSE a bolt is nearly one-dimensional.** The axis fit demands
16:1 variance along its longest axis before it will aim anything, and a bolt passes
that trivially. A weapon body does not:

* The **crossbow's** widest dimension is its bow arms, ACROSS the barrel. A fitted
  long axis would pick that, not the barrel, or be refused outright. Picking it
  would aim the shot sideways with full confidence.
* The **pistol** has a grip at right angles to its barrel, so its long axis is
  closer to the barrel but is pulled by the grip and the frame.

And "line it up with the top of the model" is not a measurable feature. It is an
offset chosen by eye, which is the guessed constant this project's rules forbid
shipping as a measured one - the same objection that killed the per-weapon rotation
earlier today. A measured bolt axis needs no such constant, which is exactly why it
landed on the bolt line first time.

The loaded projectile IS the barrel axis, already, for every ranged weapon here.

## Change

1. Replace the single-name gate with a projectile-asset test: `bolt_01`, any asset
   beginning `Bolt`, and the gun bullet. Keep it a NAME test only for selecting
   candidates - the safety is not in the name.
2. **The geometry gates stay and are what make this safe.** Rigid single-bone
   skinning, 16:1 axial variance, an unambiguous forward sign against the native
   view, bounded vertex counts, validated index ranges and lock extents. A mesh
   that is not a near-1D projectile cannot pass them, so widening the candidate
   list cannot aim from a weapon body even if one were named.
3. Extend `WaHandFor` so the gun bullet resolves to a known hand rather than
   falling through to the default with `known=false`.
4. Log the asset actually measured, and log a REFUSAL naming the asset when a
   projectile-looking candidate fails the geometry. An unknown poison-bolt mesh
   must say which test it failed instead of silently producing no laser.
5. Per-asset cache, so switching ammunition re-measures rather than reusing the
   previous projectile's axis.

## What this cannot fix

The sword has no projectile and will never get a measured axis this way; it is not
a ranged weapon and is out of scope. Any ranged weapon whose loaded projectile is
not drawn in the view model will still refuse, with a reason. The guide still drops
out while no projectile is drawn, including mid-reload.

## Verification

Extend the offline checks to cover the asset selection and a per-asset cache, with
a case proving a wide or ambiguous mesh is still refused. Then the existing five
suites, x86 build, lint, install, and diff the installed ini against the previous
one before handing back - the check that would have caught the rig radius.
