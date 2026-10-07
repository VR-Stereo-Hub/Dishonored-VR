#include "game/dishonored/hands/arm_rig.h"
#include <cstdio>
#include "core/gfx/frame_burst.h"
#include <limits>
using namespace dvr::ik;
static int checks=0,failures=0;
static Vec unit_copy(Vec v){unit(v);return v;}
static void check(bool ok,const char* name){++checks;if(!ok){++failures;printf("FAIL: %s\n",name);}}
static bool near(Vec a,Vec b,float eps=.002f){return length(a-b)<eps;}
int main(int argc,char** argv){
    Body body{{1,2,3},{0,0,-1},{1,0,0},{0,1,0}};Settings cfg;
    Vec l=shoulder(body,cfg,0,1),r=shoulder(body,cfg,1,1);
    check(fabsf(length(r-l)-36)<.001f,"width means total width");
    cfg.rightCm=7;cfg.forwardCm=3;cfg.upCm=-11;
    Vec center=(shoulder(body,cfg,0,1)+shoulder(body,cfg,1,1))*.5f;
    check(near(center,{8,-9,0}),"shared right offset is not mirrored");
    Solution sol;
    Vec pole{-.3f,-1,.6f},outward{0,0,1};

    {   // ArmIKGameArmShoulder: the game's arm re-seated on the IK shoulder, wrist untouched
        Vec ns{10,0,40},w{0,-30,60},tg{-6,-4,38};
        ShoulderFit f=shoulder_fit(ns,w,tg,.8f,1.25f,1.0f);
        check(f.ok&&near(point(f.move,w),w),"shoulder fit keeps the game's wrist");
        check(f.ok&&f.residual<.01f&&near(point(f.move,ns),tg),"shoulder fit lands on the IK shoulder");
        check(fabsf(f.offset-length(tg-ns))<.001f,"shoulder fit reports the offset it removed");
        Vec mid=(ns+w)*.5f,side=mid+unit_copy(cross(ns-w,Vec{0,0,1}))*3;
        check(fabsf(length(point(f.move,side)-point(f.move,mid))-3)<.02f,"shoulder fit keeps the arm's thickness");
        ShoulderFit same=shoulder_fit(ns,w,ns,.8f,1.25f,1.0f);
        check(same.ok&&same.angle<1e-4f&&fabsf(same.stretch-1)<1e-5f&&near(point(same.move,{3,4,5}),{3,4,5}),"shoulder fit is identity when the shoulders agree");
        ShoulderFit far=shoulder_fit(ns,w,{-200,90,0},.8f,1.25f,.6f);
        check(far.ok&&far.stretch<=1.2501f&&far.angle<=.6001f&&far.residual>1&&near(point(far.move,w),w),"shoulder fit is bounded and says what is left");
        check(!shoulder_fit(w,w,tg,.8f,1.25f,1.0f).ok&&!shoulder_fit(ns,w,{NAN,0,0},.8f,1.25f,1.0f).ok,"shoulder fit refuses a degenerate arm");
        // ArmIKGameArmMaxStretch: a takedown whose IK shoulder sits 1.6 arm lengths from the wrist,
        // straight back along the arm. The old 1.25 bound leaves it short; the 1.9 default lands.
        Vec back=w+(ns-w)*1.6f;
        ShoulderFit tight=shoulder_fit(ns,w,back,.8f,1.25f,1.0f),loose=shoulder_fit(ns,w,back,.8f,1.9f,1.0f);
        check(tight.ok&&fabsf(tight.wanted-1.6f)<.001f&&fabsf(tight.stretch-1.25f)<1e-5f&&fabsf(tight.residual-.35f*length(ns-w))<.01f,
              "shoulder fit reports the stretch it wanted and the residual the bound left");
        check(loose.ok&&fabsf(loose.stretch-1.6f)<.001f&&loose.residual<.01f&&near(point(loose.move,w),w),"a 1.9 stretch bound reaches a 1.6 takedown, wrist kept");
    }
    check(solve({0,0,0},{30,0,0},pole,outward,{},25,26,.5f,sol),"reachable solve");
    check(near(sol.shoulder,{}),"reachable shoulder stays nominal");
    check(near(sol.wrist,{30,0,0}),"wrist stays exact");
    for(int i=0;i<500;++i){
        Vec w{60*cosf(i*.08f),50*sinf(i*.11f),35*cosf(i*.07f)};
        Solution a,b;bool ok=solve({},w,pole,outward,{},25,26,.5f,a);
        check(ok&&fabsf(length(a.elbow-a.shoulder)-25)<.003f&&fabsf(length(w-a.elbow)-26)<.003f,"segment lengths survive reach correction");
        Mat3 rot=axis_angle({1,2,3},.91f);Vec translation{100,-37,62};
        bool ok2=solve(translation,rotate(rot,w)+translation,rotate(rot,pole),rotate(rot,outward),{},25,26,.5f,b);
        check(ok2&&near(b.elbow,rotate(rot,a.elbow)+translation,.008f),"rigid frame covariance");
    }
    check(solve({}, {},pole,outward,{},25,26,.5f,sol)&&finite(sol.elbow),"zero reach has deterministic direction");
    check(sol.shoulderShift>0,"close wrist shifts only solved shoulder");
    check(solve({}, {200,0,0},pole,outward,{},25,26,.5f,sol)&&near(sol.wrist,{200,0,0})&&sol.shoulderShift>100,"far wrist stays attached");
    check(solve({}, {0,-40,0},{0,-1,0},{1,0,0},{},25,26,.5f,sol)&&finite(sol.elbow),"parallel pole stays finite");
    check(!solve({}, {},pole,outward,{},-1,26,.5f,sol),"negative segment refused");
    check(!solve({}, {},pole,outward,{},1,90,.5f,sol),"inconsistent reach interval refused");
    check(!solve({}, {std::numeric_limits<float>::infinity(),0,0},pole,outward,{},25,26,.5f,sol),"infinity refused");
    Mat3 identity=frame_delta({1,0,0},{0,0,1},{1,0,0},{0,0,1});
    check(near(rotate(identity,{2,3,4}),{2,3,4}),"reference frame yields identity");
    Mat3 turn=axis_angle({0,1,0},1.1f);
    Xform skinning=skin(turn,2,{9,8,7},{3,4,5});
    check(near(point(skinning,{9,8,7}),{3,4,5}),"skin transform preserves joint anchor");
    check(fabsf(unwrap(-3.13f,3.13f)-3.1531853f)<.001f,"twist unwrap across pi");
    check(fabsf(twist(dvr::hf::identity3(),axis_angle({0,0,1},.7f),{0,0,1})-.7f)<.001f,"twist measures own wrist delta");
    BodyYaw yaw;check(fabsf(yaw.update(3.1f,0)-3.1f)<.001f,"body initial yaw");
    check(fabsf(wrap(yaw.update(-3.1f,.016f)-3.1f))<.1f,"body yaw wrap takes short path");
    PoseHistory history;
    auto& first=history.acquire(10,0,1000);history.commit(first,0,{1,2,3},.7f);
    auto& secondEye=history.acquire(10,1,1001);
    check(secondEye.yaw==0&&near(secondEye.prior[0],{}),"second eye uses original pose history and yaw");
    history.commit(secondEye,0,{9,9,9},2);
    auto& next=history.acquire(11,.2f,1016);
    check(next.fresh&&near(next.prior[0],{1,2,3})&&next.twist[0]==.7f,"one history advance per pose");
    check(near(next.prior[1],{})&&next.twist[1]==0,"one hand never populates the other's history");
    auto& old=history.acquire(9,2,1020);history.commit(old,0,{99,99,99},99);
    check(history.lastGen==11&&near(history.prior[0],{1,2,3}),"old queued pose cannot rewind live history");
    auto& resumed=history.acquire(12,1,1500);
    check(!resumed.fresh&&near(resumed.prior[0],{})&&resumed.yaw==1,"tracking gap clears elbow and yaw history");
    // Replay the two counters observed in the accepted live candidate.
    // View matches used locate ~58000; fallback used publication ~88000.
    // That old wiring starved subsequent matched draws of body/twist history.
    PoseHistory mixed, coherent;
    auto& mf=mixed.acquire(58000,0,1000);mixed.commit(mf,0,{1,0,0},2.6f);
    auto& cf=coherent.acquire(58000,0,1000);coherent.commit(cf,0,{1,0,0},2.6f);
    auto& mb=mixed.acquire(88000,.1f,1008);mixed.commit(mb,0,{1,0,0},2.7f);
    auto& cb=coherent.acquire(58001,.1f,1008);coherent.commit(cb,0,{1,0,0},2.7f);
    check(!mixed.acquire(58002,.11f,1016).fresh,"old mixed-counter control loses history after a fallback");
    const auto& current=coherent.acquire(58002,.11f,1016);
    check(current.fresh&&current.twist[0]==2.7f&&current.yaw<.02f,"matched-fallback-matched locate sequence retains twist and body yaw");
    const auto& otherEye=coherent.acquire(58002,.12f,1017);
    check(otherEye.twist[0]==current.twist[0]&&coherent.reused==1&&coherent.old==0,"two eyes share one locate history despite different publication counters");
    Mat3 world=axis_angle({0,0,1},.4f),local=axis_angle({1,0,0},-.9f),flip=dvr::hf::identity3();flip.m[8]=-1;
    for(int i=0;i<20;++i){
        Mat3 head=axis_angle({1,2,3},i*.2f);
        Mat3 view=dvr::hf::mul3(world,dvr::hf::mul3(head,flip));
        Mat3 bridge=tracking_to_local(local,view,head);
        check(near(rotate(bridge,{2,3,4}),rotate(dvr::hf::mul3(dvr::hf::transpose3(local),world),{2,3,4})),"tracking bridge cancels head pitch roll and yaw exactly once");
    }

    dvr::capture::Burst burst;
    check(burst.start(1000)&&!burst.start(1001),"capture ignores a duplicate request during the countdown");
    check(!burst.ready(5999,1,true,true)&&burst.captured==0,"capture waits five seconds");
    check(!burst.ready(6000,0,true,true),"capture refuses absent image identity");
    check(!burst.ready(6000,1,true,false)&&burst.skipped==1,"capture backpressure records a skipped source frame");
    check(!burst.ready(6001,1,true,true),"capture cannot relabel a repeated source as a new frame");
    for(unsigned i=0;i<16;++i){check(burst.ready(6002+i,i+2,true,true),"capture accepts new source image");burst.queued(true);}
    check(burst.phase==dvr::capture::Burst::Saving&&!burst.ready(6100,99,true,true),"capture ends at sixteen and waits for files");
    burst.saved(true,false);check(burst.phase==dvr::capture::Burst::Saving,"pending disk jobs are not success");
    burst.saved(false,false);check(burst.phase==dvr::capture::Burst::Done,"all saved files finish capture");
    check(burst.start(7000),"completed capture can be armed again");
    burst.ready(27001,99,true,true);check(burst.phase==dvr::capture::Burst::Failed,"missing frames time out instead of leaving capture armed forever");
    burst.start(30000);burst.ready(35000,100,true,true);burst.queued(false);
    check(burst.phase==dvr::capture::Burst::Failed,"readback or metadata write failure never reports done");
    burst.start(40000);burst.saved(false,true);check(burst.phase==dvr::capture::Burst::Failed,"worker write failure reaches capture status");
    Rig rig;rig.bones.resize(3);rig.vertices.resize(3);rig.triangles=1;
    Vertex runtime[3]{};
    for(int i=0;i<3;++i){
        rig.vertices[i].p={(float)i,0,0};rig.vertices[i].bone[0]=i;rig.vertices[i].weight[0]=1;
        runtime[i]=rig.vertices[i];runtime[i].bone[0]=(i+1)%3;
    }
    Mapping map;
    check(map_skin(rig,runtime,3,3,map)&&map.reference[1]==0&&map.reference[2]==1&&map.reference[0]==2,"map shuffled palette from all weight fields");
    runtime[0].p.y=1;
    check(!map_skin(rig,runtime,3,3,map),"foreign geometry refused");check(map.failedVertex==0&&map.nearestDistance>=1,"mapping refusal identifies mismatched vertex and distance");runtime[0].p.y=0;
    runtime[0].bone[0]=2;
    check(!map_skin(rig,runtime,3,3,map),"wrong skin weights refused");
    if(argc>1){
        FILE* f=nullptr;fopen_s(&f,argv[1],"rb");Rig real;
        bool loaded=real.load(f);if(f)fclose(f);check(loaded,"local prepared reference parses");
        if(loaded){
            // A synthetic runtime expansion models UV seams and shuffled slots,
            // quantizing weights exactly as a packed four-byte vertex does.
            std::vector<Vertex> expanded;
            int refToSlot[128];std::fill(refToSlot,refToSlot+128,-1);int slots=0;
            for(const auto& v:real.vertices)for(int k=0;k<4;++k)if(v.weight[k]>0&&refToSlot[v.bone[k]]<0)refToSlot[v.bone[k]]=slots++;
            for(size_t i=0;i<real.vertices.size();++i){
                Vertex v=real.vertices[i];
                for(int k=0;k<4;++k)if(v.weight[k]>0){v.bone[k]=slots-1-refToSlot[v.bone[k]];v.weight[k]=roundf(v.weight[k]*255)/255;}
                expanded.push_back(v);if(i%4==0)expanded.push_back(v);
            }
            check(map_skin(real,expanded.data(),(unsigned)expanded.size(),slots,map),"full local rig mapping with seams and quantization");
            printf("Mapping: %s\n",map.why);
            bool correct=true;for(size_t b=0;b<real.bones.size();++b)if(refToSlot[b]>=0&&map.reference[slots-1-refToSlot[b]]!=(int)b)correct=false;
            check(correct,"all active local reference bones map exactly");
            // Reproduce an unconverted PSK export against engine coordinates.
            // The old same-space fixture could never expose this boundary bug.
            Rig exported=real;
            for(auto& v:exported.vertices)v.p.y=-v.p.y;
            for(auto& b:exported.bones)b.head.y=-b.head.y;
            bool asymmetric=false;for(const auto& v:real.vertices)if(fabsf(v.p.y)>1)asymmetric=true;
            Mapping reflectedMap;
            if(asymmetric)check(!map_skin(exported,expanded.data(),(unsigned)expanded.size(),slots,reflectedMap),"unconverted PSK Y reflection is refused");
            FILE* legacy=nullptr;tmpfile_s(&legacy);
            if(legacy){fwrite("DVRIK001",1,8,legacy);rewind(legacy);Rig stale;check(!stale.load(legacy),"old PSK-space reference version is rejected");fclose(legacy);}

            printf("Local fixture: %zu points, %d active slots, matched %u, max weight error %.7f\n",real.vertices.size(),slots,map.matched,map.worstWeight);
        }
    }
    printf("arm IK: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
