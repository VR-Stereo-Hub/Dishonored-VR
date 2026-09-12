#include "game/dishonored/hands/bolt_axis.h"
#include "game/dishonored/hands/weapon_frame.h"
#include "game/dishonored/fire_aim_math.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    // VR-57: the CANDIDATE selection. Mirrors BrIsLoadedProjectile exactly; if the
    // two drift this test is worthless, so it is kept adjacent to it in review.
    // The names are the ones the renderer actually reported in the confirmed run.
    {
        auto candidate=[](const char* a)->bool{
            if(!a||!*a)return false;
            if(!_stricmp(a,"bolt_01"))return true;
            if(!_strnicmp(a,"Bolt",4))return true;
            if(std::strstr(a,"bullet")||std::strstr(a,"Bullet"))return true;
            return false;
        };
        // accepted: every loaded projectile seen in the run
        check(candidate("bolt_01"));
        check(candidate("Bolt_Flare"));
        check(candidate("Gun_bullet_regular"));
        // REFUSED: the weapon bodies. This is the row that matters - aiming from a
        // crossbow body would point the shot along its bow arms, across the barrel.
        check(!candidate("crossbow_01"));
        check(!candidate("Wpn_PlyGunElite"));
        check(!candidate("Wpn_PlySword01"));
        check(!candidate("EliteGun"));
        check(!candidate("Skm_Player"));
        check(!candidate(""));
        check(!candidate(nullptr));
    }

    // VR-57: the projectile must belong to the EQUIPPED weapon. Mirrors
    // BrProjectileMatchesWeapon. The crossbow was perfect until the pistol was
    // equipped, at which point a bolt draw was measured and stored as the pistol's
    // axis and signed against the pistol's forward - mirroring both weapons. The
    // asserted row is the one that actually happened: bolt_01 against EliteGun.
    {
        auto pair=[](const char* proj,const char* weapon)->bool{
            if(!proj||!*proj||!weapon||!*weapon)return false;
            const bool pb=!_stricmp(proj,"bolt_01")||!_strnicmp(proj,"Bolt",4);
            const bool pu=std::strstr(proj,"bullet")||std::strstr(proj,"Bullet");
            const bool wx=std::strstr(weapon,"crossbow")||std::strstr(weapon,"Crossbow");
            const bool wg=std::strstr(weapon,"Gun")||std::strstr(weapon,"gun")||std::strstr(weapon,"Elite");
            if(pb)return wx;
            if(pu)return wg;
            return false;
        };
        // the pairings that are real
        check(pair("bolt_01","crossbow_01"));
        check(pair("Bolt_Flare","crossbow_01"));
        check(pair("Gun_bullet_regular","Wpn_PlyGunElite"));
        check(pair("Gun_bullet_regular","EliteGun"));
        // THE BUG: a bolt must never be measured under a gun, or either way round
        check(!pair("bolt_01","EliteGun"));
        check(!pair("bolt_01","Wpn_PlyGunElite"));
        check(!pair("Bolt_Flare","EliteGun"));
        check(!pair("Gun_bullet_regular","crossbow_01"));
        // an unknown or absent weapon refuses rather than adopting under nothing
        check(!pair("bolt_01",""));
        check(!pair("bolt_01",nullptr));
        check(!pair("bolt_01","Wpn_PlySword01"));
        check(!pair("bolt_01","Skm_Player"));
    }

    for(int j=0;j<128;++j)for(int i=0;i<3;++i)points[j][i]=(j&(1<<i))?1.0f:-1.0f;
    check(!bolt_axis(points,128,a)); // wide/ambiguous geometry must never aim
    points[0][0]=std::numeric_limits<float>::quiet_NaN();check(!bolt_axis(points,128,a));
    check(!bolt_axis(points,2,a));
    std::printf("PASS %d loaded-bolt geometry, parity, palm and native convergence checks\n",checks);
}
