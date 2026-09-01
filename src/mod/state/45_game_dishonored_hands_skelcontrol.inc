// State chunk 45 for game/dishonored/hands/skelcontrol - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// Build 30.6: start weapon tracking by itself. The script hook already
// self-installs at frame 600; this closes the other half of the F6+HOME
// ritual. Wait until we are really in-game (pawn latched) with BOTH
// controllers tracked, let that hold for a few seconds so the neutral pose
// is captured from hands being held naturally rather than mid-loading, then
// do exactly what HOME does. Fires once per launch; a manual HOME first
// claims the choice and the auto-start stands down. END remains the fix-it
// button: it recaptures neutral AND re-runs the full calibration.
// (the g_autoHand* globals live up with the config block)

// Build 30.8: keyboard-free diagnostics. Whatever [Debug] Probe= names runs
// exactly once, ~2 s after weapon tracking is up, on the game thread inside
// the fault boundary. Claude edits the ini remotely; the user just launches
// the game and quits. No more hunting for keys that half the keyboards on
// earth don't have.
// ============================================================================
// 30.40 - CAMERA SEAM RECON (read-only; bioshock-vr DR-I2 playbook)
// Find the native camera-view dispatch (GetPlayerViewPoint / UpdateViewTarget
// family): enumerate the live player controller's and camera's class vtables,
// shortlist entries whose code has the two-out-param shape (ret 8 within the
// function body), install counting stubs on the shortlist, and report which
// slot fires per frame with FVector/FRotator-shaped arguments. The stubs
// count, sample the arg contents under RangeReadable, and tail-jump to the
// original - no engine state is written. Armed by [CamSeam] Recon (default 1
// in this build). This seam is the injection point for true 6DOF + roll +
// per-frame FOV (render-at-headset-FOV kills the fill-stretch morph).
// ============================================================================
static bool   g_csDone   = false;
