// Test the actual UI control against idle and edited widget events. No game.
#include <cstdio>
static float g_fovLever=0,target=108.05f;
static unsigned writes=0;
namespace dvr::camera {void set_fov_deg(float f){target=f;++writes;}}
namespace ImGui {
bool checkboxEdited=false,checkboxValue=false,sliderEdited=false;
float sliderValue=0,sliderInput=0;
bool Checkbox(const char*,bool* value){if(!checkboxEdited)return false;*value=checkboxValue;return true;}
bool SliderFloat(const char*,float* value,float,float,const char*){sliderInput=*value;if(!sliderEdited)return false;*value=sliderValue;return true;}
void TextDisabled(const char*){}
}
#include "../src/core/ui/legacy_fov_control.inc"
int main(){
 unsigned checks=0,failed=0;
 auto check=[&](bool ok,const char* name){++checks;if(!ok){++failed;std::printf("FAIL %s\n",name);}};
 for(unsigned i=0;i<10000;++i) OverlayLegacyFovControl();
 check(writes==0 && target==108.05f,"idle disabled control preserves automatic target across10k frames");
 ImGui::checkboxEdited=true;ImGui::checkboxValue=true;OverlayLegacyFovControl();
 check(writes==1 && target==95 && g_fovLever==95,"enable edits once");
 check(ImGui::sliderInput==95,"slider receives newly enabled value");
 ImGui::checkboxEdited=false;target=108.05f;
 for(unsigned i=0;i<10000;++i) OverlayLegacyFovControl();
 check(writes==1 && target==108.05f,"idle enabled control also preserves automatic target");
 ImGui::sliderEdited=true;ImGui::sliderValue=103;OverlayLegacyFovControl();
 check(writes==2 && target==103 && g_fovLever==103,"actual slider edit writes once");
 ImGui::sliderEdited=false;ImGui::checkboxEdited=true;ImGui::checkboxValue=false;OverlayLegacyFovControl();
 check(writes==3 && target==0 && g_fovLever==0,"disable edits once");
 ImGui::checkboxEdited=false;target=108.05f;OverlayLegacyFovControl();
 check(writes==3 && target==108.05f,"subsequent idle display frame cannot clear target again");
 std::printf("legacy FOV control: %u checks, %u failures\n",checks,failed);return failed?1:0;
}
