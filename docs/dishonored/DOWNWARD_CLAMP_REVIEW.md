# Downward character movement after the eye restore (VR-69)

2026-09-10. Candidate based on the headset-confirmed `b3a1ff46`, without later
integration changes. Brief startup settling is accepted for now.

## Confirmed baseline and remaining observation

The eye restore eliminated sustained weapon flicker and equipment-swap flicker
in the reported run, while retaining world and weapon stability during head
turns. The remaining flicker follows downward character movement: crouching,
descending slopes and falling. Moving the controllers or the tracked head
vertically did not reproduce it; uphill movement and jumping were also reported
stable. This is a perceptual distinction, not a measured velocity trace.

The log identifies `vr33-hands-working-59-gb3a1ff46`. Its final palette-eye
summary contains 170 unknown evaluations out of 107,600, with 15 large-step
ambiguities. The first increase is in the interval containing crouch transitions;
another increase occurs near downward capsule/ceiling movement. These three-second
summaries cannot establish event-by-event causation. Weapon switching succeeds
in the report and the log shows new weapon contracts being adopted.

The run is preserved locally under `build/vr69-bisect/eye-restore-result`.
Log SHA-256: `EE519F08C2FBFAC834535C331D3D94CDF9C6EC6F8F1D736C1DEB5BFCEAFCD559`.
Known-good DLL SHA-256:
`C615EF48547E5A822499B819260913ABE3DA293A63BD04F19F70F943EB9FA4A0`.

## Concrete writer interaction

`FovLeverApply` runs before `camera::apply_offsets` on the script dispatch.
The former clamps the Z component of four camera fields, including the selected
eye field at `kPovOffs[0]`. The latter avoids accumulating eye/position offsets
by comparing all three coordinates with its previous write.

When the clamp changes only Z in a still-owned field, the whole-vector comparison
fails. The next offset write treats X/Y, which still contain the previous eye
offset, as a fresh base. For example, base X=10, left offset=-3.155, then a Z
clamp and right offset=+3.155 yields X=10 instead of 13.155. Repeated clamping
can accumulate offsets. This defect is reproducible without the game.

The clamp tightens during descent and releases upward, which fits the directional
report. That is a hypothesis connecting the proven defect to the visual symptom;
the existing log does not expose the writer's previous vector, so it cannot prove
that this exact ownership failure occurred on every reported flicker.

## Narrow correction and regression boundaries

The clamp now informs the camera writer when it modifies an exact previous write
on the same camera object and field. It adds the Z clamp to the remembered mod
offset and updates the remembered written Z, preserving the original base. A
fresh engine vector, a different object/field, an inactive writer, or the eye
test's ownership does not receive that bookkeeping change. The Z clamp itself
still applies to the same fields under the existing conditions.

No per-axis guess is added to base recovery. An engine-side Z-only change remains
a fresh base under the original rule. Eye inference, pose lag, controller mapping,
weapon contracts, render settings, the ceiling value and its easing are unchanged.
Only camera.cpp, camera.h and one call in fov_lever.cpp affect runtime behavior.

Candidate source: `417bfad9`, directly after `b3a1ff46`. Its first eight matched
clamps log `camera/clamp-rebase` with the field and retained offsets. If this line
never appears during reproduction, the protected sequence did not execute and
the visual result must not be credited to this mechanism.

The normal x86 candidate was built and installed with unchanged ini bytes.
Build ID: `vr33-hands-working-60-g417bfad9`. DLL SHA-256:
`1781FDAA987233F5E2E9A0A448A4CB47E1A4AE595FD2D0A99AF1C9C7C722D762`.
The previous installed DLL/ini/log are backed up in
`build/vr69-bisect/before-downward-clamp`. The visual result is pending.

## Verification and next visual gate

`tools/camera-clamp-host.ps1` compiles the actual production writer/clamp functions.
All 19 checks pass. `-LegacyClamp` substitutes the original raw Z assignment and
fails six assertions in the descent/release cases. Controls cover upward release,
ordinary stereo pairs, repeated descent, roll/lean offsets, fresh engine writes,
other camera objects/fields, and invalid input. This is a deterministic writer
test, not a simulator render or a headset verdict.

Compare only with `b3a1ff46`: crouch/stand, descend/ascend the same slope, and
fall/jump; then check head turns and equipment swaps. Startup settling is not an
acceptance failure for this change. Any regression in the working behaviors sends
the installation back to the saved `b3a1ff46` DLL. The combined mainline branch
remains a separate verification step; no merge or release is implied.
