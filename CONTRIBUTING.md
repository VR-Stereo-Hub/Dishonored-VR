# Contributing

This is a VR mod for Dishonored (2012), continuing GingasVR's discontinued alpha with the
author's permission. It runs locally, on a copy of the game you own.

## If you are reporting a bug or asking for a feature

Open a GitHub issue using one of the templates. Read
[`docs/KNOWN_ISSUES.md`](docs/KNOWN_ISSUES.md) and
[`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md) first, and **attach
`dishonored_vr.log`**. It sits next to `Dishonored.exe` in `<game>\Binaries\Win32`, the
previous run is kept one deep as `dishonored_vr.prev.log`, so copy it out before relaunching.

Please do not attach game footage, frame dumps or captures. This repository carries no
game-derived content, deliberately.

## If you are on the team

The board is the source of truth: [linear.app/vr-stereo-hub](https://linear.app/vr-stereo-hub),
team **VR**, project **Dishonored VR Mod**. The whole flow, from finding a ticket to cutting a
release, is in **[`docs/LINEAR_AND_GITHUB.md`](docs/LINEAR_AND_GITHUB.md)**.

The short version:

1. Search Linear. Create the ticket from the template if it is not there, with project,
   milestone, priority and a `Type` label filled in.
2. Branch `<owner>/vr-<n>-<slug>` off `VR-Main`. Copy the name from the ticket.
3. Validate in the simulator (`tools\xrsim-*`) before asking anyone for a headset run.
4. Open the PR with `Fixes VR-<n>` as the first line of the body, and fill in the template.
5. Merge to `VR-Main`. Linear marks the ticket Done.
6. Update `docs/STATUS.md`, tick `docs/ROADMAP.md`, push.

## Before you write any code

Read [`CLAUDE.md`](CLAUDE.md). It is the contributor guide as much as the agent guide, and its
hard rules are not style preferences: they are the record of things that have already gone
wrong here.

The ones that catch people first:

- **Never commit game-derived content.** No decompiled UnrealScript, no extracted assets, no
  frame dumps, captures or crash dumps.
- **No em dashes anywhere.** Use `-`. `tools\lint.ps1` enforces it, because the character has
  caused PowerShell 5.1 parse errors and mojibake in logs.
- **Every engine address and UE3 offset lives in `src/game/dishonored/patterns.h`** and is
  documented in `docs/dishonored/ENGINE_NOTES.md` with how it was derived. Every hook
  byte-verifies its target and refuses on a mismatch.
- **Every new render lever ships default OFF with a live A/B toggle.**
- **Acceptance is a measured effect, not landed code.** A verified write is not an honoured one.
- **Never quote a chat verbatim** in a commit message, PR body, issue, comment or doc. Report
  the observation instead.

## Building

```powershell
.\tools\build.ps1 [-Release]     # d3d9.dll and the shim, simulator and smoke client
.\tools\install.ps1 [-Release]   # copies into <game>\Binaries\Win32
.\tools\lint.ps1                 # the em-dash gate and friends
```

32-bit only; the CMake guard stops a 64-bit configure on purpose. A clean clone needs
`git clone --recursive` for the submodules under `third_party/`.

## Testing without a headset

`tools\xrsim-launch.ps1` runs the game against a simulated 32-bit OpenXR runtime that presents
as a Quest 3: head and hand poses, every controller button, deterministic frame stepping and
per-eye compositor captures. Almost everything can be answered there.
[`docs/VERIFICATION.md`](docs/VERIFICATION.md) is the catalog: intent, tool, command, and how to
read the result.

Perceptual questions - comfort, judder, world scale, warp - still need a real headset and the
F10 overlay. Those tickets carry the `needs-headset` label.

## Licence and provenance

No code from UEVR (all rights reserved; concepts only). REFramework (MIT) may be adapted with an
attribution comment. The OpenXR runtime layer is adopted from the BioShock trilogy VR mod and
stays as close to that copy as the D3D9 host allows, so fixes port between the two projects.
