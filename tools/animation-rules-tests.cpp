#include "game/dishonored/animation_rules.h"
#include "game/dishonored/anim_policy_test.h"
#include <cstdio>
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
    std::printf("%u animation catalog checks, %d failures\n",checks,failures);return failures?1:0;
}
