// game/dishonored/hands/grip_pivot.cpp - VR-33 step 1: the hand turns about
// the GRIP, not about the bone's origin. See mod/state/56 for why.
//
// This file owns the pivot vector and its knob. The subtraction itself is one
// call from skelcontrol.cpp's translation path, because that is where the
// world placement is already computed and it is the only place that can apply
// it without a second write racing the first.

// The measured seed, handed over by the mesh split once it has classified.
// The wrist ring IS the cut plane, so the distance from it to the centroid of
// the hand-class triangles along the limb axis is how far past the wrist the
// hand's own mass sits - which is the forward component of the grip, measured
// on this asset rather than assumed.
static void GripSeedFromMesh(void)
{
    if (!g_msReady) return;
    for (int sd = 1; sd <= 2; sd++) {
        const int h = g_msHandBone[sd];
        if (h < 0) continue;
        const float* ax = g_msAxis[sd];

        // The hand class's centroid, along the axis. Triangles, not vertices,
        // so a dense knuckle does not outvote a sparse palm.
        double cen = 0.0; int n = 0;
        for (int t = 0; t < g_msTris; t++) {
            if (g_msTriSide[t] != sd) continue;
            float c[3];
            MsTriCentroid(t, c);
            const float d = c[0] * ax[0] + c[1] * ax[1] + c[2] * ax[2];
            // Only the hand side of the plane. `cut` is rebuilt here rather
            // than shared, because MsClassify's copy is a local.
            cen += d; n++;
        }
        if (n < 8) continue;

        // The cut plane's own position along the axis: the hand bone's
        // centroid plus the tester's offset, which is exactly what
        // MsClassify uses to classify.
        float cutPos = 0.0f;
        for (int a = 0; a < 3; a++) cutPos += g_msBoneCen[h][a] * ax[a];
        cutPos += g_msCutRel[sd];

        // The hand's mass centre, past the ring.
        double handCen = 0.0; int hn = 0;
        for (int t = 0; t < g_msTris; t++) {
            if (g_msTriSide[t] != sd) continue;
            float c[3];
            MsTriCentroid(t, c);
            const float d = c[0] * ax[0] + c[1] * ax[1] + c[2] * ax[2];
            if (d < cutPos) continue;
            handCen += d; hn++;
        }
        if (hn < 8) continue;

        const int side = sd - 1;
        g_gripSeed[side] = (float)(handCen / hn) - cutPos;
        g_gripSeedOk[side] = 1;
        if (!g_gripSet[side]) g_gripP[side][0] = g_gripSeed[side];
        Log("grip: side %s seed - the hand's centroid sits %.2f uu past the "
            "wrist ring along the limb axis (%d hand triangle(s) of %d on this "
            "side). That is the FORWARD component of the grip pivot and it is "
            "measured on this asset. Right and up seed at 0, because nothing "
            "in the mesh says where inside the palm a controller's grip sits - "
            "that is the knob's job. [Hands] GripPivotF/R/U pin it.",
            sd == 1 ? "A" : "B", g_gripSeed[side], hn, n);
    }
}


// The offset to SUBTRACT from the world position the translation drive would
// otherwise write, so the GRIP lands on the controller instead of the wrist.
// `hand` is 0 or 1; `m` is the controller's pose matrix. Returns false when
// the compensation is off or unusable, and the caller then writes exactly
// what it wrote before - the fail-soft is the old behaviour, not a wrong one.
static bool GripOffsetWorld(int hand, float (*m)[4], float* out)
{
    out[0] = out[1] = out[2] = 0.0f;
    if (!g_gripOn) return false;
    if (hand < 0 || hand > 1 || !m) { g_gripSkipped++; return false; }
    const float* P = g_gripP[hand];
    if (P[0] == 0.0f && P[1] == 0.0f && P[2] == 0.0f) return false;

    // The controller's own basis, the same convention the rotation drive
    // reads out of this matrix: -Z forward, +X right, +Y up.
    const float f[3] = { -m[0][2], -m[1][2], -m[2][2] };
    const float r[3] = {  m[0][0],  m[1][0],  m[2][0] };
    const float u[3] = {  m[0][1],  m[1][1],  m[2][1] };

    for (int k = 0; k < 3; k++)
        out[k] = f[k] * P[0] + r[k] * P[1] + u[k] * P[2];

    // A basis that is not a basis produces a hand somewhere else entirely, so
    // it refuses rather than writing a NaN into a bone.
    if (!(out[0] == out[0]) || !(out[1] == out[1]) || !(out[2] == out[2])) {
        g_gripSkipped++;
        out[0] = out[1] = out[2] = 0.0f;
        return false;
    }
    g_gripApplied++;
    return true;
}


static void GripTick(void)
{
    if (g_gripToggleReq) {
        g_gripToggleReq = 0;
        g_gripOn = !g_gripOn;
        Log("grip: >>> pivot compensation %s <<< - %s. This is the A/B: turn "
            "and roll the controller in place and watch whether the hand stays "
            "put or swings.",
            g_gripOn ? "ON" : "OFF",
            g_gripOn ? "the hand turns about the grip point"
                     : "the hand turns about the bone's origin at the wrist, "
                       "which is the pre-VR-33 behaviour");
    }
    if (g_gripSideReq) {
        g_gripSideReq = 0;
        g_gripSide = (g_gripSide + 1) % 3;
        Log("grip: the knob now moves %s",
            g_gripSide == 0 ? "BOTH hands" :
            g_gripSide == 1 ? "the LEFT hand only" : "the RIGHT hand only");
    }
    if (g_gripAxisReq) {
        g_gripAxisReq = 0;
        g_gripAxis = (g_gripAxis + 1) % 3;
        Log("grip: the knob now moves %s, %.2f uu per press",
            kGripAxisName[g_gripAxis], kGripStep);
    }
    if (g_gripNudgeReq) {
        const int d = g_gripNudgeReq; g_gripNudgeReq = 0;
        for (int s = 0; s < 2; s++) {
            if (g_gripSide && (g_gripSide - 1) != s) continue;
            g_gripP[s][g_gripAxis] += (d > 0 ? kGripStep : -kGripStep);
            if (g_gripP[s][g_gripAxis] >  40.0f) g_gripP[s][g_gripAxis] =  40.0f;
            if (g_gripP[s][g_gripAxis] < -40.0f) g_gripP[s][g_gripAxis] = -40.0f;
            g_gripSet[s] = 1;
        }
        Log("grip: %s %s - LEFT (F %.2f, R %.2f, U %.2f)  RIGHT (F %.2f, R "
            "%.2f, U %.2f) uu. Those six numbers are exactly what [Hands] "
            "GripPivotF/R/U (L and R) take, so a placement worth keeping "
            "becomes the default without another session.",
            d > 0 ? "further out" : "further back", kGripAxisName[g_gripAxis],
            g_gripP[0][0], g_gripP[0][1], g_gripP[0][2],
            g_gripP[1][0], g_gripP[1][1], g_gripP[1][2]);
    }
    if (g_gripOn) {
        const double now = MaimNowMs();
        if (now >= g_gripNextReport) {
            g_gripNextReport = now + 15000.0;
            Log("grip: beat - %u write(s) carried the pivot offset, %u could "
                "not. BOTH at zero means the hand translation drive is not "
                "running at all, which is normal in a menu and is NOT a "
                "statement about the pivot; a rising skipped count with a live "
                "drive means the controller pose was unusable. Pivot L (F %.2f "
                "R %.2f U %.2f) R (F %.2f R %.2f U %.2f), seed L %.2f%s R "
                "%.2f%s.",
                g_gripApplied, g_gripSkipped,
                g_gripP[0][0], g_gripP[0][1], g_gripP[0][2],
                g_gripP[1][0], g_gripP[1][1], g_gripP[1][2],
                g_gripSeed[0], g_gripSeedOk[0] ? "" : " (not derived yet)",
                g_gripSeed[1], g_gripSeedOk[1] ? "" : " (not derived yet)");
            g_gripApplied = g_gripSkipped = 0;
        }
    }
}


static bool GripCommand(const char* args)
{
    if (!args) return false;
    while (*args == ' ') args++;
    if (!strncmp(args, "on", 2))       { g_gripOn = true;  }
    else if (!strncmp(args, "off", 3)) { g_gripOn = false; }
    else if (!strncmp(args, "seed", 4)) {
        for (int s = 0; s < 2; s++) {
            g_gripSet[s] = 0;
            if (g_gripSeedOk[s]) g_gripP[s][0] = g_gripSeed[s];
            g_gripP[s][1] = g_gripP[s][2] = 0.0f;
        }
        Log("grip: back to the derived seed");
    } else if (!strncmp(args, "set", 3)) {
        float f = 0, r = 0, u = 0;
        if (sscanf(args + 3, "%f %f %f", &f, &r, &u) == 3) {
            for (int s = 0; s < 2; s++) {
                if (g_gripSide && (g_gripSide - 1) != s) continue;
                g_gripP[s][0] = f; g_gripP[s][1] = r; g_gripP[s][2] = u;
                g_gripSet[s] = 1;
            }
        }
    }
    Log("grip: status - %s, knob on %s / %s. Pivot L (F %.2f R %.2f U %.2f) "
        "R (F %.2f R %.2f U %.2f) uu. Seed L %.2f R %.2f. HOME cycles the "
        "axis, INSERT and DELETE nudge it, END toggles the whole thing, "
        "PAUSE picks which hand.",
        g_gripOn ? "ON" : "OFF", kGripAxisName[g_gripAxis],
        g_gripSide == 0 ? "both hands" : g_gripSide == 1 ? "left" : "right",
        g_gripP[0][0], g_gripP[0][1], g_gripP[0][2],
        g_gripP[1][0], g_gripP[1][1], g_gripP[1][2],
        g_gripSeed[0], g_gripSeed[1]);
    return true;
}
