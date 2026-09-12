#include "../src/game/dishonored/aim_ray.h"
#include <openxr/openxr.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace dvr::vr;
using namespace dvr::aim;
static uint64_t fakeMs=1000;
uint64_t GetTickCount64() { return fakeMs; }
AimVisualConfig g_aimVisual;
AimVisualStats g_aimVisualStats;
uint64_t g_aimVisualPublishedMs=1000;
uint32_t g_aimLayerLimit=16;
bool g_viewsValid=true;
XrView g_views[2] = {{XR_TYPE_VIEW},{XR_TYPE_VIEW}};
XrSpace g_space=(XrSpace)1;
XrSwapchain g_laserSwapchain=(XrSwapchain)2;
void* g_laserDot=(void*)3;
constexpr uint32_t kLaserTexSize=64;
static unsigned uploads=0;
static unsigned imageCopies=0, imageReleases=0;
static bool uploadOk=true, waitOk=true, releaseOk=true;
struct Image { void* texture=(void*)4; } g_laserImages[1];
struct Context { void CopyResource(void*,void*) {++imageCopies;} } context;
Context* g_context=&context;
XRAPI_ATTR XrResult XRAPI_CALL xrAcquireSwapchainImage(XrSwapchain,const XrSwapchainImageAcquireInfo*,uint32_t* index) {
    ++uploads; *index=0; return uploadOk ? XR_SUCCESS : XR_ERROR_RUNTIME_FAILURE;
}
XRAPI_ATTR XrResult XRAPI_CALL xrWaitSwapchainImage(XrSwapchain,const XrSwapchainImageWaitInfo*) {
    return waitOk ? XR_SUCCESS : XR_ERROR_RUNTIME_FAILURE;
}
XRAPI_ATTR XrResult XRAPI_CALL xrReleaseSwapchainImage(XrSwapchain,const XrSwapchainImageReleaseInfo*) {
    ++imageReleases; return releaseOk ? XR_SUCCESS : XR_ERROR_RUNTIME_FAILURE;
}
// Rendering tests do not need the extracted production diagnostic logger.
#define DVR_LOG_EVERY_MS(...) ((void)0)
#include "aim_visual_bodies.inc"
#undef DVR_LOG_EVERY_MS
static unsigned checks=0;
static void check(bool yes,const char* why) {++checks;if(!yes){std::fprintf(stderr,"FAIL: %s\n",why);std::exit(1);}}
static bool near(float a,float b) {return std::abs(a-b)<0.0001f;}
static Ray make(const float q[4]) {const float pos[3]={-.2f,1.3f,-.1f};return from_pose(0,true,pos,q,42,1000,1000);}
int main() {
    const float identity[4]={0,0,0,1}, yaw[4]={0,.70710678f,0,.70710678f};
    const float pitch[4]={.70710678f,0,0,.70710678f}, roll[4]={0,0,1,0};
    const float neg[4]={0,0,0,-1}, nonunit[4]={0,0,0,1.5f};
    check(make(identity).ok && near(make(identity).dirXr[2],-1),"identity aims -Z");
    check(near(make(yaw).dirXr[0],-1),"XR positive Y yaw aims -X");
    check(near(make(pitch).dirXr[1],1),"positive X pitch aims up");
    check(near(make(roll).dirXr[2],-1),"rolling controller upside down preserves pointing");
    check(near(make(neg).dirXr[2],-1) && near(make(nonunit).dirXr[2],-1),"quaternion sign and normalization");
    const float invalid[4]={0,0,0,0}, nanq[4]={0,0,0,std::numeric_limits<float>::quiet_NaN()};
    check(!make(invalid).ok && !make(nanq).ok,"invalid quaternion refused");
    const float pos[3]={0,1,0};
    check(!from_pose(0,false,pos,identity,42,1000,1000).ok,"tracking loss does not aim at origin");
    check(!from_pose(2,true,pos,identity,42,1000,1000).ok,"invalid hand refused");
    check(!from_pose(0,true,pos,identity,42,1000,1251).ok,"input sample expires");
    check(!from_pose(0,true,pos,identity,0,1000,1000).ok,"generation required");
    check(!aim_visual_fresh(1000,1251,1251),"republishing stale sample cannot revive it");
    check(!aim_visual_fresh(1251,1000,1251),"stale publish expires too");
    check(aim_visual_fresh(1000,1000,1250),"age boundary inclusive");
    for (int pitchDeg=-90;pitchDeg<=90;pitchDeg+=15) for(int yawDeg=-180;yawDeg<=180;yawDeg+=15)
        for(int rollDeg=-180;rollDeg<=180;rollDeg+=45) {
            float q[4]; dvr::xrmath::xr_local_trim_quat(pitchDeg*.01745329252f,yawDeg*.01745329252f,rollDeg*.01745329252f,q);
            Ray ray=make(q); auto v=visual(ray,true,true,8,.5f);
            const float expected[3]={std::cos(pitchDeg*.01745329252f)*std::sin(yawDeg*.01745329252f),
                std::sin(pitchDeg*.01745329252f),-std::cos(pitchDeg*.01745329252f)*std::cos(yawDeg*.01745329252f)};
            check(ray.ok && v.count==5 && v.generation==42,"one snapshot for dot and beam");
            for(int a=0;a<3;++a) check(near(ray.dirXr[a],expected[a]),"independent spherical direction, roll invariant");
            for(int i=0;i<v.count;++i) {
                float delta[3], along=0;for(int a=0;a<3;++a){delta[a]=v.points[i].pos[a]-ray.originXr[a];along+=delta[a]*ray.dirXr[a];}
                check(along>0,"every guide point is forward");
                for(int a=0;a<3;++a) check(near(delta[a],along*ray.dirXr[a]),"dot and beam share exact line");
            }
        }
    auto ray=make(identity); auto v=visual(ray,true,true,8,.5f);
    check(v.points[0].dot && near(v.points[0].pos[2],-8.1f),"endpoint first at requested distance");
    check(visual(ray,false,false,8,.5f).count==0,"off is empty");
    check(!visual(ray,true,true,NAN,.5f).valid,"nonfinite configuration refused");
    check(aim_visual_budget(1,3,13)==2 && aim_visual_budget(12,16,13)==1,"both layer limits enforced");
    // The following calls execute extracted PRODUCTION billboard and layer code.
    g_views[0].pose.position={-.032f,1.6f,0};g_views[1].pose.position={.032f,1.6f,0};
    XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    XrCompositionLayerQuad screen{XR_TYPE_COMPOSITION_LAYER_QUAD};
    XrCompositionLayerQuad quads[5];
    const XrCompositionLayerBaseHeader* layers[13]={reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection)};
    uint32_t count=1,dots=0,beam=0; bool published=false;
    g_aimVisual=v;
    auto result=build_aim_visual(quads,layers,count,13,published,dots,beam);
    check(result==AimVisualResult::Submitted && count==6 && dots==1 && beam==4,"projection submits dot and beam");
    check(uploads==1 && published,"one texture upload for all five quads");
    check(quads[0].eyeVisibility==XR_EYE_VISIBILITY_BOTH && quads[0].space==g_space,"XR LOCAL quad visible to both eyes");
    check(near(quads[0].pose.position.z,v.points[0].pos[2]) && quads[0].size.width>0,"geometry follows explicit endpoint");
    // A held projection has the same base layer type: no fresh-frame flag may suppress it.
    count=1; dots=beam=0;
    check(build_aim_visual(quads,layers,count,13,published,dots,beam)==AimVisualResult::Submitted && uploads==1,"held projection keeps visuals without duplicate upload");
    // Moving the head changes billboarding/size only, not the world endpoint.
    g_views[0].pose.position.x+=1;g_views[1].pose.position.x+=1;
    count=1; dots=beam=0;build_aim_visual(quads,layers,count,13,published,dots,beam);
    check(near(quads[0].pose.position.x,v.points[0].pos[0]),"head motion does not move aim point");
    layers[0]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&screen);count=1;
    check(build_aim_visual(quads,layers,count,13,published,dots,beam)==AimVisualResult::NotProjection,"menu quad refuses guides");
    layers[0]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection);
    fakeMs=1251;count=1;
    check(build_aim_visual(quads,layers,count,13,published,dots,beam)==AimVisualResult::Stale,"renderer independently rejects stale sample");
    fakeMs=1000;g_aimLayerLimit=2;count=1;dots=beam=0;
    check(build_aim_visual(quads,layers,count,13,published,dots,beam)==AimVisualResult::Submitted && count==2 && dots==1 && beam==0,"low budget prioritizes endpoint");
    g_aimLayerLimit=1;count=1;
    check(build_aim_visual(quads,layers,count,13,published,dots,beam)==AimVisualResult::Budget && count==1,"no layer budget refuses without touching base");
    g_aimLayerLimit=16;published=false;uploadOk=false;count=1;dots=beam=0;
    check(build_aim_visual(quads,layers,count,13,published,dots,beam)==AimVisualResult::ImageFailed && count==1 && !dots && !beam,"failed upload adds no undefined layers");
    uploadOk=true; waitOk=false; const unsigned copied=imageCopies, released=imageReleases;
    check(!publish_laser_image() && imageCopies==copied && imageReleases==released+1,"failed XR wait is released but never treated as a valid upload");
    waitOk=true; releaseOk=false;
    check(!publish_laser_image(),"failed XR release cannot authorize a layer");
    releaseOk=true;
    g_viewsValid=false;
    check(build_aim_visual(quads,layers,count,13,published,dots,beam)==AimVisualResult::NoViews,"invalid views named");
    g_viewsValid=true;g_laserDot=nullptr;
    check(build_aim_visual(quads,layers,count,13,published,dots,beam)==AimVisualResult::NoTexture,"missing texture named");
    const auto before=g_aimVisualStats.submitted;
    note_aim_visual(AimVisualResult::PairPending);note_aim_visual(AimVisualResult::EndFailed);
    check(g_aimVisualStats.submitted==before,"pair deferral and XR failure are not submissions");
    note_aim_visual(AimVisualResult::Submitted,1,4);
    check(g_aimVisualStats.submitted==before+1 && g_aimVisualStats.dotFrames==1,"outcomes count only actual success");
    std::printf("PASS: %u aim math and production compositor assertions\n",checks);
}
