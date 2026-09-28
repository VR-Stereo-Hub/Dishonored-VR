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
