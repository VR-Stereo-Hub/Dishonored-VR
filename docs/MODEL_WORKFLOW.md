# Models and animation: UModel + headless Blender

Game meshes, skeletons and textures are extracted with **UModel (UE Viewer)** and inspected,
tested and edited in **Blender running headless** (`blender -b --python`), driven by small
scripts that print a machine-readable result. Nobody needs the Blender window, though the
same `.blend` files open in it. First used for full-arm IK (`dishonored/ARM_IK.md`), where
it validated the production solver against the original skin weights over 260 frames.

**Everything extracted is game-derived and never committed**: PSK/PSA, textures, `.blend`
files, renders, reports. They live in the local **model workspace**. The scripts that
produce them are ours and are committed (`tools/blender/`, `tools/model-export.ps1`,
`tools/blender-run.ps1`). Tool locations come from the local tool file
(`tools\tool-paths.ps1`, CLAUDE.md "Tools").

## 1. Setup

```powershell
.\tools\tool-paths.ps1 -Init        # blender, umodel, psk_addon_zip, model_workspace, cooked_dir
.\tools\blender-run.ps1 -Setup      # installs/verifies the PSK/PSA add-on, prints the Blender version
```

- **Blender** 4.2+ (5.2 verified). **UModel** build 1590 (2022, the last public release),
  <https://www.gildor.org/en/projects/umodel>. **io_scene_psk_psa** 9.1.x, the PSK/PSA
  importer/exporter, <https://extensions.blender.org/add-ons/io-scene-psk-psa/>.
- `-Setup` checks the add-on's operators with `get_rna_type()`. **`hasattr(bpy.ops.psk,
  "import_file")` is True for any name** and is not a check: the first setup reported the
  add-on installed from exactly that probe while it was missing from the user profile.
- Never pass `--factory-startup`: it disables the add-on.

## 2. The model workspace

`model_workspace` in the tool file (default `Documents\DishonoredVR-Models`):

| folder | holds |
|---|---|
| `originals\<Package>\<Class>\` | untouched UModel output. Never edited |
| `working\` | editable `.blend` projects |
| `exports\` | anything written back out (relative `--out` of `export_model.py`) |
| `verification\` | reports, test scenes, renders (relative `--out` of `inspect_model.py`) |
| `index\` | cached per-package inventories for `model-export.ps1 -Find` |

## 3. Commands

```powershell
.\tools\model-export.ps1 -Find "Skm_Player|Ply_.*_as"                  # which package holds an object
.\tools\model-export.ps1 -List Startup -Grep "SkeletalMesh |AnimSet "   # what one package holds
.\tools\model-export.ps1 -Package Engine -Object Skm_Player            # PSK + textures into originals\
.\tools\blender-run.ps1 tools\blender\inspect_model.py -- --psk <x.psk> [--psa <y.psa>] [--bones] `
      [--out r.json] [--save s.blend] [--render p.png]
.\tools\blender-run.ps1 tools\blender\export_model.py -Blend <w.blend> -- --object <Name> --out <f.psk|.psa|.obj|.glb|.fbx>
```

`inspect_model.py` reports counts, materials, UVs, the skeleton, skin-weight health (no
unweighted vertex, sums to 1), the bounding box and PSA sequences, and prints every problem
as a `DVR_INSPECT problem:` line. `export_model.py` writes one object; for `.psa` it drives
the add-on's own builder directly, because `bpy.ops.psa.export` only works through its UI
`invoke` and fails headless with "No armatures".

Your own one-off Blender script runs the same way: `.\tools\blender-run.ps1 <script.py>
[-Blend <file>] -- <args>`; it sees `DVR_MODEL_WS` in its environment and exits non-zero
on an uncaught exception.

## 4. What has been verified (2026-10-04, Blender 5.2.2, add-on 9.1.3, UModel 1590)

- `Engine.Skm_Player` (the first-person arms): 2,264 welded points, 4,448 triangles,
  79 bones, 48 skinned groups, every vertex weighted. PSK export and re-import reproduce
  the same counts; OBJ and GLB export succeed.
- **Animation authoring**: a keyed action on the arms skeleton exports to PSA and
  re-imports onto the same skeleton as one 30-frame sequence.
- **Extracting the game's own animations does NOT work.** Every player AnimSet in
  `Startup.upk` (`Ply_Generic_as`, `Ply_Guns_as`, ...) loads with all tracks removed
  ("wrong CompressedTrackOffsets size") and exports nothing. Dishonored compresses
  animation with Sony's Edge Animation library on every platform, which UModel does not
  and will not decode (UModel's own forum thread on Dishonored,
  <https://www.gildor.org/smf/index.php?topic=1593.0>). Routes that remain: author new
  animation in Blender against the extracted skeleton, or record the live skin palette
  from the running game (the mod already reads it for the hands and IK) and bake that.

## 5. Getting a model back into the game

There is no repacking of cooked packages. Two routes exist:

- **The mod's own model slots.** The mod loads `<data dir>\vrhands\<name>.obj` (+ `.mtl`
  per-material `Kd` colours, + a skin texture) for `hand`, `sword`, `crossbow`, `pistol`,
  `grenade`, `razor`, `heart` (`src/core/gfx/hand_mesh.cpp`). `export_model.py --out
  <name>.obj` writes that format. Check the log line `vrhands: loaded <name>.obj (<n>
  triangles, scale <s>)` after a user-launched run.
- **Runtime skinning against the game's skeleton** (how full-arm IK works): the mod
  replaces palette matrices of the game's own mesh. Offline data it needs goes through
  `tools/prepare-arm-rig.py`.

## 6. Traps

- **UModel's PSK reflects Y** relative to the engine's mesh space. Anything compared with
  runtime data must undo it (ENGINE_NOTES, ARM_IK.md); the first IK candidate shipped the
  unconverted frame and the runtime refused it.
- **Imported bone tails are display length (1 unit), not limb length.** Use head-to-head
  distances.
- **Welded points are not draw vertices.** The engine splits vertices at UV/normal seams
  (2,771 draw vertices for the 2,264 points above).
- A Blender render is not proof of the live shader path, stereo or the game's animation
  routing. It checks geometry and skinning math only.
- If a game or asset is not covered by the tools here, look for an established extractor
  online (UModel for UE1-3, FModel for UE4/5, game-specific tools listed on the UModel
  forum), record what it is and its licence, and **ask before downloading** anything.
