# Claude takeover: rain and possession stereo

## Start here
- Work in C:/dev/Dishonored-VR on codex/misc-fixes. Keep existing branch.
- Read CLAUDE.md, AGENTS.md, only top3 STATUS sections, then this file.
- Current user instructions override older identity rules: credit GitHub BioVRDev
  on new commits (configured name/email already correct). No trailers, no names in
  branch names. No subagents, game/simulator launches, PR/merge without request.
- User wants Claude to implement the remaining fixes now. Codex performed research
  and preserved evidence; neither rain nor possession is fixed yet.
- Build/install yourself, archive both logs, verify DLL/banner, compare whole INI,
  preserve CRLF. One clearly defined question per launch and outcome meanings.
- Engine writers need current IsLiveObject and retained identity revalidation.

## Installed/accepted baseline
- Installed build452, source1d029a1eb; see build/playtest-candidates/installed.json.
  DLL SHA537812eb74f594ba3b700284f782132af14620de31509dbd0d3b55098cd091b4.
- Head tilt after hit reported fixed in recent runs. Preserve shared stale-wheel
  release guard (ui_surface.cpp), image-owned orientation and stereo synchronization.
- Successful streaming run: PID14744, about9m46s sampled, normal exit0.
  Peak private2056.9MiB, virtual2819.3MiB; prior failed snapshot private3164.5MiB,
  virtual3930.7MiB. Different play sequences, not controlled FPS comparison.
- Texture optimization is CONFIG ONLY:13 SystemSettings groups in each of user
  DishonoredEngine.ini and installed DefaultEngine.ini changed NumStreamedMips0
  to-1. Texture pack/max4096/pool160/headset resolution/F10 unchanged.
  Backups/full diffs/manifest: build/playtest-candidates/texture-streaming452.
  Preserve this accepted test configuration. No automatic clean-install mod policy
  exists yet; decide release handling after further validation.
- Full dump proved prior pause freeze was virtual-memory fatal then cleanup hang.
 2596 CPU texture twins;2D payload estimate1754.64MiB. Not proof of a leak.
  ENGINE_NOTES top memory investigation has detailed derivation.
- Health vignette frame routing made it invisible: NOT fixed. User wanted a visible
  near-eye peripheral effect. Leave as separate unresolved item.

## Evidence already preserved
- Latest successful run DLL/banner verified; both logs/currentINI archived in:
  build/playtest-candidates/claude-handoff452/support-20260918-080956-790.zip.
- D:/dvr-data/support-watch/20260918-074759-783/memory.csv and support ZIP.
  ProcDump full dump external-Memory-14744-20260918-075019.dmp is NORMAL EXIT,
  not another crash. One-shot watcher finished; NO active watcher.
- Earlier failed full dump D:/dvr-data/dumps/pause-freeze452-27044.dmp, plus x86 mini.
  build/diagnostics/inspect452.py and texture_objects452.py are local dump helpers.
  The x64 dump has WOW64 host contexts; use x86 mini for native stacks.
- Exact symbols build/symbol-archive/<DLL hash>. Never substitute new PDB.
- Other source work remains local. Do not rewrite previous commit authors.
- Existing crash ticket VR-133; verify ticket before reusing in code/commits.
  New feature tickets must be found/created first; no fabricated ticket numbers.

## Priority1: possession stereo
User says possession plays well but remains mono. Read FLICKER_REFERENCE before
changing stereo. Determine the actual refusing gate, do not force global stereo.
Decompiled declarations (ignored local tools/uscript/dishonored/DishonoredGame):
- DishonoredActivePowerComponent_Possess.uc:
  m_PossessionStage enum DPPS_Inactive0, Targeting1, PrePossess2,
  PossessionActive3, Cooldown4; m_pPossessedPawn/m_pPossessionProxyPawn.
- DisCamera_Possess.uc: m_PossessCameraState Initial0, ZoomingToPossessee1,
  Possessing2. Camera.m_pPossess_Influence is the direct camera-owned route.
- DisPossessablePawn.uc: m_pPossessingController, m_pPossessingPlayerPawn,
  m_pPossessee. DisPossessionProxyPawn extends it.
- StatePlayerMasterPrePossess/StatePlayerMasterPossess[_Base] exist but are NOT
  sufficient alone: anim_state.cpp around320 discards non-PlayerPawn before FSM
  reads. Correct safety guard; do not read player FSM offsets on another class.
- Latest log has no master=StatePlayerMasterPossess samples. Brief cinematic
  intervals2787406..2789718 and2829250..2830500 are candidates, not established
  possession times. Trace style/camera/pawn and gate reason across these intervals.
- scene_draw.cpp around260: UiSurfaceOwnsPresentation, DvrSceneVerdict /
  StereoStateEnabled can refuse reentry; inspect stereo_state.cpp and camera
  write ownership too. Sustained mono vs same-image stereo still needs evidence.
Use reflected live possession stage/target identity to narrowly authorize proper
controlled-possession camera/stereo, preserving transition/loading/menu guards.
No code change or candidate exists for this yet.

## Priority2: rain
User sees a pane in front of eyes. Prefer near-eye effect; if that cannot be made
comfortable, explicitly authorized disabling rain. Do not suppress all particles.
- Camera native m_pRainBoxEmitter (last traces Emitter,40 drops),
  m_RainBoxExtent, m_NumRainDrops, m_RainDirection, m_fRainSpawnKillRate,
  impact distance/count fields. Existing effects/owners trace in cinematic_trace.cpp.
- DisSeqAct_SetRainEmitter.uc configures drops/impacts/delay.
- DisParticleModuleRainDrops.uc uses distribution parameters MaxParticles,
  SpawnKillRate; DisParticleModuleRainImpacts separate.
- Engine ParticleSystemComponent native SetFloatParameter / activation/visibility
  APIs may provide a validated control; derive native entry points if needed.
  Do not merely write HiddenGame without render-proxy propagation.
- This is not proven Scaleform HUD routing. Moving generic vignette row did not
  solve health effect, and must not be assumed to target native rain.
- First read relevant decompiled declarations and existing engine notes, then
  implement narrow control with proper liveness. No rain patch yet.

## Keep scope controlled
Do not reopen performance experiments or animation pose broadening. Mantle-only
handoff accepted; broader generic action handoff caused severe regressions and
was reverted. Preserve wheel entry options/closing crop and controller fixes.
Update STATUS, FLICKER_REFERENCE and ENGINE_NOTES with actual results.
If collecting a new run, rearm tools/watch-crashes.ps1 with signed x86 procdump.exe
and previously authorized memory capture. User launches only.
