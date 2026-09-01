// game/dishonored/hands/graft.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).

// 35.8: same liveness rule for the graft donors. They are archetype-template
// objects (persistent packages), but "should survive" is exactly the phrase
// that preceded the 32.6 crash - so they get the identical GObjects check
// before every fast-lane write.
static inline bool GraftDonorAlive(int u)
{
    if (u < 0 || u >= 3) return false;
    uint8_t* o = g_graftDonor[u];
    if (!o || ((uintptr_t)o & 3)) return false;
    uint32_t ix = g_graftDonorIdx[u];
    if (!ix) return false;
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (!objs || ((uintptr_t)objs & 3) || ix >= num) return false;
    if ((uint8_t*)objs[ix] != o) return false;
    if (g_graftDonorCls[u] && *(void**)(o + kClassOff) != g_graftDonorCls[u])
        return false;
    return true;
}

// 35.8: rig died mid-drive (SkcAlive tripped in the fast lane). The hosts are
// gone - never touch them - but the donors are TEMPLATES whose bytes leak into
// freshly instanced rigs, so restore them the moment death is seen instead of
// waiting for the next probe to finish. Runs once per death; cheap.
static inline void GraftEmergencyRestore(void)
{
    if (!g_graftOn) return;
    for (int u = 0; u < 3; u++) {
        if (!g_graftHost[u]) continue;       // slot was never engaged
        uint8_t* d = g_graftDonor[u];
        if (d && GraftDonorAlive(u))
            memcpy(d, g_graftSave[u], 0x100);
        g_graftHost[u] = NULL;
        g_graftHand[u] = -1;
    }
    g_graftOn = false;
}


// ============================================================================
// 30.90 - SKELCONTROL PROBE.
//
// Everything so far fought the engine for its own output. SkelControls are the
// opposite: UE3's built-in facility for procedural bone override, living IN the
// AnimTree with BoneTranslation / BoneRotation / ControlStrength. They are
// evaluated as part of the animation pass, so they are never restamped - the
// engine treats them as an INPUT it respects rather than a result it recomputes.
// That is the whole reason this is worth a look after everything else failed.
//
// Two things decide it, and both are answered by one walk of GObjects:
//   1. do any SkelControl instances exist at all in this game?
//   2. is any of them a LIMB control (IK) - because a SkelControlLimb lets us
//      set an effector position and have the elbow solve itself, which is
//      exactly "put the hand where the controller is" with the arm following.
// SkelControlSingleBone also matters: it carries BoneTranslationSpace, and if
// WORLD space is available the head coupling cancels for free.
//
// Read-only. One pass, on demand, off the game thread's hot path - the same
// discipline as the other GObjects probes (§10: never per-frame).
// ============================================================================
// ==== 33.8: THE OFFSET AUDIT (one launch, then this question closes) =====
// Rotation writes land at 10k/3s and the rendered arm ignores them, while
// the F7 pin - same path, zero values - visibly snaps it. Those two facts
// contradict unless something small is wrong, and the rotation offsets/bit
// masks were GUESSED in the 30.9x era while translation's were verified by
// visible effect. So: read the truth from the engine's own reflection (the
// method that found the crouch cylinder) and print it beside our constants.
// Translation fields are included as canaries - they WORK, so reflection
// must agree with them or the audit itself is wrong. Pure diagnostic; no
// behavior changes anywhere in this build.
// ==== 35.7: THE GRAFT EXPERIMENT ============================================
// Census (35.6) found the donors: the player rig's ARCHETYPE template tree
// (Ply_Player_at) holds three pristine SkelControlSingleBone objects -
// persistent (asset-owned), the audited class, structurally identical to
// our live controls. Grafting one onto a live hand control's EMPTY
// NextControl (+0xac, measured) makes it a second, tail-position control
// on the hand bone - the Mirror's Edge topology from Dishonored's own
// parts. The experiment: overlay checkbox applies a FIXED +45 deg yaw
// through the donors. Arm visibly rotates = channel alive, wire the full
// drive next. Arm ignores it = the eater is below the control layer and
// the ME route dies here with one clean measurement.
// KNOWN QUIRK (accepted for the test): the donors are instancing TEMPLATES;
// while the test rotation is set, a checkpoint reload would stamp new hand
// controls with these values as defaults. Disable restores everything.
static void GraftTestSet(bool on)
{
    if (on == g_graftOn) return;
    if (on) {
        if (!g_graftOffNext || !g_graftDonorN || !g_skcPlayerN) {
            Log("graft: cannot engage (offsets/donors/controls missing)");
            return;
        }
        // 35.8: one donor per HAND control (skip the camera look-at - donors
        // are scarce and the drive has nothing to send it). Donor starts at
        // rotation ZERO; the fast lane delivers the real controller rotation.
        int used = 0;
        for (int i = 0; i < g_skcPlayerN && used < g_graftDonorN && i < 8; i++) {
            int hand = g_skcHandOf[i];
            if (hand < 0) continue;                    // camera / undriven
            uint8_t* host  = g_skcPlayer[i];
            uint8_t* donor = g_graftDonor[used];
            if (!host || !donor || !RangeReadable(host, 0x100) ||
                !GraftDonorAlive(used) || !RangeReadable(donor, 0x100)) continue;
            // refuse if the host already chains somewhere (never clobber)
            if (*(uint8_t**)(host + g_graftOffNext) != NULL) {
                Log("graft: host %d NextControl not empty - skipped", i);
                continue;
            }
            memcpy(g_graftSave[used], donor, 0x100);
            // donor: end of chain, full strength, instant, rotation-only,
            // neutral until the mailbox speaks
            *(uint8_t**)(donor + g_graftOffNext) = NULL;
            *(float*)(donor + g_graftOffStr)  = 1.0f;
            *(float*)(donor + g_graftOffSTgt) = 1.0f;
            *(float*)(donor + g_graftOffBTG)  = 0.0f;
            uint32_t* bools = (uint32_t*)(donor + kSkcBools);
            *bools &= ~(kSkcApplyTrans | kSkcAddTrans);
            *bools |= kSkcApplyRot;
            *bools &= ~kSkcAddRot;
            *(uint8_t*)(donor + kSkcRSpace) = (uint8_t)g_graftRotSpace;
            int32_t* rot = (int32_t*)(donor + kSkcRot);
            rot[0] = 0; rot[1] = 0; rot[2] = 0;
            // the graft itself
            g_graftHand[used] = hand;
            g_graftHost[used] = host;
            *(uint8_t**)(host + g_graftOffNext) = donor;
            Log("graft: ENGAGED donor %p onto host %d (%p) hand %d - live "
                "rotation drive, space %d", (void*)donor, i, (void*)host,
                hand, g_graftRotSpace);
            used++;
        }
        g_graftOn = used > 0;
        if (!g_graftOn) Log("graft: nothing engaged");
    } else {
        for (int u = 0; u < g_graftDonorN && u < 3; u++) {
            uint8_t* host  = g_graftHost[u];
            uint8_t* donor = g_graftDonor[u];
            if (host && RangeReadable(host, 0x100) &&
                *(uint8_t**)(host + g_graftOffNext) == donor)
                *(uint8_t**)(host + g_graftOffNext) = NULL;
            if (host && donor && RangeReadable(donor, 0x100))
                memcpy(donor, g_graftSave[u], 0x100);
            g_graftHost[u] = NULL;
            g_graftHand[u] = -1;
        }
        g_graftOn = false;
        Log("graft: disengaged - hosts unlinked, donors byte-restored");
    }
}


// 35.8: "hold your hands forward, press this" - captures the controllers'
// current orientation as the rotation drive's neutral. Factored out of the
// END handler so the overlay button can reach it (the user plays in VR;
// keyboard keys are voice-command territory).
static void SkcRotZeroNeutral(const char* why)
{
    if (!g_injSnapOk) { Log("skc/rot: cannot zero neutral - no HMD snap"); return; }
    int zeroed = 0;
    for (int hz = 0; hz < 2; hz++) {
        int dz = g_ctrlIdx[hz];
        if (dz < 0 || dz >= 16 || !g_devPoseOk[dz]) continue;
        float (*hZ)[4] = g_devPose[dz];
        float fZ[3]  = { -hZ[0][2], -hZ[1][2], -hZ[2][2] };
        float dZ[3]  = { -hZ[0][1], -hZ[1][1], -hZ[2][1] };
        float aZ = g_maimPitchOff * 3.14159265f / 180.0f;
        float rz2[3] = { fZ[0]*cosf(aZ) + dZ[0]*sinf(aZ),
                         fZ[1]*cosf(aZ) + dZ[1]*sinf(aZ),
                         fZ[2]*cosf(aZ) + dZ[2]*sinf(aZ) };
        if (V3Norm(rz2) < 0.5f) continue;
        float ry2 = atan2f(rz2[0], -rz2[2]);
        float rpc = rz2[1] < -1.f ? -1.f : (rz2[1] > 1.f ? 1.f : rz2[1]);
        // 36.0: neutral is captured against the SNAP - the same reference
        // the drive subtracts each frame (it is republished per frame as a
        // matched pair, so it is effectively live).
        float yC2 = ry2 - g_injHmdYawSnap;
        while (yC2 >  3.14159265f) yC2 -= 6.2831853f;
        while (yC2 < -3.14159265f) yC2 += 6.2831853f;
        g_skcRotNeu[hz][0] = yC2;
        g_skcRotNeu[hz][1] = asinf(rpc) - g_injHmdPitchSnap;
        g_skcRotNeuAbs[hz][0] = ry2;            // 36.4: raw, for absolute aim
        g_skcRotNeuAbs[hz][1] = asinf(rpc);
        g_skcRotNeuHmd[hz][0] = g_injHmdYawSnap;   // 36.5: head at zero, for
        g_skcRotNeuHmd[hz][1] = g_injHmdPitchSnap; // the head-follow comp
        g_skcRotNeuOk[hz] = true;
        zeroed++;
    }
    Log("skc/rot: neutral zeroed for %d hand(s) (%s)", zeroed, why);
}


// ==== 35.5: DONOR-GRAFT DISCOVERY (rotation reopened via ME-VR) =============
// The Mirror's Edge VR mod proved same-engine hand rotation by grafting a
// DONOR SkelControl onto the tail of the hand bone's NextControl chain and
// driving BoneRotation there - our 33.8 dead end wrote to the EXISTING
// controls, whose rotation output the tree eats. This is step (a): log-only.
// It resolves the chain/strength offsets from reflection, walks each cached
// hand control's NextControl chain (what runs AFTER us - where a graft must
// land), and lists every SkelControl living in the same tree instance as
// donor candidates. Nothing is written.
static void GraftDiscover()
{
    uint32_t offNext = FindPropOffset("SkelControlBase", "NextControl");
    uint32_t offStr  = FindPropOffset("SkelControlBase", "ControlStrength");
    uint32_t offSTgt = FindPropOffset("SkelControlBase", "StrengthTarget");
    uint32_t offBTG  = FindPropOffset("SkelControlBase", "BlendTimeToGo");
    // ME notes: Engine.u declares ControlStrength, BlendInTime, BlendOutTime,
    // StrengthTarget, BlendTimeToGo consecutively - provisional fallbacks.
    if (offStr && !offSTgt) offSTgt = offStr + 12;
    if (offStr && !offBTG)  offBTG  = offStr + 16;
    g_graftOffNext = offNext; g_graftOffStr = offStr;    // 35.7: for the graft
    g_graftOffSTgt = offSTgt; g_graftOffBTG = offBTG;
    Log("graft: offsets NextControl=+0x%x ControlStrength=+0x%x "
        "StrengthTarget=+0x%x BlendTimeToGo=+0x%x%s",
        offNext, offStr, offSTgt, offBTG,
        (offNext && offStr) ? "" : "  <-- MISSING, graft blocked");
    if (!offNext) return;

    // the chain DOWNSTREAM of each cached hand control
    for (int i = 0; i < g_skcPlayerN && i < 8; i++) {
        uint8_t* n = g_skcPlayer[i];
        for (int hop = 0; hop < 12 && n; hop++) {
            if (((uintptr_t)n & 3) || !RangeReadable(n, 0x100)) {
                Log("graft: hand-ctl %d hop %d: unreadable %p - chain ends",
                    i, hop, (void*)n);
                break;
            }
            const char* nm = RealName(*(uint32_t*)(n + kNameOff));
            const char* cn = ObjClassName(n);
            float st  = offStr  && RangeReadable(n + offStr, 4)
                      ? *(float*)(n + offStr) : -99.0f;
            float stt = offSTgt && RangeReadable(n + offSTgt, 4)
                      ? *(float*)(n + offSTgt) : -99.0f;
            Log("graft: hand-ctl %d hop %d: %p %s (%s) strength=%.2f "
                "target=%.2f", i, hop, (void*)n, nm ? nm : "?",
                cn ? cn : "?", st, stt);
            if (!RangeReadable(n + offNext, 4)) break;
            uint8_t* nx = *(uint8_t**)(n + offNext);
            if (nx == n) { Log("graft: hand-ctl %d self-loop", i); break; }
            n = nx;
        }
    }

    // donor candidates: every SkelControl sharing the hand controls' Outer
    // (same AnimTree instance), with name/class/strength - the ME mod used
    // Hips/Swing controls; Dishonored's tree names its own.
    if (!g_skcPlayerN || !RangeReadable((void*)kGObjHdr, 12)) return;
    uint8_t* wantOuter = NULL;
    if (RangeReadable(g_skcPlayer[0] + kOuterOff, 4))
        wantOuter = *(uint8_t**)(g_skcPlayer[0] + kOuterOff);
    if (!wantOuter) { Log("graft: hand control has no Outer - no census"); return; }
    void**   objs = *(void***)kGObjHdr;
    uint32_t num  = *(uint32_t*)(kGObjHdr + 4);
    if (((uintptr_t)objs & 3) || num < 2000 || num > 4000000) return;
    int logged = 0;
    Log("graft: ==== SkelControls in the same tree (Outer %p) ====",
        (void*)wantOuter);
    for (uint32_t i = 0; i < num && logged < 48; i++) {
        if ((i & 1023) == 0) {
            uint32_t left = num - i; if (left > 1024) left = 1024;
            if (!RangeReadable(objs + i, left * sizeof(void*))) break;
        }
        uint8_t* o = (uint8_t*)objs[i];
        if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x100)) continue;
        if (!RangeReadable(o + kOuterOff, 4) ||
            *(uint8_t**)(o + kOuterOff) != wantOuter) continue;
        const char* cn = ObjClassName(o);
        if (!cn || strncmp(cn, "SkelControl", 11)) continue;
        const char* nm = RealName(*(uint32_t*)(o + kNameOff));
        float st = offStr && RangeReadable(o + offStr, 4)
                 ? *(float*)(o + offStr) : -99.0f;
        bool isOurs = false;
        for (int q = 0; q < g_skcPlayerN; q++)
            if (g_skcPlayer[q] == o) isOurs = true;
        Log("graft:   %p %-32s %-26s strength=%.2f%s", (void*)o,
            nm ? nm : "?", cn, st, isOurs ? "  [= our hand ctl]" : "");
        logged++;
    }
    Log("graft: ==== census done (%d) - pick donors from this list ====",
        logged);

    // 35.6: the same-tree census came back with ONLY our three controls -
    // Dishonored's player FP tree has no spare controls to borrow, unlike
    // Mirror's Edge's TdPawn. So go GLOBAL: every SkelControl object in
    // GObjects, grouped by class, with each one's Outer (which tree owns
    // it). NPC trees in the loaded level should be full of candidates; a
    // donor only has to be LINKABLE into our chain, not born in our tree.
    // (Liveness caveat for later: a donor Outer'd to an NPC dies with the
    // NPC - the graft build must prefer persistent owners and keep the
    // 32.6-style liveness checks.)
    {
        struct ClsCount { const char* cls; int n; };
        char clsNames[12][40]; int clsCounts[12]; int clsN = 0;
        int shown = 0, total = 0;
        Log("graft: ==== GLOBAL SkelControl census ====");
        for (uint32_t i = 0; i < num; i++) {
            if ((i & 1023) == 0) {
                uint32_t left = num - i; if (left > 1024) left = 1024;
                if (!RangeReadable(objs + i, left * sizeof(void*))) break;
            }
            uint8_t* o = (uint8_t*)objs[i];
            if (!o || ((uintptr_t)o & 3) || !RangeReadable(o, 0x100)) continue;
            const char* cn = ObjClassName(o);
            if (!cn || strncmp(cn, "SkelControl", 11)) continue;
            total++;
            int ci = -1;
            for (int k = 0; k < clsN; k++)
                if (!strncmp(clsNames[k], cn, 39)) { ci = k; break; }
            if (ci < 0 && clsN < 12) {
                strncpy(clsNames[clsN], cn, 39); clsNames[clsN][39] = 0;
                clsCounts[clsN] = 0; ci = clsN++;
            }
            if (ci >= 0) clsCounts[ci]++;
            if (!strcmp(cn, "SkelControlSingleBone")) {
                uint8_t* ou = RangeReadable(o + kOuterOff, 4)
                            ? *(uint8_t**)(o + kOuterOff) : NULL;
                const char* on = (ou && !((uintptr_t)ou & 3) &&
                                  RangeReadable(ou, kNameOff + 4))
                               ? RealName(*(uint32_t*)(ou + kNameOff)) : NULL;
                // 35.7: the archetype template's controls are the donors -
                // persistent, our audited class, nobody's live rig.
                if (on && !strcmp(on, "Ply_Player_at") && g_graftDonorN < 3) {
                    g_graftDonorIdx[g_graftDonorN] = i;      // 35.8: liveness
                    g_graftDonorCls[g_graftDonorN] =
                        RangeReadable(o + kClassOff, 4) ? *(void**)(o + kClassOff)
                                                        : NULL;
                    g_graftDonor[g_graftDonorN++] = o;
                    Log("graft: DONOR %d = %p (Ply_Player_at template)",
                        g_graftDonorN, (void*)o);
                }
                if (shown < 40) {
                    const char* oc = (ou && !((uintptr_t)ou & 3)) ?
                                     ObjClassName(ou) : NULL;
                    float st = offStr && RangeReadable(o + offStr, 4)
                             ? *(float*)(o + offStr) : -99.0f;
                    Log("graft: G %p outer=%s(%s) strength=%.2f%s", (void*)o,
                        on ? on : "?", oc ? oc : "?", st,
                        ou == wantOuter ? "  [player tree]" : "");
                    shown++;
                }
            }
        }
        for (int k = 0; k < clsN; k++)
            Log("graft: GLOBAL class %-34s x%d", clsNames[k], clsCounts[k]);
        Log("graft: ==== GLOBAL census done (%d SkelControls total) ====",
            total);
    }
}
