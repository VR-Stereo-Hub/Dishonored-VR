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

## Build452 pause hang: live dump proves engine memory fatal (2026-09-18)

Tester reports hit-camera behavior correct in this run; health vignette invisible
after frame routing, so the effect placement change is not accepted. Rain unchanged.
Build452 log banner and installed DLL hash verified. No binary or INI changed.

Captured still-live PID27044 with full ProcDump (3605MB), a later64-bit mini
snapshot and a32-bit mini snapshot. Local dumps are D:/dvr-data/dumps/
pause-freeze452-27044*.dmp. Logs/current profile and hashes preserved in
build/playtest-candidates/pause-freeze452/support-20260918-072855-650.zip.
Exact452 symbols already archived by DLL hash. No uploads or process termination.

Full dump contains the engine fatal buffer indicating virtual-memory exhaustion.
The error text is game-generated; its generic disk-space advice is not a diagnosis.
32-bit mini dump shows main thread25820 in engine fatal cleanup, waiting, while
threads14964 and15576 wait for the allocator critical section owned by25820.
This supports an out-of-memory fatal that hangs in cleanup, not a demonstrated
new pause rendering deadlock. Process private bytes3318243328; virtual bytes
4121595904. Dump memory map below4GiB:165.28MiB free in total, largest21.875MiB.
A free-space snapshot does not identify the failed allocation size or owner.
Unlike443, no final SYSTEMMEM shadow failure is logged; engine allocator fatal
is directly evidenced by the retained message and stacks.

Tool correction: procdump64 captures AMD64/WOW64 contexts; the existing x86
reader produces invalid register values on those and must not be trusted.
The32-bit procdump.exe gives valid x86 registers/frame chains. Watcher now selects
the signed32-bit sibling when handed procdump64.exe; full64-bit dump remains
useful for memory inspection. Capture tested directly on the live failed game.

Next: attribute retained memory/texture twins using exact symbols/full dump,
then choose a measured footprint reduction. No crash-prevention fix is claimed.
Do not repeat the vignette-to-frame approach as a successful placement fix.
Restore a visible dedicated effects surface in a future candidate; do not
silently accept disappearance. Native rain emitter observed with40 drops;
health lens pointer null in final trace is not proof no red HUD draw existed.

## Build452 memory investigation and streaming test (2026-09-18)

Saved dump analyzed with exact452 symbols:2596 live CPU texture twins,
10200 created minus7604 released, zero twin allocation failures in this run.
The old shadowBytes counter is cumulative, not live memory.2580 2D twins
describe1754.64MiB of pixel payload including mip chains;16 cubes excluded.
86 textures with4096 maximum dimension account for638.67MiB;351 at2048
account for752.46MiB. Payload estimate excludes driver overhead/alignment.
Detailed derivation and limits are in ENGINE_NOTES below its new investigation.

Both current game INIs set NumStreamedMips=0 for13 SystemSettings groups.
Dump confirms large texture objects have full resident/requested mip chains.
This is strong retained-workload evidence, not proof all crashes are fixed or
that no other leak exists. Do not attribute who installed those settings.

User authorized closing the newly running game. Closed before config edits.
Prepared/applied reversible test: only those13 entries per file changed to-1.
Both full INIs backed up/diffed, CRLF verified, mod INI unchanged. Build452 retained.
Evidence/config manifest: build/playtest-candidates/texture-streaming452.
Both logs/current profile archived in its preinstall support ZIP.
Texture pack,4096 limits, pool160, headset resolution and F10 values unchanged.
32-bit memory watcher PID28440 armed at3000MB, one dump on threshold/exception/
exit; no game launched. Verify watcher output before next session's launch.

One launch question: does the previously failing play/pause sequence complete
without freezing with streaming restored? Expected: lower retained memory and
normal pause. Freeze with lower memory weakens this mitigation; renewed memory
exhaustion means streaming is insufficient. Stability in one run is not a release
guarantee. Vignette placement remains unresolved and was not changed in this test.
