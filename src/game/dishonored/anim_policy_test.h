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
    return failures;
}

