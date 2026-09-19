// Standalone cinematic rotation checks. Never loads or launches the game.
#include "../src/game/dishonored/cinematic_math.h"
#include "../src/game/dishonored/cinematic_policy.h"
#include "../src/game/dishonored/positional_math.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double unit = 2.0 * pi / 65536.0;
int failures = 0;
void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}
bool close(const dvr::cine::Matrix& a, const dvr::cine::Matrix& b,
           double tolerance = 1e-8) {
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            if (!std::isfinite(a.m[r][c]) ||
                std::fabs(a.m[r][c] - b.m[r][c]) > tolerance) return false;
    return true;
}
dvr::cine::Matrix decoded(const int32_t angles[3]) {
    return dvr::cine::rotation(angles[0] * unit, angles[1] * unit, angles[2] * unit);
}
}

int main() {
    using namespace dvr::cine;
    const Matrix identity = {{{1,0,0},{0,1,0},{0,0,1}}};
    // Explicit UE axes, independent of the implementation's trig formulas.
    const Matrix yaw90 = {{{0,-1,0},{1,0,0},{0,0,1}}};
    const Matrix pitch90 = {{{0,0,-1},{0,1,0},{1,0,0}}};
    const Matrix roll90 = {{{1,0,0},{0,0,1},{0,-1,0}}};
    check(close(rotation(0,0,0), identity), "zero rotation is identity");
    check(close(rotation(0,pi/2,0), yaw90), "positive yaw turns forward to right");
    check(close(rotation(pi/2,0,0), pitch90), "positive pitch lifts forward");
    check(close(rotation(0,0,pi/2), roll90), "positive roll tips right down");
    const Matrix arbitrary = rotation(0.31,-1.27,0.48);
    check(close(multiply(transpose(arbitrary), arbitrary), identity),
          "transpose is inverse for arbitrary reference");

    const int32_t authored[3] = {1900,17000,-5300};
    int32_t out[3] = {};
    Matrix basis = {};
    check(compose(authored, arbitrary, arbitrary, out, &basis) &&
          close(basis, decoded(authored)) && close(decoded(out), basis, 0.0002),
          "neutral head at arbitrary reference preserves authored orientation");

    const int32_t authoredRoll[3] = {0,0,16384};
    // Authored roll applied after local head pitch maps forward to +Y.
    const Matrix rollThenPitch = {{{0,0,-1},{1,0,0},{0,-1,0}}};
    const Matrix wrongOrder = {{{0,1,0},{0,0,1},{1,0,0}}};
    check(compose(authoredRoll, identity, pitch90, out, &basis) &&
          close(basis, rollThenPitch) && !close(basis, wrongOrder) &&
          close(decoded(out), basis, 0.0002),
          "authored roll plus local head pitch preserves noncommuting order");

    int32_t first[3] = {};
    Matrix firstBasis = {};
    const Matrix head = rotation(-0.23,0.72,0.11);
    bool stable = compose(authored, arbitrary, head, first, &firstBasis);
    for (int i = 0; i < 1000; ++i) {
        stable = compose(authored, arbitrary, head, out, &basis) && stable;
        stable = stable && out[0] == first[0] && out[1] == first[1] &&
                 out[2] == first[2] && close(basis, firstBasis);
    }
    check(stable, "repeated composition from authored input does not accumulate");

    const int32_t wrap[3] = {0,32760,0};
    const Matrix yawStep = rotation(0,32*unit,0);
    const Matrix expectedWrap = rotation(0,-32744*unit,0);
    check(compose(wrap, identity, yawStep, out, &basis) &&
          close(basis, expectedWrap) && close(decoded(out), expectedWrap, 0.0002),
          "yaw crossing signed rotator boundary keeps orientation continuous");

    bool poles = true;
    const int32_t zero[3] = {};
    for (double pitch : {pi/2-1e-7, pi/2, -pi/2+1e-7, -pi/2}) {
        const Matrix pole = rotation(pitch,0.61,-0.39);
        poles = compose(zero, identity, pole, out, &basis) && poles;
        poles = poles && close(basis,pole,0.0003) && close(decoded(out),pole,0.0003);
    }
    check(poles, "near vertical and exact pole outputs reconstruct full orientation");

    Matrix invalid = identity;
    invalid.m[1][2] = std::numeric_limits<double>::quiet_NaN();
    check(!compose(authored,identity,invalid,out,&basis), "NaN head is refused");
    check(!compose(authored,invalid,identity,out,&basis), "NaN reference is refused");
    const Conditions ready = {true,false,false,true,true,true,true,true,true};
    bool cadence = true;
    for (int i=0;i<64;++i) {
        // A single and a double draw both render the scene. Eye count is not
        // an ownership transition and deliberately is not a policy input.
        Conditions draw = ready;
        draw.sceneDraw=true;
        cadence = cadence && action(draw)==Action::Track;
    }
    check(cadence,"alternating single and double scene draws keep tracking");
    Conditions condition=ready; condition.sceneDraw=false;
    check(action(condition)==Action::Hold,"temporary missing scene draw holds reference");
    condition=ready;condition.runtimeReady=false;
    check(action(condition)==Action::Hold,"temporary runtime unavailability holds reference");
    condition=ready;condition.poseReady=false;
    check(action(condition)==Action::Hold,"temporary pose unavailability holds reference");
    condition=ready;condition.ownershipKnown=false;
    check(action(condition)==Action::Hold,"unknown ownership holds reference");
    condition=ready;condition.fullyAuthored=false;
    check(action(condition)==Action::Hold,"partial animation blend holds reference");
    condition=ready;condition.animationOwns=false;
    check(action(condition)==Action::Reset,"known non-animation ownership resets reference");
    condition=ready;condition.menu=true;condition.ownershipKnown=false;
    check(action(condition)==Action::Reset,"real menu resets despite unknown ownership");
    condition=ready;condition.ownerChanged=true;condition.poseReady=false;
    check(action(condition)==Action::Reset,"changed identity resets despite unavailable pose");
    condition=ready;condition.enabled=false;
    check(action(condition)==Action::Reset,"disabled tracking resets reference");

    int anchors=0;
    bool referenceValid=false;
    Matrix reference=identity;
    auto advance = [&](const Conditions& c,const Matrix& pose) {
        switch(action(c)) {
        case Action::Reset: referenceValid=false;return false;
        case Action::Hold: return false;
        case Action::Track:
            if (!referenceValid) {reference=pose;referenceValid=true;++anchors;}
            return compose(zero,reference,pose,out,&basis);
        }
        return false;
    };
    const Matrix initialPose=rotation(0.17,-0.22,0.04);
    bool preserved=advance(ready,initialPose);
    for (int i=0;i<32;++i) {
        const Matrix movingPose=rotation(0.25+0.012*i,0.13+0.019*i,-0.03);
        condition=ready;
        switch(i%5) {
        case 0:condition.sceneDraw=false;break;
        case 1:condition.runtimeReady=false;break;
        case 2:condition.poseReady=false;break;
        case 3:condition.ownershipKnown=false;break;
        case 4:condition.fullyAuthored=false;break;
        }
        const bool held=!advance(condition,movingPose);
        preserved=held && referenceValid && anchors==1 && preserved;
        const Matrix expected=multiply(transpose(initialPose),movingPose);
        preserved=advance(ready,movingPose) && preserved;
        preserved=preserved && anchors==1 && close(basis,expected,0.0003) && !close(basis,identity,0.05);
    }
    check(preserved && anchors==1,"32 temporary pauses retain one anchor and changing yaw/pitch offsets");
    condition=ready;condition.menu=true;
    const Matrix resumedPose=rotation(-0.4,0.9,0.1);
    const bool reset=!advance(condition,resumedPose) && !referenceValid;
    check(reset && advance(ready,resumedPose) && anchors==2 && close(basis,identity,0.0003),
          "real menu then resume captures a fresh physical reference");
    int32_t lockedPitch[3]={9000,12345,-2345};
    check(physical_pitch(lockedPitch,0) && lockedPitch[0]==0 && lockedPitch[1]==12345 && lockedPitch[2]==-2345,
          "forced pitch removed while authored yaw and roll remain");
    check(physical_pitch(lockedPitch,0.3) && std::abs(lockedPitch[0]-3129)<=1 && lockedPitch[1]==12345,
          "physical upward head tilt remains active");
    check(physical_pitch(lockedPitch,-0.3) && std::abs(lockedPitch[0]+3129)<=1,
          "physical downward head tilt remains active");
    check(physical_pitch(lockedPitch,2) && lockedPitch[0]==16000,
          "physical pitch matches gameplay clamp");
    check(owns_rotation(true,0,1,0),"scripted player influence uses final head scope");
    check(owns_rotation(true,0.4f,0.6f,0),"scripted blend retains the same head scope");
    check(!owns_rotation(false,0.4f,0.6f,0),"ordinary gameplay blend retains native head path");
    check(!owns_rotation(true,-1,0,1),"unknown influence refuses scripted head scope");
    // Regression: a left/right sweep while holding the Empress must not orbit
    // a tilted local axis. Expectations are independent Euler axis values.
    const int32_t steep[3]={-10518,10114,5936};
    bool sweep=true;
    for (double y : {-1.2,-0.6,0.0,0.6,1.2}) {
        sweep=comfort(steep,0.25,0.4,-0.2,-0.3,0.4+y,0.12,true,true,out,&basis) && sweep;
        sweep=sweep && std::abs(out[0]-(int32_t)std::lround(-0.3/unit))<=1 &&
            std::abs(out[1]-(int32_t)std::lround(10114+y/unit))<=1 &&
            std::abs(out[2]-(int32_t)std::lround(0.12/unit))<=1;
    }
    check(sweep,"steep authored pitch/roll cannot couple horizontal look into roll");
    const int32_t upright[3]={0,10114,0};
    int32_t levelOut[3]={}; Matrix levelBasis={};
    check(comfort(steep,0.25,0.4,-0.2,-0.3,1.0,0.12,true,true,out,&basis) &&
          comfort(upright,0.25,0.4,-0.2,-0.3,1.0,0.12,true,true,levelOut,&levelBasis) &&
          close(basis,levelBasis),"comfort ignores authored tilt even at a non-neutral entry pose");
    int32_t moved[3]={steep[0],steep[1]+2000,steep[2]};
    check(comfort(moved,0.25,0.4,-0.2,-0.3,1.0,0.12,true,true,out,&basis) &&
          out[1]==levelOut[1]+2000,"authored horizontal movement survives comfort composition");
    check(comfort(steep,0,0,0,0.1,0.2,0.3,true,false,out,&basis) &&
          std::abs(out[2]-(5936+(int32_t)std::lround(0.3/unit)))<=1,
          "pitch-only option preserves authored roll without mixing yaw axes");
    check(comfort(steep,0,0,0,0.1,0.2,0.3,false,true,out,&basis) &&
          std::abs(out[0]-(-10518+(int32_t)std::lround(0.1/unit)))<=1,
          "roll-only option preserves authored pitch");
    check(comfort(wrap,0,0,0,0,32*unit,0,true,true,out,&basis) &&
          close(basis,expectedWrap,0.0002),"upright comfort yaw crosses wrap continuously");
    check(!comfort(steep,0,0,0,0,std::numeric_limits<double>::quiet_NaN(),0,true,true,out,&basis),
          "comfort rejects invalid physical orientation");
    bool fixed=true,negative=false,reframed=true;
    const float start[3]={12,3,-21};
    for(int degrees=-180;degrees<=180;++degrees) {
        const float yaw=(float)(degrees*pi/180),heading=.7f;
        float relative[3],r[3],u[3],f[3],r0[3],u0[3],f0[3];
        dvr::position_math::reframe_yaw(start,0,yaw,relative);
        dvr::position_math::yaw_axes(heading+yaw,r,u,f);
        dvr::position_math::yaw_axes(heading,r0,u0,f0);
        float back[3];dvr::position_math::reframe_yaw(relative,yaw,0,back);
        for(int k=0;k<3;++k) {
            const float want=r0[k]*start[0]+u0[k]*start[1]+f0[k]*start[2];
            const float got=r[k]*relative[0]+u[k]*relative[1]+f[k]*relative[2];
            const float old=r0[k]*relative[0]+u0[k]*relative[1]+f0[k]*relative[2];
            fixed &= std::abs(got-want)<.00002f;
            negative |= std::abs(old-want)>10;
            reframed &= std::abs(back[k]-start[k])<.00002f;
        }
    }
    check(fixed,"361 physical yaw angles preserve fixed room position with composed camera heading");
    check(negative,"old native-matrix basis produces more than10uu false travel at fixed position");
    check(reframed,"menu entry neck correction round-trips yaw frames without positional drift");
    check(special_camera("StatePlayerMasterLeaning")==1 && special_camera("StatePlayerMasterHolePeeking")==2 &&
          special_camera("StatePlayerMasterWalk")==0 && special_camera(nullptr)==0,"only explicit lean/keyhole states claim special head look");
    int32_t carry=0;
    check(special_resume_delta(179*pi/180,-179*pi/180,carry) && std::abs(carry-364)<=1,"special exit yaw wraps across 180 degrees");
    check(!special_resume_delta(NAN,0,carry),"invalid exit reference refuses handoff");
    bool specialGood=true;
    for(int degrees=-100;degrees<=100;degrees+=5){
        const int32_t leaned[]={5000,12000,1820};
        const double angle=degrees*pi/180;
        comfort(leaned,0,0,0,.2,angle,-.1,true,true,out,&basis);
        specialGood &= std::abs(out[0]*unit-.2)<unit && std::abs(out[2]*unit+.1)<unit;
        special_resume_delta(0,angle,carry);
        specialGood &= std::abs(std::remainder((12000+carry-out[1])*unit,2*pi))<unit;
    }
    check(specialGood,"special gaze bypasses native pitch/roll and exit carries identical yaw");
    std::printf("Cinematic head math: %d failure(s)\n", failures);
    return failures ? 1 : 0;
}
