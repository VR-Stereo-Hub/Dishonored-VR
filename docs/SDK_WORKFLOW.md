# The generated UE3 SDK: CodeRed-Generator and the dismod Dishonored SDK

Two public repositories give this project a complete, offline map of the engine's reflected
classes, and are the ground other Dishonored DLL mods are built on. Both are referenced by
path from the local tool file and **never copied into the tree**: the generator is MIT, but
its *output* is game-derived (every class, field, offset and function name comes out of the
game's own reflection data), and the dismod repository carries no licence at all.

| Tool file name | What | Where it came from |
|---|---|---|
| `codered_generator` | **CodeRed-Generator** (MIT), <https://github.com/CodeRedModding/CodeRed-Generator>. A C++20 DLL that, injected into a running UE3 game, walks `GObjects`/`GNames` and writes a full C++ SDK: every class with its fields at their real offsets, every struct, enum, constant, and a `ProcessEvent` wrapper for every `UFunction` with its parameter struct. It ships a ready `Engine/Dishonored/` configuration: `GObjects` RVA `0x1023630`, `GNames` RVA `0x1035674` (VA `0x1435674` = our `kGNamesData`), `ProcessEvent` vtable index 59, 4-byte alignment, UTF-8 names |
| `dis_sdk` | **The SDK already generated for Dishonored** by the dismod project (<https://github.com/ectrc/dismod>, `external/sdk/`, "Generated with the CodeRedGenerator v1.1.3"): 44 files, 12 MB, `Core`, `Engine`, `GameFramework`, `IpDrv`, `GFxUI`, `AkAudio`, `WinDrv`, `OnlineSubsystemSteamworks`, `DishonoredGame`, `DishonoredGameContent`, each as `_structs.hpp`, `_classes.hpp`, `_parameters.hpp` and a `_classes.cpp` of wrappers. It matches the current Steam exe (the CorvoBody mod was compiled against it and against the same exe SHA-256 we have, checked 2026-10-07; every offset we cross-checked against `patterns.h` agreed) |

Setup on a machine that has neither:

```powershell
git clone --depth 1 https://github.com/CodeRedModding/CodeRed-Generator.git $env:LOCALAPPDATA\DishonoredVR\sdk\CodeRed-Generator
git clone --depth 1 https://github.com/ectrc/dismod.git                    $env:LOCALAPPDATA\DishonoredVR\sdk\dismod
.\tools\tool-paths.ps1 -Init     # detects both under %LOCALAPPDATA%\DishonoredVR\sdk\
```

## What it answers (grep the SDK before deriving anything)

| Question | Where | Example |
|---|---|---|
| **The offset of a property I can name**, offline, no game running | `<dis_sdk>\src\<Package>_classes.hpp`, the comment on each member is `// 0x<offset> (0x<size>) [flags]` | `grep -n "PlayerCamera;" Engine_classes.hpp` -> `0x0384` on `APlayerController`; `grep -n "// 0x03DC" Engine_classes.hpp` -> `APawn::Mesh` |
| **What is at an offset someone else's code uses** (a decompiled mod, a crash offset into a struct) | same, grep the offset inside the class block | CorvoBody's `pawn+988` is `0x3DC` = `Mesh` |
| **A bitfield's mask and byte** | `uint32_t bHidden : 1; // 0x0120 ... [0x00000002]` - the bracketed value after the flags is the mask | `AActor+0x120 & 0x8` = `bDeleteMe` |
| **The parameter layout to call a UFunction through ProcessEvent** | `<Package>_parameters.hpp`: one struct per function, members at their real offsets, `ReturnValue` last | `USkeletalMeshComponent_execFindSkelControl_Params { FName InControlName; USkelControlBase* ReturnValue; }` |
| **Which class declares a function / which functions a class has** | `_classes.hpp` member functions; `_classes.cpp` shows the full name string each wrapper resolves (`"Function Engine.SkeletalMeshComponent.FindSkelControl"`) | the strings CorvoBody embeds are exactly these |
| **Class hierarchy, enums, constants** | `_structs.hpp` (enums first, then structs), `_classes.hpp` (`class AX : public AY`) | `EBoneControlSpace`, `EPhysics` values |
| **Native-only fields** | **not here**: the generator only sees reflected `UProperty` objects, the same population our runtime `FindPropOffset` walks. A field that only native code knows (the hidden bytes between `EffectorLocationSpace` at `0xC4` and `EffectorSpaceBoneName` at `0xC8` on `USkelControlLimb`) shows as a gap. ENGINE_NOTES and IDA remain the source for those |

**What it does not replace.** It is a snapshot of one exe build. A game update moves
offsets, and the SDK will not know; the runtime resolver (`FindPropOffset`) will. The rule
stays: **a name from the SDK, resolved at runtime, checked against the SDK's number, refused
on a mismatch** - the SDK is the cross-check that turns a runtime resolve into a verified
one, and the offline grep that tells you the name to resolve.

## Regenerating it (a user action, not an agent's)

The generator is an **injected DLL that runs inside the game**: build
`CodeRedGenerator.sln` (x86, VS 2019+), set `Engine/Dishonored/Configuration.cpp`'s output
directory, start the game, inject the DLL (any injector), wait for its "finished" message.
That is a game launch, which the session rules forbid an agent from doing; ask. The shipped
dismod SDK is good for the current build and the snapshot date is in each file's header.

To generate for a **different** build (GOG, Epic, an older Steam depot): the `GObjects` and
`GNames` RVAs in the configuration must be re-derived for that exe first (`disasm-rva.py
xref` on the `GNames` data pointer pattern ENGINE_NOTES records; never copied), or the
configuration switched to its pattern mode.

## Using it from our code: the decision

The SDK's headers are **not** included in the proxy. Reasons, in order: the output is
game-derived and this tree never carries that; a 12 MB header set would dominate the unity
build; and the project's existing route (`patterns.h` for the few hard numbers, the runtime
reflection resolver for names, the SkelControl / ProcessEvent machinery in `ue3/`) already
calls UFunctions without a generated wrapper. The SDK is a **reference**: it is grepped,
its numbers go into `patterns.h` with "from the dismod SDK, confirmed at runtime by
`FindPropOffset`" as the derivation, and ENGINE_NOTES cites the file and line.

What a generated-SDK route *could* buy later, if the owner wants it: typed access to any of
the ~1,000 `DishonoredGame` classes without a per-field derivation (the inventory, the
player FSM state names, the camera modifier list CorvoBody reads for mantles), and a
ProcessEvent call to any of the 2,500+ script functions by name with its parameter struct
spelled out. That would be a `src/game/dishonored/sdk/` module with a curated, hand-copied
subset of declarations and an attribution comment, decided per class, not a wholesale include.

## Where it was first used

`docs/dishonored/CORVOBODY.md`: naming every offset in the CorvoBody decompile
(`pawn+0x59C` = `m_pInventory`, `pawn+0xA2C` = `m_pPlayerMasterFSM`, the `USkelControlLimb`
effector fields) and confirming that `patterns.h`'s `0x190/0x19C/0x1A8` mesh transform
offsets, `0x384` camera and `0x330` POV cache are what the SDK says.
