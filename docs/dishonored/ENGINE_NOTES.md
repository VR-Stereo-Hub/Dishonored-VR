## DisPostProcessManager UI fade timer goes NaN (VR-140, 2026-09-18)

Layout, reflected by name on build 473 (`pp/watch: armed`): `m_RequiredEffects[21]`
+0x168 (int per eEffectPp), `m_EffectStates[21]` +0x1bc (byte: 0 stopped,
1 warming, 2 running, 3 cooling, 4 aborting), `m_UIStateDuration` +0x284,
`m_UIPPFadeOutTime` +0x288, `m_UIPPFadeInTime` +0x28c, `m_UIPPWeight` +0x290
(0.2 s in and out for the wheel). Both arrays are sized Epp_Count (21), not
Epp_MAX. Effect 19 is Epp_UberUI (the menu blur); effect 3 (Dark Vision) reads
state 2 with no request all session, which is normal.

Measured (run473): a close that lands while UberUI is WARMING can leave
`m_UIStateDuration` NaN. The effect then sits in Cooling forever and the scene
post-processes to black; HUD, markers and Dark Vision silhouettes draw after it
and survive. Camera fade (`FadeAmount`) and `ColorScale` are not involved. The
game's own code path that produces the NaN was not located: `disp 0x284` has
~40 hits in the image, and none was traced. The mod repairs the value only when
it is already non-finite (`pp/repair`).

## Possession ownership and the camera rain box (VR-135/VR-136, 2026-09-18)

**Possession.** While possessing, `Controller.Pawn` is a `DisPossessablePawn`
subclass: `DisPossessionProxyPawn` (rats, fish; measured in the build452 log,
`crouch/pawn: now ... (DisPossessionProxyPawn) via ctrl+0x248`) or
`DishonoredNPCPawn` (people); `DisDLC06NPCPawn` and `DisTallboyNPCPawn` extend
the latter. The script declarations give two back-pointers on the possessable,
`m_pPossessingController` (DishonoredPlayerController) and
`m_pPossessingPlayerPawn` (DishonoredPlayerPawn); both are resolved by name
through `RflOffsetOf("DisPossessablePawn", ...)`, never by a copied number, and
the resolved offsets print on the `possession/stereo: layout` line. Validation
= pawn class in that list AND pawn live AND back-pointer == our live controller
AND the player pawn live with a PlayerPawn class. The controller also carries
`m_PossessionEffectSettings.m_Stage` (EDisPossessionEffectStage Off/Intro/While/
Warning/Outro); it is logged as evidence, not gated, until a run shows its
timing against the pawn switch. The power component's own
`DishonoredActivePowerComponent_Possess.m_PossessionStage` (PossessionActive=3)
was not needed: the pawn back-pointers are set by the engine only while a
possession is in force. The capsule reader and the anim FSM reader both refuse
this pawn on purpose (their offsets belong to DishonoredPlayerPawn) and still do.

**Rain.** `DishonoredPlayerCamera` owns the rain: `m_pRainBoxEmitter` (Emitter,
observed with 40 drops), `m_RainBoxExtent`, `m_NumRainDrops`,
`m_fRainSpawnKillRate`, `m_RainDirection` (default 0,0,-1), and separate impact
fields. The drop module is `DisParticleModuleRainDrops` (parameters
`MaxParticles`, `SpawnKillRate`, its own `m_Extent`); `Dis_SetRainEmitter` (a
Kismet action) sets drops/impact distances/start delay per level. The candidate
hides ONLY that emitter's `ParticleSystemComponent` through the native
`PrimitiveComponent.SetHidden` (found by name AND declaring class: SetHidden also
exists on Actor with a different parameter block), called through ProcessEvent on
the script lane with the re-entry flag, as `console.cpp` and `mat_hide.cpp` do.
A raw `HiddenGame` write would not reach the render proxy. The `rain/box` line
logs the box extent and the emitter's location in the camera frame; the near-eye
design waits on those numbers. Unverified in game as of this entry.

Build458 run (08:33): `m_RainBoxExtent` = 500/500/500 uu, 40 drops,
`m_RainDirection` 0,0,-1, `m_fRainSpawnKillRate` 0 in the first rainy area and
about 10000 in a later one.

**Where the rain is drawn (derived 2026-09-18, RETRACTS the first reading).**
The emitter is re-placed every camera update by the native at VA 0x6D8951
(`cmp [cam+0x4dc],0`, gated by `[cam+0x4c8]==1`): the view forward (Rotator
-> Vector, `call 0x40da70`) is taken into the emitter's frame (vtable +0x1BC),
`t = min over axes of m_RainBoxExtent[i] / |f[i]|` (the `fdivr [edi+0x4e0/4/8]`
chain, a zero component takes a constant), and the emitter moves to
`POV.Location + forward * t` (`call 0x6539C0` on GWorld). The only other read
of the extent in that function draws the debug box (`[cam+0x4c0]&8` =
`m_bDebugDrawRainBox`, `call 0x6476F0`). The same function traces from the
camera along `m_RainDirection` for shelter (`call 0x64E7A0`, flags 0x2086) and
fades `m_fRainSpawnKillRate` (0x520) on the result. Measured: 143 steady
samples put the emitter at fwd 499..662 uu, right |<20|, up ~-6 (one -153) in
the camera frame: a slab of rain about 5 m ahead that turns with the view.
That is the pane. The first `rain/box` line of each level (74..118 m away,
e.g. fwd -5080) is the emitter's spawn position before its first update, NOT
where the drops draw; the earlier note saying the actor Location does not
describe the drops was wrong and is retracted. So the extent IS the slab's
distance: `[Rain] Distance` writes it (0 centres the emitter on the camera).
The drop module's own spawn volume (`DisParticleModuleRainDrops.m_Extent`,
per template) is not read by the mod.

**The rain PANE is a lens effect (run470, 2026-09-18).** The tester's pane moved
with F10 "Lens effects distance", not "Rain distance", and the rain-box hide did
not remove it. The live lens effect in the rain was `DisEmitterCameraLensEffect_Looping`
(`lens/fx` at 4917406: DistFromCamera 90, BaseFOV 80, DrawScale 1.0, measured
fwd 72.0 uu - so this build DOES scale the distance by FOV, 90 -> ~72 at the
103-degree view). The camera rain box (m_RainBoxExtent) is a separate, real
effect; its lever works but is not the pane. Wanted next: scale the looping lens
rain down (DrawScale with KeepSize off) and a per-class lens hide.

**Lens effects (VR-137).** `DishonoredPlayerPawn.m_pCurHealthLensEffect` is an
`EmitterCameraLensEffectBase` (the red low-health vignette). All lens effects
live in `Camera.CameraLensEffects` and are placed by the native
`UpdateLocation(CamLoc, CamRot, CamFOVDeg)` (exec thunk VA 0x5BB5D0, which
calls the implementation through vtable +0x3AC; `ue3-natives class` resolved
that slot to garbage, so the implementation was not disassembled).
Declarations: `DistFromCamera` default 90 uu, `BaseFOV` 80. Whether this build
scales the distance by FOV is what the `lens/fx` line's measured `fwd`
answers. `[Lens] Distance` writes `DistFromCamera`; `[Lens] KeepSize` rescales
through native `Actor.SetDrawScale` by the same ratio. `m_Stage` of
`DisPossessionEffectSettings` is at +0 (it now resolves through
`FindPropOffsetChecked`, which accepts a zero offset).

**Possessable classes.** From the shipped scripts, `DisPossessablePawn`
subclasses: DisPossessionProxyPawn, DishonoredNPCPawn, DisTallboyNPCPawn,
DisDLC06NPCPawn, DisDLC06AssassinNPCPawn, DisDLC06ButcherNPCPawn,
DisDLC06SummonedAssassinNPCPawn, DisDLC07NPCPawn, DisDLC07AssassinNPCPawn,
DisDLC07GravehoundNPCPawn, DisDLC07SummonedAssassinNPCPawn,
DisDLC07TentacleNPCPawn. `DisPossessableInterface` is also implemented by
DisFish, DisGameCrowdAgentSkeletalRat and DisRiverKrust, which are not pawns:
possessing one puts the controller in a DisPossessionProxyPawn (measured for
rats). The validator walks the class chain through `kSuperFieldOff` (+0x44)
once that walk reproduces the player pawn's Pawn/Actor ancestry, and falls
back to the list above.

## Build452 full-dump texture accounting (2026-09-18)

Exact DLL SHA256537812eb74f594ba3b700284f782132af14620de31509dbd0d3b55098cd091b4.
PDB symbols locate dvr::d3d9ex g_map/g_mapCount and lifecycle counters.
Map traversal16-byte Ent entries, excluding null/tomb, matches2596 live;
made10200 minus released7604 exactly reconciles. Failed0. This snapshot cannot
prove absence of lifetime leaks; it does disprove treating cumulative shadowBytes
26451.47MiB as live allocation.2580 2D textures plus16 cubes remain.

For this exact Windows D3D9 binary, GetLevelDesc code at RVA0x65D50 bounds
levels using byte[this-8], dispatches via surface array[this+4].
Surface getter0x628A0 ->0x62CC0 ->0x62C60 derives dimensions from owning texture
header. Format[this+8], width[this+32], height[this+36], pool[this+20] are
consistent with these descriptors; pool2 is SYSTEMMEM. These are offline driver
layout observations, never runtime writer offsets. Formats:2090 DXT1,312 DXT5,
158 L8,10 V8U8,10 A8R8G8B8. Sum format-correct mip payloads:
DXT1737.36MiB, all2D1754.64MiB. Excludes16 cube textures, resource headers,
row alignment, GPU resources and other driver allocations. It is a payload
estimate, not a measured VirtualAlloc ownership sum. Largest dimension4096:
86 twins638.67MiB;2048:351 twins752.46MiB. Combined1391.13MiB.

Reflected Texture2D SizeX260/SizeY264, resident316/requested312 and bIsStreamable
280 mask1, Texture.LODGroup146 recovered from dump UProperty objects via existing
UProperty offset0x5c and UBool mask0x6c. Initial use of0x60 for bool mask was
invalid and corrected before drawing conclusions. Of2019 Texture2D objects,
1522 flagged streamable;428 of429 with dimensions at least2048 flagged streamable.
Sample4096 textures have13 resident/requested mips. Streamable eligibility
does not prove the group policy permits eviction.

Active user DishonoredEngine.ini and installed DefaultEngine.ini SystemSettings
both set NumStreamedMips=0 for world/normal/specular, character/normal/specular,
weapon/normal/specular, vehicle/normal/specular and cinematic. No attribution
to texture pack installer is established. Dishonored parser references its
NumStreamedMips= key at RVA0x1771A8, parses integer and stores group+0x10.
Epic's current texture settings documentation describes0 as fully resident,
-1 as all mips eligible; UE3 archived URL was inaccessible. Current documentation
is supporting context, not proof of this fork's final streaming behavior:
https://dev.epicgames.com/documentation/unreal-engine/texture-format-support-and-settings-in-unreal-engine

Targeted test changes only those13 fields per file to-1, with complete backups,
full diffs and CRLF validation under build/playtest-candidates/texture-streaming452.
No DLL change, no pack removal, no maximum resolution/pool/F10 changes. User
authorized closing game; process absent verified before edit. First attempted
installer refused on the no-process shell exit code before any writes; corrected
guard then applied and verified both files. Memory watcher uses signed x86 dumper.
Restored streaming is a mitigation candidate, not yet headset/memory-confirmed.

Alternative: reducing4096 textures to2048 would reduce that class's shadow payload
by roughlythree quarters, but is not this test. Dropping shadows blindly is unsafe:
native READONLY mip-copy locks require retained data, and DEFAULT textures cannot
serve those locks. A future pageable/reconstructible shadow backing would need
explicit correctness and streaming tests. Do not claim the lifecycle counters
justify eviction. Saved full dump remains available to investigate other owners.

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

Offline dump derivation: fatal text referenced by executable call at
RVA0x9598E points to VA0x140A950. It records the engine memory fatal and allocation
call chain through0x9DE2ED/0x9DE4AA/0x9DD151/0x9DD83F. Critical section instance
0x02F318AC has owner25820 and recursion1 in this dump. Native wrapper at
RVA0x5DD760 enters the allocation lock before dispatch; waiting threads return
through0x9DD7A2. These are read-only dump observations, not new hook addresses
or permissions to write engine memory.

## Released wheel camera ownership candidate (2026-09-17)

Latest on-disk run is448 (log ends22:29:16), not installed450. Installed450
DLL SHA256 matches its manifest; no effects/owners telemetry has run yet.
Do not label this as450 playtest evidence. Existing448 log shows530 Walk samples
with Wheel ownership and zero head rotation writes; tracking remains valid.
By contrast2232 Walk/Other samples have writes. At53567609, Walk/UpperIdle,
script menu0, UI-derived menu1, cinematic0, head valid, writes0, script age1091ms.
This establishes stale menu ownership, not proof of the exact reported hit frame.

Fix candidate extends released-wheel handling to the shared UI owner. A focused
VR controller with released grip, no script menu and no cinematic invalidates
the native wheel-open bit after250ms. Remaining menus are still scanned. The
existing wheel visual closing lease remains independent. No new engine writes
or hit-reaction suppression. Native head injection resumes via its existing
absolute pitch and menu-exit yaw path.210 controller policy checks pass.

Tester clarified effects should be retained near the real view boundary.
Installed Element.vignette was window, scale1.620. Candidate routes that
full-screen category to frame (native scene image, not a small window quad).
This broad category also includes full-screen fades; it is not a verified
health-only material ID. World anchor would retain window dimensions, so it
would not address the small border. Rain is preserved unchanged: its native
particle owner must be measured before adjusting distance/extent.
Read-only hit/health/rain diagnostics remain enabled for the candidate.

One launch question: after opening/releasing the wheel and taking hits, does
head pitch remain correct without pausing? Success supports stale UI ownership;
failure with resumed head writes points back to the native hit reaction.
Headset result pending. No game launched.

## Hit-camera, health lens and rain ownership research (2026-09-17)

Build448 stick recovery reported accepted. DLL/banner verified; logs/latestINI
archived under wheel-release/hit-tilt448. New report: hit leaves upward view bias
until pause; low-health border distracting on frame; rain removal requested.
Decompiled declarations identify DishonoredCamera_HitReact (PhysicalReact spring
base), camera.m_pHitReact_Influence, pawn.m_pCurHealthLensEffect and
m_HealthEffects (post-process plus lens emitter), DisTweaks_EmitterCameraLensEffect,
DisSeqAct_SetRainEmitter and camera.m_pRainBoxEmitter/m_NumRainDrops.
Rain drops/impacts have separate particle modules. Lens base sets foreground depth
priority and exposes BaseFOV/DistFromCamera. This is not proof the symptom is a
Scaleform HUD element; moving the generic vignette row could target the wrong draw.

No gameplay hit-react disable: the cheat also affects native strong reactions.
No guessed shader/geometry suppression. Candidate adds bounded read-only logging
of reflected live hit weight/target, health emitter/index and rain emitter/drop count
to the existing Cine Trace. No camera writer or rendering change.
Next launch question: after taking a hit, does the upward bias persist until pause?
Expected evidence is effects/owners plus synchronized existing camera trace; this
is diagnostic, not a claimed fix. Low health/rain target presence can be read from
the same log if present, without another test request.

ProcDump captured a1.23GB full dump on normal exit (code0), not threshold/crash.
One-shot watcher completed; no longer armed. Keep as baseline, not crash evidence.

## Build443 reported freeze repeats allocation failure (2026-09-17)

After longer play, tester reported a freeze instead of the prior crash dialog.
Process was already absent when inspected; no live hang dump was possible.
Installed443 DLL/banner verified, both logs/latest profile preserved under
build/playtest-candidates/wheel-options/freeze443.

Final log at51895218: SYSTEMMEM1024x1024 DXT5 single-level shadow creation fails
8007000e. VirtualFree67.1MiB, largestFree0.9MiB, committed3563.1MiB,
liveTwins3003; physicalAvailable14933.9MiB. Same allocation failure class as
the previous crash, now with a smaller requested texture and smaller free block.
Reported freeze cannot be independently classified as a deadlock without stacks.
Do not treat it as evidence of a new wheel/camera defect. No prevention fix yet.
External full-dump permission remains pending; watcher not armed. No game launched
or settings changed. Priority is capture before exhaustion and identify memory
owners/lifetimes. Full-memory dumps may contain private process data.

## Build443 texture allocation crash confirmed (2026-09-17)

Wheel options reported working before a rendering-thread Texture LockRect
D3DERR_INVALIDCALL crash. Installed443 DLL hash and log banner verified. Both
logs, latest INI and any existing crash text preserved under
build/playtest-candidates/wheel-options/crash443. Crash text may predate this run;
only verified443 log lines are attributed here.

Final log at50621312 records HRESULT8007000e creating a1920x2048 single-level
SYSTEMMEM shadow texture, format894720068 (DXT5). VirtualFree63.6MiB,
largestFree2.1MiB, committed3572.8MiB, liveTwins2869. System available commit
6974.2MiB and physical available15603.1MiB: evidence points to process address
space exhaustion/fragmentation, not system RAM exhaustion. Failure follows pause
menu entry. shadow_register_texture returns without a twin on allocation failure;
the DEFAULT texture remains, so its later lock cannot use the required SYSTEMMEM
redirect. This matches the screenshot failure and earlier427 allocation signature.

Immediate failure chain established; dominant memory owner, possible leak versus
asset load, and texture-pack contribution remain unmeasured. Do not attribute it
to head rotation or call it fixed by reverting working wheel controls.
No new candidate or setting changes. Next engineering step is measure live shadow
bytes accurately by format/mips and address-space use across load/menu boundaries,
then choose a bounded reduction or allocation-path fix. Do not report failed
texture locks as success or discard shadows that READONLY locks may require.

## Build435 rejected: restore build433 pose policy (2026-09-17)

Tester reports unwanted crouch animation, native animation close to the face and
loss of normal control/view after jumping through a window. Installed435 DLL hash
and banner verified; both logs and latestINI archived in
build/playtest-candidates/animation-visible-hands/reported435. Run ends in normal
PreExit; this is not evidence of a crash.

Confirmed design error: native_pose_requested used cancellable_action as a native
pose trigger. Generic upper/left StatePlayerAction also covers movement transitions,
not only deliberate item interactions. Logs show repeated GAME ownership during
Jump/Falling/Walk with JumpIn/JumpLandSmall sequence history and native split-hands
reason, despite saved Jump/Falling/Walk arms being0. Sequence history is supporting
context, not authoritative playback identity. This broadens hand ownership beyond
the requested mantle fix. Exact close-face and view-disruption causes remain open.

Revert all435 production changes and their policy tests to exact433 source: original
arm-driven classifier, draw bypasses, mesh palette/depth path and F10 wording.
Keep433 action-cancellation controls, camera/keyhole fixes and latest saved settings.
No new camera compensation or guessed offset. The original limitation returns:
unchecking Show game arms also restores tracked hands, overriding native hand poses.
Do not describe that option as independently controlling geometry in this rollback.

Future work must explicitly distinguish pose choice from forearm geometry, preserve
ordinary movement ownership, and first validate one named mantle path. A generic
FSM StatePlayerAction match or cancellation eligibility is not a native-pose policy.
The435 test proved that helper-level policy checks cannot establish comfortable
native rendering or correct movement integration.435 is rejected, not accepted.

Next launch is recovery only: are normal crouching, jumping and looking around
restored, including the same window exit? Normal behavior supports435 as the
regression; a remaining fault requires tracing433 or persistent session state.
No new animation-visibility test in that launch.

## VR-134: arm visibility must not select pose ownership (2026-09-17)

Build433 mantle report confirmed in verified logs: Arms.0.StatePlayerMasterMantle=0
produces PLAYER while the master FSM remains Mantle and reports a mantle sequence.
Arms=1 produces GAME. Thus the visible animation was overridden by tracked hands;
this is not evidence that the native mantle action was cancelled. Both logs/latestINI
are preserved in animation-action-controls/reported433. Native block requests were
also logged as rejected, but no general cancellation acceptance is inferred.

Correction: native action pose ownership derives from voluntary action states and
existing scripted-action defaults independently of Arms.*. Visibility selects full
arms versus the existing clipped/rounded hands under the native animated palette.
Weapon native ownership remains intact. The split bypasses controller palette and
depth overrides for native hands. Normal unchecked walking remains controller-driven.
Whole-body action visibility takes priority over upper/left states; idle/walking
do not override a real upper-body action. Visibility is frozen with pose weight for
a render frame, preserving the stereo pair. No engine state or camera change.

103 catalog/policy checks plus22 handoff checks pass. New cases cover hidden mantle
retaining its pose, unchanged checked mantle, tracked walking and split/full geometry
routing. Release builds. Headset result pending; existing split qualification still
fails open if geometry is unavailable. Earlier433 documentation calling Arms.*
independent was incomplete: it was independent of action rejection, not native pose.

Next launch: with Enable action on and Show game arms off for Mantling, do the hands
and weapon still animate through the climb while forearms remain hidden? Normal
animation supports separation; tracked/static hands mean another pose override;
visible forearms mean the split route failed. Enable action stays on for this test.

## VR-134 native action request boundary (2026-09-17)

Offline derivation used the installed Steam executable and ue3-natives --verify,
which first reproduced the known crossbow context. Dis_Lean_Toggle native exec
at VA009EE740 dispatches controller vtable+056C; controller class derivation
gives vtable01118738 and implementation00AB4C00. That calls pawn lean handler
00AB1E50, which submits master FSM requests through00A74FA0 and checks EAX
before applying the lean state. This provides a real action-entry caller rather
than a guessed state write. Addresses introduced into production live in patterns.h.

RequestState VA00A74FA0 is thiscall with three stack arguments and ret0C.
Argument1 is a request object whose+4 field is the target UClass; argument2 is
entry context and argument3 is query-only. Entry bytes55 8B EC 6A FF are complete
instructions and are replayed on passthrough. The body reads target class before
calling00A74F20 eligibility, tests its result, and only then writes pending
class/state+88/+8C and calls native handlers. Failure returns0. Lean's caller
branches around further action work when that result is zero.

The new hook returns0 before this body only for a disabled catalog entry belonging
to the current live player's exact master/upper/left FSM. It validates current
controller/pawn/FSM/class membership, a fresh sampled state and current GObjects
membership, and allows an already-current state. No pending/current state fields,
transition maps or object vtables are modified. NPCs, recovery states and unknown
ownership pass through. The native request may have callers with earlier side
effects; gameplay suppression for each supported action is not yet headset-proven.

Misleading routes: class constructors00A7CA20/40 are thunks, not native requests;
their target constructors reveal vtables but do not expose a virtual RequestState.
A displacement-only scan generated unrelated candidates and was not evidence.
Do not confuse disasm-rva input RVA with ue3-natives output absolute VA.

Build431 keyhole result: installed DLL and log banner verified before archiving
both logs/latestINI in misc-special-camera/reported431. Explicit keyhole scopes
recorded successful restores and zero sampled refusals; several one-shot exit
yaw carries completed. Tester reports door/keyhole fixed. Lean and prior texture
allocation failure remain separate open validation.

## VR-129: build411 accepted except brief inner-icon transfer (2026-09-17)

Verified411 DLL/banner; both logs and latest full INI archived in
build/playtest-candidates/vr129-live-rune-ownership/reported411. Group routing,
startup behavior and camera state reported good; residual is a brief rune-icon
transfer to window while turning. Logs have44 sampled native-rune matches and
no sampled non-vitals native-miss, so the exact flash is not captured. A missed
position match immediately falls through to ordinary routing; callback/draw
phase mismatch is a hypothesis, not a measured cause. Wider regions and longer
native snapshot lifetimes were not applied.

Candidate: only an8-vertex/10-primitive small near-square draw already matched
to a live rune can retain ownership by its existing content key for at most two
render frames AND100ms. A fallback does not renew its own lease.32 bounded slots;
menu changes, resource/load reset, config reload and ownership toggle clear it.
No native pointers, old image, old location or pixel data retained. A fallback
uses the current draw center for scaling and preserves current game visibility.
Other content, text, unsupported topology and expired entries do not qualify.
This addresses transient routing loss, not a persistent missing identity. If the
reported icon has a changing content key or gap longer than the lease, it will
still miss. Diagnostic hud/rune-continuity counts fallbacks, independently of
native parent matches; neither proves a perceptual fix.107 native HUD checks
pass, including moving-frame fallback, nonrenewal, expiry, isolation and reset.

Launch question: does only the rune inner icon now stay native during the head
turn that caused its brief window flash? No flash supports the candidate; a
remaining flash means the timing/content hypothesis needs new draw evidence;
unrelated HUD retaining native ownership means an association regression.
Camera, intros, stereo synchronization and all INI values remain unchanged.

## VR-131: clean-install startup suppression owned by proxy (2026-09-17)

The previous successful launch relied on manual game-INI changes, not automatic
proxy behavior. New LaunchArgsInstall unconditionally seeds -nostartupmovies
before reading its optional launch file. Existing ANSI/Unicode GetCommandLine
IAT hooks deliver it before the engine starts movies. No prior mod INI is needed;
missing [Startup] SkipMovies defaults1. Explicit0 opts out on first lazy config
read. Resolution0 and missing Screen settings retain startup suppression. Disabled
proxy does not install the hook. No movie asset or game config is modified by it.

Offline executable derivation: UTF16 nostartupmovies string RVA0xBC9B50 referenced
at0x0E7065; code at0x0E7040 reads FullScreenMovie.bForceNoStartupMovies, parses
that command-line flag, ORs results at0x0E7078, and branches past startup playback
at0x0E707B to0x0E70F9. This is startup-only, not nomovies. No new memory hook or
address constant is required.9 extracted production command-line tests pass with
fresh absent INI, absent keys, zero/custom size, ANSI/Unicode and explicit optout.
This verifies the generated launch path offline; next real launch remains the
integration check. Prior manual game-INI overrides are restored tofalse with full
backups/diffs so they cannot mask the proxy result. No game/simulator launched.

## VR-129/VR-87: build409 report and live rune positions (2026-09-17)

Verified409 DLL/banner, both logs and current full INI archived in
build/playtest-candidates/vr129-rune-inner-artwork/reported409. Inner artwork
partly improved, but title/distance/locator transfer and cold-start full-window
routing remain. Edge learning and previous-frame icon proximity do not provide
startup ownership; child-only expansion was insufficient. Do not call409 fixed.

New default-off NativeRuneOwnership publishes numeric positions from the
verified rune-symbol native parent callback after the inset, independent of
edge observations.32 bounded entries,100ms expiry, explicit withdrawal on hidden
or invalid input, load/menu epoch clearing. A mutex transfers snapshots between
script/render lanes. Tokens are numeric matching keys only; no native or UObject
pointer is later dereferenced. Live owner validation remains in MarkerInputs.
Native center mapped through Scaleform's aspect-fit authoring canvas; recorded409
steady native761.02/402.74 on1280x720 at3012x3122 yields.59455/.53221, matching
recorded inner bbox.580/.518/.608/.545 and description.526/.488/.661/.526.

Flash bounds:40px art,48..62px pulse,64px locator; description centered31.65px
above,172x50 background,600px text box,21px font plus shadow. The matcher uses
bounded native-centered artwork/label regions with a12px phase/shadow allowance.
It routes matching draws and scales them about one current native parent, before
legacy routing. This is still spatial draw association, not a proven GFx draw
instance tag; overlapping HUD and fast movement remain perceptual risks. Menus
excluded; hidden/expired/ambiguous samples refuse. Old NativeMarkerChildren is
OFF in the next candidate; objective-only path is unchanged.

97 HUD checks include cold-start body/title routing, aspect mapping, hidden,
stale, moved and reset snapshots plus the recorded409 rectangles.7814 native
ABI/policy checks pass. Snapshot rectangles are no substitute for headset test.

Camera: log has a single F10 EyeClamp request=0, consistent with reported relief.
Saved INI lacks EyeClamp, so explicitly preserve0 on install. Remove the requested
F10 capsule-limit checkbox, retain the established INI mechanism for reversibility.
No neck tuning or camera-writer changes. Startup trigger remains unknown; result
supports the cap diagnosis but does not establish clearance-safe behavior.

## VR-129/VR-87: build407 results and child-artwork candidate (2026-09-17)

Verified407 DLL and log banner; both logs and full saved INI preserved at
build/playtest-candidates/vr129-native-rune-boundary/reported407. Objectives
reported correct. Rune description/distance/locator/outline correct; inner art
still captured onto window. Task final counters122286/84618 calls/moved, no
refusals; rune19090/8672, no refusals. Saved inset task22%, rune17% retained.

Local Flash sprite169 contains static shape165 at depth1 (40px square) and
pulse sprite168 at depth2, whose shape167 is48px square and animates to about
62px. A single square/topology match cannot own both. New default-off
NativeMarkerChildren associates centered child draws at60..90% of a recognized
marker rectangle, with0.002 normalized-coordinate center tolerance, current or
previous frame only; requires minimum1% extent and near-square bounds. Content
identity is retained in a bounded64-entry cache (2400-frame expiry), cleared on
resource/load reset and toggle, so movement need not relearn it. Menus excluded.
Child scaling uses the common center. This remains heuristic association, not
native GFx instance-to-draw identity; overlapping similar artwork can match.
Initial association may require the marker to be briefly steady.55 native host
checks pass including stale/remote/off-center/tiny rejection, cache and reset.

Pitch residual matches the existing VR-87 final ceiling limitation, not a new
stereo flicker. Standing episode16 at+24.8deg,198 samples/eye: residualU
-4.02/-4.61uu, capdelta -3.99/-4.57uu; at-25.5deg residual+6.87/+7.12uu.
Crouched episode1 +23deg359 samples: residual-4.35/-5.30uu, cap-4.27/-5.21uu.
Rendering remains near the capsule ceiling while raw head height changes.
Existing EyeClamp now exposed at F10 Comfort and saved; unchanged enabled
fallback. No camera-writer or neck tuning changes; disable comparison pending
separate launch. Removing the cap can expose above-capsule geometry: do not
claim clearance-safe fix from these measurements.

## VR-129: rune marker boundary and independent inset (2026-09-16)

Branch renamed by request to codex/objective-marker-fixes. Read DisGadget_Heart,
DisHeartTargetTracker, DisTweaks_Heart and the HUD/tweak declarations before edits.
Heart tracks collectible targets separately from task objectives; HUD declares
m_HeartMarkers and separate rune/bone-charm marker settings. Preserve equip/reveal,
nearest-target/refresh behavior, distance and combat opacity. Do not reuse task
visibility policy to make hidden Heart targets appear.

Flash runeMarker is sprite170: description160 at depth1, locator163 at depth4,
icon169 at depth6. Its description is above the icon, as for objectives, but the
locator is an additional owned child. Preview attachment is fake authoring code.
Native Heart constructor0xBCEBD0 installs vtable0x11635D8; update slot+0x14 points
to0xBC5D00. Its parent call0xBC5D75 targets the same base0xBBD430 and returns at
0xBC5D7A. Both update/base ret24 contracts verified. This update subsequently
handles locator offset, icon presentation and description/distance behavior.

Heart code is shared with bone charms: candidate further requires the live
borrowed settings symbol to equal runeMarker. Base constructor stores settings
pointer at marker+0x0C; base clip creation reads its leading FString data/count.
Validate count11 including terminator, bounded capacity and full readable UTF-16
buffer. Native pointers are never mistaken for UObject liveness. Current owner
IsLiveObject, current load/menu live-table refresh and native caller/vtable checks
are the same as the task trial. Other Heart symbols forward unchanged.

NativeRuneMarkers defaults0; installed trial1. RuneMarkerEdgeInset defaults0.12,
range5..30 percent, with independent controls in F10 HUD > Objectives. Only visible
offscreen flags are inset, using parent coordinates while forwarding the game's
remaining arguments unchanged. Whole icon/text/locator semantic D3D ownership is
still pending native-boundary visual evidence. Existing draw matcher recognizes
both candidate edge margins without claiming native instance-to-draw identity.

7814 production wrapper/policy and44 native HUD host checks pass. Rune cases include
independent margin/toggle, wrong caller, non-rune symbol, truncated/null symbol and
all five corrupted call bytes. Release build and offline hook derivation checks
required before install. No game/simulator launched, no headset acceptance yet.

## VR-129: native task-parent boundary candidate (2026-09-16)

Derived from the unique task constructor using _icon_mc/_description_mc, not a
shape guess. Constructor VA0xBCE490 installs vtable0x11635A8; slot+0x14 is
VA0xBC57F0. Its base placement call at0xBC5865 targets0xBBD430 (return0xBC586A).
Both task update and base placement return24 bytes: six stack arguments. Native
base uses first two floats as parent X/Y and sixth word for visibility; remaining
arguments are forwarded bit-for-bit. Task update separately computes arrow angle
when flag8 is set, updates icon scale/alpha, distance string and description
visibility. Hooking only the task-to-base call preserves those child calculations.
The five-byte call and nine-byte callee prolog are checked before installation.

Constructor derives marker+8 from first argument and marker+0x10 from the native
movie parameters. The task arrow-angle code converts parameter+0x14/+0x18 from
integers to center coordinates. Candidate requires a current IsLiveObject owner,
readable borrowed parameters and bounded dimensions before changing call arguments.
Live table refreshes on load/menu epoch and retries unknown owners at most once/s.
No native marker pointer or engine field write is retained. Native marker identity
is validated by the specific caller and expected task vtable; it is not a UObject.

NativeTaskMarkers (default0, installed trial1) enables a live offscreen parent
inset. TaskMarkerEdgeInset defaults0.12, range0.05..0.30, exposed as percentage in
F10 HUD > Objectives. Visible offscreen flag9 keeps its center-to-marker direction
while clamping to the chosen inset rectangle. On-screen positions, flags, distance
and opaque arguments remain native. Menu, session-loss, disabled and invalid-owner
paths forward unchanged. Native child rotation sees the original coordinates.

This is the first perceptual test of the native boundary, not completed semantic
D3D draw ownership. The old icon classifier recognizes both its previous5% edge
and the candidate inset. Text proximity remains and can still miss. A parent update
is not a synchronous draw scope: do not use a thread-local flag here to classify
later buffered Scaleform draws. Next: prove this boundary visibly moves the whole
native marker, then connect its live parent/child identity to render ownership.

7799 host checks cover the production wrapper forwarding, call fingerprint refusal,
ray-preserving inset across angles/aspects, native fallbacks and new edge learning.
44 native HUD checks pass. Ten offline executable checks verify call/callee, ret24,
constructor/vtable and parameter dimension operands. No game/simulator launch.
Installed401 DLL and banner verified before replacement; full saved INI hash is
9c429cb77e9bc730d696357b46ed7640f5d6a4f33b8a25f25084ae8fe91a68af, newer than the
historical install-time INI hash. Preserve these latest saved values.

## VR-129: objective script ownership review (2026-09-16)

Offline review of UnrealScript declarations, UI_HUD_SF ActionScript/XML and the
merged native HUD classifier. Installed401 and all saved settings are unchanged.
This is evidence about ownership and failure modes, not a headset-confirmed fix.

### Engine ownership and intended behavior

- DishonoredObjective owns tasks; DishonoredTask_Base owns an array of targets.
  A target carries an Actor, localized target-name references, optional status and
  a vanish preset. DisSeqAct_UpdateTaskTarget can replace a target by index. A
  fixed marker per objective, or a cached Actor surviving a load, is insufficient.
- Objective and task each have separate hidden/show-HUD-marker state. Preserve
  active/completed/failed task state, marker visibility and target replacement.
- DisGFxMoviePlayerHUD declares native m_TaskMarkers and m_SortedMarkers arrays,
  separately from awareness, grenade and heart marker arrays and popup queues.
  These are native Pointer arrays, not reflected UObject marker instances. Do not
  treat their elements as safe UObject identities or infer their private layout.
- DisTweaks_GFxMoviePlayerHUD exposes task/optional marker settings: symbol,
  bounds, world/screen Z offsets, focus permission, bounding-box clamp, scale and
  alpha variation. Task-specific settings include distance display, near-target
  vanish presets and combat opacity reduction. Class defaults name PC marker-area
  ratio0.90 and console0.85, but the HUD uses the asset instance
  Twk_InGameUI.Twk_GFxMoviePlayerHUD. Blank class-default symbols and zero values
  are not evidence of the runtime asset values or of disabled functionality.
- Native method bodies are absent from these UnrealScript exports. Property
  names establish candidate inputs, not the projection, distance-unit formula,
  focus algorithm or the native marker struct ABI.

### Flash structure and limits

- Primary/secondary objective symbols are sprite178/174. Both contain description
  sprite160 at depth1 and icon177/173 at depth4. The description contains a
  background shape and a centered text field with a drop shadow. Shared sprite160
  is also used by other marker types, so its identity alone is not objective-only.
- The parent clips' exported frame action only stops the timeline. The root
  fakeObjectiveMarker function attaches a sample secondary marker at fixed screen
  coordinates; it is authoring-preview code, not live world tracking.
- This export contains no marker-specific ActionScript class that implements
  live target projection/title/distance updates. Native HUD code remains the
  next boundary to inspect; do not mistake ObjectivesNotification/Window for
  the in-world target markers.

- Verified native-registration scan reproduced the known crossbow class and
  enumerated2554 entries. Case-insensitive Marker/Objective/Task filtering found
  journal toggles/list requests, objective actions/cheats and a task-value accessor,
  but no task-marker projection/update exec. This rules out a named script-native
  shortcut in that population, not the existence of internal native update code.

### Concrete mismatches in the mod

- hud_native_icon.h learns an icon family only after an8-vertex/10-primitive,
  near-square draw reaches the hard-coded5%/95% edge band. This resembles the PC
  class-default marker area but is not a runtime semantic identity. Interior-first,
  differently scaled or differently clamped markers need not qualify.
- MarkerLabels accepts short/wide draws only near current/prior-frame icon boxes.
  Description depth precedes icon depth in the Flash asset. This cannot guarantee
  first-frame association, association after fast movement, or ownership when
  multiple markers overlap. Native render batching still needs runtime tracing.
- hud_layout.cpp lets unrecognized components fall through to spatial/default
  routing. That is a concrete route to the window while a recognized sibling stays
  native. Existing native-miss logging does not prove which missed draw is a marker.
- NativeIconScope sizes only recognized draws. Labels share the marker pivot only
  when proximity succeeds; an isolated unlearned icon can bypass capture at scale1.
  This provides a plausible explanation for size changes and separated labels,
  not proof of the cause in any particular recorded headset frame.
- NativeObjectiveUpright rotates projected geometry around an unchanged screen
  center. It can compensate screen roll; it cannot supply world-space position,
  correct head-yaw projection, or turn a screen sprite into a world billboard.

### Implementation direction and acceptance boundary

Find the native task-marker update/display boundary, preserve the game's target,
visibility, label/distance and edge behavior, and identify the complete parent
before redirecting/scaling. Keep all children under one transform and ownership.
If a world billboard is desired, separately verify the target-to-world projection
and a world-up orientation; do not conflate this with screen-roll correction.
Any engine writer needs reflected properties where available, current IsLiveObject
for UObject owners/targets, and separately verified native pointer lifetimes.
No guessed marker layout or broad shape-threshold change is justified by this review.

Future validation must include primary/optional targets, two nearby markers,
interior-first appearance, edge clamping, rapid head turns, approach/vanish,
completion/target replacement and a level load. Test visibility in both eyes and
keep icon, title and distance under the same transform. No new launch requested.

# Engine notes - Dishonored (Dishonored.exe, Steam, patch 1.4)

## VR-129: wheel auxiliary panel positioning (2026-09-16)

Offline local UI_PowerWheel_SF ActionScript inspection: shortcuts_mc (sprite166)
uses90% scale and centers from safe coordMin.x plus half its width, coordMax.y
minus half width. potions_mc (sprite180) uses coordMax minus half its width/height;
its children animate from+150px on opening. ScreenPosition works from1280x720,
adds half of Stage excess per expanded dimension, then applies a0.85 controller
or0.90 PC safe-area ratio. These parts are bottom-corner anchored to the expanded
stage, not a fixed letterboxed authored image. Candidate crop envelopes therefore
use bottom UV plus a height proportional to image width. Runtime bounds are not
yet measured; expose adjustment and test visual completeness. Icon loading still
uses the engine external-interface path documented below. No new engine address.
Derived scripts/images remain ignored under build/hud-assets, never committed.


The reverse-engineering knowledge base. Everything below was established by the original
author (GingasVR) across proxy builds 30.0 to 38.92 and recorded in the code's comments; this
file distills those comments so an agent does not have to read 23k lines to find a number.
"Verified" means the shipped build depends on it. Nothing has been re-measured by the
continuation yet (the game is not installed on the dev PC as of 2026-09-02).

Rules: every number lives in `src/game/dishonored/patterns.h` and here, with how it was
derived; a hook byte-verifies its target and refuses on mismatch; never copy a number from
another game; findings go here in the same commit as the code that uses them.

## SOLVED: VR-30, the arms follow the head - FaceRotation is the seam (2026-09-05)

**Headset-judged fixed.** Head yaw leaves the arms and weapon alone; the right
stick turns view and body together; both at once works. `[Camera] ArmBodyFacing`.

### The operation

`UDishonoredPlayerPawn::FaceRotation` is what faces the body to the view. It is
**virtual at vtable slot 252 (offset +0x3F0)**, and on this build the pawn's
vtable resolves it to **`0x00AB0D40`** (`patterns.h` `kFaceRotation`).
`__thiscall`: `this` in ecx, stack `+0` Pitch, `+4` **Yaw**, `+8` Roll,
`+0xC` DeltaTime, `ret 0x10`.

The fix does **not** write the pawn's rotation. It hooks that function, replaces
the **Yaw it is asked for** with a separated body heading, and lets the engine's
own function run - so all of the engine's dependent bookkeeping still happens.
Pitch, roll and delta time pass through; every other actor passes through.

    view yaw  = body heading + head contribution
    head turn  -> view changes, body heading does not
    stick turn -> body heading and view change together

The head contribution is bookkeeping for **our own injection**: the integer yaw
delta `ApplyHeadToViewRotation` adds is taken once, in the fresh branch, so the
2 ms modifier re-stamp and the stereo second pass cannot advance it twice. The
stick is never re-integrated - it arrives inside the incoming view and survives
a subtraction untouched. `body target = outgoing view - head contribution`.

Measured on the winning run: **3365 replacements, 0 stale**, and 1014 calls on
other pawns passed through untouched.

### How the address was derived, and why static analysis alone could not

1. `armfollow/nfp:` resolved the `FaceRotation` UFunction at runtime and reported
   its exec thunk at **RVA 0x1DAF30** - the *same* thunk for `Pawn` and
   `DishonoredPlayerPawn`, so the override is not in the thunk.
2. The thunk ends `mov edx,[edi]; mov edx,[edx+0x3f0]; mov ecx,edi; call edx` -
   a virtual dispatch. That gives slot 252.
3. The live pawn's vtable slot 252 gives `0x00AB0D40`. It ends in `ret 0x10`
   (Rotator by value + float) and has **zero static E8/E9 callers**: only ever
   reached through the vtable.
4. Which is why the probe counted **0 ProcessEvent dispatches of FaceRotation
   across a whole gameplay run**. Gameplay reaches it native-to-native, so
   hooking its script exec wrapper would have caught nothing. The one script
   call site in the corpus is inside a debug cheat path, not gameplay.

**Static analysis alone cannot find this on this image.** RTTI for the UE3
classes is stripped, and the native name strings sit in a blob loaded into
GNames at runtime, so there is no name-to-function table, no referenced string
operand and no findable vtable. `xref` returns zero on `FaceRotation`,
`UpdateRotation` and `PlayerMove_Walking`, and a pointer-table search returns
zero. **One runtime resolution is required; everything after it is static.**
Do not repeat the static search.

### Identity: the bug that wasted two builds

The writer must use the **event-latched** controller and pawn (`g_peCtrl` /
`g_pePawn`), validated as a **possession pair** through `Controller.Pawn` by
reflection.

The camera-pointer scan (`FindPlayerController`) is not good enough, and the log
shows why: it latched `23CB3800` before gameplay, the event stream named the
real controller `16D86800` twelve seconds later, and the scan did not refresh
until **57 seconds after that, once gameplay had ended**. For the whole play
window every read came from the previous scene's dead object - constant, so the
body froze *and* the stick died from one cause. A readable object of the right
class is not enough when it belongs to the scene before this one. Discovery must
not depend on the fallback head-tracking path, which returns early while
scripted tracking is healthy.

### What was falsified on the way, with numbers

| Attempt | Result |
|---|---|
| Write `pawn.Rotation.Yaw` directly (`BodyYawLock`) | **4 of 186 writes survived** to the next dispatch (2.2%); 70% of the time the engine had already restamped the pawn to the view yaw. The target was *correct* - it held at -85.6 deg across a 35 deg view swing - and it made no difference. The engine re-derives the pawn heading from the controller every tick and no field assignment outruns that. **Removed.** |
| `bRemoveMeshRotation` on `LookAtControl_LeftHand`/`_RightHand` (`ArmStripMeshRot`) | Ran clean and changed nothing: both controls found, bit resolved by reflection at **+0xb8 mask 0x10**, both reading 0 before, **304,244 writes**, both live all run, arms still followed. Ships OFF, kept as the reproducible A/B. |
| `ArmCounterYaw`, `ArmFollowWeight`, `ArmLookAtStrength` | Pre-existing, unchanged, ship off. |

**Byproduct worth keeping**: the reflected offset and mask for
`bRemoveMeshRotation` **agree** with the `0x10` in `kSkcBools` that
`skelcontrol.cpp` had been assuming unverified.

### Two dead references found by reading, not by running

- `g_haveInjRef` is declared `false` and **nothing in the tree ever assigns it**.
  `BodyYawLock` evaluated `g_hmdYaw - (g_haveInjRef ? g_injRefYaw : g_hmdYaw)`
  inline every dispatch, so its head delta was **always zero** - the subtraction
  its comment, its ini text and STATUS all described never happened.
- `ArmCounterYaw` reads the same expression but **latches it once** behind
  `g_afCntHaveRef` and subtracts the saved value, so its delta is real. Earlier
  counter-yaw results **stand**; an earlier revision of this file wrongly said
  otherwise.

### The lesson worth carrying

Seven attempts and three more this session all failed the same way: each wrote a
value the engine recomputes every tick, downstream of the one input we must
modify for the player to look around. **Nobody had measured write survival.** The
instrument that settles it in one run - read the field back at the next write
and classify it ours / engine / third party - did not exist until this session
and answered the question immediately. Measure whether a write is honoured before
tuning what it writes.

## FALSIFIED: bRemoveMeshRotation on the hand controls (2026-09-05, VR-30 option A)

`[Camera] ArmStripMeshRot=1` forces `bRemoveMeshRotation` on
`LookAtControl_LeftHand` and `_RightHand` every dispatch. It ran clean and
changed nothing: both controls found (`obj[54361]`/`obj[54362]`,
`SkelControlSingleBone`), the bit resolved by reflection at **+0xb8 mask 0x10**,
both read 0 before, **304,244 writes**, both live for the whole run, arms still
follow head yaw. The lever ships OFF and stays as the reproducible A/B.

**Byproduct worth keeping**: the reflected offset and mask AGREE with the 0x10
in `skelcontrol.cpp`'s `kSkcBools` handling, which had been shipping unverified.

**Why it could never have worked, and it is already written down above.** Attempt
3's deciding test swept a gain on `gain * (head yaw since neutral)` and found no
gain that held the weapon still, concluding "no seam between the arms and the
camera downstream"; the root cause beside it records that Arkane draws the
first-person view model **in camera space**, with no world matrix for the arms
anywhere in the constant map.

So the arms are not being rotated by anything. They sit in the camera's frame by
construction. There is no rotation to strip, no weight to zero and no flag to
flip - which is why this lever, the body-yaw hold, the counter-yaw and the four
earlier attempts all did nothing. **Every lever that looks for a rotation to
cancel is a category error.** Only two things can work: place the hands
absolutely in world space (attempt 5 proved SkelControl world-space translation
is absolute), or hide the engine arms and draw our own (attempt 4, shipped for
weapons).

**Do not add another lever of this kind without a measured target.**

**Correction (same day), and it narrows this entry.** The paragraph above
over-claimed. The null result kills THIS lever, not every rotation-based route:
a transform can be combined or baked into a bone matrix before it is uploaded,
so "no separate world matrix in the constant map" does not prove the arms cannot
be rotated independently. It also sat in direct conflict with the yaw-ladder
section, which attributes arm yaw to the PAWN's rotation. That conflict is
unresolved and must be settled at the native update path, not by preferring
whichever explanation was written last.

**What is actually established** is narrower and more useful: the commanded body
target already separates head from stick (it holds at -85.6 deg across a 35 deg
view swing), and it does not survive - 4 of 186 writes, 2.2%, with the pawn back
near the view heading 70% of the time. So the open question is which operation
overwrites it and whether that happens before the mesh transforms are built.

**Static analysis cannot answer it on this image (checked 2026-09-05).** RTTI for
the UE3 classes is stripped, and the native name strings sit in a blob loaded
into GNames at runtime, so there is no name->function table, no referenced
string operand and no findable vtable. `xref` on all four candidate strings
returns zero. It needs one runtime resolution, which `armfollow/nfp:` does.

**Also unproven and load-bearing**: the ProcessEvent hook is a PRE-hook, so a
write placed "after the head injection" is still before ProcessViewRotation's own
body finishes. Our write may simply be too early. That ordering is a measurement,
not an assumption.

## Identity

- UE3 licensee build 9099, x86, D3D9, Scaleform. No ASLR: image base 0x400000, `.reloc` ends
  at +0x1206A0C (`kModEnd`), `.data` at +0xE69000 for 0x21B3BC bytes (`kDataStart/End`).
  Derived by static analysis of the exe (the original "UE3 introspection probe", stage 3).
- The mod is a `d3d9.dll` proxy: the game's static d3d9 import loads it before any engine
  code runs, which is why the XInput IAT hook can be installed in DllMain (the input system
  decides "gamepad or not" once, at startup).

## Fixed addresses and hooks

| Symbol | Value | What | How derived / verified |
|---|---|---|---|
| `kGObjHdr` | 0x1423630 | `TArray<UObject*>` GObjects {Data, Num, Max} | static analysis; walked by `IsLiveObject`/`BuildLiveSet` |
| `kGNamesData` / `kGNamesNum` | 0x1435674 / 0x1435678 | GNames table | static analysis; `NameFromIndex` |
| `kNameOff` / `kClassOff` / `kOuterOff` | 0x28 / 0x30 / 0x24 | UObject FName, Class, Outer | probe dumps |
| `kProcessEvent` | 0x00470640 | `UObject::ProcessEvent`, prologue `55 8B EC 6A FF` | the script dispatch spine; hooked with a hand-built 96-byte trampoline (`InstallProcessEventHook`) |
| `kBlkDirHook` | 0x00bf55a3 (back 0x00bf55a8, bytes `8b 08 89 4d b4`) | Blink: the engine's aim vector at its source | THE shipped Blink redirect: the engine traces, validates and draws along the controller |
| `kBlkAimHook` | 0x00bf595f (back 0x00bf5964) | Blink: the first `movss` of the aim | observe/redirect stage |
| `kBlkDstHook` | 0x00bf5e4f (back 0x00bf5e55) | Blink: destination write | |
| `kBlkTrcHook` | 0x00bf5d1a (back 0x00bf5d1f) | Blink: the trace | |
| `kXIGetSlot` / `kXISetSlot` | 0x00f946c4 / 0x00f946c0 | exe IAT slots for `XInputGetState` (ord 2) / `XInputSetState` (ord 3) | verified at install: the slots must still point at the real functions |
| `kCamHookAt` | 0x56dd36 (`5E 8B E5 5D C3`) | camera-matrix builder epilogue | LEGACY: the spin test proved the renderer does not use this matrix |
| D3D9 vtable slots | IDirect3D9: 16 CreateDevice, 8 GetAdapterDisplayMode, 6 GetAdapterModeCount, 7 EnumAdapterModes; IDirect3DDevice9: 17 Present, 16 Reset, 94 SetVertexShaderConstantF, 37 SetRenderTarget | COM layout | |
| user32 IAT hooks | GetSystemMetrics, SystemParametersInfoA/W, GetMonitorInfoA/W, SetWindowPos, MoveWindow, SetWindowPlacement, GetClientRect (exe AND the fork's own import table) | holding the 4032x2268 window against the window manager | the fork shrank the window from inside Reset (32.76) |

## UE3 layouts used

- Camera object: `kCamRight` 0x60 (basis Y row), `kCamLoc0/1/2` 0x80/0x90/0xC4 (matrix
  translation row, cached POV location, cached POV location 2); POV rotator candidates `kPovOffs` {0x330,
  0x350, 0x374}; FOV candidates `kFovCands` {0x53c, 0x540, 0x564, 0x254}; controller/camera
  rotator bases `kPcRotBase`/`kCamRotBase` {0x9c, 0xd0}. The FOV lever writes
  `kLevCtrl` {0x3ac FOVAngle, 0x3b0 Desired, 0x3b4 Default} on the controller and `kLevCam`
  {0x254, 0x348, 0x368, 0x38c, 0x53c, 0x540, 0x564} on the camera every dispatch.
- SkeletalMeshComponent: `kMeshTrans` 0x190, `kMeshRot` 0x19c, `kMeshScale` 0x1a8,
  `kMeshScl3D` 0x1ac; pawn -> first-person mesh at `g_fpMeshOff` 0x3dc (re-derived by
  reflection if wrong).
- SkelControl (AnimTree bone-override node): `kSkcName` 0x5c FName ControlName, `kSkcStr` 0x64
  ControlStrength, `kSkcScaleProp` 0xa0, NextControl 0xac, `kSkcBools` 0xb8 (bApply/bAdd/
  bRemove bits 0x01/0x02/0x04/0x08), `kSkcTrans` 0xbc FVector BoneTranslation, `kSkcTSpace`
  0xc8, `kSkcRSpace` 0xc9, `kSkcRot` 0xd4 FRotator BoneRotation. Resolved by reflection
  (`FindPropOffset`) and audited (`SkcOffsetAudit`).
- Reflection: `FindPropOffset(class, name)` walks the UProperty chain; `FindFunctionObj`,
  `FindNameIdx`, `AsUFunction`. `ProcessEvent` parms for `ProcessViewRotation` are sniffed
  by locating `DeltaTime` in the parm block (guessing `+4` crashed).

## From the author's handoff (HANDOFF-GINGASVR.md, their build 39.4)

Measured field offsets the table above lacks: PlayerController `+0x248` current Pawn;
PlayerCamera `+0x53c` is the **sensor** (the FOV actually rendered) and `+0xC4` the cached
POV origin; SkeletalMeshComponent `+0x60/0x70/0x80` world matrix rows, `+0x90` translation,
`+0xcc` bounds origin; `kUEPerRad` 10430.378 (65536 / 2pi). The addresses are the same
binary for everyone (Steam build): never chase them for a per-machine difference.

**Three head-motion paths are on at once** and most head-tracking bugs were in the gates
between them: (1) head-mouse (`[Tracking] Enabled`, synthetic mouse; silently needs the
game window foreground and the menu gate clear); (2) the native script write
(`[HeadTrack] Native`, `ApplyHeadToViewRotation`; `ProcessViewRotation` reaches
`ProcessEvent` ONLY through the camera modifier chain, never the PlayerController, and the
rotator is at `Parms+4` or `+8` depending on the declaring class, found by locating a
plausible `DeltaTime`; yaw is a delta, pitch is absolute); (3) the direct fallback
("viewinject") into the PlayerController when path 2 goes quiet, held off during cinematics,
for 15 s after a pawn latch and while script writes are fresh. Collapse to one path only
with the game in front of you.

The VR hands ARE the weapon view models (`pPlayerMesh`); `Skm_Player` is the body and
nothing drives it. `FpCollect` walks the object graph from the pawn (depth < 3, offsets
0x20..0x600, 24-candidate cap) and world props (`wash_rag`, `FeatherDuster`) get in and eat
slots. Calibration (`FpCalibrateTick`, 8 phases) is only valid while the player holds still;
records must be banked by asset name (their 39.0), not component pointer.

Resolution: `SpoofDesktopW/H` 4096x2304 is the lie to `GetSystemMetrics`/`GetMonitorInfo`,
`RenderWidth/Height` 4032x2268 what the game is told its client rect is, the real window
1600x900 for the spectator; `hkGetClientRect` returning the render size is load-bearing
(without it the game falls back as far as 800x600 and writes that to its own ini).
Windowed mode keeps the desktop cursor visible, so script events are the only menu signal.

Traps (their section 10): every `POOL_DEFAULT` D3D9 resource must be released in `hkReset`;
never log from a static initialiser (the fork stopped loading with no log at all); check
UObject liveness by GObjects index before writing (`CamAlive`); the log is overwritten on
launch (ours now rotates to `.prev.log`); inis are CRLF; **a direct exe launch crashes at
the menu, launch through Steam**; an explicit adapter needs `D3D_DRIVER_TYPE_UNKNOWN`;
diagnostics must pay out as discovered, not after a timer.

## The head-tracking seam (verified, shipped)

`ApplyHeadToViewRotation` inside the ProcessEvent hook writes HMD pitch/yaw(/roll) into the
`ProcessViewRotation` parms; `[HeadTrack] ChainStamp=1` stamps every camera-modifier dispatch
in the same tick because which modifier survives the chain is re-decided at every level load.
Watchdog: `g_scriptHeadOK`, with a fallback re-arm (`[HeadTrack]` keys, F9). Retired
generations still in the tree as legacy: mouse injection (swims, lags, no roll), the
camera-object matrix detour (renderer ignores it), the view-projection shear at c0
(`ShearVP`/`LeanVP`, kept for the AER path).

Positional tracking (`TrackHead`): body anchor EMA, physical crouch with a self-healing
standing reference, lean/peek with a safety clamp, roomscale with deadzone bleed and
auto-recenter, deep-crouch collision-cylinder shrink (`PawnCollisionHeight`, 87.5/65/33).

## The deep-crouch capsule write moves the PAWN (2026-09-04, headset, filmed)

38.16 shrinks the crouched collision cylinder (65 -> `DeepCrouchUU`, 45) so the player fits under
more. It is the crouch height rise, and it is now default OFF.

Filmed one line per frame across the transition, two bursts of identical shape:

```
cyl 45.0  pawnZ 3405.73     the crouch, cylinder 65 -> 45
cyl 45.0  pawnZ 3385.73     dropped exactly 20.00 uu, then holds
```

**Writing `CollisionHeight` under a grounded pawn moves the pawn's origin down by exactly the
shrink**, because the engine keeps the feet planted: the origin is the capsule centre, so a shorter
capsule means a lower centre. In the worst case the pawn is left AIRBORNE - one burst filmed it
rising 34 uu and then falling 128 uu back to the floor, which the tester independently described as
"almost like noclip, I could float around".

The camera then chases that displacement with a **rate-limited convergence** - ~4-8 uu per frame
ramping up after an uncrouch (filmed with the pawn provably stationary, `pawnZ 3444.93` unchanged
across the whole ramp), a slow decay on the way down. It cannot finish before the next crouch, so
the unconverged remainder is retained and the view climbs ~20 uu per cycle without bound: 2184 uu
(22 m) in one measured run, which is why the report was "spamming crouch puts me through the
ceiling".

An A-B-A block A/B agrees to the decimal: write ON **+20.35 uu of view per cycle** (blocks 1 and 3:
+19.42, +21.28), write off **-0.26** (six consecutive cycles flat within +-2.6), difference +20.61
over 18 measured cycles.

**Nothing in the game's camera is at fault, and no eye-height field is involved.** `Pawn.EyeHeight`
(+0x32c) and `Pawn.BaseEyeHeight` (+0x328) read 85.00 unchanged across 27 cycles while the camera
climbed. The chain is: our capsule write -> the pawn's origin moves 20 uu -> the camera's catch-up
never converges -> the residue accumulates.

**Falsified along the way**, recorded so nobody re-walks them: the physical crouch detector (off
under `[Mode] GamepadOnly=1`); the engine's uncrouch arithmetic (both eye fields flat); the 38.24
eye clamp (flipping it changed nothing, `-0.05 uu`); the game's own
`DishonoredCamera_BumpSmoother` camera influence (weight forced to 0 in the game's ini, climb
continued); and our own crouch eye-drop (its `50 < ch < 86` guard means a 45 cylinder switches the
drop OFF rather than deepening it). Five mechanisms named and tested by their levers, five wrong.
What found it was filming the pawn and the camera per frame instead of naming a sixth.

The feature cannot be made safe as written: resizing a grounded pawn's capsule from outside the
engine moves the pawn, and keeping the feet planted would mean writing `Actor.Location`, which this
mod deliberately never does.

## The per-eye camera seam: write points (2026-09-02, 41.0)

The stereo methods drive the camera through `game/dishonored/camera` (a real module) on
the script lane, inside the ProcessEvent hook's camera pass right after `FovLeverApply`.
A per-eye render needs three writes; two are measured, one is not:

| Need | Write | Status |
|---|---|---|
| Rotation | `ApplyHeadToViewRotation`: HMD pitch/yaw(/roll) into the `ProcessViewRotation` parms, every dispatch of the modifier chain (`[HeadTrack] ChainStamp`) | MEASURED, shipped since 30.57 |
| FOV | the lever: `kLevCtrl` {0x3ac, 0x3b0, 0x3b4} on the controller and `kLevCam` {0x254, 0x348, 0x368, 0x38c, 0x540, 0x564} on the camera every dispatch; 0x53c is the read-only sensor (what the engine rendered) | MEASURED, shipped since 30.50; `camera::set_fov_deg` is the lever's target, `rendered_fov_deg` the sensor |
| Eye Z (the crouch clamp) | Z of {0x80, 0x330, 0x350, 0x374} on the camera, dispatch cadence | MEASURED (38.24): the clamp sticks, so at least one of those four reaches the renderer after the script pass |
| Lateral eye offset (+/- IPD/2 along `kCamRight` 0x60) | `camera::apply_eye_offset` into `[Camera] EyeField` (camera+0x330, which holds -position) | MEASURED 2026-09-02: 0x330 HONOURED 119/120, the five others DISCARDED (below) |

Why the lateral write is unproven: `head_track.cpp` records that the POV location the matrix
carries is a cache the engine recomputes each tick, yet the 38.24 eye clamp writes Z into
0x80/0x330/0x350/0x374 on the same cadence and is honoured. Either the clamp lands because
the same write repeats every dispatch (the last write before the draw wins), or because one
of the four fields is the renderer's actual source. For Z both explanations give the same
picture; for a lateral offset they do not (a field recomputed between the last dispatch and
the draw discards the offset, and a persistent field accumulates it).

The instrument: `camera eyetest <uu> [0x80|0x90|0xc4|0x330|0x350|0x374|all]` (seam word;
`camera eyetest stop` cancels). Stand still in gameplay. For 120 presents per candidate the
script lane adds `<uu>` along the camera's right row into the candidate (re-based every
tick, so a persistent field does not accumulate; restored afterwards), and the present
thread reads the render-side camera position back from vertex constant c5 (the frame-map
ABI: c5 = camera world position, captured in `core/framework/vs_const_hook.cpp`) and
projects `c5 - base` on the right vector. One line per candidate, then a summary:

    camera/eyetest: 0x80 asked +100.0 uu along right -> c5 moved +99.8 uu (mean of 118): HONOURED 118/120 frames ...
    camera/eyetest: 0x330 asked +100.0 uu along right -> c5 moved +0.3 uu (mean of 120): DISCARDED 120/120 frames - recomputed before the draw
    camera/eyetest: DONE (+100.0 uu): 0x80=HONOURED 0x90=DISCARDED 0xc4=DISCARDED 0x330=... 

HONOURED means the renderer drew from the offset position: that field is the seam's write
point (`[Camera] EyeField=`, or `camera eyefield <name>` live). DISCARDED means the engine
recomputed the field before the draw. INCONCLUSIVE means c5 moved by neither amount (the
player moved, or the draw sampled c5 from a different tick than the write). A DISCARDED-
everywhere result is a finding, not a failure: the lateral offset then needs a later write
point - a position-only patch at the camera-matrix builder epilogue (`kCamHookAt` was
disproved for ROTATION; position was never tried) or the c0 view-projection translation
(`LeanVP`, which positional tracking already uses and which AER can drive per eye).

**MEASURED (2026-09-02, dev PC, simulator lane, the auto-continued save, builds `dd10da09`
and `a3ede882`; runs 10 and 11; +100 uu, 45 presents of c5 baseline then 120 writing per
candidate):**

| Field | Reads (with c5 = (-53.6, 4835.0, -3036.1)) | Verdict |
|---|---|---|
| 0x80 `kCamLoc0` | (5.2, 500.0, -130.6) - a fixed offset vector, not the position | DISCARDED 120/120 (c5 moved 0.0) |
| 0x90 `kCamLoc1` | the same (5.2, 500.0, -130.6) | DISCARDED 120/120 |
| 0xC4 `kCamLoc2` | the same (5.2, 500.0, -130.6) | DISCARDED 120/120 |
| 0x330 `kPovOffs[0]` | (53.6, -4835.0, 3036.1) = exactly **-c5** (a view-matrix translation) | **HONOURED 119/120**: writing +100 uu into the negated field moved c5 by +99.2 uu along right (run 10, before the sign was known: -98.7 uu on 75/76 frames) |
| 0x350 `kPovOffs[1]` | (53.6, -4735.0, 3036.1), also -c5 in form | DISCARDED 120/120 (c5 moved -2.2 uu, mean) |
| 0x374 `kPovOffs[2]` | (53.6, -4835.0, 3036.1), also -c5 in form | DISCARDED 120/120 (0.0) |

So: c5 IS the camera world position (it equals minus 0x330 to the decimal), and **camera+0x330
is the eye-offset write point**: a value written there on the script lane at dispatch cadence
is what the renderer draws from, in NEGATED form. The seam's field table carries the sign
(`kFields[].sign`), `[Camera] EyeField=0x330` is the default, and `apply_eye_offset` writes
base - offset there. The camera's right row at +0x60 read (0, 1, 0) at yaw 0 (UE3: X forward,
Y right). 0x350 and 0x374 are copies the engine recomputes before the draw; 0x80/0x90/0xC4 are
not positions at all (the earlier reading of them as "matrix translation row" and "cached POV
location" was wrong - retire it). Whether 0x330 persists between dispatches (the writer's
re-base logic) was not separated out; the eyetest's own 120-present window shows the value
must be rewritten every dispatch, which the seam does.

## Head roll: UE3 positive roll = right ear DOWN (2026-09-03, measured by picture)

The second headset run reported the head TILT reversed under the projection layer. The
roll telemetry on the `headtrack:` line (41.1) first proved the write LANDS - `incoming`
(what the engine hands ProcessViewRotation) equals `wrote` on the next dispatch, so the
engine keeps our roll and it reaches the render. The direction was then measured on the
simulator by picture (run 37): `head rot 0 0 20` rolls the simulated head right-ear-down
(the simulator's roll is a rotation about the forward axis, `quat_from_ypr`), and the
game's own frame showed the world's verticals leaning with their tops to the RIGHT. A
right-ear-down head must see them lean LEFT. Reversed, as reported.

Cause: `g_hmdRoll = atan2(right.y, up.y)` is positive for the right ear UP, and UE3's
rotator roll is positive for the right ear DOWN (the picture: writing a negative roll
rolled the camera left-ear-down). The value is now negated once at its derivation, so the
ProcessViewRotation write, the matrix injection and the lean counter-rotation all inherit
UE3's sense; `[HeadInject] FlipRoll` stays the A/B override with 1 = the measured
direction. Re-measured after the fix (run 38): +20 leans the verticals left, -20 right,
the exact mirror of the faulty frames; pitch and yaw untouched (hmd pitch -30 -> view
pitch -5461 units = -30 deg).

## The camera field holds the POSITION, c5 is its negation (2026-09-03, corrects session 5)

**This one sign put every eye offset and every lean backwards in the headset.** Session 5's
`camera eyetest` measured that camera+0x330 reads exactly -c5, assumed c5 was the camera's
world position, and recorded the field as holding the NEGATED position (`kFields[].sign =
-1`). Everything downstream then wrote its offsets negated.

**The measurement that settles it** (run 35, the simulator, the sewers, a DIFFERENTIAL
picture test against a known-good reference - the c0 `LeanVP` patch, shipped and
headset-tuned since 30.35): command the same head displacement on both lanes under a
projection layer and dump the game's own frame.

| displacement | vp lane (`LeanVP`, the reference) | camera lane, sign -1 (session 5) | camera lane, sign +1 |
|---|---|---|---|
| 2 m right | the corridor wall on the LEFT, the city outside on the right (the camera went right, through the wall) | MIRRORED: wall right, city left (the camera went LEFT) | matches the reference |
| 2 m up | up inside the ceiling pipe, the corridor floor below | the camera went DOWN, under the floor looking up | matches the reference |

So writing +X into camera+0x330 moves the rendered view by +X: **the field holds the
camera's world POSITION, and c5 (the vertex constant) is its negation.** Two independent
confirmations: the shipped 38.24 crouch clamp writes a world Z into these same fields and
compares it against the pawn's world Z (which only makes sense for a position), and it was
verified to fix crouch; and the picture above.

`kFields[]` now carries two numbers: `sign` (what a wanted WORLD displacement is multiplied
by to become the field delta: +1) and `c5Sign` (how c5 answers it: -1 on the POV fields).
The eyetest asks for VIEW travel and expects c5 to move by `uu * c5Sign`; it still reads
**HONOURED 119/120** (c5 -99.2 uu for +100 asked), the postest reads HONOURED on both axes
(+29.8 for +30 right, -24.4 for -25 up), and the doubling is unaffected (L/s 52 R/s 51,
draws = 2nd = 52).

**The trap, for the next reader.** The eyetest was an instrument that could not fail its own
hypothesis: it wrote into a field and measured a DIFFERENT engine quantity (c5) that moves
with it, so it proved "the renderer noticed" and was read as "the renderer drew from the
offset position". Only the picture can answer the direction. Any future write point needs a
differential picture test against a known-good path before its sign is believed.

## Positional tracking on the camera seam (2026-09-03, S1)

The camera object carries a row-major basis at +0x50 (forward), +0x60 (right, `kCamRight`),
+0x70 (up); the original author's `FindPovRotators` matched +0x50 against the POV
rotator's forward, and 41.1 reads all three (`kCamFwd`/`kCamRight`/`kCamUp` in patterns.h),
validating them orthonormal before the first write. Measured at yaw 0 in the auto-continued
save (run 20): `fwd=(1.000 0.000 -0.002) right=(-0.000 1.000 -0.000) up=(0.002 0.000 1.000)`,
pairwise dots 0.000 (UE3: X forward, Y right, Z up).

Lean/crouch/roomscale used to ride the c0 view-projection patch (`LeanVP`), a matrix patch
the renderer's attachments do not follow. `[PosTrack] Lane=vp|camera` (default vp, the
shipped path; `postrack lane <l>` live) moves the offset onto the camera seam's write: ONE
write per dispatch of `base - (eye + position)` into camera+0x330, the position offset
resolved along the basis rows. The seam is the single owner of the offset (TrackHead
publishes it there; both lanes read it there), and the `camera postest <R> [U] [F]`
instrument overrides it with a commanded triple, takes 45 presents of c5 baseline at zero,
then judges 120 presents:

| lane | asked (R, U, F uu) | measured (c5 travel on the basis rows) | verdict |
|---|---|---|---|
| camera | +30, 0, 0 | +29.7, +0.2, +0.0 (mean of 120; 2284 seam writes) | HONOURED |
| camera | 0, -25, 0 (a crouch drop) | +0.0, -24.4, -0.0 | HONOURED |
| camera | 0, 0, +40 | +0.0, +0.4, +39.7 | HONOURED |
| camera | +30, +25, -40 | +29.7, +25.2, -39.7 | HONOURED |
| vp | +30, 0, 0 | the c0 patch ran on 120/120 presents (2760 uploads) | APPLIED (c5 cannot see a matrix patch) |

**Where that table was measured.** Run 20's rows come from the TITLE SCREEN's attract
camera (`DishonoredPlayerCamera_0`), which the state line called GAMEPLAY (the next section).
Re-measured in real gameplay (run 21, the Dunwall Sewers level, `DishonoredPlayerCamera_1`,
ProcessViewRotation dispatching at 270/3 s): `camera eyetest 100 0x330` HONOURED 120/120
(+100.0 uu), `camera postest 30 0 0` on the camera lane measured R+30.0 U+0.2 F-0.0. The
same two instruments on the level's LOADING screen ("press any key to continue", no script
dispatches) read DISCARDED / NOT HONOURED with c5 frozen: a static camera the loader
re-sets every tick. So the write point holds on the attract camera and in gameplay, and
an instrument run must know which camera it measured: the `[game] state` line now says.

So the renderer draws from the offset position on the camera lane within 1-2 %, on all
three axes, the same write point the eye offset uses. What the instrument does NOT say:
the vp lane's effect is a matrix change c5 never sees, so "the same travel on both lanes"
is only half measurable; and a shot diff on the mono screen (a simulated 40 cm head step)
reads 1.5-2.1 mean-abs-diff on both lanes against a 1.3-1.7 return-to-zero control, i.e.
the scene's own animation swamps a 14 uu lean at the quad's 16 % coverage. The picture
verdict belongs to a stereo method's projection layer (`world-6dof.xrs` `w_trans_x`).

Two facts found on the way. (1) The 38.24 eye clamp lives inside `FovLeverApply` AFTER
its early return, so it is dead whenever the lever is off - which is the mono screen's
default (`[Screen] FovLever=0`). The camera lane's writer caps its Z at the same ceiling
while the clamp is live (`camera::set_eye_ceiling`), so the two never fight; with the lever
off neither runs. (2) The seam writes ~19 times per present (every ProcessEvent dispatch
of the camera pass), which is the cadence the 38.24 clamp and the FOV lever already used.

## The game state under GamepadOnly, and the projection gate (2026-09-03, S2)

The `[game] state` line read GAMEPLAY on the title screen, in the main menu and on a
loading screen (runs 21-22; the simulator captures showed "Press any key" while the
instruments ran). Cause: every script-event tracker (the pause-menu open/close names, the
`OnToggleCinematicMode` latch, the UI vocabulary) sat inside the motion-aim block behind
`g_maimEnabled`, which `[Mode] GamepadOnly=1` turns off, and windowed mode has no cursor
test (32.9). The same class of bug as the 41.0 PreExit handler. 41.1 hoists the tracking
out (only the fire-window arm stays gated) and adds what the runs measured:

| screen | signal (measured) | state |
|---|---|---|
| title screen / main menu | `Start`, `OnFocusGained`, `BackToStartScreen`, `Req_CanContinueGame` on an object whose class contains `MoviePlayerMainMenu`; leaves with `UnregisterControllerDelegates`. Its own flag (`g_mainMenu`): the stale-flag ghost test clears `g_menuOpen` while dispatches flow, and the attract camera behind the menu dispatches | MENU |
| the attract scene's cinematic toggle | `OnToggleCinematicMode` fires ON at the title screen with the attract pawn already latched, so a level load INHERITED the latch (CINEMATIC in the sewers). A new pawn latch now clears it: a fresh level starts clean; in-level cutscenes keep their parity | CINEMATIC only in a cutscene |
| loading screen ("press any key to continue") | no ProcessViewRotation dispatch for 750 ms with a live pawn (the head-write counter read 0/3 s); the loader dispatches a burst about once a second, so leaving LOADING needs one second of continuous dispatches | LOADING (new) |
| gameplay | pawn live, no menu, no cinematic, dispatches continuous | GAMEPLAY |

The runtime layer's projection path is gated on the same verdict: `frame_hooks` arms
camera mode when the active method (or `stereo projection on`) claims a projection layer
and publishes the verdict every present; the runtime's cinematic fallback (3-present
hysteresis) drops to the quad screen on a false verdict and returns on a true one. Walked
in run 24: title MENU -> quad, main menu MENU -> quad, load LOADING -> quad, level
GAMEPLAY -> `xr: cinematic quad off`, two projection views, both eyes 72 % non-black.
Before this, the runtime's projection machinery had no caller in this game at all (camera
mode was the overlay checkbox only, and a stale verdict would have pinned the quad).

Two FOV facts from the same runs. Under a projection layer the lever's target is the
runtime's circumscribed hfov (137.0 deg at 16:9 on the simulated Quest 3; vfov 110.0 for
the 54/55 deg half-angles) and the 0x53c sensor followed it to 137.0 within a second in
gameplay (the engine interpolates: 136.1, 136.2 ... per present), the claim reading
`src=readback`. The loading screen's camera ignores the lever (sensor 75.0, the level's
natural FOV). And the lever captures its "natural base" whenever it re-arms: after an
arm-disarm-arm it captured 137 (the FOV its own previous writes had left on the
controller), so the ratio law then scales from 137, which is harmless for the target but
means "natural" is not the game's 75 any more - a re-arm should reset the FOV first if the
number is ever used as a baseline.

The simulator's `claimRatioH` reads 2.17 under this lever by construction (the claim's
tan(68.5) against the eye's own mean half-tangent, tan(54)/tan(44)): BioShock rendered
the eye's own FOV, this lever renders the circumscribed one. Not a magnification error
while the claim equals the render; `fovaudit src=readback` is the check.

## F11 reset and natural-FOV feedback (VR-50, 2026-09-15)

Offline native-key tracing identifies F11's engine fullscreen toggle before ordinary
script input. The F11 name initializer at RVA 001FF5D1 stores the name index at VA
01448708; virtual key 0x7a maps to it at RVA 005CAC21. Input code at RVA 005C8E6B
compares this name; the branch through 005C8E7D..005C8F0A queries viewport fullscreen
(vtable slot +0x58), inverts it and calls viewport resize, then returns before normal
input at 005C8F40. This was derived with the local disassembly tools, not implemented
as a new hook. Game-derived output remains uncommitted. The shipped F11 BendTime
binding does not describe this earlier native handling.

ResBeforePresentParams in render_size.cpp converts fullscreen to virtual windowed
mode while retaining its requested backbuffer. The verified307 run reset
2750x2850 -> 1355x1405 -> 2750x2850. Its FOV fell from108.07 to74.89. The earlier
natural-cache recapture described above is only harmless at an unchanged target:
FovLeverApply multiplies the current sensor by target/natural, so a slight aspect-driven
target decrease can compound downward after natural captured the mod's own output.

The353 candidate uses the existing reflected CameraCache.POV.FOV field, not a new
offset. Gameplay scopes scale its half-angle tangent by tan(requested/2)/tan(target/2),
retain zoom, and restore after both eye draws. The persistent FOV writer skips the active
gameplay scope. CfValidate checks IsLiveObject, current GObjects identity slots, ownership
and load generation; refresh rebuilds the live table before capturing new identities.
Menu exit must reacquire identity. Cinematic/exit scopes keep precedence. Rendered
acceptance remains a headset/log question. Full evidence and failed assumptions:
[PERFORMANCE.md](PERFORMANCE.md), active F11 section.

## The scene-draw root, derived live (2026-09-03, S2b)

The SequentialReentry seam needs the ONE function whose call tree draws the scene and
enqueues the present, called once per tick from the engine's tick. Derived live in runs
26-27 with the instruments in `game/dishonored/scene_probe.cpp` (`reentry census`, `reentry
stack event|caller|present`, `reentry probe`, `reentry findstart`), the method BioShock
Infinite's session 40 used (bioshock-1-vr-mod, ENGINE_NOTES "the render root"), then
confirmed statically with `tools\pe-xref.ps1`. Static walking alone was not attempted: on
Infinite it failed twice.

**1. The caller census at the camera write.** `ProcessViewRotation` (the head-tracking
write's dispatch) is dispatched from ONE call site, `call eax` returning to `0x005d0789`,
693 times in 693 presents (once per present); the dispatching object is the camera
modifier (`CameraModifier_CameraShake`). The script thread and the present thread are
DIFFERENT threads (script tid 16012, present tid 16848 that run): a render thread presents;
this game is Infinite's substrate (threaded, `OneFrameThreadLag`), not BioShock 1's.

**2. The one-shot stack scrapes** (`RtlCaptureStackBackTrace` is cut to 3 frames by
frame-pointer-omitted code; the raw call-preceded scrape walks the whole chain and, for an
`E8` site, names the function the frame ENTERED). Two chains on the game thread, walk-up
order, outermost frames last; they SHARE their outer half:

| ret | enters | role |
|---|---|---|
| `0x00ec44f3` | `0x009e3c60` | the main loop's per-frame body |
| `0x009e3d20` | `0x009e3b90` | |
| `0x009e3c4a` | `0x009e3980` | |
| `0x009e3b1f` | `0x009e03b0` | the frame function that calls the engine: at `0x9e0555` it does `mov edx,[ecx]; mov eax,[edx+0x124]; push ecx; fstp [esp]; call eax` = `GEngine->Tick(DeltaSeconds)` (a virtual with ONE float argument) |
| `0x009e055a` | (virtual) `0x00a17890` | `UDishonoredEngine::Tick` (0 direct callers, 1 `.rdata` vtable reference - a virtual, as it must be); it calls `0x00632860` at `0xa1799f` |
| `0x00a179a4` | `0x00632860` | **`UGameEngine::Tick`** (1 direct caller = the subclass above, 1 vtable reference); both chains below live inside it |
| tick chain: `0x00632a09` | `0x0065e0d0` | the world tick (1 caller) -> ... -> `0x005d0710` (1 caller) -> `call eax` at `0x5d0784` = the `ProcessViewRotation` dispatch. **The camera is computed in the TICK, before the draw** |
| draw chain: `0x006330e1` | **`0x005fc5b0`** | **the viewport draw root** (below) -> at `0x5fc92b` `mov ecx,[ebx+0x1c]; mov edx,[ecx]; mov edx,[edx+8]; ... call edx` = the viewport CLIENT's Draw through `[viewport+0x1c]` -> vtable slot 2 (Infinite's exact shape) -> a script event on the viewport client -> natives -> the generic event helper at `0x567a5e` (the ONLY direct `E8` caller of `ProcessEvent` in the exe; every other dispatch is virtual) -> `ProcessEvent(PostRender)` on `DishonoredHUD` |

**3. The bytes at the call site** (`reentry probe 633090 128`), inside `UGameEngine::Tick`:

    63309a  mov edi,[esi+0x48c]        ; this->GameViewport (UGameViewportClient*)
    6330a0  test edi,edi / jz
    ...     (a virtual on the client with one argument, slot 0xec)
    6330cd  mov eax,[esi+0x48c]
    6330d3  mov ecx,[eax+0x40]         ; GameViewport->Viewport (FViewport*)
    6330d6  test ecx,ecx / jz 6330e1
    6330da  push 1                     ; bShouldPresent = TRUE
    6330dc  call 0x5fc5b0              ; FViewport::Draw(TRUE)   <- kViewportDrawCallSite
    6330e1  ...                        ; kViewportDrawGameplayRet

and the root: `0x005fc5b0` begins `55 8b ec 6a ff 68 a3 97 f2 00 64 a1 00 00 00 00` (push
ebp; mov ebp,esp; push -1; push 0xf297a3; mov eax,fs:[0] - an SEH prologue, a function
entry), its first `ret imm16` is `ret 4` at +0x1fc (ONE stack argument: the `push 1`), its
body builds a canvas (the `lea ecx,[ebp-0x10c]; call` pair around the client-Draw dispatch)
and tears it down after. `pe-xref`: 3 direct callers (`0x4dba68`, `0x6330dc`, `0x641d87`),
0 vtable references; only `0x6330dc` is the per-tick gameplay dispatcher - the other two
are not reached in gameplay (the deny gate's foreign-caller counter reads 0 across the
runs). The control for the static tool: `ProcessEvent 0x470640` must report exactly 1
direct caller (`0x567a5e`) and ~2087 vtable references, and it does.

**The values in patterns.h**: `kViewportDraw 0x005fc5b0`, `kViewportDrawPrologue[16]`,
`kViewportDrawRetImm 4`, `kViewportDrawCallSite 0x006330da` (7 bytes `6a 01 e8 cf 94 fc
ff`, the `push 1; call`), `kViewportDrawGameplayRet 0x006330e1`, `kGameEngineTick
0x00632860` and `kViewportClientOff 0x1c` (derivation only). Every hook byte-verifies the
prologue AND the site (and that the site's rel32 targets the root) before patching, and
refuses with the bytes it found.

**4. Made to MOVE.** `reentry pulse 3` doubled three gameplay draws: `second draw ok,
call2=414/229/218 us`, presents advanced by one per pulse (the root presents in its own
tail, unlike Infinite's client draw), and under the method the pair line reads **the +1
present's c5 sits (0.02 6.17 0.00) uu from the -1 present's (|d| 6.17; ipd*scale = 6.17
expected along right)** - every pair, to the hundredth: the two presents of a tick are
drawn from two cameras half an IPD apart along the camera's right row. The capture pair
(`D:\dvr-data\xrsim\eyecheck\20260903_032840_reentry`) shows the parallax on the near pipe.

**5. The method, measured (run 28, the sewers, simulator at 90 Hz):** `reentry: beat
draws/s=53 2nd/s=53 presents/s=106`, `stereo: beat method=reentry out/s=107 L/s=54 R/s=53
mono/s=0`, `call2` 218-467 us, skips 0 on every gate, no fault, `stereo.xrs`
`projectionViews eq 2` PASS with both eyes 71/68 % non-black, eye-check leg 0 PASS (38/37)
and leg 1 PASS (projection, 0.063 m), `stereo mono` restores the call site (`reentry: hook
removed`) and the mono beat returns. The tick rate halves under the doubling on this rig
(90 -> 53 draws/s at 1080p on the simulator: the second draw is a full scene draw for the
GPU, and the game thread waits for the render thread); presents = 2x ticks holds.

**What the eye-check bands say here.** Legs 2/4/5 were calibrated on BioShock 1's
fairground (interocular mean 40-70). On this scene the MONO projection (identical images
composited at the two eye poses) reads 13-22 mean and the true stereo pair reads 6-7: a
per-eye render agrees with the compositor's per-eye poses better than one image shown
twice, so the diff FALLS. The instruments that carry the verdict here are leg 0 (the
pairing) and the pair line's c5 travel; the interocular band needs its own Dishonored
calibration once a headset run has judged fusion (KNOWN_ISSUES).

**The first headset run (2026-09-03, Quest 3 via VDXR, the user; `42-run30-quest3-reentry.log`)
failed on two counts, both explained by the log, both invisible on the simulator:**

1. *Both frames in both eyes, alternating.* The doubling ran (`draws/s=54 2nd/s=54
   presents/s=108`, `pair pacing live`, the pair c5 line 6.08 uu) but the beat read
   `L/s=36 R/s=54 mono/s=18`: a third of the LEFT tags were dropped by the method's own
   pairing check, which compared the camera position the tick's last write produced with
   the present's c5 and required them equal within 2 uu. While the player WALKS the engine
   moves the camera by a tick of travel AFTER that write (measured: `c5 5692.0 6376.0` vs
   `written 5689.5 6375.8`, ~2.5 uu along the heading, the eye offset intact), so the -1
   present failed the check (the +1 present is written in the stub right before its draw
   and always matched). Every dropped left tag broke a pair and the runtime submitted its
   latest single image to both eyes for that frame. On the simulator every run stood
   still. The position check is telemetry now (a 40 uu line for teleports); the ring's
   push/pop ORDER pairs the eyes.
2. *Head motion reversed on lean, a second motion on pitch.* Under a projection layer the
   compositor moves the image for the head's REAL displacement (the located pose,
   including the neck's travel on a pitch and the roll). The positional path was built
   for the head-locked quad: a screen-space matrix shift with a deadzone and a room-scale
   bleed that re-centres the reference within a second (the heartbeat shows the lean
   decaying to 1-4 uu while the user leaned), and `[HeadTrack] Roll=0` never rolled the
   camera. So the game rendered from a camera that had not moved while the layer said it
   had: reversed parallax on a lean, a swim on a pitch, a counter-rolling horizon. Under a
   projection layer the game camera now follows the head's RAW displacement (no deadzone,
   no bleed, no clamp, no synthetic crouch drop) through the camera lane in the yaw-only
   frame (`[PosTrack] Lane=auto`), and the head roll is written. The quad screen keeps the
   tuned lean and no roll. Not yet re-judged in the headset.

**A loose end, recorded.** The ring between the game thread's tag push (per draw) and the
present thread's pop (per present) can hold two pairs legitimately (the game thread runs a
frame ahead); the first build cleared it at depth 3 and re-paired mid-pair every few
seconds (the c5 check caught every one: "tag -1 dropped ... c5 6383.1 is not the position
the draw wrote 6376.9" - the two eye positions, 6.2 uu apart). Fixed by allowing two pairs
and letting the c5 match skip stale tags (`tagResynced` in status.json).

## VR-30: silent ownership refusal in alpha-317 (2026-09-05)

The tested alpha-317 build (`14235674` source, dirty build tag based on `af94ba89`)
reached GAMEPLAY at log time 24033218. Its 120 `armfollow/yaw` samples show the old
view/pawn coupling, but it has no `yaw: owner` or `yaw: gen` lines. Offset reflection
succeeded (`Controller.Pawn +0x248`, `Actor.Rotation +0xd0`); ownership never activated.
This run did not test the new body-target arithmetic downstream.

`YawOwnerValid` called `IsLiveObject` without `BuildLiveSet`. That helper searches
`g_liveSet`, a sorted snapshot populated by other discovery paths, rather than the
current GObjects table. A new gameplay object absent from the earlier snapshot is
rejected indefinitely. This dependency reproduces in host tests: the pre-fix code
cannot publish/apply for a valid possessed pair when the discovery snapshot is empty.
Because its guards were silent, the log alone cannot distinguish that refusal from
the other pre-fix identity guards. The repair removes the stale-snapshot dependency
and logs every refusal category with the identities involved.

No new offsets: scan the existing `kGObjHdr` table when binding a pair; cache the
found slots and compare current slot contents before each write, following CamAlive
and SkcAlive. Rebinding scans are throttled during loads. Keep the reflected possession
check. Retry unresolved reflection instead of permanently latching a startup failure.
Check snapshot validity/generation again after validation because validation can replace
the owner. A body target is applied only after a successful head write/replay. The
second-eye branch must bypass fresh integration regardless of elapsed milliseconds.

Host coverage now includes real ownership/publication/body-write functions with mocked
engine services, not just subtraction. Eleven ownership checks plus seven arithmetic
checks pass. Neither suite establishes that the engine's next view input is independent
of the pawn target; the earlier comment claiming feedback was structurally impossible
was unsupported. Headset acceptance remains pending.

## VR-30 IS NOT SOLVED, and BioShock Infinite says why the approach is wrong

**Correction to the entry below.** "Perfectly still" was reported once and did not
reproduce: a later run on a tree **byte-identical** to that build (verified with
`git diff --stat`, empty) still had the arms moving with head yaw, with the same ini. So the
result was never real, or it depended on something nobody was measuring. Either way the
lever below is not a fix, and the entry that called it one was written from a single
unverified report.

**The process failure is worth more than the code.** Four changes shipped default-ON in a
row, each reasoned from the last symptom rather than measured, each regressing the tester:
an accumulator that fed back on itself, a reference borrowed from another lane, an FOV scale
that no value could null, and a frame gate that broke the VR hook outright. This project's
own rules say measure first and ship levers off. Neither was followed, and the cost was a
headset session.

### What Infinite does instead (`docs/reference/bioshock-trilogy-vr`, local mirror)

BioShock Infinite is the same engine family - UE3 - and its VR mod **does not try to stop
the arms following the view at all**. `src/game/bioshockinf/bones.h` declares:

```c
bool drive(const FrameContext& fc, const GamePose& target, int hand, float scale,
           int armsMode, bool animMode, float capDepthCm, const float wristDeg[3]);
```

It drives one hand's **bone cluster toward a game-space target** every frame, pass-1 game
thread only, with `armsMode 2` collapsing the whole arm chain to a point behind the grip at
zero scale. The hands are **placed absolutely**; whether the body yaws underneath them is
irrelevant, because nothing about their position is derived from it.

That is the architectural answer, and Dishonored's own notes already contain the matching
measurement without anyone joining it up: attempt 5 records that **SkelControl world-space
TRANSLATION works and is absolute** - "the pin test: you can walk away from your hands" -
while world-space rotation does not. Absolute placement is exactly the shape Infinite uses,
and it is the half that was measured to work here.

So VR-30's remaining work is what its own pass criteria always said: place the hands from
the controllers. Every lever tried this session was an attempt to avoid that, and each one
failed for a different reason - which is itself the evidence that the coupling is structural
and cannot be switched off.

### What still stands from this session

The **pitch** result is real and holds: forcing the `DisableArmFollow` influences removes
the vertical follow. The corpus findings, the yaw ladder and the negatives below are all
measured or read, and none of them depend on the discredited "perfectly still" report.

## (SUPERSEDED - see above) the arms are decoupled from the view, both axes

**Headset-judged.** Pitch went first (the `DisableArmFollow` influences); yaw followed by
writing a counter-rotation into `m_ArmFollowOffset_Rot_Primary/_Secondary`. Seven recorded
attempts preceded this one and all of them looked for a switch to turn off. There is no
switch. **The answer was a subtraction**, and the field that carries it had been sitting in
the same struct as the pitch weights the whole time - a Rotator among floats, gated by a
second influence pair nobody had raised.

Ships as `[Camera] ArmCounterYaw=1.0` and `[Camera] ArmDisableWeight=1`.

### The residual, and what it means

With the gain at 1.0 the arms are **rock steady in pitch** and very close in yaw, with one
artifact left: turning the head yaws the arms **slightly against** the turn - turn right and
they drift a little left. That is **over-correction**, and it is diagnostic rather than
mysterious: the counter is subtracting a hair more yaw than the arms were actually given.

Two readings, and they are told apart by what happens when the head STOPS:

- **The offset persists when you stop turning** - the gain is too high. The arms follow at
  slightly less than 1:1, so countering at exactly 1.0 overshoots by the difference. Fix is
  arithmetic: lower `ArmCounterYaw` until it nulls (0.9 first, then bisect).
- **The offset springs back to centre when you stop** - it is a LAG mismatch, not a gain
  error. The engine's arm follow is smoothed and ours is instantaneous, so during motion we
  are ahead of it and the difference shows as a swing that settles. Fix is to match the
  smoothing, not to change the gain, and lowering the gain would then leave a permanent
  offset in exchange for a smaller swing - a bad trade.

**Do not tune this by feel without asking which of the two it is.** They look similar in the
headset for a moment and want opposite fixes.

## The YAW ladder: every candidate, ranked, from two independent sweeps (VR-30, 2026-09-05)

Two separate passes over the corpus - one aimed at the animation and skeleton, one at
scripted events and cinematics - reached the same first choice, which is worth more than
either reaching it alone. Recorded here so the next attempt starts at the top of a list
instead of at the beginning.

**The root cause, agreed by both passes**: there is no yaw driver for the arms anywhere.
Every arm/aim/look mechanism the game exposes is pitch-only or NPC-only. The arms yaw
because the **pawn** yaws - they are bones of its single mesh, and `FaceRotation` (a native
override, and the only rotation-related override on `DishonoredPlayerPawn`) sets the actor's
yaw from the controller every frame. The camera code never needed to ADD yaw, which is
exactly why every arm-follow weight measured pitch-only.

The architecture says so out loud: `StatePlayerMasterBase` carries `m_bIgnorePitch` (default
true) and **there is no `m_bIgnoreYaw` twin**. The engineers only ever needed to decouple
pitch.

### The ladder, cheapest and most likely first

1. **`m_ArmFollowOffset_Rot_Primary/_Secondary`** - Rotators (not floats) in
   `DishonoredVTSettings` with a live Yaw field, scaled by `m_ArmFollowOffset_Weight_*`
   (shipped 1.0) and gated by the `DisableArmOffset` influence pair, which is a DIFFERENT
   pair from the `DisableArmFollow` one the pitch work used. **Both sweeps ranked this
   first.** Shipped as `[Camera] ArmCounterYaw`; write `-headDelta` every dispatch.
2. **`DishonoredPlayerController.m_bLimitPlayerYaw` + `m_fStartingPlayerYaw` +
   `m_fLimitPlayerYaw_Min/_Max`** (`DishonoredPlayerController.uc:204, 224-226`) - a
   dedicated yaw clamp relative to a captured starting yaw, with its own enable bool. Plain
   transient UProperties, **no script writers anywhere**, so the native code reads them every
   frame and nothing in script will fight a write. This is the game's own "pin the view's
   yaw" and it is the strongest fallback.
3. **`m_pLookAtControl_LeftHand` / `_RightHand`** (`DishonoredPlayerPawn.uc:307-308`) -
   `SkelControlSingleBone` on the hand bones, each with a full `Rotator BoneRotation`,
   `bApplyRotation`, `bAddRotation`, a selectable `BoneRotationSpace`, and
   **`bRemoveMeshRotation`** - literally "strip the component's own rotation from this bone",
   which is the decoupling primitive stated as a flag. Untested. Note these are the SIBLINGS
   of the camera control that broke the view, so treat them with the same care.
4. **`DishonoredCamera_AnimDriven.m_CurMaxPlayerControlledYaw_Neg/_Pos`** (+ `_Target`,
   `_Blended`, `_SoftenThreshold`) - the per-axis "how far may the view deviate from the
   body" clamp that leaning, choking, conversations, possession and the DLC kill-cam all
   funnel through. Ships `m_fDefaultWeight=0.0`, so it is inert until its influence is raised.
5. **`m_Debug_Player.m_bIgnoreAimOffset`** - one bool, and `IgnorePlayerAimOffset` is the one
   cheat in the whole manager with a real script body doing nothing but writing it. Gates
   `AnimNodeAimOffset`, whose `Aim.X` is the horizontal channel. Free to try.
6. **`StatePlayerMasterLeaning.m_fMinAllowedYawDegrees/_Max`** (+/-60) - `config(PlayerState)`,
   so it is the only yaw window in the game settable from an ini with no code at all. It only
   applies while leaning, but it is a zero-cost existence proof that the clamp works.
7. **`FaceRotation` itself** - the complete fix and the last resort. Movement, melee facing
   and mantling all key off actor yaw, so neutralising it needs a separate locomotion yaw.

### Existence proofs that the game CAN hold the body while the view moves

It does this already, in five places, all with real numeric yaw windows: leaning (+/-60
degrees, config), choking (`DisTweaks_Choke`, an **asymmetric** -50/+10), conversations
(`DisConvLookAtConstraints.m_fMaxYawDelta`), possession
(`DisTweaks_Possessable.m_fYawLimitWhilePossessed_Deg`), and soiree matinees
(`DisInterpTrackAllowPlayerLook` with `m_fYaw_Min/_Max` keyframed, over an
`InterpGroupPlayer` that pins the body to a stage mark). So the capability is shipped and
tuned; the question was only ever which knob reaches it from outside a scripted state.

### Negatives worth not re-deriving

- **There is no separate arms mesh.** `DishonoredPlayerPawn`'s `pMesh` is one
  `DishonoredPlayerSkeletalComponent` carrying arms AND body (`SDPG_Foreground`,
  `bOnlyOwnerSee`, `CastShadow=false` - it is the viewmodel). `m_BodyMode`
  (`ARMS_ONLY/FULL_BODY/HIDDEN`) swaps meshes, it does not add a component. That component's
  `m_Offset` is a **Vector, not a Rotator**, so the arms cannot be rotated independently
  through it.
- No script-side flag, state or branch skips `FaceRotation`; the per-frame call is inside
  `native final UpdateRotation`.
- `LimitViewRotation` is pitch-only by signature.
- `AnimNodeAimOffset` has the `Aim` Vector2D with a horizontal range this problem wants, but
  **nothing in the game references the class**. Do not hunt for it in memory.
- No spine bender on the player (NPC-only), no cover system, no
  `bUseControllerRotationYaw` analogue, no camera YAW anim notify (the pitch one exists).

## `LookAtControl_Camera` IS THE CAMERA'S BONE CONTROL, NOT THE ARMS' (VR-30, 2026-09-05)

**Do not zero it.** Attempt 5 recorded this control as "the camera one aims the arms at the
view", and that description sent this session down a wrong road. Forcing its
`ControlStrength` to 0 did **not** touch the arms' horizontal follow, and it introduced a
regression instead: **looking up and down made the view shift forward and backward.**

The log says why, and the class name is the whole answer:

```
LookAtControl_Camera found obj[54363] 'LookAtControl_Camera'
class 'SkelControlSingleBone' @ 0FAD6800, ControlStrength reads 1.000
```

It is a **`SkelControlSingleBone`**, not a `SkelControlLookAt`. A single-bone control
overrides one bone's transform, and the bone this one is named for is the camera's -
`camera_jnt`, which this same session established is a bone ON the first-person mesh that
the camera POV rides (`DishonoredCamera_AnimDriven` is entry 0 of the non-additive core
group). So this control belongs to the **arms-drive-the-camera** direction, not the
camera-drives-the-arms one. Zeroing it stops the camera bone being placed, and the view
starts sliding fore and aft with pitch - exactly what was seen.

`[Camera] ArmLookAtStrength` is kept as a lever because the finding is worth being able to
reproduce, but it ships **-1 (off)** and should stay there. `[Hands] CameraLookAtStrength`
writes the same field and is equally dangerous; that it has been dead behind the hand-mesh
gate for a dozen builds is luck, not design.

**What this leaves for yaw.** Both remaining candidates from the previous entry are now
spent: the settings weights and the influences own pitch, and this control owns the camera.
Nothing found so far reaches the arms' horizontal follow. That makes the third reading in
the entry below the likely one - **yaw is structural**: the viewmodel is drawn in camera
space with no world matrix anywhere in the constant map, and the player's body yaws with
the view because in a flat game that IS the correct behaviour. If so, no field will switch
it off, and VR-30's remaining work is the one its own pass criteria describe - placing the
hands by controller within that frame - which is the hand drive's job, not the camera's.

## PITCH AND YAW ARE TWO DIFFERENT MECHANISMS (VR-30, 2026-09-05, headset)

Three headset runs on the same build family, and the result is a clean split nobody
predicted: **the arms' vertical (pitch) follow and horizontal (yaw) follow are owned by
different things, and only pitch has been switched off.**

| run | lever | what the headset saw |
|---|---|---|
| 1 | the game's own `DishonoredCamera.ini` `m_fDefaultWeight=1.0` | no change at all |
| 2 | `[Camera] ArmFollowWeight=0` (force the settings struct) | arms **trail** the view on both axes - the cursor outpaces them |
| 3 | `[Camera] ArmDisableWeight=1` (force the influences) | **vertical follow GONE**, horizontal unchanged |

### What each run measured, and it is the log that says so

**Run 1 - the config route is inert.** `m_fDefaultWeight=1.0` on
`DishonoredCamera_DisableArmFollow` left the live influence reading `weight 0.000,
target 0.000`. The class is `config(Camera)` and the ini section exists, but that value does
not reach the live object. Restored to stock; do not spend another session on it.

**Run 2 - the settings struct is a race we only half win.** The log shows
`m_ArmFollowWeight` and both `_Rot_` weights oscillating **1.000 <-> 0.000 between
dispatches**: our write lands, the engine's recompute lands right behind it. Roughly half
authority, and half authority is exactly what "the arms trail the cursor" looks like - they
are still pulled toward the view, just weakly enough to converge instead of track. Writing
at a higher cadence cannot fix this; the recompute runs between our dispatches whatever we
do.

**Run 3 - the influences own PITCH, and they are not what `m_ArmFollowWeight` reads.**
Forcing both `DisableArmFollow` influences to `m_Weight = m_TargetWeight = 1` with
`m_bActive` set removed the vertical follow completely and held it. **And
`m_ArmFollowWeight` still reads ~1.000 throughout** - so the recompute does NOT derive that
field from these influences, which is what the second commit assumed. The influence acts on
something else, and the pitch-only result points at the item aim additive:
`DisTweaks_InventoryItem_PlayerSpecific` declares `m_ArmPitch_OffsetKeys` (a per-item curve
mapping camera arm pitch to an arm pitch difference) and `m_bUseItemAimAdditives`, consumed
by `DisAnimNodeBlendItemAimAdditive`. That is a **pitch-named** channel, and pitch is what
went away.

### So what owns YAW

Not established. The candidates, cheapest first:

1. **`m_ArmFollowWeight` itself.** Run 2 moved yaw (to trailing), so this field does reach
   the horizontal channel - it just cannot be held against the recompute from this lane.
   Running both levers together is the free test: if yaw trails while pitch stays fixed, the
   two channels are confirmed separate and each lever's owner is named.
2. **`LookAtControl_Camera`.** Attempt 5 found three Arkane look-at controls on the player
   rig (`LookAtControl_Camera/_LeftHand/_RightHand`) and the camera one "aims the arms at the
   view". `SkcRotApply` already writes its `ControlStrength` from
   `[Hands] CameraLookAtStrength` **every dispatch** - the right cadence - and the key ships
   at 1.0. **It has never been judged, because it is dead on a shipped build**: `SkcRotApply`
   returns on `!g_skcDrive`, and `g_skcPlayer[]` is only populated by `SkelControlProbe`,
   which runs below `ApplyHandToMesh`'s `if (!g_handMesh) return`. Reaching it needs either
   `GamepadOnly=0` (which turns on a dozen other things at once) or hoisting the discovery
   above that gate. **Note the hazard before touching it**: 32.6 records this as "the single
   most dangerous store in the DLL" - writes into a freed AnimTree after `CollectGarbage`
   corrupted the heap, which is why `SkcAlive`/`GraftEmergencyRestore` exist.
3. **Structural camera-space placement.** ENGINE_NOTES' original root cause says the
   viewmodel is drawn in camera space with no world matrix anywhere in the constant map. If
   yaw is structural rather than a weighted additive, no field will switch it off and the
   answer is the hand drive placing the arms in a frame we choose.

Reading 1 and 2 together: the arms are attached to a body that yaws with the view, while
pitch is applied as an additive on top. That would make yaw *correct* engine behaviour - your
body does turn - and make the remaining work "place the hands by controller within that
frame" rather than "stop the yaw", which is what the hand drive is for.

## The arm-follow API, verified against the corpus (VR-30, 2026-09-05)

Verified this session by reading `tools/uscript/` directly, correcting and extending the
2026-09-02 research entry further down. **Still declaration-level: nothing has been read off
a live object yet.** The probe that does that ships in this same commit.

### What the game declares

`DishonoredPlayerCamera.m_DishonoredVTSettings` (`:107`) is a `DishonoredVTSettings` - a
struct declared in **`DishonoredCameraInfluenceGroup.uc:11`, not on the camera** - and the
camera's defaults ship **arm follow fully ON**: `m_ArmFollowWeight`,
`m_ArmFollowWeight_Rot_Primary/_Secondary` and `m_ArmFollowOffset_Weight_*` all 1.0, every
rotator zero (`:171`).

Four `DishonoredCameraInfluence` objects exist purely to turn it off, held both as named
pointers (`:111-114`) and as entries **2-5 of `m_Influences[]` in group 3** (`CG_DISABLE_GROUP`,
`:200-203`) - the last four entries of the last group, so they act as a final gate rather
than contributing a POV delta. Each carries `m_Weight`, `m_TargetWeight`, `m_bActive`,
`m_TransitionSpeed`, and both DisableArmFollow classes ship `m_fDefaultWeight=0.0`.

`DishonoredCameraInfluence` also declares `CAM_BLEND_INSTANT = 1000000.f` - the game's own
idiom for "snap, do not blend", i.e. what to put in `m_TransitionSpeed` to make a weight
change land the same frame.

### Three findings that constrain any approach

1. **There is no script-level way to activate an influence.** Nothing in the corpus writes
   `m_TargetWeight` or `m_bActive` - not once. Every influence class is `native(Camera)` with
   only `var` declarations and `defaultproperties`; not one of the 20 has a single function.
   The blend math and the activation triggers are C++ and cannot be read here.
2. **Nothing in the game turns arm-follow off from script either.** The only references to
   these classes anywhere are declarations and CDO array entries. There is no in-game state
   (aiming, sheathing, a cutscene) whose script turns it off, so **there is no free A/B to
   copy** - a hope the previous entry raised and this one closes.
3. **The coupling runs in BOTH directions.** `camera_jnt` is a bone on the first-person mesh
   (`DisTweaks_PlayerPawn_Camera.m_CameraBoneName`, `MatineePreviewPlayerPawn`,
   `DishonoredNotify_CameraPitchTarget`) and `DishonoredCamera_AnimDriven` is entry 0 of the
   NON-ADDITIVE core group: arms animation drives the camera POV, and then the camera POV
   drives arm follow. Breaking only the second direction may not be enough, and AnimDriven is
   the influence that owns the first.

### Corrections to the 2026-09-02 entry

- `m_UsedMaterials` is **not** beside `m_Offset`. It is one class up on
  `DisSkeletalMeshComponent`, with material indices and shown flags, not a viewmodel
  transform. The earlier dismissal as an arm-suppression route was unsupported:
  see the VR-31 research review below. Suitability depends on the player mesh's
  actual section layout and native behavior.
- Nothing in the corpus ever **writes** `m_Offset`, `m_bUseFOV` or `m_FOV`, and none appears
  in any `defaultproperties`. So the coordinate space of `m_Offset` **cannot** be derived from
  script and must be measured the way `camera postest` measures.
- "Primary/Secondary" is `EDisEquipUsage` (`_Primary=1`, `_Secondary=2`) - an equipped slot,
  most likely weapon vs offhand power. **It is not handedness.**

### The config route, which costs nothing to test

`DishonoredCamera_DisableArmFollow` is `config(Camera)` and `m_fDefaultWeight` /
`m_TransitionSpeed` are `var config`. The game's own
`Documents\My Games\Dishonored\DishonoredGame\Config\DishonoredCamera.ini` **already carries
the section**, shipping the weight at 0:

```
[DishonoredGame.DishonoredCamera_DisableArmFollow]
m_fDefaultWeight=0.000000
m_TransitionSpeed=2.000000
```

Setting that to 1.0 is a complete test of the mechanism with **no code, no build and no
install**. `_Secondary` is a separate class and needs its own section
(`[DishonoredGame.DisCamera_DisableArmFollow_Secondary]`) - it does not inherit the parent's.
It may do nothing, if `m_fDefaultWeight` is only a reset value and C++ drives `m_TargetWeight`
over the top; that outcome is itself the measurement, and it is what the probe distinguishes.

### Two other levers this turned up

- **`SetSkelControlActive(bool)` and `SetSkelControlStrength(float, float)` on
  `SkelControlBase`, plus `FindSkelControl(name)` on `SkeletalMeshComponent`, are
  script-callable natives** - the only documented callable API in the whole chain. Control
  names come from `DisTweaks_PlayerPawn_Camera.m_LookAtControlName_Camera/_LeftHand/_RightHand`.
  This is the same `LookAtControl_Camera` that attempt 5 found and that
  `[Hands] CameraLookAtStrength` already dials - but that key is dead on a shipped build
  because `GamepadOnly=1` clears `g_skcDrive`.
- **`IgnorePlayerAimOffset(bool)`** is an exec with a real script body that sets
  `m_bIgnoreAimOffset`, a plain bool in the pawn's `m_Debug_Player` struct
  (`DishonoredPlayerPawn.uc:214-228, 361`). Aim-offset is the additive that pitches the arms
  with the camera, so it is the closest script-visible analogue to disabling arm follow, and
  it is trivially pokeable.

## Head coupling of the arms (the open problem; roadmap D5)

Root cause as established: Arkane draws the first-person view model in camera space; there
is no world matrix for the arms anywhere in the constant map, so the rig's placement is
defined by the camera. Six attempts, in order:

1. Bone-bank writes in memory (`SpaceBases` +0x208 / `LocalAtoms` +0x214): no visible effect
   at any rate; the renderer keeps its own copy (`SbApply/SbTick`, legacy).
2. Render-time bone-constant drive: bone palettes arrive as VS constants at c6, 3
   registers/bone; sizes x36 = sword, x144 = arms, x204 = NPC. Moved the sword, never
   decoupled it (`rtd_drive`, legacy).
3. The deciding test (`g_rtdLockTest`): apply only `gain * (head yaw since neutral)` and sweep
   the gain: no gain held the weapon still. Conclusion: no seam between the arms and the
   camera downstream.
4. VR hands (shipped for the weapons): hide the engine's arms and draw our own OBJ meshes at
   the controller pose (`core/gfx/hand_mesh`, `vrhands\*.obj`); tracking is 1:1 by
   construction.
5. SkelControl route: world-space translation works and is absolute (the pin test: you can
   walk away from your hands) but throws away the authored resting pose; world-space rotation
   does not work (9,000 writes/s into BoneRotation outrun the recompute and the hands still
   rotate with the head). The real culprit: three controls exist, `LookAtControl_LeftHand`,
   `_RightHand` and `_Camera`; the camera one aims the arms at the view and was untouched for a
   dozen builds. Dial: `g_skcCamStrength` (ControlStrength 0 = off).
6. The donor graft (newest lane): a pristine `SkelControlSingleBone` from the player rig's
   archetype template (`Ply_Player_at`) spliced onto a live hand control's empty NextControl
   (+0xac). Residual coupling is upstream of the grafted bone; the coefficient could not be
   pinned, so `[Hands] GraftHeadFollowYaw/Pitch` (default 1.5, a guess) compensate:
   `cmd -= HC * (hmdNow - hmdAtZero)`. Safety: `SkcAlive`/`GraftDonorAlive`/
   `GraftEmergencyRestore` because writes into a freed AnimTree after `CollectGarbage` on a
   save load corrupted the heap.

Motion sickness note: position following the controller while orientation follows the head
is two contradictory cues on one object; F9 kills the whole drive for that reason.

## The OpenXR presentation (the open problem; roadmap D3, see XR_HANDOFF)

Both backends share the pipeline; only the pose source and delivery differ. The XR path
(builds 37.3-38.92) accumulated mutually exclusive theories, all still selectable:
`[Screen] RigidScreen`, `EyeCant` (measured identity, not the warp), `WorldScreen`
(geometrically correct, experientially wrong), `OverlayScene` (rejected 38.0), `XrCylinder`
/ `[VR] XrLayer` (cylinder "weird wrapped around"), `[VR] XrPoseDelay` (0-3), `StampFix`
(needs `dxvk_vr_view`, absent), `StampLive` (default 1: stamp the head pose located THIS
submit; the content already carries the head rotation, so a stamp that disagrees makes the
compositor cancel the motion: "I can look up and down for a few seconds after load, then it
locks" = the seconds before head injection starts). `[VR] FpsCap` pins the game to the
display rate because fps wandering 66-80 against 72/90 Hz was the measured stutter cause.
XR-3 architecture: a detached pace thread owns every runtime call (VDXR raced a haptic call
from the render thread and trashed the heap); the game thread only publishes eye textures.

## FovLever and the render size are ONE setting (2026-09-01)

Restoring GingasVR's tuned values after a detour explained the "uncanny" report, and the
refactor is not implicated: the frustum-fill block in `eye_quads.cpp` is byte-identical to
the original single file at `48766c07` - same clamp, same tan-linear UV mapping, same guard.
Everything that regressed was a **config value changed in session 2**.

Her tuned values, from the pre-session-2 ini backup: `RenderWidth/Height = 4032x2268`
(16:9), `SpoofDesktopW/H = 4096x2304`, `FovLever = 130`, `[PosTrack] Scale = 50`. Session 2
went to 2750x2850 with `FovLever = 100`.

**The lever and the render size are a pair.** With `XrFrustumFill = 1` (default since
38.13) the quad corners are clamped to `+/- tan(FovLever/2) * ScreenDist`:

| lever | clamp limit at D=1.6 | vs a Quest 3 frustum edge (~2.05 m) | result |
|---|---|---|---|
| **130** | 3.43 m | outside | **no clamp** - the quad fills the eye and samples the middle ~60% of a wider render. Edge to edge, nothing for reprojection to drag. |
| 100 | 1.91 m | inside horizontally, outside vertically | the clamp fires on one axis only: an **asymmetric border** returns, and with it the residual warping 38.13 exists to remove |

**Confirmed in an image, 2026-09-02.** `dump eyes` at `FovLever=100` shows the world inset in
the eye render target with a black border on all four sides - roughly 9% left, 10% top, 10%
bottom. Against the simulated Quest 3 frustum (l -54, r +44, u/d +/-55 deg) the clamp limit
at lever 100 is 1.91 m while the frustum reaches 2.20 m horizontally and 2.29 m vertically,
so it fires on every side. That border is the artifact, and it is what the tester described
as "the render square is at the top left of my vision and I can only see part of it".

So the design is: **render WIDER than the headset shows and let the fill crop**. Session 2
lowered the lever using the *authored quad* subtense formula (`W = 2 D tan(fov/2) * fill`,
`H = W / aspect`), which the frustum-fill branch overrides entirely - correct arithmetic
applied to the code path that was not running. Her 16:9 render also gives a per-eye half of
2016x2268 (aspect 0.889), close to a Quest 3 eye's 0.928; the square renders gave 0.518.

**Baseline restored** (her process rule 2: build on a snapshot confirmed good). The game
ini and all four AppCompat buckets are set to 4032x2268 to match, `PinBackbuffer=1` is kept
because it is the mechanism that makes that size hold on modern hardware, and the tester's
own hand-calibration neutrals are kept. `[PosTrack] Scale` is back to her 50 - the measured
100 below stands as a derivation and should be re-applied as a SINGLE change once the
baseline is confirmed, not bundled with the restore.

## The three rendering symptoms, and the one geometry that ties them (2026-09-02)

Measured from the 03:03 headset run at 4032x2268 requested / `capture: 3840x2160` actual,
`eye render targets: 2496x2688`, and the tester's report at those settings.

### 1. "Super pixelated, but the pause menu is huge like it's at full resolution"

**Both halves of that sentence are the same fact.** The frame is a side-by-side pair, so the
WORLD gets half the frame width per eye - `capture: 3840x2160 (per eye 1920x2160)` - while a
menu frame is MONO and each eye samples the **whole** 3840 across the same quad. The menu is
therefore drawn at exactly **twice** the horizontal sampling density of the world. The
tester's A/B is the cleanest possible confirmation of the SBS packing, and it is not a bug.

**What it costs**: 1920 per-eye columns are stretched across an eye render target 2496 wide,
a 1.30x upscale before the compositor's own resampling. To reach 1:1 the FRAME must be at
least twice the eye width: **2 x 2496 = 4992 columns**. At 3840 the world is at 77% of the
panel, which is what "really low resolution" is.

### 2. The fisheye is `FovLever`, and the code says so

`FovLever` does not only size the quad - **it writes the game camera's FOV**
(`fov_lever.cpp`, `LevWrite` into the camera's FOV field). At `FovLever=130` the game renders
a **130 degree horizontal** rectilinear frame, and the log confirms it:
`quad/fill: world scale is set by the MEASURED render FOV (fork dxvk_vr_proj) = 130.0 deg`
(129/130/137 across the run).

A 130 degree rectilinear render shown across a ~94 degree headset frustum stretches the edges
hard. That is the fisheye. The original author anticipated exactly this - `frame_hooks.cpp`
carries a safety net that disarms the lever if the rendered FOV overshoots the target, whose
comment is *"disarm rather than leave the user in a fisheye"*.

### 3. Why the black bottom border cannot be tuned away at 16:9

The quad's vertical subtense is `2*atan(tan(fovDeg/2)/frameAspect)`, so filling this rig's
99 degree vertical frustum requires:

| frame aspect | lever needed to fill 99 deg | horizontal render FOV | edge distortion |
|---|---|---|---|
| 1.778 (16:9) | **128.6** | 128.6 deg | severe - the current fisheye |
| 1.333 (4:3) | 114.6 | 114.6 deg | moderate |
| 1.25 (5:4) | 111.3 | 111.3 deg | moderate |
| 1.036 (near-square) | **100.5** | 100.5 deg | mild - the right answer |

**So the fisheye and the black border are the same setting pulling in opposite directions,
and at 16:9 there is no value that satisfies both.** The tester found this empirically:
"I couldn't find a balance where the scaling felt right where I couldn't see the bottom black
border". A taller frame is not a preference, it is the only way out.

**But taller costs sharpness**, because per-eye width is `frameWidth/2` (symptom 1). The two
constraints together want a frame that is both wide and tall:

| candidate | aspect | lever | per-eye | vs the 4992 needed for 1:1 | MP |
|---|---|---|---|---|---|
| 3840x2160 (now) | 1.778 | 129 | 1920x2160 | 77% | 8.3 |
| **3840x2880** | 1.333 | 115 | 1920x2880 | 77% | 11.1 |
| **4096x3072** | 1.333 | 115 | 2048x3072 | 82% | 12.6 |
| 4992x4800 (ideal) | 1.040 | 100 | 2496x4800 | 100% | 24.0 |

`3840x2880` is the cheapest step that buys the taller ratio at no sharpness cost - same
per-eye width, +33% pixels, and it drops the lever from 129 to 115, which is where the
fisheye should visibly ease. That is the next single change.

### Two corrections to session 4c

- **The mode-list claim was too strong.** 3840x2160 WAS honoured (`capture: 3840x2160`), so
  "must be a real display mode" is not the rule. The rule is narrower: **`PinBackbuffer=1`
  causes the crop**, because it forces the device to a size the game is not rendering at. A
  size the game will not accept merely falls back to one it will, which is harmless as long
  as the pin is off. Both effects were present at once at 2850x2750, which is what made them
  look like one.
- **There is no 2560x1440 cap.** That was read from the previous run's log, before the pin
  was turned off. 4032x2268 is still not honoured (its `setres` replies come back empty), so
  the requested number and the achieved number are different things: **trust `capture:`.**

## FovLever IS the vertical fill lever for a 16:9 render (2026-09-02)

Correcting session 4's own mistake, and completing "FovLever and the render size are ONE
setting" with the arithmetic that section was missing.

**The frustum-fill branch derives its VERTICAL extent from the frame aspect**
(`eye_quads.cpp:233-242`): `tanC = tan(fovDeg/2)`, `tanCv = tanC / aspect`, and the quad's
top/bottom are clamped to `+/- tanCv * D`. `fovDeg` is the measured render FOV, but the
lever raises it through the zoom floor: `if (fovLever >= 40) fovDeg = max(fovDeg, fovLever *
ZoomFillFloor)`. With `ZoomFillFloor=1.00`, **FovLever sets `fovDeg` outright** whenever it
exceeds the render FOV.

At `D=1.6` against this rig's frustum (x -2.202..+1.342, y -2.285..+1.546):

| render | aspect | lever | tanCv | vertical clamp | result |
|---|---|---|---|---|---|
| 2850x2750 | 1.036 | 100 | 1.150 | +/-1.84 | fills; the near-square frame carries the height |
| 3840x2160 | 1.778 | 100 | 0.670 | +/-1.07 | **67.7 deg of a 99 deg frustum - letterboxed** |
| 4032x2268 | 1.778 | **130** | 1.206 | +/-1.93 | edge to edge top and sides, ~9% black at the bottom |

**So a 16:9 render needs lever 130 and a near-square render needs lever 100.** They are not
independent, which is what the section title always said - this table is the missing half.
GingasVR ships 4032x2268 WITH FovLever=130 for exactly this reason.

**Session 4's error, recorded so it is not repeated.** The known-good restore set
`FovLever=100` (correct: it was paired with the near-square 2850x2750), and session 4c then
moved the render to 16:9 **without moving the lever**. That is the worst pairing available
and it produced the tester's "rectangular again so it didn't fill my view". Changing the
render aspect without changing the lever is not a one-variable change; the pair is the
variable.

**Second finding from the same run: the requested resolution was not honoured.** The mod
asked for 3840x2160 and the log records `capture: 2560x1440`. With `PinBackbuffer=0` this is
harmless - the device is created at what the game actually asked for, so buffer and content
agree and there is no crop - but it means **this rig will not render above 2560x1440**
(desktop 5120x1440, i.e. the monitor height caps it). Every "4032x2268" or "3840x2160" run on
this machine is really a 2560x1440 run. Anything derived from the requested number rather
than the logged `capture:` number is wrong.

## THE RENDER MUST BE A REAL DISPLAY MODE (2026-09-02) - the injected-mode crop

**This is the root cause of "tiny and in the top left corner", and probably of the project's
central open bug.** Six capture dumps on this rig, measured by non-black bounding box:

| requested buffer | actual content | is it a real display mode? | verdict |
|---|---|---|---|
| 1600x900 | 1600x900 | yes | **FULL** |
| 2560x1440 | 2560x1440 | yes | **FULL** |
| 3840x2160 | 3840x2160 | yes | **FULL** |
| 4032x2268 (GingasVR's own) | 3024x1440 | no - injected | CROPPED |
| 2750x2850 | 2750x2200 | no - injected | CROPPED |
| 2850x2750 (this rig's "known good") | **2560x1440** | no - injected | CROPPED |

**The game renders into the TOP-LEFT of the buffer and leaves the rest black.** At
2850x2750 the content is exactly 2560x1440 - 89.8% of the width, **52.3% of the height**.
Everything downstream then works on a frame that is half empty:

- **"tiny"**: the eye quad maps the WHOLE 2850x2750 frame onto itself, so the 2560x1440 of
  real content covers only ~90% x 52% of it.
- **"top left"**: the content is literally in the top-left of the frame.
- **"the eyes will not fuse"**: the SBS split assumes the halves meet at x=1425. The game
  drew its stereo pair inside 0..2560, so the halves actually meet at **x=1280**. The left
  eye is handed 0..1425 (its own view plus 145 px of the right eye's) and the right eye
  1425..2850 (the tail of the right view plus 290 px of black). Those cannot fuse, and no
  amount of separation, convergence or FovLever tuning can make them.

**Why the mod does not notice.** `PinBackbuffer=1` forces the DEVICE to 2850x2750 at
CreateDevice while the game keeps rendering at the size it asked for (the log records the
request: `CreateDevice the game asked for 2560x1440 windowed=0`). The mod then spoofs
`GetClientRect` to report 2850x2750, and the setres path READS THAT BACK and concludes
`setres: the game is already at 2850x2750 - skipping the resolution script entirely`. That
check cannot fail its own hypothesis: it is reading our own spoof. So the engine-side setres
that would genuinely resize the render never runs, and `capture: 2850x2750` is logged for a
frame that only holds 2560x1440 of picture.

**`PinBackbuffer` is not GingasVR's.** Her own tuned ini (`dishonored_vr.ini.pre-2750`) has
no `PinBackbuffer` line at all - it defaults to 0. The key was added by this project. Every
ini since carries `PinBackbuffer=1`, which is when the crop appears.

**Why this is probably "works only on her PC".** 4032x2268 is not a standard display mode
either, and it cropped to 3024x1440 here. Whether an injected mode is honoured depends on the
machine's GPU, driver and desktop mode - this rig's desktop is 5120x1440, and two of the
three cropped captures came back exactly 1440 tall. A user whose display happens to accept
the injected mode sees a correct image; everyone else gets a frame with content in one
corner and eyes that will not fuse. That is the reported shape of the central bug, and it
predicts that the affected users' desktop height is smaller than the requested render height.

**The fix is to request a resolution the display actually offers.** Applied for testing:
`RenderWidth/Height = 3840x2160`, `SpoofDesktopW/H = 3840x2160`, `PinBackbuffer=0`, with
`DishonoredEngine.ini` and all four `[AppCompatBucket1..4]` moved to 3840x2160 as well.
3840x2160 is 16:9 landscape (so the fork splices), measured FULL on this rig, and gives a
per-eye half of 1920x2160 = **aspect 0.889** - the same per-eye aspect as GingasVR's
4032x2268, which ENGINE_NOTES "FovLever and the render size are ONE setting" identifies as
the number that matters. 2560x1440 is the guaranteed-safe fallback: identical per-eye aspect,
a quarter fewer pixels, and the game asked for it itself.

**Instrument that should exist and does not.** Nothing compares the captured frame's real
content extent against the buffer size. A cheap non-black bounding-box check on the capture,
logged once per resolution change, would have caught this on the first run instead of the
fourth session. The setres check must also stop reading the mod's own `GetClientRect` spoof.

## MenuFillScale pumps the world size during GAMEPLAY (2026-09-02)

**Measured, not theorised**: the headset log of 2026-09-02 02:23 (VirtualDesktopXR, Quest 3,
the restored known-good ini). The tester reported "still rendering tiny and in the top left
corner". The log says exactly why, on one line, twice over:

```
quad: fov=100.0 fill=0.60 frameAspect=1.036 W=2.288 H=2.208 D=1.60 -> subtends 71.1 x 69.2 deg
quad: fov=100.0 fill=1.00 frameAspect=1.036 W=3.814 H=3.680 D=1.60 -> subtends 100.0 x 98.0 deg
```

40 rebuilds at `fill=0.60`, 6 at `fill=1.00`. The run ENDED at 0.60.

**The frustum it has to fill** is `L[-1.376 0.839 -1.428 0.966]` (tangents), i.e.
**94.0 deg horizontal x 99.0 deg vertical** (left 54.0, right 40.0, down 55.0, up 44.0). A
71.1 x 69.2 deg quad inside a 94 x 99 deg frustum covers about **half its solid angle**.
That is the whole of "tiny" - it is not a resolution, adapter, scale or convergence fault.

**The mechanism.** `eye_quads.cpp:134` applies `[Screen] MenuFillScale` (0.60 in the shipped
and known-good ini) whenever `g_menuOpen || g_inMenu || g_sbsMonoNow`, and `:204` skips the
`XrFrustumFill` branch on exactly the same condition. So one flag decides BOTH the size and
which geometry path runs. `present.cpp:19-41` then forces a quad rebuild whenever either
flag changes. The result is that the world size **pumps** as the flag flaps:

| t (ms) | fill | subtends |
|---|---|---|
| 52270750 | 1.00 | 100.0 x 98.0 |
| 52279406 | 0.60 | 71.1 x 69.2 |
| 52280593 | 1.00 | 100.0 x 98.0 |
| 52281328 | 0.60 | 71.1 x 69.2 |
| 52283375 | 1.00 | 100.0 x 98.0 |
| 52285125 | 0.60 | 71.1 x 69.2 |

Those are all AFTER the game reached gameplay (`crouch/raw` from 52273875 reports
`menu=0/0 ... mono=0`, the pawn moving, `pos=(6402,5110)`).

**It is the MENU flag doing it, not the splice counter.** `sbs:` logs on every change of
`g_sbsMonoNow` and its last transition is at 52263406 - before any of the pumping above. So
`g_sbsMonoNow` was steady while the fill alternated, which leaves `g_menuOpen || g_inMenu`
as the only remaining term. `present.cpp:613-616` already records why that flag is
untrustworthy in gameplay: the game's `Req_SaveSlotInfos` save-slot polls re-open it. The
38.x fix taught the MONO decision to prefer the splice counter over the menu flag for that
exact reason - but the **fill** decision and the **frustum-fill gate** were never given the
same treatment, so both still trust the flag the fork's counter was brought in to replace.

**Why it never showed on the Index.** Under OpenVR there is only ONE geometry path: the
authored quad. `MenuFillScale` shrinks a menu, which is what it is for, and there is no
second path to jump to. `XrFrustumFill` (38.13) added a second path for the OpenXR port
without making the transition between the two continuous, so on Quest the same flag flap
that merely dimmed a menu on the Index now swaps the entire quad construction mid-gameplay.
This is a good example of the class the mod's author flagged: SteamVR/Index was the tuned
target and OpenXR/Quest was a later port.

**The placement, worked from the logged frustum.** At `fill=0.60`, `D=1.60`:
`ccy = D*0.25*sum(vertical tangents) = -0.3696 m`, `ccx = 0`, `W=2.288`, `H=2.208`. Against
the left eye's frustum cross-section at D (x -2.2016..+1.3424, y -2.2848..+1.5456):

- **vertically: 21.2% black top, 57.6% world, 21.2% black bottom - symmetric.**
- horizontally, LEFT eye: 29.8% black on the temple side, 64.6% world, 5.6% on the nasal
  side; the right eye is the mirror. That per-eye asymmetry is the rigid-screen design
  (`:167-176`), not a fault, but it is why a small image reads as displaced sideways.

**This is a falsifiable prediction, and it contradicts the standing session 3c reading.**
Session 3c recorded a `dump eyes` showing the world in the "top ~54%, bottom half black".
The geometry above says the vertical border must be SYMMETRIC and about 21% on each side.
The next `dump eyes` decides it: symmetric borders confirm this model and retire the "sits
high" contradiction as a misread dump; a genuinely black bottom half falsifies it and means
something flips or crops vertically between the quad and the eye texture.

**Fix applied for testing (config only, no build)**: `[Screen] MenuFillScale=0.60 -> 1.00`,
which makes the menu-flag branch produce the same 100.0 x 98.0 deg quad as gameplay, so a
flap can no longer change the world size. Cost: menu edges crop, which is what 32.4 added
MenuFillScale to avoid. The proper fix is to stop letting the menu flag gate world geometry
at all - give the fill and the frustum-fill gate the same splice-counter-first rule the mono
decision already has - and that needs a build.

## World scale: 50 UU/m is a default, not a measurement (2026-09-01)

`[PosTrack] Scale` is 50, UE3's canonical 1 uu = 2 cm. One knob drives both the fork's
stereo separation and positional parallax, live on the F10 View tab ("world scale (uu/m)")
and on PgUp/PgDn (**PgDn = world smaller**, 5% a press).

**Prior art from another UE3 game.** BioShock Infinite's VR mod ships the same canonical 50
as its code default and its own notes say plainly that this is *not* the answer - "the true
value is the user's headset calibration". The value that came out GREEN in the headset there
was **150 UU/m**, three times canonical (source: the bioshock-trilogy-vr tree,
`docs/bioshockinfinite/ENGINE_NOTES.md` and `ROADMAP.md`, session s41 "world scale tuned to
150"). Its notes also carry the warning that applies here: BS1/BS2's calibrated 100 must
never be copied across, because it is a different engine. The same caution applies to
Infinite's 150 - it is a starting bracket for Dishonored, not a value to adopt.

So the first headset attempt at Dishonored's scale should sweep **50 -> 150**, not the
narrow 50 -> 74 that inverting GingasVR's static `dxvk_stereo.txt` marker (`sep=0.014
conv=140`) suggests. That inversion assumes the FOV he tuned at, which is unrecorded.

**Measured 2026-09-01: the separation write is honoured.** `depth:` logged
`sep asked 0.01397, fork reads back 0.01397` across a live sweep of 50 -> 73.9 UU/m
(eyes 3.16 -> 4.66 uu apart, sep +48%), and the tester reported no change in apparent world
size at all. So the proxy -> fork chain is healthy and **separation is not what makes the
world feel huge**. The likely reason it changes nothing perceptually: at convergence 140 UU
(~2.8 m at 50 UU/m) nearly everything in a room is past the distance where disparity still
carries size information, so angular size dominates - and angular size is pinned to 1:1.

### MEASURED: Dishonored is 100 UU/m, 1 uu = 1 cm (2026-09-01)

Derived by the movement-constant method below, from a walk-then-sprint run read off the
crouch diagnostic's `spd=` plateaus. Four clusters, all uncrouched: 124, 246-250,
**355-363 (14 samples)** and **537-545 (21 samples)**. The two strong plateaus are the
engine constants - **360 uu/s default, 540 uu/s sprint, ratio exactly 1.5** - and the
246-250 cluster is partial analog-stick deflection.

| scale | default | sprint | verdict |
|---|---|---|---|
| 50 (UE3 canonical) | 7.2 m/s | 10.8 m/s | **untenable** - Corvo would sprint at world-record pace and stroll faster than most people can run |
| 78 | 4.6 m/s | 6.9 m/s | fast |
| **100** | **3.6 m/s** | **5.4 m/s** | **a jog and a run - how Corvo actually moves** |
| 150 | 2.4 m/s | 3.6 m/s | a sprint slower than a jog |

Corroborated independently by eye height: 78.1 uu above the pawn origin plus a typical
~88 uu human collision half-height (1 uu = 1 cm) puts the eye at 1.66 m. `[PosTrack] Scale`
default changed 50 -> 100 in `config.cpp`, with the derivation in the comment and in the
generated ini's own help text.

**What this does and does not fix.** It corrects positional parallax amplitude, stereo
depth and hand reach - everything that maps a real metre onto game units. It does **not**
change apparent angular size, which is pinned at 1:1 by the frustum-fill path and is a
different lever entirely (`FillScale`). Do not expect it to make a too-big world smaller.

**How to DERIVE UU/m instead of tuning it by feel.** The Mirror's Edge VR mod (also UE3,
MIT) derives it from the game's own movement constants against known human speeds, and gets
three independent constants agreeing on **100 UU/m, 1 UU = 1 cm** - walk 200 UU/s = 2.0 m/s,
run 380 = 3.8 m/s, sprint 700 = 7.0 m/s (source: its `ENGINE_NOTES.md`, "World scale").
That is a falsifiable method rather than a preference, and it transfers directly: the crouch
diagnostic already prints `spd=NNuu/s`, so one run walking and sprinting in a straight line
gives Dishonored's own number. **UE3 is not uniformly 50 UU/m** - Mirror's Edge measured
100, BS1/BS2 calibrated 100, Infinite tuned to 150. 50 is only the engine's canonical
default, and a scale that is too SMALL makes the world feel too BIG.

Eye height is a weaker second estimate and is currently ambiguous: the crouch log gives
`camZ - pawnZ = 78.1 UU`, which is ~1.56 m at 50 UU/m if that origin is at the feet, but
~2.4 m at 50 (and ~1.6 m at 74) if it is the collision-cylinder centre. Resolve the origin
before trusting it.

**Separation changes DEPTH, not angular size.** With the frustum-fill path presenting the
measured render FOV at 1:1, apparent angular size is already correct by construction and
will not move with this knob. What moves is how far away things read, which is what makes a
correctly-scaled view still feel enormous. Note also that `[Screen] FillScale` (the "screen
fill" slider) is **inert while `XrFrustumFill=1`**, because the frustum-fill branch
recomputes the quad corners and discards the authored W/H.

40.2 logs the whole chain every 5 s - IPD, world scale, the resulting eye separation in game
units, xs, convergence, the separation asked for AND the value read back out of the fork -
so a knob that moves the hands but not the world names which half is dead.

## The "freeze then rescale" was VR injection, not a fault (2026-09-01)

Reported by the tester after the landscape fix: the square flat render appears, the image
freezes for about a second, and then the game is running in VR. That is the bring-up
sequence - the capture, the eye quads and the projection layer all coming up - and it was
only ever read as a glitch because what came out the other side was scaled so wrongly that
it did not look like a successful injection. The freeze itself has no fault behind it. What
remains open is world scale, above.

## THE RENDER MUST BE LANDSCAPE (2026-09-01) - the portrait splice refusal

The cause of "the eyes are super far off and both are zoomed in", and a self-inflicted
regression: session 2's resolution fix set the render to **2750x2850, which is portrait**.

`dxvk/src/d3d9/d3d9_device.cpp`:

- **Line 4381** - the main scene's splice gate ends with
  `else if (!(stVpS.Width > stVpS.Height)) stWhy = "rt-portrait";`
- **Line 4578** - the per-eye splice runs only `if (stWhy == kSplice)`.

So on a portrait viewport the fork **does not splice the main scene at all**. The world is
drawn **mono** across the full frame. The proxy, which has no idea, still hands eye 0 the
left half and eye 1 the right half (`eye_quads.cpp`, `u0 = eye ? 0.5 : 0.0`). Two unrelated
views that cannot fuse, each stretched across the whole quad and therefore magnified 2x.
That is precisely the reported symptom, and it is worse than a black screen because
everything downstream keeps reporting success.

**The splice counter lies about this.** Light shafts, shadows and the M8.1 quarter light
pass splice under *different* conditions (lines 4526-4527, `"mirrored-vp"` or
`"vp!=rt" && stQuarter`), so they keep working. The measured run showed `splices=85` with
the main scene never spliced, which kept `g_sbsMonoNow` false and the half-frame UVs on.
The fork's own log confirms it: only `shaftfix`, `shadowfix` and `M8.1 quarter light pass`
lines, no main-scene splice.

**Line 5996 carries the same landscape gate** on `dxvk_vr_proj`, the projection export:
`pjVp.Width >= 1024 && pjVp.Width > pjVp.Height`. So a portrait render also kills the one
MEASURED source of the rendered FOV. `g_liveFovX` stayed 0 for the entire session, the
frustum-fill path fell back to the ini constant `GameFOVDeg=100` with no log line, and an
assumed number set world scale from start to finish.

**Fix: 2850x2750** - the same two numbers swapped. Identical pixel cost, landscape by
100 px so the gate passes, full-frame aspect 1.036 so the eye quad subtends 100 x 98 deg at
`FovLever=100`, which is right for a Quest 3. `tools/setup-game-ini.ps1` now defaults to it
and its header explains why; keep `Width > Height` for any other value.

**This falsifies session 2's third "corrected belief".** "The eyes are not a stereo pair" was
recorded as disproved on a measurement of 32.7 mean-abs-diff static, 11.5 after a head turn,
read as ordinary parallax. Two different halves of one mono frame produce exactly that, and
the head-turn change is the image scrolling, not parallax. The eyes were genuinely not a
stereo pair. **A large diff between two crops is not evidence of stereo** - the test cannot
tell a stereo pair from two unrelated crops, so it never could have failed its hypothesis.

40.2 adds a proxy-side detector: a portrait capture now logs an Error naming the fork's
refusal reason and the fix, so this cannot be silent on both sides of the boundary again.

## The exit crash: reading the fingerprint correctly (2026-09-01)

Three records in `dishonored_vr_crash.txt`, register-identical, all after `PreExit`. The
session-2 reading of them ("0xDEDEDEDE freed-memory *writes* in d3d11.dll; the detached
pace thread is touching released D3D11 objects") is **wrong in both halves**, and both
errors came from the instrument rather than the engine.

**It is an EXECUTE fault, not a write.** `ExceptionInformation[0]` for an access violation
is three-valued: 0 read, 1 write, **8 execute (DEP)**. The fingerprinter tested it for
truth, and 8 is truthy, so every execute fault in this project's history printed as
"writing". The proof it is 8 is in the record itself: `ExceptionAddress ==
ExceptionInformation[1] == 0xDEDEDEDE`, and the module resolved to `?`. A data write would
have left `ExceptionAddress` at the faulting instruction *inside* `d3d11.dll` and printed
`[d3d11.dll+0x...]`. So EIP itself landed in freed memory - a **call through a poisoned
code pointer** (a freed vtable or callback), which is the opposite failure from a stray
store, and points at a destroyed COM object rather than at unsynchronised context use.

**The faulting thread is not the pace thread.** All three say `tid=... (other)`, and
`thread_name()` returns `"other"` only when the tid matches nothing in the registered
table. Both `present` (registered at `RenderEyesAndSubmit` entry) and `xr-pace` (registered
as the pace thread's first statement) are in that table before any crash can happen. The
direct faulter is a third-party worker - d3d11, the display driver, or the VR runtime. The
pace thread may still be the *cause* (it can outlive an object another thread then calls
through); it is not the victim, and instrumenting it as the victim will find nothing.

**Corollary for the register dump.** `ecx = esi = 0xDEDEDEDE` with `ebx = 0x24` and
`edi = eax - 0x20` is then not "a poisoned `this` being written through" but the poisoned
values still in the argument registers at the moment control was transferred - consistent
with a `thiscall` through a freed object's function pointer, made from
`d3d11.dll+0x4dfcd`'s call site (the return address in `esp[0]`).

Fixed in 40.2: `crash.cpp` decodes all three operations and names the
address-equals-EIP case explicitly.

## Why `dumps\` was always empty (2026-09-01)

Not a permissions or path problem. The crash file holds 3 `EXCEPTION` lines and **0
`minidump` lines**, which is direct proof that `unhandled()` never executed: UE3 wraps
`WinMain` in its own `__try` and installs its own filter, so the fault is consumed before
`SetUnhandledExceptionFilter`'s handler can run. The dump path was unreachable by
construction for the entire life of the project.

40.2 takes the dump from the **vectored** handler instead, which always runs, gated on the
instruction pointer resolving to **no loaded module** (`module_of` returns base 0). That
gate is fatal-only by construction - no `__except` frame can resume a thread whose EIP is
in unmapped or freed memory - so it cannot fire on the first-chance exceptions UE3 raises
and handles deliberately. It is also falsifiable: if the crash ever turns out to be an
ordinary in-module fault, no dump appears, and the wild-EIP reading is disproved by the
silence. `dbghelp.dll` is resolved once at `install()` time, never inside the handler, so
the VEH does not touch the loader lock.

**The original author read this fault correctly and session 2 inverted it.** The 38.79
comments say "EIP dededede after PreExit" and "a call through freed memory"
(`process_event.cpp`, `frame_hooks.cpp`). That is the execute-fault reading, arrived at
without the decode bug getting in the way. 38.79 acted on it by standing the **game**
thread down at `PreExit` - which is correct and necessary, and was never the whole path.

## The pace lane at shutdown (40.2)

38.79 set `g_xrRun = 0` and returned. Nothing waited. The pace thread can be up to 100 ms
inside `xrWaitFrame`, another 100 ms inside `xrWaitSwapchainImage`, or mid `CopyResource`
into `g_xriImg[eye][idx]` - swapchain textures **owned by the runtime**, never AddRef'd by
us, which stop existing when it tears its session down. So the exact fault 38.79 set out to
prevent still had an open path through the lane 38.79 did not close.

`XrPaceStop(why)` now joins with a 750 ms bound. On expiry the thread is **left running on
purpose**: `TerminateThread` would abandon `g_xrCs` held (deadlocking any later publish)
and abandon an acquired swapchain image the runtime is still tracking, which is worse than
the race. The error line is the instrument - a fault *after* it means the pace lane is
still the one to chase; a fault *without* it means the thread was already gone and the pace
lane is not the cause. The event pump's inner `while` also tests `g_xrRun` now, so a
runtime with an event backlog cannot hold the loop past a stop request.

Still not done, and deliberately not bundled in: `xrRequestExitSession` /
`xrEndSession` / `xrDestroySession` are never called. Doing that properly needs the pace
loop's cooperation and is a second behavioural change (rule 1 in the handoff's process
rules). Note also that `PreExit` is a UE3 script event: a kill or a hard crash never fires
it, and the join must NOT be moved into `DllMain(DLL_PROCESS_DETACH)`, where waiting on a
thread under the loader lock is a textbook deadlock.

## `XR_TIMEOUT_EXPIRED` is a success code (40.2)

`XrResult` is negative for failure only, so `XR_TIMEOUT_EXPIRED` (+1) makes `XR_FAILED()`
**false**. The pace loop's `if (!XR_FAILED(g_xrf.wait(...)))` therefore ran `CopyResource`
into an image the compositor had not finished reading - a data race with the runtime on the
one resource the headset displays, invisible because every call returns a success code. Now
tested as `== XR_SUCCESS`.

Same block: `g_xrpShown = seq` used to advance **before** the copies, so any frame lost to
a timeout was dropped permanently rather than retried. It now advances only once both eyes
have actually received the content. The image is still released after a timeout to keep
acquire/release paired - an unreleased image starves the swapchain within a few frames,
which is a hard stall rather than one stale frame.

## The capture cost, measured (2026-09-03, S1)

The mono screen's per-present capture (`core/gfx/capture`) was the known structural cost
and nothing had measured it. Runs 16-19 on the dev PC (simulator lane, 1920x1080 windowed,
the auto-continued save in gameplay, ~85 presents/s), with the `capture: cost/present` line
(one 3 s window each, microseconds per present):

| mode | rtd | lock | copy | upload | blit | total | what it says |
|---|---|---|---|---|---|---|---|
| sync (shipped) | 2 | 2400-3150 | 700 | 1500-1700 | 0 | 4700-5500 | `GetRenderTargetData` returns at once; **`LockRect` is the wait** (the readback is queued behind the frame in flight); the row copy of 8 MB is 0.7 ms (cached), `UpdateSubresource` 1.5 ms |
| deferred, first form (read back the previous copy AND lock it in the same present) | 2 | 2900-3100 | 700 | 1500 | 1 | 5100-5400 | **no gain**: the readback queued this present sits behind this present's rendering too, so the lock waits just the same (run 18) |
| deferred, pipelined (queue the readback this present, lock the PREVIOUS present's) | 2 | **0** | 730 | 1500-1650 | 1 | **2250-2400** | the wait is gone; the picture is one present late (head-locked screen: tolerable; a stereo method's tag travels with the slot) |
| shared (a D3D9 surface opened on D3D11) | - | - | - | - | - | - | **REFUSED** by the device: `QueryInterface(IDirect3DDevice9Ex)` fails (the game calls `Direct3DCreate9`), `CreateRenderTarget` with a shared handle returns `D3DERR_INVALIDCALL`. D3D9 shares only under 9Ex, and a 9Ex device refuses `D3DPOOL_MANAGED`, which UE3's D3D9 RHI depends on, so upgrading the device is not an option either |
| user-memory readback surface (`CreateOffscreenPlainSurface` with the buffer pointer in `pSharedHandle`, to lose the row copy) | - | - | - | - | - | - | **REFUSED**: `D3DERR_INVALIDCALL` (run 17); the runtime does not take a caller's buffer here. Not kept as a mode |

So the cheapest capture this game's device allows is the pipelined `deferred` mode: half
the cost of the shipped path, at the price of one present of latency. It also resolves a
multisampled backbuffer through its `StretchRect`, which is what the run 6
`GetRenderTargetData` failure under the game's AA setting needed. `[Capture] Mode=` ships
`sync` (every new lever default OFF); `capture mode deferred` is the live A/B, and the
headset run decides whether it becomes the default (ROADMAP S1). The remaining 2.2 ms is
the row copy plus the D3D11 upload of 8 MB; a staging-texture map would fold the two into
one and is the next step only if the headset number asks for it.

The instrument that settled it: the phase split. The first cost line lumped lock and copy
together as "copy = 3.5 ms" and read as a slow write-combined memcpy; splitting the lock
out (run 18) showed the memcpy at 0.7 ms and the wait inside `LockRect`, which is what made
the pipelined form the obvious move instead of the user-memory trick.

## The tick budget, measured (2026-09-03, session 8)

The headset ticked at 26-28/s at the Quest 3 size and the capture's cost line could not say who
owned the tick: the `LockRect` wait it counts as "capture" is also the GPU finishing the frame,
so the same numbers fit "the render thread is bound by the readback" and "the GPU is bound by
two 2496x2688 draws". `core/framework/perf` measures the split per present (eight QPC stamps in
hkPresent, the first BeginScene after Present as the frame-start marker, a D3D9 timestamp ring
with a bracket around the readback copy, read five presents back and never waited on) and prints
`perf: tick` and `perf: gpu` every 3 s. Simulator lane, RTX 4060, 2496x2688 VirtualMode, the
sewers save, `stereo reentry` (runs 44-01, 44-03, 44-04; logs in `D:\dvr-data\logs`):

| capture | tick ms | ticks/s | presents/s | CPU capture / present | of which lock | GPU 3D / present | GPU readback DMA / present |
|---|---|---|---|---|---|---|---|
| sync (the 41.0 path) | 46-48 | 21-22 | 43 | 17-21 ms | 9-13 ms | 4.8 ms | 15.5-16.8 ms |
| deferred | 36-37 | 27-28 | 54 | 9.6-10.5 ms (copy 2.8, upload 7) | 0 | 4.8 ms | 10.4 ms |
| off (the control) | pace-bound at the sim's 90 Hz | - | 93 | 0 | 0 | 2.8 ms | 0 |
| shared on a 9Ex device, SharedWait=1 | 13.3 | 75 | 151 | 3.6-3.8 ms (the fence wait) | 3.6 ms | 4.4 ms | 0.2 ms |
| shared, SharedWait=0 (ships) | 11.1, PACE-BOUND (the sim's 11.11 ms) | 90 | 180 | 0.5-0.8 ms | 0.5 ms | 5.0 ms | 0.2 ms |

What the table says: the readback owned the tick on BOTH sides. `GetRenderTargetData` into a
system-memory surface moves 25.6 MB at about 1.6 GB/s, 16 ms of GPU time per present that
serialises with the next frame's draws, plus 7.5 ms of CPU copy and upload; the actual 3D work is
5 ms per draw. `deferred` hides the CPU wait but not the DMA (the GPU still spends 21 ms per tick
copying), which is why it gains only 6 ticks/s. The shared surface (a VRAM-to-VRAM StretchRect
fenced by an event query) removes the DMA and the crossing: the tick drops from 46 ms to the
simulator's pacing limit, with the GPU at 5 ms per present. The headset at 72 Hz should be
pace-bound at the Quest 3 size with about 4 ms of GPU headroom per tick (the render thread's own
work is 3.3 ms per present; Virtual Desktop's encoder shares the GPU and is not in this table).

The confounds the instrument named: the 1080p simulator runs of session 6 (sync 83-90 vs deferred
90-92 presents/s) were PACE-BOUND by the simulator's 90 Hz gate (`wait` 3-6 ms per present), so
"deferred gained nothing" there said nothing about the capture; the menu and the loading screen
are game-thread-limited (`RENDER THREAD STARVED`, idle 18 ms of 18); and the simulator lane shows
a 130-140 ms lock stall every 2-3 s under `sync` at the Quest size (`perf: frame gap ... sat in:
the method (capture)`, lock 125 ms) that the headset run 13 never showed: a simulator-lane
artifact until measured otherwise (VERIFICATION, known simulator defects).

## The creation census: what UE3 asks of D3D9 (2026-09-03, session 8)

`core/gfx/device_census` patches the device's eight creation calls and each resource class's
Lock, and counts what the game asks (run 44-02, the sewers level fully loaded, `device census`):

- 8120 creations, 859 MB asked; **8060 MANAGED (398 MB)**: textures 5217 (DXT1 2564 of them,
  244 MB at the first GAMEPLAY; DXT3/5 463; 8bpp 521; 16bpp 15; 32bpp 12), cube textures 23,
  vertex buffers 1874 (12 MB), index buffers 962 (2.6 MB). DEFAULT: the render targets (21
  float, 18 32bpp, 253 + 101 MB), 5 depth-stencils, one dynamic write-only vertex buffer.
  SYSTEMMEM: the mod's own readback surface. No AUTOGENMIPMAP anywhere.
- **Locks on MANAGED textures: READONLY 10598, write (NOSYSLOCK) 55393, partial 56, level > 0
  45415**; cube textures 774 plain writes; every static vertex and index buffer locked once,
  plain, at creation. The READONLY locks are UE3's mip streaming copying from the old texture.
- The device: `CreateDevice adapter=0 type=HAL flags=HWVP|PURE|FPU_PRESERVE`, the present
  parameters A8R8G8B8, one backbuffer, DISCARD, LOCKABLE_BACKBUFFER, interval IMMEDIATE; caps
  DYNAMICTEXTURES and CANAUTOGENMIPMAP; 4070 MB available.

So a 9Ex device (which refuses D3DPOOL_MANAGED) needs a translation for 99 % of the game's
creations, and the DEFAULT + DYNAMIC stand-in is the wrong one: a READONLY lock of a DYNAMIC
DEFAULT texture reads VRAM through an uncached map and can return garbage after streaming. The
translation that holds is the shadow (below). The census stays on (creation calls are rare, a
lock is one hash lookup); `device census|status` on the seam, `census{}` in status.json.

## The D3D9Ex device and the managed-pool shadow (2026-09-03, session 8)

`[Device] Ex=1` (launch-time; `device ex on|off`, the F10 Display tickbox) makes
`Direct3DCreate9` return an `IDirect3D9Ex` as the game's `IDirect3D9` and `hkCreateDevice` call
`CreateDeviceEx` (a `D3DDISPLAYMODEEX` from the present parameters when fullscreen, NULL when
windowed), falling soft to the plain `CreateDevice` on the Ex object, then to the plain
`IDirect3D9`. Measured on the simulator lane (runs 44-03, 44-04): `CreateDeviceEx -> 0x0`, the
device answers `QueryInterface(IDirect3DDevice9Ex)`, `GetAdapterLUID` 0-d03b, the capture probe
reads `shared surface AVAILABLE` (a 2496x2688 A8R8G8B8 render target opened as D3D11 fmt 87).
No Reset happened on the level load under VirtualMode (the windowed device keeps its size), so
the 9Ex Reset semantics are still unmeasured; a fullscreen Reset that returns INVALIDCALL has
`ResetEx` as its contingency.

`[Device] Managed=shadow` (the default while Ex=1): every MANAGED creation is passed to the
device as DEFAULT (buffers too; they are lockable there), and every translated texture gets a
SYSTEMMEM twin with the same levels and format. The class-wide Lock hooks the census installs
redirect `LockRect`/`LockBox`/`AddDirtyRect` on a translated texture to its twin, `UnlockRect`
pushes the twin's dirty regions to the real texture with `UpdateTexture`, and the last `Release`
drops the twin: what MANAGED did inside the runtime, done in the proxy, so a READONLY lock reads
the twin (every write went through it) and the game keeps its pointer to the real texture for
everything else. Measured: 5240 twins, 65552 unlock pushes, 0 failures, the sewers rendered
intact after minutes of play (`dump capture`, run 44-04). `none` (the refusals are the
measurement), `default` (textures lose their locks) and `dynamic` are the A/B, all behind Ex.
The session's finding for the belief recorded above under "The capture cost, measured": the 9Ex
route IS possible on this game, at the price of the shadow.

## The shared hand-off needs a fence in BOTH directions (2026-09-03, session 8)

A D3D9Ex share carries no keyed mutex, so the proxy fences it by hand. The first build fenced one
direction: a D3D9 event query after the blit, waited on before D3D11 samples the slot. The
headset run 15 then reported the eyes disagreeing "90 % of the time" with every tag instrument
clean, and the other direction was the hole: D3D11's read of a slot is queued, not executed, when
the consumer returns, and the runtime's `xrEndFrame` is what flushes it; two presents later D3D9
blits the NEXT frame - the other eye's - into the same slot, and if the read has not executed
yet it samples that frame. The consumer now ends a D3D11 event query after its draw and flushes,
and the blit into a slot waits (bounded) for that query; the count of blits that found the read
still pending (`readWaits`) is the number of frames that could have carried the wrong eye: 14 in
one simulator run at 90 presents/s, so the race is real, not theoretical. Two slots suffice only
because of this fence; without it, more slots would only have made the swap rarer.

## The one-view state: what the headset logs say, the frame-identity trace, and the pairing that swapped (2026-09-04, session 9)

**The fault as measured (runs 16-17, the user, Quest 3 via VDXR, `Ex=1` + `Mode=shared`)**: after a
level load, from the first arming of the second draw, both eyes show ONE picture (the run-17 dump
pair `eye_3342/3343` differs by 2.5 grey levels with no horizontal shift improving it; the later
pairs from the same run carry 64-96 px of parallax), with every tag instrument clean.

**What the archived logs say, read again before any code was written**:

- The engine's per-present population does not change between the bad and the good state:
  `BeginScene 1.0/present`, SRT 64-75/present, `perf: gpu 3d 6.0-6.4 ms/present`, `draws/s ==
  2nd/s == 72`, the P1/P2 split the same. The only per-window number that moves is the shared
  capture's `blit fence waits`: 41-95 of 432 grabs in the bad state, 172-220 in the good state,
  in BOTH runs. Unexplained; the trace below carries it.
- The dump pair is genuinely one tick's pass 1 and pass 2: `FrameDumpTick` runs in `game_tick`
  before `end_frame`, so `eye_N` holds present N-1's output, and under `shared` (delivery = the
  previous present's slot) those are the backbuffers of presents N-2 and N-1 with their own tags.
  Both were blitted before the dump's stall. 2.5 grey levels is two renders, not one texture
  copied twice (that reads 0.0).
- The `reentry: pair` line is `DVR_LOG_FIRST_N(6)`: its six lines in run 17 are the first six
  pairs of the BAD state, and they read c5 one IPD apart. So the two draws uploaded different
  camera positions while the pictures were the same. The line samples "the last c5 upload before
  this Present" with no per-draw association; the trace ties c5 to the pixels per present now.
- No capture-mode switch in run 17 tripped a gate (no `gates ->` line at any of the eight
  switches). Every switch to `shared` was followed within 1.5-9 s by a pause menu whose resume
  re-armed the doubling; no switch to `deferred` was. The user's "shared fixes it" is confounded
  with a pause/resume; PR #6's "a mode switch trips the no-present gate" is not in the log.
- **`dump eyes` re-armed the second draw by itself**: the PNG encode on the present thread
  stalled 620-660 ms per file, the script camera writes read stale (`viewinject: ... the direct
  fallback is taking the camera back`), the state dropped to LOADING, the gates went SINGLE for
  74 ticks and DOUBLE again. Both "good" pairs of run 17 (4464, 8499) were dumped after such a
  re-arm. The encode is on a worker thread now (the present thread copies 27 MB and returns).
- `drawTid == presentTid` in run 17 was a latch artifact: `g_presentTid` was set once, at the
  first present, which at boot is the game thread's. It follows the presenting thread now and
  logs a change. Every simulator run since reads two threads.

**The frame-identity trace (`core/gfx/frame_id`, `[Perf] FrameId=1`, `frameid on|off|status`)**:
one 64x64 luma thumbnail per present at four stages, keyed by the capture serial of the pixels
(so one frame lines up across the stages whatever the delivery lag), read three presents later
and never waited on, with the c5 of the draw and the camera's right row on the same record:

| stage | where | how |
|---|---|---|
| `bb` | `capture::grab`, right after `GetBackBuffer(0)` | D3D9 `StretchRect(bb -> 64x64 A8R8G8B8 RT, LINEAR)`, `GetRenderTargetData` into a SYSTEMMEM surface, `LockRect(READONLY|DONOTWAIT)` three grabs later |
| `slot` | `reentry::end_frame`, inside the read fence | the delivered slot's SRV drawn into a 64x64 RGBA target by the blit quad, `CopyResource` to staging, `Map(DO_NOT_WAIT)` three presents later |
| `out` | the same, from an SRV of the method's output texture | the same |
| `sc` | the runtime's frame-texture seam, after `CopyResource` into the acquired swapchain image, before its release | `CopySubresourceRegion` of the centre 64x64 into a staging texture of the swapchain format |

Per left/right pair the `stereo: frameid` line (at most once a second) prints the checksums, the
mean absolute luma difference per stage, the same-eye floor (this left against the previous
left), the c5 step of the +1 present from the -1's projected on the right row (the side check),
the picture's own best horizontal shift (the right eye's content must sit LEFT of the left
eye's: negative = a true pair, convention-free), and the first stage that reads as one
picture. A 3 s summary carries the counts; a state change at stage `bb` (ten pairs in a row
below the floor, or above it again) prints once at Warn. Evidence only: nothing here re-arms,
switches or kicks.

Simulator numbers (RTX 4060, 2496x2688, the sewers, runs 45-01..05): a true pair reads
L-R 4.1 at `bb`/`slot`/`out` and 4.7-5.0 at `sc` (a centre patch, not a downscale), the same-eye
floor 1.4-1.5, c5 |d| 6.17 = ipd*scale, picture shift -1 px at 64 wide, busy reads 0 at every
stage, slot repeats 0. Stages `bb`, `slot` and `out` come out byte-identical (one 2x2 bilinear tap
at the same 64x64 centres on both APIs), so a difference between them would itself be a finding.

**The diagnostic words, one per half of the user's remedy**: `reentry rearm [n]` (n gameplay
ticks decided SINGLE at the gate, no tag, no pass 2, then the doubling resumes; the capture
untouched), `capture reinit` (the shared slots, fences and D3D11 views, or the deferred ring,
released and re-created at the next grab, the mode unchanged; one present delivers nothing),
and the existing `stereo projection off` then `auto` for the runtime's quad -> projection
transition the pause path also makes. `capture status` prints the delivered slot and the reinit
count; the beat line counts pass-2 eye writes the camera seam refused (`p2write refused=`, the
camera pointer beside it): a refused write leaves pass 2 drawing from pass 1's camera.

**What the trace found on the simulator (not the headset fault, a second fault)**: the eye tags
SWAPPED against the draws. The method's tag ring pairs by ORDER (one push per draw on the game
thread, one pop per present on the render thread), and the order claim breaks in three
measured ways: (a) across a single -> double transition when the game thread runs a frame ahead
(a `reentry rearm 2` flipped the -1 tag's c5 from the left eye's position to the right's), (b)
within a second of the first arming, and (c) spontaneously in plain gameplay - with the check
off (`reentry c5pair off`) the side flipped twice in 25 s with no pause, no re-arm, no log
event, while the perf window read `untagged 16-19` per 3 s and P2 exceeding P1 by the same
count: a present found the ring EMPTY and the next one popped its tag. In the menu state the
ring overflowed every 3 s (draws outnumber presents there: `draws/s=67 presents/s=57`). Each
such skew showed the eyes swapped until the next one. The picture agreed with the c5 side every
time (shift +1 px when the side read SWAPPED, -1 px when ok), which also settles the sign
convention by picture: the field holds the position, c5 negates it, a right eye's c5 sits at
-ipd*scale along the camera's right row.

**The fix (`[Stereo] C5Pair=1`, `reentry c5pair on|off`, the A/B)**: the within-tick invariant is
the pairing. Between pass 1 and pass 2 the world does not tick, so the ONLY thing that moves the
camera is the writer's eye: `c5(pass 2) - c5(pass 1) = -ipd*scale` along right and 0.00 along
anything else (measured: `-6.17 along right, 0.00 other` on every pair). A present whose c5 sits
there from the previous present's is a pass-2 present whatever the ring says; one at +ipd*scale
is a pass 1 after a still pass 2; anything else (a pass 1 after a moving pass 2, an extra
present) takes the ring's tag. The ring's claim is checked against the measurement on every
present it can name; three disagreements in a row drain the ring to the tag the next present
must pop (`reentry: the tag ring skewed against the draws ... realigned`, Info, 3 s), and armed
single gameplay draws push a 0 tag so their presents cannot eat the next tick's -1 (not in
menus, where the ring would only fill with junk). On the fixed build: `side ok` from the first
pair after the arming through rearms of 1, 2 and 3 ticks, a `capture reinit`, a `stereo
projection off`/`auto` and a `stereo mono`/`stereo reentry` switch; P1 == P2 per window,
`untagged 0-1`; `reentry.xrs` 11/11. Counters: `c5Agree`, `c5Disagree`, `c5Realigned`,
`c5Unknown` (presents the invariant could not name), `c5Untagged` (the ring was empty, the
measurement named the eye) in status.json `stereo`.

**The headset run on this build (run 07, 2026-09-04, the user)**: the eyes right from the load
and after every word on the F10 EYES block; `side ok` and `SWAPPED=0` on every pair, c5 |d| 6.11
(the user's IPD), the picture shift negative, L-R 3-14 at 64x64 (the one-picture dump pair of
run 17 reads 1.49 at 64x64; the run-17 true pairs 9-10) - and the ring skewed against the
draws 131 times in about four minutes: on the user's rig the order claim goes wrong every 2 s,
which before this session swapped the eyes each time. Two costs of the first build, both fixed:
the verdict compared L-R against the same-eye floor, which on a live head holds a tick of head
motion that a within-tick pair does not (false `ONE PICTURE` warns; an absolute 2.0 at 64x64
now), and stage bb's `GetRenderTargetData` read every present is a pipeline sync on that GPU:
`perf: gpu idle(d3d9)` 1.5 ms per present against 0.3 in run 17, the tick 13.9 -> 16.7 ms
(60/s under a 72 Hz headset, `wait 0.0`, the time inside the game's own Present). The trace
samples one pair every 8 ticks now (`[Perf] FrameIdEvery`), which is all the judgement needs.

**THE A/B, IN THE HEADSET (run 08, 2026-09-04, the user): the pairing IS the fault.** With `c5
pairing` unticked on the F10 EYES block the eyes stayed right until a pause/resume, and then
went wrong at once and stayed wrong: the 3 s windows read `swapped=24 of 25` then `12 of 12`,
with the picture's own shift agreeing (`shiftPos=20`, the right eye's content on the wrong
side of the left's). Ticking it back on: one transitional pair, then `swapped=0` for the
remaining eighty seconds and the shift negative throughout. The user reported the same thing
by eye, three times, without seeing the log. So the eye fault this project has chased since
run 15 - "the eyes disagree", "90 % of the time, more at the beginning", "never correct
after a load" - is the order-based pairing breaking wherever the game thread runs ahead of
the render thread, and the within-tick camera step is the fix. The run-17 dump pair whose two
eyes differed by only 1.49 at 64x64 remains the one artifact not separately explained; it has
not recurred on the fixed build, and a swapped pair of a near-symmetric corridor view is the
simplest account of it.

**What this does and does not say about the headset's one-picture state.** A swap is two
pictures, not one; the run-17 dumps were one picture, so the swap is not that fault. It is,
though, exactly what "the eyes disagree" looks like, and it happened on every state transition
the ring order got wrong, so some of the run-15/16 reports were swaps. The one-picture state
still needs the headset run with the trace: if the line reads L-R below the floor at `bb` the
engine handed one picture twice (then the engine or the camera seam owns it); above at `bb` and
below at a later stage names that stage; `reentry rearm 2`, `capture reinit` and `stereo
projection off`/`auto` each alone say which half of the user's remedy repairs it.

## Evidence handling (both cost a session)

- **Log rotation is one deep.** `dishonored_vr.log` + `.prev.log` only. Two simulator runs
  after a headset session destroyed both headset logs; only the crash text survived, and
  the surviving pair contained no `EXCEPTION` and no `PreExit` at all. Copy the log out
  before the next launch, always.
- **The crash file had no run identity.** It is opened `FILE_APPEND_DATA` / `OPEN_ALWAYS`
  and accumulates forever with nothing separating runs. `dvr-xrsim` and VDXR fault into the
  same `d3d11.dll` and produce byte-identical fingerprint text, so three records could not
  be attributed to a backend, a runtime or a build. 40.2 writes one header per run (wall
  clock, `DVR_VERSION`, `DVR_BUILD_ID`, pid, backend + runtime name via
  `dvr::crash::set_context`, called from the OpenXR backend once the runtime names itself).

## Blink's reach rule lives in its aim vector, vertical cap included (VR-36, 2026-09-13)

Measured at the source seam `0xbf55a3`, which is where the engine's own Blink aim vector
is built, in one headset run with the redirect live:

```
engine aim (-1087.11,-157.00, -59.45) len 1100    aim level: the power's full reach
engine aim (  811.29,-136.61, 500.00) len  963
engine aim (  400.12,-178.28, 500.00) len  665    aim steeply up
```

**The Z component pins at exactly +500.00 and the LENGTH falls to match.** So "you cannot
blink far upwards" is not a downstream refusal and not a collision result - it is built
into the magnitude the engine hands out, and a redirect that keeps that magnitude keeps
the rule for free. A redirect that substitutes its own length throws the cap away, and
the blink climbs into the sky. `[Blink] ReachMode` ships 0 for that reason, and modes 1
and 2 are clamped so they can only shorten what the engine offered.

The same run also settled where the trace starts. The destination the engine finally
stores, taken from the camera, sits within about half a degree of the redirected ray at
every sample (13.2 vs 12.64, 23.9 vs 23.16, 7.8 vs 7.07 degrees against the engine's
original direction). So the trace effectively starts at the camera, and the controller
sits 65 to 93 uu from it - that offset is the whole parallax between the drawn guide and
the landing point, and it was judged not visible in the headset.

## Other seams (verified)

- Blink: the source-vector detour (above) plus `BlinkControllerDir` and `BlinkReach`
  (distance by hand pitch); a landing marker drawn through the fork (`dxvk_vr_mark`).
- Motion aim: freshly spawned projectile objects near the camera get their aim vector
  rewritten (`MotionAimTick`, `MaimWriteAim`, `SteerTick`); haptics on catch.
- Melee: swing detection on controller velocity (`MeleeTick`, `[Melee]` speed/sustain/
  distance gates); block/choke state (`BlockStateTick`).
- Menu/cine: `g_menuOpen`/`g_inMenu` from script events and the cursor; `CineActive` latch
  cleared when no live pawn (the main menu fires the same toggle); dialog holds.
- Console: `RunConsole` drives the engine console from the script lane (the intro skip:
  `ce ChangeLvl_fromTower_toPrison`; `SetResApply` asks for the render size from inside the
  engine because every window-route attempt failed).
- Resolution: `hkGetAdapterDisplayMode/ModeCount/EnumAdapterModes` hand the game a
  4032x2268 mode; the window hooks hold it; `[Screen] SpoofDesktopW/H` and `ResX/ResY` must
  match.

## SOLVED: the black texture bug was sub-level writes never reaching the GPU (VR-15, 2026-09-05)

**Headset-judged fixed.** `[Device] ShadowFullCopy=1` removed the black surfaces on the
tester's rig, which confirms the mechanism below: `UpdateTexture` was not carrying writes
to mip levels above 0, so the small mips stayed as created (black) while level 0 was
correct. The lever now **ships ON** - a deliberate exception to "every render lever ships
OFF", made for the same reason as `[Stereo] HoldUntagged`: the OFF state is a visible
rendering bug. `device shadowfullcopy off` restores the fault, which is the A/B.

### What it cost, and what paid for it

The first cut of the fix made the frame rate less stable, and three separate things were
responsible. Two of them predate the fix and one was self-inflicted:

1. **The push ran on READONLY unlocks.** A READONLY lock writes nothing, so there is
   nothing to push - and this game takes **12408 READONLY locks on MANAGED textures in one
   load** (its mip streaming reads the old texture to fill the new one). Every one of them
   was a whole-texture GPU copy for no change at all, **before this session's work as well
   as after**. The lock hooks already had to look the twin up, so recording whether the
   lock was READONLY rides along in the same critical section for free (`roMask`, one bit
   per level), and the unlock skips entirely. This is pure removal and the largest win.
2. **A refused `UpdateSurface` was paid for on every single unlock**, because the fallback
   ran and then the next unlock tried again. A texture whose format refuses is now
   remembered (`surfaceRefused`) and goes straight to `UpdateTexture` from then on, so a
   refusal costs once rather than forever.
3. **The 60-second upload census walked all 32768 twin-map slots under the lock, on the
   present thread, just to decide whether to print.** Self-inflicted, this session. The
   decision now reads plain counters; the walk happens only when the line actually prints.

Also folded in: `shadow_unlocked` took the critical section twice per unlock (once for the
twin, once to bump the counter) and now takes it once, reading the entry pointer and
writing its counters after the lock is dropped. A texture released concurrently can then
only make a **counter** wrong - the entries live in a static array, so there is no freed
memory to touch - and that is a better trade than holding a lock across a D3D call.

### The original observation and the reasoning

**From a headset run on the tester's rig**: an NPC photographed at two
distances. Far away the model is largely solid black; walking toward it, the black recedes
and the material resolves correctly. At close range it is right. The fault is not a
material class and not a lighting path - **it tracks distance**, and distance is what
selects the mip level a surface is sampled at. So the black data lives in the SMALL mips
(level > 0), and level 0 is intact.

That narrows the four candidates below to one lane, and it lines up with a number already
sitting in the 2026-09-04 headset log, unread:

```
locks on MANAGED: plain=47899 READONLY=12408 DISCARD=0 NOOVERWRITE=0 partial=2986
                  level>0=50189 dirtyRects=0
```

**50189 locks on mip levels above 0, and zero `AddDirtyRect` calls in the whole run.**

Now the mechanism. The shadow pushes a written texture to the GPU with
`IDirect3DDevice9::UpdateTexture(twin, real)`. That call **takes no level**: it copies what
D3D9 believes is dirty across the chain. Until this session `shadow_unlocked()` did not
even receive the level - `hkTexUnlockRect` had it in its hand and dropped it at the door -
so there was no instrument that could have noticed a per-level fault, and nothing anywhere
in the mod knew that sub-level writes were 50189 of the traffic.

**This is a hypothesis with a mechanism, not a measurement.** What makes it testable is
`UpdateSurface`, which names its two surfaces and therefore cannot be vague about which
level it copied. `[Device] ShadowFullCopy=1` pushes exactly the level the unlock wrote,
falling back to `UpdateTexture` when `UpdateSurface` refuses (compressed and odd formats
can), so the lever can never leave the picture worse than it found it. **Black-at-distance
clearing when that lever goes on is the proof; it not clearing falsifies this cleanly.**

The four ways an upload can be lost, below, all still stand - this section narrows which
one to look at first, it does not close the others.

### The instrument and what each number means

**No headset run has yet produced a reading from it.**

`[Device] Managed=shadow` is the newest thing in the creation path. A 9Ex device refuses
`D3DPOOL_MANAGED`, so every MANAGED texture is created `DEFAULT` and given a `SYSTEMMEM`
twin; `IDirect3DTexture9::LockRect` is redirected to the twin and `UnlockRect` pushes the
twin's dirty regions to the real texture with `UpdateTexture`. A texture whose write never
completes that round trip keeps the contents it was created with, and a freshly created
D3D9 texture is **black**. That is the shape of the reported fault, which is why the
shadow is the first suspect - not because anything has been measured.

There are exactly four ways the round trip can fail, and until now every one was silent:

| | what happens | why the texture goes black |
|---|---|---|
| **twin refused** | the lock reached the twin and the runtime refused it | the write never happened |
| **no twin** | translated to DEFAULT but `shadow_twin()` came back null (twin creation refused, or the twin map was full) | the lock falls through to a DEFAULT texture, which is not lockable: D3D9 refuses it |
| **passthrough refused** | an untranslated lock the runtime refused | the write never happened |
| **surface bypass** | the game took a surface off the texture (`GetSurfaceLevel`) and locked THAT | the surface's `LockRect` is a **different vtable** (`IDirect3DSurface9` slot 13, not `IDirect3DTexture9` slot 19), so the shadow redirect never sees it and the write lands on the DEFAULT texture |

The bypass is the one worth stating plainly, because the existing redirect cannot catch it
by construction. `IDirect3DTexture9::GetSurfaceLevel` is slot 18; `IDirect3DSurface9` is
`IUnknown` 0-2, `IDirect3DResource9` 3-10, `GetContainer` 11, `GetDesc` 12, **`LockRect`
13, `UnlockRect` 14**. The census now patches 18 on textures and cube textures, records
which texture and which level/face each handed-out surface belongs to, and patches the
surface class's 13/14 once. `UnlockRect` is patched FIRST and `LockRect` only if that
succeeded: a `LockRect` hook without its own original cannot fail soft.

The `device/upload` census reports all four with the first `HRESULT` that produced each,
plus the twin population - **how many live twins have carried no successful
`UpdateTexture` at all**, which is the population the counters are counts of. Its verdict
prints the unwelcome answer as readily as the welcome one: four zeros and a clean
population say the shadow is NOT where black surfaces come from, and name the next A/B
(`[Device] Ex=0`, then `stereo arm off`, then the game with no mod).

`[Device] ShadowSurfaces` (ships **0**, `device shadowsurfaces on|off` live) is the fix for
the bypass if the bypass is real: the surface lock goes to the twin's matching surface and
the unlock pushes it. It is a lever, so it ships off and is judged in a headset.

**The twin map has burned this project once already.** On 2026-09-03 it filled 8192 slots
with tombstones across repeated quickloads (2400 live), a texture got no twin, its lock was
refused, and the game died inside D3D9 on it. That is the "no twin" row above, and it is
why the row exists rather than being assumed impossible: the map is 32768 slots now, which
made the crash go away without making the mechanism go away.

## The pause/resume desync: a one-sided tag stream (2026-09-03, session 7)

The run-40 report ("the judder stays in the LEFT eye and stops in the RIGHT") is the
signature of a held right swapchain image, and the cause is on the game side, not in a
ring. Pass 1 pushed its `-1` tag on five gates and pass 2 re-evaluated them AFTER the draw
plus four more (exiting, a test running, the c5 serial, a present since the previous draw),
so the resume window - the game thread's catch-up burst tripping the stall gate, the verdict
flapping through LOADING for a second - produced `-1` tags with no `+1`. The runtime then
counted `abortLeft`, captured the left again and submitted a stereo layer that names each
eye's swapchain: the LEFT rewritten every other present (the judder), the RIGHT showing its
pre-pause image with its old pose (reprojected smoothly, so it looked frozen). STATUS had
attributed it to `xr: sr tag ring skewed`; that ring is pushed and popped consecutively on the
present thread in this port and cannot skew - the line that does appear is `reentry: tag ring
skewed` (the method's ring, a different threshold). The quad transition was never the fault:
`reset_aer()` clears both eyes on every quad present.

Fixed at the source (813807e3): the gates are decided ONCE at depth 0 before pass 1's tag, and
pass 2 takes the decision. Instrumented first (a9b2ef12): each eye capture stamps the present
counter, every stereo submit computes the per-eye image age in PRESENTS (a healthy pair reads
L=1 R=0), the beat prints an `eyes` line, and the method prints `STALE R EYE` with the OWNER
named from the game side's skip deltas or the runtime's counters. The fail-soft (d0650a38,
`vrpace strict`, default off): a stereo submit with an eye older than one present shows the
fresh eye to both eyes for that frame. `reentry skip2 <n>` reproduces the one-sided stream on
demand: strict off, the simulator counted held-eye stereo submits and the mod printed the
line; strict on, no new stale submit and 37 fallbacks to mono. The hammer
(`tools\arming-hammer.ps1`, 15 pause/resume cycles) and `stale-eye.xrs` read 0 stale submits
on the fixed build.

## The pair phase (2026-09-03, session 7)

`xrEndFrame` time minus the frame's `predictedDisplayTime`, through
`XR_KHR_win32_convert_performance_counter_time` (enabled when the runtime lists it; the
simulator now offers it with the same split QPC conversion its clock uses), sampled at every
pair close: negative = the pair closed before its slot, positive = it missed the slot and
displays a slot late with a pose predicted for the earlier one. On the TRACE pairs line, a
5 s log line, `vrpace status`, `stereo status`, status.json `stereo.pair.phase*`. On the
simulator the number is meaningless (its predicted time is `now + period` re-anchored, and it
read +58 to +75 ms, 100 % missed, at 40-64 pairs/s on a 90 Hz schedule); the headset's is the
number the judder question is decided on. `vrpace ahead 0|1|2` (7f569463) locates the head
pose the game renders with, and the views the layer is tagged with, for `predictedDisplayTime
+ ahead x period`; `xrEndFrame`'s displayTime and the tag generation are untouched, so at 0 the
paths are byte-identical. `vrpace lag` exposes the attribution generation for the measurement.

## THE GHOSTING WAS THE CADENCE BEAT, and the verdict's threshold hid it (2026-09-04, session 15)

**SOLVED, on the headset, by a one-setting A/B.** Same build, same scene, same 2750x2850 render;
only the headset's refresh changed.

| | 120 Hz | 90 Hz |
|---|---|---|
| display period | 8.33 ms | 11.11 ms |
| `perf: tick` p50 (p90, max) | 9.1 ms (10.8, 12.9) | 11.3 ms (12.0, 15.1) |
| **display slots per frame** | **1.05 - 1.11** | **1.00 - 1.02** |
| EVEN / UNEVEN windows | 9 / 20 | **33 / 16** |
| MATCHED / UNDER-SUBMITTING | 11 / 26 | **38 / 22** |
| ghosting reported | yes | **no** |

The mechanism is arithmetic, and the `stereo: rate` line had been printing it all along:
at `off` slots of drift per frame, **one frame in `1/off` is held for an extra display slot**,
and consecutive frames shown for different durations is exactly a doubled edge under rotation.
At 1.11 that is every 9th frame. At 1.01 it is every 100th. The fault was never the resolution
and never the pose attribution - **it was the tick not dividing into the display period.**

### Session 14's falsification was wrong, and this is why

Session 14 measured 1.03-1.05 slots per frame at 2064x2208/120 Hz, read `EVEN CADENCE`, and
concluded the cadence hypothesis was dead. **The verdict was lying.** Its threshold was
`|off| > 0.06`, so it called 1.05 - a beat every twenty frames, plainly visible on a head turn -
a clean bill of health. The hypothesis was right; the instrument's *threshold* was wrong, which
is a failure mode worth naming: an instrument can be correctly built, correctly read, and still
mislead because the line between pass and fail was picked before anything was measured.

The threshold is now **0.02**, drawn at the measured edge (1.02 does not ghost, 1.05 does), and
both branches print the beat as a number - one frame in N, and the beat in Hz - so a future
"even" verdict shows the residual it is forgiving instead of hiding it.

### Why the frame rate drops FURTHER at 90 Hz than at 120 Hz

The tester's own observation, and it is not a contradiction:

- At **120 Hz** the tick (9.1 ms) never fit the 8.33 ms slot. The app was never trying to hit a
  slot - it free-ran and the compositor smeared over the mismatch continuously. There is no
  cliff to fall off when you are already permanently past the edge, so the rate reads a smooth
  100-120 and the ghosting is constant. **Smooth, and always wrong.**
- At **90 Hz** the tick (11.3 ms p50) sits *right at* the 11.11 ms period. Most frames make
  their slot, which is what removed the ghosting - but a frame that misses waits a whole period,
  so a single 11.3 ms overrun displays for 22.2 ms (45 fps instantaneous) and a run of them
  averages toward 60. **Correct, with a cliff directly underneath.**

**And the hitch RATE did not actually change.** Normalised by run length (29 vs 50 three-second
windows): 27.6 gaps/min at 120 Hz, 28.4 gaps/min at 90 Hz. Identical. They are simply visible
now, because they stand out against a locked cadence instead of disappearing into a permanently
smeared one. **54 of the 71 gaps sat in `present-tail (xrEndFrame)`, up to 101 ms** - on a Wi-Fi
streaming runtime a 101 ms block inside the submit call is the encoder or the link, not the
frame path. That is the next thing to attack, and it is not ours.

### The cost model, refit with the new point

`perf: tick` against per-eye megapixels, four sizes: **~0.63 ms/MP on a ~5.8 ms fixed floor**
(2750x2850 = 7.84 MP measured 11.3 ms against 10.8 predicted, so the floor is slightly higher
than the three-point fit said). Note the 90 Hz tick is PACE-BOUND (8 windows say so), so 11.3 ms
is partly the slot rather than the work - the render cost alone is lower and the headroom is
real but unquantified.

**The rule this leaves:** pick the refresh whose period the tick divides into, not the biggest
resolution. Read `stereo: rate` for `display slots per frame` and drive it to 1.00.

## The startup eye-starvation flicker, measured at last (2026-09-04, session 15c)

**Not new.** It normally lasts a few seconds at the start of a session; on this run it lasted
much longer, which is what finally made it measurable. The reported percept: heavy flickering
for roughly 30 seconds that looked like alternate-eye rendering viewed flat, with the weapon
taking that long to settle into alignment. It then stopped, the weapon stayed aligned, and the
rest of the session was smooth.

**What the log shows, `stereo: beat` L/s and R/s across one run** (2750x2850, 90 Hz, Quest 3
over VDXR, `alpha-272-g65ac9bd2`):

| t (s from proxy load) | out/s | L/s | R/s | none/s | draws/s | state |
|---|---|---|---|---|---|---|
| 8.5 - 17.5 | 21 - 85 | 0 | 0 | 0 | - | menu/loading, mono by design |
| **20.5** | 89 | **12** | **52** | 10 | 66 | GAMEPLAY starts; starved |
| **23.5 - 38.5** | 87 - 91 | **16 - 19** | **71 - 73** | 16 - 17 | 51 - 72 | starved |
| **44.5 onward** | 180 | **90** | **90** | **0** | **90** | **locked, and stays locked** |
| 77.5 - 83.5 | 155 - 234 | 0 | 0 | 0 | - | pause screen, mono by design |

**The cause is the tick, and the numbers say so directly.** During the starved window
`perf: tick` reads **17.5 ms against the 11.11 ms budget** and its per-class split is
`P1[-1] n=36` against `P2[+1] n=156` with `untagged 107` - the LEFT-tagged presents are a
quarter of the RIGHT ones. `reentry: beat` confirms pass 2 is running the whole time
(`2nd/s == draws/s`, skips all zero), so the second draw is NOT missing; the game is simply
producing 51-72 ticks/s against 90 display slots/s. With the tick below the display rate the
pair schedule cannot land one pair per slot, the tag stream goes lopsided, and 1016 same-eye
pushes accumulate (`reentry: pushed eye +1 TWICE in a row`, always +1, LEFT starving).

**One eye taking fresh frames at ~18 Hz while the other runs at ~73 Hz is not subtle - it is a
hard flicker, and it looks like alternate-eye rendering seen flat because that is structurally
what it has become.** It self-heals the instant `draws/s` reaches 90: L/s = R/s = 90, zero
untagged, zero stale, for the rest of the run.

The usual reason the tick is slow for the first seconds of gameplay is UE3 level streaming -
`call2` max spikes to 1836-2029 us in that window against 614-777 us once locked, and the
device census logs 14058 creations at first GAMEPLAY.

**Same root as the ghosting, at a different ratio.** When the tick is slightly longer than the
display period you get the beat (doubled edges); when it is far longer you get eye starvation
(flicker). Both are the tick not fitting the slot.

### Fix theory - NOT implemented, and the cheap test comes first

1. **`vrpace strict on` is the existing lever and has never been judged.** It already does the
   right thing in principle: a stereo submit with an eye older than one present shows the fresh
   eye to BOTH eyes instead. That converts the starved window from alternating eyes into a
   briefly flat picture, which is a far milder artifact, and it costs nothing to try - it ships
   off and toggles live. **Do this before writing any code.** The risk is that it also fires on
   the rare mid-gameplay stale eye and drops depth for a frame there, so it wants an A/B, not a
   blind default flip.
2. **If strict is not enough, the shape of a real fix** is to refuse to submit a pair at all
   while the tick cannot fill the slots, rather than submitting a lopsided one - i.e. extend
   the `HoldUntagged` idea from untagged presents to unbalanced pairs, holding the previous
   good pair until `draws/s` recovers. Bounded, because a permanent hold is a frozen image.
3. **The cheapest mitigation is not ours at all**: the window ends when streaming does, so it
   scales with load time. An SSD, and not turning the head for the first few seconds after a
   load, both shorten what the player sees.

Unresolved and worth measuring first: **why LEFT specifically.** The pushes are always `+1`
(RIGHT) doubled. A plausible mechanism is the shared-capture deferred delivery
(`SharedWait=0` delivers the PREVIOUS slot) repeating a tag when presents arrive irregularly,
but that is a hypothesis, not a measurement, and `capture sharedwait on` is the A/B that would
test it.

## The content-bbox readback prediction was FALSIFIED (2026-09-04, session 15)

Session 15 gated the 3-second full-frame CPU readback and predicted that if it were behind the
hitches, the `perf: frame gap` count would fall by roughly the number of 3-second windows in a
run. **It did not.** Samples fell from one per 3 s to 2-3 per run, and the gap rate was
unchanged (27.6 and 28.4 per minute across the two runs, against 62-82 per run before). The
counter-evidence recorded alongside the prediction - that the gaps mostly sat in
`present-tail (xrEndFrame)`, not the capture phase - was the correct read.

**The gate stays**: it removed a real, unlevered ~30 MB present-thread stall and cost nothing.
It just was not the hitch cause, and saying so is the point of having written the prediction
down.

## The two pose lanes, and why the tag can be a generation wrong (2026-09-04)

The mod samples the head TWICE per frame, on two different lanes, and the compositor only
ever sees one of them:

- **SCRIPT lane.** `on_present_begin` locates the head (`xrLocateSpace`, `openxr_runtime.cpp`),
  `DvrConsumePoses` -> `TrackHead` turns it into `g_hmdYaw` on the present thread, and the
  GAME thread's world tick reads that in `ApplyHeadToViewRotation` and writes the engine
  camera. `head_track.cpp` publishes the matched pair at that instant: `g_viewYawRad` (what
  was written) beside `g_injHmdYawSnap` (the HMD yaw it was computed from). **This is the pose
  the pixels are drawn with.**
- **PRESENT lane.** The same `on_present_begin` calls `xrLocateViews` for the same
  `locateTime`, and the projection layer's `XrCompositionLayerProjectionView.pose` is filled
  from one of three kept generations - `g_views` (N), `g_viewsContent` (N-1), `g_viewsPrev2`
  (N-2), selected by `g_poseLag`, shipping at 1. **This is the pose the compositor reprojects
  FROM.**

If those two are not the same sample, the reprojection is wrong by the difference on every
frame, the error tracks head speed, and it grows when a frame is slow. That is a doubled-edge
percept under rotation - and until 2026-09-04 nothing measured it.

**The tag is predicted to be one generation too fresh, and the game's own config says so.**
The attribution comment assumes "locate N feeds the tick that presents at N+1" - one
generation, hence `lag=1`. But `DishonoredEngine.ini [SystemSettings]` carries
**`OneFrameThreadLag=True`**: UE3's render thread runs a frame behind the game thread, so the
pixels in present N were drawn by a tick that read the head at locate **N-2**. The prediction
is therefore that the instrument reads a one-generation gap at `lag 1` and that
`vrpace lag 2` nulls it. `OneFrameThreadLag=False` is the independent second test - it removes
the skew at the source instead of compensating for it, at a throughput cost. Neither has been
run yet.

**Both eyes of a pair share ONE locate - do not go hunting a per-eye asymmetry.** Under
`reentry` the LEFT present holds the XR frame open (`pairHold`) and the RIGHT completes it;
`on_present_begin` returns at the top while a pair is open, so there is no second
`xrWaitFrame` and no re-locate between them. `g_viewsContent` is identical for both eyes. The
instrument prints per eye anyway, cheaply, so the invariant is checked rather than assumed.

**THE SIGN TRAP.** The two lanes read yaw out of the SAME rotation matrix with opposite
conventions:

| | reduces to |
|---|---|
| `xr_quat_yaw_deg` (`openxr_runtime.cpp`) | `atan2( m02, m22)` |
| `TrackHead` (`head_track.cpp`) | `atan2(-m02, m22)` |

so `g_hmdYaw == -xr_quat_yaw_deg / 57.29578` for any pose, and a naive subtraction reads about
TWICE the yaw. That would look like a catastrophic disagreement that is purely convention -
the most convincing possible way for this instrument to lie. `publish_script_head` negates
once, on the way in, and then PROVES it against live data: at the first publish it reads the
same `g_headPose` back through this file's own converter and logs
`xr: poseaudit SEAM CHECK ok|FAILED`. Do not read a delta until that line says ok.

Note also that `g_viewYawRad` is the absolute UE **rotator** yaw and composes stick turn with
head delta (`ue_math.cpp` differences the two deliberately). It is never the right thing to
compare against the tag; `g_injHmdYawSnap` is.

## The content-bbox readback: a full CPU round trip in the VRAM path (2026-09-04)

`capture.cpp`'s bbox instrument - the `100% x 100% (FULL)` / `(CROPPED)` line - needs CPU
pixels. In `shared` mode, whose entire purpose is that nothing goes to the CPU (the frame is a
VRAM-to-VRAM `StretchRect`), sampling it costs a full `GetRenderTargetData` + `LockRect` +
per-row `memcpy` of the whole frame, **on the present thread**. That is the same round trip
measured at 17-21 ms/present in `sync` mode at 2496x2688 ("The capture cost, measured"), and
it ran unconditionally every 3 seconds with no lever - about 31 MB per sample at 2750x2850.

`[Capture] BboxMs` (default 30000, `capture bbox off|<ms>` live) is the gate. A size change
still resamples immediately and unconditionally, because that is the sample that decides
CROPPED vs FULL and it must not wait for an interval.

**Falsifiable prediction, recorded before the run:** if this is behind the hitches, the
`perf: frame gap` count should fall by roughly the number of 3-second windows in a run (62-82
gaps over the last two runs is close to one per window). **The counter-evidence is already on
record**: those gaps mostly reported `sat in: present-tail (xrEndFrame)`, not the capture
phase. If the count does not move, this removed a real cost and was not the hitch cause.

## The pitch pivot: the engine's neck, measured (2026-09-03, session 7)

`camera pitchtest 30` (e374a6a2) takes three buckets of c5 (LEVEL, looking UP, looking DOWN,
60 presents each) and projects the travel from LEVEL on world up and the pitch-0 heading, so
the per-eye offset cancels and it runs under `stereo reentry`. Simulator, the sewers, 98 uu/m:

| run | head | c5 travel UP (U, F uu) | DOWN (U, F) | seam asked UP / DOWN | fit |
|---|---|---|---|---|---|
| 1 | `head rot 0 +/-30 0` at a FIXED position | -1.04, -16.58 | -7.07, +14.90 | 0 / 0 | below 0.321 m, behind 0.062 m, consistency 0.33 / -0.07 uu |
| 2 | `head pose` on an 11/9 cm arc | +1.91, -23.14 | -12.86, +19.13 | U+2.97 F-6.58 / U-5.85 F+4.20 | the same fit; the extra travel equals the ask |
| 3 | fixed position, `neck cancel 0.321 0.062` | -0.48, +0.11 | +0.14, +0.15 | the cancelling term | the render camera holds still |

So the ENGINE pitches its camera about a pivot 32 cm below and 6 cm behind the eyes: 17 cm of
backward travel at +30 deg, which the compositor (expecting only the tracked head translation)
cannot reproject - "looking up with the whole body". The tracked arc arrives on top of it
(run 2). `neck cancel` with the engine's own numbers cancels it (run 3), and the +30 frames
with and without the cancel differ by the predicted 17 cm (the lamp at the right edge cut
off, the pipe junction lower). The `[Neck]` lever (0db35c10) ships off with the measured pivot
as its defaults; the headset judges `cancel` against `off` from F10 Comfort. The 38.24 ceiling
now counts the presents it clips (0 in all runs).

### Crouched, the engine has no neck arc (VR-78, 2026-09-12, headset)

Measured with the VR-78 accounting probe (`[PosTrack] ZAccount`, `z_account.h`), which joins
each draw's camera write to that present's c5. One complete crouched episode (capsule 65,
23 s, 1823 accepted samples, closure 0.00 uu, clamp clips 0) against standing episodes on
the same run, `[Neck] Mode=cancel` at 0.321/0.062:

| stance | pitch vs LEVEL | engine base Z change | neck term Z | residual (render minus tracked head) |
|---|---|---|---|---|
| standing | -27 deg | -6.23 uu | +6.90 | up +4.5 (the final cap, see below), fwd -2.3 |
| standing | +30 deg | -1.68 uu | +1.49 | up -4.6 (the final cap), fwd +0.8 |
| crouched | -33 deg | **-0.04 uu** | +9.52 | **up +9.3, fwd -18.4** |
| crouched | +32 deg | **+0.03 uu** | +1.99 | **up -5.0, fwd +18.9** |

Standing, the base follows the measured pivot (predicted -6.3 at -27 deg). Crouched it does
not move at all, so `cancel` subtracts an arc that is absent: looking down the view ends up
back and up, looking up forward and down - the direction the headset reported. The eye clamp
did not clip in that episode, so it is not the crouched cause. The standing leftover is the
38.24 final cap: the rest eye sits exactly at the capsule ceiling, and the cap removed the
3 to 7 uu the tracked head sat above its reference. `[Neck] CrouchPivotBelowM/BehindM` holds
the crouched pivot (-1 = the standing numbers); slides and vents (capsule 33) are unmeasured.

**Repeated with the crouched pivot at 0** (second headset run, same day): the probe's fit
over fresh and recovered engine bases solved **0.002 m below, 0.002 m behind** crouched over
-48..+32 deg of pitch (1808 samples), against 0.28 m below standing on the same run. Crouched
looking up +29 deg, the forward residual fell from +18.9 to **+0.04 uu**; the up residual of
-4.75 is the final cap (VR-87), not the neck. The tester judged crouched pitching fixed, and
0/0 ships as the default.

## The console seam was dead since 41.0, and setres is inert (2026-09-03, session 7)

`RunConsole` returned -1 unless `g_fnConsoleCmd` was set, and the only latch lived inside the
resolution script 41.0 removed: every `console` word and IntroSkip returned -1 since 41.0
without reaching the engine, so "setres is a dead end" (session 2) was never re-measured on
this line. Latched again (c0bd3831); the first word that reached the engine overflowed the
game thread's stack, because `RunConsole` calls ProcessEvent, which is the mod's own hook,
which ran the still-pending request again - the request now leaves the seam before the engine
runs it and the nested call returns on the re-entry flag. Measured then: `setres 2560x1440f`
(a real mode) and `setres 1600x900w` both dispatch (`ConsoleCommand` on
`DishonoredPlayerController`, the console's `OutputText` fires), return an empty reply, and
change nothing - no Reset, no size change. `setres` is inert on this build.

## The render size: the ini route is inert, the command line is honoured (2026-09-03, session 7)

With 2560x1440 fullscreen in every place of the game's own ini (both files, all four AppCompat
buckets, the compat file rewritten by the game itself at launch) the game created a 1920x1080
WINDOWED device on every run, including the headset run, and never Reset out of it (runs
07-08); the install-side `DefaultEngine.ini` carries exactly 1920x1080. The command line is the
route: the proxy is loaded before the engine's entry point, so `ResRequest` writes the ask to
`dishonored_vr_launch.txt` and DllMain points the exe's and the CRT's `GetCommandLineA/W`
import slots (3 patched) at the line with `-ResX= -ResY= -FullScreen` appended (ce10a3f7).
Run 09: `CreateDevice -> (2560x1440 windowed=0)`, the capture and the eye swapchains followed.

A size the display does not list falls back to a real mode (2496x2688 -> 2560x1440, run 10).
`[Screen] VirtualMode=1` (3935f9f7): the proxy, which IS the game's IDirect3D9, advertises the
asked size in the mode list the game validates against (`GetAdapterModeCount` /
`EnumAdapterModes`, asked from 0x9b79b7 at startup and 0x9b9a20 in the frame loop, our mode
handed at slot 123) and, when the game asks for a FULLSCREEN device at that size, creates it
WINDOWED with the backbuffer kept. Run 12: `CreateDevice -> (2496x2688 windowed=1)`,
`capture: 2496x2688`, `res: HONOURED`, `xr: swapchain pair 2496x2688`, the runtime's
circumscribed hfov 108.0 deg at aspect 0.929 (137 at 16:9), the sim's claim ratio 1.18 (2.17
at 16:9), both eyes 77 % non-black in the sewers, the frame complete floor to ceiling by
picture. Every 41.0-era dead end ran the game WINDOWED, where the desktop clamp lives; this is
the fullscreen path. The cost: the sync readback reads 18-20 ms per present at that size
(25.6 MB each way; `[Capture] Mode=deferred` is the answer, ROADMAP S1).

## The 30.13 bone-visibility result was recorded all along, and it names +0x288 (VR-31, 2026-09-06, session 18)

Two sessions of planning treated "write 0x02 into the per-bone visibility array at
component +0x288" as an open experiment whose result was **never recorded**. It is
recorded. The original author wrote it down in a code comment, in the state chunk that
sits beside the experiment rather than in this file, which is why three passes over the
corpus missed it. It survives verbatim in `src/mod/state/40_game_dishonored_hands_arms_hide.inc`
and in the original single file (`git show 824e08d8:src/dllmain.cpp`, line 11119):

    30.14: the +0x288 byte poke was wrong - that array turned out to be some
    per-bone animation control (arms froze to the view and rode the head).

So the write was made, and it had a **visible effect that was not hiding**. That is a
stronger result than a null one: the bytes at +0x288 are read by something, and what
they drive is animation, not visibility.

### What +0x288 most likely is

UE3's `USkeletalMeshComponent` carries two per-bone byte arrays, and the probe found the
wrong one:

| array | default value | what a 2 there means |
|---|---|---|
| `BoneVisibilityStates` | 1 (`BVS_Visible`) | 2 = `BVS_ExplicitlyHidden` |
| `SkelControlIndex` | **255** (no control on this bone) | 2 = this bone is driven by SkelControl #2 |

30.12's probe reported the array as **`0xFF` everywhere except `0x02` on `spine_3_jnt`**.
`0xFF` is not a legal `BoneVisibilityStates` value at all; it is exactly
`SkelControlIndex`'s "none". On that reading, 30.13 did not hide the arm bones, it
**attached them to SkelControl #2**, and "froze to the view and rode the head" is what an
attached control does. A hidden bone does not ride anything.

That is why the mismatch matters beyond bookkeeping: the existence proof the whole route
rested on ("the game hides `spine_3_jnt` this way in first person") is probably not a
hiding at all - it is the game pointing the chest bone at a bone controller.

### The instrument that settles it without another guess

This build ships full UE3 reflection: `FindPropOffset(class, property)` reads
`UProperty::Offset` out of GObjects, the same technique the crouch cylinder (`CylinderComponent::CollisionHeight`)
and the graft (`SkelControlBase::*`) already use. So the offsets do not have to be
pattern-matched at all. `BoneVisResolve()` in `hands/arms_hide.cpp` asks the engine for
`SkeletalMeshComponent::BoneVisibilityStates`, `::SkelControlIndex` and `::RequiredBones`,
prints all three against `0x288`, and names which one the 30.12 probe actually found. A
`0` from any of them is itself an answer: this build does not declare that property.

**Do not re-derive a per-bone array by pattern-matching a byte range. Ask the reflection.**

### The lever, and what it can prove

`arms vis on|off|status|chain` (ini `[Hands] BoneVisHide`, **ships ON for the VR-31
test sessions**, back to off once the verdict is recorded) writes
`BVS_ExplicitlyHidden` into the arm-chain bones - collarbone, shoulder, upper arm, lower
arm, sleeve; never hands or fingers - at the offset the engine named, saving and
restoring the exact bytes. Three guards refuse before writing, each logging its numbers:

- the property does not exist on this build;
- the array is **empty** (`num=0`), which means the engine never allocated it and there is
  nothing per-bone hiding can drive. That would close route (a) with a reason rather than a
  failed write, and it is the same fact 30.17 saw when `HideBoneByName` allocated nothing;
- the array's length does not equal the rig's ref-skeleton bone count, so it is not
  per-bone for this mesh and our indices would land somewhere else.

While it is on, a census runs on the script lane and reads back what it wrote, classifying
every byte `held` / `reverted` / `other` against the value the engine had. This is VR-30's
method note applied: **measure write survival before tuning what the write contains**. It
separates two outcomes that look identical in a headset:

- `reverted` climbing: the engine puts the bytes back, and the write needs a later lane.
- all `held` and the arms still on screen: the write survives and **the renderer does not
  read this array**, which closes route (a) properly and hands the question to route (b),
  the c6 bone palette.

A warning that must ride along, ported from BioShock and not yet paid for here: UE3 gives
a hidden bone a zero transform, and the attachment path inverse-decomposes bone scale. If
the weapon's attach bone is in the chain, the weapon goes through the near plane. `arms vis
chain` prints the exact bone list so a report identifies the culprit rather than describing
it.

### MEASURED (2026-09-06, run 1, build `alpha-298-g5809b088`): both answers at once

One gameplay run, lever armed from the ini, nothing sent by hand:

    bonevis: reflection (prep) - BoneVisibilityStates +0x000, SkelControlIndex +0x288,
             RequiredBones +0x23c (0 = the engine does not declare that property on this class)
    bonevis: +0x288 - the array 30.12 found and 30.13 wrote 0x02 into - is SkelControlIndex.
    bonevis: no BoneVisibilityStates property - route (a) is CLOSED on this build,
             and not because a write failed

**1. `+0x288` is `SkelControlIndex`. Confirmed, not inferred.** The prediction made from
30.14's symptom was right: 30.13 pointed the arm bones at SkelControl #2 rather than hiding
them, which is why they froze to the view and rode the head. The `0xFF` default was never a
visibility value. `RequiredBones` resolving at `+0x23c` proves the resolver was working on
the right class, so the `0` for `BoneVisibilityStates` is a real absence and not a lookup
that failed.

**2. Route (a) has nothing to write into.** This build's `SkeletalMeshComponent` does not
declare `BoneVisibilityStates`. That is **consistent with** 30.17 and is the likeliest
reading of it, but it is not a proof of it - nothing here inspects the native's
implementation, and a native is free to keep state somewhere the script layer never names.
Say "no script-visible array to drive", not "this is why the native did nothing":
`HideBoneByName`
dispatching and allocating nothing is the same fact seen from the other side - there is no
array for it to allocate.

**The existence proof the route rested on is gone with it.** "The game hides `spine_3_jnt`
this way in first person" was the whole reason to try route (a), and the game is not hiding
it - it is pointing that one bone at a bone controller. Nothing in the corpus now says
Dishonored has per-bone hiding at all.

**Run 2 (2026-09-06, `alpha-299-ge4343852`) closed the subclass reading:**

    bonevis: no UProperty named BoneVisibilityStates on ANY class in GObjects,
             so it is not merely on a subclass

So the property is absent from the whole reflection corpus, not hidden on a subclass the
first lookup constrained itself out of. One reading is left - the array existing in C++
without script exposure - and run 2 did **not** answer it, through an instrument bug worth
recording because it is a repeat of a shape this project keeps paying for: **the two
cross-checks have different preconditions and were run at the same moment.** The name search
needs only GObjects and answered on the first tick; the scan needs a prepped rig, and the
first tick is about ten seconds before the pawn is latched (measured: close-out at 37135781,
`handmesh: latched pawn` at 37145234). It logged `could not prep the rig`, and the one-shot
flag then stopped it from ever trying again. The scan now retries until `g_fpCandN` and
`FpPawn()` say there is a rig, and the precondition is checked at the call site because
`ArmsPrep` logs its own failure and would otherwise bury the run.

**Run 3 (`alpha-299-g8f556300`) then found the SECOND precondition, and it is a fact worth
keeping: the first-person candidate list does not exist unless the hand-mesh drive is on.**
Both of `FpCollect`'s callers (`FpCycle`, and the collect inside `ApplyHandToMeshInner`) sit
behind that drive, and the tester runs `[Hands] Enabled=0` - so `g_fpCandN` was 0 for the
whole run, the pawn latched fine, and the scan waited for a list nobody was ever going to
build. **Anything that needs the component graph must serve its own `FpCollect`, or say in
its refusal that it is waiting on a subsystem the config has switched off.** The scan now
calls `FpCollect` itself, at most five times at 2 s apart, and names why in the log.

### VERDICT (2026-09-06, run 4, `alpha-299-g54d45a04`): route (a) is closed on evidence

The scan ran. Five per-bone byte arrays of length 79 exist on the rig, and **not one has its
values confined to 0..2**, which is what a `BoneVisibilityStates` must look like:

    bonevis: rig prepped for the scan - 10 arm-chain bone(s) of 79
    bonevis: scan +0x208 num=79 zeros=37 ones=0 twos=0 other=42 range 0..243 -> not a visibility array
    bonevis: scan +0x214 num=79 zeros=43 ones=0 twos=1 other=35 range 0..243 -> not a visibility array
    bonevis: scan +0x23c num=79 zeros=1  ones=1 twos=1 other=76 range 0..78  -> not a visibility array
    bonevis: scan +0x248 num=79 zeros=1  ones=1 twos=1 other=76 range 0..78  -> not a visibility array
    bonevis: scan +0x288 num=79 zeros=1  ones=1 twos=1 other=76 range 0..255 -> not a visibility array
    bonevis: scan done - 5 per-bone byte array(s) of length 79 on the rig

**Route (a) is closed**, on three independent instruments rather than one lookup returning
zero: the property does not resolve on `SkeletalMeshComponent`, no `UProperty` of that name
exists on **any** class in GObjects, and no byte array on the component is shaped like one.

Read the scan's rows with its limitation in mind, because two of them are old friends: the
scan matches any `TArray` whose `ArrayNum` is the bone count, **regardless of element size**,
then judges by the first `num` bytes. `+0x208` and `+0x214` are attempt 1's `SpaceBases` and
`LocalAtoms` (arrays of `FMatrix`), so their "bytes" are matrix data read sideways;
`+0x23c` / `+0x248` range 0..78, which is bone indices, and `+0x23c` is the `RequiredBones`
reflection already named. The value test rejected all five correctly, and a genuine byte
array of visibility states would have passed it, so the conclusion holds - but do not read
those rows as five candidate visibility arrays.

### What the same run found instead: route (c) has somewhere to point

`SkelControlIndex` on the arm chain, the array this build actually has:

| bone | index | state |
|---|---|---|
| 20 `Collarbone_R_Jnt` | **0** | the game drives it |
| 21 `shoulder_R_jnt`, 22 `upper_arm_R_jnt`, 23 `lower_arm_R_jnt`, 48 `sleeve_R_jnt` | 255 | free |
| 49 `Collarbone_L_Jnt` | **1** | the game drives it |
| 50 `shoulder_L_jnt`, 51 `upper_arm_L_jnt`, 52 `lower_arm_L_jnt`, 77 `sleeve_L_jnt` | 255 | free |

**8 of 10 free, and the 2 that are taken are exactly the two collarbones** - the roots of the
chain, driven by controls 0 and 1. Those two controls are not yet identified; the mod's own
control enumeration was off this run (`[Hands] Enabled=0`), so naming them is one cheap run
with the hand drive on.

This also finishes explaining 30.13. It wrote `2` onto the whole chain, which pointed the
free bones at **SkelControl #2** - a control the game owns and that 30.14 recorded as making
the arms "ride the head". The project has a name on file for a camera-tracking bone control
on this rig (`LookAtControl_Camera`, a7b18b6f, "the camera's bone control - do not zero it").
Whether #2 is that control is unverified and worth one run to settle, but the mechanism no
longer has any mystery in it: 30.13 attached arm bones to a camera-following control.

### Where the VR-31 verdict stands

- **(a) per-bone visibility: CLOSED.** No array, on three instruments.
- **(b) the c6 bone palette: OPEN and proven writable.** Sizes separate the uploads
  (**x36 = sword, x144 = arms, x204 = NPC**); zero the x144 block, leave x36, and that is
  "hide the arms, keep the weapon". Machinery in `src/legacy/rtd_drive.cpp`. Cost: it sits on
  the hottest D3D9 entry point.
- **(c) SkelControlIndex: OPEN and new.** One byte per bone, 8 of 10 free, and the mod
  already drives SkelControls (`hands/skelcontrol.cpp`, `hands/graft.cpp`). Cheaper per frame
  than (b) by a wide margin. The unknown is what a free bone can be pointed AT: the index
  selects from the AnimTree's control lists, so this needs the control list understood before
  a byte is written, and 30.13 is the standing warning about pointing bones at a control
  somebody else owns.


## VR-31 research review: geometry visibility before bone controls (2026-09-06)

Scope: source and recorded-evidence review at `7d8b602e`, including the locally
decompiled `tools/uscript/dishonored` corpus. No runtime changes or new gameplay
measurements. The goal is to remove arm geometry while retaining animated hands
and weapon attachments; absolute controller placement remains VR-52.

### Recommended first experiment: material-section visibility

These are declarations in this game's own corpus, not offsets borrowed from UDK:

| local source under `tools/uscript/dishonored/` | finding |
|---|---|
| `Engine/SkeletalMeshComponent.uc:415` | native `ShowMaterialSection(MaterialID, bShow, LODIndex)` |
| `Engine/SkeletalMeshComponent.uc:85` and `:203` | per-component LOD records contain `HiddenMaterials`; component owns `LODInfo` |
| `Engine/SkeletalMesh.uc:56` and `:146` | `BodyPart` associates owner/cut bone names and a show-if-cut flag; asset has `m_MaterialsToBodyParts` |
| `Engine/SkeletalMesh.uc:145` and `:114` | asset material array and per-LOD `LODMaterialMap` |
| `DishonoredGame/DisSkeletalMeshComponent.uc:6` | `UsedMaterial` contains material index and shown flag; transient array at `:18` |
| `DishonoredGame/DishonoredPlayerSkeletalComponent.uc:1` | player component inherits that Arkane component |
| `DishonoredGame/DishonoredPlayerPawn.uc:1191` | player `pMesh` is foreground, owner-only, and forces attachment updates in tick |

**Inference:** the shipped material visibility machinery could suppress sleeves
without changing bone transforms, if hands and sleeves use separate sections.
That would preserve the engine's authored animation and attachment inputs and
avoid a new per-upload matrix rewrite. No script body or asset material inventory
in this review proves that split exists. Material names alone cannot prove it:
one material can cover both sleeve and hand geometry or several sections.

The previous blanket dismissal of `m_UsedMaterials` confused its irrelevance to
arm rotation with its possible relevance to geometry visibility. Its native
writers are not available in these scripts; treat it as census evidence first,
not a raw-write target. Prefer the native section API, after resolving and
checking its reflected parameter layout, to editing an internal array directly.
The previous inert HideBone calls are also a reason to demand a visible result
from this native, not just successful ProcessEvent dispatch.

### Route (c): usable topology, but not selective visibility

`Engine/AnimTree.uc:25` defines each `SkelControlListHead` as a bone name,
`ControlHead`, and editor coordinate; `SkelControlLists` is declared at `:136`.
`Engine/SkelControlBase.uc:40` supplies `NextControl`. Read the live tree reached
through the component's `Animations`, not its shared `AnimTreeTemplate`.

The proposed identification run is insufficient as written. In
`src/game/dishonored/hands/skelcontrol.cpp`, `SkelControlProbe` walks GObjects and
appends matches to `g_skcPlayer[]`. Its printed control number is that buffer's
slot, not the index of an AnimTree list. It never reads `SkelControlLists`.
It also filters to `SkelControlSingleBone` after caching that class, so it is
not a complete census of all control types. Name discovery cannot establish
that list 2 is `LookAtControl_Camera`. Walk the actual lists and chains, then
cross-check their bone names against both pre- and post-physics index arrays.

`Engine/SkelControlBase.uc:37` declares `BoneScale`; `SkelControlSingleBone`
adds translation/rotation fields, not a flag to hide only that bone's geometry.
Epic's UE3 documentation states that scaling includes child bones and describes
ordered chains and sharing a control among bones. Thus a free index is useful
plumbing, but does not solve the arm/hand boundary.
[Epic: Using Skeletal Controllers](https://docs.unrealengine.com/udk/Three/UsingSkeletalControllers.html)

The repository already warns of this in
`src/mod/state/12_game_dishonored_hands_skelcontrol.inc:310`: hand scaling affects
descendants and the weapon. Its drive also excludes scale values at or below
0.05 (`hands/skelcontrol.cpp:910`); the existing size slider is not a zero-scale
hiding test. The later graft has its own scale writer (`:1231`), so a test must
account for both writers rather than assume one field has sole authority.

An arm scale of zero normally collapses descendants too. Restoring the hand
would require a measured later transform/scale override; a finite reciprocal
cannot undo a zero scale. A small nonzero scale with compensated descendants
is an experiment with wrist deformation, transform order, and numerical issues,
not a proven shortcut. A leaf sleeve bone could be a cheaper special case if
its hierarchy and influenced vertices confirm it has no required descendants.

Introducing an owned control also means object allocation/lifetime, list and
tick registration, rebuild handling, and restoration. The existing template
donor graft explicitly records template contamination across reloads in
`hands/graft.cpp`; do not promote that prototype to a clean ownership solution.
There is no measured basis for claiming route (c)'s total frame cost is lower
by a wide margin solely from the number of bytes patched.

### Route (b): distinguish whole-draw hiding from floating hands

The previous x36/x144/x204 observations remain useful signatures for those
recorded draws. They are not unique mesh IDs across every weapon, LOD, NPC,
render pass, or DLC. The active upload interception is in
`src/core/framework/vs_const_hook.cpp:210`; `src/legacy/rtd_drive.cpp` computes
drive transforms, and `src/legacy/fp_mesh.cpp:552` contains the older whole-block
and per-bone collapse routines.

Zeroing a whole x144 palette removes all geometry using that palette, including
hands in that draw. That supports floating weapons, not retained native hands.
For selective bone collapse, the mapping is still unknown: 144 registers at
three per bone represent 48 entries, while the rig census has 79 reference
bones. Reference index 52 cannot be used as slot 52 in that upload. The hook
also records separate arm draws (`vs_const_hook.cpp:223`). Establish each
draw's palette-to-reference mapping, rather than reusing skeleton indices or
assuming upload order stays fixed.

Even a correct mapping does not make skinning a per-triangle visibility mask.
A wrist vertex blended between a retained hand matrix and a collapsed forearm
matrix receives both contributions. Collapsing selected bones can stretch or
distort those triangles instead of removing them. Changing only basis entries
also leaves bone translations apart, so it does not necessarily degenerate
the whole arm. Inspect the vertex weights and test the wrist boundary.

The constant hook already exists. A narrowly gated patch does not imply another
hook installation, allocation, GObjects walk, or synchronous readback per call.
Only matched draws need a bounded copy and edits. Measure incremental cost;
being in a frequently called function alone does not establish unacceptable
overhead. Geometry correctness is the first unresolved question.

### Other alternatives and the next discriminating test

1. **Read-only census, independent of `[Hands] Enabled`.** Start at the live
   pawn's reflected mesh pointer, stamp component/asset/tree identities, resolve
   material and LOD tables, and print body-part associations and actual control
   list indices/chains. Validate reflected field sizes, bool masks, array lengths,
   and struct strides. This also avoids interpreting native mirror declarations
   such as `RefSkeleton`'s `array<int>` as their actual C++ element layout.
2. **Section A/B if there is a plausible sleeve/hand split.** Save existing
   visibility, hide one identified section through the native, verify geometry
   and visibility readback, then restore exactly. Cover applicable LODs and
   check whether Arkane's material bookkeeping overwrites the change. Prove
   hands/fingers, sword, offhand item and power effects remain; verify VR-30
   yaw behavior, crouch, reload, checkpoint reload and both eyes. One behavioral
   change per build; run the simulator before requesting headset judgment.
3. **If sections are mixed, prefer a render-only geometry mask experiment.**
   A draw/section-specific triangle filter or wrist mask can preserve the full
   skeleton and remove actual arm geometry without inherited scale. This needs
   local mesh/UV/weight inspection and reliable draw identity, plus resource-reset
   handling if using a filtered index buffer. It is more implementation work,
   but has a clearer geometry boundary than bone collapse. A compatible hands-only
   replacement mesh is another option, with asset compatibility and packaging
   work; no game assets or decompiled sources belong in the repository.
4. **Simpler fallback: floating weapons.** Hide player skin through validated
   sections or a positively identified whole draw while preserving the animated
   rig. Whole-component `SetHidden`, `bHideSkin`, and body-mode changes are
   broader candidates with attachment/update consequences to measure. Do not
   disable the skeleton or camera carrier. Showing native hands only during
   powers is a separate state-driven visibility policy and does not by itself
   remove sleeves during those powers.

The existing array experiment stays retired. Its scan was limited to inline
TArray-shaped headers in the first 0x1000 component bytes with length equal to
the bone count. No reflected property plus no matching live header is strong
evidence against that specific poke, not proof against every lazy, indirect,
or differently encoded native implementation. In particular, lack of allocation
does not establish why `HideBoneByName` did nothing. Do not spend another tester
run repeating it without new native-code evidence.

**Verdict:** material-section hiding is the best next test, conditional on the
asset split. Route (b) remains a rendering fallback with mapping and seam
questions; route (c) is lower priority for this visibility goal. No floating-hand
implementation is proven by this review.

### Run 5 (`alpha-302-gd939b9de`): the census read nothing, and it was the probe

Every column came back 0 and the experiment refused. The reflection line is where the fault
is, and it is mine, not the engine's:

    mat: reflection (auto) - LODInfo +0x318, Materials +0x000, m_UsedMaterials +0x440,
         m_MaterialsToBodyParts +0x078

**`Materials` is declared on `MeshComponent`, not `SkeletalMeshComponent`.** The lookup
constrained `Outer` to the wrong class, returned 0, and the census printed `materials=0` as
though the mesh had no sections at all. Second fault on the same line: **a component's
`Materials` is an OVERRIDE list and is normally empty** - the authoritative section count is
`SkeletalMesh::Materials` on the ASSET. So even a correct component lookup would have read 0.

**The rule this pays for, and it is the same shape as the `+0x288` misidentification:
resolve a property by NAME and let the class be a hint, never a constraint.** `MatProp` now
tries the expected class, falls back to a name-anywhere search across GObjects, and logs
which class actually declares it. A `0` after that is a real absence.

Two things the run did establish, both real:

- **`ShowMaterialSection` exists and resolved** (`UFunction @ 10207020`), so the native is
  there to dispatch if there is anything to dispatch it at.
- **`LODInfo` (+0x318), `m_UsedMaterials` (+0x440) and `m_MaterialsToBodyParts` (+0x078) all
  resolved**, and `m_MaterialsToBodyParts` read 0 entries on the player asset. That is the
  expected answer rather than a fault: Arkane built the body-part table for dismemberment,
  which is an NPC feature, so **the player rig has no owner-bone names to pick from** and the
  NAMED path was never going to have targets. The SWEEP path is what settles route (d) on
  this rig, and it needs the asset's section count, which is exactly what was misread.

A third fact the log could not distinguish and now can: `FpAssetObj` returning NULL and an
asset with an empty table printed identically. The census names which it was.

### Route (d) is built and ships the census ON (2026-09-06, session 18)

The review's recommended order is implemented, and its correction to my own next step is
accepted: **turning the hand drive on cannot name SkelControl list indices.** The existing
probe fills `g_skcPlayer[slot]` in GObjects iteration order, filtered to
`SkelControlSingleBone` and capped at 8 (`hands/skelcontrol.cpp`, the discovery loop). Those
slot numbers have no relationship to the values in `SkelControlIndex`, which index the
component's own AnimTree control lists. The suggestion to "run once with `[Hands] Enabled=1`
to name controls 0 and 1" was wrong and is withdrawn; naming them needs a walk of the real
control lists, and `SkelControlLists` is a C++ member this build does not declare to script,
so it is an offset derivation rather than a reflection lookup.

`hands/mat_hide.cpp` does two things, both aimed at one launch rather than two:

**The census, `[Hands] MatCensus=1`, read-only, automatic once per run.** For every
first-person component it prints the material count, `LODInfo[0].HiddenMaterials`,
`DisSkeletalMeshComponent::m_UsedMaterials`, and - the row that decides the route - the mesh
asset's `SkeletalMesh::m_MaterialsToBodyParts`, with the `m_OwnerBone` and `m_CutBone` FNames
resolved to strings. All four offsets come from reflection, and a `0` for any of them drops
that column and says so. It serves its own `FpCollect`, so it does not depend on the
hand-mesh drive being on - the precondition that cost run 3.

**The hide, manual.** `arms mat hide <id>` dispatches the engine's own
`ShowMaterialSection(MaterialID, bShow, LODIndex)` through ProcessEvent, exactly as
`console.cpp` dispatches `ConsoleCommand`. It is never a raw write into `HiddenMaterials`:
the native is what notifies the render thread, and a value verified in memory but never
honoured downstream is this project's oldest trap. The call reads the flag back and prints
whether it moved, so the dispatch has an acceptance test rather than a return code.
`arms mat show <id>` and `arms mat restore` undo it.

**How to read the census.** One row per material index. A row whose `owner` is a sleeve or
arm bone, with the hand on a DIFFERENT row, is route (d) working: hide the first, keep the
second, and no bone is touched. Every row sharing one owner bone means the asset has no
arm/hand split and the route is closed on the same day it opened.

**Why this is worth a run before (b) or (c).** Both of those fight the skeleton and inherit
its problems - `BoneScale` propagates to child bones, so shrinking a forearm takes the hand
and whatever is attached to it, and the c6 palette holds 48 entries against 79 reference
bones with wrist vertices weighted to both sides of any cut. Route (d) removes geometry from
a draw and leaves every transform alone, which is the property the other two have to buy.

**Not established**: that the player asset has the split, that the native works on this
component, or that the sections divide where the body parts do. Only the run says.

### SOLVED: route (d) WORKS - ShowMaterialSection hides first-person geometry (2026-09-06)

Headset-judged on `alpha-304-ge1135d25`. The sweep ran four steps and the tester reported
geometry disappearing and coming back on each one. **`ShowMaterialSection` dispatched through
ProcessEvent removes first-person geometry and the game keeps running normally.** That is
route (d) proven and the first thing in VR-31 that actually hides anything.

The plan the engine's own `GetNumElements` produced, and what it means:

| step | component | sections | LODs |
|---|---|---|---|
| 1 | `Skm_Player` (DishonoredPlayerSkeletalComponent) | 1 | 1 |
| 2 | `Wpn_PlySword01` (DishonoredItemSkeletalComponent) | 1 | 1 |
| 3 | `crossbow_01` (DishonoredItemSkeletalComponent) | 1 | 1 |
| 4 | `bolt_01` (DishonoredItemSkeletalComponent) | 1 | 1 |

Two components reported 0 sections and were skipped (`asset=NOT FOUND` on both), and the
material names came back as `MaterialInstanceConstant` for the body and the sword, with
`crossbow_01_Mat` and `Bolt_01_Mat` for the other two.

**The structural finding, and it is the one that shapes what comes next: EVERY component has
exactly ONE material section.** So route (d) hides per COMPONENT, not per body part. There is
no arm section and hand section to separate, because the whole first-person body is one
section on `Skm_Player`. The tester's step-by-step recollection was approximate and the
mapping of which step removed which limb is **not yet established** - that is what the numpad
cycler is for, and until it has been walked deliberately the per-step attribution above
should be read as the PLAN, not as what was seen.

**What this gives immediately**: floating weapons. Hide `Skm_Player` and the sword, crossbow
and bolt keep drawing, because they are separate components with their own sections.

**What it does not give**: hands. Arms and hands share one section, so hiding the arms hides
the hands with them. Route (d) cannot split them, and no amount of material work will - the
split does not exist in the asset.

**So floating HANDS needs a different source for the hands, not a finer cut of this mesh.**
The mod already has one: `core/gfx/hand_mesh.cpp` draws its own hand geometry, and the
SkelControl drive already places hands from the controllers. Hiding `Skm_Player` and drawing
our own hands is the BioShock shape, arrived at from the opposite direction - and it also
answers the powers requirement, since our own hands can be shown for powers and hidden
otherwise without touching the game's mesh at all. Untested here, and it is the next thing.

The cycler (`[Hands] MatCycle=1`, Numpad 3 next / Numpad 1 back / Numpad 2 all visible) walks
the same plan at a human pace so each section can be identified deliberately. **The hotkey
only posts a request; every dispatch happens on the script lane** in `MatCycleTick`, because
ProcessEvent from the present thread is the lane error this project has a rule about. The
timed sweep is back OFF now that the route is proven, so the two cannot fight.

### What the cycler can still settle, and what the census already settled (2026-09-06)

Worth stating before the walk, so the run is not spent proving something already on the log.
The census read the component list off the engine, and there is **exactly one component with
first-person body geometry**: `Skm_Player`. The other two that report sections are named
weapons, and the two that report none report `GetNumElements=0` from the engine itself. So
the arms, the hands and whatever else the body carries **cannot** be on separate steps -
there is nowhere else for them to be.

That leaves the walk one real question, not four:

- **step 1 (`Skm_Player`)**: does hiding it remove BOTH arms and BOTH hands, and do the
  weapons keep drawing? That is the floating-weapons verdict, and it is a yes/no on one step.
- **steps 2-4** confirm the named weapons come off individually - useful, cheap, but they
  only corroborate names the engine already gave.

The cycler still earns its walk (a component's NAME is not proof of what it draws on screen),
but the outcome that would change the plan is a surprise on step 1 - hands surviving it, or
geometry the census did not account for.

### VR-31: the hand renderer had no caller, and the frustum it wanted was the wrong one (2026-09-06)

Route (d) sends floating hands to a different SOURCE for the hands: `core/gfx/hand_mesh.cpp`,
which the mod has carried since 30.77. Reading it before wiring it turned up two things worth
recording, because neither is visible from the config or the ini.

**1. The renderer has been dead code since 41.0, and nothing said so.** `HmRenderEye` drew
into the per-eye render targets of the side-by-side present pipeline, and that pipeline was
deleted in `cc2fa936` ("remove the side-by-side present pipeline"). Since then:

- `HmRenderEye` has had **no call site at all** (`git log -S` names `cc2fa936` as the commit
  that took the last one);
- `HmEnsurePipeline` has had none either, so the shaders were never compiled;
- `[VRHands] Enabled=1` therefore changed **nothing on screen** and produced **no log line
  saying so**. A lever that silently does nothing is the fault class this project's logging
  rules exist for, and it survived because the config block, the ini keys and the model
  auto-pick (`HmPickModels`, called every tick from `skelcontrol.cpp`) all still ran.

The removal was correct at the time - the deleted code's own comment says hands "return with
the projection-layer mode" - and the projection-layer mode has been live since S2b. The
caller is what was never rebuilt.

**2. The hand pass must be composed through the LAYER'S CLAIM, not the headset's FOV.** The
frustum the hand pass reads (`g_eyeFr`, filled in `DvrPresentPoses`) was built from
`headset_half_fov_deg`. But while a projection layer is up the compositor shows the eye
texture across the fov the **layer claims**, and that claim is the engine's own rendered hfov
(`camera+0x53c` -> `DvrFovHandoff` -> `set_rendered_hfov`), not the headset's half-angles -
on the run that proved route (d) the claim was **108.1 deg**. Hands composed through the
headset's frustum would be drawn at a different angle from the world they are meant to stand
in, and would slide against it whenever the claim moved.

The derivation now matches the runtime's own, so the two cannot drift apart:

    tan(halfH) = tan(claimedHfov / 2)
    tan(halfV) = tan(halfH) * h / w        (h, w = the method's output texture)

which is exactly `openxr_runtime.cpp`'s `halfV = atan(tan(halfH) * g_swapH / g_swapW)` for
the projection views it submits. The numbers print on change (`vrhands: eye frustum from ...`)
so "the hands sit at the wrong angle" is arithmetic rather than opinion.

**3. Three other things the deleted pipeline supplied.** A **depth buffer** (the pass batches
by skin and depth-tests; the painter's sort is gone, and with no DSV bound the depth state is
inert and the back of a hand paints over its front); the **eye tag of the pixels already in
the target**, which is not the eye the next game draw will render; and a **refusal on the
quad screen**, because a head-locked quad is a picture at `[Screen] DistanceMeters`, not a
world, and eye-frustum hands have no depth there to occupy.

**Status: wired, built, UNVERIFIED.** Nothing here has been seen in a headset. `[VRHands]
Enabled` ships OFF, `vrhands on|off|status` is the live A/B, and the 3 s beat line prints
calls / draws / triangles and **names the reason for any zero** rather than leaving one.

### SOLVED (cause): the hands drew 252 triangles a present through an UNINITIALISED matrix (2026-09-06)

Run on `alpha-307-gf297df53`, first headset run of the rebuilt hand pass. The tester saw
no hands. The log said everything was healthy:

    vrhands: pipeline ready
    vrhands: depth buffer 2750x2850 D32 (29.9 MB)
    vrhands: beat calls=480 draws=480 tris=120960 last=drew

480 calls per 3 s, 252 triangles each - 132 + 120, both models complete, every present.

**The cause, found by review of the code rather than by another run.** The transform did:

    float B[9]; RtdBuildYPR(rad, B);

`RtdBuildYPR` lives in `src/legacy/rtd_drive.cpp` and is compiled **only** under
`-DDVR_WITH_LEGACY=ON`. Every shipped build takes the stub in
`src/legacy/legacy_stubs.inc` instead:

    static void RtdBuildYPR(const float* ypr, float* out) {}

So `B` was never written, and every hand vertex was rotated by nine floats of whatever
was on the stack. Undefined behaviour with a very specific signature: the counters stay
healthy because the geometry IS built and submitted, and nothing is visible because the
vertices are scattered or non-finite.

**The reason it survived the first instrument.** The near-plane reject is
`if (es[k][2] > -0.03f) bad = true;`, and `NaN > -0.03f` is **false**, so a non-finite
vertex passes it and is counted as a good triangle. The counter measured "built", the
question was "visible", and nothing in between was measured. The fix separates them:
`submitted` and `ON-SCREEN` are now different numbers, with `nonFinite`, `nearPlane`,
`degenerate` and an NDC bounding box beside them.

**How far the class extends, measured.** Of the 23 stubs in `legacy_stubs.inc`, exactly
**one** had an out-parameter that a live caller read unconditionally, and it is this one.
The only other stub with out-parameters is
`RtdSnapshot(float* R, float* T, ...) { return false; }`, and its caller
(`vs_const_hook.cpp`) gates every read on that return (`if (ok[0] || ok[1])`, then
`if (!ok[h]) continue;`), so `R`/`T` are never touched - safe by construction. The
remaining 21 are `void f(...)` with no out-parameters and are genuine no-ops, which is
what the design intends.

**The rule.** A stub that returns void and writes nothing is a no-op; a stub with an
OUT-PARAMETER is a landmine, because the caller cannot tell a value it never received
from a value it did. If a retired experiment must keep a stub with an out-parameter,
the stub has to initialise it - and a live path should not depend on a retired
experiment at all. The math this one needed is twelve lines and now lives beside its
caller as `HmBuildYPR`.

**A second defect in the same function, found reading it.** The vertex depth was
`pos.z = 0.5f * (-z)` with `pos.w = -z`, which divides to **exactly 0.5** at every
distance. The depth buffer therefore could not order anything - the first fragment to
reach a pixel won and every later one failed `LESS`. The painter's sort had been deleted
on the assumption depth replaced it. Now a standard mapping, 0 at the near plane and 1
at the far.

**Convention note.** `RtdBuildYPR` is commented "Z yaw, Y pitch, X roll" - the UE3 world
frame. The hand meshes are built in the CONTROLLER's frame, which is Y-up (+X right,
+Y up, -Z along the grip), so `HmBuildYPR` is Ry(yaw) * Rx(pitch) * Rz(roll) and the ini
keys mean what they say. Verified numerically: exact identity at zero trim, and equal to
`Ry*Rx*Rz` to 2.2e-16 over 64 angle combinations. Inert in the current configuration -
every trim value in the shipped ini is 0.0.

**Corrected from the first write-up of this run:** the claim that every present was
delivered untagged was wrong. It was read off the first three beats, which are bring-up.
Across the whole run gameplay reads `method=reentry out/s=160 L/s=80 R/s=80 mono/s=0` -
properly tagged stereo. The untagged beats are the non-gameplay states.

### VR-31: the real hands are the requirement, and route (b) starts at the DRAW (2026-09-06)

The requirement changed on tester feedback: the game's OWN hands are needed, because the
powers animate them. Replacement meshes cannot carry Arkane's power animations, so the
custom renderer (now working - see the uninitialised-matrix entry) becomes the fallback
rather than the goal.

**What is closed, and what that does NOT close.** Route (a) is closed (no
`BoneVisibilityStates` on any class). Route (d) is closed FOR HANDS (`Skm_Player` has one
material section; the headset walk removed both arms and both hands on one press). But
one shared material section rules out a MATERIAL split only. It says nothing about
whether the renderer submits the arms and the hands as separate geometry, because UE3
chunks a skeletal mesh by BONE INFLUENCE, not by material. That is route (b), it is
untouched, and it is where the work goes.

**Upload SIZE cannot answer it.** The inherited knowledge is "c6, 3 registers per bone,
x144 = arms, x36 = sword, x204 = NPC", and `[VRHands] HideGameArms` hides by matching
those sizes. Three reasons that cannot settle a split, all raised in review and all
correct:

- two chunks can carry the same bone count, hence the same upload size;
- several draws can reuse one upload, so one palette is not one draw;
- `HideSizes` is a list of what someone happened to observe, so discovery limited to it
  can only rediscover it.

The existing hook does record separate ARM draws by ordinal (`myOrd`, 30.72), but that
distinguishes left arm from right arm - it is not an arm/hand split.

**So the instrument censuses the DRAW** (`hands/draw_census.cpp`, `[Hands] DrawCensus`,
ships ON). `DrawIndexedPrimitive` is hooked (vtable **82**; the index is corroborated by
the two this file already patches, 29 `CreateDepthStencilSurface` and 94
`SetVertexShaderConstantF`, which fix the standard layout). Every draw that consumes a c6
palette is identified by what the renderer was actually holding - stream-0 vertex buffer
and stride, index buffer, vertex declaration, vertex shader, index range and primitive
count. Two draws differing in any of those are different geometry whatever their palette
sizes agree on. Numpad 6 / 4 / 5 hide one row at a time.

Two rules it observes, both from CLAUDE.md and both easy to get wrong here:

- **never take a reference to an engine D3D object inside a detour** - the four `Get*`
  calls each AddRef, so each is released on the next line and only the pointer VALUE is
  kept, as an identity token, never dereferenced;
- the hotkey posts a request and the tick acts on it. The detour itself runs on the
  RENDER thread, so the row table is filled completely before its count is published.

**Corrections to the first version of this plan, from review, recorded so they are not
re-derived:**

1. **A palette matrix's translation column is NOT the joint's position.** Skinning
   matrices normally fold in the inverse bind pose, so in bind pose they can be identity
   while the joint sits far from the mesh origin. Any "collapse the arm bones onto the
   wrist" experiment must derive the wrist point in the palette's OUTPUT space - e.g. by
   transforming the wrist's bind-pose position through its verified skinning matrix - not
   by reading a translation column. Dishonored's convention has to be confirmed first.
2. **Collapsing means a ZERO 3x3 linear part**, not an identity rotation. Identity leaves
   the geometry extended and merely moves it.
3. **Zeroing a matrix sends its contribution to the skinning OUTPUT-space origin**, not
   necessarily the world origin.
4. **Collapse does not guarantee a clean wrist.** Arm-only vertices converge only if all
   their influences receive the same point; mixed arm/hand vertices still deform, and
   triangles crossing the boundary can fan into a visible cuff. Treat collapse as a cheap
   prototype - if it seams, move to triangle filtering rather than tuning the target.
5. **The cleaner geometry answer is an INDEX-BUFFER filter**: for a verified player draw,
   submit a replacement index list containing only the hand and chosen cuff triangles,
   keeping the original vertices, weights, materials, shaders and animated palette. That
   removes arm triangles while preserving the retained triangles' original deformation,
   so finger and power animation survive without reimplementing any of it. It works even
   when arms and hands share both material and chunk. Cost: choosing the wrist boundary
   from bind-pose geometry and skin weights (topology and position, not a weight
   threshold alone, which makes holes), caching per mesh/LOD, and restoring draw state.
6. **Bisection is discovery, not a bone map.** It cannot establish every hand/finger
   influence, cannot assume a contiguous hand block, and does not prove the mapping
   survives LOD or asset changes.
7. **`FindRefSkel` is weaker than its comment claims.** The comment in `arms_hide.cpp`
   says the weapon view model must report exactly its 14 bones; the function does not
   enforce that. It accepts the first TArray in a 0x28..0x300 window whose first six
   entries resolve as names (`num` merely 8..256), so it can latch onto an unrelated FName
   array. Strengthen the structural checks before treating its output as authoritative.
8. **Controller placement is a SEPARATE acceptance gate.** `[Hands] Enabled=1` is not
   proof of 6DoF control. The target behaviour is to place and orient the WRIST from the
   controller while preserving the engine's finger transforms relative to it, tested first
   on one wrist with the arms visible and the custom hands off. Prefer an engine-side
   control, because attachments, particle origins and gameplay aim follow it; a
   render-only transform preserves visible finger animation but moves none of those.
9. The retired RTD experiments failed to decouple PLACEMENT with the transforms they
   tried. They did not falsify selective geometry FILTERING, and the distinction must stay
   explicit.

**Sequence:** identify draws -> collapse with a correct wrist target -> index-buffer
filter if the seam fails -> validate controller wrist placement -> combine, then test
Blink, Devouring Swarm, Windblast, weapons, head and stick turns, crouch, reload and
checkpoint reload.

### The alpha-303 review: four probe faults, and the rule they all share

A second review of the material probe found four faults before another run was spent on it.
All four were real, all four were mine, and three of them would have produced a confident
wrong answer rather than an obvious failure - which is the expensive kind.

1. **The visibility readback could never work.** The array helper refused an offset of 0,
   and `HiddenMaterials` is member **0** of the `LODInfo` struct, so both reads always
   failed. The census reported 0 entries and the post-call readback said "unreadable"
   whether or not hiding worked. Split into `MatArrayAt` (any offset, for reads INSIDE a
   struct) and `MatArrayProp` (requires a resolved property offset, for reads off an object).
2. **The name-anywhere fallback was unsound.** `Materials` and `LODInfo` are declared on
   several unrelated types; an offset found on `SkeletalMesh` is meaningless applied to a
   component, and logging which class it came from does not make the number valid. **A
   fallback that returns the wrong offset is worse than no offset.** Removed: every field
   names its verified declaring class and a miss is a logged refusal.
3. **The sweep could not prove what it changed.** It took `g_armComp` or candidate 0, and
   the run-5 log carries **two** player skeletal components with no evidence which draws the
   arms; it also only touched LOD 0. It now sweeps every component that reports sections and
   every LOD each declares, LOD-major so LOD 0 of everything comes first, naming component,
   section and LOD at each step.
4. **"Restore" was show-everything-it-touched.** Nothing saved the prior visibility, so a
   section the game had already hidden could be left visible afterwards. Each hide now
   records the prior flag per (component, section, LOD) and puts that exact value back.

**The rule underneath all four, and it is the same one `+0x288` and `Materials +0x000`
already paid for: a probe must be able to tell a FAILED READ from an EMPTY ANSWER.** Every
one of these faults collapsed those two into the same output. `sections=0, asset=found` is
not a closed route; it is a probe that cannot distinguish them.

The review also supplied the independent check the probe was missing:
`MeshComponent::GetNumElements()` and `GetMaterial(int)` are native and dispatchable, so the
section count and the material NAMES now come from the engine itself rather than from
another memory-layout guess. Where the dispatch and the array read disagree the log says so
and the dispatch wins. On this rig the material name is the only handle available, because
`m_MaterialsToBodyParts` is an NPC dismemberment feature and the player asset carries none.

## The hand shaders' normal path, and the palette's own scale (VR-33, 2026-09-07)

Read from the three captured hand vertex shaders, and from replaying all 28
saved packets through the shipped decomposition.

### Normals and tangents ride the bone palette

| Shader | Normal path |
|---|---|
| `DB11BB81` | Position only. No normal or tangent input |
| `F2E11B73` | Decodes normals AND tangents and multiplies both through the SAME weighted bone-palette linear rows the positions use; the bitangent comes from their cross product and a handedness term |
| `11DD5E8A` | The same palette-driven tangent frame, plus light-direction handling |

**Consequence:** a proper rigid transform composed onto the palette carries the
tangent frame with the geometry. No separate normal matrix is needed and none
exists.

**`WorldToLocal` MUST NOT be rotated.** In these shaders it converts view and
light vectors INTO component space. It is not a missing skinning-normal matrix,
and transforming it as well would apply the correction twice to lighting. The
component's own transform is unchanged by a palette edit, so `WorldToLocal`
stays correct as it is.

This closes the question for these three hashes only. A weapon shader is a
different hash and must be audited before the same reasoning is used on it.

### The palette matrices are uniformly scaled rotations, and the scale is derived

Every palette matrix measured is a rotation times a uniform scale with no shear.
Over all 28 packets, taking the anchor's dominant slot:

```
28 packet(s): 28 decomposed, 0 refused. dominant slot 10 (the same in all)
uniform scale 0.999511659 .. 0.999512255
worst anisotropy 5.960e-07 | worst orthonormality residual 9.835e-07
```

An independent check over all 48 slots in all 28 packets (1,344 matrices) gives
the same scale range and residuals at numerical precision.

**The scale is DERIVED per call, never hard-coded.** 0.999512 is what these
captures held, not a property of every future pose, mesh and pass. Only the
frame used to build the correction is normalised; the rendered palette keeps its
own scale, because the correction is composed onto the original matrices.

### The weighted BLEND is not a rotation

The same test on the anchor's weighted blend, all 28 packets: `det 0.970`,
largest Gram off-diagonal 0.0075, diagonal off by up to 0.026. Averaging
rotations contracts them. This is why orientation is read from one slot and only
the POSITION comes from the blended anchor patch.

### A slot is a render slot, not a joint

Dominant weight does not establish that a slot follows the palm rather than a
finger or the forearm, and a skinning matrix can fold in an inverse-bind
rotation, so its axes are not anatomical axes. Any constant bind orientation is
absorbed into the grip transform. What has to be measured is only that the slot
follows the palm rigidly.

Across the 28 packets the dominant slot's frame moves at most **1.05 degrees**.
The blend's reading over the same packets was identical to five decimals and
looked frozen, so the slot does respond to the engine's animation - but these
captures are all one near-idle pose and this does NOT establish that it tracks
the palm. Only a run in which the game animates the hand can.

### The lane contract for the hand draws

`MpDriveTick` runs from `present_tick.cpp` -> `DcTick` -> `MsTick`, on the
present thread. `MsDraw` runs on whichever thread the renderer draws on. The
two thread ids are now recorded and printed (`ms/palette/lane:`), so the
contract is measured rather than assumed - and the controller pose crosses
between them as one whole structure under a lock, with a generation, latched
once per ORIGINAL DRAW so both hands of a draw share it.

A group of floats followed by setting a validity flag is NOT publication: the
flag is already true from the previous sample, so a reader can combine new rows
with old ones and never know.

### The XR-to-game pose mapping is a MIRROR, and that is a convention

MEASURED 2026-09-07 over 116,908 hand draws in one run. The draw's camera basis
`B` (columns right, up, forward, recovered from the ViewProjection rows) is
RIGHT-handed, so with `F = diag(1,1,-1)` the composite pose mapping

```
M = B * F * transpose(R_head)
```

has determinant **-1**. It is a reflection between XR's frame and the game's
camera-relative world frame - which is what a right-handed runtime and a
left-handed engine should produce.

The first rotation build demanded that `B * F` be a PROPER rotation and refused
**every single draw** (`placed 0 refused 116908`). The guard was wrong, not the
game. The fail-soft held: all 116,908 draws still placed translation-only, so
the hands behaved exactly as the previous build and nothing regressed - the run
looked like "nothing changed" and the log said precisely why.

**The reflection carries through and cancels.** With `s = det(M) = +/-1`:

| quantity | determinant |
|---|---|
| `O_C = M * R_ctl` | `s` |
| `G = transpose(O_C) * (R_L * R_src)` | `s` |
| `O_C * G` | `s * s = +1` |
| `D.r = transpose(R_L) * (O_C*G) * transpose(R_src)` | `+1` |

so what is finally composed onto the palette is a proper rotation at every
controller pose, whatever the parity. Conjugation by an orthogonal `Q` sends a
rotation of angle `theta` about axis `a` to one of the SAME angle about
`det(Q) * Q a`, so a mirrored frame reverses the axis and preserves the angle -
the correct physical transport in a mirrored coordinate system, not a fault to
be patched out with a sign flip.

Only ORTHONORMALITY is still required, because a non-orthogonal basis is a
broken read rather than a convention, and transpose would not be its inverse.

### The hand draws and the pose tick are ONE thread

MEASURED in the same run: `ms/palette/lane:` reports the pose published on
thread 14224 and the draws consumed on thread 14224 - the same lane - with
0 draws seeing a stale snapshot over 11,881 publications.

This does NOT retire the locked snapshot. The contract is now measured instead
of assumed, and it is measured on one machine and one build; the lock costs an
uncontended critical section about five times per present, and it is what makes
the two hands of a draw - and later a separately drawn weapon - provably share
one controller pose.

## Dead ends (do not re-hunt)

- The camera-object matrix at `kCamHookAt` is not what the renderer draws with.
- Mouse-count head injection: swims, lags, no roll.
- "Any constant upload of 9+ registers is a bone palette" is false; writing into them corrupts
  world geometry.
- `HideBoneByName` on the arms does nothing (no visuals, no allocation); hiding is by
  render-size masks (`WeaponHideBones`, `ArmsHideTick`). **Narrower than it reads**: that
  measured the SCRIPT FUNCTION dispatching to nothing, which is a fact about
  `HideBoneByName`, not about any array. The array question is above, "The 30.13
  bone-visibility result was recorded all along".
- **`+0x288` is `SkelControlIndex`, MEASURED** (2026-09-06, reflection, run 1), not
  `BoneVisibilityStates`. Writing `0x02` there attaches the bone to SkelControl #2; 30.13
  did it and 30.14 recorded the arms freezing to the view and riding the head. Resolve an
  offset from UE3 reflection instead of pattern-matching a byte range.
- **This build declares no `BoneVisibilityStates` on `SkeletalMeshComponent`** (measured
  the same run; `RequiredBones` resolved at `+0x23c`, so the resolver was working). Per-bone
  hiding has no script-visible array to drive. That is consistent with 30.17's
  `HideBoneByName` allocating nothing and is the likeliest reading, but the native's own
  implementation was never inspected, so it is not established as the cause.
- Window-route resolution changes (work area, max tracking size, self-resize, fullscreen
  escape, client-rect spoof, mode list): six builds, the game still chose its own size; the
  engine's own setres is INERT on this build (measured 2026-09-03: it dispatches and does nothing); the command line is the way, and the fullscreen path takes a proxy-advertised mode ("The render size", session 7).
- The overlay-scene XR architecture (compositor overlay, reprojection-exempt): rejected
  at 38.0 (cannot be motion-smoothed).

## Build history (the original author's numbering)

30.x VR 2.0 (DXVK chain, hands, overlay, SkelControl, blink), 32.x resolution wars and
Blink detours, 33-36 graft and calibration, 37.x OpenXR bring-up (XR-1 bench, XR-2 sync,
XR-3 pace thread), 38.x the Quest convergence attempts, 38.92 shipped. The fork: M2 frame
map, M3 splice, M4 twins, M5 sequential + shafts, M6 wrist HUD + shadows, M7 pixel-shader
shear, M8 quarter-res light passes (M8.2 shipped).

## The arm mesh, measured (VR-31, 2026-09-06)

Everything here was read from the running game and is quoted from the log, not
derived. The full account of what the split does with it is
`docs/dishonored/ARM_HAND_SPLIT.md`.

**The mesh signature**, used to arm the lock by identity rather than by a key
press: **4448 primitives, 2771 vertices**. `[Hands] ArmMeshPrims` /
`ArmMeshVerts`.

**The vertex declaration**, stream 0, **32 bytes a vertex, 9 elements**:

```
POSITION      off=16  type=2  (D3DDECLTYPE_FLOAT3)
BLENDWEIGHT   off=12  type=8  (D3DDECLTYPE_UBYTE4N)
BLENDINDICES  off=8   type=5  (D3DDECLTYPE_UBYTE4)
TEXCOORD0             FLOAT16_2   - packed, see below
```

Stream 0 is the ONLY stream, which is what makes the clip possible at all: the
index list can be re-based onto a vertex buffer of ours without desynchronising
a second stream addressed by the same index.

**The buffers are readable.** `vb usage=0x0 pool=0 size=88672 stride=32` - not
`D3DUSAGE_WRITEONLY`, so the read lock returns real data on this asset and this
driver. That is a fact about this build, not a guarantee: the validation that
would catch an uninitialised page is still in place and still runs.

**TEXCOORD0 is FLOAT16_2, and that cost a silent fallback.** A check that
demanded a `FLOAT2` answered "NO TEXCOORD0", so the cap's colour mode never ran
and every cap took ring vertex 0 instead - which looked correct, because vertex
0 is still a vertex on the boundary ring. It would not have looked correct on a
ring that landed on a texture seam. **A refusal that produces a plausible
picture is the worst kind of refusal**, and this one is the example to cite:
the fix was to widen the decode, and to make the log line name the offset and
type it found and say which of the two paths actually ran.

**NORMAL is not a FLOAT3 here**, so cap vertices inherit the donor's normal
rather than having one forced flat to the cut plane. Re-encoding a packed normal
without knowing the asset's bias and scale would be a guessed constant shipped
as a measured one.

**The palette measures 48 bones**; the module's ceiling is 128.

### The measured cut position

`[Hands] WristCutA` / `WristCutB` = **-4.9**, mesh units from the hand bone
along the limb axis, positive toward the fingers. This is where the ring was
left after a headset walk on 2026-09-06 and it is read straight off the
`ms/wrist` line that the last press printed. It is asset-relative, so it
survives a level load and does not depend on the pawn's position. It is not
derived and it is not a guess; the derived seed it replaced was
`WristScale`=0.70.

## The player rig's skeleton, from the engine (VR-33, 2026-09-07)

Returned by `MatchRefBone` / `GetBoneName` / `GetParentBone` on the live
`DishonoredPlayerSkeletalComponent`, not derived from geometry. Reproduce with
`[Hands] BoneQuery=1`.

**Reference-skeleton indices:** `camera_jnt` 8, `hand_R_jnt` 25,
`handAttachment_R_jnt` 27, `hand_L_jnt` 54, `handAttachment_L_jnt` 56.

**The chains:**

```
hand_*_jnt           -> lower_arm_*_jnt -> upper_arm_*_jnt -> shoulder_*_jnt
                        -> Collarbone_*_Jnt -> Root_jnt -> root0_jnt -> ROOT
handAttachment_*_jnt -> hand_*_jnt -> (as above)
camera_jnt           -> head_jnt -> neck_jnt -> spine_3_jnt -> spine_2_jnt
                        -> spine_1_jnt -> spine_0_jnt -> Root_jnt -> root0_jnt -> ROOT
```

Three facts that matter and were previously assumed:

**The weapon attachment joints are CHILDREN of the hand joints.** A pose
applied at a hand carries its attachment beneath it. Necessary for the
engine-side route; not sufficient, since it says nothing about whether a later
writer or the attachment update honours the change.

**The camera is on the spine/head branch, not either arm.** The nearest common
ancestor of `camera_jnt` and either hand is `Root_jnt`, and the two arms also
only meet at `Root_jnt`. So a per-side edit at or below a hand joint cannot
disturb the view or the opposite hand.

**SKELETON INDICES ARE NOT PALETTE INDICES.** `hand_L_jnt` is bone 54 and
`handAttachment_L_jnt` is bone 56, while the arm draw's GPU palette is 48
entries (144 registers at c6). Both exceed it. The palette subsets or reorders
the skeleton, so the two orderings cannot be used interchangeably, and any
statement of the form "bone N of the palette" is about a render slot rather
than about anatomy.

### UStruct::SuperField is at +0x44

Derived rather than guessed: it is the offset at which
`DishonoredPlayerPawn` -> `Pawn` -> `Actor` -> `Object` all resolve by name.
Three correctly ordered named links is the evidence; a single plausible pointer
would not be.

### The pawn's Mesh is a DishonoredPlayerSkeletalComponent

Not a plain `SkeletalMeshComponent`. A receiver check by name equality would
refuse a valid receiver; ancestry through the Super chain is required. This was
load-bearing on the first run, not ceremony.

### The mod's outbound ProcessEvent path

`hands/mat_hide.cpp` has called `GetNumElements` and `GetMaterial` on live
components since the material route, through
`PFN_ProcessEventCall` (`__thiscall`, four arguments, parameter frame with a
NULL Result), and `console.cpp` uses the same path. `g_peReentry` is set around
those calls and **is read by `console.cpp:72` and `commands.cpp:319`** - so it
guards those two callers, and nothing in `PeHandler` tests it, so it does not
guard the hook. `PeHandler` runs
`PeLatch` and the scene-draw call-site patch before any event filtering, so a
mod-originated call needs a depth guard checked at the top of the handler, or
it fires real side effects and enters the census as a game event.

### The arm mesh's declaration uses streams 0 and 1, and a stale binding cost the caps

Measured 2026-09-07. The vertex declaration for the first-person arm mesh
references **stream mask 0x3** - streams 0 and 1 - and the draw binds only
stream 0.

The split needs to own stream 0 to re-base its index list onto its own vertex
buffer, which is what the plane clip and therefore the wrist caps depend on. It
used to veto that whenever ANY of streams 1-7 had a buffer bound, and a stream
left bound by an earlier draw satisfied it. The wrist caps disappeared between
runs with no configuration change, twice, and the log blamed "stream 2 is also
bound" - a stream the declaration never references.

**A bound stream is not a used stream.** The declaration decides what a draw
reads. Only a stream the declaration names may veto.

Note for later: the declaration DOES name stream 1, and no observed draw binds
it. If one ever does, the veto fires legitimately and the clip is genuinely
unavailable on that pass.

### MsDraw must check the draw contract, not the primitive count

The split's index list is re-based onto our own vertex buffer, so it is only
valid for the exact draw it was built from. `MsDraw` used to accept any draw
with a matching primitive type and count. A different vertex window, base
vertex, start index, stream-0 offset or declaration on the same buffer pair
would consume a split that does not describe it.

The build now records that contract and every replacement draw re-checks it.
A mismatch is passed through to the original draw - NOT dropped: once a split
is ready the caller's auto-arm fail-soft no longer applies, so declining used
to suppress the mesh entirely.

## The view-model's vertex path, read from the shader (VR-33 step 2, 2026-09-07)

Captured from a qualified hand draw and read out of the shader's own
disassembly and constant table. This replaces every inferred answer about the
palette's origin, including the "pawn root" conjecture, which was wrong.

### The chain

```
p_local = ( sum_i w_i * BoneMatrix[idx_i] ) * ( v0 * MeshExtension + MeshOrigin )
p_cam   = LocalToWorld * p_local
clip    = ViewProjectionMatrix * p_cam
```

`BoneMatrices` is declared `float4x3[75]` - three registers per bone, bone `b`
at `base + 3b`, `base + 3b + 1`, `base + 3b + 2`. The observed uploads carry 48
matrices (144 registers), well inside the declared 75.

**Weights are NOT normalised by the shader.** It sums `w_i * M_i` into one
blended matrix and applies it once. So if the effective weights do not sum to
one, a palette translation `T` moves the vertex by `wsum * T` and not by `T` -
the hazard the review raised is real, and the anchor's tolerance check is the
right guard.

**The blend index order is swizzled.** The shader computes `a0` from
`3 * blendindices` and then reads `.yxzw`, pairing weight `.y` with index `.y`
and `.x` with `.x`. The pairing is correct; only the evaluation order differs.

### Where the palette's output actually lives

`p_local` is in the COMPONENT'S LOCAL SPACE, and `LocalToWorld` maps it to a
**camera-relative** world frame - positions relative to the camera, world axes.
Checked numerically on a captured packet: the anchor at local
`(24.5, -142.5, 58.0)` maps to `(-46.2, 11.3, -24.6)`, about 53 uu from the
origin, which is where a hand sits relative to a head. The ~138 uu vertical
term in the old calibrated offset was simply the component's local origin.

**`LocalToWorld`'s rotation does not follow the head.** Across captures with
head yaw from -0.79 to +0.66 rad its rotation columns are identical to six
decimal places and only its translation moves. This CONTRADICTS the earlier
perceptual reading that the palette frame rotates with the head, and the
constants are the stronger evidence. Both readings are kept here because the
disagreement is the useful part.

### Register numbers are per shader and must never be hard-coded

Three shaders draw this mesh in one run. They agree on `ViewProjectionMatrix`
at c0, `BoneMatrices` at c6 and `LocalToWorld` at c231, and they DISAGREE
elsewhere: `MeshOrigin`/`MeshExtension` at c235/c236 in one and c238/c239 in
the others, with `WorldToLocal` at c235 present only in two.

Worse, one shader defines c4 as an immediate `(3, 1, 0, 0)` while the device
reports `(0, 0, 0, 1)` for that register. A shader immediate overrides what the
API supplied, so reading that register from the device would have been wrong by
construction.

The proxy therefore parses each shader's CTAB constant table at capture time
and uses the register indices it declares. Nothing about the layout is assumed.

## The skinning palette is a TRANSPOSED 4x3, and the two halves index differently (VR-33, 2026-09-07)

A D3D skinning palette holds each bone as three `float4`s that are the
**transpose of a 4x3**: row `r` is matrix **COLUMN** `r`, and the `w` of each
row holds that component of the translation.

So the translation reads and transforms like an ordinary vector - which is why
the probe's bone origins were sensible from the first run - and the basis does
not. Applying `M' = R.M` to the stored form multiplies the basis by R
**transposed** and along the row, while the translation column multiplies by R
the ordinary way down the rows. **The two halves genuinely use different index
patterns**, and the first build applied the translation's pattern to both.

The symptom names the bug if you know it: position lands correctly, there is
one controller angle where the hand sits exactly where it belongs, and the
whole hand turns on a lever several feet long. That is a correct translation
and a transposed basis together.

## The palette is a register INTERVAL with per-register validity, not a length (VR-33, 2026-09-07)

Caching the palette as a COUNT cannot express what the engine does. Four state
questions a length cannot answer, all of them observed:

* an update starting INSIDE the block fails an outer bounds test;
* a wide block covering `c6` can top up an existing cache but never bootstrap
  an empty one;
* a short `c6 x4` leaves the previous claimed length standing;
* an over-long upload is copied whole, after which every trailing triplet is
  treated as another bone.

Model it as a fixed interval `[6, 6 + 3*bones)` with a valid flag per register.
Every upload contributes its intersection with that interval whatever it starts
at or how far it runs, and the palette is usable only when the WHOLE interval is
valid - so a partially filled palette can never be drawn through.

## Sockets live on the ASSET, and `Mesh` is declared on `Pawn` (VR-33 step 1, 2026-09-06)

Two reflection "UNKNOWN"s that were this side looking in the wrong place rather
than the build differing from the corpus.

`SkeletalMeshComponent` declares socket **queries**, which are functions;
property reflection will never find them. The socket **list** is on the asset:

```
component -> SkeletalMesh (+0x1D4) -> Sockets (+0x160)
```

And `Mesh` is declared on `Engine.Pawn` (`Pawn.uc:187`). `FindPropOffset`
matches on the OUTER's name, so asking `DishonoredPawn` for it was always going
to miss.

## The bone palette's basis, measured (VR-33 rung 1, 2026-09-07)

The skinning matrices the game uploads to `c6` (48 bones, `c6 x144`, 3 float4
rows each, row-major 3x4 with the translation in `.w`) are expressed in a frame
whose axes are:

| axis | direction |
|---|---|
| 0 | LEFT |
| 1 | DOWN |
| 2 | FORWARD |

`left x down = -forward`, so the frame is LEFT-handed, which is what UE3 should
give and is the main reason to believe the reading rather than an artifact.

**The frame rotates with the GAME CAMERA.** A stick turn carried the three
directions round with it while the head yaw stayed at about -15 deg, which is
what separates camera-relative from world-aligned: a world-aligned frame would
have left the directions where they were. This is also the coupling behind
hands that drift with head movement - a world-space offset pushed into this
frame without composing the camera's yaw counter-rotates exactly that way.

Against OpenXR (x right, y up, z BACKWARD, so forward = -z) the map is a plain
componentwise negation: `left = -x`, `down = -y`, `forward = -z`, i.e.
`T_palette = -k * v_xr`.

### How it was measured, and the two readings that were not evidence

A delta of 15 uu on one hand class with the other class untouched as the
reference. The first two runs used a TIMER, and returned `left/down/forward`
and `down/forward/left` - the same cycle entered one step in, but nothing in
either run could prove that rather than a changed basis, and the tester said so
before it was acted on. The third run was STEPPED BY THE TESTER (F6: rest ->
axis 0 -> axis 1 -> axis 2 -> rest), which has no phase to infer: the log shows
`REST, AXIS 0, AXIS 1, AXIS 2` at a steady `hmdYaw` of about -35 deg, and the
answer was `left, down, forward`, agreeing with the first reading.

Class A is the LEFT hand and class B the RIGHT: with the delta on class A the
left hand left the crossbow while the right stayed on the sword hilt.

### Traps this cost

* `MsTick` was called from inside `DcTick`, BELOW its `if (!g_dcOn) return;`.
  Switching the draw census off therefore killed the mesh split's whole tick -
  mode cycling, the wrist knob, the palette step - and the split stopped being
  maintained, putting the arms back on screen with nothing in the log naming
  the cause. The split does not belong to the census and now ticks either way.
* The bare-numpad hotkey blocks did not check their modifier, so a
  CTRL+Numpad5 press meant for the palette probe ALSO cycled the draw census.
  One press did two things and read as "the probe did nothing".
* CTRL is the game's block. A diagnostic on a CTRL chord makes the character
  act while it is being pressed. The probe is on F6, unmodified.

## SkelControls are NOT evaluated on this build (VR-33 phase 1, 2026-09-07)

This heading was briefly withdrawn on 2026-09-07 when the scan behind it turned
out not to have swept GObjects, and restored the same day once the fixed sweep
measured the whole population and agreed. The retraction is kept below, because
the reasoning error is the reusable part.

**The three named controls are inert.** `m_pLookAtControl_LeftHand`,
`_RightHand` and `_Camera` are distinct live `SkelControlSingleBone` objects
with `ControlStrength` 1.000 - and `ControlTickTag` frozen at 10 for an entire
session while the mod wrote to one of them about 8,500 times a second. Its own
apply flags were clear (`bools 0xA`, apply=0 add=0) and its saved translation
was zero. UE3 stamps that tag when a control is EVALUATED.

**RETRACTED 2026-09-07: the "every SkelControl in GObjects - 64 of them" scan
did not walk GObjects.** Its comparison table held 64 entries and its loop
condition was `curN < 64`, so the sweep STOPPED the moment the table filled.
The "64" that was read as a population is the array's capacity, and every
object after the 64th SkelControl in the array was never visited. The scan
therefore says "none of the first 64 advanced", not "none advanced" - and the
first N entries of GObjects are the earliest-constructed objects, which is the
least representative slice available for a question about the player's live
view model. This is the project's own rule biting again: a counter is not
evidence until you know its population.

What survives the retraction is the FIRST measurement, which did not depend on
the scan: the three named `m_pLookAtControl_*` controls held `ControlTickTag`
at 10 for an entire session under ~8,500 writes a second, with their own apply
flags clear. Those three specific controls are inert. Whether ANY SkelControl
on this build is evaluated is once again open.

The scan now sweeps the whole array, tracks up to `HM_SCAN_CAP` (1024) objects
for the second-apart comparison, and prints the population it ran over -
objects walked, objects tracked, objects past the table - on the same line as
the live count, so a zero cannot be read as a census again.

### CONFIRMED with the fixed sweep (2026-09-07, same day)

The run was made in the state the retracted one lacked: `[Hands] Enabled=1`,
`[Mode] GamepadOnly=0`, so the mod's own hand subsystem WAS driving. Every one
of 34 consecutive one-second samples read identically:

```
66 SkelControl object(s) out of 103117 GObjects entries walked (FULL sweep),
66 tracked for comparison, 0 NOT tracked, 0 advanced their tick tag in the
last second. hands=1 gamepadOnly=0
```

Full sweep, whole population tracked, nothing past the table, zero advancing,
with the hands subsystem live. **No SkelControl is evaluated on this build.**
A write to one cannot move anything, at any cadence, with any flags, in any
space - and this time the population is known.

The old 64-entry table had missed exactly two objects out of 66, so the
retracted reading happened to be right. It was still not evidence; it is now.

**This does retire the 38.x "9,000 writes a second outrun the recompute"
reading.** No race was being lost - there was nothing to race.

### Cost, measured

The sweep stalls the game thread for **505-520 ms once per second** - the perf
line attributes it to `out/idle (waiting for the game thread)`, 30x the mean
present interval, ~46 display slots at 90 Hz. It is unplayable while armed and
must be switched off after any run.

### Cost note

The GObjects-wide tick scan runs on the script lane and walks every object once
a second, calling `ObjClassName` on each. Measured cost: a 505-520 ms stall of
the game thread every second (see above). It is a diagnostic, ships OFF, and
must not be left enabled.

## 2026-09-08: VR-33 weapon attachment direct repair

Source inspection of 768fbf71 found that uncached weapon matching selected the
first available hand and then excluded all components assigned to the other
hand. The preserved failed run performed 302,039 comparisons with no patch
attempts; this cannot establish whether the GPU correction itself was valid.

The matcher now uses the complete affine reference bridge, preserves native
scale, compares both hands, and revalidates each draw. The native transform
fields formerly embedded in WaReadCompXform are centralized in patterns.h as
kWaComponentLocalToWorld (+0x60) and kWaComponentTranslation (+0x90). These are
existing FpComputePivots reads, not newly discovered offsets. Row extraction
and a shared reference bridge still require independent live member matches.

The old palette extent was 256 minus its start register. It would cross the
BoneMatrices declaration into LocalToWorld and other constants. The repair
carries the CTAB register count and rejects ranges overlapping VP/LocalToWorld.
The previous zero-attempt run never exercised this destructive extent.

19 weapon host tests and 28 existing hand tests pass. A simulator launch failed
before gameplay, with an access violation instruction in Dishonored.exe at RVA
0x60907e; root cause is undetermined. No attachment result is claimed from it.
User headset testing is pending. See VR-33-HANDS-AND-WEAPONS.md for code
changes, exact test steps, remaining assumptions and rollback location.

The first user headset run of the direct repair subsequently produced 196,422
successful patched draws with zero restore failures and near-zero transform
residuals for all three named members. Weapons moved but used the opposite
hand; there was also a large apparent offset and flickering dark silhouettes
at the former positions. That run preceded the installed side-setting fix.
The next revision swaps the settings and removes camera-VP requirements from
weapon placement, allowing independently matched world-space passes with the
correction conjugated through the reference bridge. These address concrete
routing/math restrictions; visual ghost removal is still unverified. The host
suite now contains 21 weapon cases plus the existing 28 hand cases.

## THE LOADED BOLT AND A FIRED BOLT ARE DIFFERENT CLASSES (VR-59, 2026-09-08)

Source: the decompiled scripts (`DisWepCrossbow`, `DisTweaks_WepCrossbow`,
`DisProjectile_Arrow`, `DisProjectile`). Read, not guessed, and nothing from them
is reproduced here beyond the class and member names needed to name the objects.

| | Loaded bolt | Fired bolt |
|---|---|---|
| Owner | `DisWepCrossbow` (an Inventory item on the pawn) | `DisProjectile_Arrow extends DisProjectile extends Actor` |
| Member | `m_pArrowMesh_HighRes` | `m_pMesh` |
| Component name | `pArrowMesh_HighRes` | `pSkeletalComp` |
| Component class | `DishonoredItemSkeletalComponent` | `Engine.SkeletalMeshComponent` |
| Mesh asset | `bolt_01` | `bolt_01` |

`DisTweaks_WepCrossbow.m_ArrowSocketName` is the socket the loaded one hangs on.

**The mesh asset is the ONE field they share**, which is exactly why an
asset-name test cannot separate them and why buffer identity cannot either. The
component name, the component class and the owning actor all differ, and the
fired bolt is a separate ACTOR - so it is not reachable from the pawn and never
appears in the component snapshot at all.

### Two corrections to what the snapshot was believed to mean

**`FpCollect` reports the pawn's INVENTORY, not what is equipped.** Measured over
a full run: all 19 published snapshots were identical - six components, the same
six, including `bolt_01 (pArrowMesh_HighRes)` - across crossbow, sword and pistol
being held in turn. The scripts say why: the loaded bolt is a component of the
WEAPON, and the weapon stays in inventory when stowed. **Presence in the snapshot
is not evidence that a thing is in the player's hand**, and a gate built on that
reading cannot fire (VR-59's `AttachRequireLiveMember`, now default OFF).

**The pistol is not in the snapshot at all.** Only `Skm_Player`,
`Wpn_PlySword01`, `crossbow_01` and `bolt_01` ever appear. So the pistol has no
member candidate of its own and can only reach a contract through the
vertex-OR-index-buffer match in the identity route - which is why its visibility
depended on the angle between where it pointed and another asset's recorded
position.

### `lastL2W` is not a record of "drew on the view model this frame"

It is written only where a contract is adopted through the transform matcher. The
buffer-identity route, which is what actually corrects the auxiliary passes,
never refreshes it. So once the matcher misses for a mesh, that reference goes
stale permanently. Measured: 53,238 refusals in one run with the present gap
growing monotonically to 21,367, all on HELD `crossbow_01` and `bolt_01`.

Any freshness test built on it therefore starves the held weapons rather than
catching world instances. **A reference has to be maintained on the path that
uses it.** The engine-read component translation in the snapshot is the sound
alternative: it is refreshed every 4 ms whether or not the matcher succeeded.

## EQUIPMENT LIVES IN A TArray, WHICH THE COMPONENT WALK CANNOT TRAVERSE (VR-60, 2026-09-08)

Source: the decompiled scripts. Class and member names only; no offsets, because
offsets are derived against the running build.

```
DishonoredPlayerPawn.m_pInventory : DishonoredInventory
DishonoredInventory extends Object
    m_Slots : array<PawnInventorySlot>
struct native PawnInventorySlot
    m_pItem         : DishonoredInventoryItem
    m_pRequiredType : Class<DishonoredInventoryItem>
    m_RequiredUsage : EDisEquipUsage        // None | Primary | Secondary
```

**`FpCollect` cannot reach any of it.** The walk reads every 4-byte field as a
possible `UObject*`; a `TArray` field's first four bytes are a pointer to a heap
buffer of elements, so `LooksLikeObj` rejects it and the walk stops. Every
inventory item is in `m_Slots`, which is exactly such an array.

Measured consequence over a full run: all 19 published component snapshots were
identical (six components) and **the pistol never appeared at all, including
while it was the weapon in hand.** What the snapshot contains is whatever happens
to be reachable by direct pointer from the pawn, and it cannot report what it
missed because it never knew it was there.

`m_RequiredUsage` on the slot is the per-hand channel, so **walking that one array
answers "which item is in which hand" directly** - the question `WaHandFor`
currently answers with an assumption baked into asset-name substrings.

### The enums are the flags, and they already exist

```
EDisEquipUsage         None | Primary | Secondary            (DisGlobalEnums)
EItemSocket            None | Equipped | Holstered | ...     (DisGlobalEnums)
eDisPlayerStance       NotSet | NotReady | Ready | Blocking  (DishonoredPlayerPawn)
eDisPlayerActionUsage  Fullbody | Upperbody | LeftHand       (DishonoredPlayerPawn)
```

`EItemSocket` is the equipped-versus-holstered distinction that VR-59 attempt 1
needed and could not obtain from component presence. `m_PlayerStance` is indexed
BY `EDisEquipUsage`, so stance is already per-hand.

**Correction (VR-88): ActionUsage is a per-machine channel setting, not a live
takedown flag.** The pawn defaults set it to Upperbody/LeftHand. VR-88 reads
`m_pCurrentState` plus `m_pCurrentStateID` on the master/upper FSMs; the
action mappings await a headset run. See `ANIM-HANDOFF-PLAN.md`.

The route to reading all of it reliably is a UE3 property resolver keyed on FName;
`docs/dishonored/GAMEPLAY_STATE.md` is the plan and the rules for it.


## THE PROPERTY RESOLVER ALREADY EXISTED (VR-61, 2026-09-08)

Recorded as a research failure, because the mistake is more useful than the fix.
A session went to the BioShock trilogy mod for a technique to resolve properties
by name **before grepping this repo for prior art**. This repo has had one since
38.x and it is load-bearing in four modules.

`FindPropOffset(className, propName)` and `FindBoolProp` in `ue3/uobject.cpp`.
`arm_follow.cpp` resolves twelve properties through them; `crouch.cpp`,
`block_state.cpp` and `skelcontrol.cpp` also use them.

**They need no chain offsets.** Every `UProperty` is itself a `UObject` whose
`Outer` is the class that declares it, so a `GObjects` scan for (name, outer
name, class name containing `Property`) finds the property object and its own
recorded offset is the answer. Nothing to derive, no candidate layout to get
wrong. That is more robust than walking `Children` / `Next` / `SuperStruct`,
which is what the trilogy mod had to do on its own UE3 build.

| Offset | Slot | Derivation |
|---|---|---|
| `kUPropOffset` 0x5c | `UProperty::Offset` | the 38.x skelcontrol property dump: the Offset column identifies itself as small, distinct, ascending in declaration order and below the class instance size |
| `kUBoolBitMask` 0x6c | `UBoolProperty::BitMask` | same dump |

Both were inline literals in `uobject.cpp` and are now in `patterns.h`, where
this repo requires engine offsets to live, so one place owns them.

### Two traps that are properties of the lookup, not of a build

**`FindPropOffset` matches on the OUTER's name, so it needs the DECLARING class.**
A property inherited from a base class does not resolve under a subclass's name,
and for a struct member the outer is the ScriptStruct rather than the class
holding it (`arm_follow.cpp` documents this for `DishonoredVTSettings`). Offer
the candidate declaring classes and log which one answered.

**Nothing may resolve at init.** Our DLL loads from `DllMain` during the exe's
import resolution, before the exe's CRT static initializers, so `GNames` is empty
then. The gate every caller uses is `NameFromIndex(0)` reading `None`.

### The cost, which is why a cache is not optional

Each lookup is a full `GObjects` scan. The trilogy mod measured a name scan on a
poll cadence stuttering that entire game at 2-3 Hz. Resolve once, cache for the
process lifetime (offsets are stable per boot), and cache the MISSES too, since
a miss costs the same full scan.


## THE PISTOL DETACH ANGLE IS A RADIUS, CONVERTED (VR-60, 2026-09-08)

The pistol stays attached within about 45 degrees either way of the direction it
was equipped facing, and re-equipping resets that reference to wherever the
player is pointing. That reads as a rotation gate and there is no rotation gate
in the code. It is `AttachPassRadius` (60 uu), converted into an angle by
geometry.

A held weapon orbits the head at a small radius, so turning the view moves it
along a chord of `2 r sin(theta/2)`. Setting that equal to 60 uu at 45 degrees
gives **r of about 78 uu** - squarely inside the view-model band the census
measured (view-model draws sit within about 170 uu of the camera). Re-equipping
resets it because that re-captures the reference position.

Confirmed by the same run:

```
wa: instance verify - held 146073, elsewhere 35029, unverifiable 0
  reference: component 179401, recent 1701
  worst offset accepted 60.0 uu, farthest refused 3014.2 uu
```

The worst accepted offset sits exactly ON the radius, which is what a threshold
being reached by drift rather than by a genuine instance difference looks like.
The farthest refused, 3014 uu, is a real world instance - three orders of
magnitude away, which is the separation the gate was designed for.

**The verification is correct and the identity it is given is wrong.** The pistol
has no member candidate of its own, so it reaches a contract only through the
buffer lookup and is then checked against ANOTHER asset's component - one fixed
in the world while the pistol orbits the head. Any threshold would produce some
angle; the defect is the identity, which is what VR-60 is for.

This is also a worked example of a rule this project keeps re-learning: **log the
derived number, not just the inputs.** The angle was a mystery as a perception and
arithmetic as soon as the offsets either side of the radius were on the line.


## EQUIPMENT: THE ITEM ANSWERS FOR ITSELF (VR-61, 2026-09-08)

`DishonoredInventory.m_Slots` at `+0x0038` (resolved by name) is a
`TArray<PawnInventorySlot>`, stride 12 validated against the data. Read live on
a save with all four weapons carried:

```
slot[0] usage 1  DishonoredItemEmpty
slot[1] usage 2  DishonoredItemEmpty
slot[2] usage 0  DishonoredWepSword
slot[3] usage 0  DisItemPowers
slot[4] usage 0  DisWepCrossbow
slot[5] usage 0  DishonoredWepPistol
slot[6..8] usage 0  empty
```

**The pistol IS here**, along with every other weapon. It is absent from the
component snapshot only because that walk cannot traverse a TArray. So the roster
is fully readable and VR-60 has its data.

### The slot does NOT say what is equipped

`PawnInventorySlot.m_RequiredUsage` is a CONSTRAINT on what may occupy the slot,
not what is in the hand. The dump says so plainly: slot 0 requires Primary and
slot 1 requires Secondary, both holding a `DishonoredItemEmpty` placeholder, while
the real weapons sit in unconstrained slots (usage 0). Reading the slot for equip
state reports `DishonoredItemEmpty` in both hands while the player is visibly
holding a sword.

The engine keeps a slot per usage whether or not anything occupies it, so an
Empty row is legitimate data and simply not the answer.

### `DishonoredInventoryItem` carries both flags

```
m_EquipUsage : EDisEquipUsage   None | Primary | Secondary
m_CurSocket  : EItemSocket      None | Equipped | Holstered | Give
```

The item says which hand it belongs to and whether it is IN that hand or on the
body. **That is the equipped-versus-holstered distinction VR-59 attempt 1 needed
and could not obtain from component presence**, and it is a per-object fact
rather than an inference from an index or a position.

`EItemSocket`: `ItemSocket_None` 0, `ItemSocket_Equipped` 1,
`ItemSocket_Holstered` 2, `ItemSocket_Give` 3 (`DisGlobalEnums`).

### Why this matters beyond the pistol

The equipped item is the OWNER of the component the weapon attachment should be
verifying a draw against. Today a draw is checked against whatever component its
contract happens to carry, which for the pistol is another weapon entirely - the
cause of its 45-degree detach cone. Reading the equipped item per hand replaces
that with the right component, and it generalises to anything held.


### The equip flags MOVE, measured across sheathe and swap (VR-61, 2026-09-08)

A flag that reads the same in every state is not evidence it is the right flag,
so it was identified by making it move. One run, swapping ranged weapons and
sheathing repeatedly:

```
Primary   DishonoredWepSword   EQUIPPED   (every time weapons are out)
Secondary DisWepCrossbow  <->  DishonoredWepPistol
sheathe:   Secondary -> none, then Primary -> none
unsheathe: both return together
```

This matches the game exactly: the sword is a constant while weapons are out and
the Secondary hand carries the ranged weapon, so unsheathing brings out both and
sheathing puts both away. The ranged item leaves the hand slightly BEFORE the
sword on a sheathe, consistently.

**Sheathing reads socket `none` (0), never `holstered` (2).** So
`ItemSocket_Holstered` means something this run never produced. Recorded as an
observation, not a conclusion - do not assume a sheathed weapon is Holstered.

The slot dump with the item fields, on a save carrying everything:

```
slot[0] requires 1 | DishonoredItemEmpty  -> own usage 0, socket 0
slot[1] requires 2 | DishonoredItemEmpty  -> own usage 0, socket 0
slot[2] requires 0 | DishonoredWepSword   -> own usage 1, socket 1 EQUIPPED
slot[3] requires 0 | DisItemPowers        -> own usage 0, socket 0
slot[4] requires 0 | DisWepCrossbow       -> own usage 2, socket 1 EQUIPPED
slot[5] requires 0 | DishonoredWepPistol  -> own usage 0, socket 0
```

The slot constraint and the item state are plainly different things, and only the
item is the answer.


## THE MONO WINDOW AT A LOAD IS A GHOST MENU FLAG (VR-62, 2026-09-08)

Measured by the startup scoreboard on its first run, and it falsified the
prediction that `CylTruthLive` was the laggard:

```
startup: load #1 scored - the mono window was 24.30 s.
  pawn-ptr      +0.00 s      cyl           +0.00 s
  gnames        +0.00 s      view          +0.00 s
  inventory     +0.00 s      no-cine       +0.00 s
  no-menu      +24.30 s   <- the laggard
  verdict      +24.30 s      stereo       +24.30 s
```

Every term of the gameplay verdict was ready immediately except the menu flag,
and the verdict and the first stereo tag both followed it within the same
millisecond. **The whole mono window is one stuck flag.**

### The flag is a ghost, and the CLEARER was the slow part

`Dis_OpenPauseMenu` dispatches during a load and sets `g_menuOpen` when no menu
is open. A ghost-clearer already existed for exactly this, and it is what took
the time:

```
menu: stale flag cleared after  1505 ms - ... 118 gameplay dispatches still flowing
menu: stale flag cleared after 24289 ms - ...  21 gameplay dispatches still flowing
```

It required **20 cumulative view-rotation dispatches**. That is a fine proxy for
"the pipeline is still flowing" at gameplay rates and a terrible one during a
load, which is the only time it matters: **the dispatch rate falls to about 1/s
while a level settles, against roughly 78/s once it is up.** So the same ghost
cleared in 1.5 s one time and 24.3 s the next, and the second number is the
reported mono window start to finish.

**A count of arrivals cannot tell "flowing slowly" from "stopped"; recency can.**
A real menu shows NO dispatches at all, so one arriving within the last few
hundred milliseconds is the discriminator, and it is rate-independent. The other
three guards are unchanged and are what keep it safe: a live pawn (which excludes
the main menu and its dispatching 3D background), no cursor, and the flag
standing for 1500 ms.

This is a worked example of a rule this project keeps paying for: **a counter is
not evidence until you know its population.** The population here was "dispatches
per second", and it changes by a factor of eighty between the two states the test
has to tell apart.

### A contract must not outlive its level

Found in the same run, as a regression. A weapon contract holds the component it
was matched to; a load destroys that component; the contract table is evicted
only when it FILLS. So after a reload every contract points at a dead object, no
reference can be found, and every draw is refused as unverifiable.

**A refusal returns before the transform matcher, so it is a LOCKOUT rather than
a refusal**: the contract can never be re-adopted and the weapons never attach
again for the rest of the session. Measured: unverifiable past 45,000 while
corrected sat frozen and only 3 contracts had ever matched.

Contracts are now dropped when the game leaves gameplay, and any contract whose
component has been missing from a fresh snapshot for about a second is retired so
the matcher can re-adopt it.


## THE STARTUP MONO WINDOW: THREE FALSIFIED ATTEMPTS (VR-62, 2026-09-08)

Recorded because all three failed in ways that narrow the problem, and one of
them re-broke something this file had already documented.

### What is established

* **Nothing after the gameplay verdict holds the picture.** The verdict, the
  state transition and the first DOUBLE draw land in the same millisecond. The
  wait is entirely in deciding the game is in gameplay.
* **Two of the verdict's five terms are slow by construction.** `menuOpen` is set
  by a `Dis_OpenPauseMenu` dispatch during a load when no menu is open, and
  `viewLive` deliberately requires a full second of continuous dispatches to
  leave LOADING (measured at +1.52 s and +1.72 s).
* **The view-dispatch RATE is not constant.** About 1/s while a level settles
  against roughly 78/s once it is up - a factor of eighty between the two states
  any dispatch-based test has to separate.

### Attempt 1: clear the ghost menu flag on dispatch RECENCY - FALSIFIED

It works: the ghost cleared in about 1.5 s instead of 24 s. It also clears at the
MAIN MENU, which then goes stereo after a few seconds.

**38.17 in this file already recorded that exact failure** - the main menu's 3D
background keeps dispatching view rotations, so "dispatches still flowing" does
not separate it from gameplay, and the clearer firing there swept the menu UI
onto the wrist panel. The cumulative count of 20 was slow ENOUGH to hide that;
recency is not. **Part of the 24 s clear that looked like a bug was the clearer
being correctly slow at a main menu.**

`CylTruthLive` was supposed to be the discriminator that made this safe. The run
shows it is not sufficient, which is a new fact and the thing to attack next.

### Attempt 2: double on SCENE LIVENESS instead of the verdict - FALSIFIED

The hands and weapons FLASHED in the background of the pause menu. The camera
upload serial keeps moving while a menu is up - the world is still rendered
behind it - so "the scene is drawing" cannot tell a pause menu from a load, and
doubling during a menu is exactly the hazard the verdict guards: **a menu's draws
outnumber its presents, so the pair schedule breaks.**

The idea is not dead, the signal was wrong. A pause menu silences the view
dispatches while a load keeps them at ~1/s, so recency would separate them - but
the window has to be wider than 1 s, which leaves a pause doubled for that long.
That trades one artifact for another and needs measuring before it ships.

### Attempt 3: force a candidate re-collect on a load - FALSIFIED AS WRITTEN

Collecting during a load catches the rig half-built and returns one or two
components with **no body mesh**. Because the rebuild trigger is
`if (!g_fpCandN) FpCollect()`, a non-empty partial list is never refreshed and
sticks for the session: `2 component(s) resolved, 0 usable as the bridge anchor`,
repeating while the weapons sat in their default positions.

**A stale list is bad; a partial one is worse**, because the stale list at least
contained the anchor by name. Fixed by discarding a list with no anchor so the
next tick retries, bounded at 120 attempts so a genuine absence reports itself
instead of looping.

### The rule this cost

Every one of these replaced a slow, conservative test with a fast one, and every
one of them was correct about the slowness and wrong about the replacement. **The
conservative tests were slow because the fast signals do not separate the states
they need to separate** - c5 movement cannot tell a pause from a load, dispatch
flow cannot tell a main menu from gameplay, and a collect cannot tell a
half-built rig from a finished one. Any future attempt needs a signal that
distinguishes those pairs directly, not a faster version of one that does not.


## WEAPON DRAWS RUN ON THE RENDER THREAD; THE STEREO PASSES RUN ON THE GAME THREAD (2026-09-09)

Measured while trying to hand the re-entry method's eye decision to the weapon
correction. The attempt executed **zero times in 83,400 corrected draws**
(`agree 0, DISAGREE 0, no doubled pass to compare 83400`).

The re-entry method re-draws the world twice on the GAME thread (`drawTid=76156`
in the measured run). The engine queues those commands and replays them - and
every palette-fed draw with them - on the RENDER thread (`presentTid=79476`).
There is no stack for a draw hook to look up, and a per-thread marker set around
the passes can never be seen by the correction.

Consequences for anything built here later:

* A game-thread decision reaches a draw hook only if it is attached to the queued
  render work itself. A global read across the boundary is not a measurement.
* `g_sdDoublingNow` (mono versus stereo) is a *latest-state* flag and is coherent
  as a value, but it does not identify which queued commands are executing. It is
  adequate for the mono guard, where the state holds for hundreds of consecutive
  presents, and it is NOT adequate for anything per-frame.

### A number to disregard

An earlier version of that audit reported the eye inference disagreeing with the
drawing pass 39% of the time. It was read from a bare global across those two
threads and measures nothing. **Retracted.** It should not be cited.

## THE EYE BEHIND THE WEAPON CORRECTION IS INFERRED, AND "TOO SMALL TO READ" IS NOT "THE SAME EYE" (2026-09-09)

`MpEyeForPresent` in `hands/mesh_split.cpp` decides the eye once per present from
a sideways step of about one IPD in the draw's own `LocalToWorld`. When the step
fell inside the dead band it KEPT the previous present's answer. The method
presents the eyes alternately - 148 tags against 147 presents measured - so
holding is the one choice guaranteed wrong. Population: 991 of 8,341 presents.

Alternating instead removed the reported weapon flicker in stereo. The instrument
that counts unreadable presents fell from 31 windows including bursts of 565,
396, 350 and 293 consecutive presents, to **one window of one present**.

### And the state it was not reasoned about

MONO. One camera means the step is zero on every present, which the band test
correctly calls unreadable - so alternating flipped the hands by a full IPD every
single frame, and the four largest windows above were all mono. A mono frame has
no eye: it now takes no offset and the previous sample is dropped so the next
pair starts from a clean compare.

### What this does NOT explain

A residual occasional flicker is still reported with the windows empty. The
instrument records *unreadable* steps, not independently verified wrong-eye
decisions, so a step the heuristic reads confidently and gets wrong is invisible
to it. The eye inference is narrowed, not cleared.

## NOBODY OWNS THE CANDIDATE LIST - TWICE (2026-09-09)

The view-model candidate list has two ways to be wrong and had an owner for
neither.

**EMPTY, after a load.** Dropped when the game leaves gameplay (correct - a load
destroys the components). Every rebuild trigger in the tree is
`if (!g_fpCandN) FpCollect()` at a one-shot call site that has already fired: the
material census (`g_matCensusDone`), the bone-vis scan (`g_bvScanned`), the
numpad cycler, and skelcontrol's per-frame refresh, which is behind
`if (!g_handMesh || g_armsHidden) return` and never runs in the shipped
GamepadOnly configuration. Measured: one collect for a whole session, then
nothing for the 55 seconds after the second load.

Everything downstream followed and none of it was a separate bug - 0 refs,
contracts stuck at 0/64, and `2 weapon member(s) and NO bridge anchor`, those
members coming from the VR-61 equipped-item path which needs no candidate list.
The recovery could not work either: the no-anchor branch called
`FpInvalidateCandidates`, whose first line is `if (!g_fpCandN) return`, so it was
a no-op that logged as though it had acted, once a second, forever.

**STALE, after a weapon swap.** A swap does not empty the list. With the crossbow
out the list held `[3] 'pArrowMesh_HighRes' asset=bolt_01`; the pistol was
equipped, a rebuild ran while it was out and returned the pistol's mesh in that
slot; swapping back to the crossbow triggered nothing, because the list was not
empty. The bolt's draws kept arriving and kept failing -
`NEAREST MISS ... 169.241 deg / 11.00 uu | prim 310 verts 257 stride 32`, which
is the bolt's own geometry signature. The crossbow still attached because the
equipped-item path supplies the item's mesh and none of its CHILDREN.

### Three things the fix needed, and only the first is obvious

1. A trigger on equipment change. Keyed on the item OBJECT and its socket, not on
   the class name: two instances of one weapon share a name, and `g_rflState.gen`
   increments on every successful read rather than on a change.
2. A SETTLE WINDOW. The equipment event and the creation of the new weapon's
   child components need not land in the same tick, so a single collect can win
   the race, return a list with no loaded bolt, and declare success.
3. The equipped items as COLLECTION ROOTS. The pawn walk reaches an item only
   when a pointer chain happens to lead there, which is why the pistol never
   appeared in a snapshot and why the bolt came and went. The bolt is a child of
   the crossbow, not of the pawn.

### A retirement that could not fire

The stale-contract retirement lives inside `WaVerifyDraw`, so it is only reached
by a contract whose buffers are still being DRAWN. A weapon that has been put
away stops drawing, so its contract is never visited: measured as
`contract 'Wpn_PlyGunElite' hand 0 ... age 12822 ms`, twelve seconds after the
pistol went away. Retirement by ownership (the component is no longer among the
candidates and is not an equipped item's own mesh) covers that case. It only ever
retires - it never widens a radius or relaxes an angle, because an obsolete
contract does not merely go idle, it BLOCKS adoption of the real one: a refused
draw returns before the matcher.


## THE POSE TRACE, AND TWO NUMBERS THAT MUST NOT BE CITED (VR-65, 2026-09-09)

Recorded because both numbers were produced by an instrument of this project's
own making, and both were quoted before they were checked.

### RETRACTED: "the eye inference disagreed with the drawing pass 39% of the time"

Read from a bare global across two threads. The stereo passes run on the GAME
thread and the palette draws on the RENDER thread, so the value being read was
never the one being claimed. It measured nothing.

### RETRACTED: "the submitted pose is 51 degrees away from the rendered one"

Two faults at once. It differenced a UE world camera yaw against an XR
tracking-space yaw - different spaces, not comparable, and the game camera's yaw
carries body and thumbstick rotation the tracking pose never sees. On top of
that the mod publishes head yaw negated at the pose-lane seam, which doubled the
answer. `2 x yaw` is exactly what the log showed.

### AND THE SIGN-CORRECTED NEAR-ZERO IS NOT EVIDENCE EITHER

After correcting for sign the two agreed to within 0.08 degrees, which looks like
a clean bill of health and is not one. The record read the LIVE head globals at
draw time - whatever the Present thread last wrote - and compared them against a
pose derived from that same input. **Two values derived from one input agree by
construction.** The repo already had the coherent alternative and it was not
used: `g_injHmdYawSnap` / `g_injHmdPitchSnap` / `g_injHmdGen` / `g_injSnapOk`,
"the HMD orientation AS OF the last camera write", added for blink for exactly
this reason.

### What DID hold

The self-arming negative control worked: injecting +4.0 deg moved the recorded
sample by +4.16 and the reported error by -4.32. That demonstrates the comparison
responds to an angle. **It demonstrates nothing about whether the sample is the
one rendering used** - sensitivity to a perturbation is not correctness of the
input.

Record transport also held: 112 join lines with `expired 0 missing 0`, so the id
survives the pipeline with a 64-entry ring. A successful lookup proves the record
was AVAILABLE. It does not prove the FIFO associated it with the right image.

### The design that replaced it

Three records, never mixed: what the camera was TOLD (published as one unit at
the camera write, sample and resulting camera together), what rendering ACTUALLY
consumed (the c0..c3 view-projection, read on the render thread), and what OpenXR
was TOLD (per eye). Only camera-versus-render can clear the hypothesis.

Two things the first version got wrong structurally and that are worth not
repeating:

* **A compiler barrier and an id check are not cross-thread synchronisation.**
  `_ReadWriteBarrier()` orders nothing between threads, and an id check cannot
  stop a slot being overwritten while a reader copies out of it. Records are
  behind a lock now, copied out, never handed back as a pointer.
* **The audit compared whichever record arrived last against the LEFT submitted
  view**, regardless of which eye the record described. Each eye is compared
  against its own view now, and an untagged record reports unknown.

### The render observation does not assume a matrix layout

c0..c3 is rebuilt from however the engine batches it - the c5 handling already
learned that lesson when a device reset made the engine batch c5 into a wider
block and a seam matching only `startReg==5` went blind for a whole run. The
multiplication convention is then MEASURED from the game's own numbers: probe
directions around the observed camera position are projected under both
conventions, and the one that yields usable w values for a plausible fraction of
a ring is the layout. If neither validates, the render side reports nothing
rather than falling back to a guess, because a guessed layout produces a
confident number about the wrong matrix.

The yaw itself is found by search rather than decomposition: the world direction
that projects to the screen centre is where the camera looks.


## AN INI KEY THAT EXISTS BEATS EVERY COMPILED DEFAULT (VR-65, 2026-09-09)

Recorded because it invalidated two headset tests and produced a conclusion that
was not merely wrong but backwards.

`[Pace] Lag` was changed in the source from 1 to 2, shipped, tested, and reported
as juddering. Then to 0, shipped, tested, reported as juddering. On that basis
pose-history selection was declared eliminated - all three arms tried, all three
failed.

**None of those builds changed anything.** The installed `dishonored_vr.ini`
contains an explicit `Lag=1`, and the loader reads a compiled default ONLY when
the key is absent. The file was dated the previous day, `install.ps1` copies
binaries and does not touch it, and the generated default ini writes `Lag=1` too.
Both "fixed lag" tests ran at lag 1.

The A/B sequencer was unaffected because it stores into the atomic at runtime,
which is why cycling appeared to work while a "fixed" value did not - and that
apparent contradiction was written up as an unexplained correlation with the
sequencer's mere presence, complete with speculation about atomic-store side
effects and logging timing. All of it was a config file the instrument never read
back.

### The rules this cost

* **A build that changes a compiled default has changed nothing on a machine
  whose ini names that key.** Verify the EFFECTIVE value from the log, not the
  intended one.
* **The log must print what a lever resolved to and where it came from.** This
  one printed the selected arm per submitted frame (`chosen by lag arm N`) and
  that field was correct all along - it went unread, because the source diff
  looked like proof.
* An A/B that writes at runtime and an ini that writes at load are different
  mechanisms. A result from one says nothing about the other.

### And the measurement that was there the whole time

Sampled against the same run, the recorded orientation difference tracked the
sequence exactly: 0.119-1.079 deg while lag 1 was selected, 0.000-0.040 deg
across the whole twenty seconds of lag 2 - at head speeds up to 106.6 deg/s, so
not a still interval - and back to 0.473 and 0.403 deg within two seconds of
returning to lag 1.

That was averaged across the transitions and reported as "nothing in the log
distinguishes the phases". **Averaging over a variable that is being deliberately
switched destroys exactly the signal the switch was there to produce.**

A 0.3 degree error was also dismissed as too small to see. At 2750x2850 per eye
it is roughly five pixels near the centre of the image.


## THE HEAD-TURN JUDDER WAS A POSE ONE GENERATION TOO NEW (VR-65, SOLVED 2026-09-09)

The oldest and most damaging complaint in this mod. Static world geometry
juddered and ghosted on any physical head turn; thumbstick turning with the head
still was clean, and so was very slow head motion at a locked rate.

**`[Pace] Lag=2`.** Headset-confirmed, and the tester's own summary was that the
game feels an order of magnitude better to play, smooth enough that synchronous
spacewarp now works well on top of it.

### Why that value, and why the old one was wrong

The pose submitted with an eye image is chosen from a history of located views by
a fixed generation offset. The historical offset of 1 was calibrated against
**BioShock 1's single-threaded renderer**. This game has a separate render thread
AND a delayed D3D9 capture stage on top of it, so the pixels reaching the
compositor are one generation older than that assumption, and the pose submitted
with them described a head that had already moved on.

That is exactly why only a PHYSICAL turn showed it. The compositor reprojects for
head motion alone: a pose from the wrong moment costs head speed times the error,
costs nothing while the head is still, and costs nothing for a view turned with
the stick because that rotation is baked into the image and never reprojected.

### The measurement, taken by switching it live inside one run

| Selected | orientation difference, submitted vs the sample the camera consumed |
|---|---|
| lag 1 | 0.119 - 1.079 deg |
| **lag 2** | **0.000 - 0.040 deg**, across a full 20 s, at head speeds to 106.6 deg/s |
| lag 1 again | 0.473 deg, then 0.403, within two seconds |

The tester felt the same three phases in the same order without being told which
was which. An A-B-A that reverses is what makes it a result rather than a
coincidence.

### Two things that were wrongly ruled out on the way, and how

**"All three lag values were tried and all juddered."** They were not. The
installed ini names `Lag=1`, and the loader reads a compiled default ONLY when
the key is absent, so two builds that changed that default changed nothing on
this machine and both ran at lag 1. Their results were then read as the new value
failing. The loader now logs the EFFECTIVE value and whether the ini overrode it;
the per-frame field `chosen by lag arm N` was correct throughout and went unread.

**"The submit rate is the cause - 68 pairs against 90 Hz slots."** Falsified by
the tester, who ran it again and reported buttery smoothness at 61-72 submits/s
with an inconsistent presentation rate - the same cadence that had juddered.
Cadence matching is not what fixed this. It is worth keeping straight because the
smooth run was ALSO at 60 Hz with a near-perfect 56-of-60 fill, which made the
cadence story look right for the wrong reason.

**A 0.3 degree error is not too small to see.** At 2750x2850 per eye it is
roughly five pixels near the centre of the image. It was dismissed as invisible.

### The instrument mistakes this ticket cost, all one class

Every one was a number that could not mean what it appeared to mean:

* a 39% disagreement read from a bare global across two threads - retracted
* a 51-degree error differencing a UE world yaw against an XR tracking yaw, then
  doubled by a sign convention - retracted
* a near-zero produced by comparing a camera against the head values that camera
  was built from - circular by construction
* the render-side observation reading whichever `c0..c3` block came last, at
  33 uploads per view, so it was some shadow or interface matrix whose implied
  yaw sat at a constant 119 degrees
* controls that reported `passed 0 FAILED 0` for three builds running, because
  they were phased on records opened rather than checks performed, and then
  gated behind a leg that refused

**And the signal was in the log the whole time.** The orientation difference
tracked the switched variable exactly; it was averaged across the transitions and
reported as "nothing in the log distinguishes the phases". Averaging over a
variable that is being deliberately switched destroys the signal the switch
exists to produce.

### Still open

* The camera record does not yet guarantee it holds the sample the camera was
  calculated from: both writers still calculate from the loose `g_hmd*` globals
  and call `HtConsumeSample` afterwards. The signature was changed; the callers
  were not fixed.
* `g_viewsGen` is stamped one increment behind, so identical poses can appear to
  carry different generation ids.
* The render leg - what rendering actually consumed - was never obtained. The
  world view-projection has not been identified.

None of those block the fix. All of them limit what the diagnostic can prove
about rendered pixels, and they are why this is recorded as a confirmed
workaround with a measured mechanism rather than a fully verified root cause.

## THE RESOLUTION ASK: TWO FILES CARRIED THE SIZE AND NOTHING KEPT THEM EQUAL (VR-66, 2026-09-09)

### The symptom

Raising `[Screen] RenderWidth/RenderHeight` to 3200x3300, and then to 3190x3306
with both the mod ini and the game's own `DishonoredEngine.ini` written directly,
produced a fullscreen 2560x1440 device both times - half the 5120x1440 desktop,
and visibly low resolution. The mod's side looked healthy:

```
res: handed the game our 3200x3300@240 mode (slot 112)
res: CreateDevice - the game asked for 2560x1440 windowed=0 fmt=21
res: VirtualMode is on but the game asked fullscreen 2560x1440, not the
     3200x3300 it was handed
```

### The cause, and it was the mod's own file

The size had **two homes and one writer**:

| File | What it drives | Written by |
|---|---|---|
| `<gamedir>\dishonored_vr_launch.txt` | `-ResX/-ResY/-FullScreen` on the command line the engine **OBEYS** (read in `DllMain`, before the engine's entry point) | `ResRequest` only - the `res` seam word and the F10 picker |
| `dishonored_vr.ini` `[Screen] RenderWidth/Height` | the mode `VirtualMode` **ADVERTISES** in `EnumAdapterModes` (read at `EnsureConfig`, and it overwrites what `DllMain` set) | `ResRequest`, and **a text editor** |

Hand-editing the ini moved the second only. On the failed runs the launch file
still held the previous ask - it is dated 2026-09-04 on this machine, five days
before the attempts - so the engine was told `-ResX=2750 -ResY=2850` while the
proxy advertised 3200x3300. The engine asked for 2750x2850 fullscreen, that size
was not in the mode list any more (only 3200x3300 had been added), and **UE3 did
exactly what this file already records it does: fell back to a real display
mode.** The same fallback target as run 10 in "The render size" above, from the
same monitor's list.

**Neither ini ever contained 2560x1440. The mod's own stale launch file supplied
it,** by making the engine ask for a size the mod was no longer advertising. The
"stale command line" suspicion recorded when this opened was right; the stale
command line was ours.

### The fix (41.1, VR-66)

**One ask, two consumers.** `[Screen]` in `dishonored_vr.ini` is the authority
and the launch file is demoted to a mirror:

* `LaunchArgsResolveFromIni` reads `[Screen]` on the engine's **first
  `GetCommandLine` call** and overrides whatever the file carried. That call
  comes from the CRT startup glue at the exe's entry point, which is after the
  loader releases its lock - so the ini read `DllMain` must never do is safe
  there, and the ask still lands before the engine parses anything.
* `LaunchArgsBuild` sets the command-line size and `g_resWantW/H` (the advertised
  mode) from that single resolved ask, so the two cannot diverge again.
* A disagreement is logged as `launch: THE TWO ASKS DISAGREED` with both values,
  and the file is rewritten to match.
* `DllMain` installs the `GetCommandLine` hooks **even with no launch file**; an
  ini-only ask used to be inert because the old code returned before hooking.
* The `CreateDevice` mismatch warning now names the command line the engine was
  handed, the slot count, and whether the size it asked for is one of the
  adapter's real modes (the fallback signature). The old text blamed the game's
  ini, which is the inert route, and sent a session down it.

Editing `RenderWidth`/`RenderHeight` by hand now takes effect on the next launch,
by itself.

### CONFIRMED (2026-09-09, same day)

3190x3306 - the same 55:57 aspect as 2750x2850, 10.55 MP against 7.84 - was
armed and run. Every line carried it:

```
launch: the render ask is 3190x3306 fullscreen, VirtualMode ON, from the ini
        (the launch file agreed)
res: handed the game our 3190x3306@240 mode (slot 112)
res: CreateDevice - the game asked for 3190x3306 windowed=0 fmt=21
capture: 3190x3306 fmt=21 mode=sync
res: HONOURED - the game renders 3190x3306 as asked
xr: swapchain pair 3190x3306 format 29 (3 images each)
capture: 3190x3306 content bbox [0,0]-[3184,3304] = 100% x 100% (FULL)
```

Full bbox, no crop. **The engine has no ceiling at 3190x3306 and never refused
the mode** - it was only ever asking for a size nobody was advertising. The
first `launch:` line came from the launch file and the second from the ini, in
that order, which is the resolve doing exactly what it is for.

2750x2850 was restored afterwards; the size is a performance question now, not a
correctness one.

### The three lines that decide it in a log

```
launch: the render ask is <W>x<H> ... from the ini (...)      <- what won
res: handed the game our <W>x<H>@<hz> mode (slot N)           <- what is advertised
res: CreateDevice - the game asked for <W>x<H> windowed=0     <- what the engine wanted
```

All three must carry the same size. If the first two agree and the third does
not, the engine refused the mode; if the first two disagree, that is this bug
returning.

## THE WEAPON JUDDER: THE HAND WAS NORMALISED AGAINST A HEAD THE VIEW WAS NOT RENDERED FROM (VR-68, 2026-09-09)

The same fault as VR-65, one layer down, and it was hidden underneath it.

### The measurement

A lag finder compares the RENDERED camera's frame-to-frame yaw change - taken
from `MpAcquireCtx`'s basis, which comes from the render's own ViewProjection
constants - against the head's own change at several generations back. Mean
`|dB - dHead|` over 4085 moving frames, 5796 skipped as too still to
discriminate:

| lag | mean error |
|---|---:|
| 0 | 1.190 deg |
| 1 | 2.406 deg |
| **2** | **0.119 deg** |
| 3 | 2.405 deg |
| 4 | 1.192 deg |

Ten times clear of the runner-up, with `lag0 = lag4` and `lag1 = lag3` - the
symmetric V of a real minimum rather than noise.

**It compares DELTAS, never the two orientations.** The camera basis is in the
game's space and the head sample is in XR space; differencing those directly is
what produced the retracted 51-degree error. Frame-to-frame differences cancel
any fixed offset between the spaces. The draw runs on the RENDER thread and the
finder on the PRESENT thread, so the yaw is published through a seqlock - a bare
global across those two threads is the retracted 39 % figure.

### The fault

`MpDriveTick` normalised the controller against `g_devPose[0]`, the freshest
head. The draw plants the result with the camera basis from the render's own
constants, which is two generations older. The residual is two generations of
head rotation, at 1.19 deg mean - the same magnitude as the world error VR-65
fixed (0.119-1.079 deg).

**`[Pace] Lag=2` did not create this.** The hand error is rendered-versus-fresh
regardless of what the layer is tagged with. Lag 2 revealed it by removing the
world's own judder, which is why the report was that it had "probably been there".

### The fix

`g_headHist` keeps the head matrix four deep; `[Hands] PoseLag` selects the
generation the normalisation uses. Default 2, headset-confirmed by a reversing
A/B/A/B; 0 restores the old behaviour. It fails soft to the freshest head before
enough history exists.

**Not the submission lag applied twice.** The compositor reprojects the whole
image from the tagged pose. The world is correct because it was rendered from
that pose; the hand is correct only if it was placed against the same one. Both
consume that generation and neither is delayed again.

### The instrument that failed first, and why the failure was useful

An earlier build compared the head stamped at the pose consume against the head
the camera write snapshotted. Generation gap **0 on every frame**, under 0.1 deg
in 87 of 100 windows. Both values derive from `g_hmdYaw` around the same consume,
so it was near-circular and could only ever have caught a script-lane against
present-lane split - of which there is none.

**The negative result named the correct pair.** Fresh versus RENDERED, not fresh
versus fresh. Its head-speed column also read over 300 deg/s on half its lines,
because one tiny interval destroys a max; intervals under 2 ms are ignored now
and those speed figures are not usable.

## THE PERFORMANCE MEASUREMENTS (VR-67, 2026-09-09)

Open. Recorded so the next attempt starts from evidence.

* **`predictedDisplayPeriod` is not fixed and is not the panel refresh.** One run
  was asked for 40 fps (25.00 ms), the next for 80 (12.50 ms), and the 3 s summary
  only ever printed the period its window ENDED on. Every change is logged now.
  A full period does not prove spacewarp is off, and a doubled one does not prove
  it is on - the runtime's own decision depends on stats it may not get.
* **At 80 Hz: 57-80 delivered of 80.** GPU 7.7-12.4 ms per tick against 12.5 ms.
* **The worst window is not obviously pixel-bound.** 57/s, 17.5 ms tick, GPU
  12.4 ms, render-thread R 11.6 ms plus desktop Presents 4.8 ms, against a
  **0.1 ms** pacing wait. CPU, driver and synchronisation are not cleared, and
  the D3D11 bridge has never been timed at all.
* **Falsified by A/B**: the `FrameId` render-target readback, and D3D9Ex
  `SetMaximumFrameLatency` at 1, 2 and 3 - all inside the noise floor on median,
  tail and hitch count. The latency call succeeded and read back, so these are
  real negatives rather than refused levers.
* **Every gameplay hitch sits in the submission tail**, 19 of 19 in the last run,
  40-115 ms with `xrEndFrame` itself 32-108 ms against a 0.07-0.5 ms typical.
  **Where a wait is observed does not name what caused it**: that call takes a
  mutex, can wait on the previous submission, and can wait on D3D11
  synchronisation, so our own GPU work can be charged to it. The Wi-Fi link was
  reported healthy and has not been implicated.
* **Two counting traps, both paid for**: a hitch tally computed over a whole log
  rather than the gameplay window reported a shift that does not exist, and
  summary windows joined by eye rather than by timestamp produced a table where
  no row's GPU span belonged to its own frame rate.

## VR-69: a live script mono flag resets an older render eye (2026-09-10)

The first controlled candidate, `9fe2af45` plus the clean weapon-pose port
`22f275ba`, retained stable world/weapon head turns but reproduced persistent
outward weapon flicker in both eyes. Three weapon contracts remained active in
the recorded summaries. The final palette-eye population was 143,598 evaluations,
including 8,538 unknown-eye evaluations and zero large-step ambiguities.

`da1d760d` made `MpEyeForPresent` read `g_sdDoublingNow`, a live script-side
state, and clear the render-side eye and its previous sample when false. A
queued stereo draw need not belong to the latest script-side decision. This
branch remains active with PaletteEyeAlternate=0, so the earlier negative
alternation test did not exclude it. Unknown eye omits the half-IPD correction,
which moves a right-eye target right and a left-eye target left relative to their
corrected positions. That fits the report without a model-scale change, but
aggregate counters do not prove event-by-event visual correspondence.

`tools/palette-eye-host.ps1` compiles the production function with independent
script/render inputs. The first-candidate function fails five assertions; the
restored pre-#26 function passes all nine, including queued draws after a new
single-draw script tick. The restore also keeps the original large-step refusal
and per-present decision reuse. This tests a state transition, not all engine
view association. The second diagnostic is `b3a1ff46`, with camera/capture/pose
and candidate-contract behavior unchanged from the first. Its visible verdict
is pending. The retired experiment is preserved under `src/legacy/vr69`.

## VR-69: a ceiling write can invalidate camera offset ownership (2026-09-10)

After the eye restore at b3a1ff46, headset feedback confirms stable head turns
and weapon swaps, with residual flicker during downward character movement.
FovLeverApply clamps only Z in the selected camera field before apply_offsets.
The camera Writer recognizes a persistent field by all three coordinates; a
mod-owned Z clamp breaks that comparison and lets the next eye offset accumulate
on X/Y from the previous offset. The production writer reproduces this on the
host: the original clamp fails six of 19 checks; the ownership-preserving clamp
passes all 19. Fresh engine vectors and other objects/fields retain their old
behavior. No new engine address or offset is introduced.

417bfad9 changes the clamp call to preserve the original base only when the
entire pre-clamp field exactly equals that writer's own previous write on the
same object/field. Its bounded camera/clamp-rebase log establishes execution;
actual correlation with the remaining headset symptom is still pending.
See DOWNWARD_CLAMP_REVIEW.md for the evidence, scope and rollback baseline.

## VR-69: stability confirmed, projectile reticle aim restored (2026-09-10)

The 417bfad9 run contains eight camera/clamp-rebase entries and the tester confirms
the residual downward-motion flicker is gone. The successful log is preserved
locally under build/session-handoff-2026-09-10; its build/hash match the candidate.

That same run resolves motionaim=1 and redirects DisProjectile_Arrow from
view (-0.81,-0.59,-0.09) to hand (-0.51,-0.05,0.86), then again to
(0.55,0.32,0.77), followed by velocity steering. motion_aim.cpp is identical to
b38519c3 and e8ca4682; the later aim-ray experiment is not in the installed build.
The requested native-reticle restoration is an ini-only change: MotionAim.Enabled
1 -> 0. Exactly one byte changed; the confirmed DLL hash is unchanged. Both the
motion tick and fire-window arming are gated, so the next fresh launch leaves
projectiles to the game. The actual post-reset shot verdict is pending.

The full integration boundary and handoff are in SESSION_HANDOFF_2026-09-10.md.

## VR-73: THE HAND COLLECTOR TURNED OFF THE INTRO BOAT'S COLLISION (2026-09-10)

**Symptom.** New game, prologue: at the top of the water lock, when the arrival script
destroys the lift actor (`OnDestroy` on `InterpActor`), the player and the NPCs on the
boat drop about 570 uu into the water (`pawnZ 2406.8 -> 2031.1 -> 1838.4`); the NPCs die
and the mission fails. Both developer machines. A mod-off control (`disable_vr.txt`, same
game config) does not fall.

**The mutation.** `FpCollect` finds skeletal components by walking pointers out from the
pawn (depth < 3, every 4-byte field). A pawn standing on the boat points at the boat's
mesh, so the boat is one hop away. In all four failing runs, minutes into the ride:

```
handmesh:   [0] 'SkeletalMeshComponent0' asset=EmpressBoat_anim
collision: BlockActors CLEARED on 'SkeletalMeshComponent0' - a hand-driven mesh can no longer block movement
```

Two unconditional writes hit every candidate: `FpNoBlock` (the 38.23 crouch-wall fix,
clearing `BlockNonZeroExtent`/`BlockActors`) and `FpRestoreRotation`, which zeroed the
relative Rotation (`+0x19c`) and Translation (`+0x190`) of every candidate at the start
of every collect, whether the mod had ever written it or not. The same logs also list
`CorvoMask_world`, `FeatherDuster` and several doors as candidates: the collector
reaches world props generally, not just this boat. `FpIsViewModel` is a name test
(`pPlayerMesh`) and never established ownership.

**Why it is a regression.** Until `a6e00a6f` (2026-09-09, the candidate list's owner)
every `FpCollect` call site was a one-shot and the per-frame refresh sat behind a
hand-mesh gate that was off in the shipped configuration, so a whole session saw one
collect, normally before the boat mattered. `FpEnsureCandidates` re-collects from the
script tick whenever the list is empty or the equipment changed, and it runs above the
`g_handMesh` early return in `ApplyHandToMesh` (`BoneVisTick`), so it is live even with
the hand drive off. `b38519c3` does not contain `a6e00a6f`.

**Fix (unverified in a headset).** Ownership is read from the engine, not inferred from
the walk: `ActorComponent.Owner` and `Actor.Owner` are resolved by name
(`FindPropOffset`), and a candidate is owned when that chain reaches the possessed pawn
or one of the two held items within four hops (`FpOwnedByPlayer`, `fp_mesh.cpp`). Every
candidate line now logs `| owner <class> (the player's)` or `- FOREIGN, never written`.
Collision is cleared only on owned candidates (`collision: LEFT ALONE on ...` otherwise);
the transform writers (`FpDrive`, the calibration probe `FpCommandAll`) write only owned
candidates and record the prior transform on the first write (`FpMayWrite`), and
`FpRestoreRotation` returns only what was recorded, to its recorded value. The arm-hide
cull and the automatic material census skip foreign candidates too. If either Owner
offset is not found, every candidate reads as foreign and nothing is written - the log
says so (`handmesh/owner: ... NOT FOUND`).

**What to verify.** The owner line resolves both offsets; `EmpressBoat_anim` lists as
FOREIGN and never appears in a `collision: ... CLEARED` line; `Skm_Player` and the view
models list as the player's (if they do not, the 38.23 crouch wall can come back - watch
for crawl wedging); the arrival keeps everyone on the boat.

**Falsified on the way, one headset run each** (evidence and logs in the VR-73 ticket):
settings differences between the machines; the stock frame-rate bug on this boat
(mod-off control did not fall); the re-entry second draw and its tick rate (`[Stereo]
Armed=0` still fell at 74-90 ticks/s); a stale virtual-pad A read as held through the
570 ms seat-in hitch (a guard serving a neutral pad still fell, and itself produced
`Dis_Jump_ButtonDown`; the runs without it log no jump at the seat-in). The slow Z
descent at gameplay start is the water lock lifting the boat, not the boat sinking.

**Headset result, 2026-09-10 22:21 (commit `671ea554`, tested ini):** no fall. The log
resolves both offsets (`ActorComponent.Owner +0x48`, `Actor.Owner +0x10c`), lists the
boat as `owner SkeletalMeshActorMAT - FOREIGN, never written` with `collision: LEFT
ALONE`, prints no `CLEARED` line all session, and lists `Skm_Player` as the player's.
Still unseen: a weapon view model's verdict (none was collected in the prologue).

### The cinematic latch stuck ON at the arrival

With the fall gone, the same moment showed a second fault: the arrival raised the
latch (`cine: ON`), the headset went to the head-locked mono quad, and it stayed there
while the player walked and pressed buttons, until a pause and resume cleared it
(`cine: latch cleared - a loading screen`, 14 s later).

The latch is a parity flip on every `OnToggleCinematicMode`. The engine's handler
(`PlayerController`) reads the Kismet action's three inputs - Enable, Disable, Toggle -
and calls `SetCinematicMode`, which sets `PlayerController.bCinematicMode`
(`DishonoredPlayerController` overrides `SetCinematicMode` and calls a native). A
parity flip is only right if every Enable is answered by exactly one more dispatch; the
seat-in sends two in one millisecond and the arrival sends one. The ProcessEvent hook
also sees the call before the handler runs, so the flag cannot be read at the event.

First attempt (falsified 22:32): clear the latch when `bCinematicMode` (`+0x38c mask
0x8000`) had read 1 and then 0. It never read 1 in the 5 s after the arrival toggle
(`cine/truth: bCinematicMode has not read 1`) and the quad stayed until a pause. Since
every `OnToggleCinematicMode` ends in `SetCinematicMode`, which assigns that flag, a flag
that never reads 1 means either the arrival dispatch was not an Enable (the parity flip
raised the latch by mistake) or the wrong object was read (`g_peCtrl` rather than the
object the event was dispatched on). The "left stick does nothing" during the stuck
quad is consistent with the mod's own pad park, which the latch drives.

Fix (unverified): read four engine locks on the object the toggle was dispatched on -
`PlayerController.bCinematicMode`, the `bIgnoreMoveInput` counter (`IgnoreMoveInput`
adds or removes one, floored at 0), `bCinemaDisableInputMove`, and
`DishonoredPlayerController.m_bInputIgnoreInput_Cinematic` (set only natively) - polled
from `CineActive` every present, logged on change with the object's class. They may only
CLEAR the latch: after any lock read set, all clear for 250 ms; or no lock set at all in
the 2 s after the toggle. Raising the latch is unchanged, so the seat-in (ON and off in
one millisecond) behaves as before. If the objects or offsets are wrong, the change lines
show it, and the second rule would clear a real cinematic's latch after 2 s - watch for
that in the first cutscene run.

What the arrival is (tester observation, 2026-09-10): a scripted hold, not a camera
cutscene. The game locks movement for a few seconds - the left stick does nothing - while
looking around with the right stick and the head still works, then hands control back.
That is a movement lock without a look lock, which predicts `bIgnoreMoveInput` (or one of
the other two movement fields) set during the hold and `bCinemaDisableInputLook`-style
look locks clear.

**Headset result, 2026-09-11 (build `vr33-hands-working-93-g1927118c-dirty`, the code of
`d0697d25`): fixed** - stereo and movement came back by themselves at the arrival, no
fall. Offsets: `bCinematicMode +0x38c`, `bIgnoreMoveInput +0x39d`,
`bCinemaDisableInputMove +0x38c` (another bit of the same word),
`m_bInputIgnoreInput_Cinematic +0x610`. What the log shows, and what it corrects above:

* The arrival toggle read every lock 0 on the first poll (`0.0 s after the latch went
  ON`), and the latch cleared by the no-lock rule after 2.0 s. So the arrival dispatch was
  NOT an Enable: the parity flip had raised the latch by mistake, which is also why
  `bCinematicMode` alone never read 1. The 2 s window is the short mono spell still seen.
* A real cutscene shortly after (no looking around) read `bCinematicMode=1
  bIgnoreMoveInput=2 bCinemaDisableInputMove=1 m_bInputIgnoreInput_Cinematic=0` on the
  first poll and kept the head-locked quad until a loading screen, as designed.
* The opening look-around hold on the boat stayed in stereo throughout.

Follow-ups, deliberately not in this change: cutscenes in stereo with mono reserved for
menus, and a no-lock window far shorter than 2 s (a real Enable reads set on the first
poll) - VR-75.

## VR-57 native crossbow fire seam (offline derivation, 2026-09-12)

The read-only FireWatch is not needed to find this seam. Offline analysis of the
installed PE followed the FireCrossbow class metadata at 0x01361928 to constructor
0x00C29DB0. It installs the context vtable at 0x01172C80; the entry at 0x01172E30
is the native firing routine 0x00C38230. No generic UE3 firing assumption is used.

The function resolves its source pawn via 0x00BFF440 and stores it at EBP-0x54.
It computes two possible spawn positions, joining at 0x00C38BBB. At this join,
EBP-0xB8 holds the selected spawn position and EBP-0xAC the chosen unit direction.
The direction is converted to a rotator by the call at 0x00C38BD0, followed by
spawn at 0x00C38BF4. At 0x00C38DB6 the same direction local is passed to the
projectile initializer through vtable offset 0x3A4. The Arrow constructor at
0x00C57A90 installs vtable 0x011851E8, whose 0x3A4 slot selects 0x00C54E70.
That wrapper calls 0x00C540A0: the direction argument is multiplied by the speed
arguments and written to Velocity at +0x1B4/+0x1B8/+0x1BC, at
0x00C54152/0x00C5416B/0x00C54171. The initializer also derives actor orientation
from that same direction. This is initialization, not later steering.

The hook at the common join replaces only the local direction with
normalize(controllerEndpoint - selectedSpawn), before either consumer. It does
not write Actor.Rotation, change the spawn position, change speed, or retain a
projectile pointer. The bridge preserves flags, integer registers, x87/MXCSR/XMM
state and replays the six displaced bytes. Live source-pawn possession and the
exact FireCrossbow context vtable gate the write. The initializer call bytes are
also checked before installation. Runtime behavior remains untested by this
session: no game or simulator launch is authorized.

The native cache helper 0x00C14460 compares the cache tick tag at context+0xD0
against the current native tag and can refill the cache before returning it.
The firing routine calls it through 0x00C14640. Thus the categorical claim that
the aim cache is never read by firing is incorrect. The failed earlier writes
still justify bypassing that writer; no blind tag changes are introduced.

Known scope: native target/obstruction selection happens before this hook, and
native homing/assist setup may happen afterward. This candidate fixes launch
convergence; it does not claim controller-based tracing, assist removal, ballistic
impact prediction, or weapon-model alignment. Those behaviors remain separate.

## The native function registration table, and a tool for the vtable route (2026-09-12)

Three seams have now been derived by walking the same route by hand. It is
`tools/ue3-natives.py`, and it **re-derives the published crossbow numbers before
printing anything about a new class**, refusing on a mismatch - a route that
cannot reproduce an answer already written down is not evidence about a new one.

Confirmed by that tool: the crossbow (`0x01361928` / `0x00C29DB0` / `0x01172C80`,
slot `+0x1B0` -> `0x00C38230`) and the pistol (`0x01361AE0` / `0x00C29E00` /
`0x01172E60`, `+0x1B0` -> `0x00C2A3E0`).

**The image carries UE3's native function registration table: 2554 entries**
pairing an ASCII `A<Class>exec<Function>` name with the exec thunk's address.
This is a new capability for this project - a route from a function NAME to code,
where previously only class names were searchable. A UE3 exec thunk parses the
script stack and then dispatches through a VTABLE SLOT, so the thunk gives the
slot, and the slot gives the implementation.

Worked example, and the reason it was run: `UDishonoredCheatManagerexecToggle
UsableHighlight`, name at `0x010C8F1C`, thunk `0x009F3820`. The thunk dispatches
`+0x4AC` on the cheat manager (vtable `0x01144070` from class
`DishonoredCheatManager`, metadata `0x0133AE50`, ctor `0x00B832A0`), giving the
implementation `0x00B6F300`. That function is four instructions and toggles **bit
`0x400` of the dword at cheat-manager `+0x5C`**.

A `.text` sweep for readers of that bit returns exactly two, both inside one
function: `0x0060E4AF` and `0x0060E62F`. They sit in a loop that strides a
20-byte list, calls `0x00646B20` per entry to reach a cheat manager, and tests
the bit before building a box on the stack - i.e. the usable-highlight draw.

**The engine's own trace is script-callable, and that shapes the fix.** The table
gives exec thunks for `AActor::Trace` (`0x006D0ED0`), `FastTrace` (`0x006CD240`),
`TraceActors` (`0x006D6D20`) and `APlayerController::GetPlayerViewPoint`
(`0x005D1430`). So a second interaction query does not have to re-implement any
geometry or duplicate the engine's collision rules: it can run the engine's own
trace along a different ray. That is the difference between a fix that agrees
with the game by construction and one that agrees with it until a case diverges.

**The interact button is a native read, not a script call.** The binding is
`Button m_bUseButton` (plus the `GBA_Use` / `GBA_Use_Gamepad` aliases), which
sets a bool on the input object that native code polls. Property names are not in
the image - they come from the packages - so `m_bUseButton` and any
current-usable field must be resolved at RUNTIME through the existing
FName-keyed property resolver, not found offline. See GAMEPLAY_STATE.md.

## VR-85: the focused interactable, found by watching rather than reading (2026-09-12)

**`DishonoredPlayerController::m_pCrosshairActor` at `+0x69C` and
`m_pCrosshairHighlightActor` at `+0x6A0`.** Both hold the actor currently under
the crosshair; both read `none` when nothing is focused.

Found by observation, not offline analysis, because offline analysis could not
reach them: interaction has no `exec` anywhere in the 2554 native registrations,
so ProcessEvent never sees it, and property names live in the packages rather
than the image, so nothing in the PE names them. The game's own
`m_bDrawInteractableDebugBox`, armed in its ini, drew nothing - it is compiled
out of the retail build.

`PropWatch` (`ue3/prop_watch.cpp`) collected every object-typed property declared
on the player controller, the player pawn and the HUD - 46 of them, from 5376
object properties examined - and sampled them on the script lane while the tester
looked at crossbow bolts and away. Both fields transitioned
`none <-> DisProjectile_Arrow` **19 times each**, matching the deliberate
look-at/look-away repetitions and nothing else in the run.

Measured behaviour, from the same session: selection is a SINGLE winner from a
narrow trace, roughly a hand's width of tolerance around an object at arm's
length. Not a candidate set to re-rank, which rules out the cheaper design.

Everything else the probe reported in that run was a level-transition artifact -
pawn fields appearing to become `DisSeqAct_SetStoryFlag` and the like, in one
burst, because a pawn pointer is reused across a load. Those are noise and are
recorded here so the next reader does not chase them.

**ANSWERED, and the answer is that these fields cannot be driven by writing
them.** A write-and-observe experiment wrote the field on the script lane
whenever the engine's own value was `none`, then re-read its own write on the
next tick. The counter came back `wrote 1, survived to the next tick 0`: the
engine recomputes the field after our tick, every tick, so nothing downstream can
ever read what we put there. Confirmed behaviourally too - the focus did not
persist and the pickup did not happen.

So the fields are a RESULT, not an input. Aiming interaction from the controller
has to happen at whatever computes them, and that writer is still unfound. The
experiment is retired to `src/legacy/interact_focus.cpp`; it also crashed the
game, for a reason worth reading in TRAPS before any of this is revisited.

**The interaction seam itself is NOT yet found.** What is established: interaction
is entirely native (the script dump carries declarations only, and there is no
`exec` for it anywhere in the 2554 entries, so it is never exposed to script);
`DisInteractableInterface`'s `CanInteractParams` carries an `m_DisTraceFlags`, so
selection goes through a flagged trace; and `[Engine.PlayerController]
InteractDistance=512` is its length. The remaining step is the writer of the
current-usable field that the highlight loop above reads.

## VR-82 native pistol fire seam (offline derivation, 2026-09-12)

The crossbow route above, re-walked for the pistol against the same image
(SHA256 `66443f3d...e17e`). Every published crossbow number was reproduced by the
re-walk before any pistol number was trusted, which is the only reason to believe
the pistol's: class metadata `0x01361928`, constructor `0x00C29DB0`, context
vtable `0x01172C80`, firing routine `0x00C38230`, all recovered exactly.

`DisItemContext_FirePistol` and `DisItemContext_FireCrossbow` both extend
`DisItemContext_ProjectileAttack`, and their native halves are laid out the same
way. Class names are UTF-16 in `.rdata`; the metadata struct is the dword that
points at one, its `+0x14` is the constructor, and the constructor's `mov [esi],imm`
installs the context vtable whose `+0x1B0` slot is the firing routine.

| | crossbow | pistol |
|---|---|---|
| class metadata | `0x01361928` | `0x01361AE0` |
| constructor | `0x00C29DB0` | `0x00C29E00` |
| context vtable | `0x01172C80` | `0x01172E60` |
| firing routine (vtable +0x1B0) | `0x00C38230` | `0x00C2A3E0` |
| source pawn (`0x00BFF440`) | `0x00C38276` -> `ebp-0x54` | `0x00C2A417` -> `ebp-0x1C` |
| aim cache (`0x00C14640`) | `0x00C3832A` | `0x00C2A443` |
| vector -> rotator (`0x0040D260`) | `0x00C38BD0` | `0x00C2A543` |
| SpawnActor (`0x00C66070`) | `0x00C38BF4` | `0x00C2A57A` |
| projectile initializer, vtable `+0x3A4` | `0x00C38DB6` | `0x00C2A611` |
| pre-spawn join (the hook) | `0x00C38BBB` | `0x00C2A53C` |

`0x00C2A3E0` is `ret 4`, 468 instructions. **The initializer slot is the same
`+0x3A4`** even though the projectiles differ (`DisBullet` against an Arrow), and
both call it with the direction local pushed twice by address, so everything
downstream of a direction write was already traced by the crossbow work.

**The pistol derives its spawn POSITION from the aim direction; the crossbow does
not.** At `0x00C2A468`..`0x00C2A4BF`:

```
spawn = origin + dir * [tweaks + 0x420]
```

where `[tweaks+0x420]` is `DisTweaks_FirePistol::m_fBulletSpawnDistance`, shipped
at 150.0, and `tweaks` is `[esi+0xA4]` cached in `ebp-0x18`. So position and
direction are one decision. Writing only the direction would stand the bullet off
150 units along the OLD direction and then aim it from there - it would still
converge on the target, but it could begin its flight inside geometry the aim line
never crossed. The mod reconstructs the engine's origin (`spawn - dir*dist`), aims
from there, and rebuilds the standoff along the corrected direction.

**The direction local is written after the aim cache returns.** `ebp-0x48` is
passed BY ADDRESS to `0x00BFFBA0` at `0x00C2A51A`, which can still write it, so a
hook placed at the natural-looking spot - just after the aim cache - is
overwritten and changes nothing while its counter moves. `ebp-0x60`, the original
unit aim direction the standoff was built from, is written once by the aim-cache
out-param and never again, which is why the reconstruction reads that and not the
rotation local.

Locals at the join, all verified written-once before it: `ebp-0x54` spawn
position, `ebp-0x48` direction, `ebp-0x60` original aim direction, `ebp-0x18`
tweaks, `ebp-0x1C` source pawn, `ebp-0x6C` the rotator out-param.

Constants and displaced bytes are in `patterns.h`; the design is
`dishonored/VR-82-PISTOL-FIRE-SEAM.md`.

## VR-57: the view model's aimable geometry, and what it costs to read

Measured 2026-09-12 while building the model ray. Every number here came from a log
line in a real run, not from a guess.

**`c5` carries the camera position NEGATED.** The render-side camera constant is minus
the world position on this build. Found by printing a bolt's launch point beside the
controller origin derived from `c5`: launch (15195 8100 2868) against origin
(-15053 -8230 -2854) - an exact mirror through the world origin on all three axes,
five shots running, while the engine's own `camZ` in the same run read **+2879**. The
negation is applied in one accessor (`camera::render_pos_world`); `render_pos` stays
raw for the two re-entry consumers that only difference it against itself, where a
global sign cancels.

**Asset names are not mesh identities.** `bolt_01` covers more than one mesh: the same
asset name measured at **length 44.531** (variance ratio 335.3) and at **length
20.708** (ratio 39.8) in a single run. The regular and the poison bolt are different
shapes sharing a name, so anything keyed on the name alone will silently mix them.

**The view model's components and their assets**, as the renderer reports them:

| Component | Asset | Notes |
|---|---|---|
| `pArrowMesh_HighRes` | `bolt_01`, `Bolt_Flare` | the loaded bolt; 257 verts; the only reliably aimable geometry |
| `pBulletMesh` | `Gun_bullet_regular` | the pistol's loaded bullet. MEMBER but its component transform reads ALL ZEROS, so it can never be a verified held instance |
| `pPlayerMesh` | `crossbow_01`, `Wpn_PlyGunElite`, `Wpn_PlySword01` | the weapon bodies |
| `pMesh` | `EliteGun`, `Skm_Player` | the gun, the player body |

**Weapon bodies are too big for the skinned-geometry reader**, which accepts at most
1024 vertices: `crossbow_01` is **1961**, `Wpn_PlySword01` **2481**, and other
view-model draws reach **6330**. They are also skinned to more than one bone. So a
weapon body cannot supply an axis through that path at all, and a refusal that quotes
the variance test is misleading - the size check rejected it first.

**The native crossbow fire hook is weapon-specific.** It installs at the crossbow's
pre-spawn join and announces itself as `player crossbow firing context only`, so the
pistol's shots are not hooked and follow the engine's own aim. The pistol's fire path
is a separate address and is not yet traced.

**`AttachRigRadius` is clamped to a minimum of 10 and the log prints the CLAMPED
value.** An ini carrying 2 therefore produced "past the 10 uu rig radius" while the
file said 2 and the default was 200. At that bound every weapon is refused as not
being on the view model, and the hands keep placing normally - so the symptom is
"weapons stop tracking" with every nearby counter healthy.

## Menus, loads and object identity (VR-93, 2026-09-13)

**A save loaded from the pause menu replaces the player pawn and controller, and the
new pawn keeps the old one's FName** (`10783_0` before and after, pawn 16ADB400 ->
16C38C00, build 191). An FName is therefore no evidence that an actor survived a load.
The signals that do fire: the event stream latches a different pawn pointer, and the
UI event `OnLoadGameClicked` fires when the save browser opens (before any load).

**A pause resumes through a benign LOADING.** `OnResumeGameClicked` reads LOADING for
about 0.5 s until the view dispatches again, with the same pawn, controller and weapon
components throughout; no NO_PAWN is read during a pause of a second or two.
Mid-level LOADING periods of 0.75 to 10.6 s were also seen with the pawn and controller
identity unchanged across them.

**The UI observer rescan costs ~500 ms of game thread on every resume** (498 to 530 ms
over eight scans) and found the same 48 movie-player instances each time.

## The viewport draw root's callers (VR-80, 2026-09-13)

`tools/disasm-rva.py <exe> calls 0x1fc5b0` lists exactly four static E8/E9 callers of the viewport
draw root `0x005fc5b0`. The gameplay one is `0x006330dc` (`kViewportDrawCallSite`, the re-entry
stub's site). The other three, bytes read from the image:

| Site | Bytes | Form |
|---|---|---|
| `0x004dba66` | `6a 01 e8 43 0b 12 00` | `push 1; call root` - bShouldPresent TRUE, inside a small thunk (`push ebp; mov ebp,esp; mov ecx,[ebp+8]`) |
| `0x0061236a` | `e9 41 a2 fe ff` | `jmp root` - a tail call after `pop edi; pop esi; pop ebp`, argument passed through |
| `0x00641d85` | `6a 00 e8 24 a8 fb ff` | `push 0; call root` - bShouldPresent FALSE |

A draw through any of them does not reach the re-entry stub and so pushes no eye tag. Which of
them runs in play, and whether one runs after a note closes, is NOT yet measured;
`[Stereo] DrawCallerTrace` counts them through pass-through stubs (patterns.h`kViewportDrawCallerA/B/C`).
Measured (build 199, a run with a book close): A, B and C made 0 calls. `xref` finds no absolute
reference to `0x005fc5b0` and a raw search for its bytes finds none, so no vtable or pointer table
reaches the root: in gameplay the gameplay call site is its only live caller.


## GC reference crash recurrence, 2026-09-13

The pause-at-end crash repeats the existing VR-96 signature, seen in earlier
September 11-13 runs. Verified build `215-g20cc4a98-dirty`, compile time 18:01:15;
logs and dump archived in ignored `build/single-tag/playtest-crash-20260913-181413/`.
At log time 17246.765 s Dis_OpenPauseMenu is observed; 63 ms later the first AV is
at Dishonored.exe+0x65894, reading 0x3F800008, with EAX=0x3F800000 and
EDI=0x168A7434. Offline `tools/disasm-rva.py` against the installed executable
shows the reference is loaded from [EDI] at RVA 0x6586D, and the fault at
0x65894 reads the referenced object's flags at +8. Thus EDI identifies the slot
holding the invalid reference; it does not by itself identify its owning UObject.
The value equals IEEE-754 float 1.0. This is evidence of invalid reference data,
not proof that any particular mod float writer produced it.

The new 45,180,167-byte minidump repeats the capture limitation in TRAPS:
it is written 125.672 s after the first fault and carries a later exception
at engine RVA 0xAF6A73 reading address 1, on the same thread 37176. It does not
include the original reference slot 0x168A7434 or token storage 0x165481B0.
The captured stack at the original ESP no longer represents the initial fault.
`tools/read-dump.py` parses its exception and thread context despite a nonfatal
PEB-read warning from the minidump package. Do not use the later exception or
stack as the original GC context. The archived crash text file predates this
run; use the current run's main log and the screenshot for its first fault.

The writer and owning object remain unresolved. Future fault capture needs the
initial exception context and surrounding reference/token/owner memory before
the game's error path replaces them. Do not skip the invalid reference or catch
and resume this GC fault as a repair; neither would remove its source. No engine
address was added to executable code during this offline triage.


## First-fault GC diagnostic installed, 2026-09-13

The current delayed minidump cannot identify the GC reference owner. Added an
opt-in `[Diagnostics] GcFaultDump=1` path: config verifies the six instruction
bytes in patterns.h at the known fault site and arms the core crash handler.
Only an AV/read with both exception address and context EIP matching that site
captures full process memory and the original exception context, once per run.
Capture runs before stack fingerprinting and its three-message budget. It then
continues normal exception handling; no engine-memory write, GC skip, exception
recovery or change to stereo/note behavior. Unrelated faults retain old behavior.
Byte mismatch refuses and disarms the diagnostic. The option defaults off and
is not inserted into generated/saved defaults. A full dump may be large and adds
writing time only when the targeted fault occurs; it stays local under the data
folder and must not be committed.

`tools/crash-capture-host.ps1` compiles the production handler in a standalone
32-bit process: 18 checks pass, including off, byte mismatch, unrelated exception,
write/execute, missing context, wrong site, teardown, exhausted logging budget,
once-only capture and original context/flags. Its actual Windows dump was read
back: original EAX/EIP and a reference-address heap sentinel are present, with
full-memory output 23,262,847 bytes. This tests evidence capture, not crash removal.
Release build, nine exports, ini golden generation and lint pass. No game launch.

Installed build `215-g20cc4a98-dirty`, compiled `Sep 13 2026 18:27:13`.
DLL SHA256 `1a6e3156c6c994f3dd57a401e44ffe7d1f7b59f12295cbb51c877073f7250725`;
ini SHA256 `d47a790a97862653d740436d939c155d2299d9931a4a445ca4af557b99a97782`.
Backup and complete ini diff: `build/crash-triage/install-20260913-182848`.
The only installed ini change is `[Diagnostics] GcFaultDump=1`; full section/key
comparison and byte checks confirm CRLF. Confirmed note, startup and reload-eye
fixes stay enabled; startup name cache stays off. Linear remains inaccessible in
this task; no new ticket number, commit, PR or merge.

One launch question: does opening pause after a save reload still crash? Load
normally, reload the save once, play for about a minute, then open pause. If it
crashes, the initial-fault dump should preserve the bad reference and surrounding
object memory, allowing owner/property identification and a targeted writer fix.
If pause opens, this intermittent fault did not reproduce; it does not establish
that the crash is fixed. If a different fault occurs or capture fails, use its
first log fingerprint to choose the next step. The tester only launches/reports;
the agent reads the build banner, archives evidence, and analyzes it before any
relaunch. No additional note/performance question is combined with this test.


## Pause GC crash: recycled hand controls corrupted upgrade objects (2026-09-13)

**Cause identified; writer fixed; headset verification pending.** The targeted
first-chance dump captured the original exception at engine RVA 0x65894 reading
0x3F800008, EAX=0x3F800000 and EDI=0x16877434. Its 2,133,436,870-byte full-memory
capture preserves GObjects, GNames, the reference slot, and the owning object.
The matched build was `215-g20cc4a98-dirty`, compiled 18:27:13. Archive:
`build/crash-triage/playtest-20260913-183257/` (ignored). Log SHA256
`4f58bc6a63de6171ee72bc11f08fe28333c1b2e32657e4b8b556a45f5b231ceb`;
dump SHA256 `a100287a2e7001895efb67e512391ae75b425b445a6b3bf843e28ac84fef2f68`.

The evidence joins the old and new owners, not just a matching float value:

| Address | Before reload (graft log at 18441.859 s) | At initial GC fault |
|---|---|---|
| 0x168773D0 | Player SkelControlSingleBone slot 0 | Twk_Upgrade_Mask, DisTweaks_Upgrade |
| 0x168772E0 | Player SkelControlSingleBone slot 1 | Twk_Upgrade_BoneCharms2, DisTweaks_Upgrade |
| 0x168771F0 | Player SkelControlSingleBone slot 2 | Twk_Upgrade_BoneCharms, DisTweaks_Upgrade |

All three replacement objects contain 0x3F800000 at +0x64 and +0x78, exactly
the two stores made by the crawl-release loop. The mask's bad reference is
base+0x64. Dump reflection resolves that inherited field to
`DisTweaksBase.m_pSpawnedObjectClass_Editor` (ClassProperty); it must hold a class
reference, not float strength. Class ancestry and the instance's 0xEC property
size put the slot inside that object, rather than inferring ownership solely
from nearest address. Property offsets are derived from the dump; no new
upgrade-object offsets are used by the fix.

The player was tucked when reload began. At 18482.468 s the wrapper logged
`hands: back`, executing the unguarded 1.0 writes. Only THEN did
ApplyHandToMeshInner log stale controls and discard them. At 18482.578 s the new
controls were discovered. Thus the old pointers were trusted for a write before
the inner function's validity check could reject their new owners. IsReadable
only proved mapped storage; after reuse, even IsLiveObject alone would pass.
The first-fault dump's current cached controls are already the replacements,
which is why a current pointer-only search would miss the earlier corruptor.
PDB-based data inspection was performed against the matching diagnostic build
before rebuilding; raw outputs and dump data are not committed.

**Fix:** SkcSetCrawlStrength refreshes BuildLiveSet on each crawl edge, requires
IsLiveObject plus SkcAlive's saved GObjects index and class identity, and checks
both target ranges before either write. Reused/dead/unreadable entries are
skipped and mark the cached controls stale for normal rediscovery. Failed live-
table refresh skips all writes. Valid controls retain the existing 0-on-tuck /
1-on-release behavior. Edge-only logs report writes/refusals. No repair is made
to an already-corrupted game process; testing requires a fresh launch.

**Validation:** the host compiles the production writer and SkcAlive guard.
13 checks pass: valid release/restore, fresh-table update, three live recycled
upgrade objects, same-index different-class reuse, readable freed storage,
refresh failure and paired-field readability. The old writer fails three
regression checks and overwrites the recycled upgrade bytes. Menu retention 43
checks, release build, nine exports, lint and diff checks pass. The causal write
was reconstructed from the address/lifetime/log/code evidence and reproduced
in the host, not captured by a live write watchpoint. Headset crash absence is
still unverified; no claim that all possible GC corruptors are removed.

**Installed fix:** build `215-g20cc4a98-dirty`, compiled `Sep 13 2026 18:43:51`.
DLL SHA256 `b4b5fbf58817684fe78b6717695634c15e257b248634c697a0ea537662d85d06`. Entire installed ini is
byte-identical to the diagnostic build, CRLF verified; GcFaultDump stays on.
Backup, binary, matching PDB, source snapshot and empty full-ini diff are under
`build/crash-triage/fix-install-20260913-184506/`. No game launch or merge.

**One launch question:** does pausing remain safe after reloading from a crouch?
Start normally, crouch, reload the save while crouched, then stand and open/close
pause a few times. A pass plus stale-write refusals validates the repaired path;
a pass without refusals is weaker non-reproduction. A crash should still produce
the initial-fault dump, which distinguishes a missed path from another corruptor.
The tester reports the visible result; the agent verifies the banner, reads and
archives the log. Existing fast-note and stereo settings remain enabled.


### Final headset verification and default promotion, 2026-09-13

The verified 18:43:51 build completed a clean run with 11 pause openings and no
exception. The crawl writer refused nine stale control updates over three edges
and performed 57 updates to validated controls over the remaining edges. Thus
the stale-pointer guard was exercised, rather than merely failing to reproduce
the trigger. The tester confirmed normal behavior. VR-96 is fixed for this
observed cause; later distinct crash signatures must be investigated separately.
Archive `build/crash-triage/playtest-pass-20260913/`; log SHA256
`a082eae06a07fb3fcbba4656f8933d82232e9b55f06d656469b6338a249bd6be`.

The maintainer explicitly approved commit, PR and merge of the current branch,
and promotion of the complete installed settings/F10 profile to repo defaults.
This supersedes the earlier default-off disposition for these tested levers.
`release/dishonored_vr.ini` is the byte copy of that installed CRLF profile;
WriteDefaultIni and the golden match, including diagnostics, calibration values,
D:\dvr-data, fast notes, late/single-tag repair and retained menu identities.
The name cache stays off. No config version bump rewrites existing settings.
The real default writer runs in a standalone x86 host and its output is compared
byte-for-byte to the installed and packaged files. Package generation now includes
that exact ini. Presence/migration sentinels remain distinct from value defaults.
No release or milestone is declared. Linear API synchronization remains pending;
the PR's Fixes link may update VR-96 through the integration, to be verified next
session. The next feature is physical head movement during cinematics.

## VR-70 cinematic camera ownership investigation (2026-09-13)

Opening boat ride: physical head movement is reported locked while right-stick
turning works. This is existing VR-70, with VR-43 related. No new headset run
has been interpreted in this session.

PR #12 is open/unmerged. Its projection/screen policy does not implement
Matinee camera tracking. Current CineDrive::AuthoredLook has no Dishonored
consumer. Runtime cinematic_active() describes quad fallback, which also
includes menus/loading, so it is not cinematic identity. The direct fallback
must remain held during scripted scenes: taking the controller broke the boat.
The opening look-around can have no cinematic latch (VR-73 evidence).

Offline native derivation (ue3-natives.py --verify reproduced the known
FireCrossbow seam first):
- DishonoredPlayerController metadata 0x01316948 -> ctor 0x00ABD1F0 ->
  vtable 0x01118738. GetPlayerViewPoint thunk 0x005D1430 calls slot +0x3C4,
  resolved to 0x005E17A0. It reads PlayerCamera at controller+0x384 and
  returns camera+0x330 location and camera+0x33C rotation (ret 8).
- Native registry Camera.GetCameraViewPoint thunk 0x005CFEC0 independently
  copies those same cache fields at 0x005CFF85..0x005CFFB9.
- Script declarations agree: Camera.CameraCache contains TCameraCache.POV,
  with TPOV.Location and Rotation. The trace resolves this by reflection;
  no new hardcoded engine offset or hook was introduced.

These are getter semantics, not proof a cinematic honors a new write. A
draw-scoped composition over the authored cache, restored after both eyes,
is a candidate seam. First determine which head components already reach it
to avoid doubling a working rotation or translation. Do not replace the
authored camera with a controller/free-camera drive.

Read-only cinematic_trace.cpp samples at the existing gameplay draw entry,
100 ms cadence: current controller-owned camera/pawn, reflected cache and
controller rotation, camera influence weights, animation state, copied HMD
pose/generation, script dispatch/write counters, requested positional offset,
menu/latch/projection/quad states. Prior render c5 is explicitly unsynchronized
and must not be compared as a same-draw acceptance measurement. Required
reflection misses retry every 5 s after a live camera appears; missing live
objects trigger at most one table rebuild per second. No engine writes.

[Cine] Trace defaults off; cinetrace on|off is live. This is a diagnostic,
not the head-motion implementation. Installed Trace=1 enables the next
user-owned boat test. Build, nine exports, lint and production golden checks
pass. Standalone simulator passed 60 frames/zero errors with the incompatible
OBS implicit layer disabled only for its child process. Its initial -32 was
XR_ERROR_FILE_ACCESS_ERROR, not a failure of this mod. No game launch.

Plan and one-question test: CINEMATIC_HEAD_TRACKING.md.

## 2026-09-13: VR-70 boat camera bypass measured; scoped rotation candidate

Build 218-ge5c7653f, compile 19:34:31, banner verified. Opening boat trace #39-319
(281 samples,29.907s) has full Soiree animation influence 1/0/0, no menu/quad,
and continuous PVR writes. Head pitch moves 62.55 degrees; controller follows;
final cache pitch moves 0.02. Walk negative control:25 matching PC/cache rows.
No deliberate lean or stick comparison. Later resume samples are stale/quad
and excluded. CINEMATIC_HEAD_TRACKING.md has archived identity and ranges.

The candidate writes reflected CameraCache.POV.Rotation only across the two
viewport draws: authored * inverse(entry head) * current head. Location uses
the existing offset seam with a frozen request and composed stereo right axis;
rotation/location/provenance are restored before the next engine update.
No extra native hook or new literal engine field is introduced. Each new engine
store requires fresh-entry IsLiveObject, current object-slot membership, retained
class and full FName, and current controller camera/pawn links. Menus/load or
failed ownership discard the reference. Failed restore preserves foreign fields.
This is not yet a measured downstream acceptance of rotation stores.

## 2026-09-13: VR-70 reference reset caused by draw cadence

Build219-g0ebd7a3e,19:54:54,archive playtest-20260913-200152.33 entries mean
32 unintended reanchors,with menus clear,quad off,same live owner and load epoch.
20 align exactly with SINGLE(no present since previous draw); remaining reasons
are obscured by log rate limiting.2311 writes/restores,zero refused. Non-double
gating must not clear the physical reference. Candidate now overlays centered
single scene draws too and holds reference across unavailable scene/runtime/pose.

Walk samples157-159 separately override PC pitch8.59..7.90 with cache-26.59
while influence0/1/0. Other Walk phases match PC/cache. No blanket player-camera
head overlay is justified. Added reflected read-only bCinematicMode,cinematic
move/look disable,ignore cinematic,ignore move/look counters; unavailable=-1.
They do not change the legacy latch or runtime presentation policy.

## 2026-09-13: VR-70 cinematic position must not cancel the player neck arc

Build220-gdf783ca8,20:09:13:stable gaze before/after stick prompt is reported;
one anchor,3210 writes/restores,zero refusals. Pitch causes opposite vertical
motion. The installed cancel pivot is0.321m below/0.062m behind. TrackHead adds
its negative modelled arc to zRaw even when final camera influence is fully
animated. That authored camera bypasses the player pitch arc. Publish normal
and without-cancel requests together; only begin_view_scope chooses the latter.
Real positional tracking and intentional add mode stay,gameplay keeps cancellation.
This is a candidate awaiting headset acceptance,not a synchronized render proof.

## 2026-09-13: native cinematic hide-letterbox control (VR-43 related)

Actual Documents game .ini search found no letterbox/black-stripe/aspect key.
Decompiled declarations identify SeqAct_ToggleCinematicMode.m_bHideLetterbox,
DishonoredPlayerController.SetCinematicMode_Native argument8,and the cinematic
HUD mask level0. Verified native resolver reproduces DishonoredPlayerController
metadata/vtable; registration thunk009EEA00 calls slot584 to00AAF150. The latter
sets mask6010 at level0 on cinematic entry. At00AAF215 it tests [ebp+24] (arg8)
and calls009EA0C0 with mask10,level0. That helper performs HUD.m_ShowFlags[level]
&=~mask; observed array base is HUD+4E0. Resolve DishonoredHUD.m_ShowFlags by
reflection if implementing; hardcoded masks/addresses belong in patterns.h.

This proves an explicit HUD-level hide-letterbox path. Final draw consumption
and visible acceptance remain unverified; do not promise the viewport is full
resolution from the flag alone. Camera aspect constraints and transient HUD
m_bDrawUIBlackStripes are separate non-config fields. Exported native method
stubs are not evidence of actual native return values. No game-derived code
or binaries committed; no game config altered or game launched.

Native follow-through confirms a Scaleform overlay:00B960E0 queries the HUD's
mask10 through009EA130,compares its prior movie flag,and on change invokes GFx
SetBlackStripes with the resulting boolean. This path explicitly controls UI
stripes rather than a viewport rectangle. Visual confirmation that the exposed
pixels fill the headset view still belongs to the upcoming A/B.

Letterbox timing:the updater B960E0 is called at BB23D3 near the end of movie
virtual BB2280 (float delta time,slot1DC in vtable115CD90). It caches combined
flags at movie+1D4 and letterbox visibility at+1FC bit40000. Its helper009EA130
combines all six HUD mask levels,so clearing only cinematic level0 may leave
another state's stripe request active. A draw-scoped clear/restore may miss this
stateful movie update. Prefer intervening at the narrow stripe query; otherwise
a validated override must survive until consumption. Do not infer the exact
Tick/PostAdvance phase from the native signature alone.

Narrow letterbox query seam,verified offline:at00B96115 the seven-byte setup is
6A 10 E8 14 40 E5 FF (push10;direct CALL at00B96117 to009EA130,return00B9611C).
The helper is __thiscall,HUD in ECX,one32-bit stack mask,integer boolean EAX,and
ret4 on both exits. It tests all six levels. Replacing only this call with a
same-signature wrapper can return0 while a live lever is enabled and forward
otherwise. This preserves all HUD mask values and uses the game's existing
SetBlackStripes cache/update logic; disabling restores the requested visibility
on the next movie update. Any implementation must verify the seven-byte setup
and decoded target before patching and put the literals in patterns.h. This is
a derived plan,not yet patched or visually accepted.

## 2026-09-13: letterbox query interception implemented

cinematic_letterbox.cpp patches only the rel32 operand at the derived query
site on the game/script lane. It verifies the full seven-byte setup and decoded
target before patching. The __fastcall wrapper preserves ECX plus one stack
argument and calls the original __thiscall helper. It returns0 only for the
expected mask/caller under the enabled projection-session/non-menu gates.
No UObject identity is retained or written; native code supplies its current
HUD and reads its flags. No additional engine-object store requires a liveness
exception. Code patching follows the existing byte-verified call-site pattern.
OFF forwards the native result so the movie cache restores native visibility
next update; no repeated executable patching.136 x86 decision/fingerprint
checks pass. Visible filled bar areas remain the next headset acceptance.

## 2026-09-13: VR-103 script-derived stereo permission

Choice_Base declares m_DialogState: Listening0, Choosing1. Read by reflection
from the current validated FSM state; no numeric offset guessed. InDialog and
InScriptedChoice represent in-world choice scenes; InStore also inherits the
base and must not be accepted merely by inheritance. Soiree exposes Matinee and
control state. Native bodies are unavailable in the dump. The old cinematic
parity latch measurably parks the runtime at scene entry and for2s after a
no-lock handoff. Presentation permission is now separate from strict gameplay,
behind default-off StereoState. See [candidate evidence](STEREO_STATE_TRANSITIONS.md).

## 2026-09-13: VR-50 cinematic FOV source and scoped override

DisConv_PlayerLookAtSpeaker declares m_bUseZoom/m_fZoomPercentOnScreen;
StatePlayerMasterInDialog retains the look constraints. DishonoredPlayerCamera
has ControllerLook FOV priority, m_fCurFOV and a separate arms FOV. No global
cinematic zoom-off INI found. Reflection names Camera.CameraCache -> TCameraCache.POV
-> TPOV.FOV for a draw-scoped override with current live identity. Readings in
accepted225 span36.6..107.9 against108.1 target. Request is not acceptance; next
headset run tests the final-cache consumer. Full plan: [FOV/hands](CINEMATIC_FOV_AND_HANDS.md).

## 2026-09-13: VR-104 native cinematic hands

Reuse current FSM names Soiree/InDialog/InScriptedChoice in VR-88 handback.
Its existing consumers restore arm/bone/material visibility and native mesh
and weapon rendering. Default-off CinematicHandBack. Saved SkelControl release
now checks full FName/class/slot on restoration, refreshes on menu/load/ownership
edges, and marks changed identities for rediscovery. Candidate mesh/cull
restores require live-object membership plus retained full identity. Existing
measured draw-distance pair +0x1bc moved to kArmDrawDistancePair in patterns.h;
this is provenance cleanup, not a new guessed field. Combined with VR-50 at the
user's explicit request; headset acceptance remains pending.

## Combined follow-up: FOV exit, pitch comfort, mantle hands

Build230 47c626a521:40:15 is headset-confirmed for cinematic FOV suppression
and native hands/arms, with a residual shrinking square on exit. Archive:
build/cinematic-fov/playtest-20260913-220129. All31202 FOV scopes restored,
zero refusals. At tick30273250 the override released in Walk and the host
immediately claimed52 degrees; readback reached107.6 at30274375 (1125ms later).
That native exit blend explains the reported rapid expansion.

The FOV exit bridge now keeps the validated same owner's override during Walk,
Falling or Jump until the sensor is within0.5 degree of the VR target. It cannot
start from ordinary gameplay zoom. Menus/loads, invalid state/identity and an
invalid sensor cancel it. A3s bound prevents indefinite suppression if readback
stalls; the observed1125ms recovery is covered. No runtime policy change.

VR-105 clarification: suppress forced up/down TILT ONLY. Height changes,
physical HMD movement and actual climbing remain. Default-off Cine.LockPitch
uses physical absolute HMD pitch with the same clamp as gameplay. Fully authored
head-look composition replaces only its resulting pitch. Other live animation
states use a validated draw scope that retains the gameplay translation request.
Yaw/roll and position are preserved; both draws share the scope and exact incoming
fields are restored afterward. F10 View Suppress animation up/down tilt; seam
cinepitch on|off; Save As Defaults persists it.

VR-104 extension: default-off Anim.MantleHandBack uses the exact live
StatePlayerMasterMantle state and the existing native hand/weapon/arm consumers.
It does not add every locomotion state. The older explicit preference to keep
controller hands while mantling is superseded for this installed candidate by
the new request. F10 View Native hands while mantling; seam mantlehands on|off.

All three changes are combined in one build as explicitly requested. 38 FOV/
handback checks,28 head math/policy checks and13 extracted production-scope
checks pass. New checks cover convergence, timeout, stale owner, physical tilt,
unchanged yaw/roll and preserving the gameplay position request. Runtime smoke,
exports and final install identity are recorded below. No merge approval.

## 2026-09-13: upright cinematic tracking follow-up

Build232 (4ec4f457, compile22:10:59) log banner verified before interpretation.
Both logs archived in build/cinematic-fov/playtest-20260913-230307. The tester
reports the FOV exit improvement successful; mantle handback was not tested.
Forced pitch suppression exposes a remaining tilted-axis swivel while holding
the Empress. At tick32642531 authored P/Y/R=-57.78/55.56/32.61, composed
P/Y/R=-32.21/40.92/54.40. This is smooth camera-axis coupling, not eye flicker.
Replacing pitch AFTER full rotation composition leaves authored tilt in yaw/roll.

The comfort path now constructs upright yaw as authored yaw plus physical yaw
relative to the entry reference, then applies physical pitch and roll. Authored
yaw and camera location remain active. Independent default-off Cine.LockRoll
(F10 Suppress cinematic roll; cineroll on/off; Save As Defaults) removes authored
roll, retaining physical HMD roll. LockPitch keeps its independent toggle.

Decompiled DisConv_PlayerLookAtSpeaker declares maximum pitch/yaw constraints;
DishonoredCamera_PlayerControl resets controller rotation, and camera influences
form a non-additive group. Native bodies are unavailable. The log includes942
fully animation-owned,304 fully player-owned and66 blended InDialog samples.
The prior head scope skipped the latter two populations. Scripted Soiree,
InDialog and InScriptedChoice now keep a final head scope across those influences.
A100ms lease from a successful live scope suppresses controller HMD injection,
including direct fallback, so physical yaw is applied once. Native controller
and stick changes remain; script resume references stay current. Unknown weights,
menus, owner changes, stale poses and runtime loss retain refusal/reset guards.
No new engine offsets or persistent engine-field writes are introduced.

Reported restricted movement is provisionally interpreted as head rotation;
physical lean versus rotation clarification is pending. The fix is a candidate,
not a rendered acceptance claim.39 math/ownership checks and13 extracted camera
scope checks pass, including the steep authored-axis regression and preserving
ordinary gameplay blend ownership. New one-question test: holding the Empress,
look left/right and up/down; expect no orbit or forced roll and unrestricted
physical look. A remaining orbit rejects upright composition; a yaw lock points
to ownership/constraint handling. FOV and mantle settings stay enabled.

## 2026-09-13: VR-106 steep-pitch standing roll arc candidate

Smooth scene-camera motion, not eye flicker. Standing uses a0.321/0.062m neck
pivot; crouch0/0 has no modeled arc. Two legacy branches fall back to rolled
axes at horizontal forward magnitude0.2 (about78.46 degrees pitch). The x86
control reproduces -20.633/+20.633uu lateral motion at85-degree pitch and
-/+40-degree roll; zero crouch pivot yields zero. No new headset measurement.
Default-off UprightPitchArc computes pitch-only compensation and keeps camera
position axes upright at steep pitch; true eye-right stays rolled. Ordinary
pitch parity and12 numerical regressions pass. Exact-pole position refusal is
logged; normal pitch is clamped short of it. Full plan, limits and staged
playtest sequence: [standing arc](STANDING_PITCH_ROLL_ARC.md). Parent PR56 and
this child remain unmerged pending separate testing.
## 2026-09-14: cinematic input ownership must hand back activity and heading

VR-109: live PVR dispatches can perform no mod writes by design. A write-age
counter cannot alone describe camera activity during final-camera head ownership.
FaceRotation also consumed the last published gameplay heading indefinitely;
its producer stopped during cinematic free look. The candidate releases that
consumer, bounds target age and resets outgoing head contribution at the first
fresh gameplay publication. Full identity/possession and IsLiveObject checks
precede the facing request write. No new native offsets. Evidence and untested
acceptance are in CINEMATIC_FOV_AND_HANDS.md (2026-09-14).

## 2026-09-14: selectable native view-facing movement

Camera.HeadBasedMovement bypasses the separated FaceRotation request replacement.
The existing measured native seam faces the pawn toward the full view; head mode
leaves its request untouched and adds no engine writer or input rotation. The
character mode remains available and its post-cinematic drift remains open.
15 production-handler host checks pass; headset acceptance is pending. Details
and recorded failed reference reset: CINEMATIC_FOV_AND_HANDS.md latest section.


## 2026-09-14: native UI ownership versus background rendering

VR-107/VR-108 and existing VR-74/VR-71 use reflected current engine/player/world/
UI-manager ownership and GFx open/main-screen state. Loading uses the Bink overlay
movie lifetime across the Continue prompt, primed by loading mode/start/map-hints.
The save notification alone is insufficient. No fixed offsets or engine writes.
Declarations establish the properties; native timing is still unverified.
See [mono UI state](MONO_ANCHOR_UI_STATE.md) for exact field names, identity and
cadence contracts, historical build60 clamp evidence, controls and falsification.

## 2026-09-14: VR-108 persistent movie service is not presentation

Verified build250-g78abb4fa (Sep14 08:17:32), matching installed DLL hash.
Logs/INI archived at build/mono-ui-test/playtest-20260914-082607. Main-menu
anchoring/navigation is reported successful, but gameplay remains mono while
weapons track. ui/surface enters Loading at5282609 and never releases before
exit5313921. This falsifies the service-pointer lifetime lease.

Decompiled DisBinkOverlayManager declares m_pBinkMovie as native Pointer.
Verified ue3-natives derives metadata01350cf0, constructor00bb5000 and vtable
0115d130. Native initialization00b9e4ee copies global movie service0145be80
into the overlay movie field and registers the overlay through service slot24.
It is a persistent service, not an active movie allocation. Draw gate00bb9880
calls service slot1c and draws overlay text only for nonzero result.
Global initialization009dd3ef selects0093d470 (real service) or004ebb60 (null).
Real constructor0093d130 sets vtable010a4610; slot1c is00932cc0, a pure
32-bit read at service+130 then return (8b8130010000c3). Null vtable00fcde60
slot1c is00722980, returning0 (33c0c3). Real active field is initialized0,
set1 at00932c57/009337ad and cleared at00932c31/00935ea4.

Candidate reads that presentation field without calling native code. Requires
expected vtable, exact getter address/bytes, readable field and boolean range;
revalidates current live overlay pointer and service vtable after reading.
The service is not a UObject; no IsLiveObject claim is made for that allocation.
Overlay/engine reads retain current UObject liveness checks. No engine writes.
All new native addresses/offsets are centralized in patterns.h.

Loading mode/transition can acquire before presentation starts. Continue keeps
the lease while presentation is active. Idle service releases despite stale
started/hints metadata. Unknown layout does not release. Periodic ui/loading
prints each input, service pointer and presentation state, including failure.
24 host anchor/lease cases pass; native timing and headset acceptance pending.

## 2026-09-14: build252 rejected; follow script WaitMovie completion

Verified252-g2d62aca2, Sep14 08:35:52; matching installed DLL. Both logs/INI
archived at build/mono-ui-test/playtest-20260914-084045. Main menu starts with
service field+130 already1; after loading it remains1 through gameplay and
pause/unpause. At6156625 mode0 transition0 started0 hints1 yet lease1.
The claim that+130 proves visible presentation is retracted. It is overlay
selection/enabling state and remains set beyond movie playback. Do not reuse.

Additional decompiled declarations: Engine.WaitMovie, StopMovie with delayed
stop until game rendered; GamePlayerController.ShowLoadingMovie,
KeepPlayingLoadingMovie and ClientStopMovie. Native Engine.WaitMovie005e2600
calls service slot34 ->004dbc30, which waits on service+1c event via event slot14.
The service's movie-status queries004eb190/004eb2a0 also sample that event with
zero timeout and return complementary finished/unfinished results. This is a
completion signal, not hand tracking or an arbitrary button press.

Constructor00500cfe creates the event with manual-reset1 and initially false,
stores at+1c, then signals at00500d62 for initial idle. Factory00420500 creates
event vtable00fb98a8, Win32 HANDLE at+4. Its slot14 ->00416670 invokes imported
WaitForSingleObject with the caller timeout. Factory initialization uses
CreateEventW at IAT00f941d8; SetEvent/ResetEvent are00f941dc/00f941e0.
Code/slot/create bytes are verified before reading the current event handle.
A SYNCHRONIZE-only duplicate is observed at zero timeout then closed. Manual
reset means observing cannot consume completion. Current owner/service/event/
handle identity is checked; unknown/failed reads retain mono. No engine writes,
no native function calls, and no retaining handles across polls.

31 host checks include actual manual-reset event pending, completion, repeated
observation without consumption, second-load reset and invalid handle refusal.
The prior overlay-enabled value remains diagnostic only. Movie completion now
controls the existing loading lease. Headset timing is still unconfirmed.

## 2026-09-14: native drop-assassination decision (VR-111)

Decompiled DisItemContext_DropAssassinate derives directly from DisItemContext,
not the melee/finisher context. Reflected fields m_pPlayerOwner,
m_ContextStatus (idle0/failed1/in-progress2/finished3), m_CachedDropType
(none0/too-high1/do-now2), m_pCachedTarget and
m_TickTagAtWhichCacheIsValid expose its cached decision. Actor.Velocity is
secondary telemetry. Read-only diagnostic must distinguish cached tick from
an observed attack; it cannot prove rejection reason from type0 alone.

Verified ue3-natives class derivation: metadata0135ff60, constructor00c0b180,
vtable0116c7f8. Offline slot198 leads00c14960, which caches the result of
00c09a50. That routine calls00c09900 for candidate eligibility and uses a
trajectory from00c09810. The trajectory uses pawn velocity and gravity,
not the physical head's view ray. Eligibility includes target state, vertical
velocity, minimum drop and a collision trace, with distance selecting do-now
versus too-high. Do not patch a guessed camera angle or expand handback lists
to fix an attack that never entered a finisher. No new engine offsets/hooks
are introduced: all diagnostic fields resolve through reflection.

## 2026-09-14: VR-112 view-plane lens isolated on build258

Verified258-g4a78a745 compiled09:37:23, matching installed DLL hash. Both logs
archived build/mono-ui-test/playtest-20260914-101645. Failure reproduced:
crossbow retains native head aim; hand fore/aft affects its vertical position.
For the late crossbow groups, draw basis times inverse predicted basis is a
symmetric view-plane stretch, ratio1.0466343..1.0466350. Applying that same
matrix to predicted translation reproduces rendered translation within0.0014uu.
The body/hand bridge is rigid and sword has no extra stretch. This refutes a
uniform-scale error, missing component, or corrupt bridge for these samples.

Candidate AttachViewLens defaults0, with live F10 Hands checkbox and saved INI.
Fit only S=s(I-ff^T)+ff^T using the current rendered view forward axis. Validate
its matrix residual, then undo S before the existing strict rotation/position/
scale matcher. Do not accept an arbitrary affine or uniform scale. The weapon
correction is D_hand*S_inverse, so both position and orientation inherit the
same hand delta without retaining the extra lens. Known sibling passes derive
their own lens from current component/view snapshots. No engine writes or
retained lens across frames; native animation handback remains first veto.

Host tests cover lens fit, original refusal, strict match after removal,
controller translation, noncommuting hand rotation, tilted view, identity,
and refusal of uniform scale, shear, wrong axis and displaced world instance.
Next test is the same hub-arrival crossbow tracking reproduction, including
left controller fore/aft while holding head still. Expected native head aim
and axis coupling disappear. Headset acceptance pending; no merge.

## 2026-09-14: partial left-eye weapon surfaces after lens correction (VR-112)

Reported on verified build260-g7a0bbd46, compiled Sep14 10:23:35, DLL
SHA256 a37a0589c58697480d38253a112eccb7abfff2da5833d2576dd4b3fa4beee40b.
Both logs archived at build/mono-ui-test/playtest-20260914-104623.
Crossbow unsheath/tracking is headset-confirmed. New report: parts of both
weapon models intermittently disappear in the left eye, even while still;
hands remain intact. This is distinct from whole-view VR-99 note flicker.

Measured: all84 sampled draw/prediction matrix pairs (42 per hand) in this run
have identity relative scale within float noise, unlike the real1.046635 lens
in build258. Maximum eigenvalue deviation is9.72e-7 for sword and3.56e-7 for
crossbow. Build260 nevertheless applied the fitted near-identity correction.
Late-run no-delta stays126 while successful corrections continue; no restore
failure. Suppression also continues, but its population is not proof of the
reported partial-surface cause. Do not disable all auxiliary draws: prior
experiments lost legitimate lighting/colour contributions.

Candidate: reject identity lens fits within32 float epsilons and use the exact
original correction path. Keep real lens cancellation and existing instance,
rotation, position and scale guards. No new engine writes, stale matrix cache,
or depth-state change. This removes a measured false positive; whether it
caused the visible flicker still requires the headset. Two identity/roundoff
regressions fail before the change and pass afterward; real-lens/axis and
world-instance tests continue passing. AttachScaleTrace adds bounded per-eye,
per-hand main/auxiliary lens decisions with common-eye and depth-state values.

Next test: with both weapons drawn and head/controllers still, do their parts
remain continuously visible in the left eye? Success supports roundoff as the
cause. If it persists, inspect wa/lens-pass by eye and depth state, then capture
matched depth/colour pass geometry before changing suppression or tolerances.
PR58 remains draft; all three stacked PRs remain unmerged.

## 2026-09-14: residual crossbow-only transparency after boat travel (VR-112)

Verified build262-g9f27c514, compiled10:55:24, DLL SHA256
b8bf05fd81be9ce1ad53caf5dd2ba5937ae2c9a93c74db6d9fe4d2d5f3360f8d.
Both logs archived at build/mono-ui-test/playtest-20260914-110406.
Headset result: weapons stable before travel; both track after arrival;
sword has no transparency in either eye. Crossbow retains smaller partial
transparency after travel. This supports the identity bypass and narrows the
remaining symptom to real lens cancellation. The residual eye distribution
was not separately specified; do not claim it changed eyes.

Measured: crossbow now has a real lens around1.0356818. Of25 sampled same-eye,
same-Present main/auxiliary pairs with active correction,6 differ in fitted
ratio, maximum2.38e-7. No sampled common-eye mismatch. Sword lens remains
inactive. This is roundoff disagreement, not evidence of two different zooms.

Candidate reuses an identical inverse lens for numerically equivalent fits
of the same component, same Present and same eye. Reuse tolerance is32 float
epsilons, matching the identity arithmetic guard. A meaningful lens change
replaces the value immediately. Different eyes, Presents and components do
not borrow it. The fixed64-slot table contains only numeric lens snapshots
and component identity tokens, never retained hand deltas or engine writes.
Normal current identity/matching gates still authorize each draw. This is
not the retired stale-draw rescue or auxiliary-pass suppression experiment.

Seven host checks cover first fit, exact same-view roundoff reuse, opposite
eye, next Present, other component, real lens change and unknown eye. Existing
lens/identity/controller/world-instance tests pass. Release, exports, lint and
golden INI pass. The visual cause remains a hypothesis until the next test.
Next launch question: after the boat arrival, with weapons drawn and head/
controllers still, does the crossbow remain fully opaque? Success supports
pass consistency; failure requires joined per-pass transform/depth evidence,
not broad suppression or relaxed matching. wa/lens-pass logs reuse decisions.
No game launch. PR58 remains draft; no merge. The prior Linear update is
still blocked by automatic approval review pending explicit permission.

## 2026-09-14: accepted cinematic/UI/weapon integration

Build264 verifies the final residual crossbow surface fix on the headset.
The full findings and failure-to-fix chronology across PR56/57/58 are consolidated
in [STACK_ACCEPTANCE.md](STACK_ACCEPTANCE.md). It records scoped cinematic
ownership, upright pitch/roll math, head-based movement, native loading completion,
post-load stereo permission, unchanged aerial eligibility, and per-view lens
consistency. The current installed profile is promoted verbatim by request;
no new engine layout or memory writer is introduced by default promotion.

## 2026-09-14: severe physical-head-turn flicker at120Hz (VR-116)

Surface: headset world image during cinematics and gameplay, exclusively on
physical head turns. Holding still and right-stick turning do not reproduce.
This routes to image/pose attribution and cadence, not weapon transparency or
static eye-swap flicker. The VR-115 desktop branch is parked, committed at
1d6558a3; its candidate was never installed. New branch starts at VR-Main255d1c91.

Verified installed266-g5aa625ae, compiled11:25:16, DLL SHA256
257ab2551fadfec93d66a1e25067143c6643257709596f8303ecb5482015c7a4.
Both logs and INI archived at build/flicker-120/20260914-133932.
Runtime period8.33ms confirms120Hz. Pace.Lag=2 and Hands.PoseLag=2; both lag
A/Bs are off; SyncHz=0. Lag was not accidentally disabled by default promotion.
In25 steady opening windows, stereo submits/s min/median/max81/90/100;
interval SD2.18..10.32ms. Later windows drop to66..82 pairs/s. Real cadence
pressure is measured, but physical-only symptoms make pose association the
first focused test. Cadence alone is not proven to explain this severity.

Steady-opening ring accounting reconciles, with no late repair/expiry and no
empty pops in those windows. Eye summaries report healthy1/0 ages without
stale eyes or pair aborts. Sampled frame identities show distinct eyes and no
swapped-side result. These summaries are evidence against the old sustained
late-tag failure, not proof that every pixel of every frame is correct.
The old posesub trace prints109 right-eye lines and zero left-eye lines.
Its final mean input/submission orientation difference is0.120deg, moving mean
0.259deg, maximum7.934deg. A large one-off disagreement is measured; its visual
correlation is not. The old posejoin camera/render comparison uses a latest
VP observation and reports large disagreement; it cannot establish an exact
cinematic image projection and is not treated as proof of a camera-write bug.

Candidate Pace.ImageOrientation defaults0, saved by F10 View's Image-linked
head orientation checkbox. When a successful stereo texture copy carries a
valid same-eye camera record, use that record's normalized head quaternion
for the copied image's submission orientation. Missing/expired, wrong-eye,
invalid camera, missing generation or invalid quaternion retains existing lag.
The record already travels with the captured texture; no new history matching
or engine-memory write is added. Camera/hand behavior, pacing, positions and
resolution are unchanged. Positional image attribution is outside this first
rotation-only candidate. This tests camera-input association; it does not
prove that the renderer honored the input. Both eyes log decisions separately.

Eight host checks cover delayed-image pose selection, wrong/unknown eye,
missing record without output damage, invalid camera/quaternion/generation,
and sign-equivalent quaternion. Existing frame/weapon/animation tests, release,
exports,lint and golden INI pass. No game launch. Headset result pending.
Next launch: keep120Hz; in the opening, turn the physical head side to side
with the stick untouched. Does the severe flicker disappear or substantially
reduce? Improvement supports image-linked orientation; unchanged or worse
requires the new per-eye decisions plus render/pose evidence. Do not declare
90Hz a fix, or silently alter numeric lag or install the desktop experiment.

## HEADSET-CONFIRMED BREAKTHROUGH: world smoothness at 120 Hz

2026-09-14, VR-116. Build **271-g8cd27652** is the preserved known-good
world-smoothness checkpoint. The tester reports an exceptional improvement in
world stability during physical head movement, including during substantial
frame drops. This is the strongest reported world-smoothness result to date.
The remaining slight flicker is confined to hands/weapons and is NOT a failure
of the accepted world result. Preserve this baseline before hand changes.

Verified log banner: compiled Sep 14 2026 13:46:59; installed DLL SHA256
`d3853fb75b71d4cbbdcceea2281b17664939c306290b879918e5d6f40eca5102`.
Exact DLL/INI bundle: `build/playtest-candidates/vr-116-image-orientation`.
Both run logs and INI: `build/flicker-120/accepted-world`.
`Pace.ImageOrientation=1`, world lag 2 and hand lag 2 retained.
Final sampled counters: left accepted 38258/fallback 0; right accepted
38253/fallback 2. These are application counters, not perceptual measurements.

**Preserve the image's own head orientation, not a guessed fixed history age.**
The headset result supports this principle for rotational world reprojection;
it does not establish higher frame rate or positional reprojection correctness.
The old fixed-lag-only diagnosis is superseded for this reported world symptom.
Next: apply image-specific head normalization to shared hand/weapon corrections,
with a separate default-off toggle and the accepted world behavior unchanged.
No merge authorized. Historical pending-test statements below describe earlier work.


## 2026-09-14: hand/weapon follow-up candidate (VR-116)

World checkpoint remains unchanged: source 8cd27652, tag
`vr-116-world-smooth-271`, acceptance documentation commit 827b2db5.
Patch branch: `codex/vr-116-flicker-fix-patch`.

`Hands.ImageOrientation` defaults off, live in F10 View and saved independently.
With both world and hand switches on, hand draws inspect the front queued
camera record on the render/present consumer lane, without popping, repairing
or searching ahead. Unknown/wrong eye, empty/skewed queue, expired record or
invalid head basis retains legacy normalization. This is the next image's
queued record, NOT the previously delivered capture record and NOT the latest
camera publication. The association remains a candidate until headset/log
validation, particularly around late tags; eye identity alone cannot prove
absence of every possible queue fault.

A coherent snapshot now carries the head rotation it was normalized against.
The draw rebases both position and orientation by
`F * transpose(R_imageHead) * R_oldHead * F`. Controller samples, head
translation, camera movement, compositor metadata and numeric lag stay intact.
Weapons consume the same hand correction through WaCommon, including depth and
other material passes. No weapon pose-generation equality gate is restored.
No engine-memory writes or native-animation ownership changes are added.

Host tests cover noncommuting head rotation with a stationary controller,
position/orientation consistency, a fixed-history negative control and invalid
inputs. Queue tests cover nonconsumption, ordered capture, missing/wrong eyes
and excessive queue depth. Per-eye `ms/image-orientation` logs report applied
and fallback counts plus camera/hand generations. Hand result is pending.

Next launch at 120 Hz, weapons drawn: hold controllers steady and physically
turn the head side to side. Does the slight hand/weapon flicker disappear
while world smoothness remains intact? Yes supports image-specific hand
normalization; unchanged/worse requires inspecting applied/fallback counts
before changing pose timing again. Never launch the game or merge this branch.

## 2026-09-14: hand candidate inconclusive; exact world baseline restored

Build `vr-116-world-smooth-271-3-g12a974134`, compiled 14:09:46,
verified DLL SHA256 dead0ade5441462c9380f43280e7e846fa9da34f6cb4a7967194aa9384b94435.
Both logs and INI archived in `build/flicker-120/hand-test-left-jump`.
Reported: possible small hand improvement, but a one-frame LEFT-eye-only
leftward hand displacement persists during physical yaw while watching hands.
Possible reduced world smoothness was reported with uncertainty. Neither hand
acceptance nor a proven world regression should be inferred from this run.

Final sampled hand counters: left accepted 13365/fallback 1963 (12.81%);
right accepted 13493/fallback 110 (0.81%). Populations are hand draw contexts,
not distinct images or visually marked flicker events. Unknown-eye decisions
are counted in the left bucket by the current diagnostic, so the percentage
is not a pure left-image failure rate. Exact fallback reasons are not printed.
World submission samples end with left accepted 15479/fallback 0 and right
15468/fallback 0. Diff against 8cd27652 confirms no change to world submission
selection, only the separate hand control API. This cannot disprove a timing
or perceptual difference from additional render-thread work.

Routing: section 1's one-eye sideways hand/weapon jump (VR-95), NOT partial
weapon transparency (VR-112). Fixed-head normalization alone is insufficient.
The existing eye classifier holds its old eye on small right-axis jumps;
this can assign a right-eye offset inside a left-eye image. It also gates the
new record lookup, so a wrong classification can deny normalization correction.
Current evidence is suggestive, not event-correlated proof. Do not re-enable
the old blind toggle predictor: its still-head regression remains unresolved.
The old eyecheck reports every comparison UNKNOWN (7089 toggled, 454 SAME,
3 ambiguous). Its text claiming zero disagreement clears the classifier is
invalid when nothing was compared. The lookup asks about a future present;
classification needs a deferred join to the resolved image, not this immediate
future-record query. Fix that instrument before accepting its conclusion.

Restore the EXACT accepted DLL and INI from
`build/playtest-candidates/vr-116-image-orientation`, build 271-g8cd27652.
Keep hand candidate code committed, unaccepted and available separately.
Next launch at 120 Hz in the same scene with physical head turns:
does the exceptional world smoothness return compared with the hand candidate?
Yes isolates the difference to the hand candidate/build path; no leaves scene,
runtime variability or baseline perception unresolved. This is a baseline
comparison, not a claimed hand fix. Afterward, instrument actual per-view eye
identity and deferred classifier agreement before changing eye offsets.
Never launch the game. No merge or external publication authorized this turn.

## Current acceptance: world smoothness approved for main, 2026-09-14

The exact original world candidate 271-g8cd27652 was restored and independently
reconfirmed on the headset. Exceptional world stability returned. Its installed
INI is now copied byte-for-byte to the release and golden INIs, and the default
writer matches it. `Pace.ImageOrientation=1` is the generated and missing-key
default by explicit user request after acceptance; explicit 0 remains respected.

This merge contains the WORLD fix from 8cd27652 only. The unaccepted hand
normalization code from 12a974134 is absent. The branch
`codex/vr-116-flicker-fix-patch` preserves that experiment for later investigation.
The accepted DLL/INI bundle and `vr-116-world-smooth-271` tag remain recoverable.
The user explicitly approved publishing this accepted state to VR-Main.

Latest symptom refinement: residual left-eye hand flicker was observed after
exiting the opening cutscene, while loading a sewer save was essentially clean.
This makes transition state relevant; it does not prove the precise cause.
Track the deferred hand issue with VR-95 and preserve its historical predictor
regression. Do not resume or include hand changes in this merge.

Reconfirmation logs/INI: `build/flicker-120/reconfirmed-world`; banner and DLL
hash match 271-g8cd27652, compiled 13:46:59. Source changes beyond that checkpoint
are documentation and INI/default promotion only. No engine-memory writer,
camera transform, hand pose or frame pacing change is introduced by promotion.

Next: complete and verify the authorized main merge; hand/cutscene work waits.
Historical pending, publication-blocked and candidate text below records earlier
stages and does not override this scope or the new explicit merge authorization.

## The HUD sections carried from the abandoned PR #12 branch (VR-117, 2026-09-14)

The four sections below were written on `claude/dishonored-vr-hud-cinematics-149a8e`
(session 10, 2026-09-04) and are carried here verbatim because that branch never
merges: they are the measurements the HUD redo (VR-117, `core/gfx/hud_class`,
`hud_capture`, `hud_layout`) is built on. Its cinematic findings are superseded by
PR #56 and #58 and are NOT carried. Numbers were taken at 2496x2688; the shipped
size is 2750x2850 since session 15 and the rule does not depend on the size.

## The HUD and the cinematics: what the original did (archaeology, 2026-09-04, session 10)

Build 38.92 SHIPPED a working wrist HUD. It was not broken; 41.0 deleted it as collateral when
the DXVK fork was dropped, because that fork was what separated the HUD pixels from the frame
(`ad83c466` removed the fork tree at 18:26, `cc2fa936` the proxy's side-by-side pipeline and
`src/core/gfx/hud_panel.cpp` ten minutes later). This section is the record of how it behaved,
mined out of this repository's own history, so that a port onto the native D3D9 render can be
JUDGED against it rather than re-derived by guess. Every claim below carries the ref it was read
from; nothing here needs fetching. The fork survives under the tags `dxvk-base`,
`dxvk-m8.2-shipped`, `dxvk-m8.4`, `dxvk-shipped`.

### 1. The producer: what made a draw "HUD"

The classification lived in the fork, at exactly two call sites in
`dxvk-m8.2-shipped:dxvk/src/d3d9/d3d9_device.cpp`: `D3D9DeviceEx::DrawPrimitiveUP` (line 5083)
and `D3D9DeviceEx::DrawIndexedPrimitiveUP` (line 5505). **`DrawPrimitive` and
`DrawIndexedPrimitive` were never candidates**: they run a different verdict ladder
(`kDpSplice`/`kSplice`, lines 4095-4111 and 4364-4381) with the extra terms `no-vp`,
`2tri-nodepth`, `ortho`, `mirrored-vp`, and no HUD branch at all. Dishonored draws its UI
through the user-pointer paths, which is why only those two mattered. **The redirect also ran
only in the fork's side-by-side stereo mode** (`!dxvk_vr_seq`); in sequential mode the branch is
deliberately absent, with the comment at line 5066 saying that with no split there is no second
half to copy the HUD into, so the draw falls through and happens once, as the game intended.

The ladder itself (lines 4815-4826, duplicated at 5243-5256). `kUpDup` is the fall-through: a
draw is HUD if and only if it survives every reject.

| term | exact condition | what it tested, and why |
|---|---|---|
| `no-stereo` | `!stArmed` | the `dxvk_stereo.txt` marker file is absent: the whole fork behaviour is dormant |
| `in-dup` | `stInDup` | re-entrancy. The fork replays draws itself (dup, splice, redirect); the inner replay must render plainly, so the redirect sets `stInDup` around its own call |
| `recording` | `ShouldRecord()` | a D3D9 state block is being recorded; replaying a draw would corrupt the block |
| `no-rt` | `GetCommonTexture(renderTargets[0]) == nullptr` | no bound RT0 to reason about |
| `vp!=rt` | `rt0.Width != vp.Width \|\| rt0.Height != vp.Height` | the viewport must cover the whole target: excludes tiled and partial passes |
| `vp-offset` | `vp.X != 0 \|\| vp.Y != 0` | an offset viewport is a sub-region pass, not full-frame UI |
| `rt-small` | `vp.Width < 1024` | **the threshold is 1024 px**. Excludes shadow maps, bloom and LUT tiles, the 800x800 light tile |
| `rt-portrait` | `!(vp.Width > vp.Height)` | the SBS frame is always landscape; a square or portrait target is a helper buffer |
| `SPLICE` | `upWorldFx` (below) | world-space effects take the per-eye splice, never the wrist |
| `samples-rt` | texture stage 0 has `D3DUSAGE_RENDERTARGET` | **the measured M3.5 gate.** Framemap run 3: 208 full-size UP draws sampled plain textures (fonts and shapes, i.e. UI), 80 sampled the full-size render target (scene blits, bloom, tonemap). Duplicating the blits recursively squeezed the SBS pair and shredded the frame in M3.2/M3.3, so "tex0 is not a render target" is the actual UI discriminator |

`upWorldFx` (line 4808-4814) is `ZENABLE != D3DZB_FALSE` and (alpha blending on, or it samples
the RT, or the shadow fix is armed with `ZWRITEENABLE` false) and the vertex declaration does NOT
carry `HasPositionT` and a view-projection is known and it is perspective
(`|vp[3]|+|vp[7]|+|vp[11]| > 0.5` with a positive 3x3 determinant). The comment records the
measurement behind it: screen content (HUD, blits, bloom, tonemap) is `zen=0 / zf=ALWAYS` and is
untouched by the test, while UE3 particle sprites are stride 64 quads at `zen=1 / zf=LESSEQUAL /
zw=0`.

**So, in one sentence: HUD = a full-viewport, full-size, landscape, at least 1024-wide,
pre-transformed, non-depth-tested user-pointer draw that samples a plain (non render target)
texture.** Fonts and shapes.

### 2. The producer: the redirect, the render target, the clear

The redirect at line 5083, with its exclusion list and census in front of it:

```
if (!dxvk_vr_seq && dxvk_vr_hudwrist && upWhy == kUpDup) {
  const uint32_t hudPs = m_state.pixelShader ? VrPsFnvOf(m_state.pixelShader.ptr()) : 0;
  if (!VrHudSkipPs(hudPs)) {
    VrHudCensus(hudPs, PrimitiveCount);
    stInDup = true;
    const bool hudDid = VrHudRedirect(this, upSaved, [&]{ DrawPrimitiveUP(...); });
    stInDup = false;
    if (hudDid) { SetViewport(&upSaved); stSpliceAccum += 1; return D3D_OK; }
  }
}
```

Four things in there are load-bearing and every one of them is a rule for the port:

1. **`SetViewport(&upSaved)` after the redirect**, because `SetRenderTarget` resets the viewport
   to the full render target.
2. **`stSpliceAccum += 1` on every redirected draw.** The proxy's "is this gameplay" heuristic
   read the fork's splice counter and treated fewer than 8 as a flat frame. Without this, taking
   the HUD out of the frame would have made gameplay frames look like menus. Whatever replaces
   the splice counter, a redirect must still feed the gameplay heuristic.
3. The redirect sits BEFORE the both-eyes duplication block, so a redirected draw is never also
   duplicated.
4. The re-entrancy guard is the reason the inner draw lands on the HUD surface plainly.

`VrHudRedirect` (lines 772-800) creates the target lazily, once, guarded against re-entry
(`CreateTexture` can re-enter):

```
dev->CreateTexture(upSaved.Width, upSaved.Height, 1, D3DUSAGE_RENDERTARGET,
                   D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &t, nullptr);
```

Size = the viewport of the first redirected draw, i.e. the WHOLE side-by-side frame, both eye
halves wide, not a small panel buffer. One mip, `POOL_DEFAULT`. On failure it logs and
self-disables by writing `dxvk_vr_hudwrist = 0` (fail soft). `VrHudRelease()` is called from
`D3D9DeviceEx::Reset` (line 1296, "POOL_DEFAULT HUD RT must not survive a Reset"), nulling the
export first, then releasing surface then texture.

**The export is a raw COM pointer, not a shared handle**, because the proxy chain-loads the fork
in the same process:

```
extern "C" __declspec(dllexport) volatile uint32_t     dxvk_vr_hudwrist = 0;
extern "C" __declspec(dllexport) IDirect3DTexture9* volatile dxvk_vr_hudtex = nullptr;
```

The proxy resolved both by name and logged `hud: wrist exports resolved` or `NOT FOUND (fork
older than M6.0)`.

**The clear rule has three eras and the third one is the answer.**

1. M6.0 (`c22e63b0`): lazy, inside `VrHudRedirect`, keyed on a frame counter incremented at the
   end of `PresentEx`. Cleared to transparent black, because the proxy composited additively, so
   black is invisible.
2. M8.4 (`9748e9e3`): the staleness bug. Dishonored's HUD is minimalist and hides elements when
   they are full, so after dropping a carried body there can be ZERO UI draws for minutes: no
   draw means no lazy clear, so the carry icon's last pixels sit on the wrist forever (the
   tester: they are not going away even after dropping items). Same after a load. M8.4 cleared
   the HUD target at `PresentEx` when no redirected draw had arrived.
3. **Reverted (`0f3dd7d6`)**, because the render-target churn inside the fork's present path was
   a suspect for the swapchain "chain artifact", and the shipped 38.92 proxy targeted exact M8.2.
   The cure moved to the PROXY as 38.55: a plain `ColorFill` of the fork's HUD target from our
   own thread, immediately AFTER reading it. Same staleness cure, no state churn in the present
   path. **The port takes the 38.55 form: clear after copy, every present, unconditionally.**

**`hudskip`** (M6.1, `7e64dbd3`) is the exclusion list, keyed on the pixel shader's FNV-1a hash
of its DXBC bytecode, hex, comma separated, at most 8 entries, parsed from `dxvk_stereo.txt`
alongside `sep=` and `conv=`. A listed shader draws in view as before instead of moving to the
wrist: the lever for world-anchored UI (quest markers, imposters) tunable without a rebuild.
Note the M4.5 hazard recorded at the parse site: NO logging inside that static-init lambda, an
earlier `Logger::info` there stopped the DLL loading entirely. After M7.3 the hash is stamped on
the shader object at `CreatePixelShader` rather than kept in a bounded map (the earlier maps
overflowed at 2048 and 8192 entries and silently disabled fixes for late-created shaders).

**The census** (same commit) logs each distinct pixel shader that passes through the redirect
once per launch, `%08x` plus its primitive count, capped at 32 distinct shaders, running after
the skip test so skipped shaders are not censused. A wrongly captured element is identified by
hash from the log and excluded with `hudskip=`, no rebuild. The related bisector is
`dxvk_vr_killmask` bit 2 ("DUP class"), which kills every UI-class draw outright to prove a
class is responsible for an artifact.

### 3. The consumer: the gate, the readback, the panel

**The gate**, evaluated once per proxy Present (`4bba213e^:src/core/gfx/present.cpp:662-698`):

```
dlgNow = (g_dlgHudOff && MaimNowMs() < g_dlgUntilMs);
want = (g_wristHud && g_handMesh && CylTruthLive() && !dlgNow &&
        !g_menuOpen && !g_inMenu && !g_sbsMonoNow && !g_wheelHeld) ? 1 : 0;
```

Every clause is a scar:

- `g_handMesh` (35.6): the hands must actually be driving. At the main menu the attract scene
  keeps gameplay dispatches flowing and splices high, and the redirect swept the MAIN MENU onto
  the wrist. The author's phrasing: the sword follows your hand means you are playing.
- `CylTruthLive()` (38.46): a possessed pawn's collision cylinder was read within the last 3 s.
- `!g_sbsMonoNow`: the fork's per-frame splice counter under 8 means a mono frame (menus,
  loading, video).
- `!g_wheelHeld` (34.7): the power wheel is also a `kUpDup` draw and got redirected right off
  the screen; the redirect pauses while it is held.

**The readback** (`4bba213e^:src/core/gfx/present.cpp:378-431`), inside the same D3D9-side
capture as the main frame: fork RT surface -> `StretchRect` (GPU downscale, LINEAR) into a
`HUDRB_W x HUDRB_H` = **1008x568** `X8R8G8B8` `POOL_DEFAULT` target -> `GetRenderTargetData` into
a SYSTEMMEM surface -> row `memcpy` into `g_hudPix` -> **`ColorFill` of the fork's surface (the
38.55 clear)**. 1008x568x4 is about 2.3 MB per frame against the 36 MB main capture, called
noise at the time. Note the downscale is of the WHOLE side-by-side frame (aspect 1.775), not of
one eye. Both `POOL_DEFAULT` surfaces are released in the Reset hook: the 38.63 lesson is that
an unreleased default-pool resource makes Reset fail forever, i.e. a permanent black screen.

**The D3D11 side** (`cc2fa936^:src/core/gfx/hud_panel.cpp:7-48`) lazily built a
`DXGI_FORMAT_B8G8R8A8_UNORM` texture at the readback size (byte order matching the D3D9
`X8R8G8B8`, so the bytes upload straight through), a 4-vertex dynamic buffer, and **the additive
blend state**: `SrcBlend = ONE, DestBlend = ONE, BlendOp = ADD`, alpha the same. The panel's own
gate (38.50) simply requires the redirect verdict, because when the two disagreed the wrist
showed the fork target's FROZEN last frame.

**The placement math** (`hud_panel.cpp:53-84`), in head space, x right, y up, -z forward,
corners in the order TL, TR, BL, BR:

```
d  = controllerPos - headPos                    // world
pr = R_head^T * d                               // controller in head axes
pr.y += g_hudPanelUp                            // float it above the controller
if (pr.z > -0.06) return false;                 // behind or at the face: hide
n = normalize(-pr)                              // billboard: face the head exactly
if (|n| < 0.05) return false;                   // hand at the eye
rgt = worldUp x n
if (|rgt| < 0.2) return false;                  // within ~11.5 deg of vertical: hide
upP = normalize(n x rgt)
hw = PanelSize * 0.5 ; hh = hw * (568/1008)
corners = pr -+ rgt*hw +- upP*hh
```

Three properties worth carrying: the panel takes only the controller's POSITION, never its
orientation; it keeps world-up as its up axis so it never rolls; and the lift is a pure head
space +Y, not along the controller's own axis. The three rejects are the behind-the-face guard
(6 cm in front of the head plane), the degenerate distance (5 cm), and the overhead guard (the
cross product's magnitude is the sine of the angle to vertical, so 0.2 is about 11.5 deg, where
`rgt` is numerically garbage and the billboard would spin).

**The draw** (`hud_panel.cpp:86-124`) is per eye, from the eye loop after the game-image quad and
after the hands, before the ImGui overlay. Clip space is built by hand from the cached per-eye
frustum with `w = -z` and `z = 0.5w` so it always lands mid-depth, depth test OFF, additive
blend, the shared quad shader with the HUD's shader resource view. **It drew only in
projection-layer mode**: on a flat quad layer the comment says the hands and the wrist panel are
eye-frustum projected 3D content and would be geometrically wrong.

Why additive: the fork clears the target to black each frame, so black is invisible and the HUD
elements float like a hologram, with no alpha correctness needed.

Ancillary consumers: `dump hud` wrote the D3D11 texture as PNG; `[Screen] MirrorHud=1` (38.64)
stamped the already-downscaled D3D9 surface into the desktop mirror's bottom left at one fifth
height, because the panel is drawn into the HEADSET's eye textures and was never in the game
backbuffer at all.

### 4. The dialogue rule

`cc2fa936^:src/game/dishonored/ue3/process_event.cpp:245-284`. A substring match on
`PlayerChoice` over the whole event family (never a guessed exact-name list, the 34.4 lesson),
with two hard-won exclusions:

- **38.47**: the measured members are `Dis_PlayerChoice_RequestSkip`, `..._Released`
  (DishonoredPlayerInput) and `OnPlayerChoiceConfirm` (DisGFxMoviePlayerHUD). There is NO
  open/close event anywhere in the vocabulary.
- **38.48**: a fixed 6 s hold snapped back mid-conversation, because the events are momentary
  ACTIONS, not a state: one exchange fired `RequestSkip` and `OnPlayerChoiceConfirm` SEVENTEEN
  seconds apart. So the window is inferred from the pair: a skip means dialogue is running (hold
  `DialogHoldMs`, 12 s, re-armed by every member), a confirm means the choice is taken and the UI
  is closing (release after a 1500 ms grace, and only ever SHORTENING the window).
- **38.51**: `Dis_PlayerChoice_RequestSkip` fired in the same millisecond as `Dis_VersusAlt`. It
  is an INPUT ALIAS: pressing block in combat fires the skip name, so every block press killed
  the wrist HUD for the hold duration. Guard: a `RequestSkip` within 150 ms of any Versus event
  is the combat alias and is ignored.
- **38.53**: `..._RequestSkip_Released`, the release edge of the block key, fires alone hundreds
  of ms later and escapes the 150 ms guard. It is input-tail noise; only the guarded press arms.

The tombstone is at `src/game/dishonored/ue3/process_event.cpp:318-319` today.

### 5. The settings, and the shipped values

| key | default | clamp | note |
|---|---|---|---|
| `[Hud] WristHud` | 1 | | master switch, live F10 checkbox (34.8) |
| `[Hud] DialogHudOff` | 1 | | park the panel during conversations (38.47) |
| `[Hud] DialogHoldMs` | 12000 | 1000-120000 | |
| `[Hud] PanelHand` | 0 | 0 left, else right | |
| `[Hud] PanelSize` | 0.16 m | 0.05-0.60 (slider 0.06-0.40) | **shipped tuned: 0.22** |
| `[Hud] PanelUp` | 0.06 m | | head-space +Y lift |
| `[Screen] MirrorHud` | 0 | | desktop stream inset |
| `[Mode] ForceTheater` | 0 | | not a HUD feature, see below |

`tests/golden/f10-tuned-2026-09-02.ini` lines 469-473 carry the tester's own annotation: after a
full F10 tuning session the ONLY HUD change requested was a bigger panel, 0.22 m instead of 0.16.
Hand, lift and the dialogue keys were left at their defaults.

**Theater mode** was the stage-1 fallback presentation, not a HUD feature: `[Mode] ForceTheater`
selected the OpenVR overlay application type instead of scene, created a 4 m wide overlay with
0.12 curvature at (0, 1.6, -2.4) in the standing universe, and uploaded the whole captured frame
to it once per present. No eye loop, no head-driven camera, no hands, no wrist panel, no
reticle. It went with OpenVR in 41.0; the runtime layer's own cinematic quad fallback is its
replacement.

### 6. The known bug (HANDOFF-GINGASVR 8.4)

> Main menu on the wrist. Going from gameplay to the main menu, the menu gets swept onto the
> wrist panel and becomes unusable. The gate requires `CylTruthLive()` ("a possessed pawn cannot
> exist at the main menu"), but the pawn survives the transition long enough. Inherited, not a
> regression.

Read against the gate: `CylTruthLive()` is a 3 s latch on the last successful pawn cylinder read,
so the gameplay-to-main-menu transition leaves it true for up to 3 s after the menu appears, and
`g_menuOpen`/`g_inMenu` miss the MAIN menu in windowed mode (script events are the only signal
there). The 38.46 fix closed the idle-at-main-menu hole, not the transition. **A port must either
invalidate the pawn latch on a level or menu transition, or use a positive main-menu signal
rather than the pawn's absence.** Our `DvrGameplayVerdict()` already carries `!g_mainMenu` as its
own term, which is that positive signal.

### 7. The cinematics: two bodies of prior art that must not be confused

**(A) The mod's own latch, live today.** `g_cineNow` is a PARITY TOGGLE flipped by the exact
script event `OnToggleCinematicMode` (`process_event.cpp:320-326`), which is why it needs five
separate clears: a new pawn (`process_event.cpp:24-27`, a level load inherited CINEMATIC in run
22), leaving the main menu (`:237`, the title screen's attract scene flips it ON with the
level's pawn already latched), a loading screen (`commands.cpp:309`), a failed intro skip
(`console.cpp:102-106`), no live pawn and an 8 minute expiry (both inside the reader,
`game_state.cpp:5-22`). The state machine's priority is NO_PAWN > MENU > LOADING > CINEMATIC >
GAMEPLAY, so CINEMATIC is the last discriminator before GAMEPLAY.

Its readers: the state line and status.json; **the gameplay verdict**
(`present_tick.cpp:143`, `pawn && !menuOpen && !inMenu && !mainMenu && !cineNow && viewLive`),
which is the ONLY bridge from the latch to the runtime layer; the head path's direct-fallback
hold-off (`head_track.cpp:364`, reason "cinematic announced") - note this holds only the
take-the-camera-back-by-force path, the script lane's `ProcessViewRotation` write keeps running
and the matinee camera simply ignores it; the pad park (`pad_bridge.cpp:267`: every button but
START dropped, sticks and triggers zeroed, so a stray A or a chair-shuffle B can never eject the
player from a scripted sequence, and menus take precedence so menu navigation is never eaten);
and the room-scale walk gate. Note the asymmetry: the verdict and the state line read the raw
`g_cineNow`, the pad and head read the self-healing `CineActive()`.

**(B) The BioShock runtime layer's subsystem, present but starved.** Almost every input comes
from `dvr::hud::*`, which is `src/core/vr/hud_stub.cpp` answering "nothing". The decisions:

| decision | site | live here? |
|---|---|---|
| `wantCine = stale \|\| !strict` | `openxr_runtime.cpp:3609-3638` | **LIVE**: `strict` is our gameplay verdict, `stale` is a publish older than `kCineStaleMs` 300 ms; 3-present hysteresis, enter/exit/presents counters, the F10 readout at 4583 |
| `screenOnly` leg | `:3615` | DEAD (`hud::screen_only()` is always false) |
| `fovMm && !stereoCine` leg | `:3614` | DEAD (`hud::fov_mismatch()` is always false) |
| the claim-substitution branch | `:3639-3670` | UNREACHABLE (guarded by `fovMm`); its comment documents a BioShock bathysphere bug |
| the quad head-lock override | `:4101` | DEAD (`screen_only`) |
| **the HUD quad** | `:4169-4219` | DEAD: no provider is registered and `hud::texture()` is null |
| bars / effects / postfx / subs / restorert / dumparm | `:4450-4521`, `:5139-5195` | round-trip only; the `vrcine` word is not registered, so all 20+ seam words including the live `on`/`off`/`mode` are unreachable |
| `CineDrive` (off / authored / authored+look) | `:5113-5137` | `cine_drive()` has ZERO callers: entirely BioShock prior art. Our pad park plus the head hold-off are the game-side equivalent |
| `set_gate(srFrame)` | `:3687` | the signal is computed correctly (projection mode AND an eye tag popped) and THROWN AWAY by the stub |

`unsqueeze` is retired with a useful engine fact on the tombstone (`:5197-5204`): BioShock's
cinematic bars are a gameswf draw painted OVER a full-frame tonemap, not an anamorphic squeeze,
so stretching a band would crop real picture. Whether Dishonored's bars are a Scaleform draw is
an open measurement, and if they are, they fall out of the same classifier as the HUD and need a
policy.

**Subtitles**: the runtime layer's documented choice is the PANEL, and its reason is
stereo-correctness (`:4505-4512`). Off (default) means subtitles ride the head-locked quad, one
image in both eyes; on means they render into the frame, where each eye is captured from a
DIFFERENT game frame and the text can double. For Dishonored this is currently dead in the bad
direction: with no panel to ride, subtitles land in the frame regardless of the setting.

### 8. The HUD quad's texture contract (what a provider must satisfy)

```
using HudTextureProviderFn = ID3D11Texture2D* (*)(ID3D11DeviceContext* ctx);
void set_hud_texture_provider(HudTextureProviderFn fn);   // openxr_runtime.h:579
```

- Submitted only when a base layer exists AND `projectionMode` is true AND the view space is
  valid. **The panel is projection-only**: under the mono screen, or while the cinematic quad
  holds, no HUD quad is submitted.
- The texture must live on the runtime's own D3D11 device (the one the mod provides through
  `set_device_provider`, created on the LUID OpenXR named), because the copy is a
  `CopyResource`, not a shader blit: **format and dimensions must match the swapchain exactly**,
  and the swapchain is created in `g_swapFormat`, the format `create_swapchains` already picked
  for the eyes (the R8G8B8A8 family).
- **Premultiplied alpha.** The layer sets `BLEND_TEXTURE_SOURCE_ALPHA_BIT` WITHOUT the
  unpremultiplied bit; a straight-alpha capture will fringe.
- Stable dimensions: any change rebuilds the swapchain that present.
- The consumer never releases the returned texture, and a null RESULT is legal and means "no HUD
  this frame".
- Placement: head-locked, no rotation ever, position `(0, g_hudUpM, -g_hudDistM)` =
  `(0, -0.10, -1.30)` m, width `g_hudWidthM` 1.25 m, height from the texture's aspect, with F10
  sliders (0.5-3.0, 0.3-3.0, -1.0-1.0).

**Head-locked is not wrist-locked.** The runtime's quad uses view space and the project's rule is
to keep that file as close to the BioShock copy as the D3D9 host allows, so the head-locked
panel ships and is judged first; a controller-anchored panel is a second change and section 3's
billboard math is its reference.

### 9. Two stale doc lines in the adopted layer (noted, not edited)

The runtime file stays verbatim, so these are recorded here instead of fixed in place:

- `openxr_runtime.h:389` calls the stereo cinematic mode an opt-in experiment defaulting to
  quad; the code initialises `g_cineStereo{true}` at `openxr_runtime.cpp:272` with a comment
  saying stereo is the default per the user's call of 2026-07-29. The CODE default is stereo.
  (Moot while the branch is unreachable, but it will mislead a reader.)
- `openxr_runtime.cpp:174` and `openxr_runtime.h:570-571` say the HUD quad sliders persist via
  the overlay's ini save. In this repo nothing persists them: there are no `[Hud]` ini keys
  (41.0 removed them) and `set_hud_quad`/`get_hud_quad` have no callers.

## The Scaleform HUD draw class, measured (2026-09-04, session 10)

**The answer is yes, and by a simpler discriminator than the fork needed: the render target.**
Dishonored draws its world into an OFFSCREEN scene target the size of the render and paints the
whole HUD onto the BACKBUFFER at the tail of the frame. The scene is resolved to the backbuffer
with `StretchRect`, not a draw. So "this draw goes to the backbuffer" separates HUD from world
with no overlap at all, where the fork had to run a nine-term ladder because on its frame the two
shared one target.

The instrument is `core/gfx/draw_census` (`[Draws] Census=0`, `draws on|off|status|kill|unkill`),
which buckets every draw by entry point, target, viewport, depth, blend, what texture stage 0 is,
the pixel shader's bytecode hash, the vertex declaration and a primitive-count band, and prints a
table and a VERDICT every 3 s. Runs 46-01 and 46-02, the sewers, `stereo reentry`, 2496x2688, on
the simulator lane.

### What a present is made of

| | |
|---|---|
| draws per present | 1205 |
| by entry point | DrawPrimitive 0, DrawIndexedPrimitive 382, DrawPrimitiveUP 13, DrawIndexedPrimitiveUP 810 |
| distinct buckets | 133 (no overflow) |
| distinct pixel shaders | 81 |
| state blocks created | 1 in a whole run |
| **draws to the backbuffer** | **5 buckets, 15.0 per present, at ordinals 1177..1221** |
| the same in the pause menu | 10 buckets, 95.9 per present, at ordinals 1126..1223 |
| tonemap draws | **0** (the resolve is a `StretchRect`) |
| PostRender dispatches | 6.0 per viewport draw pass |

Every backbuffer bucket, in gameplay and in the menu, carries a **full-viewport draw with
`D3DRS_ZENABLE == D3DZB_FALSE`**. Nothing else in the frame goes there.

### Proven by picture, not by counter

`draws kill <key>` drops a bucket's draws so the frame says what the bucket was (the project's
rule: identify a render pass by making it MOVE). Four per-eye simulator captures, head still:

| capture | killed | result |
|---|---|---|
| `kill-a-baseline` | nothing | the sewers with the health and blood indicator top left |
| `kill-b-2065fa4d` | the 11-per-present untextured blend bucket | **the indicator is gone, the world pixel-identical** |
| `kill-c-57e5c97d` | the 1-per-present textured bucket | the indicator loses its mask and draws unclipped; the world unchanged |
| `kill-d-allbb` | all five backbuffer buckets | **the HUD is gone; the world is untouched** |

No kill of a backbuffer bucket ever changed the world, and no world bucket was ever needed to draw
the HUD. That is the separation, in both directions.

### The three things the fork's ladder got right for its frame and wrong for ours

1. **Portrait targets.** The fork rejected them (`rt-portrait`) because its side-by-side frame was
   always landscape. Our per-eye render is 2496x2688, portrait. Porting that term would have
   rejected the entire frame. It is deliberately absent from `draw_census`.
2. **User-pointer draws only.** The fork only ever classified `DrawPrimitiveUP` and
   `DrawIndexedPrimitiveUP` because on Dishonored-through-DXVK that is what the UI used. On this
   path `DrawIndexedPrimitiveUP` carries **810 world draws per present**, so the entry point
   discriminates nothing and is a reported column, not a term.
3. **`samples-rt` (texture stage 0 is not a render target).** This was the fork's measured UI
   discriminator and it is sound, but it splits the HUD rather than separating it: most of
   Dishonored's HUD draws are untextured Scaleform fills, and requiring a texture kept 1 draw per
   present of 15 and left the other 14 in the frame. Reported, not required.

The first census build carried a fourth term of its own, "after the tonemap", and it rejected
every draw in the frame for a whole run because this engine's resolve is not a draw. The
instrument said so on its own line (`tonemap draws/present=0.0`, and the near-miss list naming
`afterTonemap` as the only failing term), which is what an instrument that can fail its own
hypothesis is for.

### What this means for a redirect

- **The rule is the target, not a bucket list.** A fifth capture showed one faint element surviving
  a kill of all five measured keys: the bucket set drifts with what is on screen. A redirect must
  key on the rule (backbuffer, full viewport, depth off), never on a list of keys.
- **The menu is drawn by the same class.** A redirect gated only on the draw would sweep the pause
  menu and the main menu onto the panel, which is exactly the original's inherited bug (HANDOFF
  8.4). It must be gated on the GAME STATE, and `DvrGameplayVerdict()` already carries the
  positive signal the original lacked (`!g_mainMenu && !g_menuOpen && !g_inMenu`).
- **The cost is small.** 15 draws per present means about 30 render-target switches per present,
  against 1205 draws; the fork paid about 200.
- **The census is sound to keep unlocked.** The draws arrive on the presenting thread (the census
  checks this every present and refuses if it ever stops being true), and one state block in a
  whole run means Scaleform is not restoring device state behind the setters, so no `Apply` hook
  is needed.

The constants and their derivation are in `src/game/dishonored/patterns.h`,
"The Scaleform HUD draw class".

## The HUD panel: the redirect, and the draw that nearly ruined it (2026-09-04, session 10)

The class measured above is redirected into a private A8R8G8B8 target the backbuffer's size,
copied at Present into a shared surface, opened on the mod's D3D11 device, alpha-repaired and
handed to the runtime layer's head-locked HUD quad through `set_hud_texture_provider`
(`core/gfx/hud_capture`, `[Hud] Panel=0`, `hud on|off|status|scale <f>`, the F10 Display tickbox).

### The scene resolve is a draw, and it is the fourth term

The first build of the redirect used the three measured terms - the backbuffer, a full viewport,
depth off - and the panel came up holding **the whole game frame**, world and all, while the HUD
also stayed in the eyes. The instrument that settled it in one step was `dump hud`, which writes
the panel's own texture: it showed the frame. Then `draws kill hud` with the panel on emptied the
panel completely. So the content was not stale memory and not a bad copy: **one of the fifteen
redirected draws was painting the world.**

It is the scene resolve. Dishonored copies its offscreen scene target to the backbuffer with a
full-screen textured quad of two primitives, opaque, depth off, full viewport - which satisfies
every term the rule had. The discriminator is **alpha blending**, and it is principled rather
than convenient: something drawn ONTO a finished frame must blend to sit over it, while the frame
itself is written opaquely. The resolve is the only opaque draw in the backbuffer population and
every HUD element blends. The rule became four terms and the redirect fell from 15 draws per
present to 14.

This also explains the census's earlier `tonemap draws/present=0.0`: its detector keyed on the
fork's `samples-rt` term, and this game's resolve samples a plain texture copy, not a surface
flagged `D3DUSAGE_RENDERTARGET`. The census now detects the resolve as "the opaque full-frame
draw to the backbuffer" and says so, and `tex0_class` answers UNKNOWN rather than a guessed
"plain" when its cache is full.

### What the panel does

| | |
|---|---|
| the private target | A8R8G8B8 at the backbuffer's size and multisample type, so its depth-stencil stays legal |
| per redirected draw | `GetRenderTarget`-free: the game's target comes from the classifier's shadow, `SetRenderTarget` then `SetViewport` (the bind resets it, and a PURE device has no `GetViewport`), the draw, then both back |
| the clear | after the copy, EVERY present, unconditionally - the fork cleared lazily at the first redirected draw and a dropped body's icon sat on the wrist for minutes |
| the hand-off | two shared slots, fenced in both directions, delivering the previous slot; `BlitQuad` with alpha = max(r,g,b) into R8G8B8A8 |
| the gate | the runtime's own (a projection present carrying an eye tag) AND the game side's strict gameplay verdict AND no power wheel |

Two mistakes worth keeping written down. A D3D11 event query does not complete until the context
is flushed, so the read fence spun and timed out on every present until the flush was added
(`read waits 31767 timeouts 1033` -> `0 and 0`). And a 10 ms budget measured with `GetTickCount`
expires on the first read, because that clock's tick is about 15 ms: both waits use the
performance counter now, like the eye path's.

### Verified (simulator, run 46-05, the sewers, `stereo reentry`, 2496x2688)

- `hud on`: `redirect presents=540 redirected=14.0 draws/present delivered=540`, fences 0
  timeouts, 0 restore failures; `xr: HUD quad swapchain ready (1248x1344, 3 images)`,
  `xr: HUD quad live (1248x1344, 1.25 m wide at 1.30 m)`.
- `dump hud`: the panel texture holds the health indicator and the reticle dot and NOTHING else;
  everything around them is transparent.
- The per-eye compositor capture: the world is intact with **no HUD in it**, and the HUD is on the
  head-locked quad, in the right colour.
- `reentry.xrs` 11/11, and `perf: tick 11.1 ms (90/s)` - pace-bound, the same as without the
  panel. About 28 extra `SetRenderTarget` calls per present against a frame of 1205 draws.

### The pause menu, measured (run 47-02, 47-03)

Two facts, both against expectation, both from the log rather than reasoning:

1. **A paused menu drops the gameplay verdict on `viewLive`, not on the menu flags.** The pause
   silences `ProcessViewRotation`, and the view-pipeline term read the silence as a starved
   pipeline (`gameplay verdict: FALSE (view pipeline silent ...) menuOpen=1 viewLive=0`). Removing
   the menu term alone changed nothing. Under `[Hud] MenuOnPanel` an in-game menu now stands in
   for the view term; a loading screen has no menu flag up, so it still drops.
2. **The camera upload keeps flowing while the game is paused, so stereo keeps working.** The
   expectation was that a frozen camera would fail the re-entry's "camera silent" gate and the
   eyes would fall to mono. They did not: with the pause menu open the beat read `out/s=120 L/s=60
   R/s=60 mono/s=0`, the doubling ran and both eyes were tagged. The paused world is a live stereo
   pair, not a held one, and the compositor keeps it world-locked while the head turns. The menu
   (93 draws per present, the whole class) rode the panel; the game's own pause blur is a
   post-process on the scene target and stayed in the world, which is where it belongs.

The main menu is untouched by any of this: it has no live pawn and carries its own positive
signal (`g_mainMenu`), so it keeps the screen.

### An instrument caveat found on the way

`DumpTexturePng` (`dump eyes`, `dump hud`) writes R8G8B8A8 textures with red and blue swapped:
the eye dumps come out olive where the compositor shows teal, and the panel's red indicator dumps
blue. The COMPOSITE is correct, so this is the dump's channel choice and not the render path. It
predates this session (`dump eyes` shipped in session 9) and it has never been load-bearing,
because those dumps are read for geometry and for left-against-right differences rather than for
colour. Worth fixing; do not read a colour verdict off a dump until it is.


## How the Scaleform HUD identifies its elements (VR-118, VR-120, 2026-09-15)

Measured on the simulator (Debug build of `claude/vr-120-hud-elements`, the sewer level
opened through the console, 2750x2850, `stereo reentry`, `draws on` + `hud regions on`).

**The 2D transform is in the vertex shader, and the register it lives in is read from the
shader itself.** Every HUD-class draw on this build binds a `vs_3_0` shader (none runs on the
fixed-function path: 0 SetTransform calls per present with the slot 44 hook counting, every
probed draw with a shader bound). The four HUD shaders seen so far (the gameplay fills, the
gameplay textured draws, the menus' SHORT2 draws, the menus' FLOAT2 user-pointer draws) all
compute the position output as

    o = c[K+0]*v.x + c[K+1]*v.y + c[K+2]*v.z + c[K+3]*v.w      with K = 6

a `float4x4 Transform` held as four COLUMNS (the compiler's parameter table names it), with
the textured variants adding a `TextureMatrix` at c10..c13. Nothing uploads c0..c3 between
HUD draws, so VR-117's c0..c3 shadow read whatever the last non-HUD shader had left (the same
values on every HUD draw and every present: the signature of a stale read, not of a
transform), and its rectangles were nonsense. A SHORT2 or FLOAT2 position expands to
`(x, y, 0, 1)`, so the screen position is `(o.x/o.w, o.y/o.w)` from the x, y and w columns.

How the mod reads it, per shader, at its first HUD-class draw (`core/gfx/hud_class.cpp`,
`vs_xform_for`): `GetFunction` fetches the bytecode, `D3DDisassemble` from
`d3dcompiler_47.dll` (which reads D3D9 bytecode) disassembles it, and a parse finds the
instruction that writes the declared position output (`dcl_position oN`, or `oPos`): its
`c#` operand paired with the input's `.w` swizzle is the w column and its `r#` operand is the
sum; the instructions BEFORE it that write that sum register name the x, y and z columns
(the texgen after it reuses `r0` with other constants, so the order matters). The register
numbers are never hard-coded (the rule from the view-model's vertex path holds). A shader
that does not parse gets no rectangle and says so once (`draws/xform: ... could not be
read`); the constant shadow (`frame_hooks.cpp`) now covers c0..c31 and a row outside it
returns null, never another row. `draws vsdump` writes each shader's disassembly under
`<data_dir>\dumps` (game-derived: never committed) and logs its register lines.

Result: 9324 probes per 3 s, 0 refused, 1.0 us per probe (0.6 us before the parse; the
D3DDisassemble runs once per shader). Every rectangle inside [0,1]; the health bar's bucket
reads top-left.

**The elements are the clusters of draw rectangles, not the buckets.** A bucket is a draw
CLASS (shader, declaration, state) and its rectangle union spans everything drawn with it;
the census now keys each probed draw by (bucket, rectangle quantised to 1/40 of the screen)
and `draws regions` prints the clusters by frequency (`draws/cluster:`). Clusters are not
collected while a screen rides (a screen routes by its UI owner context, and the pause
menu's text alone overflowed a 256-row table). What the sewer level showed (normalised
backbuffer, y down):

| what | rectangle | draws/present | seen |
|---|---|---|---|
| the vitals block (health and mana bars, their frames, the splatter) | union [-0.009,0.013 - 0.172,0.253]; the health fill [0.050,0.065 - 0.101,0.210], the mana fill [0.067,0.101 - 0.129,0.213], the frames [0.035,0.031 - 0.119,0.217] and [0.048,0.071 - 0.159,0.220], one background spanning both [0.015,0.074 - 0.148,0.220] | 20 | always |
| the reticle dot | [0.497,0.497 - 0.503,0.503]; [0.480,0.481 - 0.520,0.519] with an interactable focused | 1 | always |
| the interaction prompt (label right of the reticle) | [0.524,0.481 - 0.774,0.602]: a plate, a text run [0.538,0.493 - 0.636,0.514], a rule, two icons | 4 | an interactable focused |
| an objective marker | a 0.033 x 0.032 square wherever the target projects (0.182,0.401 here) | 1 | a target in view |
| the power wheel (context Wheel) | the ring [0.226,0.275 - 0.774,0.716] and its rings, the slot icons bottom-left [0.06,0.79 - 0.23,0.95] and bottom-right [0.71,0.85 - 0.95,0.96], labels right [0.84,0.36 - 1.0,0.50], a full-screen fill | 30 | the wheel held |
| the pause menu, the journal (contexts Pause, Journal) | hundreds of glyph-sized draws | 35, 40 | riding |

The health and mana bars interleave in x (fills centred 0.076 and 0.098, frames 0.077 and
0.103, a shared background centred 0.081), so a per-draw rectangle cannot separate them
without misrouting the shared draws: they are ONE element (`vitals`). Equipment icons,
subtitles, toasts, tutorials, detection arrows, the skip gauge and dark vision did not draw on
this level from the spot reachable on the simulator; their rows exist in the layout table
without a region and route to `default` until measured (`hud region <el> x0,y0,x1,y1` live).

The identity the layout routes on is therefore the PAIR (UI owner context, rectangle): the
context separates the riding screens from the gameplay HUD at no cost, the rectangle
separates the gameplay elements, `tex0` stays in the census as a tie-breaker. The movie
identity route (a hook on the Scaleform movie's Display) was not needed for this list and is
not built; the display-object route (instance names) is a ticket.

## The HUD's blend equation, and the alpha it leaves in a sink (VR-119, 2026-09-15)

Measured with `draws/blend` (the first HUD-class draw of each colour blend tuple): every
HUD-class draw on the sewer level runs `SRCBLEND=5 DESTBLEND=6 BLENDOP=1` (SRCALPHA /
INVSRCALPHA, add) on colour with `SEPARATEALPHABLENDENABLE=1` and `SRCBLENDALPHA=2
DESTBLENDALPHA=1` (ONE / ZERO) on alpha: the game REPLACES the target's alpha with each
draw's source alpha. Into a sink cleared to transparent black that leaves the LAST draw's
alpha per pixel, not the coverage, and a black stroke (alpha 1, colour 0) reads as nothing
to the `max(r,g,b)` repair. No additive (ONE/ONE) tuple was seen on this level. The redirect
in `captured` and `mix` modes forces `ONE / INVSRCALPHA, add` on alpha around each
redirected draw through the original SetRenderState (the shadow must not see the mod's own
writes) and restores the shadowed values after: dstA = srcA + dstA*(1-srcA), the "over"
coverage, with the colour equation untouched so the colour stays premultiplied. State
blocks would bypass the forcing: `g_stateBlocksCreated` reads 0 for a whole run.

## VR-125 CPU attribution correction (2026-09-15)

Pub-view thread IP sampling finds a bounded polling loop with PAUSE in the
NVIDIA D3D9 worker's hot region. High worker CPU therefore includes spinning;
it cannot all be described as useful submission work. The observed region share
changes with sampling order. Render-thread samples are consistently spread
across engine, native D3D9 and proxy. No engine address or hook derived.
See RESOLUTION_FLOOR.md for method, evidence, limits and next scoped measurement.

## VR-125 scoped rendering attribution (2026-09-15)

Same pub-view307 diagnostic: outside-Present engine rendering and draw hooks
account for87.95% of measured render-thread cycles. Original viewport calls on
the game thread total1.23ms wall. Counter scopes are on different threads and
overlap; no serial sum or threading speedup is implied. GetThreadTimes per-stage
CPU attribution failed the wall-time sanity check due coarse accounting; keep
QueryThreadCycleTime relative only. RESOLUTION_FLOOR.md has exact populations,
restore identity, off/on/off control and the next bounded investigation.


## VR-125: D3D9 query-read helper, 2026-09-15

Offline reference-tool derivation on the installed Steam executable. The ASCII
CreateQuery OCCLUSION error expression leads via its .text xref to creation of
a query with type9. Immediately adjacent is the shared native query-read helper
at VA009BCF50 (patterns.h kD3D9QueryRead). Its native query argument calls vtable
slot7/GetData, always flags1/D3DGETDATA_FLUSH; if S_FALSE and the fourth stack
argument permits waiting, it polls again until completion or its own timeout.
It returns a boolean, not HRESULT. Observed successful and false paths clean
16 stack bytes. ABI: thiscall ECX owner, query/data/size/wait on the stack.
The15-byte prologue signature is verified; only the first6 whole non-relative
instruction bytes are relocated into the diagnostic trampoline.

Existing disasm-rva.py calls census finds five direct callsites (RVAs):005BD16B,
005BD185,005BF545,005BF596,005C131A. The first two read8-byte data; the last is
reached from the cached occlusion-result path and reads4 bytes. The005BF596
caller passes wait=1 and can retry until returned query data is nonzero. These
are offline control-flow findings, not proof the callers are active or expensive
in the hub. No interpretation of query type is made from data size alone.

The diagnostic reads GetType only on the live query argument, takes no COM
reference, retains no engine-object identity and changes no query flags/results.
It times complete helper calls on the Present thread, classifies caller return
RVA/type/wait permission, and groups them by the completed render interval's eye.
That eye label is not the issuing/creation eye of a retained query. Other-thread
calls and table overflow are reported so missing coverage cannot look like zero
cost. Performance verdicts and all subsequent research belong only in
[PERFORMANCE.md](PERFORMANCE.md).

## 2026-09-14: performance measurement population

The rollout adds no engine address or memory writer. Strict DvrGameplayVerdict
gates the desktop trial; cinematic presentation permission is intentionally not
enough. Real fresh-pair sampling uses delivered capture serials after successful
XR wait/copy/release, separate eye swapchains and successful xrEndFrame. Held
submissions cannot become new pairs merely by arriving on an even Present.
See PERFORMANCE_ROLLOUT.md for limitations and the full optimization sequence.

## InitViews timing boundary, 2026-09-15

Offline derivation from the normal headset CPU capture in PERFORMANCE.md.
Sampled return RVA0046C0C1 follows a direct call at RVA0046C0BC to VA008662A0.
The callee references the executable's own ASCII and UTF-16 InitViews labels.
Its other direct caller is RVA0046A21E; the diagnostic groups return RVAs so
these callers cannot be conflated. This is the scene renderer's preparation
stage before the caller's four-pass loop. That loop calls VA0086BF00 and
VA00864290, with shipped World/Foreground/editor pass labels. Its pass index
is not the VR eye index. World simulation is outside this render-thread path.

InitViews has ECX receiver, no stack arguments, and a plain ret at VA00867598.
The entry realigns the stack. A 16-byte signature in patterns.h guards the
hook; its first six bytes comprise three whole non-relative instructions (push ebx,
mov ebx/esp, sub esp/8) are copied to the trampoline, which resumes before stack
alignment. The receiver is forwarded only during the original call; no engine
object identity is retained or dereferenced by the diagnostic. No UObject
memory write is introduced. Synthetic x86 tests reproduce the alignment and
verify receiver/return/stack preservation through repeated calls and removal.

Within InitViews, sampled return RVA004671D9 follows its conditional cdecl
one-argument call to VA00864AD0. It is the main sampled descendant, but its
precise visibility/occlusion/mesh-gather split remains unclassified. Do not
label every sample as occlusion cost or use the low query-read result to
exclude this surrounding work. No hook or bypass is added to that child.
Measurement results and subsequent decisions belong in PERFORMANCE.md.

## Frustum-culling and reflection selector, 2026-09-15

The dominant InitViews child VA00864AD0 references the executable's own
ProcessViewFrustumCulling label at VA0107EAC4 and ParsingOctree at VA0107EAE0.
At VA00864CCA it reads its sole stack argument (renderer), then the pointer at
renderer+0x60. The dword at that pointer+0x48 selects a nonzero branch labelled
ProcessPrimitiveCullingReflectionScene versus a zero branch labelled
ProcessPrimitiveCulling. These are exact engine branch names, not inferred
from runtime addresses or class names. The reflection path filters primitive
flags before invoking the culling helpers; ordinary path processes its lists
without that reflection filter. This does not prove which visible reflection
surface owns any invocation, or that its visibility data can be reused.

ABI is cdecl, one renderer argument on the stack; plain ret at VA00865012 and
caller cleanup at VA008671D9. Its stack-realignment prefix matches InitViews:
16 verified bytes, first6 relocated, resume before alignment. Both addresses,
signatures and the two selector offsets are in patterns.h.

The diagnostic reads this selector from the borrowed live call argument, with
null/access-exception classification as unknown. It retains only scalar class,
ordinal and timings after return. It rechecks the selector at nested culling
entry and reports changes. A stack-local invocation retains the receiver only
for the duration of the original call to match its child, then is discarded.
No UObject write, delayed dereference, COM reference or menu-retained identity
is introduced. Nested timing is a subset of InitViews time, never additive.

## Live resolution through the F11 viewport path (VR-50, 2026-09-15)

The earlier inert `setres` result is specific to the controller console route. F11's
native input at RVA5C8E7D..5C8F0A invokes WindowsViewport's primary vtable slot1.
Constructor RVA5C6810 assigns primary VA010C1870 at native+0 and FViewport's
VA010C17D8 at native+4; its secondary render-target interface is assigned at native+12.
The primary slot1 is VA009C5B30, ending in `ret24` at VA009C6096.

**Six stack arguments:** width, height, fullscreen, existing viewport option, windowX,
windowY. At F11 the window coordinates are pushed before two zero-argument virtual
queries; those queries do not consume the coordinates. Treating this as a four-argument
call would corrupt the stack. FViewport virtual slots+0x58/+0x5c read bits0/1 of its
flags at+0x5c. Window coordinates are native+0x4e4/+0x4e8, HWND native+0x68.
Resize forwards to the native window/resource-update path (VA009C4710), with the existing
engine synchronization and D3D reset lifecycle; the proxy does not call Reset itself.
The constructor, input caller, flag getters and return cleanup were independently read
from the local executable using disasm-rva.py. Raw output is not committed.

Current FViewport arrives as `self` at the already verified gameplay Draw site, before
stereo tags and camera scopes. Its live UGameViewportClient owner has Viewport at+0x40,
confirmed by the existing draw caller at RVA2330D3. A one-shot request refreshes
BuildLiveSet, finds an IsLiveObject owner whose exact field matches self, and validates
both native vtables, HWND/current window thread, function signature and ret24. Ownership
is rechecked immediately before the mutating native call. Class name is only a selector,
never a liveness substitute. No viewport/object pointer survives the request.

The overlay only queues dimensions. The game thread persists/advertises them through
ResRequest before calling the native resize with fullscreen=true; VirtualMode retains
the requested dimensions while creating a windowed D3D device. The request cannot run
between eyes. A returned call is not success: actual capture dimensions must match within
10seconds, otherwise status reports unconfirmed and does not automatically retry.
A pending request times out instead of firing after an arbitrary later menu/scene.

Host fixtures include production viewport_resize.cpp and cover queue isolation,
all six arguments, exactly-once consumption, wrong thread/window/type/ABI, failed live
refresh, dead/ambiguous/replaced owner, timeout and downstream size confirmation.
23 checks pass. This is not engine/headset execution; runtime resize acceptance is
pending. Defaults and performance evidence remain in PERFORMANCE.md.

## Crouched pitch on two machines: the crawl tuck was switching the camera's own bone control off (VR-122, 2026-09-16)

**The report.** On one machine (Quest 3 through VirtualDesktopXR, 90 Hz) the camera was
judged right standing, walking, in cinematics and crouched while turning, but crouched and
pitching the head the world appeared to move with the head. The other machine had judged
crouched pitching fixed (VR-78). Step one was the inis and the logs, not the code (TRAPS
section 1): the reporting machine's installed ini differed from `release/dishonored_vr.ini`
in three `[Hud]` keys only; every `[Neck]`, `[PosTrack]`, `[Hands] Crouch*`, `[Pace]` and
`[Mode]` key was the release value, and the run resolved `Mode=cancel 0.321/0.062`,
`CrouchPivot 0.000/0.000`, `ZAccount=1`, VDXR at 90.0 Hz, 2750x2850. So the setting
diff was empty, and the answer had to come from a measurement.

**What the reporting machine's own log already said.** In its crouched episodes the
accounting probe never filled a DOWN or UP bucket: the engine-pivot fit covered CAMERA
pitch -20..+2 deg while the head pitched to -35 deg (the `headtrack:` line wrote -48.8 deg
of view pitch from a -48.8 deg head), and the crouched LEVEL bucket averaged head -8.9 deg
against camera -0.2 deg. Standing on the same run the DOWN bucket filled at head -29.4 /
camera -29.7 deg. The written view pitch reached the controller (the engine handed it back
next dispatch) but the camera's basis rows (`kCamFwd`) did not follow it while crouched.

**Reproduced on the simulator** (button crouch through the pad bridge, `head rot 0 -30 0`
held 7 s, the Hound Pits save), the same build and the release ini:

| stance | bucket | head / camera pitch | accepted | residual up / fwd (uu) | notes |
|---|---|---|---|---|---|
| standing | DOWN | -30.0 / -30.2 | 415 per eye | -0.03 / -1.46 | base -7.12, neck +7.99: the standing cancel is right |
| standing | UP | +30.0 / +29.8 | 392 per eye | -0.03 / +1.72 | |
| crouched | DOWN | (none) | 0 | | every pitched sample was "settling" (under 500 ms) |
| crouched | LEVEL | +0.2 / -0.2 | 1520 per eye | +50.51 / +5.19 | the reference; 3040 of 3040 accepted samples landed here |
| crouched | UP | (none) | 0 | | |

The picture said the same: `xrsim-shot` at head -30 deg differed from the level capture by
mean-abs 64.2 standing and **3.7 crouched** (early and late in the hold), i.e. the crouched
render did not pitch at all. Under a projection layer the compositor then shows a level
image at a pitched pose, which is the report's "the world moves with the head".

**The owner, by live A/B** (each: crouch, pitch -30 deg, capture, diff against crouched
level): `neck off` 2.6, `cinehead off` 1.8, `cinepitch off` no bucket change, **`hands off`
63.2** (the view pitched). Then, with the tuck already engaged: `hands off` 3.6 (still
level), so it is not a per-frame effect but a write that persists. The write is the crawl
tuck (38.19/38.20, `SkcSetCrawlStrength`): on every crouch it wrote `ControlStrength=0` to
the three player look-at controls, and slot 0 is `LookAtControl_Camera` (the log:
`skc: >>> PLAYER control #0 ControlName='LookAtControl_Camera'`, then
`hands/crawl-strength: 0.0, wrote 3 validated controls`), the control this file already
records as the camera's own bone control (VR-30: "do not zero it"). With it at 0 the camera
bone rests in the crouch animation's pose, level, whatever the controller's pitch says.
The headset run's log carries the identical `wrote 3 validated controls` line on every
crouch (9 episodes).

**The fix** (`[Hands] CrawlTuckCamera=0`, shipped; `hands tuckcam on|off` live; `1` is the
old behaviour): the tuck releases the two HAND controls and leaves the camera's alone; a
release (1.0) still reaches all three so a camera an earlier tuck zeroed comes back. On the
same simulator session, crouched at -30 deg: DOWN filled (754 per eye, head -30.0 / camera
-30.2), UP filled (409 per eye, +30.0 / +29.8); the captures: crouched -30 deg vs level
**62.9** with the lever off, **4.7** with `hands tuckcam on` (applied at once while tucked),
62.9 again with it off.

**The second finding, which corrects VR-78.** With its look-at control kept, the crouched
camera pitches about the SAME neck as standing: the probe fitted 0.291 m below / 0.052 m
behind crouched (rms 0.5 uu, 3304 samples) against 0.292 / 0.052 standing on the same run,
and the crouched DOWN row read base -7.03 uu, cap +4.97, forward residual +14.94 with the
crouched pivot at 0/0 (UP: -16.51). VR-78's "crouched, the engine has no neck arc" was
measured on a camera whose look-at control the tuck had just zeroed: it described that
camera truthfully, and 0/0 is the right pivot for it. So the crouched keys now apply only
while the tuck has released the camera control (`CrawlTuckCamera=1`); otherwise a crouch
keeps the standing pivot whatever an older ini's `CrouchPivot*` says, and the stance line
names which and why. Crouched at -30 deg with the standing pivot: residual up
+0.02 / fwd -1.46 uu; at +30 deg +0.02 /
+1.72 (standing on the same run: -0.03 / -1.46 and -0.03 / +1.72).

**Why the two machines differed** cannot be closed from this side, and the ticket says so.
Both machines' inis lack `CrawlTuck` (compiled default 1) and both logs show the tuck
writing 0 into three controls; the other machine's crouched buckets (VR-78, camera-pitch
buckets by construction, the probe never bucketed by head pitch) filled at -33/+32 deg
with the base not moving, which is a camera that still pitched with its look-at control at
0. The candidates, in order: an older ini that still carries `CrawlTuck=0` (the tester's
2026-09-02 profile did, `tests/golden/f10-tuned-2026-09-02.ini`, and the key is never
rewritten); the player-control latch not having matched the camera slot on that run (the
33.3 note: after a checkpoint restore the name chain fails and the pointer match needs the
component list populated); or a game option that decides whether the camera reads its
rotation from the bone. The new `config: [Hands] CrawlTuck=.. CrawlTuckCamera=..` line and
the `hands/crawl-strength ... | LookAtControl_Camera ...` line answer the first two from a
log alone; the third is the open question and the other machine's next run answers it.

**Not the cause** (each with the counterprediction that killed it): the crouched neck term
(it already read 0/0 and `neck off` changed nothing: 2.6); the eye ceiling (the cap trims
about 5 uu in BOTH stances on both machines, the VR-87 residual, and the crouched
episodes clipped 0 presents); the cinematic pitch and head-look scopes (`cinepitch off`,
`cinehead off`: no change); the game's own crouched pitch limits (with `hands off` the
crouched camera pitched to the written -30 deg).

## Weapon dial input and crop reuse (VR-126, 2026-09-16)

The existing measured wheel rectangle above is reused with margins by HUD_ANCHORS'
VR-126 implementation. No new engine addresses or writes are introduced. The
continuous-angle fault is in pad_bridge's final generic menu shaping: Wheel is
a blocked UI context and was fed through independent cardinal MenuStep pulses,
with the right stick erased. Final output now bypasses that block for Wheel and
uses radial shaping. See HUD_ANCHORS for geometry, validation and pending test.

## Menu immersion camera and UI blend (VR-126, 2026-09-16)

The existing reflected Camera.CameraCache.POV.Rotation is scoped across both eye
draws for opted-in riding menus. Camera/controller/pawn are captured from the
current chain with BuildLiveSet and ChSlot/IsLiveObject guards. A context epoch
forces refresh on each new menu interval; no address or unchecked writer is added.
Head sample publication uses the same per-image path as cinematic scopes.

The UI-only blur candidate resolves Actor.WorldInfo, WorldInfo.Game,
DishonoredGameInfo.m_pPpManager and DisPostProcessManager.m_UIPPWeight by property
name. Declarations identify a separate UI effect weight/parameters from the Kismet
effect. The wheel's m_bBlurGameWhileActive default is not established true, so that
flag alone was rejected as a sufficient route. The verified native-registration
search returned radial component functions but no UI blur toggle. Weight suppression
and visible consumption remain separate claims; headset validation is pending.
Full implementation and failed-hypothesis record: HUD_ANCHORS current VR-126 section.

## Scoped menu/cinematic translation frame correction (2026-09-16)

CameraCache.POV.Rotation is draw-scoped, while the cached camera matrix rows remain
native. The menu/cinematic scopes must map current head-yaw-relative translation
through the written composed yaw, not those rows. No new engine offset is required.
Head sample publication now includes same-locate normal/raw translation. Entry neck
correction is rebased between physical yaw frames. Existing liveness/identity and
exact owned-field restoration remain. FLICKER_REFERENCE records the verified374
report, host negative control, single-scene scope gap and pending perceptual test.

## VR-126 menu mono gaps and HUD ownership limits (2026-09-16)

Build376 playtest confirms unintended camera sliding appears fixed. Interior wheel
stereo beat intervals still include mono in17/26 samples; notes58/72. This establishes
mono interruptions, not a complete explanation for transient enlargement. HalfIPD
current-c5 telemetry is not image-owned geometry evidence. Full evidence/fix/test is
in FLICKER_REFERENCE, latest menu depth interruption section.

HUD shader/declaration/texture-class buckets are not semantic element names. Region
center routing can switch a moving draw between anchors. Candidate adds short-lived
local-content continuity plus private measured-element textures; initial spatial
classification, geometry changes and shared-content ambiguity remain limitations.
No engine-memory writes or new engine addresses introduced. HUD_ANCHORS is authoritative.

## VR-127 reading input and unresolved HUD group identity (2026-09-16)

MenuStep is a pulse generator, not analog scrolling: first press,380ms initial
repeat delay,170ms subsequent delay. Production host sampling at120Hz gives5/120
nonzero samples during a held stick; continuous reading retains120/120. Context4
includes readable notes/books; context5 is journal. No engine write is required.

Local DisGFxMoviePlayerHUD declarations expose m_TaskMarkers/m_SortedMarkers and
interaction-context groups. This does not establish native pointer layout or the
render callback corresponding to each UI object. Do not turn these declarations into
unchecked memory offsets. The current default-off routing candidates instead test
measured marker geometry and bounded neighboring prompt draws; exact tolerances,
limits, evidence and next question live in HUD_ANCHORS. No new addresses added.


## HUD closing ownership and pause upload timing (2026-09-16)

Local declaration inspection identifies DisGFxMoviePlayerBase.m_bIsClosing separately
from DisGFxMoviePlayerPowerWheel.m_bWheelIsOpen. The candidate resolves the closing
bool through FindBoolProp, reads the current manager-owned wheel instance using
CtRead, and uses it only for HUD visual ownership. No fixed offset or engine write.
Missing reflection leaves the existing owner read plus delayed-capture tail; new
hud/layout closing logs must establish runtime exercise. bMovieIsOpen alone is not
suitable because persistent movie packages can survive hiding. Installed native
m_fTimeTransition=0.0 does not establish when queued draw pixels finish.

387 pause intervals contain camera-silent refusals and mono output. The existing
serial increments on every observed c5 upload, including unchanged values. Saving
it at draw END and requiring advancement before next draw can discard uploads
inside the previous draw. The default-off pause-only recent-draw trial and its
limits are recorded in FLICKER_REFERENCE; this does not supersede historical pause
results with a claim that all pauses are mono or that a c5 serial proves eye geometry.

## Menu exit yaw and native objective labels (2026-09-16)

Source boundary: menu_immersion's render scope composes native base with inverse
entry head and current head, then restores native fields. head_track's blocked
ProcessViewRotation path advances its previous-head reference without adding yaw.
This explains missing accumulated physical turn on exit. MenuExitHeading computes
only a numeric wrapped yaw delta, after BuildLiveSet and existing MhValidate
(camera/controller/pawn slots, possession, load identity). The existing script
writer applies it once; retained pointers alone never authorize it. No new engine
address or offset. Direct fallback remains unchanged. Headset acceptance pending.

Build389 native objective icon is reported correct; title/distance remain separate
draws. NativeObjectiveLabels is bounded current/prior-present proximity association,
not a discovered semantic field. Scale the associated draw around the marker pivot
rather than its own center. Never retain glyph ownership across unrelated draws.
Ambiguous markers refuse; batching/order/layout are limitations. Host tests verify
pivot arithmetic and shader restore, not actual title identification.

## VR-129 native Scaleform assets and closing lifetime (2026-09-16)

Offline UE Viewer -export -3rdparty successfully exports UI_PowerWheel_SF.upk and
UI_HUD_SF.upk. FFDec opens their GFX and resolves external textures when exported
TGAs sit beside the movie.1280x720 wheel contains distinct wheel_mc, shortcuts_mc,
potions_mc and PC alternatives. Runtime scripts control safe-area positioning,
item population and shortcut mode; static authoring preview does not execute those.
Wheel/background/potion/D-pad close alpha tweens are250ms, with native OnClosed
notification following the wheel tween. Quick-shortcut use outside the wheel has
additional staged delays; do not generalize wheel lifetime to it. No new engine
offset/address or memory writer is introduced. Source game assets remain local.

Native upright HUD candidate reuses MpReadCtx's symmetric-perspective basis test:
normalize view-projection x/y columns, require unit homogeneous-forward column and
orthogonality, project UE world +Z, refuse near-vertical view. Focal ratio preserves
angular shape while rotating clip XY around existing marker center. Depth/clip W
and shader shadow remain unchanged; original constants restored after each draw.
Basis is captured at c5 camera upload on render thread and usable only in the same
present/thread. Unknown/asymmetric/degenerate basis refuses upright correction.
The legacy pose yaw solver refused391; this independent bounded basis extraction
is a candidate requiring its own logged/headset confirmation, not a solved pose
correspondence claim. Offline workflow and sources are in HUD_ANCHORS VR-129.

## VR-129 objective children and runtime icon imports (2026-09-16)

UI_HUD_SF offline XML: objectiveMarker_primary sprite178 and secondary174 share
_description_mc sprite160 at display depth1; _icon_mc is177/173 at depth4.
_description_mc.txt is DefineEditText159 (with drop-shadow filter); its panel is
shape158. Icon artwork is35x35 authored pixels. Child names are established from
exports; no correspondence from GFx instance to intercepted D3D draw is established.
The title-before-icon order undermines frame-local positional grouping.395 contains
8-vertex/10-primitive icon AND wide description composites, so topology alone is
not semantic identity. Runtime draw keys include resource pointers and must not be
baked as universal signatures. Current F10 panel controls are bypassed by native
objective mode; NativeObjectiveScale is the consumer that still changes recognized
native geometry. No new engine addresses, offsets or memory writers introduced.

PowerWheel imports common_assets/lib.swf, but standalone Common_assets package is
absent. DefaultEngine lists it as a startup package, and UModel's Startup inventory
locates SwfMovie lib and lib_* textures.54 external textures resolved from lib XML.
The itemIcons sprite301 is an animated shell; EquipmentIcon.SetIconImage invokes
req_EquipmentIconImage to load runtime artwork.15 Startup textures ic_item_* and
ic_pow_* match the screenshot's equipment and powers. UI_ItemIcons_Large and
UI_Powers_Large exports are journal-style alternatives, not this missing wheel art.
Revised tools/hud-assets-export.ps1 exports dependencies and remaps the import for
FFDec. Static frame export still cannot execute native callbacks. Extracted output
and full scripts remain local ignored build/hud-assets only.

## VR-133 lean/keyhole ownership and allocation failure (2026-09-17)

Source427 DLL/banner verified.119 trace samples in StatePlayerMasterLeaning
show influence(anim/player/look)=0/1/0 while ProcessViewRotation still injects
physical head orientation. Example1915: HMD roll1.91, PC1.90, final cache10.00;
2009: HMD-0.92, PC-0.91, cache9.09. The existing cinematic ownership gate does
not include this state. This supports modifier interference, not stale eyes.

Local decompiled StatePlayerMasterLeaning declares pitch limits +/-35 and yaw
+/-60, release time0.2s. DishonoredCamera_Lean is a separate influence after
player control; spring/pivot, stick pitch/roll, collision and tilt properties
are present. StatePlayerMasterHolePeeking carries a door, fade state and a
controller-reposition flag. Declarations do not establish native function bodies.
No new engine offsets required: use the existing reflected FSM and camera fields.
Candidate explicitly scopes physical head look to lean/keyhole while preserving
native location, validates current live owners and restores original fields after
both eye draws. Input delta writer stands down only on a successful scope lease;
exit applies the reference-to-current yaw once through the existing gameplay
writer after current-table revalidation. Unknown states do not acquire ownership.

At17783125 the SYSTEMMEM twin for1920x2048 DXT5 (format894720068),1 level failed
with0x8007000e. Existing shadow path then leaves an unlockable DEFAULT texture;
reported dialog says LockRect D3DERR_INVALIDCALL. Crash occurred after pause
opened, following lean. Allocation pressure is established; its source is not.
Current exe PE flag LARGE_ADDRESS_AWARE is already set. The preserved46MB dump
has no MemoryInfoList stream. Failure-only VirtualQuery/GlobalMemoryStatusEx
logging now distinguishes total/largest free virtual region and system commit.
Ordinary minidumps add memory metadata without copying full process memory.
No eviction: native mip streaming reads retained CPU twins, so dropping them
without a replacement changes resource semantics. Crash prevention remains open.

## Native SteamVR orientation audit (VR-146, 2026-09-19)

Valve added native 32-bit OpenXR in SteamVR 2.17:
[official SteamVR announcements](https://steamcommunity.com/app/250820/announcements/?snr=2_groupannouncements_detail_).
This machine's runtime identifies as 2.17.10. Both observed runs used native
OpenXR, not dvr_steamvr32.dll. The mod's old startup text incorrectly claimed
SteamVR never supported x86; corrected without changing native-first selection.

Evidence: installed build503 SHA256
30d2e34d7553c996ea94558daba22f15efa2fd0e312d422ce6f738cd4b738321.
First run used SteamVR/OpenXR in Meta compatibility mode. SteamVR settings
openxr.metaUnityPluginCompatibility was forced (2); setting it to0 while the
runtime was stopped changed the next runtime identity to SteamVR/OpenXR.
The second log banner is vr33-hands-working-503-g7159acacf, tick5343687.
The inversion remained reported: menus and world inverted, hands upright after
loading gameplay. The compatibility-only explanation is falsified for this run.
Both logs are preserved in install-506/before-install-20260919-111559 under
build/playtest-candidates/hud-improvements; the earlier settings comparison is
in steamvr-native-503 in the same parent directory.

Head tracking reports roll178.47 at tick5354265 and179.60 at5362109. TrackHead
reads -atan2(matrix[1][0],matrix[1][1]); DvrPoseTo3x4 uses a conventional Hamilton
quaternion matrix and both reference spaces are created with identity offsets.
No static evidence identifies a conversion defect. The old log cannot correlate
these samples with headset wear, so this is not evidence to subtract180 degrees.

Candidate506 measures xrLocateSpace(VIEW,LOCAL) and xrLocateViews at the same
predicted display time, logging validity, quaternion norms, derived camera roll
and head-versus-eye-midpoint angular separation. Three-second cadence,120 samples
maximum, native SteamVR only. It makes no camera or pixel corrections. Expected
test: same inversion with headset held upright in menu and gameplay. Large raw
head/eye disagreement implicates the independently located VIEW-space pose;
agreement with upright raw poses directs investigation to camera/layer transforms;
agreement near180 while worn upright directs investigation to the runtime/local
reference frame. Invalid samples or an unworn headset cannot choose among these.

A speculative whole-image Y flip was built and passed a WARP copy test, but was
never installed and was removed from production after the upright-hands report.
A pixel-copy test validates the copy operation, not its suitability as a fix.
Its source remains locally recoverable in build/steamvr-image-flip-uninstalled.
Do not revive it without identifying the inverted surface and explaining why
correct hands would remain correct. The frozen506 hash, validation and installed
INI comparison are recorded in STATUS.md. No headset-confirmed fix yet.