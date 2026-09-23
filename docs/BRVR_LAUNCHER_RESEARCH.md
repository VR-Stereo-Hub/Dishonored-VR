# BRVR launcher and controller research

Reviewed 2026-09-23 against local BioShock Remastered VR commit 9768974 and the
installed Build/Final/openvr_input JSON files. Source/fixture findings below do
not establish headset compatibility. No BRVR files were edited.

## Installation and UX

Sources: dist/Setup.bat, dist/Uninstall.bat, dist/CollectLogs.bat,
BioshockVR/Core/Keybinds.h, OpenXRShim/src/shim_input.cpp and docs/modules/shim.md.

BRVR discovers Steam and Epic layouts (including FinalEpic), then offers a manual
folder fallback. It probes write access, identifies actual config paths, reads
existing resolution/FOV/controller choices and preserves unrelated keys. It
verifies loader selection after copying and reports failure instead of treating
a completed command as a successful install. Backups and unexpectedly-shrunken-ini
guards protect existing settings.

Dishonored retains its Steam-library discovery, known-folder resolution, scoped
game-ini writes, atomic payload replacement, installed DLL hash and foreign proxy
backup/restore. Copying BRVR's store heuristics would imply unsupported executable
compatibility. The existing ownership-based uninstall is safer than deleting every
filename that might belong to another graphics wrapper.

BRVR separates headset choice from controller layout and gives help based on the
resolved runtime and modifier. Beyond with Index controllers differs from Beyond
with Vive wands. WMR is not a single universal button layout. Dishonored now exposes
the actual modifier, D-pad stick and pause-chord values, and shows current shortcuts
above its default Quest diagram. It does not silently guess hardware presets.

## Logging

BRVR uses CIM instead of wmic for OS, CPU, RAM and GPU/driver facts; missing facts
are reported as unavailable. The collector asks when/what happened, inventories
evidence after copying, checks LocalAppData and VirtualStore, avoids stale gathered
files, handles redirected desktops and preserves useful ZIP error messages.

Dishonored already makes a unique staging folder, records timestamps/hashes,
includes current/previous game logs and watchdog evidence, and requires an explicit
choice for memory dumps. This continuation adds launcher logs, old setup-log names,
the install record, game settings, VirtualStore shadows, generated SteamVR JSON and
CIM/runtime details. A concrete gap: ovrshim.log normally lives in LocalAppData
regardless of configured DataDir. The old collector could miss it; it now checks
both, keeping distinct evidence names. Collection stays local.

Useful follow-ups are an optional short problem description and input-state
validation. Do not bundle unrelated Steam account data or entire config directories.

## Installed JSON audit

All four installed BRVR controller binding files are semantically identical to
the current embedded JSON constants. actions.json declares /actions/gamepad with
21 actions, including two vibration outputs. The shim regenerates these files
every launch; source strings are the durable defaults, while SteamVR per-user
binding overrides are the supported persistent customization route.

| File | BRVR mapping | Dishonored difference |
|---|---|---|
| bindings_knuckles.json | Joysticks/clicks, four face buttons, left trackpad menu, trackpad touch modifiers, grip force_sensor/force, haptics | Grip uses trigger/pull; no touch modifier outputs or haptic block |
| bindings_oculus_touch.json | Face/menu buttons, analog triggers/grips, both thumbrests, aim/grip poses and haptics | Missing outputs need comparison/port using Dishonored's action names |
| bindings_vive_controller.json | Trackpads/clicks, triggers, menu/jump buttons, poses and haptics; intentionally minimal, no grips | Dishonored adds grips but still lacks some face actions |
| bindings_holographic_controller.json | Joysticks, digital grips, trackpad quadrants for A/B/X/Y, touch modifiers, menu, poses, haptics; provisional | Only A/X trackpad clicks are mapped, leaving B/Y missing |

Direct copying is wrong: BRVR uses /actions/gamepad and turn, trigger_l/r,
thumb_l/r, rest_l/r, gpose_l/r; Dishonored uses /actions/gameplay and look,
plasmid/fire, stick_l/r, thumbrest_l/r, pose_l/r. Output names and types must match
the action manifest. Native xrSuggestInteractionProfileBindings does not fix shim
JSON: the shim acknowledges suggestions but authors its own SteamVR bindings.

BRVR's Index force mapping is intentional. Capacitive grip position can remain
high merely from holding the controller; squeeze force represents an intentional
squeeze. The source rejects inventing force_sensor/click. A port needs rest,
squeeze and release measurements with Dishonored's block/choke and wheel behavior.
The WMR quadrant mapping is also explicitly provisional, not headset-tested.

The wider shim contract matters: new actions require JSON updates; new statically
imported OpenXR functions must exist in the shim. Optional APIs should resolve
through xrGetInstanceProcAddr. BRVR records a startup failure from an absent export.
Haptics require action creation, binding output, shim entry point and SteamVR call,
with bounded pulse duration and useful first-result logging. No projection,
geometry or grip-threshold constants were copied during launcher work.

## Keybinding architecture and disposition

BRVR's centralized Keybinds.h names each action, permits NONE, logs resolved
mappings and warns about duplicate hotkeys. This avoids numpad-only controls on
compact keyboards and catches one key triggering multiple features. A future
Dishonored binding editor should share one action registry instead of introducing
a second mapping table.

Implemented here: up-front preferences, existing controller shortcut editing,
current-setting guidance, embedded zoomable Bindings image, Steam launch, local
log collection, per-user shortcuts and profile-preservation checks.

Separate follow-up: port missing shim outputs with renamed actions, verify their
types and schema, then test Index and other actual hardware one question per run.
The launcher's headset descriptions do not establish broad hardware validation.
