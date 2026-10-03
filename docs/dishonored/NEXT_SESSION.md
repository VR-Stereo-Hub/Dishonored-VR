## 2026-10-03: accepted F10 ReShade and native launcher audit

Build 266 passed the tester's F10 ReShade controller usability check. The native launcher
now follows the selected sidebar design and exposes Collect logs on Overview. Read the
newest STATUS and INSTALLER entries for the audit fixes and evidence. ReShade runtime
install/on/off/removal are implemented; HD texture installation stays manual for 1.0.3.
Preserve the tester's Enabled=1, Carinth preset and local height; public Enabled=0 and
HeightOffsetM=+0.060 remain. Right-hand trims are already the approved repository defaults.
Full INI comparison and CRLF verification are mandatory on any install. No additional
headset run is needed solely for the launcher layout; do not claim whole-release acceptance.
No game launch, merge or release declaration. Publication is blocked pending explicit
approval of the origin destination after automatic approval review rejected the push.

# Next session: texture-pack candidate

The active candidate is `codex/vr-133-texture-reshade`. Start with the newest STATUS entry
and the community integration section in [PERFORMANCE.md](PERFORMANCE.md).
One next-launch question: does loading an existing save and quickloading it three times
return to gameplay every time without a crash? ReShade stays absent for this test.
Verify installed build/banner and archive current plus previous logs before another launch.
The original controller-remapping handoff below is historical; do not replace the active
texture test with it. No merge is authorized.

# Next session

Controller bind remapping is built on `claude/controller-remap` (PR against staging, not merged):
the reference is [CONTROLLER_BINDS.md](CONTROLLER_BINDS.md). It is host-tested only.

Owed, in order:

1. Headset: with no `[ControllerBinds]` section, check the everyday actions, physical crouch and a
   sword swing behave exactly as before (the log's `input/binds (config): ... the shipped layout`).
2. F10 > Controls > Button mapping: move one action with "Press to set", use it in game, then
   "Reset to default layout"; the log names each change.
3. Optional: a simulator sequence asserting the `pad: xbtn=` bits per action before and after a
   remap (`binds Jump B`), so the default can never drift unnoticed.

Linear refuses new tickets (free issue limit): free a slot or name an existing ticket for the PR.
