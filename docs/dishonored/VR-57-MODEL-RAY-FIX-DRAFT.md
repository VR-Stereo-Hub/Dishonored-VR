# VR-57 model ray and native launch correction: working draft

User request: preserve the newly tuned hand offsets, align the guide with the
crossbow model using measured geometry, and repair bolt-to-guide disagreement.
Saved before implementation so another session can resume. No game launch.

Baseline: 6de3b88a, FollowHandTrim implementation after 1af1527e native fire hook.
Reported behavior: translation moves the guide, guide lies left of the model,
and bolt no longer agrees with the guide. These are observations, not yet causes.

Execution:

1. Preserve current source, installed settings and run evidence. Read current
   firing/refusal logs and inspect the exact ray consumed by native firing.
2. Inspect existing crossbow/held-bolt geometry identification and transforms.
   A stable axis needs a verified mesh identity, local geometry, sign and pivot;
   do not infer it from a generic longest axis or a transient flying projectile.
3. Derive one model-based origin/direction through the same placement transform
   that draws the weapon. Keep pose/hand/calibration identity and freshness with
   it. Avoid publishing whichever eye or draw happened last as a current ray.
4. Fix publication/mapping/consumer discrepancies supported by code and run
   evidence. Native spawn should converge on the exact published endpoint;
   gravity, obstruction and assist must not be mislabeled coordinate faults.
5. Validate actual production math and hook contracts offline, build x86, install
   the candidate and verify hashes while preserving tuned offsets. Do not launch
   the game or restore the old DLL. Save implementation findings and remaining
   limits for handoff. Do not claim measured runtime success without evidence.

This is a draft, not a claim that the geometry source or cause has been found.

Confirmed and saved: FireFrame was published BEFORE follow-trim changed g_ray;
visuals used the changed ray. Publication now follows transport, and unavailable
calibration invalidates the shared ray. This is a demonstrated code defect.
The run contains six native writes and delayed velocity measurements compared
to the old ray, so their small residuals did not validate the displayed guide.

Implementation saved: new bolt_axis.h pure PCA/transport and bolt_model_ray.cpp
read-only measurement on verified held bolt color draws. Requires rigid skinning,
16:1 axial variance and an unambiguous native-view forward sign. Stores the ray
in corrected palm space; current grip/trim reconstructs it without per-eye lag.
ModelRay is default off and will be enabled in this installation. Snapshot expires
after 250 ms without a qualified loaded-bolt draw. Geometry acceptance remains
unverified in game. Existing range/trim values are preserved. Geometry/transport
638 host checks and existing 73184 checks have passed so far; final build/install
and handoff are still pending.
