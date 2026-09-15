# UE3 performance research, 2026-09-15 (VR-125)

## Finding and limits

Low whole-machine utilization plus weak resolution scaling is compatible with
serialized CPU rendering and GPU feeding gaps. It does not prove one broken INI
or that the unused CPU cores can execute this workload automatically. The light
trace records render-thread72.43% running,26.91% blocked,0.57% ready. On12 logical
processors, one fully occupied thread represents only about8.3% of aggregate
scheduled CPU time. Do not equate low aggregate CPU with a fast critical thread.
Quarter-pixel rendering improved throughput about6%, not zero. Increasing pixel
count substantially reduced throughput. There is both a resolution-independent
floor and a higher-resolution cost; sole CPU or sole GPU ownership is unproven.

## Primary sources

- [Epic UE3 level optimization](https://docs.unrealengine.com/udk/Three/LevelOptimization.html):
  game/render thread idle distinction, driver overhead, object/light interactions,
  scene-rendering counters and decal section multiplication. Intended for UE3;
  modern UE4/5 console-variable lists are not evidence a Dishonored key exists.
- [Microsoft D3D9 profiling](https://learn.microsoft.com/en-us/windows/win32/direct3d9/accurately-profiling-direct3d-api-calls):
  batching and deferred state work can charge cost to later calls. Optimizing the
  API where elapsed time appears need not remove the underlying work.
- [NVIDIA occlusion culling](https://developer.nvidia.com/gpugems/gpugems/part-v-performance-and-practicalities/chapter-29-efficient-occlusion-culling):
  early query-result waits can destroy CPU/GPU overlap; several outstanding
  queries let other useful work proceed before results are needed.
- [Microsoft D3D9 queries](https://learn.microsoft.com/en-us/windows/win32/direct3d9/queries):
  GetData can poll without flushing or request a command-buffer flush; result
  availability and command submission are different from CPU execution cost.
- [Microsoft D3D9 optimization](https://learn.microsoft.com/en-us/windows/win32/direct3d9/performance-optimizations):
  draw/state overhead, batching and correct dynamic-buffer locking matter.
- [Microsoft Task Manager GPU accounting](https://devblogs.microsoft.com/directx/gpus-in-the-task-manager/):
  GPU engines are distinct; the summary chooses the busiest engine rather than
  averaging all engines. Inspect3D/copy/video activity and clocks for VR streaming.

## Ranked unresolved mechanisms

1. Per-eye engine synchronization, or serial dependency on the D3D9 worker.
   Viewport Draw is invoked twice, with bShouldPresent=true each time. This does
   not prove that a frame-completion wait occurs twice. It is the exact boundary
   to inspect. Attribute game-owned EVENT/OCCLUSION query GetData calls, their
   flags, result age, callers and total elapsed polling episodes per eye. Keep
   game queries distinct from the mod's image-ownership fences and diagnostics.
   Existing query CPU samples are small, but CPU samples cannot bound blocked
   wall time. If queries do not own material waiting, abandon this hypothesis.
   Do not blindly remove FLUSH or lie about completion: image corruption and
   stale-resource use are possible. A fix must preserve producer/consumer order.
2. Duplicated CPU scene preparation and expensive driver submissions. The world
   tick occurs before the intercepted viewport draw; we do not deliberately
   invoke AI/world simulation twice. Per-view visibility, draw setup, object/light
   interactions or render callbacks can still repeat. Rendering an NPC can be
   expensive independently of deciding what the NPC should do. Lower resolution
   does not reduce object or draw counts. Our wider VR view is another reason a
   flat-game comparison must match FOV. A second viewport call's0.6ms game-thread
   duration excludes the queued work on the separate render thread.
3. Visibility-history or occlusion-query interaction between the two eye views.
   Executable strings confirm the OCCLUSION query creation path and InitViews
   exist. They do not demonstrate query use, count, reuse or blocking in the hub.
   No existing query instrumentation separates game query types/eye/caller.
   No confirmed generic AllowOcclusionQueries/FinishCurrentFrame setting was
   found. Never transplant UE5 r.* recipes into this build and claim they work.
   If queries are cheap but draw counts are excessive, inspect culling behavior
   and per-eye view history; no shared-visibility shortcut without stereo safety.
4. Repeated auxiliary scene captures. Decompiled SceneCaptureComponent declares
   FrameRate, SetFrameRate, SetEnabled and skip-if-occluded controls. Presence is
   not evidence of active hub captures. Count live captures and render executions
   before changing them. A view-independent capture could potentially update
   once per pair; reflections can be view-dependent and must not be assumed safe.
5. External cap, runtime half-rate policy, clocks or memory/streaming stalls.
   Mod FpsCap=0, ForceNoVSync=1; game UseVsync=False, bSmoothFrameRate=FALSE;
   OneFrameThreadLag=True. This checks configured game/mod asks, not every driver
   profile or runtime override. Around60fps on a120Hz headset alone proves no
   half-rate lock. Need actual pacing/driver-policy evidence and effective clocks.
   PoolSize160 is a legacy texture-streaming budget, not total available VRAM;
   no evidence it causes stationary hub throughput loss. Do not inflate blindly.

## Already checked, and next decision

Native indexed draw/state work is substantial in aggregate; sampled vertex-buffer
and texture locks were tiny. Dynamic-shadow removal produced about1% variation
in prior simulator comparison, not a useful win; do not generalize to all lighting.
Nonblocking Present engaged but produced zero busy skips and no gain. Shared
capture has no per-eye CPU image readback on the installed route. HUD remains
explicitly excluded; accepted image orientation and fences remain protected.

Next investigation: establish whether game-owned D3D9 query polling or a frame
completion wait is repeated per eye, before another performance toggle. Offline
caller identification and existing trace analysis first; only if runtime counters
are required, use bounded aggregated timing with exact query ownership and one
focused run. Pair this with a retained GPU timeline if available, not an unbounded
heavy trace. If that path is cheap, proceed to per-eye scene/draw workload and
culling rather than another synchronization guess. No promised speedup.

Research only: no DLL/INI changes, build, installation, game launch, new ticket,
recorder or playtest. Exact307 remains installed. Earlier experiment source is
preserved on this branch, default off and rejected for promotion.
