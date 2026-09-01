// State chunk 48 for legacy/cam_seam - included by src/mod/dishonoredvr.cpp.
// Original order is preserved across chunks; do not reorder.


// ============================================================================
// 30.45 - POV WIGGLE TEST. The census confirmed three POV blocks on the live
// DishonoredPlayerCamera: +0x330, +0x350, +0x374 (loc tracks walking, rot
// tracks view incl. roll, fov is live 90/95). This answers WHICH one the
// renderer obeys: for each block in turn, hold a +4000-unit (~22 deg) yaw
// offset for 3 seconds (rewritten every game tick on top of the engine's own
// value, since it recomputes per frame), with 2 s rest between. The block
// whose window visibly turns the view is the injection point. The v1-era
// lesson ("renderer ignores camera writes") was about writing ONCE - a
// per-tick reassert after the engine's update is a different mechanism.
// Armed by [CamSeam] Wiggle (default ON this build); requires weapon-tracking
// auto-start (proves gameplay, not menu).
// ============================================================================
static const uint32_t kPovOffs[3] = {0x330, 0x350, 0x374};
static int    g_wigPhase = -1;      // -1 idle, 0..5 = block0,rest,block1,rest,block2,done
static double g_wigNext  = 0.0;

// ============================================================================
// 30.46 - FOV WIGGLE. The POV-block yaw wiggle proved those three blocks are
// per-frame OUTPUTS (all three windows fired, zero visible effect). But the
// static block at +0x120 reads fov=100.0 - exactly the user's configured
// m_fDefaultFOV - so +0x138 is likely the camera's DefaultFOV INPUT field,
// read by the per-frame update. If writing it zooms the view, we own the
// rendered FOV live: render at the HEADSET's FOV, present 1:1, morph gone.
// Three windows: 70 (zoom in), 130 (wide), restore 100. Rotation and position
// injection are already solved (view hook + lean); FOV is the missing third.
// ============================================================================
static int    g_fwigPhase = -1;
static double g_fwigNext  = 0.0;
static float  g_fwigSaved = 0.0f;

// 30.49: the fovhunt (user zoomed during scan 3, rendered FOV 95 -> 32.5)
// sorted the camera's FOV fields cleanly:
//   track the rendered value: +0x53c, +0x540  (current/desired interpolator?)
//   stay at base 95:          +0x564, +0x254  (base FOV fields)
//   static echoes:            +0x138 (=config 100; writes proven dead)
// One 3-second window per candidate, forcing 70 (a clear zoom-in) with the
// engine's own value saved/restored. The window that visibly zooms = the
// live input.
static const uint32_t kFovCands[4] = {0x53c, 0x540, 0x564, 0x254};

// ============================================================================
// 30.48 - LIVE FOV FIELD HUNT. +0x138 was a config echo (writes: no effect).
// In UE3 the FOV pipeline runs through the CONTROLLER (FOVAngle/DefaultFOV)
// before the camera composes its per-frame POV. We hold the live controller
// and pawn; scan all three objects for floats in the plausible FOV range and
// log them against the RENDERED FOV (the fork's live projection export). The
// field that TRACKS the rendered value - especially while the user sprints or
// aims, which modulates FOV - is the true input. On demand from the overlay;
// 6 scans over 12 s.
// ============================================================================
static int    g_fhRuns = 0;
static double g_fhNext = 0.0;
