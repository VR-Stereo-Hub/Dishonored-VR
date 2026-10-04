# UnrealScript decompilation workspace

Working area for the decompiled UnrealScript of Dishonored's script packages
(`Core`, `Engine`, `GameFramework`, `DishonoredGame`, `GFxUI`, ...), produced with
UE Explorer 1.6.2's UELib. The corpus is declarations and defaultproperties: class
hierarchies, property names, shipped tuning values. Function bodies are not in it.

**Everything in this directory except this README is gitignored, deliberately.**
Decompiled UnrealScript is Arkane/Bethesda's copyrighted code and UE Explorer is
third-party software: neither is ever committed or redistributed. Record *findings*
(class layouts, property names, offsets) as summaries in
`docs/dishonored/ENGINE_NOTES.md` instead.

Local layout (the tool file, `tools\tool-paths.ps1`, records the first two):

| path | what |
|---|---|
| `dishonored\<Package>\<Class>.uc` | the corpus (`uscript_corpus`) |
| `_ueexplorer\ue-explorer\` | UE Explorer + `ExportScripts.exe`, our UELib batch driver (`ueexplorer`) |
| `_tool\` | `ExportScripts.cs` (its source), `export.ps1` (one process per package, so a UELib StackOverflow kills one package, not the batch), `inventory.ps1`, `decompressor\` |
| `_work\unpacked\` | the packages, decompressed for UELib |

Rebuilding the corpus on a new machine: obtain UE Explorer
(<https://github.com/UE-Explorer/UE-Explorer>), decompress the cooked packages into
`_work\unpacked\`, then run `_tool\export.ps1`.
