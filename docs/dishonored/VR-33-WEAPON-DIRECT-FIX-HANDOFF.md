# VR-33 weapon attachment: direct repair handoff

Date: 2026-09-08. Base: 768fbf71 on claude/vr-33-rotation-grip-and-weapons.
Status: implemented, Release built, host tests passed, installed for a user-run
headset test. Visual attachment is NOT confirmed. No hide sweep is required.

## What the previous run actually established

The preserved pre-repair log reports 10,585,656 routed draws, 302,039 comparisons,
zero matches, zero attempted patched draws and zero successful patched draws.
The reported nearest candidate was consistently the sword. This establishes a
matcher failure, not a failed GPU correction. Absence of blinking was expected
with WeaponId disabled. The old nearest-so-far label was inaccurate: it reported
the last comparison, not a minimum over a time window.

## Implemented repairs

1. **Both hands participate.** The old uncached route stopped at the first valid
   WaCommon, normally hand 0, and then filtered ALL candidates to that hand.
   Hand 1 could never win while hand 0 was available. The new route predicts all
   eligible components for both current views, then chooses the winning hand.
   Only candidates inside every acceptance band compete. A slightly closer
   candidate outside one band cannot exclude a valid candidate.
2. **Full affine bridge.** Native component matrices retain scale. The bridge
   is K = L_draw_ref * inverse(L_native_ref), and the predicted member is
   K * L_native_member. Inverses support scaled transforms; transpose-only
   inversion is reserved for genuinely orthonormal matrices elsewhere.
   The old bridge subtracted translations and compared rotations directly.
   It never established that native and shader orientations were identical.
   A reference fitting its own K is a tautology, NOT verification that every
   weapon uses this conversion. Independent member matches are the live test.
3. **Exact palette bounds.** PcRefreshLayout now publishes BoneMatrices'
   reflected register COUNT as well as its start. WaDraw requires complete
   three-register matrices within the device bank and no overlap with VP or
   LocalToWorld. A hand-style c6 x225 declaration patches c6..c230 only.
   The old path would patch 250 registers from c6, overwriting LocalToWorld
   and lighting if it ever matched. That bug was dormant in the failed run.
4. **Fresh authorization on every draw.** The contract table is reporting
   history, not permission to patch shared buffers forever. Each draw must
   match a currently snapshotted owned component. Keys now include stream
   offset, primitive type and minIndex. A full table evicts its oldest record
   instead of disabling acquisition for new weapons. Device reset clears
   weapon corrections, contracts and the reflected layout.
5. **Coherent snapshots.** The script lane copies component records under an
   SRW lock. Each published hand correction carries that immutable snapshot,
   its generation, Present, controller-pose generation and eye. Snapshots older
   than 100 ms refuse publication. This prevents torn publication; it does not
   establish the native animation-to-render timing. Components still come from
   the existing owned-object collector, which this repair does not rewrite.
6. **Assembly ambiguity.** Crossbow and loaded bolt may share one correction
   when indistinguishable in the same hand. Different hands or different
   assemblies refuse, including an exact zero-error tie. Same-hand alone is
   insufficient authority. Scale participates with a 0.5 percent band; angular
   and positional bands remain 0.25 degrees and 1 uu in the installed ini.
7. **Draw state restoration.** Only the reflected bone block is changed. The
   real draw HRESULT is returned. Palette restoration is attempted after a
   failed upload too. The weapon uses the same optional full-depth-range
   treatment as the hands and restores the original viewport after drawing.
8. **No scene-budget starvation.** AttachMaxTry remains a diagnostic watermark.
   Exceeding it is counted, but cannot permanently suppress later weapon draws
   just because world draws consumed the first 3,000 comparisons. Work remains
   bounded per draw by two hands and at most 16 native components each.
9. **Hand assignment.** Defaults now agree with FpHandFor and the documented
   authored attachment convention: sword RIGHT (1), crossbow/bolt LEFT (0).
   The installed configuration is changed to those same values. These remain
   overridable. No grip recapture or orientation/position trim was changed.
10. **Useful reporting.** `wa: beat v2` reports comparisons for each hand,
    missing-view/bridge/layout/source conditions, stale snapshots, actual draw
    attempts/successes and restoration failures. `wa: interval nearest` gives
    true interval minima per hand, including scale error. `wa: contract` lists
    each retained draw contract's asset, side, register range and placed count.
    Component rows now all print at Info once per ten seconds; a rate limiter
    inside the former loop effectively suppressed the later rows.

The runtime and frame_test share weapon_frame.h. The new tests exercise both
hand selection, excluded-candidate ranking, ambiguous ties, scale rejection,
current-transform revalidation, affine inversion/bridging, singular rejection,
register bounds and outside-block sentinels, and common-delta conjugation.

## Validation and installation

- Release build succeeded. Existing DVR_CAT macro-redefinition warning remains.
- 28 existing hand tests and 19 weapon regression tests passed.
- tools/lint.ps1 and git diff --check passed.
- Built and installed DLL hashes were compared after installation.
- Local rollback files are in build/wa-before-codex: original d3d9.dll, ini and
  the useful pre-repair headset log. Do not commit them.
- A simulated-runtime launch reached a session but crashed before gameplay.
  The dump's fault instruction is in Dishonored.exe at RVA 0x60907e, exception
  0xC0000005. This does not identify the root cause. The available attachment
  beat had zero attempted weapon patches. There is no live placement evidence
  from that run. Further simulator runs were stopped at the user's direction.
- The simulator selection was per-process. The installed headset ini was
  verified unchanged before the deliberate two hand-assignment edits.

## User test, no sweep or extra calibration

Launch normally with the headset. Draw the crossbow and sword. Move only the
left controller: crossbow and loaded bolt should follow that hand. Move only
the right: sword should follow. Turn and translate the head while holding both
controllers still. Check each eye, then holster/re-equip and reload.

Read `wa: contract` for BOTH crossbow_01 and bolt_01, and confirm increasing
placed counts and zero restore failures. A successful crossbow draw alone does
not prove that every pass or the bolt was covered. The first live run should
also confirm that sword and crossbow use the intended hands; do not conceal a
side mismatch with a large grip offset.

## If the test still fails

- No member comparisons: examine component REF/member rows, no-view,
  no-bridge and stale-snapshot counts. Do not widen matching tolerances.
- Comparisons on both hands but no matches: use the interval angle, position
  and scale minima. Inspect native/render snapshot timing and whether the
  bridge predicts independent members. The bridge is an implementation
  hypothesis until that check succeeds.
- Attempts increase but no visible motion: inspect the actual shader's palette
  consumption and whether another pass supplies an unmoved visible copy.
- A part moves but another stays: inspect that part's reflected shader and
  component membership. Unskinned shaders are still unsupported; no pretend
  one-bone fallback was added. Known native candidates here are skeletal
  components, but a particular rendering path can still be unsupported.
- Corrections unavailable before the hand draws: establish a same-generation
  source earlier in that view. Never reuse the preceding eye/Present.
- Pose dropouts only during movement: measure CPU component freshness against
  rendering, rather than increasing the 100 ms snapshot freshness bound.

Gameplay projectile aim, melee collision, released bolts and muzzle effects
remain engine-side work. This patch carries identified held skeletal draws
through the hand's existing correction; it does not change gameplay consumers.
Do not start another automatic hide sweep or retune the working hand rotation
before inspecting the new attachment counters.

## First headset result and the follow-up revision

The user ran the first installed direct fix while the follow-up was still
being prepared. That run used AttachSwordHand=0 and AttachCrossbowHand=1.
The user observed weapons moving with the wrong hand, a large palm offset,
and flickering dark silhouettes at the former weapon positions.

The preserved run (build/wa-first-moving.log, local only) reports 196,422
successful patched draws, zero restore failures, and near-zero independently
predicted transform residuals for sword, crossbow and bolt. Thus identification
and submission now have positive live evidence. Correct alignment and complete
pass coverage do not. The hand-assignment correction described above was NOT
in that first test. Applying the opposite palm's delta is a concrete cause of
wrong-hand placement and an apparent offset; do not add a metre-scale trim.

The follow-up also removes MpAcquireCtx's camera-projection requirements from
weapon placement. A weapon draw needs LocalToWorld, its bone palette and a
current hand correction; it does not solve a controller pose from VP. Requiring
a symmetric camera VP was excluding otherwise eligible non-camera passes.
Hand placement/capture retain their existing VP requirements.

Each member can now match either K * nativeMember (the reference draw space)
or nativeMember directly (a world-space pass). The latter receives
inverse(K) * D_common * K before conversion into member-local space. Both
predictions are independent of the candidate draw being tested, and both
retain strict position/orientation/scale gates. No constants are guessed when
a shader lacks BoneMatrices or LocalToWorld. No previous-eye correction is
borrowed for a pass that occurs before the current hand publication.

Two additional host cases cover a bone palette without camera VP and equality
of world-space versus rebased-space motion. Final total: 28 hand + 21 weapon
cases. Ghost removal and corrected grip alignment remain pending user testing.
If the silhouette survives, obtain its shader/range and view timing rather
than asserting it is a shadow pass from its appearance alone.

Final installed follow-up: 2026-09-08 07:57:37 local build time. DLL SHA-256:
DFD481BA8FFBE423CD9F60A349112AC1628BA6A605F272AAA9C07AC8FC1FF3F4.
The installed DLL matched the build output byte-for-byte. Configuration diff
against the backup contains only AttachSwordHand 0 -> 1 and
AttachCrossbowHand 1 -> 0. The build stamp may still identify base 768fbf71
with a dirty-tree suffix because installation preceded the source checkpoint;
use this SHA-256 and the new per-contract log lines to identify the binary.
