// core/gfx/reentry_pair.inc - the reentry method's tag ring and its c5 pairing, in one
// place so the shipped method and the VR-80 host model compile the SAME code.
//
// Included by core/gfx/reentry.cpp inside its anonymous namespace, and by
// tools/reentry-pair-tests.cpp with the log macros stubbed. Moved verbatim from
// reentry.cpp (2026-09-13); the only change is that the arbitration reads the
// present's c5, the camera right row and ipd*scale from an ArbView instead of
// calling the camera seam, and keeps its history in an ArbState instead of the
// method's members. No behaviour change: the method builds the view from the same
// calls it made inline.

// The tag ring: game thread pushes (per draw), present thread pops (per
// present). Power-of-two, SPSC, self-healing on a skew.
constexpr uint32_t kRing = 8;
// VR-65: the tag carries the RECORD the draw was rendered with, so the pose
// travels with the image instead of being chosen by timing at submission.
// VR-80: draw is the game side's draw attempt id and pushMs the push time (ledger only).
struct Tag { int eye; bool posOk; float pos[3]; uint32_t rec; uint32_t acct; uint32_t draw; double pushMs; };
Tag           g_ring[kRing];
volatile LONG g_ringHead = 0, g_ringTail = 0;
uint32_t      g_ringDropped = 0, g_ringCleared = 0, g_tagMismatch = 0, g_tagOk = 0, g_tagUntagged = 0;
uint32_t      g_tagNoFrame = 0;   // 41.1 (session 8): tagged presents whose grab delivered no frame (no tag pushed)

uint32_t g_tagResynced = 0;
// 41.1 (session 9): the within-tick invariant. Between pass 1 and pass 2 the
// world does not tick, so the ONLY thing that moves the camera is the writer's
// eye: c5(pass 2) - c5(pass 1) = -ipd*scale along the camera's right row (the
// field holds the position, c5 negates it), nothing along forward or up. A
// present whose c5 sits exactly there from the previous present's IS a pass-2
// present, whatever the ring says; one whose c5 sits exactly +ipd*scale along
// right is a pass 1 after a still pass 2. The ring's order claim is checked
// against that measurement every present: a disagreement is counted, and
// three in a row drain the ring to the next expected tag (a tag eaten by a present that
// drew nothing, or pushed by a draw that never presented - both happen, the
// menu's draws outnumber its presents). Measured on the simulator: the tags
// swapped across a re-arm and within a second of the first arming (the
// frameid line's side check), the picture agreeing.
bool     g_c5Pair = true;              // [Stereo] C5Pair=1; `reentry c5pair on|off`
uint32_t g_c5Agree = 0, g_c5Disagree = 0, g_c5Realigned = 0, g_c5Verdicts = 0, g_c5Unknown = 0, g_c5Untagged = 0;
// 41.1 (session 10): what the measurement DID with a disagreement, and the
// ground truth it is judged by. g_c5Took/g_c5Held split the override by arm;
// g_c5Refused counts the manufactured tags no longer invented on an empty
// ring; g_pushSameEye is the fault ITSELF - two consecutive presents pushed
// the same eye to the runtime, which is what the runtime reports as abortLeft
// one stage later. It is counted at the push, on the present thread, with no
// lag and no inference: if the stale line's owner is right, this moves with it.
uint32_t g_c5Took = 0, g_c5Held = 0, g_c5Refused = 0, g_pushSameEye = 0;
int      g_lastPushedEye = 0;
// VR-80 ledger accounting: every tag that enters or leaves the ring, by how.
uint32_t g_pushAccepted = 0, g_pushRejected = 0, g_lastRejectedDraw = 0;
uint32_t g_popNormal = 0, g_popRepair = 0, g_popClearRemoved = 0, g_popClears = 0, g_popEmpty = 0, g_lifecycleRemoved = 0;
// VR-80 candidate F-late ([Stereo] LateTagRepair, default off; `reentry latetag on|off`).
// Measured in headset run 5: a present can show a draw's image a moment BEFORE that draw's
// tag reaches the ring. The pop is empty, the tag lands a few ms later, and every later pop
// is one tag behind the images until the three-disagreement drain catches up - one present
// with the left image in the right eye per episode cycle. With the lever on, an empty pop in
// a tagged stream whose c5 names an eye OWES that eye one tag; the next present removes the
// front tag as a repair only if its own c5 names the opposite eye and the front tag is the
// owed eye. Needing the next present's c5 to confirm is what keeps the fragile cross-tick
// arm from triggering it alone on a moving player.
bool     g_lateTagRepair = false;
uint32_t g_lateOwed = 0, g_lateRepaired = 0, g_lateExpired = 0;

double pair_now_ms() {
    static LARGE_INTEGER f = {};
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    return f.QuadPart ? (double)c.QuadPart * 1000.0 / (double)f.QuadPart : 0.0;
}

enum PopKind { POP_NORMAL, POP_REPAIR };

// Pop the tag for THIS present, strictly in push order. The game thread runs
// up to a frame ahead of the render thread (UE3's OneFrameThreadLag), so two
// pairs can sit in the ring legitimately; a depth beyond that is a skew and
// the ring is cleared. The ORDER pairs the eyes: one push per draw, one pop
// per present. (The 2026-09-03 headset run: re-aligning by camera position
// mis-paired a walking player - the engine moves the camera after the tick's
// write - and showed both frames in both eyes. Position is telemetry now.)
bool pop_tag(Tag& out, const float* c5, PopKind kind = POP_NORMAL) {
    (void)c5;
    const LONG tail = g_ringTail;
    const LONG head = InterlockedCompareExchange(&g_ringHead, 0, 0);
    if (tail == head) { ++g_popEmpty; return false; }
    if (head - tail > 6) {
        InterlockedExchange(&g_ringTail, head);
        ++g_ringCleared;
        ++g_popClears; g_popClearRemoved += (uint32_t)(head - tail);
        DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 3000,
                         "reentry: tag ring skewed (depth %ld) - cleared, mono until the next pair", head - tail);
        return false;
    }
    out = g_ring[tail & (kRing - 1)];
    InterlockedExchange(&g_ringTail, tail + 1);
    if (kind == POP_REPAIR) ++g_popRepair; else ++g_popNormal;
    return true;
}

// Push one draw's tag (game thread). Rejected, and counted, when the ring is full.
bool push_tag(int eyeSign, const float pos[3], uint32_t rec, uint32_t acct, uint32_t draw = 0) {
    const LONG head = InterlockedCompareExchange(&g_ringHead, 0, 0), tail = InterlockedCompareExchange(&g_ringTail, 0, 0);
    if (head - tail >= (LONG)kRing) { ++g_ringDropped; ++g_pushRejected; g_lastRejectedDraw = draw; return false; }   // no consumer (no present) or stalled
    Tag& t = g_ring[head & (kRing - 1)];
    t.eye = eyeSign;
    t.posOk = pos != nullptr;
    t.rec = rec;
    t.acct = acct;
    t.draw = draw;
    t.pushMs = pair_now_ms();
    if (pos) memcpy(t.pos, pos, sizeof(t.pos));
    InterlockedExchange(&g_ringHead, head + 1);
    ++g_pushAccepted;
    return true;
}

// The eye of the tag the next pop would return (false = empty).
bool peek_tag(int& eye) {
    const LONG tail = g_ringTail;
    const LONG head = InterlockedCompareExchange(&g_ringHead, 0, 0);
    if (tail == head) return false;
    eye = g_ring[tail & (kRing - 1)].eye;
    return true;
}

// The whole tag at the front of the ring, for the ledger's corroboration (false = empty).
bool peek_tag_full(Tag& out) {
    const LONG tail = g_ringTail;
    const LONG head = InterlockedCompareExchange(&g_ringHead, 0, 0);
    if (tail == head) return false;
    out = g_ring[tail & (kRing - 1)];
    return true;
}

// VR-95: corroborate only the front tag, never search or consume the queue.
// Bound is deliberately below an eye separation. It is a conservative guard,
// not a measured tolerance; moving cameras may refuse and retain legacy logic.
int corroborated_hand_eye(const float* c5, float ipd, uint32_t* rec, float* error) {
    *rec=0; *error=-1.0f;
    const LONG depth=InterlockedCompareExchange(&g_ringHead,0,0)-g_ringTail;
    Tag tag={};
    if (!c5 || !(ipd>0.0f) || !(ipd<100.0f) || depth<=0 || depth>6 ||
        !peek_tag_full(tag) || !tag.posOk || !tag.rec ||
        (tag.eye!=-1 && tag.eye!=1)) return 0;
    float d2=0;
    for(int i=0;i<3;++i) {
        const float d=c5[i]-tag.pos[i];
        if(!(d==d) || !(fabsf(d)<1e6f)) return 0;
        d2+=d*d;
    }
    *rec=tag.rec; *error=sqrtf(d2);
    const float bound=ipd*0.1f<0.25f?ipd*0.1f:0.25f;
    return *error<=bound?tag.eye:0;
}

// VR-80: what one present's pop and arbitration DID, filled on every call (cheap) so the
// ledger can print it. It records; it decides nothing.
enum ArbAction { ACT_AGREE = 1, ACT_TOOK = 2, ACT_HELD = 4, ACT_REALIGN = 8, ACT_INVENT = 16, ACT_REFUSE = 32, ACT_UNKNOWN = 64,
                 ACT_LATE = 128, ACT_OWE = 256 };
enum PopResult { POPR_EMPTY = 0, POPR_TAG = 1, POPR_CLEAR = 2 };
struct ArbTrace {
    int      popResult = POPR_EMPTY;
    LONG     tailBefore = 0, headBefore = 0;
    uint32_t clearRemoved = 0;
    uint32_t newestDraw = 0;          // the draw id of the newest tag in the ring before the pop
    uint32_t streakBefore = 0, streakAfter = 0;
    int      action = 0;
    uint32_t removed[6] = {0, 0, 0, 0, 0, 0};
    int      removedN = 0;            // repair removals (can exceed the six recorded)
    int      drainStop = 0;           // 1 next is the other eye, 2 empty, 3 a pop failed or cleared, 4 a late-tag repair
    int      lateEye = 0;             // F-late: the removed late tag's eye and record (the previous present's image)
    uint32_t lateRec = 0;
};

// The arbitration's memory: the previous present's c5 and the disagreement streak.
struct ArbState {
    float    prevC5[3] = {0.0f, 0.0f, 0.0f};
    bool     prevC5Ok = false;
    uint32_t c5Streak = 0;
    int      owedEye = 0;         // F-late: the eye an empty pop's image showed without a tag (0 = none owed)
    bool     inStream = false;    // F-late: the previous present popped a tag, or owed one
};

// What the arbitration reads about THIS present.
struct ArbView {
    bool  haveC5 = false;
    float c5now[3] = {0.0f, 0.0f, 0.0f};
    bool  basisOk = false;
    float br[3] = {0.0f, 0.0f, 0.0f};   // the camera's right row
    float ipd = 0.0f;                   // ipd * world scale, uu
};

// Pop this present's tag and let the c5 measurement check (and possibly override or
// drain) it. Returns whether the present is tagged; `t` holds the tag and its eye.
bool pop_and_arbitrate(ArbState& st, const ArbView& v, Tag& t, int& ringEye, int& inv,
                       float& along, float& other, ArbTrace* tr = nullptr) {
    const bool haveC5 = v.haveC5;
    const float* c5now = v.c5now;
    const uint32_t clearsBefore = g_popClears, clearedBefore = g_popClearRemoved;
    if (tr) {
        *tr = ArbTrace{};
        tr->tailBefore = g_ringTail;
        tr->headBefore = InterlockedCompareExchange(&g_ringHead, 0, 0);
        if (tr->headBefore != tr->tailBefore) tr->newestDraw = g_ring[(tr->headBefore - 1) & (kRing - 1)].draw;
        tr->streakBefore = st.c5Streak;
    }
    // The within-tick invariant (see g_c5Pair): what this present's c5
    // says about its pass, before the ring's claim is read. (Computed before the pop
    // since VR-80's F-late reads it there; it depends on c5 alone, so the order is free.)
    inv = 0;
    along = 0.0f; other = 0.0f;
    {
        const float* br = v.br;
        const float ipd = v.ipd;
        if (haveC5 && st.prevC5Ok && ipd > 1.0f && v.basisOk) {
            const float s[3] = {c5now[0] - st.prevC5[0], c5now[1] - st.prevC5[1], c5now[2] - st.prevC5[2]};
            along = s[0] * br[0] + s[1] * br[1] + s[2] * br[2];
            const float o[3] = {s[0] - along * br[0], s[1] - along * br[1], s[2] - along * br[2]};
            other = sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
            if (fabsf(along + ipd) < 0.35f * ipd && other < 0.5f * ipd) inv = +1;        // pass 2 after pass 1
            else if (fabsf(along - ipd) < 0.35f * ipd && other < 0.5f * ipd) inv = -1;   // pass 1 after a still pass 2
        }
        if (haveC5) { memcpy(st.prevC5, c5now, sizeof(st.prevC5)); st.prevC5Ok = true; }
    }
    // F-late: settle a tag owed by the previous present before this one pops.
    if (st.owedEye != 0) {
        Tag front;
        if (g_lateTagRepair && g_c5Pair && inv == -st.owedEye && peek_tag_full(front) && front.eye == st.owedEye) {
            Tag late;
            if (pop_tag(late, nullptr, POP_REPAIR)) {
                ++g_lateRepaired;
                if (tr) {
                    tr->action |= ACT_LATE; tr->drainStop = 4; tr->lateEye = late.eye; tr->lateRec = late.rec;
                    if (tr->removedN < 6) tr->removed[tr->removedN] = late.draw;
                    ++tr->removedN;
                }
                DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
                                 "reentry: late tag - the previous present showed a %s image before its tag arrived; "
                                 "that tag was removed as a repair so this present pops its own (repaired %u, owed %u, "
                                 "expired %u) [Stereo] LateTagRepair",
                                 st.owedEye < 0 ? "LEFT" : "RIGHT", g_lateRepaired, g_lateOwed, g_lateExpired);
            }
        } else {
            ++g_lateExpired;   // not confirmed by this present's c5, or the front tag is not the owed eye
        }
        st.owedEye = 0;
    }
    bool tagged = pop_tag(t, haveC5 ? c5now : nullptr);
    const bool popped = tagged;
    if (tr) {
        tr->popResult = tagged ? POPR_TAG : (g_popClears != clearsBefore ? POPR_CLEAR : POPR_EMPTY);
        tr->clearRemoved = g_popClearRemoved - clearedBefore;
    }
    ringEye = tagged ? t.eye : 0;   // VR-78: before the pairing may override it
    // F-late: an empty pop (not a depth clear) in a tagged stream whose c5 names an eye owes it.
    if (g_lateTagRepair && g_c5Pair && !popped && inv != 0 && st.inStream && g_popClears == clearsBefore) {
        st.owedEye = inv;
        ++g_lateOwed;
        if (tr) tr->action |= ACT_OWE;
    }
    st.inStream = popped || st.owedEye != 0;
    // The measurement is the pairing; the ring's order is the fallback for
    // the presents the invariant cannot name (a pass 1 after a moving
    // pass 2) and is kept aligned by the measurement (measured on the
    // simulator: the ring skews on its own in plain gameplay, ~2 per 25 s,
    // an extra present popping the next draw's tag; with the order alone
    // each skew swapped the eyes until the next).
    //
    // 41.1 (session 10): THE TWO ARMS ARE NOT EQUALLY TRUSTWORTHY, and
    // trusting them equally is what made the stale RIGHT eye.
    //   inv=+1 ("pass 2 after pass 1") compares two draws with NO world
    //     tick between them. The step is exactly -ipd by construction.
    //     Robust: it may override the ring on its own.
    //   inv=-1 ("pass 1 after a still pass 2") is the ONLY arm that reasons
    //     ACROSS a world tick, and is valid solely while the player is near
    //     still. A gently moving player (turning in place, decelerating,
    //     crouch-walk) parks the tick's travel inside the +-0.35*ipd window
    //     and this arm then names a genuine pass-2 present a pass 1.
    // A wrong -1 adds a LEFT and drops a RIGHT: two -1 reach the runtime
    // (abortLeft), the left swapchain is written twice and the right is not
    // written at all - `STALE R EYE ... ages L=0 R=2`, `stale submits L=0
    // R=20`, and not one game-side gate moves, because the game side DID
    // run pass 2 and DID push its +1. The fragile arm must therefore agree
    // with the ring, or defer to it and let the streak realign.
    if (g_c5Pair && inv != 0) {
        const bool robust = (inv == +1);   // within-tick: no world tick to cross
        if (tagged && t.eye != 0) {
            ++g_c5Verdicts;
            if (t.eye == inv) { ++g_c5Agree; st.c5Streak = 0; t.eye = inv; if (tr) tr->action |= ACT_AGREE; }
            else {
                ++g_c5Disagree;
                if (robust) { ++g_c5Took; t.eye = inv; if (tr) tr->action |= ACT_TOOK; }
                else {
                    // The cross-tick arm loses to the ring on its own. The
                    // streak below is what promotes a real skew.
                    ++g_c5Held;
                    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Debug, 3000,
                                     "reentry: c5 says pass 1 (step %+.2f along right, %.2f other; ipd*scale %.2f) but "
                                     "the ring says %+d - the cross-tick arm DEFERS to the ring (held %u, took %u, "
                                     "%u agree / %u disagree); three in a row still realign",
                                     along, other, v.ipd, t.eye,
                                     g_c5Held, g_c5Took, g_c5Agree, g_c5Disagree);
                }
                if (++st.c5Streak >= 3) {
                    // The ring is off: drain it to the tag the NEXT present
                    // must pop (the other eye of this measured one), so a
                    // backlog of any depth (a method re-select leaves up to
                    // a ring of stale tags) aligns in one present.
                    Tag t2;
                    bool popped = false;
                    for (uint32_t k = 0; k < kRing; ++k) {
                        int next = 0;
                        if (!peek_tag(next)) { if (tr) tr->drainStop = 2; break; }
                        if (next == -inv) { if (tr) tr->drainStop = 1; break; }
                        if (pop_tag(t2, nullptr, POP_REPAIR)) {
                            popped = true;
                            if (tr) { if (tr->removedN < 6) tr->removed[tr->removedN] = t2.draw; ++tr->removedN; }
                        } else { if (tr) tr->drainStop = 3; break; }
                    }
                    ++g_c5Realigned;
                    st.c5Streak = 0;
                    t.eye = inv;   // the streak earned the override, either arm
                    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 3000,
                                     "reentry: the tag ring skewed against the draws - three presents in a row whose c5 "
                                     "step (%+.2f along right, %.2f other; ipd*scale %.2f) named the other pass; realigned "
                                     "by one pop (%s) - realigned %u times, %u agree / %u disagree so far (the eyes "
                                     "followed the measurement throughout; the order alone would have swapped them)",
                                     along, other, v.ipd,
                                     popped ? "a tag dropped" : "the ring was empty", g_c5Realigned, g_c5Agree, g_c5Disagree);
                }
            }
            tagged = true;
        } else {
            // Nothing to override: the ring was empty, or it delivered the
            // 0 tag a SINGLE gameplay draw pushes (scene_draw's one push
            // per draw). Only the within-tick arm may name an eye out of
            // nothing - a manufactured -1 here is a LEFT that no draw ever
            // pushed, and it is invisible downstream because setting
            // tagged=true is exactly what keeps `method untagged presents`
            // reading 0. An unnamed present goes out untagged and honest;
            // [Stereo] HoldUntagged covers the one-frame mono flick.
            ++g_c5Untagged;
            if (robust) { t.eye = inv; tagged = true; if (tr) tr->action |= ACT_INVENT; }
            else { ++g_c5Refused; t.eye = 0; if (tr) tr->action |= ACT_REFUSE; }
        }
    } else if (inv == 0 && tagged && t.eye != 0) { ++g_c5Unknown; if (tr) tr->action |= ACT_UNKNOWN; }
    if (tr) tr->streakAfter = st.c5Streak;
    return tagged;
}

// Reload trace: L,R,0 tags can accompany L,L,R cameras around a SINGLE tick.
// This observer proposes a correction only for that adjacent triple. It never
// searches or drains the queue. The caller must retire the still-buffered wrong
// R label before using the saved R record on the now-confirmed right image.
struct SingleTagState {
    int previousEye = 0;
    bool previousMeasured = false;
    bool pending = false;
    Tag right = {};
};

bool observe_single_tag(SingleTagState& st, const ArbView& v, const Tag& raw,
                        int ringEye, int finalEye, int inv, float along,
                        float other, uint32_t action, Tag& recovered) {
    const bool geometry = v.haveC5 && v.basisOk && v.ipd > 1.0f;
    const float tolerance = 0.1f * v.ipd; // strict corroboration; moving/ambiguous cases decline
    bool repair = false;
    if (st.pending && geometry && ringEye == 0 && raw.draw == st.right.draw + 1 &&
        inv == +1 && (action & ACT_INVENT)) {
        const float x = v.c5now[0] - st.right.pos[0];
        const float y = v.c5now[1] - st.right.pos[1];
        const float z = v.c5now[2] - st.right.pos[2];
        repair = x*x + y*y + z*z < tolerance*tolerance;
        if (repair) recovered = st.right;
    }
    st.pending = false; // one immediately following present only
    if (geometry && st.previousMeasured && ringEye == +1 && finalEye == +1 && st.previousEye == -1 &&
        inv == 0 && raw.posOk && raw.rec != 0 && (action & ACT_UNKNOWN) &&
        fabsf(along) < tolerance && other < tolerance) {
        const float dx = raw.pos[0] - v.c5now[0];
        const float dy = raw.pos[1] - v.c5now[1];
        const float dz = raw.pos[2] - v.c5now[2];
        const float a = dx*v.br[0] + dy*v.br[1] + dz*v.br[2];
        const float ox = dx-a*v.br[0], oy = dy-a*v.br[1], oz = dz-a*v.br[2];
        if (fabsf(a + v.ipd) < tolerance && ox*ox + oy*oy + oz*oz < tolerance*tolerance) {
            st.right = raw;
            st.pending = true;
        }
    }
    st.previousEye = finalEye;
    st.previousMeasured = geometry;
    return repair;
}
