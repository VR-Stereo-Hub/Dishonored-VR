# Next session: DLAA headset check, then DLSS Super Resolution

Read CLAUDE.md, only the newest three sections of docs/STATUS.md, and this brief. The
maintained record is
[PERFORMANCE: DLAA through an x64 NGX helper](PERFORMANCE.md#2026-09-26-dlaa-through-an-x64-ngx-helper-phase-1---built-host-and-simulator-verified).
The FSR section before it still holds the render/output split (gate 2) and jitter (gate 3)
plans; both apply unchanged to DLSS SR.

## Workspace and boundaries

- Worktree `C:\dev\Dishonored-VR\build\worktrees\fsr` (directory name kept), branch
  `claude/dlss-dlaa`, stacked on the unmerged `claude/motion-vectors`. No merges authorized.
- Order: DLAA (done, phase 1) -> DLSS SR -> FSR 3.1. No frame generation.
- Credit commits to BioVRDev. No trailers. No Linear ticket exists (issue limit).
- NGX SDK: `tools\fetch-ngx.ps1` (pinned v310.7.0, gitignored). `tools\build.ps1` builds the
  helper when the SDK is present; `tools\dlss-host-test.ps1` is the 10-check host test.

## State

Installed build 19:33 with the helper in `<game>\dvr_dlss\`. INI unchanged from before the
session (DLAA off). Simulator: DLAA 69/s per eye, stereo 90 -> 69/s. No headset verdict yet.

## Headset question (one launch)

F10 > Advanced > Display > Clarity and anti-aliasing > "NVIDIA DLAA (experimental)", toggled
live with the panel closed between looks. Question: does DLAA visibly reduce edge shimmer and
crawl compared with off, without objectionable smearing on moving NPCs or the hands?
Expected in the log: `dlss: DLAA ready on NVIDIA ...`, then `dlss: DLAA N/s L N/s R` lines
with `fallback 0/s`. A frame-rate drop is expected (DLAA at full eye size is not free).

## Then

1. DLSS SR: separate render and output sizes (FSR gate 2 plan), pass ow > w to
   `Client::build` (the helper already creates an SR feature for it), pick the quality mode
   from the ratio, measure frame time against native with the same output size.
2. Projection jitter (FSR gate 3) - the biggest remaining quality lever for DLSS and FSR.
3. FSR 3.1 in-process: build the Win32 static libs with the three patches in PERFORMANCE,
   a 32-bit D3D12 device sharing the proxy's textures, reuse `dlss_gpu` guides.
