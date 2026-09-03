# UnrealScript decompilation workspace

Working area for decompiling the game's script packages.

**Everything in this directory except this README is ignored, deliberately.**
The decompiled source is Arkane/Bethesda's copyrighted content: it is a local
research aid and is never committed, published, or quoted at length. Findings
go to `docs/dishonored/ENGINE_NOTES.md` in our own words, citing the class and
member NAME; the source text stays here.

## The two steps, and why one is not enough

Dishonored ships **no `.u` files at all**. It is a cooked-console build, so the
script is baked into `.upk` alongside the content, and every script package is
**LZO-compressed**. UE Explorer 1.6 and older cannot read a compressed package:
it exits 0 and writes a `.UPKG` metadata stub with no classes, which looks
exactly like success. Decompress first.

```
decompress.exe -out=unpacked "<game>\DishonoredGame\CookedPCConsole\<Pkg>.upk"
UEExplorer.exe "unpacked\<Pkg>.upk" -console -export=classes -silent
```

Exports land in `%APPDATA%\EliotVU\UE Explorer\<version>\Exported\<Pkg>\Classes\`,
NOT next to the package. Running UE Explorer with no file path opens the GUI and
blocks.

## Where the script actually lives

`DishonoredGame\CookedPCConsole\` is the only directory with packages, and only
ten of its 470 are script: `Core`, `Engine`, `GameFramework`, `GFxUI`, `IpDrv`,
`AkAudio`, `OnlineSubsystemPC`, `OnlineSubsystemSteamworks`, `DishonoredGame`
(the big one) and `Startup`. The rest is content. `DishonoredGame\DLC\` exists
but is empty unless DLC is installed.

## Package header, measured

File version **801**, licensee version **30**, engine version **9411**, cooker
133, compression flags **0x02 (LZO)**. Note the engine version: `CLAUDE.md`
describes the game as UE3 build 9099 and the packages disagree.

## What is reliable

There is no native-function table for this game, so treat function BODIES as a
strong hint and DECLARATIONS as near-certain. Declaration ORDER is what makes
this useful for offsets - adjacent `var` lines are adjacent in memory, so one
known offset anchors its neighbours, and `FindPropOffset(class, name)` then
confirms each against the running game. That is a derivation, not a guess, and
it is what `game-cmd.ps1 "fovprobe"` prints.
