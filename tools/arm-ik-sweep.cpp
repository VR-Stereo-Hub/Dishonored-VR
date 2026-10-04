// Exports the production solver's skin matrices for an offline Blender sweep.
// Output includes local game-derived rig data and must never be committed.
#include "game/dishonored/hands/arm_rig.h"
#include <cstdio>
using namespace dvr::ik;
struct Pose {const char* name;Vec wrist[2];float roll=0,length=1;bool fingers=false;float scale=1;};
int main(int argc,char** argv){
    if(argc!=3){puts("usage: arm-ik-sweep reference.bin sweep.json");return 2;}
    FILE* f=nullptr;fopen_s(&f,argv[1],"rb");Rig rig;bool ok=rig.load(f);if(f)fclose(f);if(!ok)return 3;
    Chain chains[2];if(!chains[0].build(rig,false)||!chains[1].build(rig,true))return 4;
    Pose poses[]={
        {"Relaxed",{{24,104,20},{-24,104,20}}},
        {"Forward",{{18,140,40},{-18,140,40}}},
        {"Extended",{{18,146,90},{-18,146,90}}},
        {"Close",{{18,146,-2},{-18,146,-2}}},
        {"Crossed",{{-20,132,26},{20,132,26}}},
        {"Raised",{{20,205,15},{-20,205,15}}},
        {"Wide",{{85,142,0},{-85,142,0}}},
        {"Down",{{20,85,0},{-20,85,0}}},
        {"Behind",{{18,130,-50},{-18,130,-50}}},
        {"Asymmetric",{{24,104,20},{-20,205,15}}},
        {"Roll 0",{{18,140,40},{-18,140,40}},0},
        {"Roll 45",{{18,140,40},{-18,140,40}},45},
        {"Roll 90",{{18,140,40},{-18,140,40}},90},
        {"Roll 135",{{18,140,40},{-18,140,40}},135},
        {"Roll 179",{{18,140,40},{-18,140,40}},179},
        {"Roll 181",{{18,140,40},{-18,140,40}},181},
        {"Roll 225",{{18,140,40},{-18,140,40}},225},
        {"Roll 270",{{18,140,40},{-18,140,40}},270},
        {"Roll 315",{{18,140,40},{-18,140,40}},315},
        {"Roll 360",{{18,140,40},{-18,140,40}},360},
        {"Finger animation",{{18,140,40},{-18,140,40}},360,1,true},
        {"Shorter arms",{{18,140,40},{-18,140,40}},360,.75f},
        {"Longer arms",{{18,140,40},{-18,140,40}},360,1.25f},
        {"Shoulder singularity",{{18,146,-6},{-18,146,-6}}},
        {"Installed hand size 0.85",{{18,140,40},{-18,140,40}},0,1,false,.85f},
        {"Independent far reach",{{18,146,90},{-18,140,40}},0,1,false,.85f},
    };
    FILE* out=nullptr;fopen_s(&out,argv[2],"wb");if(!out)return 5;
    fprintf(out,"{\"bones\":[");
    for(size_t i=0;i<rig.bones.size();++i)fprintf(out,"%s\"%s\"",i?",":"",rig.bones[i].name);
    fprintf(out,"],\"frames\":[\n");
    Body body{{0,166,0},{0,0,1},{-1,0,0},{0,1,0}};Settings settings;
    Vec prior[2]{};float lastTwist[2]{};unsigned frame=0;int failures=0;
    float worstLength=0,worstJoin=0;Vec oldStationary{};bool stationaryOk=true;
    for(size_t pi=0;pi<sizeof(poses)/sizeof(poses[0]);++pi){
        const Pose& current=poses[pi];const Pose& before=pi?poses[pi-1]:current;
        for(int sub=1;sub<=10;++sub){
            float t=sub/10.f;Xform palette[128];
            for(auto& m:palette){m.r=dvr::hf::identity3();m.t[0]=m.t[1]=m.t[2]=0;}
            ArmPose arms[2];Vec targets[2];float roll=before.roll+(current.roll-before.roll)*t;
            float armLength=before.length+(current.length-before.length)*t;
            float handScale=before.scale+(current.scale-before.scale)*t;
            for(int h=0;h<2;++h){
                const Chain& c=chains[h];targets[h]=before.wrist[h]+(current.wrist[h]-before.wrist[h])*t;
                Vec e0=rig.bones[c.lower].head,w0=rig.bones[c.wrist].head;
                int finger=rig.find(h?"middle_0_R_jnt":"middle_0_L_jnt");
                Vec handDir=rig.bones[finger].head-w0;
                Vec refNormal=cross(handDir,{h?-1.f:1.f,0,0});
                Mat3 handRotation=frame_delta(handDir,refNormal,{0,0,1},{0,1,0});
                handRotation=dvr::hf::mul3(axis_angle({0,0,1},(h?-1:1)*roll/57.2957795f),handRotation);
                Xform hand=skin(handRotation,handScale,w0,targets[h]);
                Vec pole=body.up*-1+body.right*((h?1.f:-1.f)*settings.elbowOut)+body.forward*-.3f;
                if(!pose_arm(rig,c,shoulder(body,settings,h,1),pole,body.right*(h?1.f:-1.f),prior[h],lastTwist[h],frame>0,hand,armLength,.5f,arms[h])){++failures;continue;}
                prior[h]=arms[h].joints.pole;lastTwist[h]=arms[h].trackedTwist;
                float a=length(e0-rig.bones[c.upper].head)*armLength*handScale,b=length(w0-e0)*armLength*handScale;
                worstLength=std::max(worstLength,std::max(fabsf(length(arms[h].joints.elbow-arms[h].joints.shoulder)-a),fabsf(length(targets[h]-arms[h].joints.elbow)-b)));
                worstJoin=std::max(worstJoin,length(point(arms[h].skin[c.lower],w0)-targets[h]));
                for(size_t bone=0;bone<rig.bones.size();++bone){
                    if(c.region[bone]==3){
                        palette[bone]=hand;
                        // A synthetic knuckle clip affects descendants only.
                        if(current.fingers&&rig.descendant((int)bone,finger)){
                            Xform local=skin(axis_angle({1,0,0},.65f*t),1,rig.bones[finger].head,rig.bones[finger].head);
                            palette[bone]=dvr::hf::xform_mul(hand,local);
                        }
                    }else if(c.region[bone])palette[bone]=arms[h].skin[bone];
                }
            }
            // Same wrist/roll through finger-only clip: arm pose must stay still.
            if(current.fingers){if(sub>1&&length(oldStationary-arms[0].joints.elbow)>.05f)stationaryOk=false;oldStationary=arms[0].joints.elbow;}
            fprintf(out,"%s{\"frame\":%u,\"label\":\"%s\",\"key\":%s,\"joints\":[",frame?",\n":"",frame+1,current.name,sub==10?"true":"false");
            for(int h=0;h<2;++h){const auto& j=arms[h].joints;fprintf(out,"%s[[%.7g,%.7g,%.7g],[%.7g,%.7g,%.7g],[%.7g,%.7g,%.7g]]",h?",":"",j.shoulder.x,j.shoulder.y,j.shoulder.z,j.elbow.x,j.elbow.y,j.elbow.z,targets[h].x,targets[h].y,targets[h].z);}
            fprintf(out,"],\"skin\":[");
            for(size_t i=0;i<rig.bones.size();++i){
                const auto& m=palette[i];fprintf(out,"%s[",i?",":"");
                for(int r=0;r<3;++r){for(int c=0;c<3;++c)fprintf(out,"%s%.8g",r||c?",":"",m.r.m[r*3+c]);fprintf(out,",%.8g",m.t[r]);}fprintf(out,"]");
            }
            fprintf(out,"]}");++frame;
        }
    }
    fprintf(out,"],\"validation\":{\"frames\":%u,\"solveFailures\":%d,\"maxSegmentError\":%.8g,\"maxWristJoinError\":%.8g,\"fingerOnlyArmStable\":%s}}\n",frame,failures,worstLength,worstJoin,stationaryOk?"true":"false");
    fclose(out);
    printf("IK sweep: %u frames, %d solve failures, segment error %.7f, wrist join %.7f, finger-only arm stable %s\n",frame,failures,worstLength,worstJoin,stationaryOk?"yes":"NO");
    return failures||worstLength>.01f||worstJoin>.01f||!stationaryOk?1:0;
}
