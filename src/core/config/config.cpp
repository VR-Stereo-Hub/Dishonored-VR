// core/config/config.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


static void WriteDefaultIni(const char* ini)
{
    FILE* f = fopen(ini, "w");
    if (!f) return;
    fprintf(f,
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
        "; HeightOffsetM shifts the eyes vertically (metres, negative = lower). -0.090 is the\n"
        "; value the 2026-09-03/04 headset runs were judged at (it was the code default and\n"
        "; unwritten before); F10 View tunes it per person, SAVE AS DEFAULTS writes it back.\n"
        "HeightOffsetM=-0.090\n"
        "[Stereo]\n"
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
        "[Camera]\n"
        "; EyeField= the camera field the per-eye offset is written to. 0x330 was measured\n"
        "; 2026-09-02 with `camera eyetest` (HONOURED 119/120; docs/dishonored/ENGINE_NOTES.md,\n"
        "; the per-eye camera seam); none disables the write.\n"
        "EyeField=0x330\n"
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
        "Lag=1\n"
        "SyncHz=0\n"
        "[Perf]\n"
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
        "; The mono screen: a head-locked quad DistanceMeters away and WidthMeters\n"
        "; wide. Per-eye rendering will replace it (docs/ROADMAP.md).\n"
        "DistanceMeters=1.75\n"
        "; HeadLocked=1 keeps the screen in front of your eyes (turning your head turns the\n"
        "; game camera); 0 leaves it standing in the room where you recentered.\n"
        "HeadLocked=1\n"
        "WidthMeters=2.4\n"
        "; RenderWidth/RenderHeight/RenderFullscreen (41.1): the render-resolution picker's\n"
        "; ask, 0 = the game's own size. It takes effect at the NEXT LAUNCH: the size goes\n"
        "; into the game's own ini (Documents\\My Games\\Dishonored, [SystemSettings] and the four\n"
        "; AppCompat buckets, backed up once as .pre-dvr) because the engine's own setres does\n"
        "; nothing on this build (measured). A fullscreen size must be a display mode the\n"
        "; adapter lists (`res modes`) - or VirtualMode=1 makes the proxy advertise it and\n"
        "; create the fullscreen device windowed with the backbuffer kept, the route to the\n"
        "; eye's own near-square size. `res <W>x<H>[f|w]` asks live; F10 Display has the picker\n"
        "; and the verdict (`res: HONOURED` reads the capture, never the requested number).\n"
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
        "GamepadOnly=1\n"
        "[VR]\n"
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
        "[Paths]\n"
        "; DataDir= where the harness files go (command.txt, status.json, dumps, the\n"
        "; shim manifest). Empty = %%LOCALAPPDATA%%\\DishonoredVR. Set it to a folder the\n"
        "; game and the tools both see for real (docs/VERIFICATION.md gotcha 14).\n"
        "DataDir=\n"
        "[Controllers]\n"
        "; Stage 6.4: Index controllers = virtual Xbox-360 pad via SteamVR's\n"
        "; ACTION input system (rebindable in SteamVR > Controller Bindings).\n"
        "; Left stick = move. Right stick X = turn (Y = head only in gameplay).\n"
        "; R trigger = right hand (sword)  L trigger = left hand (power/gun)\n"
        "; R A = JUMP + menu confirm       R B = stealth + menu back\n"
        "; L A = interact/use              L B = pause menu\n"
        "; L trackpad press = power wheel  R trackpad press = zoom\n"
        "; R grip = choke/attack           L grip = adrenaline (Y)\n"
        "; L stick click = sneak (LS)      R stick click = RECENTER lean\n"
        "; Head-mouse auto-pauses while a menu (visible cursor) is open.\n"
        "Enabled=1\n"
        "Deadzone=0.12\n"
        "Haptics=1\n"
        "[PosTrack]\n"
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
        "Scale=98\n"
        "MaxMeters=0.80\n"
        "FlipX=0\n"
        "[Neck]\n"
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
        "Enabled=1\n"
        "Hand=left\n"
        "PitchOffsetDeg=40\n"
        "WindowMs=1200\n"
        "MaxDistUU=900\n"
        "FlipRight=0\n"
        "FlipUp=0\n"
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
        "; Build 30.19: swing the RIGHT controller to attack with the sword.\n"
        "; A real swing above SwingSpeed (m/s) presses the attack input for\n"
        "; HoldMs; CooldownMs paces combos (one swing = one strike). Your\n"
        "; trigger still attacks as before. Too sensitive? raise SwingSpeed.\n"
        "; Swings not registering? lower it.\n"
        "Enabled=1\n"
        "SwingSpeed=1.8\n"
        "HoldMs=220\n"
        "CooldownMs=300\n"
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
        "PivotMix=1.0\n"
        "; Unreal units per metre of hand travel. 0 = follow [PosTrack] Scale so\n"
        "; hands and world stay the same size.\n"
        "ScaleUU=0\n"
        "MaxOffsetUU=120\n"
        "; 0 = raw pose. Raise toward 0.9 only if the hands look jittery.\n"
        "SmoothAlpha=0.0\n"
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
        "RotScale=1.0\n"
        "; The pivot assumes the rig's origin is at your eye. If rotation swings\n"
        "; the arms from somewhere below you, slide it back (unreal units).\n"
        "PivotUp=0\n"
        "; If the weapon and the hand pull APART, the weapon's component axes\n"
        "; differ from the arms'. These degrees rotate the weapon's copy of the\n"
        "; transform to match. Tune them live in the F10 overlay.\n"
        "WpnYaw=0\n"
        "WpnPitch=0\n"
        "WpnRoll=0\n"
        "[HeadInject]\n"
        "; (legacy, unused)\n"
        "FlipYaw=1\n"
        "FlipPitch=1\n"
        "FlipRoll=1\n", kConfigVersion);
    fclose(f);
}


static void LoadConfig()
{
    char ini[MAX_PATH];
    _snprintf(ini, MAX_PATH, "%s\\dishonored_vr.ini", g_dir);
    Log("config: LoadConfig begin");

    // create if missing, OR refresh if it predates this build's tuned defaults
    bool missing = GetFileAttributesA(ini) == INVALID_FILE_ATTRIBUTES;
    int ver = (int)IniFloat(ini, "Meta", "Version", 0);
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
        GetPrivateProfileStringA("Paths", "DataDir", "", dd, sizeof(dd), ini);
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
        dvr::stereo::set_hold_untagged(GetPrivateProfileIntA("Stereo", "HoldUntagged", 3, ini));
    }

    {   // [Camera] EyeField: where the per-eye offset is written (measured by
        // `camera eyetest`; empty until then - the seam says so once)
        char ef[16] = "";
        GetPrivateProfileStringA("Camera", "EyeField", "0x330", ef, sizeof(ef), ini);
        dvr::camera::set_eye_field(ef);
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
    g_posScaleUU = IniFloat(ini, "PosTrack", "Scale", 98.0f);
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
    }
    g_padEnabled  = IniFloat(ini, "Controllers", "Enabled", 1) != 0.0f;
    g_padHaptics  = IniFloat(ini, "Controllers", "Haptics", 1) != 0.0f;
    g_padDeadzone = IniFloat(ini, "Controllers", "Deadzone", 0.12f);
    if (g_padDeadzone < 0.0f)  g_padDeadzone = 0.0f;
    if (g_padDeadzone > 0.6f)  g_padDeadzone = 0.6f;
    g_fireTraceEnabled = IniFloat(ini, "Debug", "FireTrace", 1) != 0.0f;
    g_maimEnabled  = IniFloat(ini, "MotionAim", "Enabled", 1) != 0.0f;
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
    }
    {   // 41.1 (session 8): the tick budget's levers, both default on
        const bool inst = IniFloat(ini, "Perf", "Instruments", 1) != 0.0f;
        const bool gpu = IniFloat(ini, "Perf", "GpuQueries", 1) != 0.0f;
        if (!inst) dvr::perf::set_enabled(false);
        if (!gpu) dvr::perf::set_gpu_enabled(false);
        const bool fid = IniFloat(ini, "Perf", "FrameId", 1) != 0.0f;   // 41.1 (session 9): the frame-identity trace
        dvr::frameid::set_enabled(fid);
        dvr::frameid::set_every((uint32_t)IniFloat(ini, "Perf", "FrameIdEvery", 8));
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
        g_hmEnable   = IniFloat(ini, "VRHands", "Enabled", 0) != 0.0f;
        g_hmHideGame = IniFloat(ini, "VRHands", "HideGameArms", 1) != 0.0f;
        g_hmScale    = IniFloat(ini, "VRHands", "Scale", 1.0f);
        if (g_hmScale < 0.2f) g_hmScale = 0.2f;
        if (g_hmScale > 4.0f) g_hmScale = 4.0f;
        g_hmModel[0] = (int)IniFloat(ini, "VRHands", "LeftModel", 2);
        g_hmModel[1] = (int)IniFloat(ini, "VRHands", "RightModel", 1);
        g_hmAuto = IniFloat(ini, "VRHands", "FollowEquipped", 1) != 0.0f;
        g_hmObjScale = IniFloat(ini, "VRHands", "ObjScale", 0.01f);
        g_hmHotReload = IniFloat(ini, "VRHands", "HotReload", 1) != 0.0f;
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
        g_rtdEnable = IniFloat(ini, "HandRender", "Enabled",   0) != 0.0f;
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
    g_blkReachMode  = (int)IniFloat(ini, "Blink", "ReachMode", 2);
    if (g_blkReachMode < 0 || g_blkReachMode > 2) g_blkReachMode = 2;
    g_blkReachUU    = IniFloat(ini, "Blink", "ReachUU", 0.0f);
    g_blkNearUU     = IniFloat(ini, "Blink", "NearUU", 150.0f);
    g_blkPitchNear  = IniFloat(ini, "Blink", "PitchNearDeg", -55.0f);
    g_blkPitchFar   = IniFloat(ini, "Blink", "PitchFarDeg",   -5.0f);
    g_blkMarkerBackUU = IniFloat(ini, "Blink", "MarkerPullbackUU", 60.0f);
    g_blkDirAim       = IniFloat(ini, "Blink", "AimAtSource", 1) != 0.0f;
    g_blkDriveUI  = g_blkAimOnCfg;
    g_aimAllPowers = IniFloat(ini, "Blink", "AimAllPowers", 1) != 0.0f;  // 38.52
    Log("config: Blink controller aim %s (native detour at 0xbf5e4f)",
        g_blkAimOnCfg ? "ON" : "off");
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
    g_crouchToggle    = IniFloat(ini, "Hands", "CrouchToggle", 1) != 0.0f;
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
    g_slideAssist    = IniFloat(ini, "Input", "SlideAssist", 1) != 0.0f; // 38.22
    g_eyeClampCfg    = IniFloat(ini, "PosTrack", "EyeClamp", 1) != 0.0f; // 38.24
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
    g_heightOffsetM = IniFloat(ini, "Tracking", "HeightOffsetM", -0.09f);
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
    g_ovlDev = IniFloat(ini, "Overlay", "DevTools", 0) != 0.0f;
    g_ovlPtrEnable = IniFloat(ini, "Overlay", "ControllerPointer", 0) != 0.0f;
    g_ovlPtrHand = IniFloat(ini, "Overlay", "PointerHand", 1) != 0.0f ? 1 : 0;
    g_ovlPtrGain = IniFloat(ini, "Overlay", "PointerSpeed", 2.2f);
    g_autoHand      = IniFloat(ini, "HandTracking", "AutoStart", 1) != 0.0f;
    // 32.96: was 4 s on top of discovery time - the user asked why motion
    // controls take so long after a load. 1.5 s is enough for the rig to be
    // real; everything the auto-start needs is already gated on the pawn and
    // both controllers being live.
    g_autoHandDelay = IniFloat(ini, "HandTracking", "DelaySec", 1.5f);
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
            const int lag = GetPrivateProfileIntA("Pace", "Lag", 1, ini);
            int syncHz = GetPrivateProfileIntA("Pace", "SyncHz", 0, ini);
            dvr::vr::set_pace_ahead(ahead);
            dvr::vr::set_pair_strict(strict != 0);
            dvr::vr::set_pose_lag(lag);
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
    // controllers stay a plain gamepad until the render is settled.
    //
    // What stays ON deliberately: head tracking and its rotation writes,
    // positional head tracking, the FOV lever, and the virtual gamepad. This
    // is NOT the XR SAFE bisector above - that one also stops the head.
    //
    // The author's process rules say motion crouch and hands "must never stop
    // working". This does not retire them: it is one key, it logs loudly, and
    // the code is untouched. Set GamepadOnly=0 to get them all back.
    g_gamepadOnly = IniFloat(ini, "Mode", "GamepadOnly", 1) != 0.0f;
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

static void OverlaySaveDefaults()
{
    char ini[MAX_PATH];
    _snprintf(ini, MAX_PATH, "%s\\dishonored_vr.ini", g_dir);
    char v[64];
    _snprintf(v, 64, "%.1f", g_posScaleUU);
    WritePrivateProfileStringA("PosTrack", "Scale", v, ini);
    _snprintf(v, 64, "%.2f", g_screenDist);
    WritePrivateProfileStringA("Screen", "DistanceMeters", v, ini);
    _snprintf(v, 64, "%.3f", g_heightOffsetM);
    WritePrivateProfileStringA("Tracking", "HeightOffsetM", v, ini);
    _snprintf(v, 64, "%.0f", (float)g_fovLever);      // 30.51: persist the lever
    WritePrivateProfileStringA("Screen", "FovLever", v, ini);
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
    WritePrivateProfileStringA("Overlay", "DevTools", g_ovlDev ? "1" : "0", ini);
    WritePrivateProfileStringA("VRHands", "Enabled", g_hmEnable ? "1" : "0", ini);
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
    WritePrivateProfileStringA("Stereo", "Armed", dvr::stereo::armed() ? "1" : "0", ini);
    { char hv[16]; _snprintf(hv, sizeof(hv), "%d", dvr::stereo::hold_untagged());
      WritePrivateProfileStringA("Stereo", "HoldUntagged", hv, ini); }
    // 41.1: the [Neck] lever
    WritePrivateProfileStringA("Neck", "Mode", NeckModeName(g_neckMode), ini);
    _snprintf(v, 64, "%.3f", g_neckBelowM);
    WritePrivateProfileStringA("Neck", "PivotBelowM", v, ini);
    _snprintf(v, 64, "%.3f", g_neckBehindM);
    WritePrivateProfileStringA("Neck", "PivotBehindM", v, ini);
    // 41.1: the [Pace] levers, so a headset run's choice survives the session
    _snprintf(v, 64, "%d", dvr::vr::pace_ahead());
    WritePrivateProfileStringA("Pace", "Ahead", v, ini);
    WritePrivateProfileStringA("Pace", "Strict", dvr::vr::pair_strict() ? "1" : "0", ini);
    _snprintf(v, 64, "%d", dvr::vr::get_pose_lag());
    WritePrivateProfileStringA("Pace", "Lag", v, ini);
    // Sync OFF saves as 0 whatever the target was, so a SAVE AS DEFAULTS taken
    // after an A/B that ended on `off` does not resurrect the rate next launch.
    _snprintf(v, 64, "%u", dvr::vr::pace_sync() ? dvr::vr::pace_sync_hz() : 0u);
    WritePrivateProfileStringA("Pace", "SyncHz", v, ini);

    Log("overlay: saved defaults (scale %.1f dist %.2f)", g_posScaleUU, g_screenDist);
}
