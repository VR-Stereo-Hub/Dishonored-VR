#include "game/dishonored/animation_rules.h"
#include "game/dishonored/anim_policy_test.h"
#include <cstdio>
#include <initializer_list>
int main(){
    using namespace dvr::anim;int failures=AnimPolicyTests();unsigned checks=0;
    auto check=[&](bool ok,const char* label){++checks;if(!ok){++failures;std::printf("FAIL %s\n",label);}};
    check(armRuleCount==40,"complete shipped state catalog");
    for(int i=0;i<armRuleCount;++i){
        check(arm_rule_index(armRules[i].lane,armRules[i].state)==i,"unique lane and state");
        check(armRules[i].label[0]!=0,"label present");
    }
    check(arm_rule_index(0,"Unknown")==-1,"unknown state inherits existing policy");
    check(arm_rule_index(1,"StatePlayerAction")!=arm_rule_index(2,"StatePlayerAction"),"upper and left rules independent");
    check(arm_rule_value(-1,true) && !arm_rule_value(-1,false),"unset inherits defaults");
    check(!arm_rule_value(0,true) && arm_rule_value(1,false),"checkbox overrides either default");
    unsigned cancellable=0;for(int i=0;i<armRuleCount;++i)if(cancellable_action(i))++cancellable;
    check(cancellable==18,"bounded cancellable entry catalog");
    check(!cancellable_action(-1) && !cancellable_action(armRuleCount),"invalid indices cannot cancel");
    for(const char* state:{"StatePlayerMasterWalk","StatePlayerMasterFalling","StatePlayerMasterDead","StatePlayerMasterPossess","StatePlayerMasterStunned"})
        check(!cancellable_action(arm_rule_index(0,state)),"locomotion/recovery remains reachable");
    check(cancellable_action(arm_rule_index(0,"StatePlayerMasterLeaning")),"lean request cancellable");
    check(cancellable_action(arm_rule_index(0,"StatePlayerMasterJump")),"jump request cancellable");
    check(cancellable_action(arm_rule_index(1,"StatePlayerGrabCorpse")),"body pickup request cancellable");
    check(!cancellable_action(arm_rule_index(1,"StatePlayerCarryCorpseIdle")),"carry recovery remains reachable");
    const int mantle=arm_rule_index(0,"StatePlayerMasterMantle");
    check(native_pose_requested(mantle,true,false),"hidden mantle arms retain native pose");
    check(native_pose_requested(mantle,false,false),"voluntary action retains pose without inherited visibility");
    check(native_pose_requested(mantle,true,true),"visible mantle arms retain same pose");
    check(!native_pose_requested(arm_rule_index(0,"StatePlayerMasterWalk"),false,false),"walking remains tracked");
    check(split_native_hands(true,false),"native pose with hidden arms uses animated hand geometry");
    check(!split_native_hands(true,true),"native pose with visible arms passes full mesh");
    check(!split_native_hands(false,false),"tracked pose retains normal hand pipeline");
    std::printf("%u animation catalog checks, %d failures\n",checks,failures);return failures?1:0;
}
