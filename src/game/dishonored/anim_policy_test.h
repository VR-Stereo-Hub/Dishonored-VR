#pragma once
#include "anim_policy.h"
#include <cstdio>
inline int AnimPolicyTests() {
    using namespace dvr::anim;
    int failures=0;
    auto check=[&](const char* label,bool ok) { std::printf("anim/%s %s\n",label,ok?"PASS":"FAIL"); if(!ok) ++failures; };
    check("list-trim",listed(" Walk, Choke ,Other","Choke"));
    check("exact-class",!listed("ChokeExtra,Choke","Chok"));
    check("empty-list",!listed(" , , ","Walk"));
    check("freshness-boundary",fresh(100,250) && !fresh(100,251) && !fresh(0,0) && !fresh(200,100));
    Handoff h;
    h.update(true,true,true,100,250,150);
    check("immediate-owner",h.game && h.value(100,150)==1);
    check("entry-midpoint",fabsf(h.value(175,150)-0.5f)<0.0001f);
    check("native-end",h.value(250,150)==0);
    h.update(true,false,true,300,250,150);
    h.update(true,false,true,549,250,150);
    check("hold-release",h.game && h.value(549,150)==0);
    h.update(true,false,true,550,250,150);
    check("exit-start-identity",!h.game && h.value(550,150)==0);
    check("exit-controller",h.value(700,150)==1);
    h.update(true,true,true,600,250,150);
    check("reentry-continuity",fabsf(h.value(600,150)-1.0f/3)<0.0001f);
    h.update(false,true,true,610,250,150);
    check("unknown-inert",!h.game && h.value(610,150)==1);
    h.update(true,true,false,620,250,150);
    check("disabled-inert",!h.game && h.value(620,150)==1);
    h.update(true,true,true,630,0,0);
    check("instant-entry",h.value(630,0)==0);
    h.update(true,false,true,640,0,0);
    check("instant-exit",!h.game && h.value(640,0)==1);
    dvr::hf::Xform t={dvr::hf::euler_xyz_deg_to_mat(0,0,180),{20,-10,4}};
    const auto zero=blend_transform(t,0),half=blend_transform(t,0.5f),one=blend_transform(t,1);
    check("identity-endpoint",zero.r.m[0]==1 && zero.t[0]==0);
    check("target-endpoint",one.r.m[0]==t.r.m[0] && one.t[0]==20);
    check("half-turn-finite",fabsf(half.r.m[0])<0.001f && dvr::hf::basis_is_orthonormal(half.r,0.001f));
    check("translation",half.t[0]==10 && half.t[1]==-5);
    for(float& v:t.r.m) v*=0.85f;
    const auto scaled=blend_transform(t,0.5f);
    const float length=sqrtf(scaled.r.m[0]*scaled.r.m[0]+scaled.r.m[3]*scaled.r.m[3]+scaled.r.m[6]*scaled.r.m[6]);
    check("model-scale",fabsf(length-0.925f)<0.001f);
    t.r.m[0]=99;
    const auto bad=blend_transform(t,0.5f);
    check("shear-refused",bad.r.m[0]==1 && bad.t[0]==0);
    // A weapon inherits the already-blended hand delta by conjugation. Applying
    // blend again in the weapon's local frame would violate this invariant.
    dvr::hf::Xform weapon={dvr::hf::identity3(),{10,5,2}};
    const auto local=dvr::hf::xform_mul(dvr::hf::xform_mul(dvr::hf::xform_inv(weapon),half),weapon);
    const auto world=dvr::hf::xform_mul(weapon,local);
    const auto expected=dvr::hf::xform_mul(half,weapon);
    check("shared-hand-weapon-delta",fabsf(world.t[0]-expected.t[0])<0.001f && fabsf(world.t[1]-expected.t[1])<0.001f);

    // ---- SmoothBlend --------------------------------------------------------------
    check("ease-endpoints",smootherstep(0)==0 && smootherstep(1)==1 && fabsf(smootherstep(0.5f)-0.5f)<1e-6f);
    check("ease-flat-ends",smootherstep(0.01f)<0.0001f && smootherstep(0.99f)>0.9999f);
    bool monotonic=true; float last=0;
    for(int i=1;i<=100;++i){const float v=smootherstep(i/100.0f); if(v<last) monotonic=false; last=v;}
    check("ease-monotonic",monotonic);
    Handoff s; s.smooth=true; s.inMs=200; s.outMs=400;
    s.update(true,true,true,1000,250,150);                       // blendMs ignored when smooth
    check("smooth-entry-start",s.value(1000,150)==1);
    check("smooth-entry-mid",fabsf(s.value(1100,150)-0.5f)<0.0001f);
    check("smooth-entry-slow-start",s.value(1010,150)>0.99f);     // linear would be 0.95
    check("smooth-entry-end",s.value(1200,150)==0);
    s.update(true,false,true,1300,0,150);                        // release, return begins
    check("smooth-exit-uses-out-ms",fabsf(s.value(1500,150)-0.5f)<0.0001f && s.value(1700,150)==1);
    s.update(true,true,true,1500,0,150);                         // reversal at the return's midpoint
    check("smooth-reversal-continuous",fabsf(s.value(1500,150)-0.5f)<0.0001f);
    check("smooth-reversal-share",s.value(1600,150)==0);          // 0.5 of the 200 ms entry
    s.update(false,true,true,1700,0,150);
    check("smooth-reset-keeps-shape",s.smooth && s.inMs==200 && s.outMs==400 && s.value(1700,150)==1);
    // The palm path: a correction that rotates 120 degrees about a far axis while moving.
    dvr::hf::Xform c={dvr::hf::euler_xyz_deg_to_mat(10,-35,120),{-30,55,12}};
    const float palm[3]={40,-12,25};
    float palmTarget[3]; dvr::hf::apply_point(c,palm,palmTarget);
    float worstNew=0, worstOld=0;
    for(int i=1;i<20;++i){
        const float w=i/20.0f; float line[3], pn[3], po[3];
        for(int k=0;k<3;++k) line[k]=palm[k]+(palmTarget[k]-palm[k])*w;
        dvr::hf::apply_point(blend_transform_palm(c,w,palm),palm,pn);
        dvr::hf::apply_point(blend_transform(c,w),palm,po);
        float dn=0, d0=0; for(int k=0;k<3;++k){dn+=(pn[k]-line[k])*(pn[k]-line[k]); d0+=(po[k]-line[k])*(po[k]-line[k]);}
        worstNew=fmaxf(worstNew,sqrtf(dn)); worstOld=fmaxf(worstOld,sqrtf(d0));
    }
    check("palm-straight-line",worstNew<0.001f);
    check("palm-negative-control",worstOld>5.0f);   // the old blend arcs: proves the test can fail
    const auto pe0=blend_transform_palm(c,0,palm), pe1=blend_transform_palm(c,1,palm);
    check("palm-endpoints",pe0.t[0]==0 && pe0.r.m[0]==1 && pe1.t[0]==c.t[0] && pe1.r.m[4]==c.r.m[4]);
    const auto pmid=blend_transform_palm(c,0.5f,palm), omid=blend_transform(c,0.5f);
    bool sameRot=true; for(int k=0;k<9;++k) if(fabsf(pmid.r.m[k]-omid.r.m[k])>1e-6f) sameRot=false;
    check("palm-same-rotation",sameRot);
    dvr::hf::Xform sheared=c; sheared.r.m[0]=99;
    const auto ps=blend_transform_palm(sheared,0.5f,palm);
    check("palm-shear-refused",ps.r.m[0]==1 && ps.t[0]==0);
    // Ownership through the return.
    check("mask-held-while-returning",render_hand_mask(true,true,0,2,false,0.4f)==2);
    check("mask-dropped-at-controller",render_hand_mask(true,true,0,2,false,1.0f)==0);
    check("mask-held-in-hysteresis",render_hand_mask(true,true,0,3,true,0.0f)==3);
    check("mask-new-match-wins",render_hand_mask(true,true,3,2,true,0.2f)==3);
    check("mask-invalid-clears",render_hand_mask(false,true,3,3,true,0.0f)==0);
    // CinematicArms motion gate: start 20 uu/s for 120 ms, stop below 8 uu/s for 600 ms.
    MotionGate g; bool o=false;
    o=g.update(3,0,20,8,120,600);       check("gate-still-closed",!o);
    o=g.update(50,100,20,8,120,600);    check("gate-spike-not-yet",!o);
    o=g.update(50,180,20,8,120,600);    check("gate-short-burst-closed",!o);   // 80 ms above
    o=g.update(5,200,20,8,120,600);     o=g.update(50,300,20,8,120,600);
    o=g.update(50,420,20,8,120,600);    check("gate-opens-after-120ms",o);
    o=g.update(12,500,20,8,120,600);    check("gate-holds-between-thresholds",o);
    o=g.update(4,600,20,8,120,600);     o=g.update(4,1100,20,8,120,600);
    check("gate-holds-short-stillness",o);
    o=g.update(4,1200,20,8,120,600);    check("gate-closes-after-600ms",!o);
    o=g.update(-1,1300,20,8,120,600);   check("gate-bad-speed-is-still",!o);
    o=g.update(NAN,1400,20,8,120,600);  check("gate-nan-is-still",!o);
    return failures;
}

