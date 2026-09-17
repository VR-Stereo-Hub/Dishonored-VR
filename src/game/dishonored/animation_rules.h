// VR-134: shipped player FSM vocabulary, documented in ANIM-HANDOFF-PLAN.md.
// These are action states, not sequence-history entries (which can be stale).
#pragma once
#include <cstring>
namespace dvr::anim {
struct ArmRule { int lane; const char* state; const char* label; };
inline constexpr ArmRule armRules[]={
    {0,"StatePlayerMasterWalk","Walking"},
    {0,"StatePlayerMasterLeaning","Leaning"},
    {0,"StatePlayerMasterSwim","Swimming"},
    {0,"StatePlayerMasterJump","Jumping"},
    {0,"StatePlayerMasterFalling","Falling"},
    {0,"StatePlayerMasterAction","Full-body action"},
    {0,"StatePlayerMasterVersus","Combat encounter"},
    {0,"StatePlayerMasterChangeReadyStance","Ready stance"},
    {0,"StatePlayerMasterMantle","Mantling"},
    {0,"StatePlayerMasterStunned","Stunned"},
    {0,"StatePlayerMasterDead","Death"},
    {0,"StatePlayerMasterInDialog","Dialogue"},
    {0,"StatePlayerMasterSoiree","Scripted scene"},
    {0,"StatePlayerMasterInScriptedChoice","Scripted choice"},
    {0,"StatePlayerMasterAssassinate","Assassination"},
    {0,"StatePlayerMasterHolePeeking","Keyhole peeking"},
    {0,"StatePlayerMasterClimb","Climbing"},
    {0,"StatePlayerMasterPrePossess","Entering possession"},
    {0,"StatePlayerMasterPossess","Possession"},
    {0,"StatePlayerMasterSlide","Sliding"},
    {0,"StatePlayerMasterMinigame","Minigame"},
    {0,"StatePlayerMasterChoke","Choking"},
    {0,"StatePlayerMasterInStore","Store"},
    {1,"StatePlayerUpperIdle","Idle"},
    {1,"StatePlayerMeleeAttack","Melee attack"},
    {1,"StatePlayerBlock","Blocking"},
    {1,"StatePlayerGenericFatality","Fatality"},
    {1,"StatePlayerAction","Item action"},
    {1,"StatePlayerTransitionItemIn","Item transition"},
    {1,"StatePlayerEquipChange","Equipment change"},
    {1,"StatePlayerChangeReadyStance","Ready stance"},
    {1,"StatePlayerGrabMovable","Grab object"},
    {1,"StatePlayerGrabCorpse","Grab body"},
    {1,"StatePlayerCarryCorpseIdle","Carry body"},
    {2,"StatePlayerUpperIdle","Idle"},
    {2,"StatePlayerAction","Item action"},
    {2,"StatePlayerTransitionItemIn","Item transition"},
    {2,"StatePlayerEquipChange","Equipment change"},
    {2,"StatePlayerUpperNav","Navigation"},
    {2,"StatePlayerGrabMovable","Grab object"},
};
inline constexpr int armRuleCount=sizeof(armRules)/sizeof(armRules[0]);
inline int arm_rule_index(int lane,const char* state) {
    for(int i=0;i<armRuleCount;++i)if(armRules[i].lane==lane && !std::strcmp(armRules[i].state,state))return i;
    return -1;
}
inline bool arm_rule_value(int overrideValue,bool inherited) {
    return overrideValue<0 ? inherited : overrideValue!=0;
}
}
