// ReShade's public API drives controls drawn by the existing F10 ImGui context.
// Handles are enumerated and used in this frame only; reloads invalidate them.
#include "core/ui/reshade_panel.h"
#include "core/gfx/reshade_runtime.h"
#include "core/util/log.h"
#include "../../../third_party/reshade/reshade_api.hpp"
#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <limits>
#include <cmath>
#include <windows.h>
#include <chrono>
#include <set>

namespace dvr::reshade_panel {
namespace {
using Runtime = reshade::api::effect_runtime;
using Uniform = reshade::api::effect_uniform_variable;
using Technique = reshade::api::effect_technique;
using Format = reshade::api::format;
std::filesystem::path from_utf8(const std::string& value) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(value.data()),value.size()));
}
std::string utf8(const std::filesystem::path& path) {
    const auto value=path.u8string();
    return std::string(reinterpret_cast<const char*>(value.data()),value.size());
}
std::string annotation(Runtime* runtime, Uniform variable, const char* key) {
    size_t size = 0;
    if (!runtime->get_annotation_string_from_uniform_variable(variable,key,nullptr,&size) || !size || size > 65536) return {};
    std::string value(size, '\0');
    runtime->get_annotation_string_from_uniform_variable(variable,key,value.data(),&size);
    if (!value.empty() && value.back() == '\0') value.pop_back();
    return value;
}
void tip(const std::string& text) {
    if (!text.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
        ImGui::BeginTooltip(); ImGui::PushTextWrapPos(ImGui::GetFontSize()*30);
        ImGui::TextUnformatted(text.c_str()); ImGui::PopTextWrapPos(); ImGui::EndTooltip();
    }
}
bool is_preset(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::string line; size_t bytes = 0;
    while (bytes < 131072 && std::getline(file,line)) {
        bytes += line.size()+1;
        if (line.rfind("Techniques=",0)==0 || line.rfind("TechniqueSorting=",0)==0) return true;
    }
    return false;
}
std::vector<std::string> presets(const std::string& current) {
    std::vector<std::string> result;
    if (!current.empty()) result.push_back(current);
    const std::filesystem::path dirs[] = {std::filesystem::path(reshade_runtime::directory()),from_utf8(current).parent_path()};
    for (const auto& dir : dirs) {
        if (dir.empty()) continue;
        std::error_code error;
        auto it = std::filesystem::directory_iterator(dir,error);
        for (unsigned count=0; !error && it!=std::filesystem::directory_iterator() && count<1024; it.increment(error),++count) {
            const auto path=it->path(); auto ext=path.extension().wstring();
            std::transform(ext.begin(),ext.end(),ext.begin(),towlower);
            if (ext!=L".ini" || !it->is_regular_file(error) || !is_preset(path)) continue;
            const auto name=utf8(path);
            if (std::find(result.begin(),result.end(),name)==result.end()) result.push_back(name);
        }
    }
    return result;
}
// TechniqueSorting is ReShade's global order, including unrelated installed shaders.
// A preset owns its configured sections, enabled techniques, and selections saved by F10.
std::set<std::string> preset_effects(const std::string& path) {
    std::set<std::string> files;
    std::ifstream input(from_utf8(path)); std::string line;
    while(std::getline(input,line)) {
        if(!line.empty() && line.back()=='\r')line.pop_back();
        if(line.size()>2 && line.front()=='[' && line.back()==']') {
            auto name=line.substr(1,line.size()-2);
            if(name.size()>3 && name.substr(name.size()-3)==".fx")files.insert(name);
        }
        if(line.rfind("Techniques=",0)==0 || line.rfind("DVRPresetEffects=",0)==0) {
            size_t at=line.find('=')+1;
            while(at<line.size()) {
                auto end=line.find(',',at);auto token=line.substr(at,end-at);
                auto sep=token.find('@');if(sep!=std::string::npos)token=token.substr(sep+1);
                if(!token.empty())files.insert(token);
                if(end==std::string::npos)break;at=end+1;
            }
        }
    }
    return files;
}
bool remember_effect(const std::string& path,const std::string& effect) {
    auto files=preset_effects(path); files.insert(effect);std::string value;
    for(const auto& file:files) {if(!value.empty())value+=",";value+=file;}
    return WritePrivateProfileStringW(L"DVR",L"DVRPresetEffects",from_utf8(value).wstring().c_str(),from_utf8(path).wstring().c_str())!=FALSE;
}
void uniform_control(Runtime* runtime, Uniform variable) {
    bool hidden=false, noedit=false;
    runtime->get_annotation_bool_from_uniform_variable(variable,"hidden",&hidden,1);
    if (hidden || !annotation(runtime,variable,"source").empty()) return;
    runtime->get_annotation_bool_from_uniform_variable(variable,"noedit",&noedit,1);
    char name[512]={}; runtime->get_uniform_variable_name(variable,name);
    std::string label=annotation(runtime,variable,"ui_label"); if(label.empty())label=name;
    const auto type=annotation(runtime,variable,"ui_type"), help=annotation(runtime,variable,"ui_tooltip");
    const auto text=annotation(runtime,variable,"ui_text"), items=annotation(runtime,variable,"ui_items");
    Format base=Format::unknown; uint32_t rows=0,cols=0,length=0;
    runtime->get_uniform_variable_type(variable,&base,&rows,&cols,&length);
    const uint32_t count=rows*cols, arrays=length ? length : 1;
    if(!count || count>16 || arrays>64) return;
    ImGui::PushID(name);
    if(!text.empty()) { ImGui::PushTextWrapPos(); ImGui::TextUnformatted(text.c_str()); ImGui::PopTextWrapPos(); }
    ImGui::PushTextWrapPos(); ImGui::TextUnformatted(label.c_str()); ImGui::PopTextWrapPos(); tip(help);
    ImGui::BeginDisabled(noedit);
    bool changed=false;
    for(uint32_t element=0;element<arrays;++element) {
        ImGui::PushID((int)element);
        if(arrays>1)ImGui::TextDisabled("Element %u",element+1);
        float fv[16]={}; int32_t iv[16]={}; uint32_t uv[16]={}; bool bv[16]={};
        if(base==Format::r32_float)runtime->get_uniform_value_float(variable,fv,count,element);
        else if(base==Format::r32_sint)runtime->get_uniform_value_int(variable,iv,count,element);
        else if(base==Format::r32_uint)runtime->get_uniform_value_uint(variable,uv,count,element);
        else if(base==Format::r32_typeless)runtime->get_uniform_value_bool(variable,bv,count,element);
        bool edit=false;
        for(uint32_t component=0;component<count;++component) {
            ImGui::PushID((int)component);
            if(count>1) { ImGui::TextDisabled("%u",component+1); ImGui::SameLine(); }
            ImGui::SetNextItemWidth(-1);
            if(base==Format::r32_typeless) {
                if(type=="button") { const bool pressed=ImGui::Button("Apply",ImVec2(-1,0));edit|=bv[component]!=pressed;bv[component]=pressed; }
                else edit|=ImGui::Checkbox("Enabled",&bv[component]);
            } else if((base==Format::r32_sint || base==Format::r32_uint) && !items.empty() && (type=="combo" || type=="list" || type=="radio")) {
                // ReShade annotations contain NUL-separated choices. Keep both terminators.
                std::vector<const char*> choices;
                for(size_t pos=0;pos<items.size();) { if(!items[pos])break;choices.push_back(items.c_str()+pos);const size_t next=items.find('\0',pos);if(next==std::string::npos)break;pos=next+1; }
                int selected=base==Format::r32_sint ? iv[component] : (uv[component]<choices.size() ? (int)uv[component] : -1);
                if(!choices.empty() && ImGui::Combo("##value",&selected,choices.data(),(int)choices.size())) {
                    if(base==Format::r32_sint)iv[component]=selected;else uv[component]=(uint32_t)selected;edit=true;
                }
            } else if(base==Format::r32_float) {
                float low=0,high=1,step=.001f;
                const bool lo=runtime->get_annotation_float_from_uniform_variable(variable,"ui_min",&low,1);
                const bool hi=runtime->get_annotation_float_from_uniform_variable(variable,"ui_max",&high,1);
                runtime->get_annotation_float_from_uniform_variable(variable,"ui_step",&step,1);
                if(!(step>0) || !std::isfinite(step))step=.001f;
                const bool bounded=((lo && hi) || type=="slider" || type=="color" || (type.empty() && (count==3 || count==4))) && std::isfinite(low) && std::isfinite(high) && high>low;
                edit|=bounded ? ImGui::SliderFloat("##value",&fv[component],low,high,"%.4f",ImGuiSliderFlags_AlwaysClamp)
                              : ImGui::DragFloat("##value",&fv[component],step,0,0,"%.4f");
            } else if(base==Format::r32_sint || base==Format::r32_uint) {
                int32_t low=0,high=1; uint32_t ulow=0,uhigh=1;
                bool lo=false,hi=false;
                if(base==Format::r32_sint) { lo=runtime->get_annotation_int_from_uniform_variable(variable,"ui_min",&low,1);hi=runtime->get_annotation_int_from_uniform_variable(variable,"ui_max",&high,1); }
                else { lo=runtime->get_annotation_uint_from_uniform_variable(variable,"ui_min",&ulow,1);hi=runtime->get_annotation_uint_from_uniform_variable(variable,"ui_max",&uhigh,1); }
                const bool bounded=((lo && hi) || type=="slider") && (base==Format::r32_sint ? high>low : uhigh>ulow);
                const auto dataType=base==Format::r32_sint ? ImGuiDataType_S32 : ImGuiDataType_U32;
                void* value=base==Format::r32_sint ? (void*)&iv[component] : (void*)&uv[component];
                const void* min=base==Format::r32_sint ? (void*)&low : (void*)&ulow;
                const void* max=base==Format::r32_sint ? (void*)&high : (void*)&uhigh;
                edit|=bounded ? ImGui::SliderScalar("##value",dataType,value,min,max,nullptr,ImGuiSliderFlags_AlwaysClamp)
                              : ImGui::DragScalar("##value",dataType,value,1.0f);
            }
            tip(help); ImGui::PopID();
        }
        if(edit) {
            if(base==Format::r32_float)runtime->set_uniform_value_float(variable,fv,count,element);
            else if(base==Format::r32_sint)runtime->set_uniform_value_int(variable,iv,count,element);
            else if(base==Format::r32_uint)runtime->set_uniform_value_uint(variable,uv,count,element);
            else if(base==Format::r32_typeless)runtime->set_uniform_value_bool(variable,bv,count,element);
            changed=true;
        }
        ImGui::PopID();
    }
    if(ImGui::SmallButton("Reset this setting")) { runtime->reset_uniform_value(variable);changed=true; }
    if(changed)runtime->save_current_preset();
    ImGui::EndDisabled();ImGui::Spacing();ImGui::PopID();
}
}
void draw() {
    bool enabled=reshade_runtime::enabled_next_start();
    static bool saveFailed=false;
    if(ImGui::Checkbox("Enable ReShade next launch",&enabled))saveFailed=!reshade_runtime::set_enabled_next_start(enabled);
    tip("Optional. Off by default. Changing this takes effect after restarting Dishonored.");
    if(saveFailed)ImGui::TextWrapped("Could not save the setting. Check that dishonored_vr.ini is writable.");
    Runtime* runtime=reshade_runtime::api();
    if(!runtime) {
        if(!reshade_runtime::manual() && enabled) {
            ImGui::TextWrapped("The legacy ReShade integration does not support this tab. Switch to the VR integration, then restart.");
            if(ImGui::Button("Use VR integration"))saveFailed=!reshade_runtime::set_enabled_next_start(true);
            return;
        }
        // A load that was tried and failed is not fixed by restarting: say why instead.
        if(enabled && reshade_runtime::load_failure()) {
            ImGui::TextWrapped("ReShade did not start this launch (%s).",reshade_runtime::load_failure());
            ImGui::TextWrapped("Restarting will not fix this. Run Install ReShade in the launcher again - it repairs ReShade.ini and the shader folders - then use Collect logs if it still fails.");
            return;
        }
        ImGui::TextWrapped(enabled ? "Restart Dishonored to load the VR ReShade integration." : "ReShade is installed and disabled. Enable it above, then restart Dishonored.");return;
    }
    if(!enabled)ImGui::TextWrapped("ReShade will be disabled next launch. Its controls remain available until you exit.");
    bool effects=runtime->get_effects_state();
    if(ImGui::Checkbox("Effects on",&effects)) {
        runtime->set_effects_state(effects);
        DVR_LOG(::dvr::log::Cat::present,::dvr::log::Level::Info,"reshade: effects %s by F10",effects ? "ON" : "off");
    }
    tip("Live toggle, also available with Scroll Lock. Your preset's effect selections are kept.");
    char path[32768]={};size_t size=sizeof(path);runtime->get_current_preset_path(path,&size);
    const auto title=utf8(from_utf8(path).filename());
    ImGui::TextDisabled("PRESET"); ImGui::SetNextItemWidth(-1);
    static std::vector<std::string> choices;
    if(ImGui::BeginCombo("##preset",title.empty()?"Choose a preset":title.c_str())) {
        if(ImGui::IsWindowAppearing())choices=presets(path);
        for(const auto& choice:choices) {
            const auto name=utf8(from_utf8(choice).filename());
            ImGui::PushID(choice.c_str());
            if(ImGui::Selectable(name.c_str(),choice==path)) { runtime->save_current_preset();runtime->set_current_preset_path(choice.c_str()); }
            ImGui::PopID();
        }
        if(choices.empty())ImGui::TextWrapped("Put a ReShade preset INI beside Dishonored.exe.");
        ImGui::EndCombo();
    }
    bool optimized=reshade_runtime::performance_mode();
    if(ImGui::Checkbox("Performance mode",&optimized)) { reshade_runtime::set_performance_mode(optimized);return; }
    tip("Compiles fixed shader settings for performance. Turn off to edit shader parameters; changing this reloads effects.");
    if(ImGui::Button("Reload effects")) { runtime->save_current_preset();runtime->reload_effect_next_frame(nullptr);return; }
    ImGui::SameLine();if(ImGui::Button("Save preset"))runtime->save_current_preset();
    ImGui::TextWrapped("Changes save to the selected preset. Point and click with the trigger; use the stick to scroll or nudge a value.");
    static bool showAll=false;
    ImGui::Checkbox("Show all installed effects",&showAll);
    static std::string cachedPath;
    static std::set<std::string> members;
    static auto checked=std::chrono::steady_clock::time_point{};
    const auto now=std::chrono::steady_clock::now();
    if(cachedPath!=path || now-checked>std::chrono::seconds(1)) {
        cachedPath=path;members=preset_effects(path);checked=now;
    }
    struct Entry { Technique handle;std::string file,label; };
    std::vector<Entry> entries;
    runtime->enumerate_techniques(nullptr,[&](Runtime* r,Technique t) {
        char file[512]={},label[512]={};r->get_technique_effect_name(t,file);r->get_technique_name(t,label);
        if(showAll || members.count(file) || r->get_technique_state(t))entries.push_back({t,file,label});
    });
    if(entries.empty()) { ImGui::TextWrapped("Effects are loading, or no shader packages were found. Check the ReShade log if this message remains.");return; }
    static bool presetSaveFailed=false;
    if(presetSaveFailed)ImGui::TextWrapped("Could not save effect membership. Check that the selected preset is writable.");
    std::vector<std::string> drawn;
    for(const auto& entry:entries) {
        ImGui::PushID(entry.file.c_str());ImGui::PushID(entry.label.c_str());
        bool on=runtime->get_technique_state(entry.handle);
        if(ImGui::Checkbox(entry.label.c_str(),&on)) {
            presetSaveFailed=!remember_effect(path,entry.file);
            if(!presetSaveFailed) {
                members.insert(entry.file);runtime->set_technique_state(entry.handle,on);runtime->save_current_preset();
                DVR_LOG(::dvr::log::Cat::present,::dvr::log::Level::Info,"reshade: %s@%s %s by F10",entry.label.c_str(),entry.file.c_str(),on ? "ON" : "off");
            }
        }
        tip(entry.file);ImGui::PopID();ImGui::PopID();
    }
    ImGui::Separator();
    if(optimized) { ImGui::TextWrapped("Turn off Performance mode above to adjust shader settings.");return; }
    for(const auto& entry:entries) {
        if(std::find(drawn.begin(),drawn.end(),entry.file)!=drawn.end())continue;
        drawn.push_back(entry.file);
        if(ImGui::CollapsingHeader(entry.file.c_str())) {
            ImGui::PushID(entry.file.c_str());
            runtime->enumerate_uniform_variables(entry.file.c_str(),[](Runtime* r,Uniform v,void*) { uniform_control(r,v); },nullptr);
            ImGui::PopID();
        }
    }
}
}
