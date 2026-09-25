#include "core/framework/render_profile.h"
#include "core/input/controller_emulation.h"
// core/config/config.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static bool g_configResetPending = false;

static bool WriteDefaultIni(const char* ini)
{
    // The maintainer requested the complete tested profile as repo defaults,
    // including saved F10 flags and diagnostics. Keep release/ini byte-aligned.
    FILE* f = fopen(ini, "w");
    if (!f) return false;
    const int written = fprintf(f,
        "; Dishonored VR config - edit while game is closed\n"
        "; (auto-refreshed when the mod's defaults change)\n"
        "[Meta]\n"
        "Version=%d\n"
        "[Tracking]\n"
        "; head tracking drives the game camera via mouse emulation.\n"
        "; Calibrate: pick a landmark, turn your head 90 degrees; if the\n"
        "; world turns less than you did, RAISE CountsPerDegree; if more, lower.\n"
        "Enabled=1\n"
        "YawCountsPerDegree=11.5\n"
        "PitchCountsPerDegree=11.5\n"
        "InvertPitch=0\n"
        "; HeightOffsetM shifts the eyes vertically (metres, negative = lower). +0.060 is\n"
        "; headset-judged 2026-09-05, arrived at by ear across the arm-decoupling runs\n"
        "; (-0.090 -> -0.050 -> +0.060 as the arms stopped moving and the scale settled).\n"
        "; It pairs with [PosTrack] Scale=108 from the same session - eye height and world\n"
        "; scale are judged together, so moving one alone will read as wrong.\n"
        "; F10 View tunes it per person, SAVE AS DEFAULTS writes it back.\n"
        "HeightOffsetM=0.060\n"
        "PhysicalCrouch=1\n"
        "[Stereo]\n"
        "PairTrace=1\n"
        "DrawCallerTrace=1\n"
        "RingLedger=1\n"
        "LateTagRepair=1\n"
        "; Method=mono|aer|reentry: the rung of the stereo ladder (docs/ARCHITECTURE.md).\n"
        "; reentry (ships, 41.1) draws the scene twice per tick, once per eye, into a\n"
        "; projection layer - native stereo, HEADSET-VERIFIED on a Quest 3 (2026-09-03); mono\n"
        "; shows the game on a head-locked screen in both eyes (the fallback, and what a\n"
        "; refused method leaves running); aer is a design stub and refuses with a note.\n"
        "; `stereo <name>` switches live and fails soft. Armed=1|0: whether the selected method\n"
        "; RUNS (the F10 Display tickbox, `stereo arm on|off`); 0 parks the game on the mono\n"
        "; screen without forgetting the selection. C5Pair=1 (41.1, session 9): each present's\n"
        "; eye tag is checked against its camera step from the previous present (pass 2 sits\n"
        "; exactly one IPD along right of pass 1, nothing else moves inside a tick) and the\n"
        "; tag ring is realigned when they disagree - the tags rode the other draw across\n"
        "; every pause, load and re-arm before (the eyes SWAPPED). 0 = the ring's order alone.\n"
        "Method=reentry\n"
        "Armed=1\n"
        "C5Pair=1\n"
        "; HoldUntagged=N (41.1): a tick that fails the second draw gates presents\n"
        "; UNTAGGED, and an untagged present is the mono path - the same image in BOTH\n"
        "; eyes. The error scales with disparity, so it is invisible on distant geometry\n"
        "; and glaring on the viewmodel at 30-50 cm: the tester saw it as the arms and\n"
        "; weapon flickering (2026-09-03, mono/s 1-4 against out/s 220). N holds up to N\n"
        "; CONSECUTIVE untagged presents back - the previous pair is re-submitted, so the\n"
        "; compositor genuinely keeps showing it - before letting one through as mono, so\n"
        "; a real transition (menu, load, cinematic) still reaches mono within N presents.\n"
        "; 3 ships: HEADSET-JUDGED on a Quest 3 (2026-09-03), mono/s 1-4 -> 0 with every\n"
        "; other flicker gone. 0 = OFF, the pre-41.1 behaviour, and the A/B for it.\n"
        "; `stereo hold <n>` live; `stereo status` and the beat report how many were held.\n"
        "HoldUntagged=3\n"
        "; Set from the tested machine's ini (VR-72): F10 panel and calibration keys.\n"
        "LagAB=0\n"
        "SingleTagRepair=1\n"
        "[Camera]\n"
        "HeadBasedMovement=1\n"
        "; EyeField= the camera field the per-eye offset is written to. 0x330 was measured\n"
        "; 2026-09-02 with `camera eyetest` (HONOURED 119/120; docs/dishonored/ENGINE_NOTES.md,\n"
        "; the per-eye camera seam); none disables the write.\n"
        "EyeField=0x330\n"
        "; ArmFollowWeight (VR-30): how hard the arms and weapon follow where you LOOK.\n"
        "; -1 = off, the game's own value stands (ships). 0..1 forces it: 0 should stop the\n"
        "; arms following the view entirely, 1 is the shipped behaviour written by us instead\n"
        "; of by the engine (a control - if 1 looks stock, the write is landing).\n"
        "; Measured 2026-09-05: the engine RECOMPUTES these weights every frame (they ride\n"
        "; 0.982..1.000 and never leave it), so this is written on every script dispatch, the\n"
        "; same cadence the FOV lever uses to outrun the same recompute. The game's own\n"
        "; DishonoredCamera.ini m_fDefaultWeight was tried first and does nothing.\n"
        "; This is ROTATION only; the position channel belongs to a separate ticket.\n"
        "; `arms follow <0..1>` and `arms follow off` are the live A/B.\n"
        "ArmFollowWeight=-1\n"
        "; ArmDisableWeight (VR-30): the OTHER way at the same thing, and the better one.\n"
        "; Rather than pasting a weight over the engine's recompute (which measured as a\n"
        "; 1.000<->0.000 fight between dispatches, seen as the arms TRAILING the view), this\n"
        "; forces the game's own DisableArmFollow influences, so the recompute derives an arm\n"
        "; follow of 0 by itself and holds it. -1 = off (ships), 1 = full decoupling.\n"
        "; m_Weight, m_TargetWeight and m_bActive are all written every dispatch, because the\n"
        "; game pulls the target back to 0 about 60 ms after raising it on its own.\n"
        "; `arms disable <0..1>` and `arms disable off` are the live A/B.\n"
        "ArmDisableWeight=1\n"
        "; ArmLookAtStrength (VR-30): the YAW candidate. ArmDisableWeight above took the\n"
        "; arms' VERTICAL follow and left the horizontal alone, and forcing the settings\n"
        "; weights on top of it added nothing - so both of those act on one channel and\n"
        "; neither reaches yaw. LookAtControl_Camera is the Arkane look-at that aims the\n"
        "; arms at the view; 0 stops it. -1 = off (ships), 0..1 forces its ControlStrength.\n"
        "; This one finds the node itself, so it works on a stock install - the mod's older\n"
        "; [Hands] CameraLookAtStrength writes the same field but is dead behind the\n"
        "; hand-mesh gate that [Mode] GamepadOnly=1 closes.\n"
        "; `arms lookat <0..1>` and `arms lookat off` are the live A/B.\n"
        "ArmLookAtStrength=-1\n"
        "; ArmCounterYaw (VR-30): THE YAW LEVER, and it ships ON at 1.0.\n"
        "; Reading the scripts end to end says there is no yaw driver for the arms at all -\n"
        "; every arm/aim/look mechanism the game exposes is pitch-only or NPC-only. The arms\n"
        "; yaw because the PAWN yaws: they are bones of its mesh, and the pawn's native\n"
        "; FaceRotation sets its yaw from the controller every frame. There is nothing to\n"
        "; switch off. So this SUBTRACTS instead: m_ArmFollowOffset_Rot_Primary/_Secondary\n"
        "; are the game's own authored arm rotation offsets (Rotators, already scaled by a\n"
        "; weight the engine respects), and they get a yaw equal and opposite to how far your\n"
        "; head has turned since the reference. The arms then hold still in the world while\n"
        "; the view turns, and attachments follow for free because the engine applies it.\n"
        "; Stick turning moves the BODY yaw, not the head's, so it is untouched - the body\n"
        "; comes round and the arms come with it, which is what you want.\n"
        "; 1.0 = hold still, 0.5 = follow at half rate (tune by feel), 0 = write a zero offset\n"
        "; as a control, -1 = off. `arms yaw <0..2>` and `arms yaw off` are the live A/B.\n"
        "ArmCounterYaw=-1\n"
        "; Set from the tested machine's ini (VR-72): F10 panel and calibration keys.\n"
        "BodyYawLock=-1\n"
        "ArmStripMeshRot=-1\n"
        "ArmBodyFacing=1\n"
        "[Capture]\n"
        "; Mode=sync|deferred|shared: how the game's frame reaches the headset\n"
        "; (core/gfx/capture). sync reads the frame back and waits for it every present\n"
        "; (the old baseline, ~5 ms per present at 1080p, 15 ms at the Quest 3 size);\n"
        "; deferred copies on the GPU, queues the readback and locks it one present later\n"
        "; (~2.3 ms at 1080p, ~10 ms at the Quest 3 size; the picture is one present late;\n"
        "; also resolves a multisampled backbuffer; the fallback when the device cannot\n"
        "; share); shared (ships, 41.1, headset-judged 2026-09-03) needs [Device] Ex=1 and\n"
        "; keeps the frame in VRAM (the log's capture/probe lines say). `capture mode <m>` switches\n"
        "; live; `capture status` prints the cost. shared (needs [Device] Ex=1) keeps the frame\n"
        "; in VRAM: two shared render targets, each blit fenced by a D3D9 event query;\n"
        "; SharedWait=0 delivers the previous present's slot (no wait in the common case),\n"
        "; 1 delivers this present's after its fence (zero latency, the CPU waits for the frame\n"
        "; in flight). `capture sharedwait on|off` live.\n"
        "; BboxMs: how often the content-bbox instrument (the FULL/CROPPED line) resamples.\n"
        "; Each sample is a full-frame GetRenderTargetData + LockRect + row copy on the\n"
        "; present thread - the same round trip that makes Mode=sync cost 17-21 ms/present -\n"
        "; so this is a frame-time setting, not a logging one. 0 = only after a size change,\n"
        "; which is the sample that decides FULL vs CROPPED. `capture bbox off|<ms>` live.\n"
        "Mode=shared\n"
        "SharedWait=0\n"
        "BboxMs=30000\n"
        "[Pace]\n"
        "ImageOrientation=1\n"
        "; The pair pacing levers of the projection layer (stereo reentry), all live on\n"
        "; the `vrpace` seam word and the F10 Runtime panel; SAVE AS DEFAULTS writes them.\n"
        "; Ahead=0|1|2: locate the head pose (and the layer's views) this many display\n"
        "; periods past predictedDisplayTime - a pair that closes after its slot displays a\n"
        "; slot late, and 1 makes the pose match that slot (the log's `pair phase` line says\n"
        "; which). Strict=1: a stereo submit whose eye is older than one present shows the\n"
        "; fresh eye to both eyes for that frame instead of a held eye (the pause/resume\n"
        "; desync fail-soft). Lag=0|1|2: which locate generation the layer is tagged with\n"
        "; (1 = one back, the measured default). All ship at today's behaviour.\n"
        "; SyncHz=0|10..500: lock the pair schedule to this rate instead of letting it\n"
        "; free-run at whatever the game produces. 0 = off (ships). This is the JUDDER\n"
        "; lever: a game that cannot hold the headset's rate wants a clean SUBMULTIPLE of\n"
        "; it, so every frame is held for the same whole number of display slots - 60 or\n"
        "; 40 on a 120 Hz headset, 45 on 90. An uneven cadence is seen as doubled edges on\n"
        "; world geometry during a head turn, and the log's `stereo: rate` line says\n"
        "; UNEVEN CADENCE with the slots-per-frame that names it. `vrpace sync <hz>` and\n"
        "; `vrpace sync off` are the live A/B; judge it in the headset before saving.\n"
        "Ahead=0\n"
        "Strict=0\n"
        "Lag=2\n"
        "SyncHz=0\n"
        "[Perf]\n"
        "CpuScopes=0\n"
        "NativeProfile=0\n"
        "DiagnosticAb=0\n"
        "BridgeGpu=1\n"
        "RenderProfile=0\n"
        "DesktopAb=0\n"
        "; The tick budget (core/framework/perf): Instruments=1 keeps one record per present\n"
        "; (eight clock stamps) and prints the `perf: tick` line every 3 s; GpuQueries=1 adds\n"
        "; the D3D9 timestamp ring behind the `perf: gpu` line (read five presents back, never\n"
        "; waited on). `perf on|off`, `perf gpu on|off` live; `mark <text>` and the F10 MARK\n"
        "; button stamp a felt freeze. ForceNoVSync=1 presents without vsync (the game's own\n"
        "; setting is ignored while the headset paces the game). FrameId=1 runs the frame-\n"
        "; identity trace (core/gfx/frame_id): a 64x64 thumbnail of every present at four\n"
        "; stages (the backbuffer, the shared slot, the eye texture, the swapchain image),\n"
        "; read three presents later, and per left/right pair the `stereo: frameid` line\n"
        "; says at which stage the two eyes stopped being two pictures. 16 KB per present;\n"
        "; `frameid on|off|status` live.\n"
        "Instruments=1\n"
        "GpuQueries=1\n"
        "ForceNoVSync=1\n"
        "FrameId=1\n"
        "FrameIdEvery=8\n"
        "; Ab (41.1, VR-67): the performance A/B. 1 = on the FIRST present of a run it starts\n"
        "; an announced plan of segments, switching ONE lever at a time and returning to the\n"
        "; baseline between alternatives, then prints a per-segment DISTRIBUTION (p50/p95/p99/\n"
        "; max and how many intervals ran over twice the median). A window MEAN cannot see a\n"
        "; frame drop, which is why this exists. Nothing about what is rendered changes, and\n"
        "; the baseline is restored when the plan ends. `perf ab status|off|restart|seg <ms>`.\n"
        "Ab=0\n"
        "[Device]\n"
        "; Ex=1 creates the game's D3D9 device as D3D9Ex (core/gfx/d3d9ex), which is what lets\n"
        "; [Capture] Mode=shared keep the frame in VRAM (the CPU readback owned the tick at the\n"
        "; Quest 3 size: 16 ms of GPU copy per present, measured 2026-09-03). A 9Ex device refuses\n"
        "; D3DPOOL_MANAGED, which this game asks for on every static texture and buffer, so\n"
        "; Managed= says what stands in: shadow (a system-memory twin per texture, locks\n"
        "; redirected; the safe one - the game locks textures READONLY while streaming),\n"
        "; dynamic (DEFAULT+DYNAMIC: READONLY locks read uncached VRAM), default (textures lose\n"
        "; their locks), none (the refusals are the measurement). Launch-time: `device ex on|off`\n"
        "; and the F10 Display tickbox write the key for the NEXT launch. Ships ON since 41.1\n"
        "; (headset-judged 2026-09-03: the Quest 3 size at the headset's rate); Ex=0 is the\n"
        "; plain device and the readback capture, the fallback if the 9Ex device misbehaves.\n"
        "Ex=1\n"
        "Managed=shadow\n"
        "; ShadowSurfaces=0|1 (VR-15, the black texture bug). Managed=shadow redirects a lock\n"
        "; taken on the TEXTURE to its system-memory twin, but the game can also take a\n"
        "; SURFACE off the texture (GetSurfaceLevel) and lock that - a different vtable, so\n"
        "; the redirect never sees it and the write lands on the DEFAULT texture, where D3D9\n"
        "; refuses it and the texture keeps the contents it was created with: black. 1 sends\n"
        "; that lock to the twin's matching surface too. Ships OFF (0) until a headset says it\n"
        "; removes black surfaces; `device shadowsurfaces on|off` is the live A/B, and the\n"
        "; log's `device/upload` verdict says whether the bypass is happening at all.\n"
        "ShadowSurfaces=0\n"
        "; ShadowFullCopy=0|1 (VR-15, the black texture bug). SOLVED and headset-judged\n"
        "; 2026-09-05: this was the black texture bug. The shadow used to push a written\n"
        "; texture with UpdateTexture, which takes NO LEVEL, and writes to mip levels above\n"
        "; 0 were not being carried - so a surface was black at DISTANCE (small mips) and\n"
        "; correct up close (level 0). The game locks level>0 fifty thousand times in one\n"
        "; load and never calls AddDirtyRect once. 1 pushes exactly the level the unlock\n"
        "; wrote, with UpdateSurface, which names its two surfaces and cannot be vague about\n"
        "; which level it copied; a format that refuses is remembered and goes straight to\n"
        "; UpdateTexture from then on, so a refusal is paid for once, not every unlock.\n"
        "; SHIPS ON (1) - a deliberate exception to 'every render lever ships OFF', made\n"
        "; because 0 is a visible rendering bug, the same call as [Stereo] HoldUntagged.\n"
        "; `device shadowfullcopy on|off` is the live A/B; 0 restores the fault.\n"
        "ShadowFullCopy=1\n"
        "[Screen]\n"
        "; Gameplay horizontal FOV; 0 restores headset-derived. Live: projectionfov 60..120|off.\n"
        "ProjectionFov=103.00\n"
        "AnchorCinematic=1\n"
        "AnchorMissionStats=1\n"
        "AnchorStore=1\n"
        "AnchorWheel=1\n"
        "AnchorJournal=1\n"
        "AnchorNote=1\n"
        "AnchorPause=1\n"
        "AnchorLoading=1\n"
        "AnchorMainMenu=1\n"
        "AnchorOther=1\n"
        "AnchorMono=1\n"
        "; The mono screen: a head-locked quad DistanceMeters away and WidthMeters\n"
        "; wide. Per-eye rendering will replace it (docs/ROADMAP.md).\n"
        "DistanceMeters=1.75\n"
        "; HeadLocked=1 keeps the screen in front of your eyes (turning your head turns the\n"
        "; game camera); 0 leaves it standing in the room where you recentered.\n"
        "HeadLocked=1\n"
        "WidthMeters=2.4\n"
        "; RenderWidth/RenderHeight/RenderFullscreen (41.1): the render-resolution picker's\n"
        "; ask, 0 = the game's own size. It takes effect at the NEXT LAUNCH, and THESE FOUR\n"
        "; KEYS ARE THE ONLY PLACE THE SIZE LIVES (VR-66): the proxy appends -ResX/-ResY to the\n"
        "; command line the engine reads, and VirtualMode advertises that same size in the\n"
        "; adapter's mode list, from this one ask. Editing them by hand is enough; the game's\n"
        "; own ini is written too but the engine ignores it, and its own setres does nothing on\n"
        "; this build (both measured). dishonored_vr_launch.txt beside the exe is a MIRROR of\n"
        "; these keys, rewritten whenever it disagrees - never edit it instead of this.\n"
        "; A fullscreen size must be a display mode the adapter lists (`res modes`) - or\n"
        "; VirtualMode=1 makes the proxy advertise it and create the fullscreen device windowed\n"
        "; with the backbuffer kept, the route to the eye's own near-square size.\n"
        "; `res <W>x<H>[f|w]` asks live; F10 Display has the picker and the verdict\n"
        "; (`res: HONOURED` reads the capture, never the requested number).\n"
        "; 41.1 (session 9): the values the headset judged (Quest 3 through VirtualDesktopXR,\n"
        "; 2026-09-03/04) - the runtime's recommended per-eye size, asked on the game's command\n"
        "; line and ADVERTISED by the proxy so the game creates it (VirtualMode=1; without it the\n"
        "; game falls back to a display mode and the picture is soft). Another headset wants its\n"
        "; own size: the F10 Display picker writes these three for the next launch, and\n"
        "; RenderWidth=0 RenderHeight=0 asks for nothing at all (the game's own size).\n"
        "; 41.1 (session 15, 2026-09-04): 2750x2850 with the HEADSET AT 90 Hz. This pair is one\n"
        "; setting, not two. The tick at this size is ~11.3 ms and the 90 Hz display period is\n"
        "; 11.11 ms, so the pair schedule lands 1.00-1.02 display slots per frame and the\n"
        "; ghosting on a head turn is gone (judged on a Quest 3 over VDXR: none reported at\n"
        "; 90 Hz, still reported at 120). The SAME size at 120 Hz beats at 1.05-1.11 slots and\n"
        "; ghosts badly - the fault was never the resolution, it was the tick not dividing into\n"
        "; the display period. If you change one, check `stereo: rate` for the other.\n"
        "RenderWidth=2750\n"
        "RenderHeight=2850\n"
        "RenderFullscreen=1\n"
        "VirtualMode=1\n"
        "; FovLever WRITES the game camera FOV on every script dispatch (0 = off).\n"
        "; FovLever writes the game's camera FOV every tick (40..160; 0 = off, the game's\n"
        "; own FOV). 130 filled the old side-by-side render vertically; the mono screen\n"
        "; shows the frame as the game draws it, so it ships off.\n"
        "FovLever=0\n"
        "[Mode]\n"
        "; GamepadOnly=1 makes the VR controllers behave as a plain gamepad:\n"
        "; hands, hand mesh, motion aim, motion melee, motion crouch and\n"
        "; controller Blink aim are all off, and no hand or weapon model is\n"
        "; scaled. Head tracking, positional tracking and the FOV lever keep\n"
        "; working. Default 1 while the render is being fitted - set 0 to\n"
        "; get the motion controls back.\n"
        "GamepadOnly=0\n"
        "[VR]\n"
        "DesktopMirrorOff=1\n"
        "DesktopMirrorStrictOff=1\n"
        "ReduceDesktopPresent=1\n"
        "; Runtime=auto tries the 32-bit OpenXR runtime the system registers (Virtual\n"
        "; Desktop's VDXR, Oculus) and falls back to the bundled SteamVR shim\n"
        "; (dvr_steamvr32.dll) when there is none; native|steamvr force one.\n"
        "Runtime=auto\n"
        "; XrRuntimeJson= a runtime manifest for this launch (the simulator, or a\n"
        "; Steam launch that cannot carry XR_RUNTIME_JSON). Empty = the loader's choice.\n"
        "XrRuntimeJson=\n"
        "XrHaptics=1\n"
        "; FpsCap pins the game to a rate (0 = off): 72 with VD at 72 Hz, 45 at 90.\n"
        "FpsCap=0\n"
        "; DesktopEyeSource=tag|draw: draw pins by current backbuffer identity (VR-76).\n"
        "; Live A/B: desktopeye draw|tag. tag is the legacy pin, which leaks the other eye under shared capture.\n"
        "DesktopEyeSource=draw\n"
        "DisableBadApiLayers=1\n"
        "[Paths]\n"
        "; DataDir= where the harness files go (command.txt, status.json, dumps, the\n"
        "; shim manifest). Empty = %%LOCALAPPDATA%%\\DishonoredVR. Set it to a folder the\n"
        "; game and the tools both see for real (docs/VERIFICATION.md gotcha 14).\n"
        "DataDir=D:\\dvr-data\n"
        "[Controllers]\n"
        "; A jump, B stealth, X interact, Y lean/adrenaline.\n"
        "; Menu tap pauses; modifier+menu (or hold menu) opens journal.\n"
        "; Modifier:0 off,1 right thumbrest,2 R3,4 left thumbrest (3 retired).\n"
        "; Flip:0 left stick D-pad,1 right stick D-pad. Chord: X+Y as menu.\n"
        "DpadModifier=1\n"
        "DpadFlip=0\n"
        "PauseChord=1\n"
        "Enabled=1\n"
        "Deadzone=0.12\n"
        "Haptics=1\n"
        "; IndexTuning (VR-224): Index controller hand frames, hold trims, the empty left\n"
        "; hand's pose and the force-sensor grip. -1 = on when the launcher's headset is\n"
        "; Valve Index, Bigscreen Beyond or Vive Pro 2, 0 = off, 1 = on. Shim parts need Index\n"
        "; controllers.\n"
        "IndexTuning=-1\n"
        "[Turning]\n"
        "; SnapTurn=1 turns the view AND your body in fixed steps from the right stick;\n"
        "; 0 = the game's smooth turn. Live: `snapturn on|off`, or F10 > Controls > Turning.\n"
        "; A step fires once per push past SnapThreshold; the stick must fall under\n"
        "; SnapRearm before the next. SnapRepeatMs>0 repeats a held push every N ms.\n"
        "; Steps do not fire in menus, the power wheel, books, cinematics or keyholes,\n"
        "; where the stick keeps its usual job.\n"
        "SnapTurn=0\n"
        "SnapAngle=45\n"
        "SnapThreshold=0.6\n"
        "SnapRearm=0.3\n"
        "SnapRepeatMs=0\n"
        "[PosTrack]\n"
        "ZAccount=1\n"
        "ZAccountRoll=0\n"
        "; Stage 5: positional head tracking - lean/peek/crouch with your real\n"
        "; head. F4 = toggle, F5 = re-center to your current head position.\n"
        "; Lane=auto|vp|camera: where the offset is applied. vp patches the view-projection\n"
        "; matrix at c0 (the mono screen's path; attachments do not follow); camera writes it\n"
        "; into the camera field with the per-eye offset (game/dishonored/camera); auto = vp\n"
        "; on the mono screen, camera under a projection layer (stereo), where the head's raw\n"
        "; displacement drives the camera. `postrack lane <l>` switches live; `camera postest\n"
        "; <R> [U] [F]` measures the travel in uu.\n"
        "Lane=auto\n"
        "; Scale = game units per meter. 50 is GingasVR's shipped value and is\n"
        "; the baseline her release was tuned around, so it is what ships.\n"
        "; NOTE: 100 is the MEASURED value (1 uu = 1 cm), derived from the\n"
        "; game's own movement constants - 360 uu/s default and 540 uu/s\n"
        "; sprint. At 50 those are 7.2 and 10.8 m/s, a world-record sprint\n"
        "; pace for walking around; at 100 they are a 3.6 m/s jog and a\n"
        "; 5.4 m/s run. Try 100 as a SINGLE change once the rest of the\n"
        "; baseline is confirmed good, and keep whichever you prefer.\n"
        "; Too weak? raise it. Too strong/swimmy? lower it. MaxMeters clamps\n"
        "; how far it will follow.\n"
        "; If leaning LEFT moves the world the wrong way set FlipX=1.\n"
        "Enabled=1\n"
        "; 108 uu/m: headset-judged 2026-09-05, tuned live with PgDn from 98 and kept.\n"
        "Scale=108.0\n"
        "MaxMeters=0.80\n"
        "; EyeClamp (38.24) keeps the tracked eye inside the engine's own bounds. The\n"
        "; headset run of 2026-09-19 kept it OFF; 1 restores the clamp and its margin.\n"
        "EyeClamp=0\n"
        "FlipX=0\n"
        "[Neck]\n"
        "UprightPitchArc=1\n"
        "; The pitch pivot (41.1). A real head pitches about a point below and behind the\n"
        "; eyes, so looking up or down moves the eye on an arc. Mode=off|add|cancel, under\n"
        "; a projection layer only: off = the tracked displacement alone; add = the\n"
        "; modelled arc is ADDED (a rig without positional tracking, or an arc the tracking is\n"
        "; not supplying); cancel = the arc is SUBTRACTED, for an engine that already pitches\n"
        "; its camera about its own neck (then PivotBelowM/PivotBehindM hold the ENGINE's\n"
        "; numbers, measured by `camera pitchtest`). `neck off|add|cancel [below] [behind]`\n"
        "; switches live; F10 Comfort has the buttons and sliders. The defaults ARE the\n"
        "; engine's pivot, MEASURED 2026-09-03 on the simulator (0.321 m below, 0.062 m\n"
        "; behind: 17 cm of backward travel at +30 deg). cancel SHIPS: the headset run of\n"
        "; 2026-09-03 judged it right (the user); off and add stay as the live A/B. A human\n"
        "; neck for `add` is about 0.11 / 0.09.\n"
        "Mode=cancel\n"
        "PivotBelowM=0.321\n"
        "PivotBehindM=0.062\n"
        "CrouchPivotBelowM=0.000\n"
        "CrouchPivotBehindM=0.000\n"
        "RollArc=0\n"
        "StanceBlendMs=150\n"
        "[Crosshair]\n"
        "; VR-57: visual controller guide only; shots and native reticle unchanged.\n"
        "; Dot/beam share one runtime AIM-pose ray. Fixed distance, no surface trace.\n"
        "; Live: F10 Aim, or crosshair dot|laser on|off, hand left|right.\n"
        "Dot=1\n"
        "Laser=0\n"
        "Hand=left\n"
        "DistanceM=8.000\n"
        "SizeDeg=0.500\n"
        "; VR-141: the reticle colour, 0..255 each (white). Live: F10 HUD tab.\n"
        "ColorR=255\n"
        "ColorG=255\n"
        "ColorB=255\n"
        "; VR-189: one reticle position for everything but the pistol and the crossbow\n"
        "; (any ammo, any upgrade), degrees in the controller frame: +X right, +Y up.\n"
        "; The aim follows the dot. Live: F10 HUD tab, Reticle.\n"
        "OtherItemsX=0.00\n"
        "OtherItemsY=-48.00\n"
        "; Reserved; hiding the game reticle is not implemented in this step.\n"
        "; BothPoses=1 draws the GRIP pose ray beside the AIM pose ray, at 60\n"
        "; size, so a headset can name which one lies along the controller.\n"
        "BothPoses=0\n"
        "; VR-57 test 1: ControlDot=1 draws a HEAD-anchored dot straight ahead of the\n"
        "; view at 1.50 m and at DistanceM, with no controller anywhere in it. Both\n"
        "; must land on one screen point, and that point on the centre of the game's\n"
        "; own rendered image. Off the centre means the projection layer is misaligned\n"
        "; with the world it carries; on the centre puts the controller ray back under\n"
        "; suspicion. `crosshair control on|off` switches it live.\n"
        "ControlDot=0\n"
        "HideGame=0\n"
        "[MotionAim]\n"
        "; Stage 7.3: hand-aimed projectile weapons (crossbow bolts, pistol\n"
        "; bullets, grenades). After you pull the fire trigger, the freshly\n"
        "; spawned projectile is redirected along your controller's ray.\n"
        "; Hand: which controller aims (left = Corvo's gun hand). PitchOffsetDeg\n"
        "; tilts the ray down from the controller's raw pose toward a natural\n"
        "; point (tune live: PageUp = shots land higher, PageDown = lower,\n"
        "; 5 deg steps; the log prints the value - copy your favorite here).\n"
        "; FlipRight/FlipUp=1 mirror the ray if left/right or up/down aim is\n"
        "; reversed. End key = toggle on/off live.\n"
        "Enabled=0\n"
        "Hand=left\n"
        "PitchOffsetDeg=40\n"
        "WindowMs=1200\n"
        "MaxDistUU=900\n"
        "FlipRight=0\n"
        "FlipUp=0\n"
        "[Aim]\n"
        "; VR-57 step 2, READ-ONLY. SeamProbe=1 samples the game's own aim-assist\n"
        "; cache four times a second and logs it (aimseam: lines): the tick tag,\n"
        "; whether a target was found, its world position and direction, the\n"
        "; projected screen point, and how far that direction sits from the view\n"
        "; and from the controller ray. It answers where the shot's direction\n"
        "; comes from before anything writes it. Nothing the game reads is\n"
        "; written. SeamVerbose=1 also logs the instances whose tick tag is not\n"
        "; moving (the NPC contexts), which is the control.\n"
        "SeamProbe=0\n"
        "SeamVerbose=0\n"
        "; DriveFromHand=1 (VR-57 step 3) WRITES that cache from the controller\n"
        "; ray instead of reading it: the found flag, the aim position and the aim\n"
        "; direction, leaving the tick tag, the projected screen point and the\n"
        "; tracking flag alone. It is the test of whether the fire path reads this\n"
        "; cache at all - if the bolt still follows the crosshair, it does not.\n"
        "; DriveDistanceUU is how far along the ray the written aim point sits.\n"
        "DriveFromHand=0\n"
        "DriveDistanceUU=800\n"
        "; ShotProbe (VR-57 Phase 1) measures what the fired bolt actually did against\n"
        "; what the controller asked for. READ-ONLY: it never writes to the game. The\n"
        "; number it exists to print is the MISS at the plane of the visible dot, in\n"
        "; units and metres - not the angle between the bolt and the ray, because a bolt\n"
        "; launched parallel to the ray from a muzzle offset from the controller misses\n"
        "; the dot by the whole transverse gap at zero angle. Works with DriveFromHand\n"
        "; either way, and the drive-off run is the baseline.\n"
        "ShotProbe=0\n"
        "; FollowHandTrim (VR-57): transport the hand trim onto the published aim ray,\n"
        "; so tuning the hand with the numpad carries the dot, the beam and the shot\n"
        "; with it instead of leaving them on the physical controller. The whole ray\n"
        "; moves - it rotates about the untrimmed palm origin and then takes the\n"
        "; trim translation - so it stays attached to the hand the way the weapon does.\n"
        "; It is NOT a measured barrel axis: any baseline offset between the AIM pose\n"
        "; and the barrel is preserved. Off returns the AIM-pose ray untouched.\n"
        "FollowHandTrim=1\n"
        "; ModelRay: measure the loaded bolt geometry; overrides FollowHandTrim.\n"
        "ModelRay=1\n"
        "; FireWatch (VR-57 Phase B) records named script dispatches - anything whose\n"
        "; name mentions Aim, Fire, Shoot, Launch, Projectile or ViewPoint - and prints\n"
        "; the ones preceding each scored bolt, with the caller that made them. READ-ONLY.\n"
        "; The crossbow fire path is native, but Pawn.GetBaseAimRotation is a script\n"
        "; event, and a native caller reaching a script event goes through ProcessEvent -\n"
        "; so if the shot asks the pawn for an aim, the ask is visible by name. An empty\n"
        "; list is a real answer: the seam is wholly native. Needs ShotProbe=1.\n"
        "FireWatch=0\n"
        "; Native crossbow launch direction, converging from the muzzle to the controller dot.\n"
        "; Independent of the old HUD cache drive and MotionAim; live toggle in F10 Aim.\n"
        "FireFromHand=1\n"
        "; SourceProbe=1 (VR-166) names every object that asks the shared power-aim helper\n"
        "; (the one Blink uses) for a vector, and how far that vector sits off the view.\n"
        "; READ-ONLY. It answers which powers and thrown items one seam could aim by hand.\n"
        "SourceProbe=1\n"
        "; InteractFromHand=1 (VR-166): what you can pick up, open or use is chosen along the\n"
        "; weapon ray instead of your view. The engine still traces and validates; 0 = head.\n"
        "InteractFromHand=1\n"
        "; ThrowFromHand=1 (VR-166): grenades leave along the weapon ray instead of your view.\n"
        "; The spawn point, speed and arc stay the game's; 0 = head.\n"
        "ThrowFromHand=1\n"
        "; GadgetFromHand=1 (VR-166): spring razors are placed along the weapon ray; 0 = head.\n"
        "GadgetFromHand=1\n"
        "; CarryThrowFromHand=1 (VR-181): a carried bottle, rock or crate is thrown along the\n"
        "; weapon ray instead of your view. Release point, speed and spin stay the game's; 0 = head.\n"
        "CarryThrowFromHand=1\n"
        "; CarryThrowLeftTrigger=1 (VR-181): while carrying, the LEFT trigger throws and the right\n"
        "; trigger does the left's job; 0 = the game's layout (the right trigger throws).\n"
        "CarryThrowLeftTrigger=1\n"
        "; CarryHoldAtHand=1 (VR-181): a carried object is held at your hand instead of in front of\n"
        "; your view, CarryHoldForwardCm ahead of it along the weapon ray (negative pulls it in,\n"
        "; -40..60); 0 = the game's hold. CarryHoldRotate=1 turns it with your wrist.\n"
        "CarryHoldAtHand=1\n"
        "; Where it sits in the hand, in the hand's own frame: cm forward/right/up and a trim in\n"
        "; degrees (pitch/yaw/roll). CarryHoldWorldDepth=1 draws it in the world, not the weapon layer.\n"
        "CarryHoldForwardCm=-20.6\n"
        "CarryHoldRightCm=8.1\n"
        "CarryHoldUpCm=-28.1\n"
        "CarryHoldPitch=3.9\n"
        "CarryHoldYaw=12.7\n"
        "CarryHoldRoll=-27.6\n"
        "CarryHoldRotate=1\n"
        "CarryHoldWorldDepth=1\n"
        "; CarryHoldKeepPickupAngle=0: the object sits the same way in the hand every time (then the\n"
        "; trims); 1 keeps whatever angle it was picked up at.\n"
        "CarryHoldKeepPickupAngle=0\n"
        "; CarryHoldAnchor=1: place it from the GAME camera; 0 = from the last render sample (A/B).\n"
        "CarryHoldAnchor=1\n"
        "; CarryHoldReticleAnchor=1: the hold stays put when [Crosshair] OtherItemsX/Y is re-tuned. It\n"
        "; is built on the reticle it was tuned at, CarryHoldReticleX/Y (degrees); 0 = it rides the live\n"
        "; reticle and moves with every reticle change. Live: F10 Aim.\n"
        "CarryHoldReticleAnchor=1\n"
        "CarryHoldReticleX=-3.60\n"
        "CarryHoldReticleY=-37.20\n"
        "; PowersFromHand=1 (VR-44): Windblast, Possession and Devouring Swarm aim along the\n"
        "; weapon ray instead of your view; 0 = head.\n"
        "PowersFromHand=1\n"
        "; HandRayGameAnchor=1: the hand ray starts from the GAME camera. 0 = from the last render sample,\n"
        "; which jumps between eyes and made the grab prompt flicker at the edge of reach (A/B only).\n"
        "HandRayGameAnchor=1\n"
        "PropWatch=0\n"
        "InteractFocus=0\n"
        "[HandTracking]\n"
        "; Build 30.6: weapon tracking starts by itself a few seconds after\n"
        "; you are in-game with both controllers tracked - no F6+HOME needed\n"
        "; (F6/HOME still work manually). If the neutral pose captured badly,\n"
        "; hold the controllers naturally and press END to recalibrate\n"
        "; everything; HOME still toggles tracking off/on.\n"
        "AutoStart=1\n"
        "DelaySec=4\n"
        "; Depth = hands push/pull the weapon. WristRoll = weapon rolls with\n"
        "; your wrist (off by default).\n"
        "Depth=1\n"
        "WristRoll=0\n"
        "[Melee]\n"
        "; Swing the RIGHT controller to attack with the sword. Your trigger still\n"
        "; attacks as before: a swing ADDS a short press of the attack, it never\n"
        "; replaces the trigger. CooldownMs paces combos (one swing = one strike).\n"
        "; F10 > Controls > Motion sword shows your PEAK hand speed live; tune from it.\n"
        ";\n"
        "; Detector picks how a swing is recognised:\n"
        ";   sustain  a swing must stay above SwingSpeed (m/s) for SwingMs AND travel\n"
        ";            SwingDistM, then the attack is pressed for HoldMs. The original\n"
        ";            detector, kept so the two can be compared in the headset.\n"
        ";   edge     the attack fires the instant your hand speed crosses EdgeSpeed,\n"
        ";            so the game's own wind-up lands the hit where your arm is going.\n"
        ";            Your head's movement is subtracted (HeadRel), so turning your\n"
        ";            body is not a swing. Needs the sword in your hand (RequireSword),\n"
        ";            a gameplay view, no grip held and the F10 overlay closed.\n"
        "; edge is the default since it was judged in a headset (2026-09-20). Switch\n"
        "; live with the command `swing mode sustain` or in F10.\n"
        "Enabled=1\n"
        "Detector=edge\n"
        "SwingSpeed=1.8\n"
        "SwingMs=120\n"
        "SwingDistM=0.25\n"
        "HoldMs=220\n"
        "CooldownMs=200\n"
        "; edge: EdgeSpeed is the hand speed (m/s) that counts as a swing. Swings not\n"
        "; registering? lower it toward your PEAK. Attacking while you walk or reach?\n"
        "; raise it. RearmSpeed is how slow the hand must get before the next swing\n"
        "; can fire (never above 0.9 x EdgeSpeed); raise it if fast combos drop swings.\n"
        "; The log counts every hand movement by its peak speed (`swing: census`), the\n"
        "; ones that attacked and the ones that did not: EdgeSpeed belongs in the gap\n"
        "; between the two lists. EdgeTravelM makes a swing cover that many metres\n"
        "; before it can attack (0 = off); it delays a real swing, it never refuses one.\n"
        "; EdgeSpeedRev marks that the 3.6 -> 3.0 default change has been applied once.\n"
        "EdgeSpeed=3.0\n"
        "EdgeSpeedRev=1\n"
        "EdgeTravelM=0\n"
        "RearmSpeed=1.0\n"
        "; edge: the attack is pressed for PulseMs, and at least until the game has\n"
        "; read the pad PulseMinPolls times (a hitch can swallow a short press).\n"
        "PulseMs=120\n"
        "PulseMinPolls=2\n"
        "HeadRel=1\n"
        "; Median=1 decides on the middle of the last three speed readings, so one bad\n"
        "; tracking sample can never swing the sword. Costs one frame. 0 = raw.\n"
        "Median=1\n"
        "RequireSword=1\n"
        "; Output is the pad input a swing presses: rt (right trigger) or rb (right\n"
        "; shoulder). Leave rt unless the log says `swing: NOT HONOURED ... the game\n"
        "; BLOCKED instead`, which means this install binds the attack the other way.\n"
        "Output=rt\n"
        "; After a swing the mod watches for the game to start the attack and logs\n"
        "; HONOURED or NOT HONOURED; HonourMs is how long it watches. HonourHaptic=1\n"
        "; adds a second, softer tick when the game confirms it.\n"
        "HonourMs=600\n"
        "HonourHaptic=1\n"
        "Haptic=1\n"
        "; The sneak kill (needs Detector=edge). Creeping up behind a guard you stab,\n"
        "; you do not slash: Stab=1 makes a deliberate STAB of the sword hand (the\n"
        "; motion is StabStyle, below) press the attack while you are crouched, even\n"
        "; when it is slower than a swing. A FAST stab already counts as a swing. The\n"
        "; game decides what the attack becomes: behind an unaware guard\n"
        "; it is the stealth kill. It is its own shape on purpose: a thrust is slower\n"
        "; than any slash, and lowering the slash speed instead would turn every reach\n"
        "; into an attack in the middle of a stealth approach.\n"
        ";   StabSpeed    how fast the hand must EXTEND (m/s) to start a thrust\n"
        ";   StabTravelM  how far it must extend (m) within StabWindowMs\n"
        ";   StabRatio    how straight: extension gained / path travelled (0-1)\n"
        ";   StabForward  how forward: 1 = exactly where you face, 0 = sideways\n"
        ";   StabArm      sneak = only while crouched; always = standing too (testing)\n"
        "; Stab not registering? the log line `stab: REJECTED` names the bar it missed\n"
        "; and by how much - lower that one. ShoulderRightM/DownM/BackM place your right\n"
        "; shoulder from your head, for a build far from average.\n"
        "; StabStyle is the MOTION. plunge = the blade is in a reverse grip, so raise your\n"
        "; fist to about your shoulder and drive it down, a little forward - what Corvo\n"
        "; does from behind. It must START high (StabStartBelowM = how far below your\n"
        "; shoulder line it may start), which is what tells it from reaching for loot.\n"
        "; thrust = straight out from the shoulder, the way you face. With plunge,\n"
        "; StabForward is how closely the move follows that down-and-forward line.\n"
        "Stab=1\n"
        "StabArm=sneak\n"
        "StabStyle=plunge\n"
        "StabStartBelowM=0.05\n"
        "StabSpeed=1.5\n"
        "StabTravelM=0.20\n"
        "StabRatio=0.75\n"
        "StabForward=0.5\n"
        "StabWindowMs=400\n"
        "ShoulderRightM=0.17\n"
        "ShoulderDownM=0.22\n"
        "ShoulderBackM=0.04\n"
        "[Debug]\n"
        "; One-shot diagnostic, runs ~2 s after weapon tracking comes up and\n"
        "; writes to the log. Values: bones census graph ue3 view. Leave empty\n"
        "; for none. (Claude sets this remotely when a measurement is needed.)\n"
        "Probe=\n"
        "[HandRender]\n"
        "; Build 30.70 - THE render-time hand/weapon drive.\n"
        "; The drawn pose reaches the GPU as vertex constants at c6, three\n"
        "; registers per bone. We apply one shared rigid transform there, built\n"
        "; from your controller RELATIVE TO YOUR HEAD, so head motion cancels\n"
        "; analytically and the weapon stops drifting with your view.\n"
        "; Enabled=0 falls back to the old component drive.\n"
        "Enabled=1\n"
        "DriveArms=1\n"
        "DriveWeapon=1\n"
        "; Upload sizes that identify each rig. Both are driven by the SAME\n"
        "; transform, so which label lands on which size does not change how it\n"
        "; behaves - it only decides which one the WpnYaw/Pitch/Roll correction\n"
        "; applies to. The 30.69 sweep saw exactly three sizes on screen (36,\n"
        "; 144, 204); 204 is an NPC and is never touched. WHICH of 36 and 144 is\n"
        "; the sword is still open - settle it with the identifier in the F10\n"
        "; overlay, which wiggles one size at a time while you watch, then press\n"
        "; the assign button. 0 = drive nothing here.\n"
        "WeaponRegs=36\n"
        "ArmsRegs=144\n"
        "; Which controller drives the pair. Both rigs share it, so the hand\n"
        "; stays welded to the weapon.\n"
        "Hand=right\n"
        "; 0 = the rig pivots about the viewpoint, 1 = about your hand (spins in\n"
        "; place, like something actually held).\n"
        "PivotMix=1.00\n"
        "; Unreal units per metre of hand travel. 0 = follow [PosTrack] Scale so\n"
        "; hands and world stay the same size.\n"
        "ScaleUU=0\n"
        "MaxOffsetUU=120\n"
        "; 0 = raw pose. Raise toward 0.9 only if the hands look jittery.\n"
        "SmoothAlpha=0.00\n"
        "; Resting trim in rig space, unreal units: X forward, Y right, Z up.\n"
        "; THIS IS THE ONE THAT MATTERS. The drive assumes the game's rest hand\n"
        "; sits where your controller was when you pressed END; whatever is left\n"
        "; over stays glued to your head. Trim by minus the visible error and\n"
        "; the drift goes to zero (measured: 10 uu of error = 28 cm of swim per\n"
        "; 90 degrees of head turn).\n"
        "TrimX=0\n"
        "TrimY=0\n"
        "TrimZ=0\n"
        "; RotInvert=1 is the one fix if the weapon turns the WRONG WAY.\n"
        "; RotScale=0 removes rotation and leaves pure translation - use it to\n"
        "; tell which half of the drive is misbehaving before tuning anything.\n"
        "RotInvert=0\n"
        "RotScale=1.00\n"
        "; The pivot assumes the rig's origin is at your eye. If rotation swings\n"
        "; the arms from somewhere below you, slide it back (unreal units).\n"
        "PivotUp=0\n"
        "; If the weapon and the hand pull APART, the weapon's component axes\n"
        "; differ from the arms'. These degrees rotate the weapon's copy of the\n"
        "; transform to match. Tune them live in the F10 overlay.\n"
        "WpnYaw=0.0\n"
        "WpnPitch=0.0\n"
        "WpnRoll=0.0\n"
        "; Set from the tested machine's ini (VR-72): F10 panel and calibration keys.\n"
        "LTrimX=0.0\n"
        "RTrimX=0.0\n"
        "LTrimY=0.0\n"
        "RTrimY=0.0\n"
        "LTrimZ=0.0\n"
        "RTrimZ=0.0\n"
        "Weapon2Regs=0\n"
        "WeaponHand=1\n"
        "Weapon2Hand=0\n"
        "RightArmFirstBone=0\n"
        "RightArmLastBone=0\n"
        "ShowRings=0\n"
        "RingSizeMeters=0.045\n"
        "FollowHeadYaw=1.00\n"
        "FollowHeadPitch=0.00\n"
        "Axis0Source=2\n"
        "Axis0Flip=0\n"
        "Axis1Source=0\n"
        "Axis1Flip=0\n"
        "Axis2Source=1\n"
        "Axis2Flip=0\n"
        "RouteByDrawOrder=0\n"
        "DrawOrderHands=-1,-1,-1,-1,-1,-1,-1,-1\n"
        "[Hands]\n"
        "RoundedWrist=1\n"
        "RoundedWristDepth=0.570\n"
        "PaletteEyeMenuHalfStep=1\n"
        "CrawlTuck=1\n"
        "CrawlTuckCamera=0\n"
        "AttachViewLens=1\n"
        "AttachScaleTrace=1\n"
        "AttachKeepOnMenu=1\n"
        "AttachKeepOnNote=1\n"
        "; PoseLag (41.2, VR-68): which generation of the head the hand and the weapon are\n"
        "; normalised against. The engine renders a frame from the head TWO locate generations\n"
        "; back - measured, the rendered camera motion matched that sample to 0.119 deg against\n"
        "; 1.19 at the freshest, over 4085 moving frames - so a hand placed against the FRESHEST\n"
        "; head is planted in a view built from a different one, and the leftover is two\n"
        "; generations of head rotation. That is the weapon judder, and it is the same fault the\n"
        "; world had before [Pace] Lag=2 fixed it, one layer down.\n"
        "; 2 is confirmed in a headset by a reversing A/B/A/B; 0 is the pre-VR-68 behaviour.\n"
        "; PoseLagAb=1 walks 0/2/0/2 on 15 s segments so the comparison can be felt again.\n"
        "PoseLag=2\n"
        "PoseLagAb=0\n"
        "; Set from the tested machine's ini (VR-72): F10 panel and calibration keys.\n"
        "Enabled=1\n"
        "FromControllers=1\n"
        "WorldSpace=0\n"
        "WorldRotation=0\n"
        "Position=1\n"
        "Rotation=0\n"
        "RemoveMeshRotation=0\n"
        "CameraLookAtStrength=1.00\n"
        "LeftControlStrength=1.00\n"
        "RightControlStrength=1.00\n"
        "CrouchOffLFwd=0.0\n"
        "CrouchOffLRight=0.0\n"
        "CrouchOffLUp=0.0\n"
        "CrouchOffRFwd=0.0\n"
        "CrouchOffRRight=0.0\n"
        "CrouchOffRUp=0.0\n"
        "PerStanceTrim=1\n"
        "BlockOffLFwd=0.0\n"
        "BlockOffLRight=0.0\n"
        "BlockOffLUp=0.0\n"
        "BlockOffRFwd=0.0\n"
        "BlockOffRRight=0.0\n"
        "BlockOffRUp=0.0\n"
        "BlockTrim=1\n"
        "CrouchSource=3\n"
        "CrouchDropUU=20\n"
        "CrouchHoldMs=250\n"
        "CrouchDiag=0\n"
        "CrouchToggle=0\n"
        "CrouchButtonMask=8192\n"
        "CrouchMaskVer=2\n"
        "GraftRotation=0\n"
        "GraftRotSpace=0\n"
        "GraftHeadComp=1\n"
        "GraftAimAbs=1\n"
        "GraftHeadFollowYaw=1.50\n"
        "GraftHeadFollowPitch=1.50\n"
        "RotSignYaw=1\n"
        "RotSignPitch=1\n"
        "AddToAnim=1\n"
        "ScaleUU=50.0\n"
        "ClampUU=120.0\n"
        "Space=3\n"
        "Strength=1.00\n"
        "CounterHeadYaw=0.00\n"
        "HandSize=1.00\n"
        "WorldScaleUU=100\n"
        "RollGain=1.00\n"
        "LTrimFwd=0.0\n"
        "LTrimRight=0.0\n"
        "LTrimUp=0.0\n"
        "RTrimFwd=0.0\n"
        "RTrimRight=0.0\n"
        "RTrimUp=0.0\n"
        "NeutralLRight=-0.1086\n"
        "NeutralLUp=-0.1632\n"
        "NeutralLFwd=0.3664\n"
        "NeutralRRight=0.1721\n"
        "NeutralRUp=-0.2383\n"
        "NeutralRFwd=0.3879\n"
        "NeutralSaved=1\n"
        "BoneQuery=0\n"
        "HandMoveTest=0\n"
        "DrawCensus=1\n"
        "MatCycle=0\n"
        "Palette=1\n"
        "PaletteAmount=20.0\n"
        "PaletteAxis=1\n"
        "PaletteHand=0\n"
        "PaletteYawFix=0\n"
        "PaletteFrameProbe=0\n"
        "PaletteWorld=1\n"
        "PaletteDepthRange=1\n"
        "PaletteEyeOffset=1\n"
        "PaletteEyePredictToggle=0\n"
        "PaletteEyeAlternate=0\n"
        "PaletteEyeFromMeasured=0\n"
        "PaletteEyeHunt=0\n"
        "PaletteCapture=0\n"
        "PaletteAbsolute=0\n"
        "PaletteDrive=0\n"
        "PaletteDriveGain=1.00\n"
        "PaletteStep=0\n"
        "PaletteRotate=1\n"
        "; AnchorBone=1 (VR-183): the palm is placed from the hand (wrist) bone, so finger animation\n"
        "; cannot swing the hand. 0 = the old choice, the bone most weighted on the palm patch.\n"
        "AnchorBone=1\n"
        "; RigidWrist=1 (VR-184): the wrist cut and cap stay rigid with the hand, so arm animation\n"
        "; cannot bend them; the fingers still animate. 0 = the game's own weights.\n"
        "RigidWrist=1\n"
        "WeaponId=0\n"
        "WeaponIdMs=1500\n"
        "PaletteFrameTol=0.0200\n"
        "GripLX=16.2230\n"
        "GripLY=53.3325\n"
        "GripLZ=-101.2634\n"
        "GripRX=82.9366\n"
        "GripRY=-31.1034\n"
        "GripRZ=-19.5772\n"
        "PaletteSweep=0\n"
        "PaletteSweepSeconds=3.0\n"
        "; The MEASURED MODEL AXIS (VR-57), and the grip it was measured against.\n"
        "; This is the crossbow bolt's own lengthwise axis in the palm frame - the barrel\n"
        "; line - so the guide and the shot sit on the weapon instead of on the bare\n"
        "; controller. It can only be MEASURED from a drawn crossbow bolt, so shipping it\n"
        "; means a first launch, or a save loaded with the pistol out, has a correct laser\n"
        "; immediately rather than none until the crossbow is equipped.\n"
        ";\n"
        "; It ships as a MATCHED PAIR with Grip* above: the grip defines the palm frame the\n"
        "; axis is expressed in, so a different grip makes this record meaningless and the\n"
        "; loader discards it (saying so, with both values) and measures again. Hand TRIM\n"
        "; changes are fine and need no new record - the frame is rebuilt from the current\n"
        "; trim every time, which is why tuning the hand carries the laser with it.\n"
        "ModelAxisLOX=-0.164994\n"
        "ModelAxisLDX=-0.153351\n"
        "ModelAxisLGX=16.2230\n"
        "ModelAxisLOY=0.232584\n"
        "ModelAxisLDY=0.735209\n"
        "ModelAxisLGY=53.3325\n"
        "ModelAxisLOZ=0.237439\n"
        "ModelAxisLDZ=0.660266\n"
        "ModelAxisLGZ=-101.2634\n"
        "GripLVersion=2\n"
        "GripLParity=-1\n"
        "GripRVersion=2\n"
        "GripRParity=-1\n"
        "TrimTZ=0.0120\n"
        "Adjust=1\n"
        "AdjStepT=1\n"
        "AdjStepR=3\n"
        "TrimLTX=0.0257\n"
        "TrimLRX=5.55\n"
        "TrimLTY=0.0124\n"
        "TrimLRY=7.52\n"
        "TrimLTZ=0.0478\n"
        "TrimLRZ=-1.20\n"
        "TrimRTX=0.0400\n"
        "TrimRRX=-42.00\n"
        "TrimRTY=0.0200\n"
        "TrimRRY=67.00\n"
        "TrimRTZ=0.0120\n"
        "TrimRRZ=3.00\n"
        "; PowerTrim=1: the LEFT hand uses its own trim, TrimLPT*/TrimLPR*, while it holds a power\n"
        "; (seeded from TrimL* when absent). The numpad left modes edit it while a power is out; F10\n"
        "; Hands has sliders for all three. 0 = one left trim for everything.\n"
        "PowerTrim=1\n"
        "TrimLPTX=0.0267\n"
        "TrimLPRX=9.28\n"
        "TrimLPTY=0.0120\n"
        "TrimLPRY=-11.20\n"
        "TrimLPTZ=0.0505\n"
        "TrimLPRZ=-8.36\n"
        "; AdjustInView=1: numpad and F10 steps move the hand along your view (right, forward, up, and\n"
        "; pitch/yaw/roll about them) instead of the tilted palm axes. The stored trim is unchanged in kind.\n"
        "AdjustInView=1\n"
        "AttachWeapons=1\n"
        "AttachSwordHand=1\n"
        "AttachCrossbowHand=0\n"
        "ModelScale=0.85\n"
        "AttachAngleTol=0.25\n"
        "AttachPosTol=1.00\n"
        "AttachMargin=4.0\n"
        "AttachMaxTry=3000\n"
        "AttachGhostFix=1\n"
        "AttachProbe=1\n"
        "AttachProbeBudget=400\n"
        "AttachSnapshotMaxMs=100\n"
        "BoneVisHide=0\n"
        "MatCensus=1\n"
        "MatAuto=0\n"
        "ArmSplit=1\n"
        "ArmSplitAuto=1\n"
        "ArmSplitMode=1\n"
        "ArmMeshPrims=4448\n"
        "ArmMeshVerts=2771\n"
        "WristScaleA=0.70\n"
        "WristScaleB=0.70\n"
        "WristPlane=1\n"
        "CutCap=1\n"
        "CutCapTwoSided=1\n"
        "PoseReport=1\n"
        "PaletteWeightTol=0.0200\n"
        "AttachCensus=1\n"
        "AttachSuppressUnplaced=1\n"
        "AttachViewModelUU=500\n"
        "AttachNearAngle=20.00\n"
        "AttachNearPos=30.00\n"
        "AttachNearMargin=1.50\n"
        "AttachDropUncorrected=1\n"
        "AttachEquippedMembers=1\n"
        "AttachVerifyInstance=1\n"
        "AttachHeldMaxPresents=2\n"
        "AttachRequireFreshRef=0\n"
        "AttachRequireLiveMember=0\n"
        "AttachVetoReleasesBuffers=1\n"
        "AttachInstanceVetoRelaxed=1\n"
        "AttachRefMaxPresents=2\n"
        "AttachRigRadius=200\n"
        "AttachPassRadius=60\n"
        "WristEdge=3\n"
        "WristStep=1\n"
        "WristAxis=0\n"
        "WristCutA=-10.00\n"
        "WristCutB=-10.00\n"
        "[Blink]\n"
        "; Set from the tested machine's ini (VR-72): F10 panel and calibration keys.\n"
        "ControllerAim=1\n"
        "Marker=1\n"
        "ReachMode=0\n"
        "ReachUU=0\n"
        "NearUU=150\n"
        "PitchNearDeg=-55.0\n"
        "PitchFarDeg=-5.0\n"
        "MarkerPullbackUU=60\n"
        "AimAtSource=1\n"
        "UseAimRay=1\n"
        "OptVer=3\n"
        "[Overlay]\n"
        "UiScale=1.00\n"
        "; Set from the tested machine's ini (VR-72): F10 panel and calibration keys.\n"
        "; Level: which F10 controls are shown. basic = player settings, advanced = preference detail,\n"
        "; debug = fixes that should stay on, A/B levers and instruments. Live: the selector at the top.\n"
        "Level=basic\n"
        "; ReticleWhileOpen=1: the reticle stays on while the F10 panel is up and hides where the panel\n"
        "; covers it, so it can be tuned beside the panel. 0 = off while the panel is up (as before).\n"
        "ReticleWhileOpen=1\n"
        "[VRHands]\n"
        "; Set from the tested machine's ini (VR-72): F10 panel and calibration keys.\n"
        "Enabled=0\n"
        "HideGameArms=0\n"
        "Scale=1.00\n"
        "LeftModel=2\n"
        "RightModel=1\n"
        "FollowEquipped=1\n"
        "HideStaticParts=1\n"
        "HideStaticRadiusUU=70\n"
        "M1Yaw=0.0\n"
        "M1PosX=0.0000\n"
        "M1Pitch=0.0\n"
        "M1PosY=0.0000\n"
        "M1Roll=0.0\n"
        "M1PosZ=0.0000\n"
        "M2Yaw=0.0\n"
        "M2PosX=0.0000\n"
        "M2Pitch=0.0\n"
        "M2PosY=0.0000\n"
        "M2Roll=0.0\n"
        "M2PosZ=0.0000\n"
        "M3Yaw=0.0\n"
        "M3PosX=0.0000\n"
        "M3Pitch=0.0\n"
        "M3PosY=0.0000\n"
        "M3Roll=0.0\n"
        "M3PosZ=0.0000\n"
        "M4Yaw=0.0\n"
        "M4PosX=0.0000\n"
        "M4Pitch=0.0\n"
        "M4PosY=0.0000\n"
        "M4Roll=0.0\n"
        "M4PosZ=0.0000\n"
        "M5Yaw=0.0\n"
        "M5PosX=0.0000\n"
        "M5Pitch=0.0\n"
        "M5PosY=0.0000\n"
        "M5Roll=0.0\n"
        "M5PosZ=0.0000\n"
        "M6Yaw=0.0\n"
        "M6PosX=0.0000\n"
        "M6Pitch=0.0\n"
        "M6PosY=0.0000\n"
        "M6Roll=0.0\n"
        "M6PosZ=0.0000\n"
        "M7Yaw=0.0\n"
        "M7PosX=0.0000\n"
        "M7Pitch=0.0\n"
        "M7PosY=0.0000\n"
        "M7Roll=0.0\n"
        "M7PosZ=0.0000\n"
        "LPosX=0.0000\n"
        "LYaw=0.0\n"
        "LPosY=0.0000\n"
        "LPitch=0.0\n"
        "LPosZ=0.0000\n"
        "LRoll=0.0\n"
        "RPosX=0.0000\n"
        "RYaw=0.0\n"
        "RPosY=0.0000\n"
        "RPitch=0.0\n"
        "RPosZ=0.0000\n"
        "RRoll=0.0\n"
        "CalibTriangle=0\n"
        "[HeadInject]\n"
        "; (legacy, unused)\n"
        "FlipYaw=1\n"
        "FlipPitch=1\n"
        "FlipRoll=1\n"
        "[Menu]\n"
        "SurfaceGuard=1\n"
        "UiKeepOnMenu=1\n"
        "NoteFastMono=1\n"
        "UiFlags=1\n"
        "PawnFromController=1\n"
        "CacheNameLookups=1\n"
        "\n"
        "; GameOptsOnStart=1 reads the GAME's own option settings into the log once, a few\n"
        "; seconds after gameplay starts. It exists because the tester plays in a headset and\n"
        "; cannot reach a prompt, so a diagnostic that has to be asked for never runs at all.\n"
        "; Those twelve settings (kill cam, head bob, crosshair, auto aim, light shafts, ...)\n"
        "; are NOT in the game's inis - they live in Steam's OPTIONS.sav profile blob, and the\n"
        "; [SystemSettings] entries are only a mirror of it which has already been caught\n"
        "; disagreeing with the menu. The line prints both sides, so a disagreement shows.\n"
        "; Read-only, one burst, then silent. The F10 Advanced tab re-runs it on demand.\n"
        "[GameOptions]\n"
        "; Restore the VR preset each launch; F10 Advanced can disable this.\n"
        "DefaultsAtStartup=1\n"
        "\n"
        "[Diagnostics]\n"
        "GcFaultDump=1\n"
        "GameOptsOnStart=1\n"
        "; CamModProbe=1 (VR-165) names which camera modifier is still weighted while the\n"
        "; camera swings. A tester came off a chain and the camera kept swinging for ~23 s;\n"
        "; measured, our own writer was flat and the GAME camera oscillated, so the owner is\n"
        "; a modifier that is not releasing. Read-only, but it ships OFF: its census rebuilt\n"
        "; the whole live-object table every 500 ms on the game thread (VR-204: hitches).\n"
        "CamModProbe=0\n"
        "; SwingTrace=1 (VR-165) samples the render camera once per PRESENT and dumps the raw\n"
        "; samples when a big excursion trips. It derives no frequency on purpose: two earlier\n"
        "; instruments each reported a rate that was really their own sampling rate.\n"
        "SwingTrace=1\n"
        "\n"
        "[Cine]\n"
        "LockPitch=1\n"
        "LockFov=1\n"
        "StereoState=1\n"
        "PossessionStereo=1\n"
        "HideBorders=1\n"
        "HeadLook=1\n"
        "SpecialHeadLook=1\n"
        "Trace=0\n"
        "LockRoll=1\n"
        "; SkipHoldMs (VR-165): during a cutscene the pad is parked - sticks and triggers to\n"
        "; zero, buttons dropped - so a stray press cannot eject you from a scripted scene.\n"
        "; That also dropped the game's own hold-to-skip, and a tester could not skip the\n"
        "; chair scene at all. A button HELD this long passes through the park; a pulse never\n"
        "; can, which is the protection 38.65 actually wanted. 0 = park everything, as before.\n"
        "SkipHoldMs=300\n"
        "\n"
        "; The game's own camera shake (VR-172). On a monitor a bobbing, kicking camera\n"
        "; is feedback; in a headset it is the view moving without your head. Suppress=1\n"
        "; removes it. Each line below set to 1 lets the game move the camera for that\n"
        "; again: Walk (bob and roll), Fire (the weapon kick), Landing (landing, physical\n"
        "; impulses), Hits (hits and jolts), Generic (general shake and rumble). Smoother\n"
        "; is not a shake: it glides the camera over stairs and steps, and ships at 1. Live:\n"
        "; `camshake on|off`, `camshake allow <name> on|off`, or F10 > Controls > Camera\n"
        "; shake. `camshake status` says which were measured and which are by name only.\n"
        "; PopSmoothing (VR-165): 1 lets the game glide the camera back after a collision\n"
        "; pop (a chain release, a knockback). In VR that glide reads back the mod's own\n"
        "; offset, never finishes, and leaves the view lifted and swinging; 0 (the\n"
        "; default) snaps instead. Live: `camshake allow popsmooth on|off`.\n"
        "[CameraShake]\n"
        "Suppress=1\n"
        "Walk=0\n"
        "Fire=1\n"
        "Landing=1\n"
        "Hits=0\n"
        "Generic=1\n"
        "Smoother=1\n"
        "PopSmoothing=0\n"
        "\n"
        "[Rain]\n"
        "Recovery=1\n"
        "Hide=0\n"
        "Trace=1\n"
        "Distance=-1\n"
        "\n"
        "; The sword's swing trail (VR-171). The game draws a swoosh along the path of\n"
        "; its own attack animation; in the headset the blade is in YOUR hand, so the\n"
        "; ribbon hangs where the blade is not. Hide=1 withholds it for the player's\n"
        "; swings only (enemy trails are untouched). Live: `swordtrail on|off`, or\n"
        "; F10 > Controls > Motion sword. Template is part of the name of the particle\n"
        "; effect to hide; `swordtrail census` then one swing names what an attack adds.\n"
        "[SwordTrail]\n"
        "Hide=1\n"
        "Trace=1\n"
        "Template=Sword_Trail\n"
        "\n"
        "[Lens]\n"
        "Distance=18\n"
        "KeepSize=0\n"
        "Trace=1\n"
        "FollowHead=1\n"
        "RainStrength=100\n"
        "\n"
        "[Mirror]\n"
        "Enabled=1\n"
        "Assets=Wpn_PlyGunElite,crossbow_01\n"
        "Eps=0.25\n"
        "FillRadius=1.5\n"
        "BackFaces=0\n"
        "Caps=1\n"
        "CoverTol=0.3\n"
        "Straddle=2.0\n"
        "DepthBias=1\n"
        "\n"
        "; Drop takedowns from above. An attack pressed in the air before the game has\n"
        "; found the guard below would be an ordinary slash; Assist=1 holds it up to\n"
        "; HoldMs for the game to find one. Fallback=1 still attacks on landing if none\n"
        "; is found. ReachScale multiplies how far ahead along the fall the game looks\n"
        "; (1.00 = the original game; 2.00 is the headset-tuned value). Live:\n"
        "; `drop status|on|off|hold|fallback|reach`.\n"
        "[DropTakedown]\n"
        "Assist=1\n"
        "HoldMs=420\n"
        "Fallback=1\n"
        "ReachScale=2.00\n"
        "\n"
        "[Anim]\n"
        "DropWatch=1\n"
        "MoveTrace=0\n"
        "MantleHandBack=1\n"
        "; HandAnimMelee: a TRIGGER sword attack plays the game's swing on the tracked hand\n"
        "; and returns it to the controller. A physical swing (the motion sword) never does:\n"
        "; your arm is the animation. HandAnimMeleeSwing=1 hands physical swings back too.\n"
        "; HandAnimMeleeRev=1 marks an ini that has seen the 0 -> 1 default move; leave it.\n"
        "HandAnimMelee=1\n"
        "HandAnimMeleeRev=1\n"
        "HandAnimMeleeSwing=0\n"
        "; HandAnimMeleeBothHands=1 makes the left hand follow the clip too; 0 keeps it on the\n"
        "; controller (the trigger clip is right-handed).\n"
        "HandAnimMeleeBothHands=0\n"
        "HandAnimFire=0\n"
        "CinematicHandBack=1\n"
        "StateWatch=1\n"
        "HandBack=1\n"
        "ReleaseMs=250\n"
        "HandBackBlendMs=150\n"
        "; Arms.<lane>.<state> and Action.<lane>.<state> are the per-state handback rules\n"
        "; (VR-88). 1 hands the state back to the game's own animation, 0 keeps the VR\n"
        "; hands driving it. Lane 0 is the master state, 1 and 2 the upper-body states.\n"
        "; These are the rules the 2026-09-19 headset run was played with.\n"
        "Arms.0.StatePlayerMasterAction=1\n"
        "Arms.0.StatePlayerMasterWalk=0\n"
        "Arms.0.StatePlayerMasterLeaning=0\n"
        "Arms.0.StatePlayerMasterSwim=0\n"
        "Arms.0.StatePlayerMasterJump=0\n"
        "Arms.0.StatePlayerMasterFalling=0\n"
        "Arms.0.StatePlayerMasterHolePeeking=0\n"
        "Arms.0.StatePlayerMasterSlide=0\n"
        "Arms.0.StatePlayerMasterMantle=0\n"
        "Arms.1.StatePlayerUpperIdle=0\n"
        "Arms.1.StatePlayerMeleeAttack=0\n"
        "Arms.1.StatePlayerGrabMovable=1\n"
        "Arms.1.StatePlayerBlock=0\n"
        "Arms.2.StatePlayerUpperIdle=0\n"
        "Arms.2.StatePlayerGrabMovable=0\n"
        "Action.0.StatePlayerMasterJump=1\n"
        "Action.0.StatePlayerMasterMantle=1\n"
        "Action.1.StatePlayerBlock=1\n"
        "Arms.0.StatePlayerMasterAssassinate=1\n"
        "[Draws]\n"
        "; The HUD draw census (core/gfx/hud_class), default OFF. Census=1 buckets every draw in\n"
        "; a present by entry point, render target, viewport, depth state, blend, texture stage\n"
        "; 0, pixel shader, vertex declaration and primitive count, and prints a table and a\n"
        "; VERDICT every 3 s: whether the Scaleform HUD draws separate from the world with NO\n"
        "; overlap. It costs a bucket lookup per draw, so leave it off unless you are measuring.\n"
        "; `draws on|off|status|regions|kill <key>|hud|unkill` live, and the F10 HUD tickbox.\n"
        "Census=0\n"
        "[Hud]\n"
        "ReadingTilt=0\n"
        "ReadingTiltReference=1\n"
        "WheelPartsAlphaMode=mix\n"
        "WheelPartsAlphaGain=0.280\n"
        "WheelPartsAlphaFloor=0.000\n"
        "WheelPartsAlphaGamma=0.630\n"
        "WheelPartsAlphaMix=1.930\n"
        "WheelSidePanels=1\n"
        "NativeGameplayReference=0\n"
        "NativeObjectiveUpright=1\n"
        "WheelCloseAnimation=1\n"
        "MenuExitHeading=1\n"
        "NativeObjectiveLabels=1\n"
        "PauseSceneFreshness=1\n"
        "; MenuSceneFreshness: reuse observed scene uploads for up to 100ms in head-tracked menus.\n"
        "; Separate test lever; stale scenes and menu/level transitions still refuse.\n"
        "MenuSceneFreshness=1\n"
        "PauseAlphaMode=repair\n"
        "PauseAlphaGain=1.950\n"
        "PauseAlphaFloor=0.660\n"
        "PauseAlphaGamma=0.850\n"
        "PauseAlphaMix=1.350\n"
        "NativeObjectiveIcons=1\n"
        "NativeObjectiveScale=0.330\n"
        "; The native marker levers (VR-129). NativeTaskMarkers keeps an offscreen\n"
        "; objective marker inside the frame at TaskMarkerEdgeInset; the Rune keys do\n"
        "; the same for the Heart's rune markers, and NativeRuneOwnership keeps that\n"
        "; group native from its first appearance. On, at 0.220, is what the\n"
        "; 2026-09-19 headset run was played with. Live: F10 HUD tab.\n"
        "NativeTaskMarkers=1\n"
        "TaskMarkerEdgeInset=0.220\n"
        "NativeRuneMarkers=1\n"
        "RuneMarkerEdgeInset=0.220\n"
        "NativeRuneOwnership=1\n"
        "NativeMarkerChildren=0\n"
        "; VR-148 / VR-149. Both SHIP ON, and both have a live F10 HUD toggle.\n"
        "; NativeAwarenessMarkers=1 leaves the enemy awareness meters in the game\n"
        "; image at the position the engine gave them, instead of letting them fall\n"
        "; to the default row and ride the default window panel (they were near the\n"
        "; right enemies but not on them). MEASURED on a Quest 3 over VDXR 2026-09-19:\n"
        "; 2996 placements published, 0 refused, 5238 draws matched, 4 ambiguous, and\n"
        "; the widest accepted draw 61x62 authoring px against a 160x160 bound.\n"
        "; NativeHeartAllSymbols=1 treats EVERY Heart marker, not only the one whose\n"
        "; Flash symbol is runeMarker, which is how bone charms get the rune marker\n"
        "; treatment - they ride the same marker and differ only by that symbol.\n"
        "; STILL UNPROVEN: no bone charm has yet been revealed by the Heart in a run,\n"
        "; so this is on because it is what the tested machine ran, not because the\n"
        "; feature has been seen working. hud/heart-symbol names every symbol a run\n"
        "; sees, on or off, and that line is what will confirm or refute it.\n"
        "NativeHeartAllSymbols=1\n"
        "NativeAwarenessMarkers=1\n"
        "ObjectiveScreenTracking=0\n"
        "GroupInteractions=1\n"
        "RouteObjectives=1\n"
        "WeaponDialAlphaMode=mix\n"
        "WeaponDialAlphaMix=1.000\n"
        "ReadingAlphaMode=repair\n"
        "ReadingAlphaGain=1.000\n"
        "ReadingAlphaFloor=0.000\n"
        "ReadingAlphaGamma=1.020\n"
        "ReadingAlphaMix=0.990\n"
        "InteractionAlphaMode=repair\n"
        "InteractionAlphaGain=1.090\n"
        "InteractionAlphaFloor=0.000\n"
        "InteractionAlphaGamma=0.250\n"
        "InteractionAlphaMix=1.000\n"
        "NoteHandRight=0.320\n"
        "JournalHandRight=0.320\n"
        "WeaponDialAlphaGain=0.600\n"
        "WeaponDialAlphaFloor=0.180\n"
        "WeaponDialAlphaGamma=1.160\n"
        "NoteFollowHand=1\n"
        "NoteHandWidth=0.660\n"
        "NoteHandDistance=0.020\n"
        "JournalFollowHand=1\n"
        "JournalHandWidth=0.660\n"
        "JournalHandDistance=0.020\n"
        "WeaponDialDirectionOnly=1\n"
        "WeaponDialDeadzone=0.003\n"
        "WeaponDialCircle=1\n"
        "WeaponDialDistance=0.040\n"
        "HeadLookPause=1\n"
        "NoBlurPause=1\n"
        "HeadLookNote=1\n"
        "NoBlurNote=1\n"
        "HeadLookJournal=1\n"
        "NoBlurJournal=1\n"
        "HeadLookWheel=1\n"
        "NoBlurWheel=1\n"
        "HeadLookStore=0\n"
        "NoBlurStore=0\n"
        "HeadLookMissionStats=0\n"
        "NoBlurMissionStats=0\n"
        "WeaponDial=1\n"
        "WeaponDialWidth=0.360\n"
        "WeaponDialRadius=0.080\n"
        "WeaponDialCropX=0.440\n"
        "WeaponDialCropY=0.390\n"
        "; THE HUD ON ITS ANCHORS (VR-117; core/gfx/hud_class, hud_capture, hud_layout).\n"
        "; Panel=1 redirects the game's own Scaleform HUD draws into private targets and shows\n"
        "; them on quads in the headset instead of in the world: a WINDOW in front of the\n"
        "; player and the tracked HAND (what build 38.92 shipped as the wrist HUD). While it\n"
        "; is on the HUD is NOT in the eye textures and NOT in the desktop window: that is\n"
        "; what a redirect means. 0 leaves the HUD in the frame, as the game draws it. If the\n"
        "; hand-off to D3D11 cannot be built the redirect stays off and the HUD keeps drawing\n"
        "; into the frame, because losing it entirely would be worse. Loading screens, the\n"
        "; main menu, cutscenes on the mono screen and the power wheel leave it in the frame.\n"
        "; `hud on|off|status` live, and the F10 HUD tab.\n"
        "Panel=1\n"
        "; SlotScale: each sink's texture is the render's size times this. The window subtends\n"
        "; about 50 degrees, so half is already more than the headset resolves.\n"
        "SlotScale=0.50\n"
        "; Regions=1 reads each HUD draw's screen rectangle from its vertices (through the vertex\n"
        "; shader's own transform, VR-118) and routes it to an ELEMENT by the Region.<name> table\n"
        "; below; 0 routes every draw to 'default' (the whole HUD as one quad: the A/B). The\n"
        "; rectangles are measured with `draws on` + `draws regions`. `hud regions on|off` live.\n"
        "Regions=1\n"
        "; Element.<name>=off|frame|window|world|handL|handR (VR-120): which anchor an element\n"
        "; rides. 'frame' leaves it in the eye textures as the game draws it, 'off' hides it,\n"
        "; 'window' is head-locked in front of you, 'world' is parked where you recentred (F5,\n"
        "; `hud window recenter`), handL/handR ride the tracked hand. 'default' takes every draw\n"
        "; no row claims (an element the mod has not named yet still shows, and `hud list` counts\n"
        "; it); the six screens (pause, note, journal, wheel, store, missionstats) route by their\n"
        "; UI owner while they ride, and take the mono screen when set off or frame. A row with\n"
        "; no Region rides 'default' until `hud region <name> x0,y0,x1,y1` names one.\n"
        "; Element.<name>.WinX/WinY/WinScale place it on the window or the world window,\n"
        "; .HandX/HandY/HandScale on either hand (metres in the anchor's plane, a size factor).\n"
        "; `hud anchor <name|all> <anchor>`, `hud place <name> window|hand <x> <y> [scale]`,\n"
        "; `hud list`, the F10 HUD tab, `hud reset`.\n"
        "Element.default=window\n"
        "Element.vitals=handR\n"
        "Element.reticle=window\n"
        "; ReticleOnAim=1 (VR-166): the reticle row - centred gauges such as the grenade cook ring -\n"
        "; rides the aim dot along the weapon ray (head-facing, same apparent size), and the dot\n"
        "; hides while it draws. 0 = the row stays on its own anchor.\n"
        "ReticleOnAim=1\n"
        "Element.prompt=window\n"
        "Element.equipment=window\n"
        "Element.subtitles=window\n"
        "Element.objective=window\n"
        "Element.toast=window\n"
        "Element.tutorial=window\n"
        "Element.detection=frame\n"
        "Element.skipgauge=window\n"
        "Element.darkvision=window\n"
        "Element.vignette=frame\n"
        "Element.pause=world\n"
        "Element.note=window\n"
        "Element.journal=window\n"
        "Element.wheel=window\n"
        "Element.store=window\n"
        "Element.missionstats=window\n"
        "; Region.<name>=x0,y0,x1,y1 (normalised backbuffer, y down): the rectangle that claims\n"
        "; a draw whose centre lies inside. These three are the measured ones (the sewer level,\n"
        "; 2026-09-15, with a margin around each element's union: ENGINE_NOTES, \"How the\n"
        "; Scaleform HUD identifies its elements\"); the health and mana bars interleave, hence\n"
        "; one 'vitals' row. Unset = unmeasured (rides 'default'). `hud region <name> ...`.\n"
        "Region.vitals=0.000,0.000,0.200,0.270\n"
        "Region.reticle=0.470,0.470,0.530,0.530\n"
        "Region.prompt=0.520,0.460,0.800,0.620\n"
        "; The window (shared by 'window' and 'world'): distance and width in metres; Height 0 =\n"
        "; the texture's aspect, else a centred crop; Up and Lateral offset it in its plane.\n"
        "WindowDistance=1.390\n"
        "WindowWidth=1.210\n"
        "WindowHeight=0.000\n"
        "WindowUp=-0.100\n"
        "WindowLateral=0.000\n"
        "; The two hand panels (38.92's values): X/Y/Z an offset in the grip's own frame; Lift\n"
        "; along world up; Width in metres; Orient billboard (faces the head, never rolls: what\n"
        "; 38.92 did) or grip (a watch face on the back of the hand, Tilt degrees toward the\n"
        "; eyes). `hud hand l|r x|y|z|lift|width|tilt <v>`, `hud hand l|r billboard|grip`.\n"
        "HandL.X=0.000\n"
        "HandL.Y=0.000\n"
        "HandL.Z=0.000\n"
        "HandL.Lift=0.203\n"
        "HandL.Width=0.400\n"
        "HandL.Orient=grip\n"
        "HandL.Tilt=-31.000\n"
        "HandR.X=0.028\n"
        "HandR.Y=-0.006\n"
        "HandR.Z=0.081\n"
        "HandR.Lift=0.135\n"
        "HandR.Width=0.220\n"
        "HandR.Orient=billboard\n"
        "HandR.Tilt=2.000\n"
        "; The alpha (VR-119; core/gfx/blit_quad): how the quads' transparency is derived from the\n"
        "; sink. repair = max(r,g,b) (what 41.2 shipped; dark strokes such as text outlines go faint);\n"
        "; captured = the sink's own coverage, which the redirect then forces on every HUD draw\n"
        "; (separate alpha ONE/INVSRCALPHA, eight SetRenderState calls per draw); mix = the larger\n"
        "; of the two with repair scaled by AlphaMix. AlphaGain multiplies the alpha, AlphaFloor\n"
        "; is a minimum for any pixel with colour (keeps thin strokes), AlphaGamma nudges the\n"
        "; colour. Backdrop.window/hand=r,g,b,a composes a plate UNDER the quads of that anchor\n"
        "; (a=0: none). Every value at identity is 41.2's picture. `hud alpha mode|gain|floor|\n"
        "; gamma|mix|backdrop ...` live, the F10 HUD tab; `dump hud` writes the alpha as grey.\n"
        "AlphaMode=repair\n"
        "AlphaGain=1.000\n"
        "AlphaFloor=0.000\n"
        "AlphaGamma=1.000\n"
        "AlphaMix=1.000\n"
        "Backdrop.window=0.000,0.000,0.000,0.000\n"
        "Backdrop.hand=0.000,0.000,0.000,0.000\n"
        "; MenuInWindow=1 lets the in-game screens (pause, note, journal, the power wheel, store,\n"
        "; mission stats, each with its own Window<Context> opt-in) ride their Element.<name>\n"
        "; anchor with the world in stereo behind them; resuming never drops the projection. 0 is\n"
        "; the old behaviour: every screen takes the mono screen ([Screen] Anchor* place it). The\n"
        "; MAIN menu and loading screens always use the mono screen. `hud menu on|off`, `hud menu\n"
        "; Pause on|off`.\n"
        "MenuInWindow=1\n"
        "WindowPause=1\n"
        "WindowNote=1\n"
        "WindowJournal=1\n"
        "WindowWheel=1\n"
        "WindowStore=0\n"
        "WindowMissionStats=1\n"
        "Element.default.WinX=0.244\n"
        "Element.default.WinY=-0.063\n"
        "Element.default.WinScale=1.570\n"
        "Element.vitals.WinX=-0.167\n"
        "Element.vitals.WinY=0.106\n"
        "Element.vitals.WinScale=1.150\n"
        "Element.vignette.WinX=0.000\n"
        "Element.vignette.WinY=0.000\n"
        "Element.vignette.WinScale=1.620\n"
        "Element.vitals.HandX=0.065\n"
        "Element.vitals.HandY=-0.102\n"
        "Element.vitals.HandScale=0.270\n"
        "Element.reticle.WinX=0.000\n"
        "Element.reticle.WinY=0.000\n"
        "Element.reticle.WinScale=1.000\n"
        "Element.equipment.WinX=0.000\n"
        "Element.equipment.WinY=0.000\n"
        "Element.equipment.WinScale=1.000\n"
        "Element.subtitles.WinX=0.000\n"
        "Element.subtitles.WinY=0.000\n"
        "Element.subtitles.WinScale=1.000\n"
        "Element.skipgauge.WinX=0.000\n"
        "Element.skipgauge.WinY=0.000\n"
        "Element.skipgauge.WinScale=1.000\n"
        "Element.default.HandX=0.000\n"
        "Element.default.HandY=0.000\n"
        "Element.default.HandScale=1.000\n"
        "Element.reticle.HandX=0.000\n"
        "Element.reticle.HandY=0.000\n"
        "Element.reticle.HandScale=1.000\n"
        "Element.prompt.WinX=0.000\n"
        "Element.prompt.WinY=0.000\n"
        "Element.prompt.WinScale=1.170\n"
        "Element.prompt.HandX=0.000\n"
        "Element.prompt.HandY=0.000\n"
        "Element.prompt.HandScale=1.000\n"
        "Element.equipment.HandX=0.000\n"
        "Element.equipment.HandY=0.000\n"
        "Element.equipment.HandScale=1.000\n"
        "Element.subtitles.HandX=0.000\n"
        "Element.subtitles.HandY=0.000\n"
        "Element.subtitles.HandScale=1.000\n"
        "Element.objective.WinX=0.000\n"
        "Element.objective.WinY=0.000\n"
        "Element.objective.WinScale=1.000\n"
        "Element.objective.HandX=0.000\n"
        "Element.objective.HandY=0.000\n"
        "Element.objective.HandScale=1.000\n"
        "Element.toast.WinX=0.000\n"
        "Element.toast.WinY=0.000\n"
        "Element.toast.WinScale=1.000\n"
        "Element.toast.HandX=0.000\n"
        "Element.toast.HandY=0.000\n"
        "Element.toast.HandScale=1.000\n"
        "Element.tutorial.WinX=0.000\n"
        "Element.tutorial.WinY=0.000\n"
        "Element.tutorial.WinScale=1.000\n"
        "Element.tutorial.HandX=0.000\n"
        "Element.tutorial.HandY=0.000\n"
        "Element.tutorial.HandScale=1.000\n"
        "Element.detection.WinX=0.009\n"
        "Element.detection.WinY=0.000\n"
        "Element.detection.WinScale=1.000\n"
        "Element.detection.HandX=0.000\n"
        "Element.detection.HandY=0.000\n"
        "Element.detection.HandScale=1.000\n"
        "Element.skipgauge.HandX=0.000\n"
        "Element.skipgauge.HandY=0.000\n"
        "Element.skipgauge.HandScale=1.000\n"
        "Element.darkvision.WinX=0.000\n"
        "Element.darkvision.WinY=0.000\n"
        "Element.darkvision.WinScale=1.000\n"
        "Element.darkvision.HandX=0.000\n"
        "Element.darkvision.HandY=0.000\n"
        "Element.darkvision.HandScale=1.000\n"
        "Element.vignette.HandX=0.000\n"
        "Element.vignette.HandY=0.000\n"
        "Element.vignette.HandScale=1.000\n"
        "Element.pause.WinX=0.000\n"
        "Element.pause.WinY=0.000\n"
        "Element.pause.WinScale=1.520\n"
        "Element.pause.HandX=0.000\n"
        "Element.pause.HandY=0.000\n"
        "Element.pause.HandScale=1.000\n"
        "Element.note.WinX=0.000\n"
        "Element.note.WinY=-0.000\n"
        "Element.note.WinScale=1.000\n"
        "Element.note.HandX=0.046\n"
        "Element.note.HandY=-0.047\n"
        "Element.note.HandScale=2.300\n"
        "Element.journal.WinX=0.000\n"
        "Element.journal.WinY=0.000\n"
        "Element.journal.WinScale=1.000\n"
        "Element.journal.HandX=0.000\n"
        "Element.journal.HandY=0.000\n"
        "Element.journal.HandScale=1.000\n"
        "Element.wheel.WinX=0.000\n"
        "Element.wheel.WinY=0.000\n"
        "Element.wheel.WinScale=1.030\n"
        "Element.wheel.HandX=0.000\n"
        "Element.wheel.HandY=0.000\n"
        "Element.wheel.HandScale=1.000\n"
        "Element.store.WinX=0.000\n"
        "Element.store.WinY=0.000\n"
        "Element.store.WinScale=1.000\n"
        "Element.store.HandX=0.000\n"
        "Element.store.HandY=0.000\n"
        "Element.store.HandScale=1.000\n"
        "Element.missionstats.WinX=0.000\n"
        "Element.missionstats.WinY=0.000\n"
        "Element.missionstats.WinScale=1.000\n"
        "Element.missionstats.HandX=0.000\n"
        "Element.missionstats.HandY=0.000\n"
        "Element.missionstats.HandScale=1.000\n"
        "WheelShortcuts.Crop0=0.055\n"
        "WheelShortcuts.Crop1=0.228\n"
        "WheelShortcuts.Crop2=0.952\n"
        "WheelShortcuts.Crop3=0.163\n"
        "WheelPotions.Crop0=0.723\n"
        "WheelPotions.Crop1=0.947\n"
        "WheelPotions.Crop2=0.948\n"
        "WheelPotions.Crop3=0.082\n"
        "Element.wheelpotions.WinX=0.716\n"
        "Element.wheelpotions.WinY=0.660\n"
        "Element.wheelpotions.WinScale=0.780\n"
        "Element.wheelshortcuts.WinX=-0.504\n"
        "Element.wheelshortcuts.WinY=0.695\n"
        "Element.wheelshortcuts.WinScale=0.730\n"
        "", kConfigVersion);
    const int closed = fclose(f);
    return written > 0 && closed == 0;
}

// VR-199: replace only a complete, flushed profile. A failed reset leaves the old ini intact.
static bool ConfigRestoreDefaults(const char* ini)
{
    char temp[MAX_PATH], backup[MAX_PATH];
    if (_snprintf(temp, sizeof(temp), "%s.reset-tmp", ini) < 0 ||
        _snprintf(backup, sizeof(backup), "%s.pre-reset", ini) < 0) return false;
    char json[2 * MAX_PATH] = "", runtime[32] = "", data[MAX_PATH] = "";
    GetPrivateProfileStringA("VR", "XrRuntimeJson", "", json, sizeof(json), ini);
    GetPrivateProfileStringA("VR", "Runtime", "", runtime, sizeof(runtime), ini);
    GetPrivateProfileStringA("Paths", "DataDir", "", data, sizeof(data), ini);
    if (!CopyFileA(ini, backup, FALSE) || !WriteDefaultIni(temp)) return false;
    bool ok = true;
    if (json[0]) ok = WritePrivateProfileStringA("VR", "XrRuntimeJson", json, temp) != FALSE && ok;
    if (runtime[0]) ok = WritePrivateProfileStringA("VR", "Runtime", runtime, temp) != FALSE && ok;
    if (data[0]) ok = WritePrivateProfileStringA("Paths", "DataDir", data, temp) != FALSE && ok;
    WritePrivateProfileStringA(nullptr, nullptr, nullptr, temp);
    if (ok) ok = MoveFileExA(temp, ini, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) DeleteFileA(temp);
    WritePrivateProfileStringA(nullptr, nullptr, nullptr, ini);
    return ok;
}

static void LoadConfig()
{
    char ini[MAX_PATH];
    _snprintf(ini, MAX_PATH, "%s\\dishonored_vr.ini", g_dir);
    Log("config: LoadConfig begin");
    {   // VR-223: the headset the player told the launcher they have, so a log
        // names the hardware without anyone having to ask. Read from the
        // launcher's own file (always %LOCALAPPDATA%, never [Paths] DataDir),
        // not the mod ini, which a version bump rewrites. The runtime's own
        // system name is logged at session start; the two can disagree (a Quest
        // on SteamVR reports through SteamVR), which is exactly why both print.
        char local[MAX_PATH] = "", launcherIni[MAX_PATH] = "", model[64] = "";
        const DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", local, sizeof(local));
        if (n && n < sizeof(local)) {
            _snprintf(launcherIni, MAX_PATH, "%s\\DishonoredVR\\launcher.ini", local);
            launcherIni[MAX_PATH - 1] = 0;
            GetPrivateProfileStringA("Headset", "Model", "", model, sizeof(model), launcherIni);
        }
        Log("config: headset (user reported in the launcher): %s",
            model[0] ? model : "not recorded (launcher never run on this account, or older than VR-223)");
        // VR-224: [Controllers] IndexTuning -1 auto | 0 off | 1 on. Auto follows the
        // headset above: the tuning was measured on Index controllers, which both the
        // Index and the Beyond ship with. The shim reads the verdict from
        // DVR_INDEX_TUNING (it is loaded later, by the OpenXR loader, in this process).
        const int it = GetPrivateProfileIntA("Controllers", "IndexTuning", -1, ini);
        // Vive Pro 2 counts: its wands are not supported, so it is played on Index controllers.
        const bool autoHs = !strcmp(model, "Valve Index") || !strcmp(model, "Bigscreen Beyond 1 / 2") ||
                            !strcmp(model, "Vive Pro 2");
        g_indexTuning = it == 1 || (it != 0 && autoHs);
        SetEnvironmentVariableA("DVR_INDEX_TUNING", g_indexTuning ? "1" : "0");
        Log("config: [Controllers] IndexTuning=%d -> %s (owner: %s)", it, g_indexTuning ? "ON" : "off",
            it == 1 ? "ini, forced on" : it == 0 ? "ini, forced off"
            : autoHs ? "launcher headset is an Index-controller headset" : "launcher headset is not Index, Beyond or Vive Pro 2");
    }

    // create if missing, OR refresh if it predates this build's tuned defaults
    bool missing = GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES;
    int ver = (int)IniFloat(ini, "Meta", "Version", 11);
    if (!missing && GetPrivateProfileIntA("Meta", "ResetDefaults", 0, ini)) {
        const bool restored = ConfigRestoreDefaults(ini);
        Log("config: reset to shipped defaults %s (runtime and DataDir preserved)",
            restored ? "complete; previous profile saved as .pre-reset" : "FAILED; keeping existing profile");
        if (restored) ver = kConfigVersion;
    }
    if (missing || ver < kConfigVersion) {
        // 41.0: a launcher may have put the runtime selection into an ini that
        // has never been through this build ([VR] XrRuntimeJson from
        // xrsim-launch.ps1 -ViaSteam, or [VR] Runtime=steamvr by hand). The
        // wholesale rewrite must carry those two over, or a Steam launch on
        // the simulator lands on the registry runtime and the launcher's
        // assertion is the only thing that notices (measured 2026-09-02).
        char keepJson[2 * MAX_PATH] = "", keepRt[16] = "", keepDd[MAX_PATH] = "";
        if (!missing) {
            GetPrivateProfileStringA("VR", "XrRuntimeJson", "", keepJson, sizeof(keepJson), ini);
            GetPrivateProfileStringA("VR", "Runtime", "", keepRt, sizeof(keepRt), ini);
            GetPrivateProfileStringA("Paths", "DataDir", "", keepDd, sizeof(keepDd), ini);
        }
        WriteDefaultIni(ini);
        if (keepJson[0]) WritePrivateProfileStringA("VR", "XrRuntimeJson", keepJson, ini);
        if (keepRt[0])   WritePrivateProfileStringA("VR", "Runtime", keepRt, ini);
        if (keepDd[0])   WritePrivateProfileStringA("Paths", "DataDir", keepDd, ini);
        Log("config: wrote fresh ini (was %s, now v%d)%s%s",
            missing ? "missing" : "outdated", kConfigVersion,
            keepJson[0] ? " - kept [VR] XrRuntimeJson" : "",
            keepRt[0] ? " - kept [VR] Runtime" : "");
    }
    {   // [Paths] DataDir: where the harness files go. Applied before any of
        // them is written (the command seam and status.json start after the
        // config); the dev PC's tool sandbox virtualizes writes under the user
        // profile, so a real location (D:\dvr-data) is what a Steam launch and
        // the harness can both see (docs/VERIFICATION.md gotcha 14).
        char dd[MAX_PATH] = "";
        GetPrivateProfileStringA("Paths", "DataDir", "D:\\dvr-data", dd, sizeof(dd), ini);
        if (dd[0]) {
            dvr::paths::set_data_dir(dd);
            Log("config: [Paths] DataDir -> %s (command.txt, status.json, dumps, the shim manifest)",
                dvr::paths::data_dir());
        }
    }
    g_trackingEnabled = IniFloat(ini, "Tracking", "Enabled", 1) != 0.0f;
    g_yawCounts    = IniFloat(ini, "Tracking", "YawCountsPerDegree", 11.5f);
    g_pitchCounts  = IniFloat(ini, "Tracking", "PitchCountsPerDegree", 11.5f);
    g_invertPitch  = IniFloat(ini, "Tracking", "InvertPitch", 0) != 0.0f;
    g_screenDist   = IniFloat(ini, "Screen", "DistanceMeters", 1.75f);
    if (g_screenDist < 0.5f) g_screenDist = 0.5f;
    if (g_screenDist > 6.0f) g_screenDist = 6.0f;
    g_screenWidth  = IniFloat(ini, "Screen", "WidthMeters", 2.4f);
    if (g_screenWidth < 0.5f) g_screenWidth = 0.5f;
    if (g_screenWidth > 10.0f) g_screenWidth = 10.0f;
    dvr::vr::set_screen(g_screenDist, g_screenWidth);
    dvr::vr::set_screen_head_locked(IniFloat(ini, "Screen", "HeadLocked", 1) != 0.0f);
    {   // 41.1 [Screen] Render*: the picker's ask (core/window/render_size.cpp)
        // 41.1 (session 9): the headset-judged size, and the advertisement that makes the
        // game create it, are the DEFAULTS now (an ini naming neither key gets them);
        // `res 0x0` writes an explicit 0 and still means "the game's own size".
        // 41.1 (session 15, 2026-09-04): 2750x2850, judged on the headset. It is
        // NOT simply "bigger is better" - it is the size whose tick lands one
        // display period at 90 Hz, which is what killed the ghosting (see
        // ENGINE_NOTES, "The ghosting was the cadence beat"). At 120 Hz the same
        // size beats at 1.05-1.11 slots per frame and ghosts.
        g_resWantW = (uint32_t)GetPrivateProfileIntA("Screen", "RenderWidth", 2750, ini);
        g_resWantH = (uint32_t)GetPrivateProfileIntA("Screen", "RenderHeight", 2850, ini);
        g_resWantFull = GetPrivateProfileIntA("Screen", "RenderFullscreen", 1, ini) != 0;
        g_resVirtual = GetPrivateProfileIntA("Screen", "VirtualMode", 1, ini) != 0 || g_launchVirtual;
        if (!g_resWantW && g_launchW && g_launchH) {   // the launch file carried an ask the ini lost
            g_resWantW = g_launchW; g_resWantH = g_launchH; g_resWantFull = g_launchFull;
            Log("config: [Screen] Render* read 0 but the launch file asked %ux%u %s - using that for the verdict",
                g_resWantW, g_resWantH, g_resWantFull ? "fullscreen" : "windowed");
        }
        if (g_resWantW && g_resWantH)
            Log("config: [Screen] Render %ux%u %s, VirtualMode=%d (the verdict prints once the capture knows the size)",
                g_resWantW, g_resWantH, g_resWantFull ? "fullscreen" : "windowed", (int)g_resVirtual);
    }
    {   // [Stereo] Method: the rung of the ladder (docs/ARCHITECTURE.md); a
        // stub or an unknown name logs why and leaves the mono screen running.
        // Recorded here, APPLIED by Direct3DCreate9 after the game side has
        // registered its hooks (41.1: selecting here refused reentry every
        // boot - the scene-draw hooks did not exist yet).
        char sm[16] = "";
        GetPrivateProfileStringA("Stereo", "Method", "reentry", sm, sizeof(sm), ini);
        // The second draw stands itself down when it is not producing presents
        // of its own - it can only overwrite pass 1 in the same backbuffer then,
        // which makes the frame alternate between the eyes. See SceneDrawBeat.
        g_sdStandDown = IniFloat(ini, "Stereo", "ReentryStandDown", 1) != 0.0f;
        dvr::stereo::set_config_method(sm);
        dvr::stereo::set_armed(GetPrivateProfileIntA("Stereo", "Armed", 1, ini) != 0);
        dvr::stereo::set_reentry_c5_pair(GetPrivateProfileIntA("Stereo", "C5Pair", 1, ini) != 0);   // 41.1 (session 9)
        dvr::stereo::set_reentry_single_tag(GetPrivateProfileIntA("Stereo", "SingleTagRepair", 1, ini) != 0);
        dvr::stereo::set_reentry_late_tag(GetPrivateProfileIntA("Stereo", "LateTagRepair", 1, ini) != 0);   // Confirmed profile default; F10 retains the A/B.
        dvr::stereo::set_hold_untagged(GetPrivateProfileIntA("Stereo", "HoldUntagged", 3, ini));
    }

    {   // [Camera] EyeField: where the per-eye offset is written (measured by
        // `camera eyetest`; empty until then - the seam says so once)
        char ef[16] = "";
        GetPrivateProfileStringA("Camera", "EyeField", "0x330", ef, sizeof(ef), ini);
        dvr::camera::set_eye_field(ef);
    }
    {   // VR-30: [Camera] ArmFollowWeight. -1 = off (ships). Read here rather
        // than under [Hands] on purpose: the GamepadOnly block near the end of
        // this function clears the hand switches, and this must survive it.
        const float afw = IniFloat(ini, "Camera", "ArmFollowWeight", -1.0f);
        if (afw > 1.0f)
            Log("config: [Camera] ArmFollowWeight=%.2f is out of range (-1 = off, 0..1) - left OFF", afw);
        else if (afw >= 0.0f)
            ArmFollowSetForce(afw, "ini");
        const float adw = IniFloat(ini, "Camera", "ArmDisableWeight", 1.0f);
        if (adw > 1.0f)
            Log("config: [Camera] ArmDisableWeight=%.2f is out of range (-1 = off, 0..1) - left OFF", adw);
        else if (adw >= 0.0f)
            ArmFollowSetInfluence(adw, "ini");
        const float als = IniFloat(ini, "Camera", "ArmLookAtStrength", -1.0f);
        if (als > 1.0f)
            Log("config: [Camera] ArmLookAtStrength=%.2f is out of range (-1 = off, 0..1) - left OFF", als);
        else if (als >= 0.0f)
            ArmFollowSetLookAt(als, "ini");
        // VR-30: the yaw lever. Ships ON at 1.0 - a deliberate exception to
        // "every render lever ships OFF", made on the user's direction because
        // the arms following the view is the fault the whole ticket is about.
        const float acy = IniFloat(ini, "Camera", "ArmCounterYaw", -1.0f);
        if (acy > 2.0f)
            Log("config: [Camera] ArmCounterYaw=%.2f is out of range (-1 = off, 0..2) - left OFF", acy);
        else if (acy >= 0.0f)
            ArmFollowSetCounterYaw(acy, "ini");
        // VR-30: the core fix - hold the body instead of correcting the arms
        HeadMovementSet(GetPrivateProfileIntA("Camera","HeadBasedMovement",1,ini)!=0);
        const float fac = IniFloat(ini, "Camera", "ArmBodyFacing", 1.0f);
        if (fac >= 0.0f) ArmFollowSetFacing(fac, "ini");
        const float asr = IniFloat(ini, "Camera", "ArmStripMeshRot", -1.0f);
        if (asr >= 0.0f) ArmFollowSetStripRot(asr, "ini");
    }
    {   // [Capture] Mode: the capture path (sync is the baseline; an impossible
        // mode is refused with the reason and sync keeps running)
        char cm[16] = "";
        GetPrivateProfileStringA("Capture", "Mode", "shared", cm, sizeof(cm), ini);
        if (!_stricmp(cm, "off")) {   // the A/B control is live-only: a frozen headset at boot is a trap
            Log("config: [Capture] Mode=off refused (live only, 'capture mode off' on the seam) - sync");
            strcpy(cm, "sync");
        }
        if (!dvr::capture::set_mode(cm)) dvr::capture::set_mode("sync");
        dvr::capture::set_shared_wait(IniFloat(ini, "Capture", "SharedWait", 0) != 0.0f);
        {   // [Capture] BboxMs: how often the content-bbox instrument resamples.
            // Each sample is a full-frame CPU readback on the present thread even
            // in shared mode (capture.h says why), so this is a frame-time knob,
            // not a logging knob. 0 = only after a size change.
            float bb = IniFloat(ini, "Capture", "BboxMs", 30000);
            if (bb < 0.0f) bb = 0.0f;
            if (bb > 600000.0f) bb = 600000.0f;
            dvr::capture::set_bbox_ms((uint32_t)bb);
        }
    }
    g_flipYaw   = IniFloat(ini, "HeadInject", "FlipYaw",   1) < 0 ? -1 : 1;
    g_flipPitch = IniFloat(ini, "HeadInject", "FlipPitch", 1) < 0 ? -1 : 1;
    g_flipRoll  = IniFloat(ini, "HeadInject", "FlipRoll",  1) < 0 ? -1 : 1;
    g_posTrack   = IniFloat(ini, "PosTrack", "Enabled", 1) != 0.0f;
    {   // [PosTrack] Lane: vp (the c0 patch, shipped) or camera (the seam's write)
        char pl[16] = "";
        GetPrivateProfileStringA("PosTrack", "Lane", "auto", pl, sizeof(pl), ini);
        if (!dvr::camera::set_pos_lane(pl)) dvr::camera::set_pos_lane("auto");
    }
    g_crouchEyeCfg   = IniFloat(ini, "PosTrack", "CrouchEyeDrop", 1) != 0.0f; // 38.15
    g_crouchEyeScale = IniFloat(ini, "PosTrack", "CrouchEyeScale", 1.0f);
    if (g_crouchEyeScale < 0.0f) g_crouchEyeScale = 0.0f;
    if (g_crouchEyeScale > 2.0f) g_crouchEyeScale = 2.0f;
    // 41.2 DEEP CROUCH NOW DEFAULTS OFF - it is the crouch height rise, and the
    // "I could float around" with it.
    //
    // 38.16 shrinks the crouched collision cylinder (65 -> DeepCrouchUU, 45) so
    // the player fits under more. MEASURED on the headset 2026-09-04, filmed one
    // line per frame across the transition: that write TELEPORTS THE PAWN DOWN
    // BY EXACTLY OUR SHRINK, 20.00 uu, on every crouch -
    //
    //     cyl 45.0  pawnZ 3405.73     the crouch, cylinder 65 -> 45
    //     cyl 45.0  pawnZ 3385.73     dropped exactly 20.00, then holds
    //
    // - because the engine keeps the feet planted, so a shorter capsule means a
    // lower origin. In the worst case it leaves the pawn AIRBORNE: one burst
    // filmed it rising 34 uu and then falling 128 uu back to the floor, which is
    // what the tester independently described as "almost like noclip, I could
    // float around". The body really was off the ground.
    //
    // The camera then chases that displacement with a RATE-LIMITED convergence
    // (~4-8 uu per frame ramping up after an uncrouch, filmed with the pawn
    // provably stationary; a slow decay on the way down). It cannot finish
    // before the next crouch, so the unconverged remainder is retained and the
    // view climbs ~20 uu per cycle without bound - 2184 uu, 22 m, in one run.
    //
    // An A-B-A block A/B agrees to the decimal: write ON +20.35 uu of view per
    // cycle (blocks 1 and 3: +19.42, +21.28), write off -0.26 (six consecutive
    // cycles flat within +-2.6), difference +20.61 over 18 measured cycles.
    //
    // It cannot be made safe as written: resizing a grounded pawn's capsule from
    // outside the engine moves the pawn, and keeping the feet planted would mean
    // writing Actor.Location, which this mod deliberately never does. DeepCrouch=1
    // restores the old behaviour and the bug with it.
    g_deepCrouchCfg = IniFloat(ini, "PosTrack", "DeepCrouch", 0) != 0.0f;    // 38.16, default off since 41.2
    g_deepCrouchUU  = IniFloat(ini, "PosTrack", "DeepCrouchUU", 45.0f);
    if (g_deepCrouchUU < 33.0f) g_deepCrouchUU = 33.0f;   // never below vents
    if (g_deepCrouchUU > 64.0f) g_deepCrouchUU = 64.0f;
    // 30.43: vtable recon OFF by default now (it found nothing camera-shaped);
    // the POV block scan replaces it.
    g_csRecon    = IniFloat(ini, "CamSeam", "Recon", 0) != 0.0f;
    g_povProbe   = IniFloat(ini, "CamSeam", "PovProbe", 1) != 0.0f;
    g_povWiggle  = IniFloat(ini, "CamSeam", "Wiggle", 0) != 0.0f;
    // 30.51: the FOV lever survives launches (0 = off)
    {
        float lv = IniFloat(ini, "Screen", "FovLever", 0.0f);   // 41.0: off on the mono screen
        g_fovLever = (lv >= 40.0f && lv <= 160.0f) ? lv : 0.0f;
        dvr::camera::set_fov_deg(g_fovLever);
        if (g_fovLever > 0.0f) Log("config: FOV lever armed at %.0f deg", lv);
    }
    g_autoFocus = IniFloat(ini, "Input", "AutoFocus", 1) != 0.0f;
    // 40.3: NOW 98, and the measurement below is what it converged on. The
    // restore that 40.2b was waiting for happened (4032x2268 + FovLever=130),
    // and the tester then tuned world scale by feel in the F10 overlay and
    // landed on 98 - within 2% of the 100 derived from the movement constants,
    // arrived at independently and without seeing the number. That is the
    // cross-check 40.2b asked for, so the measured value is now the default.
    //
    // 40.2: MEASURED, not the engine's canonical default. Dishonored's own
    // movement constants are 360 uu/s (default) and 540 uu/s (sprint), read
    // off the crouch diagnostic's spd= plateaus over a walk-then-sprint run:
    // 14 samples at 355-363 and 21 at 537-545, ratio exactly 1.5. At the UE3
    // canonical 50 uu/m those are 7.2 and 10.8 m/s - Corvo would sprint at
    // world-record pace and stroll faster than most people can run. At 100
    // they are 3.6 m/s and 5.4 m/s, a jog and a run, which is how he moves.
    // 1 uu = 1 cm, the same convention the Mirror's Edge VR mod derived for
    // its own UE3 build from three agreeing movement constants.
    // Corroborated by eye height: 78.1 uu above the pawn origin plus a typical
    // ~88 uu human collision half-height puts the eye at 1.66 m.
    g_posScaleUU = IniFloat(ini, "PosTrack", "Scale", 108.0f);
    if (g_posScaleUU < 1.0f)    g_posScaleUU = 1.0f;
    if (g_posScaleUU > 400.0f)  g_posScaleUU = 400.0f;
    g_roomScaleCfg = IniFloat(ini, "PosTrack", "RoomScale", 1) != 0.0f;  // 38.46
    g_roomDeadM    = IniFloat(ini, "PosTrack", "RoomDeadM", 0.14f);
    if (g_roomDeadM < 0.03f) g_roomDeadM = 0.03f;
    if (g_roomDeadM > 0.60f) g_roomDeadM = 0.60f;
    g_roomGain     = IniFloat(ini, "PosTrack", "RoomGain", 2.4f);
    if (g_roomGain < 0.2f)  g_roomGain = 0.2f;
    if (g_roomGain > 12.0f) g_roomGain = 12.0f;
    g_roomMaxStick = IniFloat(ini, "PosTrack", "RoomMaxStick", 0.90f);
    if (g_roomMaxStick < 0.1f) g_roomMaxStick = 0.1f;
    if (g_roomMaxStick > 1.0f) g_roomMaxStick = 1.0f;
    g_roomBleedMS  = IniFloat(ini, "PosTrack", "RoomBleedMS", 0.90f);
    if (g_roomBleedMS < 0.05f) g_roomBleedMS = 0.05f;
    if (g_roomBleedMS > 6.0f)  g_roomBleedMS = 6.0f;
    g_posMaxM    = IniFloat(ini, "PosTrack", "MaxMeters", 0.80f);
    if (g_posMaxM < 0.05f) g_posMaxM = 0.05f;
    if (g_posMaxM > 2.0f)  g_posMaxM = 2.0f;
    g_posFlipX   = IniFloat(ini, "PosTrack", "FlipX", 0) != 0.0f;
    {   // 41.1 [Neck]: the pitch pivot lever. Default `cancel` (headset-judged 2026-09-03,
        // the engine's own neck cancelled); an unknown mode logs and stays off.
        char nm[16] = "";
        GetPrivateProfileStringA("Neck", "Mode", "cancel", nm, sizeof(nm), ini);
        int mode = !_stricmp(nm, "add") ? 1 : !_stricmp(nm, "cancel") ? 2 : 0;
        if (mode == 0 && _stricmp(nm, "off") != 0 && nm[0])
            Log("config: [Neck] Mode='%s' unknown (off|add|cancel) - off", nm);
        g_neckMode = mode;
        g_neckBelowM = IniFloat(ini, "Neck", "PivotBelowM", 0.321f)   /* the engine's pivot, measured 2026-09-03 */;
        g_neckBehindM = IniFloat(ini, "Neck", "PivotBehindM", 0.062f);
        if (g_neckBelowM < 0.0f) g_neckBelowM = 0.0f;   if (g_neckBelowM > 0.5f) g_neckBelowM = 0.5f;
        if (g_neckBehindM < 0.0f) g_neckBehindM = 0.0f; if (g_neckBehindM > 0.5f) g_neckBehindM = 0.5f;
        if (mode)
            Log("config: [Neck] Mode=%s PivotBelowM=%.3f PivotBehindM=%.3f (the pitch pivot lever, projection only)",
                nm, g_neckBelowM, g_neckBehindM);
        // VR-78: the crouched pivot. -1 = the standing numbers (the pre-VR-78 behaviour).
        char cbuf[32] = "";
        GetPrivateProfileStringA("Neck", "CrouchPivotBelowM", "", cbuf, sizeof(cbuf), ini);
        const bool cbHave = cbuf[0] != 0;
        g_neckCrouchBelowM = IniFloat(ini, "Neck", "CrouchPivotBelowM", 0.0f);   // VR-78: measured twice, headset-judged
        g_neckCrouchBehindM = IniFloat(ini, "Neck", "CrouchPivotBehindM", 0.0f);
        if (g_neckCrouchBelowM > 0.5f) g_neckCrouchBelowM = 0.5f;
        if (g_neckCrouchBehindM > 0.5f) g_neckCrouchBehindM = 0.5f;
        if (g_neckCrouchBelowM < 0.0f) g_neckCrouchBelowM = -1.0f;
        if (g_neckCrouchBehindM < 0.0f) g_neckCrouchBehindM = -1.0f;
        // VR-91: whether ROLL enters the arc. It must not - the arc models the
        // engine's own neck and the engine's is a pitch arc - so this ships 0 and
        // 1 is the one-key A/B back to the measured fault.
        dvr::camera::set_upright_pitch_arc(IniFloat(ini,"Neck","UprightPitchArc",1)!=0);
        g_neckRollArc = IniFloat(ini, "Neck", "RollArc", 0) != 0.0f;
        g_neckStanceBlendMs = IniFloat(ini, "Neck", "StanceBlendMs", 150.0f);
        if (g_neckStanceBlendMs < 0.0f) g_neckStanceBlendMs = 0.0f;
        if (g_neckStanceBlendMs > 2000.0f) g_neckStanceBlendMs = 2000.0f;
        Log("config: [Neck] CrouchPivotBelowM=%.3f CrouchPivotBehindM=%.3f StanceBlendMs=%.0f - %s (%s)",
            g_neckCrouchBelowM, g_neckCrouchBehindM, g_neckStanceBlendMs, cbHave ? "from the ini" : "absent, compiled default",
            g_neckCrouchBelowM < 0.0f && g_neckCrouchBehindM < 0.0f
                ? "-1 = the standing pivot while crouched: the pre-VR-78 behaviour"
                : "plain crouch uses its own pivot ONLY while the crawl tuck has released the camera's look-at control "
                  "([Hands] CrawlTuckCamera=1, the camera VR-78 measured); with the control kept the engine's crouched "
                  "neck is the standing one and the standing pivot applies (VR-122). Slides and vents keep the standing one");
        Log("config: [Neck] RollArc=%d - the arc is built from a %s frame. Roll in the "
            "arc measured 17 to 19 uu of INVERTED lateral camera motion at 30 deg of "
            "roll (VR-91); the real lateral swing a roll produces is already in the "
            "tracked head displacement, so modelling it here counted it twice.",
            (int)g_neckRollArc, g_neckRollArc ? "ROLLED head (pre-VR-91)" : "roll-free");
    }
    dvr::controller::configure({int(GetPrivateProfileIntA("Controllers","DpadModifier",1,ini)),
        GetPrivateProfileIntA("Controllers","DpadFlip",0,ini)!=0,
        GetPrivateProfileIntA("Controllers","PauseChord",1,ini)!=0});
    const auto controller=dvr::controller::config();
    Log("controls: modifier=%d dpad=%s X+Y=%d; Y=native, menu tap=START, modifier/hold+menu=BACK",
        controller.modifier,controller.flip ? "right" : "left",int(controller.pauseChord));
    g_padEnabled  = IniFloat(ini, "Controllers", "Enabled", 1) != 0.0f;
    g_padHaptics  = IniFloat(ini, "Controllers", "Haptics", 1) != 0.0f;
    g_padDeadzone = IniFloat(ini, "Controllers", "Deadzone", 0.12f);
    if (g_padDeadzone < 0.0f)  g_padDeadzone = 0.0f;
    if (g_padDeadzone > 0.6f)  g_padDeadzone = 0.6f;
    g_fireTraceEnabled = IniFloat(ini, "Debug", "FireTrace", 1) != 0.0f;
    g_maimEnabled  = IniFloat(ini, "MotionAim", "Enabled", 0) != 0.0f;
    g_asOn      = IniFloat(ini, "Aim", "SeamProbe", 0) != 0.0f;    // VR-57: read-only fire-seam probe
    g_asVerbose = IniFloat(ini, "Aim", "SeamVerbose", 0) != 0.0f;
    if (g_asOn)
        Log("config: [Aim] SeamProbe=1 - sampling the game's own aim-assist cache "
            "four times a second (read-only, aimseam: lines)%s",
            g_asVerbose ? "; verbose: quiet instances logged too" : "");
    g_asDrive = IniFloat(ini, "Aim", "DriveFromHand", 0) != 0.0f;
    g_asDriveDistUU = IniFloat(ini, "Aim", "DriveDistanceUU", 800.0f);
    g_shOn = GetPrivateProfileIntA("Aim", "ShotProbe", 0, ini) != 0;
    g_fwOn = GetPrivateProfileIntA("Aim", "FireWatch", 0, ini) != 0;
    FireAimSet(GetPrivateProfileIntA("Aim", "FireFromHand", 1, ini) != 0, "ini");
    PwSet(GetPrivateProfileIntA("Aim", "PropWatch", 0, ini) != 0, "ini");
    if (g_fwOn)
        Log("config: [Aim] FireWatch=1 - read-only. Named script dispatches before each "
            "bolt are recorded and printed. It proves ORDER and PRESENCE only; an empty "
            "list means the fire path asks nothing through script. ShotProbe is %s, and "
            "this needs it to have shots to print against.", g_shOn ? "on" : "OFF - "
            "turn it on or nothing will print");
    if (g_shOn)
        Log("config: [Aim] ShotProbe=1 - read-only bolt measurement is ON. It writes "
            "nothing to the game. The acceptance number it prints is the MISS at the "
            "visible dot's plane, not the bolt/ray angle; the drive is %s, and a "
            "drive-off run is the baseline.", g_asDrive ? "also ON" : "off");
    if (g_asDriveDistUU < 50.0f || g_asDriveDistUU > 20000.0f) g_asDriveDistUU = 800.0f;
    if (g_asDrive)
        Log("config: [Aim] DriveFromHand=1 - the controller ray is WRITTEN into the "
            "equipped weapon's aim-assist cache at %.0f uu. Shots may change; "
            "MotionAim stays separate and is %s.", (double)g_asDriveDistUU,
            g_maimEnabled ? "ALSO ON (turn it off: they fight)" : "off");
    {
        dvr::aim::Config crosshair;
        crosshair.dot = GetPrivateProfileIntA("Crosshair", "Dot", 1, ini) != 0;
        crosshair.laser = GetPrivateProfileIntA("Crosshair", "Laser", 0, ini) != 0;
        char hand[32]; GetPrivateProfileStringA("Crosshair", "Hand", "left", hand, sizeof(hand), ini);
        crosshair.hand = !_stricmp(hand, "left") ? 0 : !_stricmp(hand, "right") ? 1 : -1;
        crosshair.distanceM = IniFloat(ini, "Crosshair", "DistanceM", 8.0f);
        crosshair.sizeDeg = IniFloat(ini, "Crosshair", "SizeDeg", 0.69f);   // VR-141: the tester's run490 size
        crosshair.bothPoses = GetPrivateProfileIntA("Crosshair", "BothPoses", 0, ini) != 0;
        crosshair.controlDot = GetPrivateProfileIntA("Crosshair", "ControlDot", 0, ini) != 0;
        crosshair.rgb[0] = GetPrivateProfileIntA("Crosshair", "ColorR", 255, ini);   // VR-141: white by default
        crosshair.rgb[1] = GetPrivateProfileIntA("Crosshair", "ColorG", 255, ini);
        crosshair.rgb[2] = GetPrivateProfileIntA("Crosshair", "ColorB", 255, ini);
        crosshair.otherXDeg = IniFloat(ini, "Crosshair", "OtherItemsX", 0.0f);   // VR-189: the tester's tuned position
        crosshair.otherYDeg = IniFloat(ini, "Crosshair", "OtherItemsY", -48.0f);
        dvr::aim::configure(crosshair, ini);
        if (GetPrivateProfileIntA("Crosshair", "HideGame", 0, ini))
            Log("crosshair: HideGame is reserved and unsupported; native reticle remains visible");
        if (g_maimEnabled && (crosshair.dot || crosshair.laser))
            Log("crosshair: MotionAim is ON independently; this guide does not control its projectile ray");
    }
    {
        char hb[32];
        GetPrivateProfileStringA("MotionAim", "Hand", "left", hb, sizeof(hb), ini);
        g_maimHand = (hb[0] == 'r' || hb[0] == 'R') ? 1 : 0;
    }
    g_maimPitchOff = IniFloat(ini, "MotionAim", "PitchOffsetDeg", 40.0f);
    if (g_maimPitchOff < -90.0f) g_maimPitchOff = -90.0f;
    if (g_maimPitchOff >  90.0f) g_maimPitchOff =  90.0f;
    g_maimWindowMs = IniFloat(ini, "MotionAim", "WindowMs", 1200.0f);
    if (g_maimWindowMs < 100.0f)  g_maimWindowMs = 100.0f;
    if (g_maimWindowMs > 5000.0f) g_maimWindowMs = 5000.0f;
    g_maimMaxDist  = IniFloat(ini, "MotionAim", "MaxDistUU", 900.0f);
    if (g_maimMaxDist < 100.0f)  g_maimMaxDist = 100.0f;
    if (g_maimMaxDist > 2000.0f) g_maimMaxDist = 2000.0f;
    g_maimFlipR = IniFloat(ini, "MotionAim", "FlipRight", 0) != 0.0f ? 1 : 0;
    g_maimFlipU = IniFloat(ini, "MotionAim", "FlipUp", 0) != 0.0f ? 1 : 0;
    g_wpnDiag    = IniFloat(ini, "Weapon", "Diag", 0) != 0.0f;
    g_wpnEnabled = IniFloat(ini, "Weapon", "Enabled", 0) != 0.0f;
    g_wpnFindMesh = false;   // forced off: it auto-ran during loads and crashed
    g_wpnMaster  = IniFloat(ini, "Weapon", "Enabled", 0) != 0.0f;
    g_wpnAttach  = IniFloat(ini, "Weapon", "Attach", 0) != 0.0f;
    g_wpnRadius  = IniFloat(ini, "Weapon", "AttachRadius", 60.0f);
    if (g_wpnRadius < 5.0f)   g_wpnRadius = 5.0f;
    if (g_wpnRadius > 4000.0f) g_wpnRadius = 4000.0f;
    g_wpnShowNear= IniFloat(ini, "Weapon", "ShowNear", 0) != 0.0f;
    g_scanEnabled = IniFloat(ini, "Debug", "VsScan", 0) != 0.0f;
    g_forceNoVSync = IniFloat(ini, "Perf", "ForceNoVSync", 1) != 0.0f;
    {   // 41.1 (session 8): [Device] Ex and Managed, read before the first Direct3DCreate9
        const bool ex = IniFloat(ini, "Device", "Ex", 1) != 0.0f;
        char mm[16] = "";
        GetPrivateProfileStringA("Device", "Managed", "shadow", mm, sizeof(mm), ini);
        dvr::d3d9ex::Managed m;
        if (!dvr::d3d9ex::parse_managed(mm, &m)) { Log("config: [Device] Managed='%s' unknown (none|default|dynamic|shadow) - shadow", mm); m = dvr::d3d9ex::Managed::Shadow; }
        dvr::d3d9ex::set_config(ex, m);
        // VR-15: the surface-bypass redirect, default off, live via `device shadowsurfaces`
        dvr::census::set_shadow_surfaces(IniFloat(ini, "Device", "ShadowSurfaces", 0) != 0.0f);
        // VR-15: the per-level push, the candidate fix for black-at-distance
        dvr::d3d9ex::set_full_copy(IniFloat(ini, "Device", "ShadowFullCopy", 1) != 0.0f);
        dvr::scene_prepare::configure(IniFloat(ini, "Perf", "ScenePrepareProfile", 0) != 0.0f);
        dvr::query_profile::configure(IniFloat(ini, "Perf", "QueryWaitProfile", 0) != 0.0f);
    }
    {   // 41.1 (session 8): the tick budget's levers, both default on
        const bool inst = IniFloat(ini, "Perf", "Instruments", 1) != 0.0f;
        dvr::perf::set_cpu_scopes(GetPrivateProfileIntA("Perf", "CpuScopes", 0, ini)!=0);
        dvr::perf::set_parts(GetPrivateProfileIntA("Perf", "Parts", 0, ini)!=0);   // VR-160, default off
        dvr::gpu_memory::set_enabled(GetPrivateProfileIntA("Perf", "GpuMem", 1, ini)!=0);   // VR-160: off the present thread
        dvr::native_profile::set_enabled(GetPrivateProfileIntA("Perf", "NativeProfile", 0, ini)!=0);
        dvr::bridge_profile::set_enabled(GetPrivateProfileIntA("Perf", "BridgeGpu", 0, ini)!=0);
        const bool gpu = IniFloat(ini, "Perf", "GpuQueries", 1) != 0.0f;
        if (!inst) dvr::perf::set_enabled(false);
        if (!gpu) dvr::perf::set_gpu_enabled(false);
        const bool fid = IniFloat(ini, "Perf", "FrameId", 1) != 0.0f;   // 41.1 (session 9): the frame-identity trace
        dvr::frameid::set_enabled(fid);
        dvr::frameid::set_every((uint32_t)IniFloat(ini, "Perf", "FrameIdEvery", 8));
        const bool diagnosticAb=GetPrivateProfileIntA("Perf","DiagnosticAb",0,ini)!=0;
        dvr::perf::ab_set_enabled(!diagnosticAb && GetPrivateProfileIntA("Perf", "Ab", 0, ini) != 0);
        dvr::diag_ab::set_enabled(diagnosticAb);
        const int desktopTrial = GetPrivateProfileIntA("Perf", "DesktopAb", 0, ini);
        dvr::perf::desktop_ab_set_reduced(desktopTrial == 2);
        dvr::perf::desktop_ab_set_pacing(desktopTrial == 3);
        dvr::perf::desktop_ab_set_enabled(!diagnosticAb && desktopTrial >= 1 && desktopTrial <= 3);
        dvr::render_profile::set_enabled(GetPrivateProfileIntA("Perf", "RenderProfile", 0, ini) != 0);
        // VR-68: which head generation the HAND normalisation uses. 0 = the
        // freshest (historical); 2 = the one the rendered view was built from,
        // which is what bv/lag measured. PoseLagAb walks 0/2/0/2 so a headset
        // run decides it.
        g_mpPoseLag = GetPrivateProfileIntA("Hands", "PoseLag", 2, ini);
        g_mpPoseLagAb = GetPrivateProfileIntA("Hands", "PoseLagAb", 0, ini) != 0;
        Log("config: [Hands] PoseLag=%d PoseLagAb=%d - the head sample the hand is normalised against. 2 is the measured and headset-confirmed answer: bv/lag put the RENDERED camera at lag 2 (0.119 deg against 1.19 at lag 0 over 4085 moving frames) and a reversing A/B/A/B in a headset agreed. PoseLag=0 restores the old behaviour if you want to feel the difference.", g_mpPoseLag, (int)g_mpPoseLagAb);
        Log("config: [Perf] Instruments=%d GpuQueries=%d FrameId=%d (the tick line, the gpu line and the frameid line every 3 s)",
            inst ? 1 : 0, gpu ? 1 : 0, fid ? 1 : 0);

    }
    Log("config: per-frame diagnostics vsscan=%d shownear=%d (both off = more fps)",
        (int)g_scanEnabled, (int)g_wpnShowNear);
    g_wpnPosScale= IniFloat(ini, "Weapon", "PosScale", 55.0f);
    if (g_wpnPosScale < 0.0f)   g_wpnPosScale = 0.0f;
    if (g_wpnPosScale > 400.0f) g_wpnPosScale = 400.0f;
    g_wpnPosMax  = IniFloat(ini, "Weapon", "PosMax", 140.0f);
    if (g_wpnPosMax < 0.0f)    g_wpnPosMax = 0.0f;
    if (g_wpnPosMax > 1000.0f) g_wpnPosMax = 1000.0f;
    g_rotInject = IniFloat(ini, "HeadTrack", "Native", 1) != 0.0f;
    g_rotRoll   = IniFloat(ini, "HeadTrack", "Roll", 0) != 0.0f;
    Log("config: native head tracking %s (F3 toggles, F5 recentres)",
        g_rotInject ? "ON" : "off");
    g_wpnAutoSmall= IniFloat(ini, "Weapon", "AutoSmall", 0) != 0.0f;
    g_wpnMaxBones= (int)IniFloat(ini, "Weapon", "MaxBones", 20);
    g_wpnFpTol   = IniFloat(ini, "Weapon", "MeshTolerance", 0.35f);
    if (g_wpnFpTol < 0.02f) g_wpnFpTol = 0.02f;
    if (g_wpnFpTol > 2.0f)  g_wpnFpTol = 2.0f;
    if (g_wpnMaxBones < 1)  g_wpnMaxBones = 1;
    if (g_wpnMaxBones > 80) g_wpnMaxBones = 80;
    BlockCfgLoad();

    // 30.77: our own VR hands
    {
        // 41.2 (VR-31): back OFF. Its verdict is recorded - the renderer works
        // (the uninitialised-matrix fix) and tracks the controllers - but the
        // requirement moved to the game's OWN hands, because the powers animate
        // them, and the built-in box primitives were a distraction on screen
        // during the route (b) diagnosis. This is the FALLBACK now: `vrhands
        // on`, and real art in vrhands\*.obj, if route (b) dead-ends.
        g_hmEnable   = IniFloat(ini, "VRHands", "Enabled", 0) != 0.0f;
        // HideGameArms ships OFF with it, and that is NOT an oversight. It
        // collapses the game's own view-model rigs by upload size (30.77, the
        // vs-const path) - a second behavioural change, in the same build as
        // the first, and the author's own process rule is one per build. It
        // also makes the first run diagnostic instead of pass/fail: with the
        // game's arms still drawn, they are the reference our hands are judged
        // against. If our hands land right, this flag is the whole remaining
        // step to floating hands.
        g_hmHideGame = IniFloat(ini, "VRHands", "HideGameArms", 0) != 0.0f;
        g_hmScale    = IniFloat(ini, "VRHands", "Scale", 1.0f);
        if (g_hmScale < 0.2f) g_hmScale = 0.2f;
        if (g_hmScale > 4.0f) g_hmScale = 4.0f;
        g_hmModel[0] = (int)IniFloat(ini, "VRHands", "LeftModel", 2);
        g_hmModel[1] = (int)IniFloat(ini, "VRHands", "RightModel", 1);
        g_hmAuto = IniFloat(ini, "VRHands", "FollowEquipped", 1) != 0.0f;
        g_hmObjScale = IniFloat(ini, "VRHands", "ObjScale", 0.01f);
        g_hmHotReload = IniFloat(ini, "VRHands", "HotReload", 1) != 0.0f;
        // 41.2 (VR-31): the fallback instrument, OFF. Only worth arming if the
        // hands are still invisible once the beat says they are on-screen.
        g_hmCalib = IniFloat(ini, "VRHands", "CalibTriangle", 0) != 0.0f;
        if (g_hmObjScale < 0.0001f) g_hmObjScale = 0.0001f;
        if (g_hmObjScale > 1.0f)    g_hmObjScale = 1.0f;
        g_hmHideStatic   = IniFloat(ini, "VRHands", "HideStaticParts", 1) != 0.0f;
        g_hmHideStaticUU = IniFloat(ini, "VRHands", "HideStaticRadiusUU", 70.0f);
        if (g_hmHideStaticUU < 10.0f)  g_hmHideStaticUU = 10.0f;
        if (g_hmHideStaticUU > 400.0f) g_hmHideStaticUU = 400.0f;
        g_hmStaticDraws = (int)IniFloat(ini, "VRHands", "HideStaticDraws", 6);
        if (g_hmStaticDraws < 1)  g_hmStaticDraws = 1;
        if (g_hmStaticDraws > 32) g_hmStaticDraws = 32;
        { char hb2[128];
          GetPrivateProfileStringA("VRHands", "HideSizes", "3,6,30,36,144",
                                   hb2, sizeof(hb2), ini);
          int n3 = 0; const char* c = hb2;
          for (int q = 0; q < 12; q++) g_hmHideSize[q] = 0;
          while (*c && n3 < 12) {
              while (*c == ' ' || *c == ',') c++;
              if (!*c) break;
              int v4 = atoi(c);
              if (v4 >= 1 && v4 <= 250) g_hmHideSize[n3++] = (UINT)v4;
              while (*c && *c != ',') c++;
          } }
        { static const char* mr[3] = { "Yaw", "Pitch", "Roll" };
          static const char* mp[3] = { "X", "Y", "Z" };
          for (int mi = 1; mi < HM_COUNT; mi++)
            for (int q = 0; q < 3; q++) {
                char k[32];
                _snprintf(k, sizeof(k), "M%d%s", mi, mr[q]);
                g_hmMRot[mi][q] = IniFloat(ini, "VRHands", k, 0.0f);
                _snprintf(k, sizeof(k), "M%dPos%s", mi, mp[q]);
                g_hmMPos[mi][q] = IniFloat(ini, "VRHands", k, 0.0f);
            } }
        static const char* pk[3] = { "PosX", "PosY", "PosZ" };
        static const char* rk[3] = { "Yaw", "Pitch", "Roll" };
        for (int hh = 0; hh < 2; hh++)
            for (int q = 0; q < 3; q++) {
                char k[32];
                _snprintf(k, sizeof(k), "%s%s", hh ? "R" : "L", pk[q]);
                g_hmPos[hh][q] = IniFloat(ini, "VRHands", k, 0.0f);
                _snprintf(k, sizeof(k), "%s%s", hh ? "R" : "L", rk[q]);
                g_hmRot[hh][q] = IniFloat(ini, "VRHands", k, 0.0f);
            }
        Log("config: VR hands %s (hide game arms=%d, models L=%d R=%d, scale %.2f)",
            g_hmEnable ? "ON" : "off", (int)g_hmHideGame,
            g_hmModel[0], g_hmModel[1], g_hmScale);
    }

    // 30.70: the render-time hand/weapon drive
    {
        g_rtdEnable = IniFloat(ini, "HandRender", "Enabled",   1) != 0.0f;
        g_rtdDoArms = IniFloat(ini, "HandRender", "DriveArms", 1) != 0.0f;
        g_rtdDoWpn  = IniFloat(ini, "HandRender", "DriveWeapon", 1) != 0.0f;
        int wr = (int)IniFloat(ini, "HandRender", "WeaponRegs", 36);
        int ar = (int)IniFloat(ini, "HandRender", "ArmsRegs",  144);
        // 0 is legal and means "drive nothing here" (the identifier's release
        // button), so it must survive the clamp.
        g_rtdSizeWpn  = (wr == 0 || (wr >= 3 && wr <= 250)) ? (UINT)wr : 36;
        g_rtdSizeArms = (ar == 0 || (ar >= 3 && ar <= 250)) ? (UINT)ar : 144;
        char hb[32];
        GetPrivateProfileStringA("HandRender", "Hand", "right", hb, sizeof(hb), ini);
        g_rtdArmsHand = (hb[0] == 'l' || hb[0] == 'L') ? 0 : 1;
        int w2 = (int)IniFloat(ini, "HandRender", "Weapon2Regs", 0);
        g_rtdSizeWpn2 = (w2 == 0 || (w2 >= 3 && w2 <= 250)) ? (UINT)w2 : 0;
        g_rtdWpnHand  = IniFloat(ini, "HandRender", "WeaponHand",  1) != 0.0f ? 1 : 0;
        g_rtdWpn2Hand = IniFloat(ini, "HandRender", "Weapon2Hand", 0) != 0.0f ? 1 : 0;
        g_rtdSplitLo  = (int)IniFloat(ini, "HandRender", "RightArmFirstBone", 0);
        g_rtdSplitHi  = (int)IniFloat(ini, "HandRender", "RightArmLastBone",  0);
        if (g_rtdSplitLo < 0)  g_rtdSplitLo = 0;
        if (g_rtdSplitHi < 0)  g_rtdSplitHi = 0;
        if (g_rtdSplitLo > 90) g_rtdSplitLo = 90;
        if (g_rtdSplitHi > 90) g_rtdSplitHi = 90;
        g_rtdMarkers  = IniFloat(ini, "HandRender", "ShowRings", 0) != 0.0f;
        g_rtdMarkSize = IniFloat(ini, "HandRender", "RingSizeMeters", 0.045f);
        if (g_rtdMarkSize < 0.01f) g_rtdMarkSize = 0.01f;
        if (g_rtdMarkSize > 0.15f) g_rtdMarkSize = 0.15f;
        g_rtdFollowYaw   = IniFloat(ini, "HandRender", "FollowHeadYaw",   1.0f);
        g_rtdFollowPitch = IniFloat(ini, "HandRender", "FollowHeadPitch", 0.0f);
        for (int q = 0; q < 3; q++) {
            char k[32];
            _snprintf(k, sizeof(k), "Axis%dSource", q);
            int v3 = (int)IniFloat(ini, "HandRender", k, (float)g_rtdMapSrc[q]);
            g_rtdMapSrc[q] = (v3 >= 0 && v3 <= 2) ? v3 : g_rtdMapSrc[q];
            _snprintf(k, sizeof(k), "Axis%dFlip", q);
            g_rtdMapSgn[q] = IniFloat(ini, "HandRender", k, 0.0f) != 0.0f ? -1.0f : 1.0f;
        }
        if (g_rtdFollowYaw   < 0.0f) g_rtdFollowYaw   = 0.0f;
        if (g_rtdFollowYaw   > 1.0f) g_rtdFollowYaw   = 1.0f;
        if (g_rtdFollowPitch < 0.0f) g_rtdFollowPitch = 0.0f;
        if (g_rtdFollowPitch > 1.0f) g_rtdFollowPitch = 1.0f;
        g_rtdUseOrdinals = IniFloat(ini, "HandRender", "RouteByDrawOrder", 0) != 0.0f;
        { char ob[64];
          GetPrivateProfileStringA("HandRender", "DrawOrderHands", "", ob, sizeof(ob), ini);
          // "-1,0,1,-1" style: one entry per draw, -1 none / 0 left / 1 right
          int q = 0; const char* c = ob;
          while (*c && q < 8) {
              while (*c == ' ' || *c == ',') c++;
              if (!*c) break;
              int v2 = atoi(c);
              g_rtdOrdHand[q++] = (v2 == 0) ? 0 : (v2 == 1 ? 1 : -1);
              while (*c && *c != ',') c++;
          } }
        g_rtdPivotMix = IniFloat(ini, "HandRender", "PivotMix", 1.0f);
        if (g_rtdPivotMix < 0.0f) g_rtdPivotMix = 0.0f;
        if (g_rtdPivotMix > 1.0f) g_rtdPivotMix = 1.0f;
        g_rtdScaleUU = IniFloat(ini, "HandRender", "ScaleUU", 0.0f);
        if (g_rtdScaleUU < 0.0f)   g_rtdScaleUU = 0.0f;
        if (g_rtdScaleUU > 400.0f) g_rtdScaleUU = 400.0f;
        g_rtdPosMax = IniFloat(ini, "HandRender", "MaxOffsetUU", 120.0f);
        if (g_rtdPosMax < 0.0f)    g_rtdPosMax = 0.0f;
        if (g_rtdPosMax > 1000.0f) g_rtdPosMax = 1000.0f;
        g_rtdSmooth = IniFloat(ini, "HandRender", "SmoothAlpha", 0.0f);
        if (g_rtdSmooth < 0.0f)  g_rtdSmooth = 0.0f;
        if (g_rtdSmooth > 0.95f) g_rtdSmooth = 0.95f;
        g_rtdTrim[0][0] = IniFloat(ini, "HandRender", "LTrimX", 0.0f);
        g_rtdTrim[0][1] = IniFloat(ini, "HandRender", "LTrimY", 0.0f);
        g_rtdTrim[0][2] = IniFloat(ini, "HandRender", "LTrimZ", 0.0f);
        g_rtdTrim[1][0] = IniFloat(ini, "HandRender", "RTrimX", 0.0f);
        g_rtdTrim[1][1] = IniFloat(ini, "HandRender", "RTrimY", 0.0f);
        g_rtdTrim[1][2] = IniFloat(ini, "HandRender", "RTrimZ", 0.0f);
        g_rtdWpnYPR[0] = IniFloat(ini, "HandRender", "WpnYaw",   0.0f);
        g_rtdWpnYPR[1] = IniFloat(ini, "HandRender", "WpnPitch", 0.0f);
        g_rtdWpnYPR[2] = IniFloat(ini, "HandRender", "WpnRoll",  0.0f);
        g_rtdRotInvert = IniFloat(ini, "HandRender", "RotInvert", 0) != 0.0f;
        g_rtdRotScale  = IniFloat(ini, "HandRender", "RotScale", 1.0f);
        if (g_rtdRotScale < 0.0f) g_rtdRotScale = 0.0f;
        if (g_rtdRotScale > 1.0f) g_rtdRotScale = 1.0f;
        g_rtdPivotUp = IniFloat(ini, "HandRender", "PivotUp", 0.0f);
        if (g_rtdPivotUp < -300.0f) g_rtdPivotUp = -300.0f;
        if (g_rtdPivotUp >  300.0f) g_rtdPivotUp =  300.0f;
        Log("config: hand render drive %s (arms=%d c6 x%u, weapon=%d c6 x%u, "
            "split %d-%d, pivot %.2f, scale %s, max %.0fuu)",
            g_rtdEnable ? "ON" : "off", (int)g_rtdDoArms, g_rtdSizeArms,
            (int)g_rtdDoWpn, g_rtdSizeWpn, g_rtdSplitLo, g_rtdSplitHi,
            g_rtdPivotMix, g_rtdScaleUU > 1.0f ? "fixed" : "world", g_rtdPosMax);
    }

    Log("config: weapon attach=%d radius=%.0fuu shownear=%d",
        (int)g_wpnAttach, g_wpnRadius, (int)g_wpnShowNear);
    g_wpnTestYaw = IniFloat(ini, "Weapon", "TestYawDeg", 0.0f);
    g_wpnFlipX   = IniFloat(ini, "Weapon", "FlipX", 0) != 0.0f ? 1 : 0;
    g_wpnFlipY   = IniFloat(ini, "Weapon", "FlipY", 0) != 0.0f ? 1 : 0;
    Log("config: weapon diag=%d enabled=%d testyaw=%.0f flipx=%d flipy=%d",
        (int)g_wpnDiag, (int)g_wpnEnabled, g_wpnTestYaw, g_wpnFlipX, g_wpnFlipY);
    Log("config: motionaim=%d hand=%s pitchoff=%.0f window=%.0fms maxdist=%.0fuu flipR=%d flipU=%d",
        (int)g_maimEnabled, g_maimHand ? "right" : "left", g_maimPitchOff,
        g_maimWindowMs, g_maimMaxDist, g_maimFlipR, g_maimFlipU);
    // 30.97: the hands drive that actually works - Arkane's own per-hand
    // SkelControls, driven from the controllers.
    g_skcDrive   = IniFloat(ini, "Hands", "Enabled", 1) != 0.0f;
    g_skcLive    = IniFloat(ini, "Hands", "FromControllers", 1) != 0.0f;
    g_skcWorld   = IniFloat(ini, "Hands", "WorldSpace", 0) != 0.0f;
    g_skcDoTrans = IniFloat(ini, "Hands", "Position", 1) != 0.0f;
    g_skcDoRot   = IniFloat(ini, "Hands", "Rotation", 0) != 0.0f;
    g_skcWorldRot= IniFloat(ini, "Hands", "WorldRotation", 0) != 0.0f;
    g_skcRollGain= IniFloat(ini, "Hands", "RollGain", 1.0f);
    g_skcAddMode = IniFloat(ini, "Hands", "AddToAnim", 1) != 0.0f;
    g_skcScaleUU = IniFloat(ini, "Hands", "ScaleUU", 50.0f);
    g_skcMax     = IniFloat(ini, "Hands", "ClampUU", 120.0f);
    g_skcSpace   = (int)IniFloat(ini, "Hands", "Space", 3);
    g_skcCounterYaw = IniFloat(ini, "Hands", "CounterHeadYaw", 0.0f);
    g_skcHandSize   = IniFloat(ini, "Hands", "HandSize", 1.0f);
    g_skcRemoveMeshRot = IniFloat(ini, "Hands", "RemoveMeshRotation", 0) != 0.0f;
    g_skcCamStrength   = IniFloat(ini, "Hands", "CameraLookAtStrength", 1.0f);
    g_skcHandCtlStr[0] = IniFloat(ini, "Hands", "LeftControlStrength", 1.0f);
    g_skcHandCtlStr[1] = IniFloat(ini, "Hands", "RightControlStrength", 1.0f);
    g_skcWorldScale = IniFloat(ini, "Hands", "WorldScaleUU", 100.0f);
    // 32.12: a saved neutral means the hands land in the same place every
    // launch, so the trim is calibrated once and then left alone.
    // 32.27: MEASURED WORKING - Blink lands where the controller points.
    g_blkAimOnCfg = IniFloat(ini, "Blink", "ControllerAim", 1) != 0.0f;
    // 32.32: back ON by default. The user's key fact - the centre-blindness
    // predates controller aiming and started when stereo went in - rules out
    // "the point has no surface under it" as the cause. It is the draw's
    // screen-space sampling versus the splice, which is a shader-constant
    // problem, not a gate problem. Our own marker sidesteps the engine's decal
    // entirely, and with 32.31 tracing down the controller ray it now sits on
    // the real landing spot rather than an approximation.
    g_blkMarker   = IniFloat(ini, "Blink", "Marker", 1) != 0.0f;
    // 32.33: DEFAULT OFF. 32.31 shipped the trace redirect as the default AND
    // disabled the working destination patch in the same build - so if the new
    // path did not take, aiming fell back to head aim with nothing driving it.
    // That is exactly what happened. Never replace a working path with an
    // unverified one in a single build; make the new one opt-in until it is
    // measured.
    // 32.36: back ON - it is the correct architecture and it can no longer
    // cost us aiming. The head-coupled DISTANCE is why the marker slides
    // forward and back when you tilt your head: the direction is the
    // controller's but the length still comes from the engine's trace along
    // the VIEW, so looking at the floor shortens it and looking at the sky
    // stretches it. Redirecting the trace fixes the length, the surface the
    // decal needs, and the duplicate marker, all at once.
    g_blkTraceAim = IniFloat(ini, "Blink", "RedirectTrace", 0) != 0.0f;
    // VR-36: DEFAULT 0 - the engine's own reach for this activation, with only
    // the DIRECTION taken from the controller. The engine's aim vector carries
    // its reach rule in its magnitude, including a hard +500 uu vertical cap
    // measured 2026-09-13, so keeping the magnitude keeps the rule. Modes 1 and
    // 2 replace it; they can now only shorten it (see BlinkReach), but 0 is the
    // one that reproduces the game's own distances exactly.
    g_blkReachMode  = (int)IniFloat(ini, "Blink", "ReachMode", 0);
    if (g_blkReachMode < 0 || g_blkReachMode > 2) g_blkReachMode = 2;
    g_blkReachUU    = IniFloat(ini, "Blink", "ReachUU", 0.0f);
    g_blkNearUU     = IniFloat(ini, "Blink", "NearUU", 150.0f);
    g_blkPitchNear  = IniFloat(ini, "Blink", "PitchNearDeg", -55.0f);
    g_blkPitchFar   = IniFloat(ini, "Blink", "PitchFarDeg",   -5.0f);
    g_blkMarkerBackUU = IniFloat(ini, "Blink", "MarkerPullbackUU", 60.0f);
    g_blkDirAim       = IniFloat(ini, "Blink", "AimAtSource", 1) != 0.0f;
    // VR-36: which ray Blink aims with. 1 = the published ray, the same
    // publication the crosshair dot, the laser and the crossbow/pistol launch
    // hooks consume. 0 = the legacy MotionAim ray (grip pose plus
    // [MotionAim] PitchOffsetDeg), kept as a named A/B and never reached as a
    // silent fallback - a refusal of the published ray leaves the ENGINE's own
    // head aim in place, because what is being aimed is where the player lands.
    g_blkUseAimRay    = IniFloat(ini, "Blink", "UseAimRay", 1) != 0.0f;
    g_blkDriveUI  = g_blkAimOnCfg;
    g_aimAllPowers = IniFloat(ini, "Blink", "AimAllPowers", 1) != 0.0f;  // 38.52
    Log("config: [Blink] ControllerAim=%d UseAimRay=%d AimAtSource=%d ReachMode=%d "
        "- Blink aims with the %s ray, redirected at its SOURCE (0xbf55a3) so the "
        "engine still traces, collides and refuses; the destination seam "
        "(0xbf5e4f) is READ-ONLY since VR-36",
        (int)g_blkAimOnCfg, (int)g_blkUseAimRay, (int)g_blkDirAim, g_blkReachMode,
        g_blkAimOnCfg ? (g_blkUseAimRay ? "published (shared with the dot and the shots)"
                                        : "legacy MotionAim")
                      : "engine's own head");
    g_skcCrouchTrimOn = IniFloat(ini, "Hands", "PerStanceTrim", 1) != 0.0f;
    g_crouchSrc       = (int)IniFloat(ini, "Hands", "CrouchSource", 3);
    if (g_crouchSrc < 0 || g_crouchSrc > 3) g_crouchSrc = 3;
    // 32.41: eye height is measurably dead (camZ-pawnZ was flat at 76-78 uu
    // across 1858 of 1995 samples), so an ini left on source 1 by an earlier
    // build is migrated rather than silently kept on a signal we have proven
    // carries no stance information.
    if (g_crouchSrc == 1) {
        g_crouchSrc = 3;
        Log("config: crouch source migrated from eye height to the crouch button");
    }
    g_eyeDropUU       = IniFloat(ini, "Hands", "CrouchDropUU", 20.0f);
    g_crouchHoldMs    = IniFloat(ini, "Hands", "CrouchHoldMs", 250.0f);
    // 32.93: default OFF. It answered its question weeks ago (which crouch
    // signal is real) and has been printing five lines a second ever since.
    g_crouchDiag      = IniFloat(ini, "Hands", "CrouchDiag", 0) != 0.0f;
    g_skcRotSignY = IniFloat(ini, "Hands", "RotSignYaw", 1) < 0 ? -1 : 1;
    g_skcRotSignP = IniFloat(ini, "Hands", "RotSignPitch", 1) < 0 ? -1 : 1;
    // 35.8: the donor-graft rotation drive. GraftRotation=1 arms the WISH -
    // the graft engages once the rig probe finds controls and donors.
    g_graftWant     = IniFloat(ini, "Hands", "GraftRotation", 0) != 0.0f;
    g_graftRotSpace = (int)IniFloat(ini, "Hands", "GraftRotSpace", 0);
    if (g_graftRotSpace < 0 || g_graftRotSpace > 4) g_graftRotSpace = 0;
    g_graftHeadComp = IniFloat(ini, "Hands", "GraftHeadComp", 1) != 0.0f;  // 35.9
    g_graftAimAbs   = IniFloat(ini, "Hands", "GraftAimAbs", 1) != 0.0f;    // 36.4
    g_graftHCY = IniFloat(ini, "Hands", "GraftHeadFollowYaw", 1.5f);       // 36.5
    g_graftHCP = IniFloat(ini, "Hands", "GraftHeadFollowPitch", 1.5f);
    if (g_graftHCY < -2.0f || g_graftHCY > 2.0f) g_graftHCY = 1.5f;
    if (g_graftHCP < -2.0f || g_graftHCP > 2.0f) g_graftHCP = 1.5f;
    g_blkProbeForce   = IniFloat(ini, "Blink", "BlinkProbe", 0) != 0.0f;
    g_crouchToggle    = IniFloat(ini, "Hands", "CrouchToggle", 0) != 0.0f;
    g_elixirOn     = IniFloat(ini, "Input", "HealthElixirLongPress", 1) != 0.0f;  // 36.6
    g_elixirHoldMs = IniFloat(ini, "Input", "HealthElixirHoldMs", 400.0f);  // 36.7:
    if (g_elixirHoldMs < 150.0f)  g_elixirHoldMs = 150.0f;  // dedicated input now -
                                                            // shorter hold suffices
    if (g_elixirHoldMs > 3000.0f) g_elixirHoldMs = 3000.0f;
    {
        char kb[8] = "";
        GetPrivateProfileStringA("Input", "HealthElixirKey", "R", kb, 8, ini);
        char c = kb[0];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) g_elixirVk = c;
    }
    Log("config: health elixir long-press %s (hold %.0f ms, key %c)",
        g_elixirOn ? "ON" : "off", g_elixirHoldMs, (char)g_elixirVk);
    g_crouchBtnMask   = (uint32_t)IniFloat(ini, "Hands", "CrouchButtonMask", 0x2000);
    if (!g_crouchBtnMask) g_crouchBtnMask = 0x2000;
    // 32.44: one-shot reset. Builds up to 32.43 could adopt a button that
    // merely happened to be pressed near a blink, and that adoption is saved
    // to the ini - so it survives the fix unless it is explicitly cleared.
    if (IniFloat(ini, "Hands", "CrouchMaskVer", 0) < 2.0f) {
        if (g_crouchBtnMask != 0x2000) {
            Log("config: crouch button mask reset to 0x2000 (was 0x%04x) -"
                " earlier builds could learn the wrong button",
                (unsigned)g_crouchBtnMask);
            g_crouchBtnMask = 0x2000;
        }
    }
    for (int hh = 0; hh < 2; hh++) {
        static const char* kAx2[3] = { "Fwd", "Right", "Up" };
        for (int q = 0; q < 3; q++) {
            char k[64];
            // 32.39: NEW key. The old Crouch?Trim? keys held absolute values
            // under the replace model; reading them as offsets would double
            // the trim, so they are deliberately left behind.
            _snprintf(k, 64, "CrouchOff%c%s", hh ? 'R' : 'L', kAx2[q]);
            g_skcTrimCrouch[hh][q] = IniFloat(ini, "Hands", k, 0.0f);
            // 34.9: standing-BLOCK offset, same shape (defaults 0 = no
            // change until tuned). Applies only while blocking un-crouched.
            _snprintf(k, 64, "BlockOff%c%s", hh ? 'R' : 'L', kAx2[q]);
            g_skcTrimBlock[hh][q] = IniFloat(ini, "Hands", k, 0.0f);
        }
    }
    g_skcBlockTrimOn = IniFloat(ini, "Hands", "BlockTrim", 1) != 0.0f;
    g_crawlTuckCfg   = IniFloat(ini, "Hands", "CrawlTuck", 1) != 0.0f;  // 38.19
    {
        // VR-122: both halves of the crouched tuck, resolved and logged with where
        // each came from (TRAPS section 1: an ini key that exists beats the default).
        const int tuckKey = GetPrivateProfileIntA("Hands", "CrawlTuck", -1, ini);
        const int camKey  = GetPrivateProfileIntA("Hands", "CrawlTuckCamera", -1, ini);
        g_crawlTuckCamera = camKey > 0;
        Log("config: [Hands] CrawlTuck=%d (%s) CrawlTuckCamera=%d (%s) - a crouch (capsule below 76 uu) "
            "releases the hand look-at controls to the game's animation; the camera's look-at control is %s. "
            "VR-122: releasing it too held the crouched view level while the head pitched. `hands tuckcam on|off` "
            "is the live A/B; the tuck edge logs `hands/crawl-strength` with what it did to LookAtControl_Camera.",
            (int)g_crawlTuckCfg, tuckKey < 0 ? "absent from the ini, the built-in default" : "from the ini",
            (int)g_crawlTuckCamera, camKey < 0 ? "absent from the ini, the built-in default" : "from the ini",
            g_crawlTuckCamera ? "RELEASED too (the pre-VR-122 behaviour)" : "left alone");
    }
    g_slideAssist    = IniFloat(ini, "Input", "SlideAssist", 1) != 0.0f; // 38.22
    // First-fault evidence is enabled in the explicitly requested tested profile.
    // Set Diagnostics/GcFaultDump=0 to disable full-memory capture.
    dvr::crash::configure_read_fault_dump(
        GetPrivateProfileIntA("Diagnostics", "GcFaultDump", 1, ini) ? kGcReferenceReadFault : 0,
        kGcReferenceReadBytes, sizeof(kGcReferenceReadBytes));

    // VR-78: vertical accounting is part of the explicitly promoted tested
    // profile. Missing keys now enable it; explicit user overrides still win.
    {
        const int za = GetPrivateProfileIntA("PosTrack", "ZAccount", 1, ini);
        if (za >= 0) dvr::zacct::set_enabled(za != 0, "[PosTrack] ZAccount in the ini");
        // VR-80: the pair trace - bounded per-present lines joining the ring's eye,
        // the eye chosen, the camera write the tag carries and c5, after a return
        // to gameplay and around a pairing override. Never saved.
        const int pt = GetPrivateProfileIntA("Stereo", "PairTrace", 1, ini);
        if (pt >= 0) dvr::zacct::set_trace(pt != 0, "[Stereo] PairTrace in the ini");
        // VR-80: count the viewport draw root's other three callers through
        // pass-through stubs, and annotate each traced present with what drew
        // since the last one. Diagnostic; the sites are restored when off.
        // VR-80: the ring ledger - one record per present of what entered and left the tag
        // ring, printed in bounded windows, with a reconcile of every tag. Never saved.
        dvr::stereo::set_reentry_ledger(GetPrivateProfileIntA("Stereo", "RingLedger", 1, ini) != 0);
        const int dc = GetPrivateProfileIntA("Stereo", "DrawCallerTrace", 1, ini);
        DrawCallersSet(dc != 0);
        if (dc) Log("config: [Stereo] DrawCallerTrace=1 - the draw root's other callers will be counted "
                    "from the next script dispatch");
        // VR-91: which QUESTION the probe is answering. The pitch mode rejects
        // any sample rolled past 12 degrees, so it cannot see a roll fault at
        // all; roll mode bins by head roll and measures laterally instead. Same
        // rule as the key above - absent means the compiled default, and neither
        // is ever materialised into a player ini by a save.
        const int zr = GetPrivateProfileIntA("PosTrack", "ZAccountRoll", 0, ini);
        if (zr >= 0) dvr::zacct::set_roll_mode(zr != 0, "[PosTrack] ZAccountRoll in the ini");
        Log("config: [PosTrack] ZAccount=%d ZAccountRoll=%d - %s, accounting the %s",
            za > 0 ? 1 : 0, zr > 0 ? 1 : 0,
            za < 0 ? "absent, compiled default off" : "from the ini",
            dvr::zacct::roll_mode() ? "LATERAL residual against head ROLL (VR-91)"
                                    : "VERTICAL residual against camera PITCH (VR-78)");
    }
    g_eyeClampCfg    = IniFloat(ini, "PosTrack", "EyeClamp", 0) != 0.0f; // 38.24; off since the 2026-09-19 run
    g_eyeClampMargin = IniFloat(ini, "PosTrack", "EyeClampMargin", 8.0f);
    if (g_eyeClampMargin < 2.0f)  g_eyeClampMargin = 2.0f;
    if (g_eyeClampMargin > 30.0f) g_eyeClampMargin = 30.0f;
    g_eyeClampRate   = IniFloat(ini, "PosTrack", "EyeClampRate", 300.0f); // 38.26
    if (g_eyeClampRate < 0.0f)     g_eyeClampRate = 0.0f;   // 0 = instant (38.24)
    if (g_eyeClampRate > 4000.0f)  g_eyeClampRate = 4000.0f;
    if (g_eyeClampRate > 0.0f && g_eyeClampRate < 40.0f) g_eyeClampRate = 40.0f;
    g_sprintHoldCfg   = IniFloat(ini, "Input", "SprintHold", 0) != 0.0f;    // 38.28, off by default from 38.29
    g_sprintPulseMs   = IniFloat(ini, "Input", "SprintPulseMs", 130.0f);
    if (g_sprintPulseMs < 60.0f)  g_sprintPulseMs = 60.0f;
    if (g_sprintPulseMs > 400.0f) g_sprintPulseMs = 400.0f;
    g_crouchHideCfg   = IniFloat(ini, "Hands", "CrouchHideArms", 1) != 0.0f; // 38.29
    // VR-31 route (a). Back to OFF now the verdict is in: `arms vis status`
    // runs the diagnostic on demand, and there is no array left to write to.
    g_boneVisCfg      = IniFloat(ini, "Hands", "BoneVisHide", 0) != 0.0f;
    // VR-31 route (d). Read-only census, ships ON: it costs one walk per run
    // and it is what the route decision is waiting on.
    g_matCensusCfg    = IniFloat(ini, "Hands", "MatCensus", 1) != 0.0f;
    // VR-31 route (d): the automatic hide/restore A/B. Ships ON and undoes
    // itself; MatAuto=0 leaves the census read-only.
    g_matAutoCfg      = IniFloat(ini, "Hands", "MatAuto", 0) != 0.0f;
    g_matCycleCfg     = IniFloat(ini, "Hands", "MatCycle", 0) != 0.0f;
    // 41.2 (VR-31) route (b) step 1. ON for the test sessions: read-only until
    // a numpad key is pressed, and a run that shows nothing is itself the
    // answer. Reverts to OFF when the arm/hand split is settled.
    g_dcOn            = IniFloat(ini, "Hands", "DrawCensus", 1) != 0.0f;
    // VR-31 step 2: the arm/hand split derived from bone influence. ON by
    // default and auto-arming, because the eyeballed slice mask it replaces was
    // tuned on one hand and took too much of the other - a cut derived from the
    // mesh's own skinning is per-arm by construction. It fails soft: a mesh it
    // cannot read is drawn exactly as the game asked, and the log says why.
    g_msOn            = IniFloat(ini, "Hands", "ArmSplit", 1) != 0.0f;
    g_msAuto          = IniFloat(ini, "Hands", "ArmSplitAuto", 1) != 0.0f;
    g_msMode          = (int)IniFloat(ini, "Hands", "ArmSplitMode", (float)MS_MODE_HANDS);
    if (g_msMode < 0 || g_msMode >= MS_MODE_N) g_msMode = MS_MODE_HANDS;
    // The measured shape of Corvo's first-person arm mesh. The auto-arm matches
    // on it exactly, so a different asset is left alone; change these two if the
    // log says the signature never matched.
    g_msWantPrims     = (uint32_t)IniFloat(ini, "Hands", "ArmMeshPrims", 4448.0f);
    g_msWantVerts     = (uint32_t)IniFloat(ini, "Hands", "ArmMeshVerts", 2771.0f);
    // 0.70 is the tester's measured cut from the 2026-09-06 headset run. It is
    // the SEED for the plane's starting position, not the cut itself.
    g_msWristScale[1] = IniFloat(ini, "Hands", "WristScaleA", 0.70f);
    g_msWristScale[2] = IniFloat(ini, "Hands", "WristScaleB", 0.70f);
    // The cut is a PLANE across the forearm, not a sphere around the hand bone.
    // The sphere moves in whole bones - a press changed nothing at all from
    // scale 0.50 to 0.67, then flipped 156 triangles per arm at once - and its
    // edge is the outline of a bone's influence region rather than a cut. A
    // plane perpendicular to the forearm cuts it in a circle and moves smoothly.
    g_msPlane         = IniFloat(ini, "Hands", "WristPlane", 1) != 0.0f;
    g_msCap           = IniFloat(ini, "Hands", "CutCap", 1) != 0.0f;
    g_msRoundWrist    = IniFloat(ini, "Hands", "RoundedWrist", 0) != 0.0f;
    g_msRoundDepth    = fminf(.8f,fmaxf(.05f,IniFloat(ini,"Hands","RoundedWristDepth",.57f)));
    g_msCapTwo        = IniFloat(ini, "Hands", "CutCapTwoSided", 1) != 0.0f;
    // VR-33 step 1. READ-ONLY, so it ships ON: it resolves engine names and
    // reports what it could not find, and writes nothing anywhere.
    g_prOn            = IniFloat(ini, "Hands", "PoseReport", 1) != 0.0f;
    // VR-33 step 1b. This one MAKES ENGINE CALLS, so it ships OFF and is
    // separate from the read-only report, which keeps working either way.
#if DVR_WITH_LEGACY
    g_bqOn            = IniFloat(ini, "Hands", "BoneQuery", 0) != 0.0f;
#endif
    // VR-33 phase 1. This one WRITES to the skeleton, so it ships OFF and
    // restores every field it touched when it is switched off.
#if DVR_WITH_LEGACY
    g_hmOn            = IniFloat(ini, "Hands", "HandMoveTest", 0) != 0.0f;
    g_hmAmount        = IniFloat(ini, "Hands", "HandMoveUU", 10.0f);
    g_hmAxis          = (int)IniFloat(ini, "Hands", "HandMoveAxis", 0);
    if (g_hmAxis < 0 || g_hmAxis > 2) g_hmAxis = 0;
#endif
    // VR-33: the draw-scoped bone palette. A RENDER LEVER with a live A/B -
    // Palette=0 leaves MsDraw issuing the single merged draw it always did, and
    // nothing in the frame path changes. VR-72: it ships ON with the rest of the
    // headset-confirmed configuration the defaults now match (see ARCHITECTURE).
    g_mpOn            = IniFloat(ini, "Hands", "Palette", 1) != 0.0f;
#if DVR_WITH_LEGACY
    g_mpAmount        = IniFloat(ini, "Hands", "PaletteAmount", 20.0f);
    g_mpAxis          = (int)IniFloat(ini, "Hands", "PaletteAxis", 1);
    if (g_mpAxis < 0 || g_mpAxis > 2) g_mpAxis = 1;
    g_mpHand          = (int)IniFloat(ini, "Hands", "PaletteHand", 0);
    if (g_mpHand < 0 || g_mpHand > 2) g_mpHand = 0;
#endif
    g_mpDriveGain     = IniFloat(ini, "Hands", "PaletteDriveGain", 1.0f);
    if (g_mpDriveGain < 0.05f) g_mpDriveGain = 0.05f;
    if (g_mpDriveGain > 5.0f)  g_mpDriveGain = 5.0f;
    g_mpWorld         = IniFloat(ini, "Hands", "PaletteWorld", 1) != 0.0f;
    g_mpEyeHunt       = IniFloat(ini, "Hands", "PaletteEyeHunt", 0) != 0.0f;
    g_mpDepth         = IniFloat(ini, "Hands", "PaletteDepthRange", 1) != 0.0f;
    g_mpEyeOffset     = IniFloat(ini, "Hands", "PaletteEyeOffset", 1) != 0.0f;
    // VR-95: on a jump too small to read, predict the toggle instead of holding
    // the previous eye. Ships OFF as a new lever must; the fault it removes was
    // measured on the headset as 90 flagged presents, every one of them the
    // classifier saying RIGHT while the tag said LEFT.
    g_mpEyeMenuHalfStep = IniFloat(ini,"Hands","PaletteEyeMenuHalfStep",0)!=0;
    Log("config: [Hands] PaletteEyeMenuHalfStep=%d - menu signed half-IPD jump candidate; no toggle prediction",(int)g_mpEyeMenuHalfStep);
    g_mpEyePredict    = IniFloat(ini, "Hands", "PaletteEyePredictToggle", 0) != 0.0f;
    Log("config: [Hands] PaletteEyePredictToggle=%d - an unreadable eye jump %s. "
        "Holding was measured robbing the LEFT eye's hands of their own half-IPD "
        "during a head roll (VR-95); the prediction is capped at two in a row so a "
        "genuinely non-alternating stream still holds.",
        (int)g_mpEyePredict,
        g_mpEyePredict ? "PREDICTS the toggle" : "holds the previous eye (pre-VR-95)");
#if DVR_WITH_LEGACY
    g_pcOn            = IniFloat(ini, "Hands", "PaletteCapture", 0) != 0.0f;
#endif
    g_mpWsumTol       = IniFloat(ini, "Hands", "PaletteWeightTol", 0.02f);
    if (g_mpWsumTol < 0.0001f) g_mpWsumTol = 0.0001f;
#if DVR_WITH_LEGACY
    g_mpStep          = IniFloat(ini, "Hands", "PaletteStep", 0) != 0.0f;
#endif
    // VR-33 rotation and grip. PaletteRotate defaults OFF here per the project
    // rule for a new render lever; the installed ini turns it on for the run
    // that is testing it, and the previous stage stays reachable by turning it
    // back off.
    g_mpRotate        = IniFloat(ini, "Hands", "PaletteRotate", 1) != 0.0f;
    g_mpAnchorHandBone = IniFloat(ini, "Hands", "AnchorBone", 1) != 0.0f;   // VR-183: palm frame from the hand bone
    g_msRigidWrist = IniFloat(ini, "Hands", "RigidWrist", 1) != 0.0f;         // VR-184: the wrist cut and cap rigid with the hand
    // VR-33: attachment matches owned component transforms independently of
    // the optional hide sweep. Installed test configuration enables it;
    // a fresh configuration leaves this render lever off.
    g_waOn            = IniFloat(ini, "Hands", "AttachWeapons", 1) != 0.0f;
    g_waSwordHand     = (int)IniFloat(ini, "Hands", "AttachSwordHand", 1);
    g_waXbowHand      = (int)IniFloat(ini, "Hands", "AttachCrossbowHand", 0);
    g_waAngTolDeg     = IniFloat(ini, "Hands", "AttachAngleTol", 0.25f);
    g_waPosTolUU      = IniFloat(ini, "Hands", "AttachPosTol",   1.0f);
    g_waMarginX       = IniFloat(ini, "Hands", "AttachMargin",   4.0f);
    g_waMaxTry        = (int)IniFloat(ini, "Hands", "AttachMaxTry", 3000);
    g_waGhostFix      = IniFloat(ini, "Hands", "AttachGhostFix", 1) != 0.0f;
#if DVR_WITH_LEGACY
    g_waProbe         = IniFloat(ini, "Hands", "AttachProbe", 1) != 0.0f;
#endif
    g_waViewLens = IniFloat(ini, "Hands", "AttachViewLens", 1) != 0.0f;
    Log("config: [Hands] AttachViewLens=%d",g_waViewLens);
    g_waScaleTrace = IniFloat(ini, "Hands", "AttachScaleTrace", 1) != 0.0f;
    Log("config: [Hands] AttachScaleTrace=%d (read-only transform mismatch trace)", g_waScaleTrace);
    g_waCensusOn      = IniFloat(ini, "Hands", "AttachCensus", 1) != 0.0f;
    g_waSuppressUnplaced = IniFloat(ini, "Hands", "AttachSuppressUnplaced", 1) != 0.0f;
    // 100 ms, not 20. It was tightened to 20 chasing a view-model sway theory
    // that the headset then falsified, and the tighter bound REFUSED to publish
    // a correction often enough to blink both weapons at once several times a
    // second - they share this gate, which is why they blinked together and why
    // that was the clue. A bound this loose has never been shown to cost
    // accuracy; the tight one was shown to cost the picture.
    g_waSnapMaxMs     = IniFloat(ini, "Hands", "AttachSnapshotMaxMs", 100.0f);
    if (g_waSnapMaxMs < 4.0f)   g_waSnapMaxMs = 4.0f;
    if (g_waSnapMaxMs > 250.0f) g_waSnapMaxMs = 250.0f;
    g_waViewModelUU   = IniFloat(ini, "Hands", "AttachViewModelUU", 500.0f);
    g_waNearAngDeg    = IniFloat(ini, "Hands", "AttachNearAngle", 20.0f);
    g_waNearPosUU     = IniFloat(ini, "Hands", "AttachNearPos", 30.0f);
    g_waNearMargin    = IniFloat(ini, "Hands", "AttachNearMargin", 1.5f);
    g_waDropUncorrected = IniFloat(ini, "Hands", "AttachDropUncorrected", 1) != 0.0f;
    if (g_waNearMargin < 1.05f) g_waNearMargin = 1.05f;
    if (g_waNearMargin > 8.0f)  g_waNearMargin = 8.0f;
    if (g_waViewModelUU < 10.0f)   g_waViewModelUU = 10.0f;
    if (g_waViewModelUU > 1500.0f) g_waViewModelUU = 1500.0f;
    if (g_waNearAngDeg  < 0.25f)   g_waNearAngDeg  = 0.25f;
    if (g_waNearAngDeg  > 60.0f)   g_waNearAngDeg  = 60.0f;
    if (g_waNearPosUU   < 1.0f)    g_waNearPosUU   = 1.0f;
    if (g_waNearPosUU   > 200.0f)  g_waNearPosUU   = 200.0f;
    // VR-59: the fired bolt. These are INSTANCE tests, not distances - a bolt
    // fired into a nearby wall is inside every radius below on merit, and with
    // its weapon stowed the fault reaches a bolt at ANY distance, which is
    // itself proof no radius was gating it. All four default ON; each one off
    // restores the pre-VR-59 behaviour of that single step, so they A/B alone.
    // BOTH DEFAULT OFF, on measurement. Each rested on a premise the first
    // headset run falsified, and each cost more than the bug it targeted.
    //
    // AttachRequireFreshRef demanded that a contract have drawn on the view
    // model within AttachRefMaxPresents presents. lastL2W is refreshed ONLY by
    // the transform matcher, never by the buffer-identity route that does the
    // correcting, so once the matcher misses the reference goes stale forever
    // and this gate blocks the only remaining route. Measured: 53,238 refusals
    // in one run, all on HELD crossbow_01 and bolt_01, with the present gap
    // growing monotonically to 21,367 - the reference was set once and never
    // again. The held bolt stopped following the hand and drew natively.
    //
    // AttachRequireLiveMember asked whether that asset is a live member of the
    // hand. It cannot answer the question: FpCollect walks the pawn INVENTORY,
    // so every snapshot in that run held the same six components regardless of
    // what was equipped, and bolt_01 (pArrowMesh_HighRes) was present
    // throughout. DisWepCrossbow says why - the loaded bolt is
    // m_pArrowMesh_HighRes, a component of the WEAPON, which exists whether or
    // not the crossbow is drawn. Presence is not equipment.
    // VR-59 attempt 2. Every draw on a weapon's buffers is verified against the
    // component the contract was matched to, and a draw that matches nothing is
    // handed back exactly as the engine drew it. OFF restores trusting buffer
    // identity, which is what every build before this did, so the two compare
    // directly in a headset.
    // VR-61: the gameplay state flags. Read-only and off the frame path, so it
    // ships ON: its whole purpose is to report what the game is doing, and a
    // reporter nobody enables reports nothing.
    dvr::anim::configure(ini);
    dvr::drop::configure(ini);   // [DropTakedown] and [Anim] DropWatch
    dvr::swing::configure(ini);   // VR-37: the motion sword's own [Melee] keys
    CineTraceConfigure(ini);
    UiSurfaceConfigure(ini);
    {   // VR-117: the HUD on its anchors. The layout owns the [Hud] placement keys.
        dvr::hudlayout::configure(ini);
        dvr::hudcap::set_slot_scale(IniFloat(ini, "Hud", "SlotScale", 0.50f));
        dvr::hudcap::set_enabled(IniFloat(ini, "Hud", "Panel", 1) != 0.0f);
        dvr::hudcap::set_once_per_pair(IniFloat(ini, "Hud", "OncePerPair", 1) != 0.0f);   // VR-160: ON since the headset verdict (no HUD flicker reported); 0 restores every present
        dvr::hudclass::set_regions_enabled(IniFloat(ini, "Hud", "Regions", 0) != 0.0f);
        dvr::hudclass::set_census_enabled(IniFloat(ini, "Draws", "Census", 0) != 0.0f);
    }
    CineBordersConfigure(ini);
    // VR-165: how long a button must be HELD to pass the cinematic pad park.
    g_cineSkipHoldMs = (int)IniFloat(ini, "Cine", "SkipHoldMs", 300);
    Log("cine: SkipHoldMs=%d ms (a held button reaches the game during a cutscene so hold-to-skip works; a pulse still cannot. 0 = park every button)", g_cineSkipHoldMs);
    StereoStateConfigure(ini);
    PossessionStereoConfigure(ini);
    RainConfigure(ini);
    SwordTrailConfigure(ini);   // VR-171
    CamShakeConfigure(ini);   // VR-172
    dvr::snap::configure(ini);   // VR-219: [Turning] snap turn
    LensConfigure(ini);
    WmConfigure(ini);
    GameOptsConfigure(ini);   // VR-157: [Diagnostics] GameOptsOnStart
    CamModConfigure(ini);     // VR-165: [Diagnostics] CamModProbe
    SwingTraceConfigure(ini); // VR-165: [Diagnostics] SwingTrace
    AimSourceConfigure(ini);  // VR-166: [Aim] SourceProbe
    InteractAimConfigure(ini); // VR-166: [Aim] InteractFromHand
    ThrowAimConfigure(ini);    // VR-166: [Aim] ThrowFromHand
    GadgetAimConfigure(ini);   // VR-166: [Aim] GadgetFromHand
    CarryThrowAimConfigure(ini); // VR-181: [Aim] CarryThrowFromHand
    PowerAimConfigure(ini);    // VR-44: [Aim] PowersFromHand
    CineFovConfigure(ini);
    CinePitchConfigure(ini);
    g_rflStateOn = IniFloat(ini, "Hands", "StateFlags", 1) != 0.0f;
    // VR-60: offer the equipped item's own component as a candidate. OFF returns
    // to the pointer walk alone, which cannot see the pistol at all.
    g_waEquippedMembers = IniFloat(ini, "Hands", "AttachEquippedMembers", 1) != 0.0f;
    // VR-62: rebuild the candidate list while the game is in gameplay and the
    // list is empty. ON, because with it OFF nothing owns the rebuild at all -
    // every other FpCollect call site is a one-shot that has already fired by
    // the time a load empties the list, and the weapons never attach again for
    // the rest of the session. The key exists so the two compare directly.
    g_fpAutoRecollect = IniFloat(ini, "Hands", "AttachAutoRecollect", 1) != 0.0f;
    // Walk the equipped items as collection roots as well as the pawn. OFF
    // returns to the pawn walk alone, which reaches an item's children only when
    // a pointer chain happens to lead there - the crossbow's loaded bolt came
    // and went with that luck.
    g_fpEquipRoots = IniFloat(ini, "Hands", "AttachCollectEquippedRoots", 1) != 0.0f;
    // On a candidate refresh, retire only contracts whose component is DEAD.
    // A stowed weapon's component is alive and keeps its contract, so a swap
    // back attaches on the first frame instead of re-identifying from scratch.
    g_waRetireDead = IniFloat(ini, "Hands", "AttachRetireDeadOnRefresh", 1) != 0.0f;
    // VR-93: a menu over a live pawn suspends the contracts and the candidate
    // list instead of dropping them; the resume validates every retained object
    // against a freshly built live-object table (class and FName) before use.
    // OFF is the old transition exactly. F10 Hands, live.
    g_mkOn = IniFloat(ini, "Hands", "AttachKeepOnMenu", 1) != 0.0f;
    Log("config: [Hands] AttachKeepOnMenu=%d - a menu over a live pawn %s.", g_mkOn ? 1 : 0,
        g_mkOn ? "SUSPENDS the weapon contracts and candidates, validated on resume"
               : "drops the weapon contracts and candidates, as before VR-93");
    // The book/note screen reads LOADING, not MENU; this lets it suspend too, on
    // the UI observer's open bit. Needs AttachKeepOnMenu=1 and [Menu] UiProbe=1.
    g_mkNoteOn = IniFloat(ini, "Hands", "AttachKeepOnNote", 1) != 0.0f;
    Log("config: [Hands] AttachKeepOnNote=%d - the book/note screen %s.", g_mkNoteOn ? 1 : 0,
        g_mkNoteOn ? "suspends like a menu while the observer sees it open"
                   : "drops the weapon records, as before");
    // VR-65: the pose trace's NEGATIVE CONTROL, and it runs itself. A few
    // seconds into a session it records a deliberately wrong head yaw for about
    // a second; the submission join must report exactly that error and must
    // return to zero after. Without it, a join that reports zero has not been
    // shown capable of reporting anything else. PoseSelfTestRecords=0 disables.
    // VR-65: the announced lag comparison. It was the discriminator the refusing
    // render leg could not supply; its answer (lag 2) now ships as [Pace] Lag, so
    // the comparison ships OFF (VR-72) and LagAB=1 walks it again. It changes
    // nothing but the pose-history selection and restores the baseline by itself.
    dvr::vr::set_lag_ab(IniFloat(ini, "Stereo", "LagAB", 0) != 0.0f,
                        (uint32_t)IniFloat(ini, "Stereo", "LagABSegMs", 20000));
    dvr::pose::configure_controls(
        (uint32_t)IniFloat(ini, "Stereo", "PoseControlsAfter", 60),
        (uint32_t)IniFloat(ini, "Stereo", "PoseControlsEach", 30),
        IniFloat(ini, "Stereo", "PoseControlsDeg", 6.0f));
    // How many extra collects a weapon swap is worth, and how far apart. The
    // equipment event and the new weapon's child components do not have to
    // appear in the same tick, so one rebuild can win the race and return a list
    // with no loaded bolt in it. 0 disables the settle window.
    g_fpSettleTries = (int)IniFloat(ini, "Hands", "AttachSwapSettleTries", 5);
    if (g_fpSettleTries < 0)  g_fpSettleTries = 0;
    if (g_fpSettleTries > 30) g_fpSettleTries = 30;
    g_fpSettleGapMs = IniFloat(ini, "Hands", "AttachSwapSettleGapMs", 400.0f);
    if (g_fpSettleGapMs < 50.0)   g_fpSettleGapMs = 50.0;
    if (g_fpSettleGapMs > 5000.0) g_fpSettleGapMs = 5000.0;
    // VR-16: take the eye from the pass that is drawing instead of inferring it
    // from a jump in LocalToWorld. ON since the audit measured the inference
    // disagreeing with the drawing pass 39% of the time, steadily, on the draws
    // that are inside a pass at all. OFF restores the inference so the two
    // compare directly in one session.
    // FALSIFIED and inert: the passes run on the game thread and the palette
    // draws on the render thread, so 0 of 83,400 draws ever found a pass to read.
    g_mpEyeFromPass = IniFloat(ini, "Hands", "PaletteEyeFromPass", 0) != 0.0f;
    // When the eye step is too small to read, ALTERNATE rather than hold the
    // previous present's answer. The method presents the eyes alternately, so
    // holding is the one choice guaranteed wrong; 12% of presents took that path
    // in the flicker run. OFF restores the hold for a direct comparison.
    g_mpEyeAlternate = IniFloat(ini, "Hands", "PaletteEyeAlternate", 0) != 0.0f;
    // How long a contract may keep refusing after its component disappears
    // before it is retired so the matcher can re-adopt. 90 presents is about a
    // second at 90 Hz - long enough that a one-frame snapshot gap is not a
    // retirement, short enough that a lockout cannot outlive a load.
    g_waStaleMaxPresents = (int)IniFloat(ini, "Hands", "AttachContractStalePresents", 90);
    g_sdGateSceneLive = IniFloat(ini, "Stereo", "GateOnSceneLive", 0) != 0.0f;
    g_sdSceneQuietMs  = IniFloat(ini, "Stereo", "SceneQuietMs", 400.0f);
    if (g_sdSceneQuietMs < 50.0f)   g_sdSceneQuietMs = 50.0f;
    if (g_sdSceneQuietMs > 2000.0f) g_sdSceneQuietMs = 2000.0f;
    // VR-62: the movie-player probe. Read-only observation, and it ships ON for
    // the same reason the equipment reader does - a reporter nobody enables
    // reports nothing, and this one exists to be read out of a tester's log.
    g_uiOn = IniFloat(ini, "Menu", "UiProbe", 1) != 0.0f;
    // VR-93 B2: a menu that kept the weapon records does not queue the observer
    // rescan, which otherwise holds the resume ~500 ms. Needs AttachKeepOnMenu=1
    // to have any effect. OFF queues it on every menu, as before.
    g_uiKeepOnMenu = IniFloat(ini, "Menu", "UiKeepOnMenu", 1) != 0.0f;
    // VR-93 research: report changes of the screen flags the script dump declares
    // (GAMEPLAY_STATE.md section 9). Read-only, logs changes only.
    g_ufOn = IniFloat(ini, "Menu", "UiFlags", 1) != 0.0f;
    g_nameIndexCacheOn = IniFloat(ini, "Menu", "CacheNameLookups", 1) != 0.0f;   // headset-confirmed 2026-09-22
    Log("config: [Menu] CacheNameLookups=%d - %s", g_nameIndexCacheOn ? 1 : 0,
        g_nameIndexCacheOn ? "reuse validated name IDs" : "scan names for every lookup (legacy)");
    g_pawnFromController = IniFloat(ini, "Menu", "PawnFromController", 1) != 0.0f;
    Log("config: [Menu] PawnFromController=%d - capsule liveness uses %s",
        g_pawnFromController ? 1 : 0,
        g_pawnFromController ? "the validated possessed pawn" : "the event-latched pawn (legacy)");
    // VR-98: a note or book switches to mono and back as soon as the note movie opens and closes,
    // instead of the load rules' 750 ms silence wait and one-second resume hold. Needs the UI observer.
    g_uiNoteFastMono = IniFloat(ini, "Menu", "NoteFastMono", 1) != 0.0f;
    Log("config: [Menu] NoteFastMono=%d - a note %s.", g_uiNoteFastMono ? 1 : 0,
        g_uiNoteFastMono ? "goes mono when its movie opens and stereo on the first dispatch after it closes"
                         : "waits 750 ms of view silence to go mono and a full second of dispatches to return (the load rules)");
    Log("config: [Menu] UiKeepOnMenu=%d - a kept menu %s the UI observer rescan.",
        g_uiKeepOnMenu ? 1 : 0, g_uiKeepOnMenu ? "SKIPS" : "still queues");
    g_menuGhostByRate  = IniFloat(ini, "Menu", "GhostClearByRate", 0) != 0.0f;
    g_menuGhostQuietMs = IniFloat(ini, "Menu", "GhostQuietMs", 400.0f);
    if (g_menuGhostQuietMs < 50.0)   g_menuGhostQuietMs = 50.0;
    if (g_menuGhostQuietMs > 5000.0) g_menuGhostQuietMs = 5000.0;
    if (g_waStaleMaxPresents < 1)    g_waStaleMaxPresents = 1;
    if (g_waStaleMaxPresents > 9000) g_waStaleMaxPresents = 9000;
    g_waVerifyInstance = IniFloat(ini, "Hands", "AttachVerifyInstance", 1) != 0.0f;
    g_waHeldMaxPresents = (int)IniFloat(ini, "Hands", "AttachHeldMaxPresents", 2);
    if (g_waHeldMaxPresents < 0)  g_waHeldMaxPresents = 0;
    if (g_waHeldMaxPresents > 90) g_waHeldMaxPresents = 90;
    g_waReqFreshRef   = IniFloat(ini, "Hands", "AttachRequireFreshRef", 0) != 0.0f;
    g_waReqLiveMember = IniFloat(ini, "Hands", "AttachRequireLiveMember", 0) != 0.0f;
    g_waVetoFrees     = IniFloat(ini, "Hands", "AttachVetoReleasesBuffers", 1) != 0.0f;
    g_waVetoRelaxed   = IniFloat(ini, "Hands", "AttachInstanceVetoRelaxed", 1) != 0.0f;
    g_waRefPresents   = (int)IniFloat(ini, "Hands", "AttachRefMaxPresents", 2);
    if (g_waRefPresents < 0)  g_waRefPresents = 0;
    if (g_waRefPresents > 90) g_waRefPresents = 90;
    g_waRigRadiusUU   = IniFloat(ini, "Hands", "AttachRigRadius", 200.0f);
    g_waPassRadiusUU  = IniFloat(ini, "Hands", "AttachPassRadius", 60.0f);
    if (g_waRigRadiusUU  < 10.0f)   g_waRigRadiusUU  = 10.0f;
    if (g_waRigRadiusUU  > 5000.0f) g_waRigRadiusUU  = 5000.0f;
    if (g_waPassRadiusUU < 1.0f)    g_waPassRadiusUU = 1.0f;
    if (g_waPassRadiusUU > 5000.0f) g_waPassRadiusUU = 5000.0f;
#if DVR_WITH_LEGACY
    g_waProbeBudget   = (int)IniFloat(ini, "Hands", "AttachProbeBudget", 400);
    if (g_waProbeBudget < 0)     g_waProbeBudget = 0;
    if (g_waProbeBudget > 20000) g_waProbeBudget = 20000;
#endif
    if (g_waSwordHand < 0 || g_waSwordHand > 1) g_waSwordHand = 1;
    if (g_waXbowHand  < 0 || g_waXbowHand  > 1) g_waXbowHand  = 0;
    if (g_waAngTolDeg < 0.01f) g_waAngTolDeg = 0.01f;
    if (g_waAngTolDeg > 30.0f) g_waAngTolDeg = 30.0f;
    if (g_waPosTolUU  < 0.05f) g_waPosTolUU  = 0.05f;
    if (g_waPosTolUU  > 200.0f) g_waPosTolUU = 200.0f;
    if (g_waMarginX   < 1.2f)  g_waMarginX   = 1.2f;
    if (g_waMaxTry    < 100)   g_waMaxTry    = 100;
    if (g_waMaxTry    > 200000) g_waMaxTry   = 200000;
#if DVR_WITH_LEGACY
    // The long sweep is now an OPTIONAL targeted confirmation, not the
    // attachment's gate, so it defaults OFF: it costs 26 seconds of blinking
    // and the attachment no longer consumes its output.
    g_wiOn            = IniFloat(ini, "Hands", "WeaponId", 0) != 0.0f;
    g_wiPhaseMs       = IniFloat(ini, "Hands", "WeaponIdMs", 1500.0f);
    if (g_wiPhaseMs < 300.0f)  g_wiPhaseMs = 300.0f;
    if (g_wiPhaseMs > 8000.0f) g_wiPhaseMs = 8000.0f;
#endif
    if (g_waOn) {
        Log("config: [Hands] AttachWeapons=1 - the weapon identifies its OWN "
            "draws and no sweep is involved. A coordinate bridge is built from "
            "the body mesh, whose draw is already identified by the split, and "
            "each owned component's complete draw transform is then predicted "
            "and matched on orientation AND position within %.2f deg / %.2f uu, "
            "unique by %.1fx over the runner-up. A matched member takes the "
            "SAME correction the hand took, conjugated into its own space - not "
            "a new grip - so the game's own hand-to-weapon and weapon-to-bolt "
            "relationships survive, and so does the bolt's internal animation.",
            (double)g_waAngTolDeg, (double)g_waPosTolUU, (double)g_waMarginX);
        Log("config: [Hands] AttachGhostFix=%s - a weapon mesh is drawn more "
            "than once per frame, and only a pass declaring LocalToWorld can be "
            "identified by transform. The others were left where the engine put "
            "them, which is the dark copy standing at the weapon's old position. "
            "A refused draw sharing a matched contract's buffers, range and "
            "primitive count but drawn by a DIFFERENT shader takes that "
            "contract's correction from the SAME Present - same mesh, same "
            "frame, so the same delta. Read the wa/ghost: lines.",
            g_waGhostFix ? "1" : "0");
        Log("config: attachment gates use those angle/position bands and a "
            "0.5 percent scale band. Both hands are compared; every draw is "
            "revalidated. Read 'wa: interval nearest' per hand for actual "
            "residuals. AttachMaxTry is diagnostic only; it never starves "
            "later weapon draws. Do not widen bands to manufacture a match.");
        Log("config: the hand sides are an ASSUMPTION, not measured attachment "
            "data - sword %s, crossbow and bolt %s. An asset that matches "
            "neither name is not attached at all rather than swept into a "
            "default hand.",
            g_waSwordHand ? "RIGHT" : "LEFT", g_waXbowHand ? "RIGHT" : "LEFT");
    }
#if DVR_WITH_LEGACY
    if (g_wiOn)
        Log("config: [Hands] WeaponId=1 - the weapon identifier will run ONE "
            "sweep, %.1f s per component, hiding each first-person component "
            "in turn and recording which skinned draws stop being submitted. "
            "A draw present in the baseline and absent exactly while a named "
            "component is hidden BELONGS to it. Equip the weapon you care "
            "about first: a component that is not drawn in the baseline "
            "cannot be identified. Read the wid: lines.", g_wiPhaseMs / 1000.0);
#endif
    g_mpFrameTolOrtho = IniFloat(ini, "Hands", "PaletteFrameTol", 0.02f);
    if (g_mpFrameTolOrtho < 0.0005f) g_mpFrameTolOrtho = 0.0005f;
    if (g_mpFrameTolOrtho > 0.25f)   g_mpFrameTolOrtho = 0.25f;
    g_mpFrameTolAniso = g_mpFrameTolOrtho;
    {
        // THE CALIBRATION RECORD. Three degrees per side hold a PROPER
        // rotation, in extrinsic X then Y then Z (R = Rz*Ry*Rx), and the
        // reflection is carried separately as a parity sign. G = P * R_saved,
        // exactly, because P*P = I.
        //
        // A record with no GripVersion is REFUSED. Those angles were written by
        // the build that reduced an improper G to three rotation angles, which
        // cannot represent it: loading them would restore a MIRRORED hand while
        // looking like a perfectly good calibration. Refusing costs one capture
        // press; accepting costs a confusing headset run.
        static const char* ax[3] = { "X", "Y", "Z" };
        for (int h = 0; h < 2; h++) {
            const char* sfx = h ? "R" : "L";
            char key[32];
            _snprintf(key, sizeof(key), "Grip%sVersion", sfx);
            g_mpGripVer[h] = (int)IniFloat(ini, "Hands", key, 0.0f);
            _snprintf(key, sizeof(key), "Grip%sParity", sfx);
            g_mpGripParity[h] = (int)IniFloat(ini, "Hands", key, 0.0f);
            for (int a = 0; a < 3; a++) {
                _snprintf(key, sizeof(key), "Grip%s%s", sfx, ax[a]);
                g_mpGripDeg[h][a] = IniFloat(ini, "Hands", key, 0.0f);
            }
            const bool usable = (g_mpGripVer[h] == MP_GRIP_VERSION) &&
                                (g_mpGripParity[h] == 1 || g_mpGripParity[h] == -1);
            if (usable) {
                g_mpGrip[h] = dvr::hf::join_parity(
                    g_mpGripParity[h],
                    dvr::hf::euler_xyz_deg_to_mat(g_mpGripDeg[h][0],
                                                  g_mpGripDeg[h][1],
                                                  g_mpGripDeg[h][2]));
                g_mpGripHave[h] = true;
                Log("config: the %s hand's grip calibration LOADED - version %d, "
                    "parity %+d, proper rotation %+.2f %+.2f %+.2f degrees. No "
                    "capture is needed this launch.",
                    h ? "right" : "left", g_mpGripVer[h], g_mpGripParity[h],
                    (double)g_mpGripDeg[h][0], (double)g_mpGripDeg[h][1],
                    (double)g_mpGripDeg[h][2]);
            } else {
                g_mpGrip[h] = dvr::hf::identity3();   // replaced per draw by the
                g_mpGripHave[h] = false;              // parity-matched default
                if (g_mpGripVer[h] != 0 || g_mpGripDeg[h][0] != 0.0f ||
                    g_mpGripDeg[h][1] != 0.0f || g_mpGripDeg[h][2] != 0.0f)
                    Log("config: the %s hand has a grip record this build cannot "
                        "use (version %d, parity %+d). A pre-version-%d record "
                        "stored three rotation angles only, and the solved grip "
                        "on this game is a REFLECTION that no product of proper "
                        "rotations can represent - loading it would put the hand "
                        "back inside out. Press SHIFT+F7 once and it will be "
                        "saved correctly and never asked for again.",
                        h ? "right" : "left", g_mpGripVer[h], g_mpGripParity[h],
                        MP_GRIP_VERSION);
            }
            g_mpGripFromIni[h] = true;
        }
        // The hand trim, in the calibrated palm frame. Metres and degrees,
        // PER HAND. The pre-numpad build stored one shared trim under
        // TrimTX/TrimRX; those keys are read as the seed for BOTH hands so a
        // trim already dialled in by hand is not silently thrown away, and the
        // migration is logged rather than done quietly.
        static const char* axn[3] = { "X", "Y", "Z" };
        // VR-57: a local finite test. MpFinite lives in a translation unit the
        // unity build includes AFTER this one, and a NaN must be refused before
        // the clamp because every comparison against it is false.
        struct Fin { static bool ok(float v) { return v == v && v > -1.0e30f && v < 1.0e30f; } };
        float seedT[3], seedR[3];
        bool  seeded = false;
        for (int a = 0; a < 3; a++) {
            char k[32];
            _snprintf(k, sizeof(k), "TrimT%s", axn[a]);
            seedT[a] = IniFloat(ini, "Hands", k, 0.0f);
            _snprintf(k, sizeof(k), "TrimR%s", axn[a]);
            seedR[a] = IniFloat(ini, "Hands", k, 0.0f);
            if (seedT[a] != 0.0f || seedR[a] != 0.0f) seeded = true;
        }
        for (int h = 0; h < 2; h++) {
            const char* sfx = h ? "R" : "L";
            for (int a = 0; a < 3; a++) {
                char k[32];
                _snprintf(k, sizeof(k), "Trim%sT%s", sfx, axn[a]);
                const float reqT = IniFloat(ini, "Hands", k, seedT[a]);
                _snprintf(k, sizeof(k), "Trim%sR%s", sfx, axn[a]);
                const float reqR = IniFloat(ini, "Hands", k, seedR[a]);
                // VR-57: nonfinite is refused BEFORE clamping, because clamping a
                // NaN keeps the NaN - every comparison against it is false. The
                // seed keys reach here too, so they get the same validation rather
                // than a second set of rules.
                g_mpTrimT[h][a] = Fin::ok(reqT) ? reqT : 0.0f;
                g_mpTrimR[h][a] = Fin::ok(reqR) ? reqR : 0.0f;
                if (!Fin::ok(reqT) || !Fin::ok(reqR))
                    Log("config: [Hands] Trim%s axis %s had a nonfinite value "
                        "(T %g, R %g); that axis is zeroed rather than clamped, "
                        "because a clamp cannot bound a NaN.", sfx, axn[a],
                        (double)reqT, (double)reqR);
                if (g_mpTrimT[h][a] >  kMpTrimPosLimit) g_mpTrimT[h][a] =  kMpTrimPosLimit;
                if (g_mpTrimT[h][a] < -kMpTrimPosLimit) g_mpTrimT[h][a] = -kMpTrimPosLimit;
                if (g_mpTrimR[h][a] >  kMpTrimRotLimit) g_mpTrimR[h][a] =  kMpTrimRotLimit;
                if (g_mpTrimR[h][a] < -kMpTrimRotLimit) g_mpTrimR[h][a] = -kMpTrimRotLimit;
                // Requested against effective, so a clamp on LOAD is visible. A
                // value tuned live and then bounded by the next load is exactly
                // the fault that made one shared limit necessary.
                if (g_mpTrimT[h][a] != reqT && Fin::ok(reqT))
                    Log("config: [Hands] Trim%sT%s requested %+.4f, effective "
                        "%+.4f m (bound +-%.2f)", sfx, axn[a], (double)reqT,
                        (double)g_mpTrimT[h][a], (double)kMpTrimPosLimit);
                if (g_mpTrimR[h][a] != reqR && Fin::ok(reqR))
                    Log("config: [Hands] Trim%sR%s requested %+.2f, effective "
                        "%+.2f deg (bound +-%.0f)", sfx, axn[a], (double)reqR,
                        (double)g_mpTrimR[h][a], (double)kMpTrimRotLimit);
            }
        }
        // The powers trim: its own keys, seeded from the LEFT trim when they are absent, so
        // turning it on changes nothing until it is edited (mesh_split state, MpTrimTFor).
        g_mpPowTrimOn = GetPrivateProfileIntA("Hands", "PowerTrim", 1, ini) != 0;
        g_mpAdjView = GetPrivateProfileIntA("Hands", "AdjustInView", 1, ini) != 0;
        for (int a = 0; a < 3; a++) {
            char k[32];
            _snprintf(k, sizeof(k), "TrimLPT%s", axn[a]);
            const float reqT = IniFloat(ini, "Hands", k, g_mpTrimT[0][a]);
            _snprintf(k, sizeof(k), "TrimLPR%s", axn[a]);
            const float reqR = IniFloat(ini, "Hands", k, g_mpTrimR[0][a]);
            g_mpTrimPT[a] = Fin::ok(reqT) ? (reqT > kMpTrimPosLimit ? kMpTrimPosLimit : reqT < -kMpTrimPosLimit ? -kMpTrimPosLimit : reqT) : 0.0f;
            g_mpTrimPR[a] = Fin::ok(reqR) ? (reqR > kMpTrimRotLimit ? kMpTrimRotLimit : reqR < -kMpTrimRotLimit ? -kMpTrimRotLimit : reqR) : 0.0f;
        }
        Log("config: powers hand trim %s - translation (%+.1f %+.1f %+.1f) mm rotation "
            "(%+.2f %+.2f %+.2f) deg, used for the LEFT hand while it holds a power "
            "([Hands] PowerTrim, TrimLPT*/TrimLPR*; seeded from the left trim when absent)",
            g_mpPowTrimOn ? "ON" : "off",
            (double)(g_mpTrimPT[0]*1000.0f), (double)(g_mpTrimPT[1]*1000.0f), (double)(g_mpTrimPT[2]*1000.0f),
            (double)g_mpTrimPR[0], (double)g_mpTrimPR[1], (double)g_mpTrimPR[2]);
        if (seeded)
            Log("config: the shared hand trim from the previous build "
                "(TrimTX/TrimRX, %.1f %.1f %.1f mm / %.2f %.2f %.2f deg) was "
                "copied to BOTH hands as the starting point for the per-hand "
                "keys. It is now TrimL*/TrimR* and the numpad writes those; the "
                "old keys are read once more and then ignored.",
                (double)(seedT[0]*1000.0f), (double)(seedT[1]*1000.0f),
                (double)(seedT[2]*1000.0f),
                (double)seedR[0], (double)seedR[1], (double)seedR[2]);
        MpPublishHandCal(0);
        MpPublishHandCal(1);
        Log("config: hand trim bounds - rotation +-%.0f deg per axis (raised from "
            "%.0f, which measurably prevented further adjustment), translation "
            "+-%.2f m. One limit serves the ini load and the numpad adjustment, so "
            "a live value cannot be clamped back by the next load.",
            (double)kMpTrimRotLimit, (double)kMpTrimRotNotice,
            (double)kMpTrimPosLimit);
        Log("config: hand trim LOADED - left translation (%+.1f %+.1f %+.1f) mm "
            "rotation (%+.2f %+.2f %+.2f) deg | right translation "
            "(%+.1f %+.1f %+.1f) mm rotation (%+.2f %+.2f %+.2f) deg. All zero "
            "is the uncalibrated state and is what a first run should print.",
            (double)(g_mpTrimT[0][0]*1000.0f), (double)(g_mpTrimT[0][1]*1000.0f),
            (double)(g_mpTrimT[0][2]*1000.0f),
            (double)g_mpTrimR[0][0], (double)g_mpTrimR[0][1], (double)g_mpTrimR[0][2],
            (double)(g_mpTrimT[1][0]*1000.0f), (double)(g_mpTrimT[1][1]*1000.0f),
            (double)(g_mpTrimT[1][2]*1000.0f),
            (double)g_mpTrimR[1][0], (double)g_mpTrimR[1][1], (double)g_mpTrimR[1][2]);
    }

    // VR-57: RESTORE A PREVIOUSLY MEASURED MODEL AXIS.
    //
    // It can only be measured from a drawn crossbow bolt, so a session that loads a
    // save with the pistol out never measures one and had no guide at all. The ray is
    // a palm-frame constant, so it is written down on first measurement and restored
    // here as the fallback.
    //
    // It is accepted only if the grip it was measured against still matches: the grip
    // defines the palm frame the ray is expressed in, so a recalibration invalidates
    // it. Hand TRIM changes are fine and need no check - the frame is rebuilt from the
    // current trim every time the ray is used, which is why tuning carries it.
    {
        static const char* const ax[3] = { "X", "Y", "Z" };
        for (int h = 0; h < 2; h++) {
            const char* sfx = h ? "R" : "L";
            float o[3], d[3], gsaved[3];
            bool have = true;
            for (int a2 = 0; a2 < 3 && have; a2++) {
                char k[40];
                _snprintf(k, sizeof(k), "ModelAxis%sO%s", sfx, ax[a2]);
                o[a2] = IniFloat(ini, "Hands", k, 9999.0f);
                _snprintf(k, sizeof(k), "ModelAxis%sD%s", sfx, ax[a2]);
                d[a2] = IniFloat(ini, "Hands", k, 9999.0f);
                _snprintf(k, sizeof(k), "ModelAxis%sG%s", sfx, ax[a2]);
                gsaved[a2] = IniFloat(ini, "Hands", k, 9999.0f);
                if (o[a2] > 9000.0f || d[a2] > 9000.0f || gsaved[a2] > 9000.0f) have = false;
            }
            if (!have) continue;
            float gripDrift = 0.0f;
            for (int a2 = 0; a2 < 3; a2++) {
                const float e = gsaved[a2] - g_mpGripDeg[h][a2];
                gripDrift += e < 0 ? -e : e;
            }
            if (gripDrift > 0.5f) {
                Log("config: the %s hand has a stored model axis measured against grip "
                    "(%.2f %.2f %.2f) but the grip is now (%.2f %.2f %.2f), %.2f deg "
                    "apart. The grip DEFINES the palm frame the axis is expressed in, "
                    "so the record is discarded rather than aimed through a frame that "
                    "no longer exists. It will be measured again from the crossbow.",
                    h ? "right" : "left", (double)gsaved[0], (double)gsaved[1],
                    (double)gsaved[2], (double)g_mpGripDeg[h][0],
                    (double)g_mpGripDeg[h][1], (double)g_mpGripDeg[h][2],
                    (double)gripDrift);
                continue;
            }
            dvr::hands::preload_model_ray(h, o, d);
            Log("config: the %s hand's model axis RESTORED - origin (%.4f %.4f %.4f) m, "
                "direction (%.4f %.4f %.4f). A session that never equips the crossbow "
                "now has a guide from the first frame instead of none. It is bounded on "
                "load exactly as a live measurement is, so an edited record cannot "
                "install a ray a measurement would have refused.",
                h ? "right" : "left", (double)o[0], (double)o[1], (double)o[2],
                (double)d[0], (double)d[1], (double)d[2]);
        }
    }

    // VR-57: FollowHandTrim. The published ray is transported by the hand's own
    // trim, so tuning the hand carries the guide and the shot with it. Default OFF
    // in new configurations; off returns the AIM-pose ray untouched.
    {
        dvr::aim::Config ch = dvr::aim::config();
        ch.followHandTrim = GetPrivateProfileIntA("Aim", "FollowHandTrim", 1, ini) != 0;
        ch.modelRay = GetPrivateProfileIntA("Aim", "ModelRay", 1, ini) != 0;
        dvr::aim::configure(ch, ini);
        Log("config: [Aim] FollowHandTrim=%d - %s. This is NOT a measured barrel "
            "axis or muzzle position: it transports the hand trim onto the existing "
            "AIM-pose ray, so any baseline offset between that ray and the weapon's "
            "barrel is preserved, not removed.",
            ch.followHandTrim ? 1 : 0,
            ch.followHandTrim ? "the ray follows the hand trim"
                              : "the ray is the AIM pose, unchanged");
    }

    // THE MODEL SCALE. Hands and held weapons, one uniform factor.
    g_mpModelScale = IniFloat(ini, "Hands", "ModelScale", 0.85f);
    if (g_mpModelScale < 0.3f) g_mpModelScale = 0.3f;
    if (g_mpModelScale > 2.0f) g_mpModelScale = 2.0f;
    if (g_mpModelScale != 1.0f)
        Log("config: [Hands] ModelScale=%.2f - the hands and anything held in "
            "them are drawn at %.0f%% size, scaled about the tracked palm so "
            "the grip stays where tracking put it. This is NOT the world scale: "
            "PageUp/PageDown set the stereo separation and change the apparent "
            "size of the whole frame, which can never make the hands smaller "
            "relative to the room. It is not the hand travel either "
            "(WorldScaleUU=%.0f, PaletteDriveGain=%.2f), which is how far the "
            "hand moves per metre of controller.",
            (double)g_mpModelScale, (double)(g_mpModelScale * 100.0f),
            (double)g_skcWorldScale, (double)g_mpDriveGain);
    else
        Log("config: [Hands] ModelScale=1.00 - hands and weapons at their own "
            "size, which is the geometry the previous builds were measured "
            "with. Lower it to about 0.75 to shrink both together; the F10 "
            "'hand / weapon size' slider drives the same value live.");
    // THE DOUBLE-APPLY, named rather than left to be discovered. HandSize
    // writes SkelControlBase.BoneScale engine-side, which lands in the palette
    // BEFORE this correction and therefore multiplies with it.
    if (g_mpModelScale != 1.0f && g_skcHandSize != 1.0f)
        Log("config: WARNING - [Hands] ModelScale=%.2f AND HandSize=%.2f are "
            "BOTH away from 1.0, so the hand is scaled TWICE, to about %.0f%%. "
            "HandSize is the older engine-side BoneScale write and it does not "
            "reach a separately-componented weapon, so the two also disagree "
            "about the crossbow. Set HandSize=1.00 and use ModelScale alone.",
            (double)g_mpModelScale, (double)g_skcHandSize,
            (double)(g_mpModelScale * g_skcHandSize * 100.0f));

    // THE NUMPAD ADJUST and its explicit claim. Defaults ON: it is the only
    // way to correct a hand in the headset, and a lever the tester has to
    // switch on before he can report anything costs a whole run.
    g_mpAdjOn    = IniFloat(ini, "Hands", "Adjust", 1) != 0.0f;
    g_mpAdjStepT = (int)IniFloat(ini, "Hands", "AdjStepT", 1);
    g_mpAdjStepR = (int)IniFloat(ini, "Hands", "AdjStepR", 3);
    if (g_mpAdjStepT < 0 || g_mpAdjStepT > 2) g_mpAdjStepT = 1;
    if (g_mpAdjStepR < 0 || g_mpAdjStepR > 6) g_mpAdjStepR = 3;
    if (g_mpAdjOn) {
        Log("config: [Hands] Adjust=1 - THE NUMPAD IS CLAIMED BY THE HAND "
            "ADJUST. Numpad 9 cycles LEFT position / LEFT rotation / RIGHT "
            "position / RIGHT rotation and NAMES the mode in the log; 8/2 is "
            "forward/back or pitch, 6/4 right/left or yaw, 0/5 up/down or roll; "
            "7 cycles the step (now %.1f cm / %.2f deg). Every press logs the "
            "new value and writes it to [Hands] Trim<L|R><T|R><X|Y|Z>, so a "
            "good alignment survives a restart with nothing typed into the ini.",
            (double)(kMpAdjStepT[g_mpAdjStepT] * 100.0f),
            (double)kMpAdjStepR[g_mpAdjStepR]);
        Log("config: what gave those keys up - the draw census and its "
            "eighth-cutter (Numpad 4-9) are suppressed entirely while Adjust=1 "
            "(census armed: %s); the material cycler keeps Numpad 1 and 3 and "
            "gives up 2 (cycler armed: %s); the mesh split gives up Numpad 0 "
            "and its mode cycle MOVES TO NUMPAD 1%s. Numpad + - * / . are "
            "untouched and still belong to the split. Set Adjust=0 to hand "
            "every key back.",
            g_dcOn ? "yes, and it will not respond to the numpad" : "no",
            g_matCycleCfg ? "yes" : "no",
            g_matCycleCfg
                ? " - EXCEPT that the material cycler is armed and owns Numpad "
                  "1, so the split's mode cycle is unreachable this run. Set "
                  "MatCycle=0 to get it back"
                : "");
    } else {
        Log("config: [Hands] Adjust=0 - the numpad adjust is OFF and the hands "
            "cannot be trimmed in the headset. The census, the material cycler "
            "and the mesh split keep every numpad key.");
    }
    if (g_mpOn && g_mpWorld && g_mpRotate)
        Log("config: [Hands] PaletteRotate=1 - the hands take a FULL RIGID "
            "correction, not just a translation. Orientation is read from the "
            "dominant palette slot of each hand's anchor, the controller is "
            "converted through the same physical mapping the working position "
            "path uses (F * transpose(R_head) * R_ctl, then the draw's own "
            "camera basis), and the grip transform G is %+.0f %+.0f %+.0f (L) "
            "/ %+.0f %+.0f %+.0f (R) degrees, extrinsic X,Y,Z. G at zero means "
            "the hands will TRACK your wrists but sit at a fixed wrong angle "
            "until the grip is captured (SHIFT+F7) or these angles are filled "
            "in. If any part of the rotation refuses, placement falls back to "
            "TRANSLATION ONLY - a hand that tracks but is not oriented is worth "
            "much more than a hand that does not track. Read ms/palette/frame.",
            (double)g_mpGripDeg[0][0], (double)g_mpGripDeg[0][1],
            (double)g_mpGripDeg[0][2], (double)g_mpGripDeg[1][0],
            (double)g_mpGripDeg[1][1], (double)g_mpGripDeg[1][2]);
    if (g_mpOn && g_mpWorld)
        Log("config: [Hands] PaletteWorld=1 - the palm is placed through the "
            "MEASURED chain. LocalToWorld and ViewProjectionMatrix are read "
            "from the device at each draw, through the register indices that "
            "shader's own constant table declares, and the palm's current "
            "position is re-skinned from the game's palette every frame. No "
            "calibration and no neutral. The one number still assumed is the "
            "scale: %.0f uu/m x gain %.2f, against [PosTrack] Scale=%.0f - a "
            "scale error shows as a GAIN error, not as drift.",
            (double)g_skcWorldScale, (double)g_mpDriveGain, (double)g_posScaleUU);
#if DVR_WITH_LEGACY
#include "legacy/vr33/palette_axis_config_log.inc"
#endif
    // 3 = CLIP the triangles that straddle the plane, which is the only rule
    // whose boundary is the plane itself. 0, 1 and 2 round the cut to whole
    // triangles and leave a sawtooth one triangle high - on the coarse cuff
    // geometry that reads as spikes hanging off the wrist.
    g_msEdge          = (int)IniFloat(ini, "Hands", "WristEdge", 3);
    if (g_msEdge < 0 || g_msEdge > 3) g_msEdge = 3;
    g_msStepMode      = (int)IniFloat(ini, "Hands", "WristStep", 1);
    if (g_msStepMode < 0 || g_msStepMode > 2) g_msStepMode = 1;
    // 0 = square to the forearm BONE, 1 = square to the longest direction of
    // the arm's triangles. The bone is the anatomical answer and the default; a
    // tapered sleeve can lean the triangle answer off it, and a ring square to
    // the wrong one is slanted across the forearm.
    g_msAxisMode      = (int)IniFloat(ini, "Hands", "WristAxis", 0);
    if (g_msAxisMode < 0 || g_msAxisMode > 1) g_msAxisMode = 0;
    // Where the ring sits, in mesh units from the hand bone, positive toward
    // the fingers. -4.9 on both arms is the tester's MEASURED position from the
    // 2026-09-06 headset run - the place the ring was left after walking it up
    // and down the forearm with the knob, read straight off the `ms/wrist` line
    // that press printed. It is not a derived number and not a guess, and it is
    // asset-relative (a distance from the hand bone along the limb axis), so it
    // survives a level load and does not depend on where the pawn is standing.
    //
    // The -1e9 sentinel still means "derive it", which places the ring where
    // the seeded sphere was; that is what a user gets by deleting the key, and
    // it is the escape hatch if a future asset makes -4.9 wrong.
    for (int s = 1; s <= 2; s++) {
        const float c = IniFloat(ini, "Hands", s == 1 ? "WristCutA" : "WristCutB",
                                 -10.0f);   // VR-188: the Cuffs preset ships as the default
        g_msCutSet[s] = (c > -1e8f) ? 1 : 0;
        if (g_msCutSet[s]) g_msCutRel[s] = c;
    }
    for (int s = 1; s <= 2; s++) {
        if (g_msWristScale[s] < 0.2f) g_msWristScale[s] = 0.2f;
        if (g_msWristScale[s] > 5.0f) g_msWristScale[s] = 5.0f;
    }
    g_matStepMs       = IniFloat(ini, "Hands", "MatStepMs", 5000.0f);
    if (g_matStepMs < 1500.0f)  g_matStepMs = 1500.0f;
    if (g_matStepMs > 20000.0f) g_matStepMs = 20000.0f;
    g_crouchHideCyl   = IniFloat(ini, "Hands", "CrouchHideCyl", 76.0f);
    if (g_crouchHideCyl < 20.0f) g_crouchHideCyl = 20.0f;
    if (g_crouchHideCyl > 87.0f) g_crouchHideCyl = 87.0f;
    g_crouchHideScale = IniFloat(ini, "Hands", "CrouchHideScale", 0.02f);
    if (g_crouchHideScale < 0.002f) g_crouchHideScale = 0.002f;
    if (g_crouchHideScale > 1.0f)   g_crouchHideScale = 1.0f;
    g_handFloorCfg    = IniFloat(ini, "Hands", "HandFloor", 1) != 0.0f;      // 38.27
    g_handFloorMargin = IniFloat(ini, "Hands", "HandFloorMargin", 4.0f);
    if (g_handFloorMargin < 0.0f)  g_handFloorMargin = 0.0f;
    if (g_handFloorMargin > 40.0f) g_handFloorMargin = 40.0f;
    g_skcNeutralSaved = IniFloat(ini, "Hands", "NeutralSaved", 0) != 0.0f;
    if (g_skcNeutralSaved) {
        static const char* kAx[3] = { "Right", "Up", "Fwd" };
        for (int hh = 0; hh < 2; hh++) {
            for (int q = 0; q < 3; q++) {
                char k[64];
                _snprintf(k, 64, "Neutral%c%s", hh ? 'R' : 'L', kAx[q]);
                g_skcNeutral[hh][q] = IniFloat(ini, "Hands", k, 0.0f);
            }
            g_skcHaveNeutral[hh] = true;
        }
        Log("config: hand neutrals LOADED  L=(%.3f,%.3f,%.3f) R=(%.3f,%.3f,%.3f) m"
            " - no per-launch calibration needed",
            g_skcNeutral[0][0], g_skcNeutral[0][1], g_skcNeutral[0][2],
            g_skcNeutral[1][0], g_skcNeutral[1][1], g_skcNeutral[1][2]);
    } else {
        Log("config: no saved hand neutral - the first good pose this session "
            "will be captured AND saved, so this is the last time");
    }
    if (g_skcHandSize < 0.3f) g_skcHandSize = 0.3f;
    if (g_skcHandSize > 2.0f) g_skcHandSize = 2.0f;
    g_skcStrength= IniFloat(ini, "Hands", "Strength", 1.0f);
    { static const char* tk[3] = { "TrimFwd", "TrimRight", "TrimUp" };
      for (int hh = 0; hh < 2; hh++)
        for (int q = 0; q < 3; q++) {
            char k[32];
            _snprintf(k, sizeof(k), "%s%s", hh ? "R" : "L", tk[q]);
            g_skcTrim[hh][q] = IniFloat(ini, "Hands", k, 0.0f);
        } }
    Log("config: hands drive %s (controllers=%d world=%d pos=%d rot=%d add=%d "
        "scale %.0f uu/m)", g_skcDrive ? "ON" : "off", (int)g_skcLive,
        (int)g_skcWorld, (int)g_skcDoTrans, (int)g_skcDoRot, (int)g_skcAddMode,
        g_skcScaleUU);
    g_heightOffsetM = IniFloat(ini, "Tracking", "HeightOffsetM", 0.06f);
    if (g_heightOffsetM < -1.5f) g_heightOffsetM = -1.5f;
    if (g_heightOffsetM >  1.5f) g_heightOffsetM =  1.5f;
    g_crouchOn     = IniFloat(ini, "Tracking", "PhysicalCrouch", 1) != 0.0f;
    g_crouchDropM  = IniFloat(ini, "Tracking", "CrouchDropM", 0.22f);
    g_crouchReleaseM = IniFloat(ini, "Tracking", "CrouchStandM", 0.14f);
    if (g_crouchDropM < 0.05f) g_crouchDropM = 0.05f;
    if (g_crouchDropM > 0.80f) g_crouchDropM = 0.80f;
    if (g_crouchReleaseM > g_crouchDropM - 0.02f) g_crouchReleaseM = g_crouchDropM - 0.02f;
    Log("config: physical crouch %s (down at %.2f m, up at %.2f m) - this "
        "line reports the SETTING; watch for 'crouch: DOWN' to know it fires",
        g_crouchOn ? "armed" : "off", g_crouchDropM, g_crouchReleaseM);
    {   // VR-196: the F10 view level. [Overlay] Level=basic|advanced|debug; an ini that only has
        // the old DevTools=1 opens on debug, so a maintainer's panel does not shrink on upgrade.
        char lv[16] = "";
        GetPrivateProfileStringA("Overlay", "Level", "", lv, sizeof(lv), ini);
        const int legacy = IniFloat(ini, "Overlay", "DevTools", 0) != 0.0f ? dvr::ovl::Debug : dvr::ovl::Basic;
        dvr::ovl::set_level(dvr::ovl::parse_level(lv, legacy));
        g_ovlDev = dvr::ovl::level() == dvr::ovl::Debug;
        Log("config: F10 view level %s ([Overlay] Level%s)", dvr::ovl::level_name(dvr::ovl::level()),
            lv[0] ? "" : legacy == dvr::ovl::Debug ? ", migrated from DevTools=1" : ", default");
    }
    // VR-174: the F10 panel from the controllers, on by default. [Overlay] PointerSpeed is
    // retired: the cursor comes from the eye's FOV now, not a gain.
    g_ovlPtrEnable = IniFloat(ini, "Overlay", "ControllerPointer", 1) != 0.0f;
    g_ovlPtrHand = IniFloat(ini, "Overlay", "PointerHand", 1) != 0.0f ? 1 : 0;
    g_ovlReticle = IniFloat(ini, "Overlay", "ReticleWhileOpen", 1) != 0.0f;
    dvr::vr::set_chord_tap_opens_panel(g_ovlPtrEnable);
    {
        const float ui = IniFloat(ini, "Overlay", "UiScale", 0.0f);   // 0 = from the eye texture
        if (ui >= 0.8f && ui <= 2.5f) g_ovlUiScale = ui;
    }
    g_autoHand      = IniFloat(ini, "HandTracking", "AutoStart", 1) != 0.0f;
    // 32.96: was 4 s on top of discovery time - the user asked why motion
    // controls take so long after a load. 1.5 s is enough for the rig to be
    // real; everything the auto-start needs is already gated on the pawn and
    // both controllers being live.
    g_autoHandDelay = IniFloat(ini, "HandTracking", "DelaySec", 4);
    if (g_autoHandDelay < 0.5f)  g_autoHandDelay = 0.5f;
    if (g_autoHandDelay > 60.0f) g_autoHandDelay = 60.0f;
    g_fpPosOn  = IniFloat(ini, "HandTracking", "Depth", 1) != 0.0f;
    g_fpRollOn = IniFloat(ini, "HandTracking", "WristRoll", 0) != 0.0f;
    g_anchorTau = IniFloat(ini, "HandTracking", "AnchorTauSec", 2.5f);
    if (g_anchorTau < 0.5f)  g_anchorTau = 0.5f;
    if (g_anchorTau > 30.0f) g_anchorTau = 30.0f;
    {
        int hv = (int)IniFloat(ini, "HandTracking", "ArmHideValue", 2);
        if (hv < 0) hv = 0; if (hv > 255) hv = 255;
        g_armVal = (uint8_t)hv;      // experiment dial, in case 0x02 is wrong
    }
    GetPrivateProfileStringA("Debug", "Probe", "", g_dbgProbe,
                             sizeof(g_dbgProbe), ini);
    g_swarmAim = IniFloat(ini, "MotionAim", "SwarmAim", 1) != 0.0f;   // 38.55
    g_introSkip = (int)IniFloat(ini, "Debug", "IntroSkip", 0);    // 38.69
    if (g_introSkip < 0 || g_introSkip > 2) g_introSkip = 0;
    g_introSkipDelayMs = (int)IniFloat(ini, "Debug", "IntroSkipDelayMs", 8000);
    if (g_introSkipDelayMs < 1000)  g_introSkipDelayMs = 1000;
    if (g_introSkipDelayMs > 60000) g_introSkipDelayMs = 60000;
    g_meleeOn     = IniFloat(ini, "Melee", "Enabled", 1) != 0.0f;
    g_meleeSpeed  = IniFloat(ini, "Melee", "SwingSpeed", 1.8f);
    if (g_meleeSpeed < 0.5f) g_meleeSpeed = 0.5f;
    if (g_meleeSpeed > 6.0f) g_meleeSpeed = 6.0f;
    g_meleeHoldMs = IniFloat(ini, "Melee", "HoldMs", 220.0f);
    if (g_meleeHoldMs < 50.0f)  g_meleeHoldMs = 50.0f;
    if (g_meleeHoldMs > 800.0f) g_meleeHoldMs = 800.0f;
    g_meleeCoolMs = IniFloat(ini, "Melee", "CooldownMs", 300.0f);
    if (g_meleeCoolMs < g_meleeHoldMs) g_meleeCoolMs = g_meleeHoldMs;
    if (g_meleeCoolMs > 2000.0f) g_meleeCoolMs = 2000.0f;
    g_meleeSwingMs = IniFloat(ini, "Melee", "SwingMs", 120.0f);
    if (g_meleeSwingMs < 0.0f)   g_meleeSwingMs = 0.0f;
    if (g_meleeSwingMs > 500.0f) g_meleeSwingMs = 500.0f;
    g_meleeSwingDist = IniFloat(ini, "Melee", "SwingDistM", 0.25f);
    if (g_meleeSwingDist < 0.0f)  g_meleeSwingDist = 0.0f;
    if (g_meleeSwingDist > 1.5f)  g_meleeSwingDist = 1.5f;
    g_meleeHaptic = IniFloat(ini, "Melee", "Haptic", 1) != 0.0f;
    Log("config: melee=%d swing=%.1fm/s sustain=%.0fms dist=%.2fm hold=%.0fms "
        "cooldown=%.0fms", (int)g_meleeOn, g_meleeSpeed, g_meleeSwingMs,
        g_meleeSwingDist, g_meleeHoldMs, g_meleeCoolMs);
    {   // [VR]: the runtime layer's keys. XrRuntimeJson names a manifest for a
        // Steam launch that must not depend on an env var (the simulator, or
        // a shim); XrHaptics=0 kills haptics; FpsCap is the even-cadence limiter.
        GetPrivateProfileStringA("VR", "XrRuntimeJson", "", g_xrJsonIni,
                                 sizeof(g_xrJsonIni), ini);
        if (g_xrJsonIni[0])
            Log("config: XR runtime manifest (ini): %s", g_xrJsonIni);
        dvr::vr::set_runtime_json(g_xrJsonIni);
        {
            char rm[16] = "";
            GetPrivateProfileStringA("VR", "Runtime", "auto", rm, sizeof(rm), ini);
            dvr::vr::set_runtime_mode(rm);
        }
        g_xrHaptics = GetPrivateProfileIntA("VR", "XrHaptics", 1, ini) != 0; // 38.10
        g_vrKeepAlive = GetPrivateProfileIntA("Screen", "KeepAliveUnfocused", 1, ini) != 0; // 38.78
        g_chainStamp = GetPrivateProfileIntA("HeadTrack", "ChainStamp", 1, ini) != 0; // 38.88
        g_fpsCap = IniFloat(ini, "VR", "FpsCap", 0.0f);                  // 38.14
        {
            char source[32] = "";
            GetPrivateProfileStringA("VR", "DesktopEyeSource", "", source, sizeof(source), ini);
            dvr::desktop_eye::set_source(source[0] ? source : "draw",
                source[0] ? ini : "compiled default (ini key absent)");
            dvr::desktop_eye::set_reduced_present(GetPrivateProfileIntA("VR", "ReduceDesktopPresent", 1, ini) != 0);
            dvr::desktop_eye::set_mirror_off(GetPrivateProfileIntA("VR", "DesktopMirrorOff", 1, ini) != 0);
            dvr::desktop_eye::set_strict_off(GetPrivateProfileIntA("VR", "DesktopMirrorStrictOff", 1, ini) != 0);
        }
        // ApiLayerGuard runs before LoadConfig and reads this key itself; the
        // read here only keeps the global in step for the ini rewrite.
        g_algGuard = IniFloat(ini, "VR", "DisableBadApiLayers", 1) != 0.0f;
        if (g_fpsCap < 0.0f) g_fpsCap = 0.0f;
        if (g_fpsCap > 0.0f && g_fpsCap < 20.0f)  g_fpsCap = 20.0f;
        if (g_fpsCap > 144.0f) g_fpsCap = 144.0f;
        dvr::frame::set_fps_cap(g_fpsCap);
        if (g_fpsCap > 0.0f)
            Log("config: FPS cap %.1f (even-cadence limiter)", g_fpsCap);
        {   // 41.1: [Pace] - the projection layer's pacing levers (defaults = today)
            const int ahead = GetPrivateProfileIntA("Pace", "Ahead", 0, ini);
            const int strict = GetPrivateProfileIntA("Pace", "Strict", 0, ini);
            const int lag = GetPrivateProfileIntA("Pace", "Lag", 2, ini);
            int syncHz = GetPrivateProfileIntA("Pace", "SyncHz", 0, ini);
            dvr::vr::set_pace_ahead(ahead);
            dvr::vr::set_pair_strict(strict != 0);
            dvr::vr::set_pose_lag(lag);
            dvr::vr::set_image_orientation(GetPrivateProfileIntA("Pace","ImageOrientation",1,ini)!=0);
            // PRINT WHAT IT RESOLVED TO, AND WHETHER THE FILE SAID SO. Two headset
            // tests were wasted shipping a changed compiled default to a machine
            // whose ini names the key: the loader reads a default only when the key
            // is ABSENT, so both builds ran at the old value and the result was read
            // as the new value failing. A source diff is not an effective setting.
            {
                const bool fromFile =
                    GetPrivateProfileIntA("Pace", "Lag", -1, ini) != -1;
                Log("config: [Pace] Lag=%d - %s. This is the value the submission "
                    "path will use; the per-frame log field 'chosen by lag arm N' "
                    "is what it actually did.",
                    lag, fromFile ? "FROM THE INI, which overrides the built-in "
                                    "default. Changing the default in the source "
                                    "will NOT change this machine."
                                  : "the built-in default (the ini does not name "
                                    "the key)");
            }
            // The gate REFUSES a value it cannot honour rather than half-arming
            // it: the number that was read is logged and sync stays off, so a
            // typo cannot silently pace the game at 3 Hz.
            if (syncHz != 0 && (syncHz < 10 || syncHz > 500)) {
                Log("config: [Pace] SyncHz=%d is out of range (10..500, or 0 for off) - REFUSED, the pair "
                    "schedule free-runs", syncHz);
                syncHz = 0;
            }
            dvr::vr::set_pace_sync_hz((unsigned)syncHz);
            dvr::vr::set_pace_sync(syncHz != 0);
            if (ahead || strict || lag != 1 || syncHz)
                Log("config: [Pace] Ahead=%d Strict=%d Lag=%d SyncHz=%d%s", ahead, strict, lag, syncHz,
                    syncHz ? " (the pair schedule is LOCKED to that rate; `vrpace sync off` is the live A/B)"
                           : " (levers off today's behaviour)");
        }
        // 37.5: SAFE mode - rendering + head tracking only, every game-memory
        // writer held back. The crash bisector: if XR-safe holds stable, one
        // of the writers is the killer; if it still dies, they are innocent.
        char sf[8] = "";
        if (GetEnvironmentVariableA("DISHONORED_VR_XR_SAFE", sf, sizeof(sf))
            && sf[0] == '1') {
            g_rotInject = false;       // no head->camera rotation writes
            g_fovLever  = 0.0f;        // no FOV enforcement writes
            dvr::camera::set_fov_deg(g_fovLever);
            g_skcDrive  = false;       // no SkelControl hand writes
            g_handMesh  = false;       // no hand collect/drive
            g_autoHandDone = true;     // and no auto re-arm of it
            g_blkAimOnCfg = false;     // no blink native detour writes
            g_blkDriveUI  = false;
            g_meleeOn   = false;
            Log("config: XR SAFE MODE - look around only, all game-memory "
                "writers OFF (crash bisector)");
        }
    }
    // 40.3 GAMEPAD-ONLY. The rendering is not converged (world scale, the
    // frame aspect and the FOV lever are still being fitted against each
    // other), and motion controls make that harder to judge: hand meshes and
    // a weapon that follow a mis-scaled world give the eye a second, wrong
    // reference for how big things are, and every hand calibration is one
    // more variable in a run that is supposed to be measuring one. So the
    // controllers stayed a plain gamepad until the render was settled.
    // VR-72: it ships 0 now - the headset-confirmed configuration runs the hands,
    // weapon placement and motion controls - and 1 remains the bisector it was.
    //
    // What stays ON deliberately: head tracking and its rotation writes,
    // positional head tracking, the FOV lever, and the virtual gamepad. This
    // is NOT the XR SAFE bisector above - that one also stops the head.
    //
    // The author's process rules say motion crouch and hands "must never stop
    // working". This does not retire them: it is one key, it logs loudly, and
    // the code is untouched. Set GamepadOnly=0 to get them all back.
    g_gamepadOnly = IniFloat(ini, "Mode", "GamepadOnly", 0) != 0.0f;
    if (g_gamepadOnly) {
        g_skcDrive     = false;    // no SkelControl hand writes
        g_handMesh     = false;    // no hand mesh collect/drive
        g_autoHandDone = true;     // and no auto re-arm of it
        g_blkAimOnCfg  = false;    // Blink aims down the view, not the hand
        g_blkDriveUI   = false;
        g_meleeOn      = false;    // no motion melee
        g_maimEnabled  = false;    // no motion aim
        g_crouchOn     = false;    // fixed eye height - a moving one is a
                                   // second variable while judging scale
        Log("config: [Mode] GamepadOnly=1 - the controllers are a plain "
            "gamepad. Hands, hand mesh, motion aim, motion melee, motion "
            "crouch and controller Blink aim are all OFF, and no hand or "
            "weapon model is scaled. Head tracking, positional tracking and "
            "the FOV lever are UNAFFECTED and still running. This is a "
            "deliberate default while the render is being fitted - set "
            "GamepadOnly=0 in dishonored_vr.ini to restore motion controls.");
    }
    Log("config: handtracking autostart=%d delay=%.0fs depth=%d roll=%d probe='%s'",
        (int)g_autoHand, g_autoHandDelay, (int)g_fpPosOn, (int)g_fpRollOn,
        g_dbgProbe);
    Log("config: tracking=%d yaw=%.1f pitch=%.1f screen dist=%.2f width=%.2f | pos=%d scale=%.0f max=%.2f flipx=%d",
        (int)g_trackingEnabled, g_yawCounts, g_pitchCounts,
        g_screenDist, g_screenWidth,
        (int)g_posTrack, g_posScaleUU, g_posMaxM, (int)g_posFlipX);
}

static void EnsureConfig()
{
    if (g_configLoaded || g_disabled) return;
    g_configLoaded = true;
    // BEFORE anything else can reach the OpenXR loader. An implicit API layer
    // that cannot load in a 32-bit process fails xrCreateInstance for the native
    // runtime AND the shim with the same error, which reads as "no VR at all"
    // and sends the diagnosis at the runtime instead of at the layer. Measured
    // on 2026-09-06: an OBS mirror layer, x64 only, registered under HKCU (which
    // WOW64 does not redirect), XrResult(-32).
    ApiLayerGuard();
    LoadConfig();
        {   // [Log] Level=info  Cats=blink:debug,openxr:trace - env DVR_LOG / DVR_LOG_CATS win
            char ini[MAX_PATH], lv[32] = "", cats[512] = "";
            _snprintf(ini, MAX_PATH, "%s\\dishonored_vr.ini", g_dir);
            GetPrivateProfileStringA("Log", "Level", "", lv, sizeof(lv), ini);
            GetPrivateProfileStringA("Log", "Cats", "", cats, sizeof(cats), ini);
            if (!GetEnvironmentVariableA("DVR_LOG", NULL, 0)) dvr::log::configure(lv, "");
            if (!GetEnvironmentVariableA("DVR_LOG_CATS", NULL, 0)) dvr::log::configure("", cats);
        }
}


// 41.1 (session 8): the [Device] keys are launch-time; the seam word and the
// F10 tickbox write them for the NEXT launch (no version bump: the rewrite
// would wipe a tuned ini), and say so.
// 41.1 (session 9): one key into the ini for the F10 tickboxes that are live
// AND persistent (the trace, the c5 pairing), so a tester's choice survives
// the next launch without an ini edit.
static void ConfigWriteKey(const char* section, const char* key, const char* value, const char* who)
{
    char ini[MAX_PATH];
    _snprintf(ini, MAX_PATH, "%s\\dishonored_vr.ini", g_dir);
    WritePrivateProfileStringA(section, key, value, ini);
    Log("config: [%s] %s=%s written by %s (live now, and the next launch's default)", section, key, value, who);
}
static void DeviceSetEx(bool on, const char* who)
{
    char ini[MAX_PATH];
    _snprintf(ini, MAX_PATH, "%s\\dishonored_vr.ini", g_dir);
    WritePrivateProfileStringA("Device", "Ex", on ? "1" : "0", ini);
    // The only reason to ask for the 9Ex device is the shared capture, and the
    // 2026-09-03 headset run asked for the device and never switched the
    // capture: the tickbox picks the capture mode for the next launch too.
    WritePrivateProfileStringA("Capture", "Mode", on ? "shared" : "deferred", ini);
    Log("device: [Device] Ex=%d and [Capture] Mode=%s written by %s - both take effect at the NEXT LAUNCH (this "
        "run's device %s 9Ex, capture %s)",
        on ? 1 : 0, on ? "shared" : "deferred", who, dvr::d3d9ex::device_is_ex() ? "IS" : "is not",
        dvr::capture::mode_name());
}
static void DeviceSetManaged(const char* name, const char* who)
{
    dvr::d3d9ex::Managed m;
    if (!dvr::d3d9ex::parse_managed(name, &m)) { Log("device: managed none|default|dynamic|shadow (asked '%s')", name); return; }
    char ini[MAX_PATH];
    _snprintf(ini, MAX_PATH, "%s\\dishonored_vr.ini", g_dir);
    WritePrivateProfileStringA("Device", "Managed", dvr::d3d9ex::managed_name(m), ini);
    Log("device: [Device] Managed=%s written by %s - takes effect at the NEXT LAUNCH (inert while Ex=0)",
        dvr::d3d9ex::managed_name(m), who);
}

static bool ConfigResetPending() { return g_configResetPending; }

static bool ConfigRequestReset()
{
    char ini[MAX_PATH];
    _snprintf(ini, sizeof(ini), "%s\\dishonored_vr.ini", g_dir);
    const bool ok = WritePrivateProfileStringA("Meta", "ResetDefaults", "1", ini) != FALSE;
    Log("config: reset to shipped defaults %s; runtime and DataDir will be preserved",
        ok ? "queued for next launch" : "FAILED");
    if (ok) g_configResetPending = true;
    return ok;
}

static void OverlaySaveDefaults()
{
    char ini[MAX_PATH];
    _snprintf(ini, MAX_PATH, "%s\\dishonored_vr.ini", g_dir);
    WritePrivateProfileStringA("Perf", "CpuScopes", dvr::perf::cpu_scopes_enabled() ? "1" : "0", ini);
    WritePrivateProfileStringA("Perf", "NativeProfile", dvr::native_profile::enabled() ? "1" : "0", ini);
    WritePrivateProfileStringA("Perf", "BridgeGpu", dvr::bridge_profile::enabled() ? "1" : "0", ini);
    WritePrivateProfileStringA("Menu", "CacheNameLookups", g_nameIndexCacheOn ? "1" : "0", ini);
    WritePrivateProfileStringA("Menu", "PawnFromController", g_pawnFromController ? "1" : "0", ini);
    char v[64];
    const auto controller=dvr::controller::config();
    _snprintf(v,64,"%d",controller.modifier);
    WritePrivateProfileStringA("Controllers","DpadModifier",v,ini);
    WritePrivateProfileStringA("Controllers","DpadFlip",controller.flip ? "1" : "0",ini);
    WritePrivateProfileStringA("Controllers","PauseChord",controller.pauseChord ? "1" : "0",ini);

    _snprintf(v, 64, "%.1f", g_posScaleUU);
    WritePrivateProfileStringA("PosTrack", "Scale", v, ini);
    _snprintf(v, 64, "%.2f", g_screenDist);
    WritePrivateProfileStringA("Screen", "DistanceMeters", v, ini);
    _snprintf(v, 64, "%.3f", g_heightOffsetM);
    WritePrivateProfileStringA("Tracking", "HeightOffsetM", v, ini);
    _snprintf(v, 64, "%.0f", (float)g_fovLever);      // 30.51: persist the lever
    WritePrivateProfileStringA("Screen", "FovLever", v, ini);
    _snprintf(v,64,"%.2f",ProjectionFovGet());
    WritePrivateProfileStringA("Screen","ProjectionFov",v,ini);
    // 30.70: the hand drive's live-tuned values, so a good calibration sticks
    WritePrivateProfileStringA("HandRender", "Enabled", g_rtdEnable ? "1" : "0", ini);
    WritePrivateProfileStringA("HandRender", "DriveArms", g_rtdDoArms ? "1" : "0", ini);
    WritePrivateProfileStringA("HandRender", "DriveWeapon", g_rtdDoWpn ? "1" : "0", ini);
    _snprintf(v, 64, "%.2f", g_rtdPivotMix);
    WritePrivateProfileStringA("HandRender", "PivotMix", v, ini);
    _snprintf(v, 64, "%.0f", g_rtdScaleUU);
    WritePrivateProfileStringA("HandRender", "ScaleUU", v, ini);
    _snprintf(v, 64, "%.0f", g_rtdPosMax);
    WritePrivateProfileStringA("HandRender", "MaxOffsetUU", v, ini);
    _snprintf(v, 64, "%.2f", g_rtdSmooth);
    WritePrivateProfileStringA("HandRender", "SmoothAlpha", v, ini);
    WritePrivateProfileStringA("HandRender", "RotInvert", g_rtdRotInvert ? "1" : "0", ini);
    _snprintf(v, 64, "%.2f", g_rtdRotScale);
    WritePrivateProfileStringA("HandRender", "RotScale", v, ini);
    _snprintf(v, 64, "%.0f", g_rtdPivotUp);
    WritePrivateProfileStringA("HandRender", "PivotUp", v, ini);
    _snprintf(v, 64, "%u", g_rtdSizeArms);          // whatever the identifier proved
    WritePrivateProfileStringA("HandRender", "ArmsRegs", v, ini);
    _snprintf(v, 64, "%u", g_rtdSizeWpn);
    WritePrivateProfileStringA("HandRender", "WeaponRegs", v, ini);
    { static const char* kLKey[3] = { "LTrimX", "LTrimY", "LTrimZ" };
      static const char* kRKey[3] = { "RTrimX", "RTrimY", "RTrimZ" };
      static const char* kWpnKey[3] = { "WpnYaw", "WpnPitch", "WpnRoll" };
      for (int k = 0; k < 3; k++) {
          _snprintf(v, 64, "%.1f", g_rtdTrim[0][k]);
          WritePrivateProfileStringA("HandRender", kLKey[k], v, ini);
          _snprintf(v, 64, "%.1f", g_rtdTrim[1][k]);
          WritePrivateProfileStringA("HandRender", kRKey[k], v, ini);
          _snprintf(v, 64, "%.1f", g_rtdWpnYPR[k]);
          WritePrivateProfileStringA("HandRender", kWpnKey[k], v, ini);
      } }
    _snprintf(v, 64, "%u", g_rtdSizeWpn2);
    WritePrivateProfileStringA("HandRender", "Weapon2Regs", v, ini);
    WritePrivateProfileStringA("HandRender", "WeaponHand",  g_rtdWpnHand  ? "1" : "0", ini);
    WritePrivateProfileStringA("HandRender", "Weapon2Hand", g_rtdWpn2Hand ? "1" : "0", ini);
    WritePrivateProfileStringA("HandRender", "Hand", g_rtdArmsHand ? "right" : "left", ini);
    _snprintf(v, 64, "%d", g_rtdSplitLo);
    WritePrivateProfileStringA("HandRender", "RightArmFirstBone", v, ini);
    _snprintf(v, 64, "%d", g_rtdSplitHi);
    WritePrivateProfileStringA("HandRender", "RightArmLastBone", v, ini);
    WritePrivateProfileStringA("HandRender", "ShowRings", g_rtdMarkers ? "1" : "0", ini);
    _snprintf(v, 64, "%.3f", g_rtdMarkSize);
    WritePrivateProfileStringA("HandRender", "RingSizeMeters", v, ini);
    _snprintf(v, 64, "%.2f", g_rtdFollowYaw);
    WritePrivateProfileStringA("HandRender", "FollowHeadYaw", v, ini);
    _snprintf(v, 64, "%.2f", g_rtdFollowPitch);
    WritePrivateProfileStringA("HandRender", "FollowHeadPitch", v, ini);
    for (int q = 0; q < 3; q++) {
        char k[32];
        _snprintf(k, sizeof(k), "Axis%dSource", q);
        _snprintf(v, 64, "%d", g_rtdMapSrc[q]);
        WritePrivateProfileStringA("HandRender", k, v, ini);
        _snprintf(k, sizeof(k), "Axis%dFlip", q);
        WritePrivateProfileStringA("HandRender", k, g_rtdMapSgn[q] < 0.0f ? "1" : "0", ini);
    }
    WritePrivateProfileStringA("HandRender", "RouteByDrawOrder",
                               g_rtdUseOrdinals ? "1" : "0", ini);
    { char ob[64]; int n2 = 0;
      for (int q = 0; q < 8; q++)
          n2 += _snprintf(ob + n2, (int)sizeof(ob) - n2, q ? ",%d" : "%d", g_rtdOrdHand[q]);
      WritePrivateProfileStringA("HandRender", "DrawOrderHands", ob, ini); }

    // 31.9: the [Hands] section was LOADED but never SAVED - I wrote the config
    // reader and forgot the writer, so every trim and toggle tuned in the
    // headset was silently discarded on exit. Everything the panel can change
    // is written here now.
    WritePrivateProfileStringA("Hands", "Enabled", g_skcDrive ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "BoneVisHide", g_boneVisCfg ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "MatCensus", g_matCensusCfg ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "MatAuto", g_matAutoCfg ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "MatCycle", g_matCycleCfg ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "DrawCensus", g_dcOn ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "ArmSplit", g_msOn ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "ArmSplitAuto", g_msAuto ? "1" : "0", ini);
    _snprintf(v, 64, "%d", g_msMode);
    WritePrivateProfileStringA("Hands", "ArmSplitMode", v, ini);
    _snprintf(v, 64, "%u", g_msWantPrims);
    WritePrivateProfileStringA("Hands", "ArmMeshPrims", v, ini);
    _snprintf(v, 64, "%u", g_msWantVerts);
    WritePrivateProfileStringA("Hands", "ArmMeshVerts", v, ini);
    _snprintf(v, 64, "%.2f", g_msWristScale[1]);
    WritePrivateProfileStringA("Hands", "WristScaleA", v, ini);
    _snprintf(v, 64, "%.2f", g_msWristScale[2]);
    WritePrivateProfileStringA("Hands", "WristScaleB", v, ini);
    WritePrivateProfileStringA("Hands", "WristPlane", g_msPlane ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "CutCap", g_msCap ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "RoundedWrist", g_msRoundWrist ? "1" : "0", ini);
    _snprintf(v,64,"%.3f",g_msRoundDepth);
    WritePrivateProfileStringA("Hands", "RoundedWristDepth", v, ini);
    WritePrivateProfileStringA("Hands", "CutCapTwoSided", g_msCapTwo ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "PoseReport", g_prOn ? "1" : "0", ini);
#if DVR_WITH_LEGACY
    WritePrivateProfileStringA("Hands", "BoneQuery", g_bqOn ? "1" : "0", ini);
#endif
#if DVR_WITH_LEGACY
    WritePrivateProfileStringA("Hands", "HandMoveTest", g_hmOn ? "1" : "0", ini);
#endif
    WritePrivateProfileStringA("Hands", "Palette", g_mpOn ? "1" : "0", ini);
#if DVR_WITH_LEGACY
    _snprintf(v, 64, "%.1f", g_mpAmount);
    WritePrivateProfileStringA("Hands", "PaletteAmount", v, ini);
    _snprintf(v, 64, "%d", g_mpAxis);
    WritePrivateProfileStringA("Hands", "PaletteAxis", v, ini);
    _snprintf(v, 64, "%d", g_mpHand);
    WritePrivateProfileStringA("Hands", "PaletteHand", v, ini);
#endif
    _snprintf(v, 64, "%.2f", g_mpDriveGain);
    WritePrivateProfileStringA("Hands", "PaletteDriveGain", v, ini);
    WritePrivateProfileStringA("Hands", "PaletteWorld", g_mpWorld ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "PaletteDepthRange", g_mpDepth ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "PaletteEyeOffset", g_mpEyeOffset ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands","PaletteEyeMenuHalfStep",g_mpEyeMenuHalfStep ? "1" : "0",ini);
    WritePrivateProfileStringA("Hands", "PaletteEyePredictToggle", g_mpEyePredict ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "PaletteEyeHunt", g_mpEyeHunt ? "1" : "0", ini);
#if DVR_WITH_LEGACY
    WritePrivateProfileStringA("Hands", "PaletteCapture", g_pcOn ? "1" : "0", ini);
#endif
    _snprintf(v, 64, "%.4f", g_mpWsumTol);
    WritePrivateProfileStringA("Hands", "PaletteWeightTol", v, ini);
#if DVR_WITH_LEGACY
    WritePrivateProfileStringA("Hands", "PaletteStep", g_mpStep ? "1" : "0", ini);
#endif
    WritePrivateProfileStringA("Hands", "PaletteRotate", g_mpRotate ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "AttachViewLens", g_waViewLens ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "AttachWeapons", g_waOn ? "1" : "0", ini);
    _snprintf(v, 64, "%.2f", g_waAngTolDeg);
    WritePrivateProfileStringA("Hands", "AttachAngleTol", v, ini);
    _snprintf(v, 64, "%.2f", g_waPosTolUU);
    WritePrivateProfileStringA("Hands", "AttachPosTol", v, ini);
    _snprintf(v, 64, "%.1f", g_waMarginX);
    WritePrivateProfileStringA("Hands", "AttachMargin", v, ini);
    _snprintf(v, 64, "%d", g_waMaxTry);
    WritePrivateProfileStringA("Hands", "AttachMaxTry", v, ini);
    WritePrivateProfileStringA("Hands", "AttachGhostFix", g_waGhostFix ? "1" : "0", ini);
#if DVR_WITH_LEGACY
    WritePrivateProfileStringA("Hands", "AttachProbe", g_waProbe ? "1" : "0", ini);
#endif
    WritePrivateProfileStringA("Hands", "AttachCensus", g_waCensusOn ? "1" : "0", ini);
    _snprintf(v, 64, "%.0f", g_waSnapMaxMs);
    WritePrivateProfileStringA("Hands", "AttachSnapshotMaxMs", v, ini);
    WritePrivateProfileStringA("Hands", "AttachSuppressUnplaced",
                               g_waSuppressUnplaced ? "1" : "0", ini);
    _snprintf(v, 64, "%.0f", g_waViewModelUU);
    WritePrivateProfileStringA("Hands", "AttachViewModelUU", v, ini);
    _snprintf(v, 64, "%.2f", g_waNearAngDeg);
    WritePrivateProfileStringA("Hands", "AttachNearAngle", v, ini);
    _snprintf(v, 64, "%.2f", g_waNearPosUU);
    WritePrivateProfileStringA("Hands", "AttachNearPos", v, ini);
    _snprintf(v, 64, "%.2f", g_waNearMargin);
    WritePrivateProfileStringA("Hands", "AttachNearMargin", v, ini);
    WritePrivateProfileStringA("Hands", "AttachDropUncorrected",
                               g_waDropUncorrected ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "AttachEquippedMembers",
                               g_waEquippedMembers ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "AttachVerifyInstance",
                               g_waVerifyInstance ? "1" : "0", ini);
    _snprintf(v, 64, "%d", g_waHeldMaxPresents);
    WritePrivateProfileStringA("Hands", "AttachHeldMaxPresents", v, ini);
    WritePrivateProfileStringA("Hands", "AttachRequireFreshRef",
                               g_waReqFreshRef ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "AttachRequireLiveMember",
                               g_waReqLiveMember ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "AttachVetoReleasesBuffers",
                               g_waVetoFrees ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "AttachInstanceVetoRelaxed",
                               g_waVetoRelaxed ? "1" : "0", ini);
    _snprintf(v, 64, "%d", g_waRefPresents);
    WritePrivateProfileStringA("Hands", "AttachRefMaxPresents", v, ini);
    // Format IMMEDIATELY before the write. This line used to reuse whatever `v`
    // last held, because its _snprintf sat seventeen lines above beside a write
    // that does not take `v` at all - so every save wrote AttachRefMaxPresents's
    // value (2) into AttachRigRadius. 2 clamps up to 10 on the next load, at
    // which bound every weapon is refused as not being on the view model and the
    // weapons stop tracking the hands, while the log prints the CLAMPED 10 and
    // every nearby counter reads healthy. See TRAPS.
    _snprintf(v, 64, "%.0f", g_waRigRadiusUU);
    WritePrivateProfileStringA("Hands", "AttachRigRadius", v, ini);
    _snprintf(v, 64, "%.0f", g_waPassRadiusUU);
    WritePrivateProfileStringA("Hands", "AttachPassRadius", v, ini);
#if DVR_WITH_LEGACY
    _snprintf(v, 64, "%d", g_waProbeBudget);
    WritePrivateProfileStringA("Hands", "AttachProbeBudget", v, ini);
#endif
    _snprintf(v, 64, "%d", g_waSwordHand);
    WritePrivateProfileStringA("Hands", "AttachSwordHand", v, ini);
    _snprintf(v, 64, "%d", g_waXbowHand);
    WritePrivateProfileStringA("Hands", "AttachCrossbowHand", v, ini);
#if DVR_WITH_LEGACY
    WritePrivateProfileStringA("Hands", "WeaponId", g_wiOn ? "1" : "0", ini);
    _snprintf(v, 64, "%.0f", g_wiPhaseMs);
    WritePrivateProfileStringA("Hands", "WeaponIdMs", v, ini);
#endif
    _snprintf(v, 64, "%.4f", g_mpFrameTolOrtho);
    WritePrivateProfileStringA("Hands", "PaletteFrameTol", v, ini);
    {
        static const char* ax[3] = { "X", "Y", "Z" };
        for (int h = 0; h < 2; h++) {
            const char* sfx = h ? "R" : "L";
            char key[32];
            _snprintf(key, sizeof(key), "Grip%sVersion", sfx);
            _snprintf(v, 64, "%d", g_mpGripVer[h]);
            WritePrivateProfileStringA("Hands", key, v, ini);
            _snprintf(key, sizeof(key), "Grip%sParity", sfx);
            _snprintf(v, 64, "%d", g_mpGripParity[h]);
            WritePrivateProfileStringA("Hands", key, v, ini);
            for (int a = 0; a < 3; a++) {
                _snprintf(key, sizeof(key), "Grip%s%s", sfx, ax[a]);
                _snprintf(v, 64, "%.4f", g_mpGripDeg[h][a]);
                WritePrivateProfileStringA("Hands", key, v, ini);
            }
        }
        for (int h = 0; h < 2; h++) {
            const char* sfx = h ? "R" : "L";
            char key[32];
            for (int a = 0; a < 3; a++) {
                _snprintf(key, sizeof(key), "Trim%sT%s", sfx, ax[a]);
                _snprintf(v, 64, "%.4f", g_mpTrimT[h][a]);
                WritePrivateProfileStringA("Hands", key, v, ini);
                _snprintf(key, sizeof(key), "Trim%sR%s", sfx, ax[a]);
                _snprintf(v, 64, "%.2f", g_mpTrimR[h][a]);
                WritePrivateProfileStringA("Hands", key, v, ini);
            }
        }
        WritePrivateProfileStringA("Hands", "PowerTrim", g_mpPowTrimOn ? "1" : "0", ini);
        for (int a = 0; a < 3; a++) {
            char key[32];
            _snprintf(key, sizeof(key), "TrimLPT%s", ax[a]);
            _snprintf(v, 64, "%.4f", g_mpTrimPT[a]);
            WritePrivateProfileStringA("Hands", key, v, ini);
            _snprintf(key, sizeof(key), "TrimLPR%s", ax[a]);
            _snprintf(v, 64, "%.2f", g_mpTrimPR[a]);
            WritePrivateProfileStringA("Hands", key, v, ini);
        }
        _snprintf(v, 64, "%.2f", g_mpModelScale);
        WritePrivateProfileStringA("Hands", "ModelScale", v, ini);
        WritePrivateProfileStringA("Hands", "Adjust", g_mpAdjOn ? "1" : "0", ini);
        _snprintf(v, 64, "%d", g_mpAdjStepT);
        WritePrivateProfileStringA("Hands", "AdjStepT", v, ini);
        _snprintf(v, 64, "%d", g_mpAdjStepR);
        WritePrivateProfileStringA("Hands", "AdjStepR", v, ini);
    }
#if DVR_WITH_LEGACY
    _snprintf(v, 64, "%.1f", g_hmAmount);
    WritePrivateProfileStringA("Hands", "HandMoveUU", v, ini);
    _snprintf(v, 64, "%d", g_hmAxis);
    WritePrivateProfileStringA("Hands", "HandMoveAxis", v, ini);
#endif
    _snprintf(v, 64, "%d", g_msEdge);
    WritePrivateProfileStringA("Hands", "WristEdge", v, ini);
    _snprintf(v, 64, "%d", g_msStepMode);
    WritePrivateProfileStringA("Hands", "WristStep", v, ini);
    _snprintf(v, 64, "%d", g_msAxisMode);
    WritePrivateProfileStringA("Hands", "WristAxis", v, ini);
    if (g_msCutSet[1]) { _snprintf(v, 64, "%.2f", g_msCutRel[1]);
                         WritePrivateProfileStringA("Hands", "WristCutA", v, ini); }
    if (g_msCutSet[2]) { _snprintf(v, 64, "%.2f", g_msCutRel[2]);
                         WritePrivateProfileStringA("Hands", "WristCutB", v, ini); }
    WritePrivateProfileStringA("VR", "DisableBadApiLayers", g_algGuard ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "FromControllers", g_skcLive ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "WorldSpace", g_skcWorld ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "WorldRotation", g_skcWorldRot ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "Position", g_skcDoTrans ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "Rotation", g_skcDoRot ? "1" : "0", ini);
    WritePrivateProfileStringA("Hands", "RemoveMeshRotation",
                               g_skcRemoveMeshRot ? "1" : "0", ini);
    _snprintf(v, 64, "%.2f", g_skcCamStrength);
    WritePrivateProfileStringA("Hands", "CameraLookAtStrength", v, ini);
    _snprintf(v, 64, "%.2f", g_skcHandCtlStr[0]);
    WritePrivateProfileStringA("Hands", "LeftControlStrength", v, ini);
    _snprintf(v, 64, "%.2f", g_skcHandCtlStr[1]);
    WritePrivateProfileStringA("Hands", "RightControlStrength", v, ini);
    {   // 32.25: the crouch trim set
        static const char* kAx3[3] = { "Fwd", "Right", "Up" };
        for (int hh = 0; hh < 2; hh++)
            for (int q = 0; q < 3; q++) {
                char k[64], vv[64];
                _snprintf(k, 64, "CrouchOff%c%s", hh ? 'R' : 'L', kAx3[q]);
                _snprintf(vv, 64, "%.1f", g_skcTrimCrouch[hh][q]);
                WritePrivateProfileStringA("Hands", k, vv, ini);
            }
        WritePrivateProfileStringA("Hands", "PerStanceTrim",
                                   g_skcCrouchTrimOn ? "1" : "0", ini);
        for (int hh = 0; hh < 2; hh++)      // 34.9: the block trim set
            for (int q = 0; q < 3; q++) {
                char k[64], vv[64];
                _snprintf(k, 64, "BlockOff%c%s", hh ? 'R' : 'L', kAx3[q]);
                _snprintf(vv, 64, "%.1f", g_skcTrimBlock[hh][q]);
                WritePrivateProfileStringA("Hands", k, vv, ini);
            }
        WritePrivateProfileStringA("Hands", "BlockTrim",
                                   g_skcBlockTrimOn ? "1" : "0", ini);
        WritePrivateProfileStringA("Hands", "CrawlTuckCamera",           // VR-122
                                   g_crawlTuckCamera ? "1" : "0", ini);
        _snprintf(v, 64, "%d", g_crouchSrc);
        WritePrivateProfileStringA("Hands", "CrouchSource", v, ini);
        _snprintf(v, 64, "%.0f", g_eyeDropUU);
        WritePrivateProfileStringA("Hands", "CrouchDropUU", v, ini);
        _snprintf(v, 64, "%.0f", g_crouchHoldMs);
        WritePrivateProfileStringA("Hands", "CrouchHoldMs", v, ini);
        WritePrivateProfileStringA("Hands", "CrouchDiag",
                                   g_crouchDiag ? "1" : "0", ini);
        WritePrivateProfileStringA("Hands", "CrouchToggle",
                                   g_crouchToggle ? "1" : "0", ini);
        _snprintf(v, 64, "%u", (unsigned)g_crouchBtnMask);
        WritePrivateProfileStringA("Hands", "CrouchButtonMask", v, ini);
        WritePrivateProfileStringA("Hands", "CrouchMaskVer", "2", ini);
        // 35.8: the donor-graft rotation drive's knobs
        WritePrivateProfileStringA("Hands", "GraftRotation",
                                   g_graftWant ? "1" : "0", ini);
        _snprintf(v, 64, "%d", g_graftRotSpace);
        WritePrivateProfileStringA("Hands", "GraftRotSpace", v, ini);
        WritePrivateProfileStringA("Hands", "GraftHeadComp",
                                   g_graftHeadComp ? "1" : "0", ini);
        WritePrivateProfileStringA("Hands", "GraftAimAbs",
                                   g_graftAimAbs ? "1" : "0", ini);
        _snprintf(v, 64, "%.2f", g_graftHCY);
        WritePrivateProfileStringA("Hands", "GraftHeadFollowYaw", v, ini);
        _snprintf(v, 64, "%.2f", g_graftHCP);
        WritePrivateProfileStringA("Hands", "GraftHeadFollowPitch", v, ini);
        WritePrivateProfileStringA("Hands", "RotSignYaw",
                                   g_skcRotSignY < 0 ? "-1" : "1", ini);
        WritePrivateProfileStringA("Hands", "RotSignPitch",
                                   g_skcRotSignP < 0 ? "-1" : "1", ini);
    }
    dvr::anim::save(ini);   // VR-88: the F10 Hands checkbox must survive a restart
    dvr::drop::save(ini);
    dvr::swing::save(ini);  // VR-37: the motion sword's levers
    // VR-117: the HUD redirect, the census, the region probe and the layout
    WritePrivateProfileStringA("Hud", "Panel", dvr::hudcap::enabled() ? "1" : "0", ini);
    _snprintf(v, 64, "%.2f", dvr::hudcap::slot_scale());
    WritePrivateProfileStringA("Hud", "SlotScale", v, ini);
    WritePrivateProfileStringA("Hud", "Regions", dvr::hudclass::regions_enabled() ? "1" : "0", ini);
    WritePrivateProfileStringA("Draws", "Census", dvr::hudclass::census_enabled() ? "1" : "0", ini);
    dvr::hudlayout::save(ini);
    WritePrivateProfileStringA("Menu","SurfaceGuard",UiSurfaceEnabled() ? "1" : "0",ini);
    WritePrivateProfileStringA("Screen","AnchorMono",dvr::vr::mono_anchor_enabled() ? "1" : "0",ini);
    for(unsigned i=0;i<dvr::mono::Count;++i) {
        char key[64]; _snprintf(key,sizeof(key),"Anchor%s",dvr::mono::names[i]);
        WritePrivateProfileStringA("Screen",key,(dvr::vr::mono_anchor_contexts()&(1u<<i)) ? "1" : "0",ini);
    }
    WritePrivateProfileStringA("Cine","HeadLook",CineHeadEnabled() ? "1" : "0",ini);
    WritePrivateProfileStringA("Cine","SpecialHeadLook",SpecialHeadEnabled() ? "1" : "0",ini);
    WritePrivateProfileStringA("Cine","HideBorders",CineBordersEnabled() ? "1" : "0",ini);
    WritePrivateProfileStringA("Cine","StereoState",StereoStateEnabled() ? "1" : "0",ini);
    WritePrivateProfileStringA("Cine","PossessionStereo",PossessionStereoEnabled() ? "1" : "0",ini);
    WritePrivateProfileStringA("Rain","Hide",RainHideEnabled() ? "1" : "0",ini);
    SwordTrailSave(ini);   // VR-171
    CamShakeSave(ini);   // VR-172
    dvr::snap::save(ini);   // VR-219
    WritePrivateProfileStringA("Rain","Trace",RainTraceEnabled() ? "1" : "0",ini);
    { char v[16]; _snprintf(v,sizeof(v),"%d",RainDistance()); WritePrivateProfileStringA("Rain","Distance",v,ini);
      _snprintf(v,sizeof(v),"%d",LensDistance()); WritePrivateProfileStringA("Lens","Distance",v,ini); }
    WritePrivateProfileStringA("Lens","KeepSize",LensKeepSize() ? "1" : "0",ini);
    WritePrivateProfileStringA("Lens","Trace",LensTraceEnabled() ? "1" : "0",ini);
    WritePrivateProfileStringA("Lens","FollowHead",LensFollowHead() ? "1" : "0",ini);
    { char v[16]; _snprintf(v,sizeof(v),"%d",LensRainPct()); WritePrivateProfileStringA("Lens","RainStrength",v,ini); }
    WritePrivateProfileStringA("Mirror","Enabled",WmEnabled() ? "1" : "0",ini);
    WritePrivateProfileStringA("Cine","LockFov",CineFovEnabled() ? "1" : "0",ini);
    WritePrivateProfileStringA("Cine","LockRoll",CineRollEnabled() ? "1" : "0",ini);
    WritePrivateProfileStringA("Camera","HeadBasedMovement",HeadMovementEnabled() ? "1" : "0",ini);
    WritePrivateProfileStringA("Cine","LockPitch",CinePitchEnabled() ? "1" : "0",ini);
    WritePrivateProfileStringA("Blink", "ControllerAim",
                               g_blkDriveUI ? "1" : "0", ini);
    WritePrivateProfileStringA("Blink", "Marker", g_blkMarker ? "1" : "0", ini);
    _snprintf(v, 64, "%d", g_blkReachMode);
    WritePrivateProfileStringA("Blink", "ReachMode", v, ini);
    _snprintf(v, 64, "%.0f", g_blkReachUU);
    WritePrivateProfileStringA("Blink", "ReachUU", v, ini);
    _snprintf(v, 64, "%.0f", g_blkNearUU);
    WritePrivateProfileStringA("Blink", "NearUU", v, ini);
    _snprintf(v, 64, "%.1f", g_blkPitchNear);
    WritePrivateProfileStringA("Blink", "PitchNearDeg", v, ini);
    _snprintf(v, 64, "%.1f", g_blkPitchFar);
    WritePrivateProfileStringA("Blink", "PitchFarDeg", v, ini);
    _snprintf(v, 64, "%.0f", g_blkMarkerBackUU);
    WritePrivateProfileStringA("Blink", "MarkerPullbackUU", v, ini);
    WritePrivateProfileStringA("Blink", "AimAtSource",
                               g_blkDirAim ? "1" : "0", ini);
    WritePrivateProfileStringA("Blink", "UseAimRay",
                               g_blkUseAimRay ? "1" : "0", ini);
    WritePrivateProfileStringA("Blink", "OptVer", "3", ini);
    WritePrivateProfileStringA("Hands", "AddToAnim", g_skcAddMode ? "1" : "0", ini);
    _snprintf(v, 64, "%.1f", g_skcScaleUU);
    WritePrivateProfileStringA("Hands", "ScaleUU", v, ini);
    _snprintf(v, 64, "%.1f", g_skcMax);
    WritePrivateProfileStringA("Hands", "ClampUU", v, ini);
    _snprintf(v, 64, "%d", g_skcSpace);
    WritePrivateProfileStringA("Hands", "Space", v, ini);
    _snprintf(v, 64, "%.2f", g_skcStrength);
    WritePrivateProfileStringA("Hands", "Strength", v, ini);
    _snprintf(v, 64, "%.2f", g_skcCounterYaw);
    WritePrivateProfileStringA("Hands", "CounterHeadYaw", v, ini);
    _snprintf(v, 64, "%.2f", g_skcHandSize);
    WritePrivateProfileStringA("Hands", "HandSize", v, ini);
    _snprintf(v, 64, "%.0f", g_skcWorldScale);
    WritePrivateProfileStringA("Hands", "WorldScaleUU", v, ini);
    _snprintf(v, 64, "%.2f", g_skcRollGain);
    WritePrivateProfileStringA("Hands", "RollGain", v, ini);
    { static const char* hk[3] = { "TrimFwd", "TrimRight", "TrimUp" };
      for (int hh = 0; hh < 2; hh++)
        for (int q = 0; q < 3; q++) {
            char k[32];
            _snprintf(k, sizeof(k), "%s%s", hh ? "R" : "L", hk[q]);
            _snprintf(v, 64, "%.1f", g_skcTrim[hh][q]);
            WritePrivateProfileStringA("Hands", k, v, ini);
        } }
    WritePrivateProfileStringA("Overlay", "Level", dvr::ovl::level_name(dvr::ovl::level()), ini);
    WritePrivateProfileStringA("VRHands", "Enabled", g_hmEnable ? "1" : "0", ini);
    WritePrivateProfileStringA("VRHands", "CalibTriangle", g_hmCalib ? "1" : "0", ini);
    WritePrivateProfileStringA("VRHands", "HideGameArms", g_hmHideGame ? "1" : "0", ini);
    _snprintf(v, 64, "%.2f", g_hmScale);
    WritePrivateProfileStringA("VRHands", "Scale", v, ini);
    _snprintf(v, 64, "%d", g_hmModel[0]);
    WritePrivateProfileStringA("VRHands", "LeftModel", v, ini);
    _snprintf(v, 64, "%d", g_hmModel[1]);
    WritePrivateProfileStringA("VRHands", "RightModel", v, ini);
    WritePrivateProfileStringA("VRHands", "FollowEquipped", g_hmAuto ? "1" : "0", ini);
    WritePrivateProfileStringA("VRHands", "HideStaticParts", g_hmHideStatic ? "1" : "0", ini);
    _snprintf(v, 64, "%.0f", g_hmHideStaticUU);
    WritePrivateProfileStringA("VRHands", "HideStaticRadiusUU", v, ini);
    { static const char* mr2[3] = { "Yaw", "Pitch", "Roll" };
      static const char* mp2[3] = { "X", "Y", "Z" };
      for (int mi = 1; mi < HM_COUNT; mi++)
        for (int q = 0; q < 3; q++) {
            char k[32];
            _snprintf(k, sizeof(k), "M%d%s", mi, mr2[q]);
            _snprintf(v, 64, "%.1f", g_hmMRot[mi][q]);
            WritePrivateProfileStringA("VRHands", k, v, ini);
            _snprintf(k, sizeof(k), "M%dPos%s", mi, mp2[q]);
            _snprintf(v, 64, "%.4f", g_hmMPos[mi][q]);
            WritePrivateProfileStringA("VRHands", k, v, ini);
        } }
    { static const char* pk2[3] = { "PosX", "PosY", "PosZ" };
      static const char* rk2[3] = { "Yaw", "Pitch", "Roll" };
      for (int hh = 0; hh < 2; hh++)
        for (int q = 0; q < 3; q++) {
            char k[32];
            _snprintf(k, sizeof(k), "%s%s", hh ? "R" : "L", pk2[q]);
            _snprintf(v, 64, "%.4f", g_hmPos[hh][q]);
            WritePrivateProfileStringA("VRHands", k, v, ini);
            _snprintf(k, sizeof(k), "%s%s", hh ? "R" : "L", rk2[q]);
            _snprintf(v, 64, "%.1f", g_hmRot[hh][q]);
            WritePrivateProfileStringA("VRHands", k, v, ini);
        } }

    // 41.1: the render-resolution ask
    _snprintf(v, 64, "%u", g_resWantW);
    WritePrivateProfileStringA("Screen", "RenderWidth", v, ini);
    _snprintf(v, 64, "%u", g_resWantH);
    WritePrivateProfileStringA("Screen", "RenderHeight", v, ini);
    WritePrivateProfileStringA("Screen", "RenderFullscreen", g_resWantFull ? "1" : "0", ini);
    WritePrivateProfileStringA("Screen", "VirtualMode", g_resVirtual ? "1" : "0", ini);
    // 41.1 (session 8): the capture mode (off is live-only and is not saved)
    if (dvr::capture::mode() != dvr::capture::Mode::Off)
        WritePrivateProfileStringA("Capture", "Mode", dvr::capture::mode_name(), ini);
    // 41.1 (session 8): the device levers as they were READ this run (the
    // seam word writes the ask for the next launch; a save must not undo it)
    {
        char cur[16] = "";
        GetPrivateProfileStringA("Device", "Ex", dvr::d3d9ex::ex_wanted() ? "1" : "0", cur, sizeof(cur), ini);
        WritePrivateProfileStringA("Device", "Ex", cur, ini);
        GetPrivateProfileStringA("Device", "Managed", dvr::d3d9ex::managed_name(dvr::d3d9ex::managed_mode()), cur, sizeof(cur), ini);
        WritePrivateProfileStringA("Device", "Managed", cur, ini);
    }
    // 41.1: the stereo selection and the tickbox
    WritePrivateProfileStringA("Stereo", "Method", dvr::stereo::wanted_name(), ini);
    WritePrivateProfileStringA("VR", "DesktopEyeSource", dvr::desktop_eye::source_name(), ini);
    WritePrivateProfileStringA("VR", "ReduceDesktopPresent", dvr::desktop_eye::reduced_present() ? "1" : "0", ini);
    WritePrivateProfileStringA("VR", "DesktopMirrorOff", dvr::desktop_eye::mirror_off() ? "1" : "0", ini);
    WritePrivateProfileStringA("VR", "DesktopMirrorStrictOff", dvr::desktop_eye::strict_off() ? "1" : "0", ini);
    {
        const auto crosshair = dvr::aim::config();
        WritePrivateProfileStringA("Aim", "FireFromHand", FireAimEnabled() ? "1" : "0", ini);
        WritePrivateProfileStringA("Aim", "PropWatch", PwEnabled() ? "1" : "0", ini);
        WritePrivateProfileStringA("Aim", "ModelRay", dvr::aim::config().modelRay ? "1" : "0", ini);
        WritePrivateProfileStringA("Aim", "FollowHandTrim", dvr::aim::config().followHandTrim ? "1" : "0", ini);
        WritePrivateProfileStringA("Crosshair", "Dot", crosshair.dot ? "1" : "0", ini);
        WritePrivateProfileStringA("Crosshair", "Laser", crosshair.laser ? "1" : "0", ini);
        WritePrivateProfileStringA("Crosshair", "Hand", crosshair.hand ? "right" : "left", ini);
        WritePrivateProfileStringA("Crosshair", "ControlDot", crosshair.controlDot ? "1" : "0", ini);
        _snprintf(v, 64, "%.3f", crosshair.distanceM);
        WritePrivateProfileStringA("Crosshair", "DistanceM", v, ini);
        _snprintf(v, 64, "%.3f", crosshair.sizeDeg);
        WritePrivateProfileStringA("Crosshair", "SizeDeg", v, ini);
        const char* rgbKeys[3] = {"ColorR", "ColorG", "ColorB"};
        for (int i = 0; i < 3; ++i) { _snprintf(v, 64, "%d", crosshair.rgb[i]); WritePrivateProfileStringA("Crosshair", rgbKeys[i], v, ini); }
        _snprintf(v, 64, "%.2f", crosshair.otherXDeg); WritePrivateProfileStringA("Crosshair", "OtherItemsX", v, ini);
        _snprintf(v, 64, "%.2f", crosshair.otherYDeg); WritePrivateProfileStringA("Crosshair", "OtherItemsY", v, ini);
    }
    WritePrivateProfileStringA("Stereo", "Armed", dvr::stereo::armed() ? "1" : "0", ini);
    { char hv[16]; _snprintf(hv, sizeof(hv), "%d", dvr::stereo::hold_untagged());
      WritePrivateProfileStringA("Stereo", "HoldUntagged", hv, ini); }
    WritePrivateProfileStringA("PosTrack", "EyeClamp", g_eyeClampCfg ? "1" : "0", ini);
    // 41.1: the [Neck] lever
    WritePrivateProfileStringA("Neck", "Mode", NeckModeName(g_neckMode), ini);
    _snprintf(v, 64, "%.3f", g_neckBelowM);
    WritePrivateProfileStringA("Neck", "PivotBelowM", v, ini);
    _snprintf(v, 64, "%.3f", g_neckBehindM);
    WritePrivateProfileStringA("Neck", "PivotBehindM", v, ini);
    _snprintf(v, 64, "%.3f", g_neckCrouchBelowM);    // VR-78: formatted right before its own write
    WritePrivateProfileStringA("Neck", "UprightPitchArc", dvr::camera::upright_pitch_arc() ? "1" : "0", ini);
    WritePrivateProfileStringA("Neck", "RollArc", g_neckRollArc ? "1" : "0", ini);
    WritePrivateProfileStringA("Neck", "CrouchPivotBelowM", v, ini);
    _snprintf(v, 64, "%.3f", g_neckCrouchBehindM);
    WritePrivateProfileStringA("Neck", "CrouchPivotBehindM", v, ini);
    _snprintf(v, 64, "%.0f", g_neckStanceBlendMs);
    WritePrivateProfileStringA("Neck", "StanceBlendMs", v, ini);
    // 41.1: the [Pace] levers, so a headset run's choice survives the session
    _snprintf(v, 64, "%d", dvr::vr::pace_ahead());
    WritePrivateProfileStringA("Pace", "Ahead", v, ini);
    WritePrivateProfileStringA("Pace", "Strict", dvr::vr::pair_strict() ? "1" : "0", ini);
    _snprintf(v, 64, "%d", dvr::vr::get_pose_lag());
    WritePrivateProfileStringA("Pace", "Lag", v, ini);
    WritePrivateProfileStringA("Pace","ImageOrientation",dvr::vr::image_orientation_enabled()?"1":"0",ini);
    // Sync OFF saves as 0 whatever the target was, so a SAVE AS DEFAULTS taken
    // after an A/B that ended on `off` does not resurrect the rate next launch.
    _snprintf(v, 64, "%u", dvr::vr::pace_sync() ? dvr::vr::pace_sync_hz() : 0u);
    WritePrivateProfileStringA("Pace", "SyncHz", v, ini);

    Log("overlay: saved defaults (scale %.1f dist %.2f)", g_posScaleUU, g_screenDist);
}
