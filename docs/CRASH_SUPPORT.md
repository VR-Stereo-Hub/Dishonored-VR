# Crash reports and release diagnostics

## Player workflow

Double-click Collect VR Support.cmd in the installed Win32 folder after a problem,
preferably before launching again. It creates a timestamped ZIP in Desktop /
DishonoredVR Support and selects it in Explorer. Send that ZIP with a description
of the action, headset/runtime, texture mods and whether the problem repeats.

The bundle contains current/previous logs, mod settings, crash text, runtime status,
shim log when present, DLL/executable hashes and a dump inventory. Missing optional
files are allowed. Game assets, saves and dumps are excluded by default.
Logs can contain local paths/account names. Nothing is uploaded automatically.
Maintainers can explicitly include the latest dump; memory dumps should be shared
privately and their timestamps checked against the reported run.

## Maintainer setup

Microsoft ProcDump: https://learn.microsoft.com/en-us/sysinternals/downloads/procdump
Use the signed Microsoft executable, kept outside the release package.
tools/watch-crashes.ps1 verifies its signature, waits for an exact executable
path, then attaches by PID. It never launches or kills the game and does not
register a system-wide debugger. Explicitly opt into a diagnostic session.

- Crash mode: one mini dump on an unhandled exception or process termination.
  A normal exit also triggers; a termination dump is not proof of a crash.
- Memory mode: one full dump at the chosen process commit threshold, unhandled
  exception or termination, whichever arrives first. Default threshold3000MB
  is a diagnostic trigger, not a proven safe budget. Full dumps may be several GB
  and can pause the process during capture. Obtain explicit consent.
- Neither catches every possible failure. A game-handled fatal dialog may not
  trigger an exception dump until exit. Capturing the still-live failed process
  may be necessary. Threshold capture can precede the interesting allocation.
- After the one dump, monitoring ends. Rearm for another run. Every2seconds a
  CSV records private/virtual/working-set bytes and handles while attached.
- Evidence and automatically preserved logs go under DataDir/support-watch.
  Dumps go under DataDir/dumps. No upload happens.

Keep exact release symbols. tools/package.ps1 now calls archive-symbols.ps1,
which retains d3d9.dll and d3d9.pdb under build/symbol-archive/<DLL SHA256>.
Back this archive up with release artifacts; never substitute another build's PDB.
The archive is maintainer-only and is not in the player package.

## Current allocation crash: VR-133

Build443 fails creating a DXT5 SYSTEMMEM texture copy with only63.6MiB virtual
space free and a largest free block of2.1MiB. The game is already large-address-aware.
Available physical RAM is not the limiting quantity. No prevention fix is claimed.

Next diagnosis compares early/high-memory dumps and texture-copy lifetimes:
separate a leak from a consistently oversized asset workload. Reducing the texture
pack is a useful controlled A/B, not an established fix. Lower eye resolution may
also free render-target memory but does not fix a leaking allocation owner.
Do not silently discard CPU texture copies: UE3 reads them during mip streaming.
Do not fake successful texture locks. A robust allocation-path change must preserve
readback/update semantics and be tested under forced failure before release.

Validation so far: collector tested against real installed logs and synthetic
locked-log/path-with-spaces/custom-DataDir fixtures; dump inclusion is opt-in.
No game launched. External game dump capture requires explicit consent.
