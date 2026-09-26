# Next session: accepted September 26 baseline

Read CLAUDE.md fully, only the three newest STATUS sections, and this handoff.
The September 25-26 session is consolidated through PR132 into staging, including
PR131 cinematic work and PR128 shared-capture compatibility. Finalized branches are
retained. Start new work from fetched origin/staging in an isolated codex worktree.
The primary checkout belongs to a concurrent collaborator; do not switch or reset it.

The user accepted the local HUD/hand/menu/walking fixes. Installed build remains
v1.0.1-50-g6bc58a449; see STATUS for DLL/INI hashes, archive and installed manifest.
Do not substitute the newly integrated build without the task requiring it. Full INI
compatibility, whole-file diff and CRLF verification apply on every installation.
Never launch the game; the user does that. One defined question per needed launch.
No subagents. Credit commits to BioVRDev. No merge to staging without new explicit
permission for new work, and never merge to VR-Main. Retain finalized branches.

Preserve semantic HUD ownership through native Display/queue/replay. The low-health
D-pad/potion is an independent mode4 power-wheel movie; its native sprite movie owner
is at+BC, not+90. Do not restore the failed all-HUD-clip census prerequisite or private
tutorial panel. The final direct-owner correction95ae3f7af is headset accepted.
Rain remains LensDistance18/KeepSize0 and the accepted profile. Physical swings
remain tracked while trigger attacks can animate (HandAnimMeleeSwing0).

Walking fix6bc58a449 replaces repeated raw-slot probes with current live-object
membership and a whole-range check plus guarded boundary fallback. Cadence and
selection scope are unchanged. Final run accepted;269 measured collections average
6.349ms, maximum9.653ms. This is not zero overhead or a controlled whole-frame A/B.

Remaining scope: VR-229 remote early prison judder awaits the6187b2fd4 package
verdict. Scoped eye separation was reported fixed. The latest queued-progress code
is included in staging but was not in installed build50. Treat returned evidence by
its banner/hash. Global/FX and DLC05 HUD families remain selectively unverified;
do not blanket-capture them or claim universal HUD coverage. See HUD_ANCHORS,
FLICKER_REFERENCE and PERFORMANCE for the failed hypotheses and evidence.

New task: supplied by the user at the beginning of the next session.
