#include "game/dishonored/hands/bolt_axis.h"
#include "game/dishonored/hands/weapon_frame.h"
#include "game/dishonored/fire_aim_math.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
static int checks=0;
static void check(bool b){++checks;if(!b){std::printf("FAIL %d\n",checks);std::exit(1);}}
int main(){
    using namespace dvr::hf;
    float points[128][3];
    for(int j=0;j<128;++j){points[j][0]=(j/8)*2.0f;points[j][1]=0.4f*std::cos(j*0.78539816f);points[j][2]=0.4f*std::sin(j*0.78539816f);}
    BoltAxis a;check(bolt_axis(points,128,a));check(std::fabs(a.dir[0])>0.99999f);check(a.ratio>100);
    float tip[3],dir[3];bolt_tip(a,1,tip,dir);check(std::fabs(tip[0]-30)<0.001f);
    bolt_tip(a,-1,tip,dir);check(std::fabs(tip[0])<0.001f&&dir[0]<-0.999f);
    for(int parity=-1;parity<=1;parity+=2)for(int angle=-170;angle<180;angle+=17){
        const Mat3 rc=euler_xyz_deg_to_mat(angle,angle*.3f,-angle*.7f);
        const Mat3 grip=mul3(euler_xyz_deg_to_mat(31,53,-21),parity_factor(parity));
        const float trim[3]={float(angle),-83,62},tm[3]={-.08f,.12f,-.03f},p0[3]={-.2f,1.4f,-.4f};
        const float local[3]={.4f,-.1f,.03f},ld[3]={1,0,0};float p[3],d[3];
        check(palm_ray_to_xr(rc,grip,p0,trim,tm,local,ld,p,d));
        // Independent reference: build in a reflected camera basis, then undo
        // the basis. Production stays in XR and never receives this M.
        const Mat3 m=mul3(euler_xyz_deg_to_mat(-22,37,4),parity_factor(-1));
        float cp[3];mulv3(m,p0,cp);
        const auto palm=palm_target(mul3(m,rc),grip,cp,euler_xyz_deg_to_mat(trim[0],trim[1],trim[2]),tm);
        float q[3],r[3],dq[3];mulv3(palm.r,local,q);for(int i=0;i<3;++i)q[i]+=palm.t[i];
        mulv3(transpose3(m),q,r);mulv3(palm.r,ld,q);mulv3(transpose3(m),q,dq);
        for(int i=0;i<3;++i){check(std::fabs(p[i]-r[i])<1e-5f);check(std::fabs(d[i]-dq[i])<1e-5f);}
        // Model->corrected draw->palm roundtrip includes unrelated component and
        // skin transforms. This catches using raw palette/model axes directly.
        Xform inv;check(dvr::wf::inverse(palm,&inv));
        float recovered[3];mulv3(inv.r,r,recovered); // use actual camera point below
        mulv3(m,r,q);mulv3(inv.r,q,recovered);
        for(int i=0;i<3;++i)check(std::fabs(recovered[i]+inv.t[i]-local[i])<1e-5f);
        // The same geometry endpoint is reachable by the native muzzle solve.
        dvr::aim::FireFrame f;f.ray.ok=true;f.ray.gen=9;f.ray.sampleMs=1000;f.headValid=true;f.headQuat[3]=1;f.distanceM=8;
        for(int i=0;i<3;++i){f.ray.originXr[i]=p[i];f.ray.dirXr[i]=d[i];f.headPos[i]=p0[i];}
        const float camera[3]={0,0,0},spawn[3]={10,20,-5};dvr::fireaim::Solution sol;
        check(dvr::fireaim::solve(f,1010,0,0,camera,108,spawn,sol));
        float length=0;for(int i=0;i<3;++i)length+=(sol.target[i]-spawn[i])*(sol.target[i]-spawn[i]);length=std::sqrt(length);
        for(int i=0;i<3;++i)check(std::fabs(spawn[i]+length*sol.direction[i]-sol.target[i])<0.001f);
    }
    for(int j=0;j<128;++j)for(int i=0;i<3;++i)points[j][i]=(j&(1<<i))?1.0f:-1.0f;
    check(!bolt_axis(points,128,a)); // wide/ambiguous geometry must never aim
    points[0][0]=std::numeric_limits<float>::quiet_NaN();check(!bolt_axis(points,128,a));
    check(!bolt_axis(points,2,a));
    std::printf("PASS %d loaded-bolt geometry, parity, palm and native convergence checks\n",checks);
}
