// Execute extracted production lookup bodies. Independent ring contents stand
// in for camera writes; rendered axes are supplied separately by each fixture.
#include "core/vr/pose_record.h"
#include "core/gfx/clarity_math.h"
#include <cstdio>
#include <cmath>
#include <cstring>
namespace dvr::clock { static double testNow=1000; double now_ms(){return testNow;} }
namespace dvr::pose {
static constexpr unsigned kRing=128;
static Record g_ring[kRing];
static void ensure_cs(){}
struct Lock { Lock(){} ~Lock(){} };
#include "pose_view_body.inc"
}
using namespace dvr::pose;
static int passed=0,failed=0;
static void check(const char* name,bool ok){printf("pose-view/%s %s\n",name,ok?"PASS":"FAIL");ok?++passed:++failed;}
static Record rec(unsigned id,float yaw=0,float pitch=0,float roll=0){
    Record v{};v.id=id;v.pairId=id;v.eye=-1;v.eyePosOk=true;v.openedMs=990;
    v.cameraIdentity=0x1234;v.sceneEpoch=3;v.cam.ok=true;
    v.cam.yawDeg=yaw;v.cam.pitchDeg=pitch;v.cam.rollDeg=roll;
    v.track.ok=true;v.track.gen=id;v.track.py=1.6f;
    v.track.qy=sinf(yaw*0.00872664626f);v.track.qw=cosf(yaw*0.00872664626f);
    return v;
}
static void reset(){memset(g_ring,0,sizeof(g_ring));}
static const float zero[3]={};
static bool resolve(float yaw,float pitch,float roll,Record& out){
    const auto b=dvr::clarity::basis_from_rotator(pitch,yaw,roll);
    Record anchor{};float dist=0,second=0;
    if(!find_view(zero,0.05f,400,&anchor,&dist,&second))return false;
#ifdef OLD_CONTROL
    // Build 239 refuses every position tie regardless of observed rotation.
    if(second<0.10f)return false;
    out=anchor;return true;
#else
    return resolve_view_tie(zero,b.f,b.r,b.u,400,anchor,&out);
#endif
}
int main(){
    Record out{};float d=0,s=0;
    reset();g_ring[1]=rec(1,12);g_ring[2]=rec(2,14);
    check("old_position_lookup_is_tied",find_view(zero,.05f,400,&out,&d,&s)&&s==0);
    check("head_turn_selects_older_rendered_view",resolve(12,0,0,out)&&out.id==1);
    check("head_turn_selects_newer_rendered_view",resolve(14,0,0,out)&&out.id==2);
    check("written_rotation_lag_refuses",!resolve(13,0,0,out));
    check("wrong_convention_refuses",!resolve(192,0,0,out));
    reset();g_ring[1]=rec(1,179.99f,31,-22);g_ring[2]=rec(2,170,31,-22);
    check("combined_pitch_yaw_roll",resolve(179.99f,31,-22,out)&&out.id==1);
    check("yaw_wrap",resolve(-180.01f,31,-22,out)&&out.id==1);
    check("wrong_pitch_refuses",!resolve(179.99f,32,-22,out));
    check("wrong_roll_refuses",!resolve(179.99f,31,-21,out));
    reset();g_ring[1]=rec(1,0);g_ring[2]=rec(2,0);
    check("equivalent_heads_newest",resolve(0,0,0,out)&&out.id==2);
    g_ring[2].track.qw=-1;
    check("negated_quaternion_same_pose",resolve(0,0,0,out));
    g_ring[2].track.px=0.00001f;
    check("tiny_tracking_noise_equivalent",resolve(0,0,0,out));
    g_ring[2].track.px=0.0001f;
    check("same_view_different_head_position_refused",!resolve(0,0,0,out));
    g_ring[2]=rec(2,0);g_ring[2].track.qy=0.001f;
    check("same_view_different_head_rotation_refused",!resolve(0,0,0,out));
    g_ring[2]=rec(2,0);g_ring[2].eye=1;
    check("opposite_eye_refused_even_same_generation",(g_ring[2].track.gen=1,!resolve(0,0,0,out)));
    g_ring[2]=rec(2,0);g_ring[2].sceneEpoch=4;
    check("other_level_refused",!resolve(0,0,0,out));
    g_ring[2]=rec(2,0);g_ring[2].cameraIdentity=0x5678;
    check("other_camera_refused",!resolve(0,0,0,out));
    g_ring[2]=rec(2,0);g_ring[2].cam.ok=false;
    check("unknown_camera_refused",!resolve(0,0,0,out));
    g_ring[2]=rec(2,0);g_ring[2].track.ok=false;
    check("unknown_head_refused",!resolve(0,0,0,out));
    g_ring[2]=rec(2,0);g_ring[2].cam.yawDeg=NAN;
    check("nonfinite_competitor_refused",!resolve(0,0,0,out));
    reset();g_ring[1]=rec(1,0);g_ring[1].track.qw=0;
    check("invalid_candidate_quaternion_refused",!resolve(0,0,0,out));
    g_ring[1]=rec(1,0);g_ring[1].track.py=NAN;
    check("nonfinite_candidate_position_refused",!resolve(0,0,0,out));
    reset();g_ring[1]=rec(1,0);g_ring[2]=rec(2,0.04f);
    check("near_angle_conflict_refused",!resolve(0,0,0,out));
    g_ring[2]=rec(2,2);
    check("quantization_noise_accepted",resolve(.02f,0,0,out)&&out.id==1);
    check("outside_match_tolerance_refused",!resolve(.04f,0,0,out));
    g_ring[1].eyePos[0]=.06f;
    check("matching_angle_wrong_position_refused",!resolve(0,0,0,out));
    g_ring[1].eyePos[0]=0;g_ring[1].openedMs=599;
    check("expired_view_refused",!resolve(0,0,0,out));
    g_ring[1].openedMs=1001;
    check("future_view_refused",!resolve(0,0,0,out));
    reset();check("empty_ring_refused",!resolve(0,0,0,out));
    reset();g_ring[1]=rec(1,12);g_ring[2]=rec(2,14);g_ring[2].eyePos[0]=10;
    check("legacy_unique_lookup_unchanged",find_view(zero,.05f,400,&out,&d,&s)&&out.id==1&&s==10);
    printf("pose-view: %d PASS, %d FAIL\n",passed,failed);
    return failed?1:0;
}
