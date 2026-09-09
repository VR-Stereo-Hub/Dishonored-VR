// game/dishonored/aim_ray.cpp - THE aim ray (VR-57, 41.2). Included by
// src/mod/dishonoredvr.cpp (unity build).
//
// ================== ONE RAY ==================
//
// This project's own rule, paid for in the BioShock trilogy mod:
//
//     Anything that claims to point where shots go derives from the IDENTICAL
//     ray. The crosshair, the projectile and any assist read the same source.
//
// A crosshair computed from a second, parallel derivation is how two things
// silently disagree, and the disagreement is invisible until a player misses a
// shot they were told they would hit. So this file exists to make that
// structurally true rather than true by review.
//
// ---- WHAT IS SHARED, AND WHAT HONESTLY IS NOT ----
//
// The composition is `MaimDirFromView(viewYaw, viewPitch, rel, out)`: the
// hand's right/up/forward components relative to the head, resolved into a view
// basis. Every consumer calls THAT function - there is one algebra and it lives
// in one place.
//
// The two consumers pass a DIFFERENT view basis, and that difference is real,
// not an oversight:
//
//   * The projectile steering (motion_aim.cpp) passes the SHOT'S OWN spawn
//     forward. That is the ground truth of where the game aimed, read off the
//     projectile the engine actually created. It cannot exist before the shot.
//   * The crosshair passes the view the camera is CURRENTLY on
//     (g_viewYawRad/g_viewPitchRad, the values head_track just wrote). It has to,
//     because there is no projectile yet.
//
// So the crosshair is a PREDICTION of the ray a shot fired now would take. That
// is a falsifiable claim, and the acceptance test for VR-57 is exactly it: fire,
// and the bolt lands under the dot. If the two view bases disagree - if the
// engine builds the shot from a view we are not tracking - the shot will land
// off the dot and this line is where to look. The status word prints both.
//
// ---- ORIGIN ----
//
// The composition yields a DIRECTION only. The origin is the game camera's own
// world position (`g_camObj + 0x80`, the same read blink.cpp uses), because that
// is the point the direction is relative to. Using the hand's position instead
// would be a second, unverified derivation of where shots start.

// The ray, in GAME world space. `why` names the first thing that refused, so a
// missing dot is a sentence rather than a silence.
struct AimRay {
    float origin[3];
    float dir[3];
    float viewYaw, viewPitch;   // the basis it was composed against
    bool  ok;
    const char* why;
};

static AimRay      g_arLast;
static uint32_t    g_arOk = 0, g_arFail = 0;

// THE accessor. Every consumer that wants to know where the weapon points calls
// this and nothing else.
static bool AimRayGet(AimRay* out)
{
    AimRay r;
    memset(&r, 0, sizeof(r));
    r.why = "not attempted";

    // FIRST, the reason that hides every other reason. [Mode] GamepadOnly=1
    // turns off hands, hand mesh, motion aim, motion melee, motion crouch and
    // controller Blink in one switch - so the ray has no hand to point along
    // and no consumer to feed. Naming it here is the whole point: the first
    // version of this instrument was called from the motion-aim tick, which
    // GamepadOnly stops, so it printed NOTHING and a run looked like a build
    // that had not loaded.
    if (g_gamepadOnly) {
        r.why = "[Mode] GamepadOnly=1 - the controllers are a plain gamepad, so motion aim is off and "
                "there is no hand ray to derive. Set GamepadOnly=0 (VR-40) before expecting a crosshair.";
        g_arLast = r; ++g_arFail;
        if (out) *out = r;
        return false;
    }

    float rel[3];
    if (!MaimHandRel(rel)) {
        r.why = "no controller pose (MaimHandRel refused)";
        g_arLast = r; ++g_arFail;
        if (out) *out = r;
        return false;
    }

    // The view the camera is on right now. head_track writes these as it drives
    // the camera, so they are the basis the engine is rendering against - not a
    // reconstruction.
    r.viewYaw = g_viewYawRad;
    r.viewPitch = g_viewPitchRad;

    // THE ONE COMPOSITION. Shared verbatim with the projectile steering.
    MaimDirFromView(r.viewYaw, r.viewPitch, rel, r.dir);

    const float dl = sqrtf(r.dir[0]*r.dir[0] + r.dir[1]*r.dir[1] + r.dir[2]*r.dir[2]);
    if (!(dl > 0.5f)) {
        r.why = "the composed direction is degenerate";
        g_arLast = r; ++g_arFail;
        if (out) *out = r;
        return false;
    }

    // The origin: the game camera's own world position. Never the hand's - that
    // would be a second derivation of where a shot starts.
    if (!g_camObj || !RangeReadable((void*)(g_camObj + 0x80), 12)) {
        r.why = "the camera object has no readable world position yet";
        g_arLast = r; ++g_arFail;
        if (out) *out = r;
        return false;
    }
    const float* cp = (const float*)(g_camObj + 0x80);
    r.origin[0] = cp[0]; r.origin[1] = cp[1]; r.origin[2] = cp[2];

    r.ok = true;
    r.why = "ok";
    g_arLast = r; ++g_arOk;
    if (out) *out = r;
    return true;
}

// Evaluated from the PRESENT path, which always runs. It deliberately does NOT
// live on the motion-aim tick: that tick is switched off by [Mode] GamepadOnly,
// and an instrument that can be silently disabled is worse than none - the
// first version printed nothing at all and the run read as a build that had not
// loaded. Whatever is wrong, this now says so once every 5 s.
static void AimRayTick(void)
{
    static double s_next = 0.0;
    const double nowMs = MaimNowMs();
    if (nowMs < s_next) return;
    s_next = nowMs + 5000.0;
    AimRayStatus();
}

// `aimray status` - what the ray is, and which parts of it are measured rather
// than assumed. It prints the REFUSAL reason too, because a ray that is not
// available is the normal state in a menu and must not read as a fault.
static void AimRayStatus(void)
{
    AimRay r;
    const bool ok = AimRayGet(&r);
    Log("aimray: %s | origin (%.0f %.0f %.0f) uu dir (%+.3f %+.3f %+.3f) | composed against the view the "
        "camera is CURRENTLY on (yaw %+.1f pitch %+.1f deg) | %u ok / %u refused since load | %s",
        ok ? "OK" : "UNAVAILABLE",
        r.origin[0], r.origin[1], r.origin[2], r.dir[0], r.dir[1], r.dir[2],
        r.viewYaw * 57.29578f, r.viewPitch * 57.29578f, g_arOk, g_arFail, r.why);
    // The doctrine line once per run, not every 5 s - it never changes.
    static bool s_saidOnce = false;
    if (s_saidOnce) return;
    s_saidOnce = true;
    Log("aimray: ONE RAY - the projectile steering and this share MaimDirFromView verbatim. They differ "
        "only in the view basis: the projectile passes its OWN spawn forward (the ground truth of where "
        "the game aimed, which cannot exist before the shot), this passes the camera's current view. So a "
        "crosshair built on this is a PREDICTION, and the test is that the bolt lands under it. If it does "
        "not, the engine builds the shot from a view we are not tracking, and that is the thing to fix - "
        "NOT by giving the crosshair its own second derivation.");
}
