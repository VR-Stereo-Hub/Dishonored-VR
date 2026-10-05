#include "ui/screens.h"
#include "ui/widgets.h"
#include "model/fake_states.h"
#include "core/ui/ovl_ui.h"
#include "core/util/log.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>
#include <cstdlib>
#include <map>
namespace dvr::log { uint8_t g_levels[(int)Cat::COUNT]={};void write(Cat,Level,const char*,...){} }
static std::map<ImGuiID,ImRect> bounds;
static std::map<std::string,ImRect> labels;
void ImGuiTestEngineHook_ItemAdd(ImGuiContext* ctx,ImGuiID id,const ImRect& box,const ImGuiLastItemData*){bounds[id]=box;for(const char* name:{"##stereo-mode","##runtime","##modifier","##upscaler","##upscaler-quality","##upscaler-preset"})if(ctx->CurrentWindow && ctx->CurrentWindow->IDStack.Size && id==ctx->CurrentWindow->GetID(name))labels[name]=box;}
void ImGuiTestEngineHook_ItemInfo(ImGuiContext*,ImGuiID id,const char* label,ImGuiItemStatusFlags){if(label && bounds.count(id))labels[label]=bounds[id];}
void ImGuiTestEngineHook_Log(ImGuiContext*,const char*,...){}
const char* ImGuiTestEngine_FindItemDebugLabel(ImGuiContext*,ImGuiID){return nullptr;}
namespace dvr::setup { void Report::add(StepStatus status,std::string title,std::string detail){steps.push_back({title,detail,status});if(status==StepStatus::Failed)ok=false;} }
using namespace dvr::setup;
static ViewState v;
static int count=0;
static void check(bool ok,const char* text){++count;if(!ok){fprintf(stderr,"FAIL: %s\n",text);exit(1);}}
static UiAction frame(){labels.clear();bounds.clear();ImGui::NewFrame();auto a=ui::draw(v);ImGui::Render();return a;}
static void reveal(const char* name){
 for(int attempt=0;attempt<8;++attempt){
  if(labels.count(name)) {
   const auto box=labels[name];
   auto& ctx=*ImGui::GetCurrentContext();
   for(auto* win:ctx.Windows)if(strstr(win->Name,"/##page_") && win->Active){
    if(box.Min.y>=win->InnerRect.Min.y && box.Max.y<=win->InnerRect.Max.y)return;
    ImGui::SetScrollY(win,win->Scroll.y+box.Min.y-win->InnerRect.Min.y-40*ImGui::GetStyle().FontScaleDpi);
   }
  }
  frame();frame();
 }
}
static UiAction click(const char* name){
 if(!labels.count(name)){for(const auto& p:labels)fprintf(stderr,"label: %s\n",p.first.c_str());}
 check(labels.count(name)!=0,name);auto box=labels[name];auto& io=ImGui::GetIO();
 io.AddMousePosEvent((box.Min.x+box.Max.x)*.5f,(box.Min.y+box.Max.y)*.5f);frame();
 io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);return frame();
}
static void state(const char* name){check(fake_state(name,&v),name);frame();frame();}
static void suite(float dpi,float width,float height){
 printf("Testing dpi=%g width=%g height=%g\n",dpi,width,height);
 auto* ctx=ImGui::CreateContext();ctx->TestEngineHookItems=true;
 auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize=ImVec2(width*dpi,height*dpi);io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
 dvr::ovl::load_fonts();dvr::ovl::apply_theme();ui::apply_launcher_style();ImGui::GetStyle().ScaleAllSizes(dpi);ImGui::GetStyle().FontScaleDpi=dpi;
 unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
 state("manage");
 const struct{const char* label;UiAction action;} nav[]={{"Overview",UiAction::Overview},{"Settings",UiAction::ChangeSettings},{"Mods",UiAction::ShowMods},{"Bindings",UiAction::ShowGuide},{"Updates",UiAction::ShowUpdates},{"Help & about",UiAction::ShowAbout}};
 for(auto item:nav)check(click(item.label)==item.action,item.label);
 check(click("Collect logs")==UiAction::CollectSupport,"overview collection routes to real collector");
 check(click("Play")==UiAction::Launch,"play returns launch action without executing it");
 const auto play=labels["Play"];
 check(play.Max.x<=io.DisplaySize.x && play.Max.y<=io.DisplaySize.y && play.Min.x>io.DisplaySize.x*.7f,"play remains bottom right and visible");
 v.det.running=process::Running::Yes;frame();check(click("Play")==UiAction::None,"play blocked while running");
 check(click("Collect logs")==UiAction::CollectSupport,"read-only support collection allowed while running");
 v.busy=true;frame();check(click("Settings")==UiAction::None,"navigation blocked during worker");v.busy=false;
 state("setup-change");check(click("Apply settings")==UiAction::Install,"apply routes to settings worker");
 v.det.running=process::Running::Yes;frame();check(click("Apply settings")==UiAction::None,"apply blocked while running");v.det.running=process::Running::No;
 frame();click("Controls");check(v.settingsPage==1,"controls page selected");
 if(height>=800){click("Edit button mapping");check(v.bindingsOpen,"mapping editor expands");}
 click("Display");check(v.settingsPage==0,"display page selected");
 click("##stereo-mode");frame();frame();click("AFW (experimental)");check(v.choices.stereoEdit==1,"AFW selection is an explicit edit");
 check(v.settingsDirty,"AFW edit marks draft unsaved");
 check(click("Play")==UiAction::None && v.confirmPlay,"unsaved Play opens confirmation");frame();frame();
 check(click("Back to settings")==UiAction::ChangeSettings && !v.confirmPlay && v.settingsDirty,"return from Play guard retains draft");
 state("setup-change");
 reveal("##upscaler");click("##upscaler");frame();frame();click("NVIDIA DLSS");frame();frame();
 check(v.choices.upscalerEdit==1 && v.settingsDirty,"DLSS selection marks draft dirty");
 reveal("##upscaler-quality");click("##upscaler-quality");frame();frame();click("Ultra Quality");
 check(v.choices.upscalerQualityEdit==5,"Ultra Quality retains saved value 5");
 reveal("##upscaler-preset");click("##upscaler-preset");frame();frame();click("Fast (default)");
 check(v.choices.upscalerPresetEdit==5,"DLSS preset selection uses shared F10 model choice");
 reveal("##upscaler");click("##upscaler");frame();frame();click("AMD FSR");frame();frame();
 check(v.choices.upscalerEdit==2 && !labels.count("##upscaler-preset"),"FSR hides NVIDIA-only presets");
 reveal("##upscaler");click("##upscaler");frame();frame();click("Off");frame();frame();
 check(v.choices.upscalerEdit==0 && !labels.count("##upscaler-quality"),"Off hides inactive upscaler options");
 state("mods");v.det.reshadeSupported=false;frame();check(click("Install ReShade 6.8")==UiAction::None,"runtime download waits for compatible installed build");
 v.det.reshadeSupported=true;frame();check(click("Install ReShade 6.8")==UiAction::InstallReShade,"compatible runtime download works across build hashes");
 v.det.reshadeInstalled=true;frame();check(click("Turn ReShade on")==UiAction::ToggleReShade,"ReShade enable action wired");
 check(click("Uninstall ReShade runtime")==UiAction::RemoveReShade,"ReShade remove action wired");
 v.det.reshadeSupported=false;v.det.reshadeEnabled=true;frame();
 check(click("Turn ReShade off")==UiAction::ToggleReShade,"disable remains available for an unsupported/different runtime");
 check(click("Uninstall ReShade runtime")==UiAction::RemoveReShade,"uninstall remains available for an unsupported/different runtime");
 v.det.reshadeEnabled=false;frame();check(click("Turn ReShade on")==UiAction::None,"enable refuses an unsupported runtime");
 v.det.reshadeSupported=true;
 v.det.running=process::Running::Yes;frame();check(click("Turn ReShade on")==UiAction::None,"ReShade mutation blocked while running");
 state("about-updates");check(click("Check for updates")==UiAction::CheckUpdates,"update check wired");
 v.updateDownloading=true;frame();frame();frame();
 auto* modal=ImGui::FindWindowByName("Updating Dishonored VR");
 check(modal && modal->Active,"download progress modal is visible");
 check(modal->Size.x>=400*dpi && modal->Size.x<io.DisplaySize.x,"progress dialog has readable width within viewport");
 check(modal->Size.y<200*dpi && modal->ScrollMax.y==0,"progress text fits without vertical scrolling");
 check(modal->Pos.x>=0 && modal->Pos.y>=0 && modal->Pos.x+modal->Size.x<=io.DisplaySize.x && modal->Pos.y+modal->Size.y<=io.DisplaySize.y,"progress modal remains entirely visible");
 check(click("Overview")==UiAction::None,"update modal prevents navigation");
 v.updateDownloading=false;frame();frame();
 check(!modal->Active,"progress dialog closes on completion or failure");
 ImGui::DestroyContext(ctx);
}
int main(){suite(1,960,850);suite(1.5f,960,850);suite(1,760,640);printf("PASS: %d native launcher interaction checks\n",count);}
