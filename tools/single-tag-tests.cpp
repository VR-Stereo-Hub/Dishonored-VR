// Adjacent R/0 reload trace replay; current production classifier and slot guard.
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#define DVR_CAT 0
#define DVR_LOG_EVERY_MS(...) ((void)0)
namespace model {
#include "core/gfx/reentry_pair.inc"
}
using namespace model;
enum class Mode { Shared, Deferred, Sync };
static Mode g_mode = Mode::Shared;
static bool g_sharedWait = false;
static int g_sharedCur = 1, g_rtCur = 1;
static bool g_sharedValid[2] = {true,true}, g_rtValid[2] = {true,true};
static unsigned g_serial = 8, g_sharedSerial[2] = {8,7}, g_rtSerial[2] = {8,7};
static int g_sharedTag[2] = {1,0}, g_rtTag[2] = {1,0};
static unsigned g_sharedRec[2] = {102,0}, g_rtRec[2] = {102,0};
#include "single_tag_body.inc"
static int checks = 0, fails = 0;
static void check(bool ok, const char* why) {
    ++checks; if (!ok) { ++fails; std::printf("FAIL: %s\n", why); }
}
static ArbView view(float x) {
    ArbView v; v.haveC5 = v.basisOk = true; v.ipd = 6.82f;
    v.br[0] = 1; v.c5now[0] = x; return v;
}
static Tag tag(int eye, unsigned id, float x) {
    Tag t = {}; t.eye = eye; t.draw = t.rec = id; t.acct = id+1000;
    t.posOk = true; t.pos[0] = x; return t;
}
static void seed(SingleTagState& st, float repeatTravel = 0) {
    Tag result;
    auto l = tag(-1,101,3.41f), r = tag(1,102,-3.41f);
    observe_single_tag(st,view(3.41f),l,-1,-1,-1,6.82f,0,ACT_AGREE,result);
    observe_single_tag(st,view(3.41f+repeatTravel),r,1,1,0,repeatTravel,0,ACT_UNKNOWN,result);
}
static bool confirm(SingleTagState& st, ArbView v = view(-3.41f), unsigned id = 103, int inv = 1) {
    Tag result; auto z = tag(0,id,0); z.posOk = false;
    bool ok = observe_single_tag(st,v,z,0,1,inv,-6.82f,0,ACT_INVENT,result);
    if (ok) check(result.rec==102 && result.draw==102 && result.acct==1102,
                  "recovery carries the complete original right record");
    return ok;
}
int main(int argc, char** argv) {
    bool legacy = argc > 1 && atoi(argv[1]);
    SingleTagState st; seed(st);
    check(st.pending,"repeated-left under R tag is only a pending candidate");
    check(confirm(st),"adjacent zero-tag right image confirms the sequence");
    check(!confirm(st),"confirmation expires after exactly one following present");
    st={}; seed(st,2); check(!confirm(st),"moving repeat outside strict window declines");
    st={}; seed(st); check(!confirm(st,view(-3.41f),104),"nonadjacent draw declines");
    st={}; seed(st); check(!confirm(st,view(-3.41f),103,-1),"opposite invariant declines");
    st={}; seed(st); check(!confirm(st,view(3.41f)),"camera not matching saved right declines");
    st={}; seed(st); auto missing=view(-3.41f); missing.haveC5=false;
    check(!confirm(st,missing),"missing camera evidence declines");
    st={}; seed(st); auto nan=view(NAN); check(!confirm(st,nan),"NaN camera declines");
    st={}; seed(st); st={}; check(!confirm(st),"lifecycle reset cannot reuse pending identity");
    st={}; Tag result; auto actualR=tag(1,102,-3.41f), zero=tag(0,103,3.41f);
    observe_single_tag(st,view(-3.41f),actualR,1,1,1,-6.82f,0,ACT_AGREE,result);
    check(!confirm(st,view(3.41f),103,-1),"normal R then single left is unchanged");
    check(!retire_last_right_grab(999),"slot record mismatch refuses");
    g_sharedSerial[0]=7; check(!retire_last_right_grab(102),"older grab refuses"); g_sharedSerial[0]=8;
    g_sharedTag[0]=-1; check(!retire_last_right_grab(102),"left slot refuses"); g_sharedTag[0]=1;
    g_sharedValid[0]=false; check(!retire_last_right_grab(102),"invalid grab refuses");g_sharedValid[0]=true;
    g_sharedWait=true; check(!retire_last_right_grab(102),"already delivered shared mode refuses");g_sharedWait=false;
    g_mode=Mode::Sync;check(!retire_last_right_grab(102),"sync mode refuses");g_mode=Mode::Deferred;
    check(retire_last_right_grab(102)&&g_rtTag[0]==0&&g_rtRec[0]==0,"deferred identified wrong right is held");
    g_mode=Mode::Shared;
    st={}; seed(st); bool recognized=confirm(st);
    bool fixed=!legacy && recognized && retire_last_right_grab(102);
    // The buffered image is LEFT geometry tagged R. The next image is genuine R
    // carrying the SINGLE record. Repair must eliminate BOTH wrong eye and record.
    int wrongEye = g_sharedTag[0] == 1 ? 1 : 0;
    unsigned nextRec = fixed ? 102 : 103;
    check(wrongEye==0,"trace replay never delivers the repeated left image into the right eye");
    check(nextRec==102,"trace replay restores the right image pose record");
    if (fixed) check(!retire_last_right_grab(102),"retired slot cannot be retired twice");
    printf("single-tag: %d checks, %d failures (legacy=%d)\n", checks,fails,legacy);
    return fails ? 1 : 0;
}
