// Bounded, mod-owned reference data and runtime skin correspondence. No engine
// offsets or game-derived tables are compiled into the proxy.
#pragma once
#include "arm_ik.h"
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdint>

namespace dvr::ik {
struct Bone {char name[64];int32_t parent;Vec head;};
struct Vertex {Vec p;int32_t bone[4];float weight[4];};
static_assert(sizeof(Bone)==80 && sizeof(Vertex)==44,"IK reference disk layout");
struct Rig {
    std::vector<Bone> bones;
    std::vector<Vertex> vertices;
    unsigned triangles=0;
    int find(const char* name)const{
        for(size_t i=0;i<bones.size();++i)if(!strcmp(bones[i].name,name))return (int)i;
        return -1;
    }
    bool descendant(int b,int root)const{
        for(size_t n=0;n<bones.size()&&b>=0;++n){if(b==root)return true;b=bones[b].parent;}
        return false;
    }
    bool load(FILE* f){
        bones.clear();vertices.clear();triangles=0;
        char magic[8];uint32_t counts[3];
        if(!f||fread(magic,1,8,f)!=8||memcmp(magic,"DVRIK002",8)||fread(counts,4,3,f)!=3)return false;
        if(counts[0]<3||counts[0]>128||counts[1]<3||counts[1]>8192||counts[2]<1||counts[2]>16384)return false;
        bones.resize(counts[0]);vertices.resize(counts[1]);triangles=counts[2];
        if(fread(bones.data(),sizeof(Bone),bones.size(),f)!=bones.size()||
           fread(vertices.data(),sizeof(Vertex),vertices.size(),f)!=vertices.size()||fgetc(f)!=EOF)return false;
        for(size_t i=0;i<bones.size();++i){
            const auto& b=bones[i];
            if(!memchr(b.name,0,64)||!b.name[0]||!finite(b.head)||b.parent< -1||b.parent>=(int)bones.size())return false;
            int p=(int)i;
            for(size_t n=0;p>=0;++n){if(n>=bones.size())return false;p=bones[p].parent;}
            for(size_t j=0;j<i;++j)if(!strcmp(b.name,bones[j].name))return false;
        }
        for(const auto& v:vertices){
            if(!finite(v.p))return false;float sum=0;
            for(int j=0;j<4;++j){
                float w=v.weight[j];int b=v.bone[j];
                if(!std::isfinite(w)||w<0||w>1||b< -1||b>=(int)bones.size()||(w>0&&b<0))return false;
                if(w>0)for(int k=0;k<j;++k)if(v.weight[k]>0&&v.bone[k]==b)return false;
                sum+=w;
            }
            if(fabsf(sum-1)>.001f)return false;
        }
        return true;
    }
};
struct Mapping {
    int reference[128];
    unsigned matched=0;
    int failedVertex=-1;
    float nearestDistance=0;
    Vec failedPosition{};
    float worstPosition=0,worstWeight=0;
    const char* why="not mapped";
    Mapping(){std::fill(reference,reference+128,-1);}
};
struct Chain {
    int collar=-1,upper=-1,lower=-1,wrist=-1;
    // 0 unrelated, 1 upper/clavicle, 2 forearm/helper/sleeve, 3 native hand.
    unsigned char region[128]{};
    bool build(const Rig& rig,bool right){
        *this=Chain{};
        collar=rig.find(right?"Collarbone_R_Jnt":"Collarbone_L_Jnt");
        upper=rig.find(right?"upper_arm_R_jnt":"upper_arm_L_jnt");
        lower=rig.find(right?"lower_arm_R_jnt":"lower_arm_L_jnt");
        wrist=rig.find(right?"hand_R_jnt":"hand_L_jnt");
        if(collar<0||upper<0||lower<0||wrist<0||!rig.descendant(upper,collar)||
           !rig.descendant(lower,upper)||!rig.descendant(wrist,lower))return false;
        if(length(rig.bones[lower].head-rig.bones[upper].head)<.001f||
           length(rig.bones[wrist].head-rig.bones[lower].head)<.001f)return false;
        for(size_t i=0;i<rig.bones.size();++i){
            if(rig.descendant((int)i,wrist))region[i]=3;
            else if(rig.descendant((int)i,lower))region[i]=2;
            else if(rig.descendant((int)i,collar))region[i]=1;
        }
        return true;
    }
};
struct ArmPose {
    Xform skin[128]{};
    Solution joints;
    float trackedTwist=0;
};
inline Mat3 length_basis(Vec direction,float scale,float lengthScale){
    unit(direction);float v[3]={direction.x,direction.y,direction.z};Mat3 m=hf::identity3();
    for(int r=0;r<3;++r)for(int c=0;c<3;++c)m.m[r*3+c]=scale*(m.m[r*3+c]+(lengthScale-1)*v[r]*v[c]);
    return m;
}
// Hand matrices are left to the caller. Both the runtime and Blender sweep use
// this function to build the arm skin matrices and meet that exact wrist.
inline bool pose_arm(const Rig& rig,const Chain& chain,Vec nominal,Vec pole,Vec outward,
                     Vec priorPole,float priorTwist,bool fresh,const Xform& wristSkin,
                     float lengthScale,float margin,ArmPose& result){
    if(chain.upper<0||chain.wrist<0||!std::isfinite(lengthScale)||lengthScale<.5f||lengthScale>2)return false;
    hf::ScaledRot wr;
    if(!hf::decompose_scaled_rotation(wristSkin.r,.06f,.06f,&wr))return false;
    const Vec s0=rig.bones[chain.upper].head,e0=rig.bones[chain.lower].head,w0=rig.bones[chain.wrist].head;
    Vec ue=e0-s0,ew=w0-e0,refNormal=cross(ue,ew);if(!unit(refNormal))refNormal=fallback(ue);
    Vec wrist=point(wristSkin,w0);
    const float a=length(ue)*wr.scale*lengthScale,b=length(ew)*wr.scale*lengthScale;
    Solution sol;Mat3 up,fore;Vec axis;
    auto attempt=[&](Vec p){
        if(!solve(nominal,wrist,p,outward,priorPole,a,b,margin,sol))return false;
        Vec normal=cross(sol.pole,wrist-sol.shoulder);if(!unit(normal))return false;
        up=frame_delta(ue,refNormal,sol.elbow-sol.shoulder,normal);
        fore=frame_delta(ew,refNormal,wrist-sol.elbow,normal);
        axis=wrist-sol.elbow;return unit(axis);
    };
    if(!attempt(pole))return false;
    float roll=twist(fore,wr.r,axis);
    constexpr float rad=3.14159265359f/180;
    if(fresh&&fabsf(roll)>110*rad)roll=unwrap(roll,priorTwist);
    roll=std::clamp(roll,-250*rad,250*rad);result.trackedTwist=roll;
    Vec basePole=sol.pole;
    if(fabsf(roll)>80*rad){
        float swivel=std::clamp(roll-std::copysign(80*rad,roll),-70*rad,70*rad);
        Vec sw=wrist-sol.shoulder;unit(sw);
        if(!attempt(rotate(axis_angle(sw,swivel),basePole)))return false;
        roll=unwrap(twist(fore,wr.r,axis),roll-swivel);
    }
    // Store the pre-swivel pole so wrist-driven elbow lift cannot feed back.
    result.joints=sol;result.joints.pole=basePole;
    Mat3 upperScale=length_basis(ue,wr.scale,lengthScale),foreScale=length_basis(ew,wr.scale,lengthScale);
    Mat3 upperM=hf::mul3(up,upperScale),foreM=hf::mul3(fore,foreScale);
    for(size_t i=0;i<rig.bones.size();++i){
        if(chain.region[i]==0||chain.region[i]==3)continue;
        Xform m{};
        if(chain.region[i]==1){m.r=upperM;put(sol.shoulder-rotate(m.r,s0),m.t);}
        else {
            float fraction=std::clamp(dot(rig.bones[i].head-e0,ew)/dot(ew,ew),0.f,1.f);
            // Authored lower-arm/sleeve weights overlap across most of the
            // forearm, not just at each helper head. Leaving the lower arm at
            // zero roll made a 50/50 blend differ by 127 degrees and shrink to
            // 45% radius. Share 70% of the axial roll at the elbow, then ramp to
            // the wrist. The elbow/wrist joint positions remain exact.
            float twistFraction=.7f+.3f*fraction;
            Mat3 turn=axis_angle(axis,roll*twistFraction);
            m.r=hf::mul3(turn,foreM);
            Vec target=sol.elbow+rotate(foreM,rig.bones[i].head-e0);
            put(target-rotate(m.r,rig.bones[i].head),m.t);
        }
        result.skin[i]=m;
    }
    return true;
}
// ArmIKGameArmShoulder: the game's own arm, moved so its shoulder sits on the IK shoulder while
// its wrist stays exactly where the game put it. The game draws its arm for its own camera and
// body; the IK shoulder hangs from the tracked head, so the two shoulders differ and the game's
// arm showed its shoulder out in front (run 3, 2026-10-04). One transform for the whole arm,
// about the wrist: a stretch along the shoulder-wrist line (so the cross-section at the wrist
// keeps meeting the hand), then the smallest rotation that points that line at the IK shoulder.
// The elbow's bend and side are the game's. Stretch and angle are bounded; `residual` is what
// the bounds left between the two shoulders; `wanted` is the stretch it would have taken unbounded.
struct ShoulderFit {Xform move{};float angle=0,stretch=1,wanted=1,offset=0,residual=0;bool ok=false;};
inline ShoulderFit shoulder_fit(Vec nativeShoulder,Vec wrist,Vec target,float minStretch,float maxStretch,float maxAngle){
    ShoulderFit out;out.move.r=hf::identity3();
    Vec from=nativeShoulder-wrist,to=target-wrist;
    const float a=length(from),b=length(to);
    if(!finite(nativeShoulder)||!finite(wrist)||!finite(target)||!(a>1)||!(b>1))return out;
    out.offset=length(target-nativeShoulder);
    out.wanted=b/a;
    out.stretch=std::clamp(out.wanted,minStretch,maxStretch);
    Vec d=from*(1/a),e=to*(1/b);
    Vec axis=cross(d,e);const float sine=length(axis),cosine=std::clamp(dot(d,e),-1.f,1.f);
    out.angle=std::min(atan2f(sine,cosine),maxAngle);
    Mat3 turn=sine>1e-5f?axis_angle(axis,out.angle):hf::identity3();
    float v[3]={d.x,d.y,d.z};Mat3 grow=hf::identity3();
    for(int r=0;r<3;++r)for(int k=0;k<3;++k)grow.m[r*3+k]+=(out.stretch-1)*v[r]*v[k];
    out.move.r=hf::mul3(turn,grow);
    put(wrist-rotate(out.move.r,wrist),out.move.t);
    out.residual=length(point(out.move,nativeShoulder)-target);
    out.ok=true;return out;
}
// Every vertex must match a reference position, then every active slot must
// match ONE unique reference weight field over the ENTIRE vertex population.
// This catches shuffled palettes, duplicate/ambiguous slots and foreign meshes.
inline bool map_skin(const Rig& rig,const Vertex* vertices,unsigned count,unsigned slots,Mapping& out){
    out=Mapping{};
    if(!vertices||count<3||count>8192||slots<1||slots>128||rig.bones.empty()||rig.vertices.empty())return false;
    const unsigned nb=(unsigned)rig.bones.size();
    std::vector<double> crossWeight(slots*nb,0),runtimeNorm(slots,0),referenceNorm(nb,0);
    for(unsigned v=0;v<count;++v){
        const auto& cur=vertices[v];
        if(!finite(cur.p)){out.why="non-finite runtime vertex";return false;}
        float closest=INFINITY;int nearest=-1;
        for(unsigned p=0;p<rig.vertices.size();++p){
            Vec delta=rig.vertices[p].p-cur.p;float ds=dot(delta,delta);
            if(ds<closest){closest=ds;nearest=(int)p;}
        }
        if(nearest<0||closest>=.03f*.03f){
            out.failedVertex=(int)v;out.failedPosition=cur.p;out.nearestDistance=sqrtf(closest);
            out.why="runtime position absent from local reference";return false;
        }
        out.worstPosition=std::max(out.worstPosition,sqrtf(closest));++out.matched;
        const auto& ref=rig.vertices[nearest];float sum=0;
        for(int i=0;i<4;++i){
            const float w=cur.weight[i];const int s=cur.bone[i];
            if(!std::isfinite(w)||w<0||w>1||(w>0&&(s<0||s>=(int)slots))){out.why="invalid runtime influence";return false;}
            sum+=w;if(w<=0)continue;
            runtimeNorm[s]+=w*w;
            for(int j=0;j<4;++j)if(ref.weight[j]>0)crossWeight[s*nb+ref.bone[j]]+=w*ref.weight[j];
        }
        if(fabsf(sum-1)>.02f){out.why="runtime weights do not sum to one";return false;}
        for(int j=0;j<4;++j)if(ref.weight[j]>0)referenceNorm[ref.bone[j]]+=ref.weight[j]*ref.weight[j];
    }
    bool used[128]{};
    for(unsigned s=0;s<slots;++s){
        if(runtimeNorm[s]<1e-8)continue;
        double best=1e30,second=1e30;int chosen=-1;
        for(unsigned b=0;b<nb;++b){
            const double error=std::max(0.,runtimeNorm[s]+referenceNorm[b]-2*crossWeight[s*nb+b]);
            const double relative=error/std::max(1e-8,runtimeNorm[s]);
            if(relative<best){second=best;best=relative;chosen=(int)b;}else second=std::min(second,relative);
        }
        if(chosen<0||best>.002||second-best<.01||used[chosen]){out.why="ambiguous or mismatched palette weight field";return false;}
        used[chosen]=true;out.reference[s]=chosen;out.worstWeight=std::max(out.worstWeight,(float)best);
    }
    out.why="validated vertex positions and skin weights";return true;
}
} // namespace dvr::ik
