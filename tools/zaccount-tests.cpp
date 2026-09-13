// Host tests for the VR-78 accounting probe (src/game/dishonored/z_account.cpp),
// compiled against the production file by tools/zaccount-host.ps1.
//
// Every case builds SYNTHETIC records with a known answer and checks the probe
// reads that answer - including the unwelcome ones. A probe that cannot print
// PITCH_RESIDUAL, CLOSURE or a reject reason when those are the truth is not an
// instrument, so each of those is provoked on purpose.
#include "game/dishonored/z_account.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

using namespace dvr::zacct;

static std::vector<std::string> g_lines;
void ZacctHostLine(const char* line) { g_lines.push_back(line); }

static int g_fail = 0, g_checks = 0;
static void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) { ++g_fail; printf("FAIL: %s\n", what); }
}

static std::string joined() {
    std::string s;
    for (auto& l : g_lines) { s += l; s += "\n"; }
    return s;
}
static bool has(const char* needle) { return joined().find(needle) != std::string::npos; }
// the first "<key>=<n>" in any REPORT summary line
static long count_of(const char* key) {
    long total = 0;
    for (auto& l : g_lines) {
        if (l.find("REPORT") == std::string::npos) continue;
        std::string k = std::string(" ") + key + "=";
        size_t p = l.find(k);
        if (p != std::string::npos) total += strtol(l.c_str() + p + k.size(), nullptr, 10);
    }
    return total;
}

struct Sim {
    // the scene
    float pawn[3] = {1000.0f, 2000.0f, 500.0f};
    float cyl = 87.5f;
    float neutralU = 79.5f, neutralF = 10.0f;
    float engBelow = 32.1f, engBehind = 6.2f;      // the ENGINE's pivot, uu
    float cfgBelow = 0.321f, cfgBehind = 0.062f;   // what [Neck] believes, m
    float humanBelow = 32.1f, humanBehind = 6.2f;  // the player's real neck, uu
    float ceilRel = 0.0f;                          // 0 = no clamp, else ceiling above pawn
    float c5SignUsed = -1.0f;                      // the sign the fake renderer uses
    double ms = 0.0;
    uint32_t serial = 1;
    bool staleSerial = false, wrongEye = false, badTagPos = false, torn = false, moving = false;
    Write last;                                    // the writer's memory (persisted base)
    bool lastOk = false;

    static void arc(float below, float behind, float deg, float& up, float& fwd) {
        const float th = deg * 0.0174533f;
        up = below * (cosf(th) - 1.0f) + behind * sinf(th);
        fwd = behind * (cosf(th) - 1.0f) - below * sinf(th);
    }

    // One tick: the clamp, the pass-1 write + tag, pass 2 + tag, two presents.
    void tick(float deg) {
        ms += 11.1;
        if (moving) pawn[0] += 0.5f;
        float eu, ef, hu, hf;
        arc(engBelow, engBehind, deg, eu, ef);
        arc(humanBelow, humanBehind, deg, hu, hf);
        // the engine rewrites the field fresh every tick
        const float engine[3] = {pawn[0] + neutralF + ef, pawn[1], pawn[2] + neutralU + eu};
        Clamp c;
        c.ms = ms; c.ran = true; c.cyl = cyl; c.cylAgeMs = 10.0;
        memcpy(c.pawn, pawn, sizeof(c.pawn));
        c.fieldOff[1] = 0x330; c.readable[1] = true; c.pre[1] = engine[2];
        float fieldZ = engine[2];
        const float ceil = pawn[2] + ceilRel;
        if (ceilRel > 0.0f && fieldZ > ceil) fieldZ = ceil;
        c.post[1] = fieldZ;
        c.ceilRaw = c.ceilEased = ceil;
        note_clamp(c);

        Head h;
        h.ms = ms - 2.0; h.ok = true; h.posTrack = true; h.projection = true; h.neckMode = 2;
        h.raw[1] = hu; h.raw[2] = hf;
        const float nu = cfgBelow * 100.0f * (cosf(deg * 0.0174533f) - 1.0f) + cfgBehind * 100.0f * sinf(deg * 0.0174533f);
        const float nf = cfgBehind * 100.0f * (cosf(deg * 0.0174533f) - 1.0f) - cfgBelow * 100.0f * sinf(deg * 0.0174533f);
        h.neck[1] = -nu; h.neck[2] = -nf;
        for (int i = 0; i < 3; ++i) h.pos[i] = h.raw[i] + h.neck[i];
        h.pitchDeg = deg; h.neckBelowM = cfgBelow; h.neckBehindM = cfgBehind; h.scale = 100.0f;
        publish_head(h);

        for (int pass = 0; pass < 2; ++pass) {
            const int eye = pass ? 1 : -1;
            Write w;
            w.ms = ms; w.cam = (uint8_t*)0x1000; w.fieldOff = 0x330; w.sign = 1.0f; w.c5Sign = -1.0f;
            w.eye = eye; w.secondPass = pass == 1; w.wrote = true;
            w.projection = true; w.laneCamera = true; w.posLive = true; w.basisOk = true;
            w.clampOk = clamp_latest(&w.clamp);
            w.headOk = head_snapshot(&w.head);
            w.torn = torn;
            w.camPitchDeg = deg;
            w.heading[0] = 1.0f; w.heading[1] = 0.0f;
            // pass 2 finds our pass-1 write in the field: the base is RECOVERED
            w.persisted = pass == 1;
            w.base[0] = engine[0]; w.base[1] = engine[1]; w.base[2] = fieldZ;
            w.eyeW[1] = eye * 3.2f;
            w.posW[0] = h.pos[2]; w.posW[2] = h.pos[1];
            const float cand = w.base[2] + w.eyeW[2] + w.posW[2];
            w.capOn = ceilRel > 0.0f; w.candZ = cand;
            w.capDelta = (ceilRel > 0.0f && cand > ceil) ? ceil - cand : 0.0f;
            for (int i = 0; i < 3; ++i) w.written[i] = w.base[i] + w.eyeW[i] + w.posW[i];
            w.written[2] += w.capDelta;
            note_write(w);
            float tagPos[3] = {w.written[0] * w.c5Sign, w.written[1] * w.c5Sign, w.written[2] * w.c5Sign};
            if (badTagPos) tagPos[0] += 5.0f;
            const uint32_t id = pin_for_tag(tagPos);
            const float c5[3] = {w.written[0] * c5SignUsed, w.written[1] * c5SignUsed, w.written[2] * c5SignUsed};
            if (!staleSerial) ++serial;
            const int finalEye = wrongEye ? -eye : eye;
            on_present(finalEye, finalEye, true, id, true, c5, serial, ms);
        }
    }
    void hold(float deg, int ticks) { for (int i = 0; i < ticks; ++i) tick(deg); }
    void sweep() { hold(0.0f, 130); hold(-30.0f, 130); hold(30.0f, 130); }
    // a pitch ramp so the pivot fit has variance
    void ramp() { for (int i = -35; i <= 35; ++i) tick((float)i); }
};

static void start() { g_lines.clear(); set_enabled(false, "test"); set_enabled(true, "test"); }

int main() {
    // A: standing, the neck cancels the engine's own arc exactly, the head's own
    // arc rides on top. The camera follows the head: no residual, and the fresh
    // bases solve the configured pivot.
    {
        start();
        Sim s; s.ramp(); s.sweep();
        flush("test A");
        check(has("NO_MEASURED_CAMERA_RESIDUAL"), "A: a clean standing sweep reads NO_MEASURED_CAMERA_RESIDUAL");
        check(!has("PITCH_RESIDUAL"), "A: no PITCH_RESIDUAL on a clean sweep");
        check(has("PIVOT_MATCHES: "), "A: the fresh bases solve the configured pivot");
        check(has("below 0.321 m behind 0.062 m"), "A: the solved pivot is 0.321/0.062");
        check(has("PERSISTENT_BASE"), "A: pass 2's recovered base is flagged, not hidden");
        check(!has("CLIPPED"), "A: nothing clipped without a ceiling");
    }
    // B: crouched, the clamp welds the base to the ceiling and the final cap eats
    // the neck's upward correction, while the real head dips. The view stays put
    // while the head drops: the reported fault, and it must be SEEN.
    {
        start();
        Sim s; s.cyl = 65.0f; s.neutralU = 95.0f; s.ceilRel = 57.0f;
        s.ramp(); s.sweep();
        flush("test B");
        check(has("CROUCHED"), "B: the episode is CROUCHED");
        check(has("PITCH_RESIDUAL"), "B: the clamp-shaped fault reads PITCH_RESIDUAL");
        check(has("CLIPPED"), "B: and CLIPPED");
        check(has("NO_ENGINE_SAMPLES"), "B: no unclipped engine eye exists to fit");
    }
    // C: the pairing chose the other eye: never joined.
    {
        start();
        Sim s; s.wrongEye = true; s.sweep();
        flush("test C");
        check(count_of("eye") > 0, "C: a wrong-eye present is rejected as eye");
        check(has("INCOMPLETE"), "C: and nothing is measured");
    }
    // D: the c5 did not advance between presents.
    {
        start();
        Sim s; s.staleSerial = true; s.sweep();
        flush("test D");
        check(count_of("staleC5") > 0, "D: a repeated c5 serial is rejected as staleC5");
    }
    // E: an id that was overwritten before its present arrived.
    {
        start();
        Sim s; s.hold(0.0f, 2);
        note_write(Write());
        const uint32_t old = pin_for_tag(nullptr);
        for (int i = 0; i < 40; ++i) pin_for_tag(nullptr);
        const float c5[3] = {0, 0, 0};
        on_present(-1, -1, true, old, true, c5, 999999, 1.0);
        on_present(-1, -1, true, 0, true, c5, 1000000, 1.0);
        on_present(-1, +1, true, old + 40, true, c5, 1000001, 1.0);
        s.sweep();   // opens a stance so the counts are reported
        flush("test E");
        check(count_of("expired") > 0, "E: an overwritten id is rejected as expired");
        check(count_of("missing") > 0, "E: a tag with no id is rejected as missing");
        check(count_of("override") > 0, "E: a pairing override is rejected as override");
    }
    // F: the tag carries a position that is not this write.
    {
        start();
        Sim s; s.badTagPos = true; s.sweep();
        flush("test F");
        check(count_of("tagPos") > 0, "F: a tag position mismatch is rejected as tagPos");
    }
    // G: the renderer's c5 sign is not the field's convention: the closure must fail.
    {
        start();
        Sim s; s.c5SignUsed = 1.0f; s.sweep();
        flush("test G");
        check(has("CLOSURE"), "G: a wrong c5 sign reads CLOSURE");
        check(!has("NO_MEASURED_CAMERA_RESIDUAL"), "G: and never claims a clean camera");
    }
    // H: the engine's real pivot is not the configured one: the neck mis-cancels.
    {
        start();
        Sim s; s.engBelow = 20.0f; s.ramp(); s.sweep();
        flush("test H");
        check(has("PIVOT_MISMATCH"), "H: a different engine pivot reads PIVOT_MISMATCH");
        check(has("below 0.200 m"), "H: and solves the engine's 0.200 m");
        check(has("PITCH_RESIDUAL"), "H: and the mis-cancel shows as PITCH_RESIDUAL");
    }
    // I: the pawn walks.
    {
        start();
        Sim s; s.moving = true; s.sweep();
        flush("test I");
        check(count_of("moving") > 0, "I: a moving pawn is rejected as moving");
    }
    // J: the head snapshot is not the triple the writer used.
    {
        start();
        Sim s; s.torn = true; s.sweep();
        flush("test J");
        check(count_of("torn") > 0, "J: a torn snapshot is rejected as torn");
    }
    // K: level only: the pivot cannot be fitted.
    {
        start();
        Sim s; s.hold(0.0f, 200);
        flush("test K");
        check(has("LOW_VARIANCE"), "K: one pitch reads LOW_VARIANCE, not a fit");
    }

    // L: launch 1's crouched shape (2026-09-12): the engine has NO arc, the neck
    // cancels the standing one anyway. Looking down must read up and BACK.
    {
        start();
        Sim s; s.cyl = 65.0f; s.neutralU = 55.5f; s.engBelow = 0.0f; s.engBehind = 0.0f;
        s.ramp(); s.sweep();
        flush("test L");
        check(has("PITCH_RESIDUAL"), "L: cancelling an absent arc reads PITCH_RESIDUAL");
        check(has("PIVOT_MISMATCH"), "L: and the engine samples solve a pivot unlike the one in use");
        bool downUpBack = false;
        for (auto& l : g_lines)
            if (l.find("CROUCHED DOWN L") != std::string::npos && l.find("residual up +") != std::string::npos &&
                l.find("fwd -") != std::string::npos) downUpBack = true;
        check(downUpBack, "L: DOWN reads residual up + and fwd - (back and up, as reported in the headset)");
    }
    // M: the VR-78 fix's shape: the neck in use matches the engine's absent arc.
    {
        start();
        Sim s; s.cyl = 65.0f; s.neutralU = 55.5f; s.engBelow = 0.0f; s.engBehind = 0.0f;
        s.cfgBelow = 0.0f; s.cfgBehind = 0.0f;
        s.ramp(); s.sweep();
        flush("test M");
        check(has("NO_MEASURED_CAMERA_RESIDUAL"), "M: a zero crouched pivot against a zero engine arc is clean");
        check(has("PIVOT_MATCHES"), "M: and the fit agrees with the pivot in use");
    }

    if (g_fail) {
        printf("\n---- log of the last case ----\n%s", joined().c_str());
        printf("zaccount host: %d of %d checks FAILED\n", g_fail, g_checks);
        return 1;
    }
    printf("zaccount host: all %d checks passed\n", g_checks);
    return 0;
}
