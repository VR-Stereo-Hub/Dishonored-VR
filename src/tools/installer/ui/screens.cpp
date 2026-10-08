// tools/installer/ui/screens.cpp - see screens.h.
#include "ui/screens.h"
#include "ui/widgets.h"
#include "sys/fs.h"
#include "core/ui/ovl_ui.h"
#include <stdio.h>
#include <string>
#include <d3d11.h>
#include "sys/resources.h"
#include "payload_ids.h"
#include "core/util/log.h"

namespace dvr::setup::ui {

namespace {
ID3D11ShaderResourceView* guideTexture = nullptr;
unsigned guideWidth = 0, guideHeight = 0;
std::string n(const std::wstring& w) { return fs::narrow(w); }
bool same_settings(const Choices& a,const Choices& b) {
    if(a.runtime!=b.runtime || a.quality!=b.quality || a.pixelPercent!=b.pixelPercent || !(a.exact==b.exact) ||
        a.textureMemory!=b.textureMemory || a.stereoEdit!=b.stereoEdit || a.swapSticksEdit!=b.swapSticksEdit || a.bindingEdits!=b.bindingEdits)return false;
    if(a.upscalerEdit!=b.upscalerEdit || a.upscalerQualityEdit!=b.upscalerQualityEdit || a.upscalerPresetEdit!=b.upscalerPresetEdit)return false;
    for(int i=0;i<PreferenceCount;++i)if(a.preferences[i]!=b.preferences[i])return false;
    for(int i=0;i<dvr::binds::ActionCount;++i)if(a.bindings.src[i]!=b.bindings.src[i])return false;
    return a.bindings.swapSticks==b.bindings.swapSticks;
}

const char* kRuntimeTips[3] = {
    "Pins Virtual Desktop's own OpenXR runtime (VDXR) for this game. Recommended: 120 Hz, or 144 Hz with Virtual Desktop Beta. Keep SSW off.",
    "Index, Vive, WMR through SteamVR, Quest over Link or Steam Link. The mod brings its own bridge (dvr_steamvr32.dll); start SteamVR before the game.",
    "The mod tries the 32-bit OpenXR runtime Windows registers and falls back to the SteamVR bridge when there is none. Pick this when unsure.",
};
const char* kQualityTips[4] = {
    "75% of the tested pixels, both axes scaled together. For an 8 GB card, or when Balanced stutters.",
    "2750x2850 per eye. The recommended starting resolution.",
    "120% of the tested pixels. Sharper and slower; judged on a 4070 Ti SUPER class card. Not the place to start.",
    "150% of the tested pixels. For cards with clear headroom at Quality; drop back if the headset stutters.",
};

void game_section(ViewState& v, UiAction* action)
{
    if (!heading("Game location", "Steam or GOG, original 32-bit Dishonored. The mod goes in Binaries\\Win32.")) return;
    if(v.det.game.unsupported64) banner(v.det.gameNote.c_str());
    if(v.det.games.size()>1 && ImGui::BeginCombo("Detected libraries",n(v.det.gameDir).c_str())) {
        for(int i=0;i<(int)v.det.games.size();++i) {
            const auto label=n(v.det.games[i].dir);
            if(ImGui::Selectable(label.c_str(),fs::iequals(v.det.games[i].dir,v.det.gameDir))) {v.selectedGame=i;*action=UiAction::SelectGame;}
        }
        ImGui::EndCombo();
    }
    const float bw = ImGui::CalcTextSize("Choose folder...").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::AlignTextToFramePadding();
    ImGui::BeginChild("##gamepath", ImVec2(ImGui::GetContentRegionAvail().x - bw - ImGui::GetStyle().ItemSpacing.x, ImGui::GetFrameHeight()),
                      ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    ImGui::AlignTextToFramePadding();
    if (!v.det.gameDir.empty()) path_text(n(v.det.gameDir).c_str());
    else ImGui::TextDisabled("Not found");
    ImGui::EndChild();
    ImGui::SameLine();
    if (button("Choose folder...", false, !v.busy)) *action = UiAction::Browse;
    dvr::ovl::tip("Point at the folder holding Dishonored.exe (Binaries\\Win32) for a Steam or GOG installation. You can also select the game root or Binaries folder.");

    std::string status = v.det.gameNote;
    const ImVec4* colour = nullptr;
    const ImVec4 warn = col_brass_hi();
    if (v.det.gameFound && v.det.running != process::Running::No) {
        status = v.det.running == process::Running::Yes
            ? "Dishonored is running. Quit it all the way to the desktop first; its d3d9.dll is in use."
            : "Could not read the process list to check whether Dishonored is running.";
        colour = &warn;
    } else if (v.det.gameFound && v.det.needsElevation) {
        status = "Writing to this folder needs administrator rights. Install will ask for them once.";
        colour = &warn;
    } else if (v.det.gameFound && !v.det.gameWritable) {
        status = "This folder cannot be written: " + n(fs::win_error_text(v.det.gameWriteErr));
        colour = &warn;
    } else if (!v.det.gameFound) {
        colour = &warn;
    } else if (v.det.modInstalled && !v.changingSettings) {
        status += ". The mod is already installed here; installing again keeps your settings.";
    }
    status_slot("##gamestatus", 1, status.c_str(), colour);
}

// VR-223: which headset the player has, asked once and required. Recorded for
// diagnostics only; the runtime pills below it are what the mod acts on.
void open_headset_picker(ViewState& v)
{
    v.headsetPick = headset_index(v.headset);
    v.headsetOther[0] = 0;
    if (v.headsetPick == kHeadsetOther) snprintf(v.headsetOther, sizeof(v.headsetOther), "%s", v.headset.c_str());
    v.headsetPicking = true;
}

void headset_picker(ViewState& v, UiAction* action)
{
    const bool required = v.headset.empty();
    if (!required && !v.headsetPicking) return;
    if (v.busy || v.updateDownloading) return;
    const char* id = "Which headset do you have?";
    if (!ImGui::IsPopupOpen(id)) ImGui::OpenPopup(id);
    ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x * 0.86f, 0), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) return;
    wrapped_faded(required
        ? "Pick the headset you play on. It goes into your logs so a problem report says which hardware it came from, and Index, Beyond or Vive Pro 2 also turns on the Index controller tuning. You can change it later."
        : "Recorded in your logs for problem reports. Index, Beyond or Vive Pro 2 also turns on the Index controller tuning.");
    bool focusOther = false;
    if (ImGui::BeginTable("##headsets", 2, ImGuiTableFlags_SizingStretchSame)) {
        for (int i = 0; i <= kHeadsetCount; ++i) {
            ImGui::TableNextColumn();
            const char* label = i == kHeadsetOther ? "Something else" : kHeadsets[i];
            ImGui::PushID(i);
            if (ImGui::Selectable(label, v.headsetPick == i, ImGuiSelectableFlags_NoAutoClosePopups)) {
                focusOther = i == kHeadsetOther && v.headsetPick != i;
                v.headsetPick = i;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    std::string pickedName;
    if (v.headsetPick == kHeadsetOther) {
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (focusOther) ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##headset-other", "Type the headset name", v.headsetOther, kHeadsetNameMax + 1);
        pickedName = clean_headset_name(v.headsetOther);
    } else if (v.headsetPick >= 0 && v.headsetPick < kHeadsetCount) {
        pickedName = kHeadsets[v.headsetPick];
    }
    if (headset_gets_index_tuning(pickedName))
        wrapped_faded("Index controllers: the mod applies its Index controller tuning (hand frames, hold angles, force-sensor grip). [Controllers] IndexTuning=0 in dishonored_vr.ini turns it off.");
    ImGui::Spacing();
    if (button(required ? "Continue" : "Save", true, !pickedName.empty())) {
        v.headsetPending = pickedName;
        *action = UiAction::SaveHeadset;
        ImGui::CloseCurrentPopup();
    }
    if (!required) {
        ImGui::SameLine();
        if (button("Cancel")) { v.headsetPicking = false; ImGui::CloseCurrentPopup(); }
    } else if (pickedName.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled(v.headsetPick == kHeadsetOther ? "Type a name to continue." : "Pick one to continue.");
    }
    ImGui::EndPopup();
}

void headset_row(ViewState& v)
{
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("Headset model:");
    ImGui::SameLine();
    ImGui::TextUnformatted(v.headset.empty() ? "not recorded" : v.headset.c_str());
    ImGui::SameLine();
    if (button("Change##headset", false, !v.busy)) open_headset_picker(v);
    dvr::ovl::tip(headset_gets_index_tuning(v.headset)
        ? "The headset you play on. Recorded in the logs, and it turns on the mod's Index controller tuning ([Controllers] IndexTuning)."
        : "The headset you play on. Recorded in the launcher and game logs for problem reports; it changes no setting.");
}

void headset_section(ViewState& v)
{
    heading("Headset & rendering");
    const float factor=ImGui::GetFontSize()/16;
    const float field=ImGui::GetContentRegionAvail().x*.55f;
    ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Headset model");
    ImGui::SameLine(field);ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(v.headset.empty()?"Not selected":v.headset.c_str());ImGui::SameLine();
    if(button("Change##headset"))open_headset_picker(v);
    ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("VR runtime");ImGui::SameLine(ImGui::GetContentRegionAvail().x-225*factor);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const char* labels[]={"Virtual Desktop (VDXR)","SteamVR","Automatic"};
    int rt=(int)v.choices.runtime;
    if(ImGui::Combo("##runtime",&rt,labels,3))v.choices.runtime=(Runtime)rt;
    dvr::ovl::tip(kRuntimeTips[rt]);
    ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("Rendering mode");ImGui::SameLine(ImGui::GetContentRegionAvail().x-225*factor);
    ImGui::SetNextItemWidth(-FLT_MIN);
    const int method=v.choices.stereoMethod;
    if(ImGui::BeginCombo("##stereo-mode",method==1?"AFW (experimental)":method==0?"Stereo":"Custom (kept)")) {
        if(ImGui::Selectable("Stereo",method==0))v.choices.stereoMethod=v.choices.stereoEdit=0;
        if(ImGui::Selectable("AFW (experimental)",method==1))v.choices.stereoMethod=v.choices.stereoEdit=1;
        ImGui::EndCombo();
    }
    dvr::ovl::tip("Stereo draws both eyes per game tick. AFW alternates eyes and warps the other eye. DLAA or DLSS is recommended with AFW. Applies next launch.");
    wrapped_faded("Connect your headset before playing. Virtual Desktop: 120 Hz or 144 Hz beta; SSW off.");
    if(v.choices.runtime==Runtime::Vdxr && !v.det.vdxrPresent)wrapped_faded("Virtual Desktop Streamer was not found. The mod will choose a runtime until it is installed.");
}

void quality_section(ViewState& v)
{
    heading("Render quality");
    const float factor=ImGui::GetFontSize()/16;
    const char* labels[]={"Performance\n75% pixels","Balanced\n100% pixels","Quality\n120% pixels","Ultra\n150% pixels"};
    const float width=(ImGui::GetContentRegionAvail().x-ImGui::GetStyle().ItemSpacing.x*3)/4;
    for(int i=0;i<4;++i) {
        if(i)ImGui::SameLine();
        const bool selected=(int)v.choices.quality==i;
        ImGui::PushStyleColor(ImGuiCol_Button,selected?ImVec4(.78f,.75f,.67f,1):ImVec4(.067f,.102f,.118f,1));
        ImGui::PushStyleColor(ImGuiCol_Text,selected?ImVec4(.15f,.17f,.17f,1):col_bone());
        if(ImGui::Button(labels[i],ImVec2(width,49*factor)))v.choices.choose((Quality)i);
        ImGui::PopStyleColor(2);dvr::ovl::tip(kQualityTips[i]);
    }
    float pct=v.choices.quality==Quality::Custom?v.choices.pixelPercent:percent_for_quality(v.choices.quality,v.choices.pixelPercent);
    ImGui::Text("Total pixels %.0f%%",pct);
    const Size size=v.choices.size();
    const auto resolution=fs::format("%ux%u per eye",size.w,size.h);
    ImGui::SameLine(ImGui::GetContentRegionAvail().x-ImGui::CalcTextSize(resolution.c_str()).x);
    ImGui::TextUnformatted(resolution.c_str());
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(0,0));
    if(ImGui::SliderFloat("##total-pixels",&pct,50,kMaxPercent,"",ImGuiSliderFlags_AlwaysClamp))v.choices.choose_percent(pct);
    ImGui::PopStyleVar();
    wrapped_faded("Higher is sharper and slower. Custom range: 50-450%.");
}

void upscaler_section(ViewState& v)
{
    heading("Upscaling & anti-aliasing");
    const float factor=ImGui::GetFontSize()/16;
    const auto field=[&](const char* label) {
        ImGui::AlignTextToFramePadding(); ImGui::TextUnformatted(label);
        ImGui::SameLine(ImGui::GetContentRegionAvail().x-270*factor);
        ImGui::SetNextItemWidth(-FLT_MIN);
    };
    const char* backends[]={"Off", "NVIDIA DLSS", "AMD FSR"};
    field("Upscaler");
    if(ImGui::BeginCombo("##upscaler",v.choices.upscaler>=0 && v.choices.upscaler<3 ? backends[v.choices.upscaler] : "Custom (kept)")) {
        for(int i=0;i<3;++i) if(ImGui::Selectable(backends[i],i==v.choices.upscaler))v.choices.upscaler=v.choices.upscalerEdit=i;
        ImGui::EndCombo();
    }
    dvr::ovl::tip("DLSS requires an NVIDIA RTX GPU. FSR supports AMD, NVIDIA and Intel GPUs. Applies next launch.");
    if(v.choices.upscaler>0) {
        const char* qualities[]={v.choices.upscaler==2 ? "Native AA" : "DLAA (native)", "Quality", "Balanced", "Performance", "Ultra Performance", "Ultra Quality"};
        const int order[]={0,5,1,2,3,4};
        field("Upscaler quality");
        const int q=v.choices.upscalerQuality;
        if(ImGui::BeginCombo("##upscaler-quality",q>=0 && q<6 ? qualities[q] : "Custom (kept)")) {
            for(int i:order)if(ImGui::Selectable(qualities[i],i==q))v.choices.upscalerQuality=v.choices.upscalerQualityEdit=i;
            ImGui::EndCombo();
        }
        if(v.choices.upscaler==1) {
            field("DLSS preset"); const int preset=v.choices.upscalerPreset;
            if(ImGui::BeginCombo("##upscaler-preset",preset>=0 && preset<dvr::dlss::kModelChoiceCount ? dvr::dlss::kModelChoices[preset].name : "Custom (kept)")) {
                for(int i=0;i<dvr::dlss::kModelChoiceCount;++i) {
                    if(ImGui::Selectable(dvr::dlss::kModelChoices[i].name,i==preset))v.choices.upscalerPreset=v.choices.upscalerPresetEdit=i;
                    dvr::ovl::tip(dvr::dlss::kModelChoices[i].tip);
                }
                ImGui::EndCombo();
            }
        } else wrapped_faded("FSR uses its own presets. F10 shows the FSR versions available on your GPU.");
        wrapped_faded("Render quality above sets the output resolution. Upscaler quality controls how small the game renders before rebuilding it.");
    }
}

void preference_checkbox(ViewState& v, int id, const char* tip)
{
    const Preference& p = kPreferences[id];
    const int stored = v.choices.preferences[id] < 0 ? p.fallback : v.choices.preferences[id];
    const bool inverted = p.inverted || id == Rain;
    bool on = inverted ? !stored : stored != 0;
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1,1));
    if (ImGui::Checkbox(id == Rain ? "Rain overlay" : p.label, &on))
        v.choices.preferences[id] = inverted ? !on : on;
    ImGui::PopStyleVar();
    dvr::ovl::tip(tip);
}

void preferences_section(ViewState& v, bool controls = false)
{
    if (!controls) {
        preference_checkbox(v,Mirror,"Shows the desktop mirror. Off by default; can improve performance.");ImGui::SameLine(ImGui::GetContentRegionAvail().x*.5f);
        preference_checkbox(v,Rain,"Shows the close rain layer. Sky rain and splashes remain either way.");
        bool paged=v.choices.textureMemory==1;
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,ImVec2(1,1));
        if(ImGui::Checkbox("Texture pack compatibility",&paged))v.choices.textureMemory=paged?1:0;
        ImGui::PopStyleVar();
        wrapped_faded("Helps large texture packs fit within the game memory limit. May run slower on PCs with little free memory.");
        return;
    }
    heading("Movement & comfort");
    preference_checkbox(v,Crouch,"Duck in your room to crouch. Controller crouch still works.");ImGui::SameLine(ImGui::GetContentRegionAvail().x*.5f);
    preference_checkbox(v,SnapTurn,"Replaces smooth stick turning with fixed steps.");
    wrapped_faded("Controller crouch still works. Snap turning replaces smooth stick turning with fixed steps.");
    heading("Controller shortcuts");
    const char* names[]={"Off","Right thumbrest","R3 (right stick click)","Left thumbrest"};
    const int modes[]={0,1,2,4};
    const int modifier=v.choices.preferences[Modifier]<0?1:v.choices.preferences[Modifier];
    int sel=modifier==4?3:modifier>=0 && modifier<=2?modifier:0;
    ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("D-pad modifier");ImGui::SameLine(ImGui::GetContentRegionAvail().x*.55f);ImGui::SetNextItemWidth(-FLT_MIN);
    if(ImGui::Combo("##modifier",&sel,names,4))v.choices.preferences[Modifier]=modes[sel];
    const int old=v.choices.preferences[DpadFlip];
    preference_checkbox(v,DpadFlip,"Choose which stick becomes the D-pad while the modifier is held.");
    if(old!=v.choices.preferences[DpadFlip]) {
        const int mod=v.choices.preferences[Modifier]<0?1:v.choices.preferences[Modifier];
        if(mod==1 || mod==4)v.choices.preferences[Modifier]=v.choices.preferences[DpadFlip]?4:1;
    }
    preference_checkbox(v,PauseChord,"Press X and Y together to pause when the runtime reserves the menu button.");
    wrapped_faded("Hold the modifier and move a stick for item shortcuts. R3 takes over its health-elixir hold.");
    wrapped_faded("Touch: thumbrest shortcuts. Index: use R3. Vive and WMR may need SteamVR remapping. Beyond uses the controllers you pair; Pico and PSVR2 need compatible runtime bindings.");
}

void bindings_section(ViewState& v)
{
    heading("Button mapping");
    wrapped_faded("Choose the button for each action, just like in F10.");
    if(button(v.bindingsOpen ? "Hide button mapping" : "Edit button mapping"))v.bindingsOpen=!v.bindingsOpen;
    if(!v.bindingsOpen)return;
    auto& layout = v.choices.bindings;
    for (int a = 0; a < dvr::binds::ActionCount; ++a) {
        ImGui::PushID(a);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
        if (ImGui::BeginCombo(dvr::binds::info(a).label, dvr::binds::source_label(layout.src[a]))) {
            for (int b = 0; b < dvr::binds::SourceCount; ++b) {
                if (ImGui::Selectable(dvr::binds::source_label(b), layout.src[a] == b)) {
                    layout.src[a] = (dvr::binds::Source)b;
                    v.choices.bindingEdits |= uint16_t(1u << a);
                }
            }
            ImGui::EndCombo();
        }
        dvr::ovl::tip(dvr::binds::info(a).tip);
        if (dvr::binds::conflicts(layout, a)) ImGui::TextDisabled("Shared button: both actions will activate.");
        ImGui::PopID();
    }
    if (ImGui::Checkbox("Swap move and turn sticks", &layout.swapSticks))
        v.choices.swapSticksEdit = layout.swapSticks;
    if (button("Reset button mapping")) {
        layout = dvr::binds::Layout{};
        v.choices.bindingEdits = (1u << dvr::binds::ActionCount) - 1;
        v.choices.swapSticksEdit = 0;
    }
    wrapped_faded("To assign a button by pressing it, use the F10 menu while playing.");
}

UiAction draw_setup(ViewState& v)
{
    UiAction action = UiAction::None;
    page_header(v.det.modInstalled ? "Settings" : "Set up Dishonored VR", "Set up your headset, picture and controls for the next launch.");
    if (!v.det.modInstalled) game_section(v, &action);
    const char* tabs[]={"Display","Controls"};
    for(int i=0;i<2;++i) {
        if(i)ImGui::SameLine();
        const bool selected=v.settingsPage==i;
        ImGui::PushStyleColor(ImGuiCol_Button,selected?ImVec4(.78f,.75f,.67f,1):ImVec4(.067f,.102f,.118f,1));
        ImGui::PushStyleColor(ImGuiCol_Text,selected?ImVec4(.15f,.17f,.17f,1):col_bone());
        if(ImGui::Button(tabs[i],ImVec2(ImGui::GetFontSize()*6.25f,ImGui::GetFrameHeight())))v.settingsPage=i;
        ImGui::PopStyleColor(2);
    }
    ImGui::Separator();ImGui::Spacing();
    if (v.settingsPage == 0) {
        headset_section(v);
        quality_section(v);
        upscaler_section(v);
        preferences_section(v);
    } else {
        preferences_section(v, true);
        bindings_section(v);
        heading("More settings in game");
        wrapped_faded("L3 + R3 opens height, hands, reticle, HUD, display and comfort settings. Hold both sticks for the Virtual Desktop overlay.");
    }
    return action;
}

UiAction draw_done(ViewState& v)
{
    UiAction action = UiAction::None;
    page_header(v.report.ok ? "Operation complete" : "The operation could not finish");
    for (const auto& step : v.report.steps) step_row(step);
    if (v.report.baselinePending) {
        wrapped("Waiting for the game's first run to finish. This window applies the required game settings after you close the game.");
        if (button("Apply now", false, v.det.configExists && v.det.running == process::Running::No)) action = UiAction::ApplyBaseline;
    }
    // ReShade work started on the Mods screen returns there.
    const bool fromMods = v.lastOp.rfind("reshade", 0) == 0 || v.lastOp == "import-presets";
    if (button(fromMods ? "Back to Mods" : "Back to overview")) action = fromMods ? UiAction::ShowMods : UiAction::Overview;
    return action;
}

UiAction draw_manage(ViewState& v)
{
    UiAction action = UiAction::None;
    page_header("Ready for Dunwall", "Your installation and the things you need before playing.");
    game_section(v, &action);
    if (heading("Current setup", nullptr)) {
        wrapped(v.det.modInstalled ? (v.det.disabled ? "VR is disabled." : "VR is enabled.") : "The VR mod is not installed here yet.");
        if (v.det.record.valid) wrapped_faded(fs::format("Installed %s | %s", v.det.record.version.c_str(), v.det.record.buildId.c_str()).c_str());
        if (v.det.iniExists) wrapped_faded(fs::format("%s | %ux%u per eye", runtime_label(v.det.iniRuntime), v.det.iniSize.w, v.det.iniSize.h).c_str());
        headset_row(v);
        if (button(v.det.modInstalled ? "Change settings" : "Set up VR")) action = UiAction::ChangeSettings;
    }
    if (heading("Quick actions", nullptr)) {
        if (button("Collect logs", true, v.det.gameFound)) action = UiAction::CollectSupport;
        ImGui::SameLine();
        if (button("Open game folder", false, v.det.gameFound)) action = UiAction::OpenGameFolder;
        wrapped_faded("Collect logs creates a local support ZIP below 24 MB. Nothing is uploaded.");
        if (button("Create desktop shortcut")) action = UiAction::DesktopShortcut;
        ImGui::SameLine();
        if (button("Create Start menu shortcut")) action = UiAction::StartShortcut;
    }
    if (v.confirmUninstall) {
        wrapped("Remove the VR mod and restore the proxy that was here before? Existing ReShade files and presets are kept.");
        ImGui::Checkbox("Also delete dishonored_vr.ini (my settings)", &v.deleteIni);
        if (button("Remove the mod", true, v.det.running == process::Running::No)) action = UiAction::ConfirmUninstall;
        ImGui::SameLine(); if (button("Keep it")) action = UiAction::CancelUninstall;
    } else if (heading("Manage installation", nullptr, false)) {
        if (button(v.det.disabled ? "Enable VR" : "Disable VR", false, v.det.modInstalled)) action = UiAction::ToggleDisable;
        ImGui::SameLine();
        if (button("Uninstall", false, v.det.modInstalled && v.det.running == process::Running::No)) action = UiAction::Uninstall;
    }
    return action;
}

UiAction draw_mods(ViewState& v)
{
    UiAction action = UiAction::None;
    page_header("Mods", "Optional visual effects and texture packs.");
    if (heading("ReShade", "Optional visual effects. Disabled by default.")) {
        // The status names what is missing: a ReShade32.dll alone reads as installed but
        // never starts (no ReShade.ini) and has nothing to draw (no shader packages).
        const bool incomplete = v.det.reshadeInstalled && (!v.det.reshadeIniPresent || !v.det.reshadeShadersPresent);
        if (!v.det.reshadeInstalled) {
            wrapped("Not installed. Install adds the verified ReShade 6.8 runtime, the Standard Effects, SweetFX and prod80 shaders, and a ReShade.ini.");
        } else if (incomplete) {
            ImGui::PushStyleColor(ImGuiCol_Text, col_brass_hi());
            wrapped(!v.det.reshadeIniPresent && !v.det.reshadeShadersPresent ? "Incomplete: ReShade.ini and the shader packages are missing. Click Repair ReShade."
                    : !v.det.reshadeIniPresent ? "Incomplete: ReShade.ini is missing, so ReShade cannot start. Click Repair ReShade."
                    : "Incomplete: the shader packages are missing, so presets have no effects. Click Repair ReShade.");
            ImGui::PopStyleColor();
        } else {
            wrapped(v.det.reshadeEnabled ? "Installed and on for the next game launch." : "Installed and off. Click Turn ReShade on, then start the game.");
        }
        if (!v.det.reshadeLastRun.empty()) wrapped_faded(v.det.reshadeLastRun.c_str());
        if (button(!v.det.reshadeInstalled ? "Install ReShade 6.8" : incomplete ? "Repair ReShade" : "Reinstall ReShade 6.8", incomplete,
            v.det.gameFound && v.det.modInstalled && v.det.reshadeSupported && v.det.running == process::Running::No)) action = UiAction::InstallReShade;
        if (v.det.reshadeInstalled) {
            const bool ready = v.det.gameFound && v.det.iniExists && v.det.running == process::Running::No;
            if (button(v.det.reshadeEnabled ? "Turn ReShade off" : "Turn ReShade on", false, ready && (v.det.reshadeEnabled || v.det.reshadeSupported))) action = UiAction::ToggleReShade;
            ImGui::SameLine();
            if (button("Uninstall ReShade runtime", false, ready)) action = UiAction::RemoveReShade;
            wrapped_faded("Uninstall keeps a runtime backup, your presets, shaders and ReShade.ini.");
        }
        if (!v.det.reshadeSupported) wrapped_faded("Install a VR build with ReShade support before installing or enabling effects. Existing effects can still be disabled or removed.");
        wrapped_faded("F10 > ReShade provides preset selection, effects, and shader settings with the same motion controls. Effects add GPU work.");
        if (button("Get the Carinth preset")) action = UiAction::OpenPresetSource;
        ImGui::SameLine();
        if (button("Open game folder", false, v.det.gameFound)) action = UiAction::OpenGameFolder;
        wrapped_faded(v.det.reshadeShadersPresent ? "Opens the preset's Nexus Mods page. Its shaders are already installed: drop the download in the box below and only the preset is taken from it."
                                                  : incomplete ? "Opens the preset's Nexus Mods page. Repair ReShade first - it brings the shaders this preset needs."
                                                  : "Opens the preset's Nexus Mods page. Install ReShade first - it brings the shaders this preset needs.");
        ImGui::Spacing();
        ImGui::BeginChild("##preset-drop", ImVec2(-1, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
        wrapped("Drop ReShade presets here");
        wrapped_faded("Drag a preset .ini, a preset download (.zip) or its folder anywhere onto this window. Presets go beside Dishonored.exe; shaders and textures go to dvr-reshade-shaders\\custom. DLLs are never copied, so a d3d9.dll inside a download cannot replace the VR mod. Then pick the preset in F10 > ReShade.");
        if (button("Choose files...", false, v.det.gameFound && !v.busy)) action = UiAction::ChoosePresetFiles;
        ImGui::EndChild();
    }
    if (heading("HD Texture Pack 2.0", "Requires the author's pack and TFC Installer.")) {
        if (button("Get HD Texture Pack 2.0")) action = UiAction::OpenTextureSource;
        wrapped_faded("Install with the author's TFC Installer instructions. Retain its backups so you can remove the pack later.");
        wrapped_faded("Settings > Display includes texture address space compatibility.");
    }
    return action;
}

UiAction draw_updates(ViewState& v)
{
    UiAction action = UiAction::None;
    page_header("Updates", "Launcher and VR mod updates stay together.");
    wrapped_faded(fs::format("This launcher: %s | %s", v.det.version.c_str(), v.det.buildId.c_str()).c_str());
    if(button("Check for updates",true,!v.updateChecking && !v.updateDownloading)) action=UiAction::CheckUpdates;
    wrapped_faded(v.updateMessage.empty()?"Checks stable GitHub releases; saved history is available offline.":v.updateMessage.c_str());
    if (dvr::ovl::checkbox("Overwrite INI and in-game settings on update (recommended)", &v.choices.overwriteSettings)) action = UiAction::SaveUpdatePreference;
    wrapped_faded(v.choices.overwriteSettings ? "Uses the new defaults and backs up your current INI." : "Keeps current settings. An older INI schema may still require a backed-up refresh.");
    if(!v.releases.empty() && updates::newer(v.releases.front().version,v.det.version)) {
        const auto& latest=v.releases.front();
        if(button(("Download update "+latest.version).c_str(),true,latest.downloadable() && v.det.running==process::Running::No)) action=UiAction::DownloadUpdate;
        if(!latest.downloadable()) wrapped_faded("No verified launcher download is available for this release yet.");
    }
    const auto installLabel = v.det.installedIsEmbedded() ? "Reinstall " + v.det.version : "Install bundled build " + v.det.version;
    if (button(installLabel.c_str(), false, v.det.gameFound && v.det.payloadOk && v.det.running == process::Running::No)) action = UiAction::Update;
    if (heading("Changelog history", nullptr)) {
        if(v.releases.empty()) {
            const auto notes=resources::rcdata(IDR_RELEASE_NOTES);
            if(notes.ok()) {const std::string text((const char*)notes.data,notes.size);wrapped(text.c_str());}
        }
        for(const auto& release:v.releases) {
            if(ImGui::CollapsingHeader((release.version+"  "+release.published).c_str())) wrapped(release.notes.c_str());
        }
    }
    if (button("GitHub releases")) action = UiAction::OpenReleases;
    return action;
}
UiAction draw_about(ViewState& v)
{
    UiAction action = UiAction::None;
    page_header("Help & about", "Troubleshooting, project credits and support.");
    heading("Need help?");
    if (button("Collect logs", true, v.det.gameFound)) action = UiAction::CollectSupport;
    ImGui::SameLine(); if (button("Open launcher log")) action = UiAction::OpenLog;
    wrapped_faded("The support ZIP stays on your computer. Attach it to a problem report when needed.");
    ImGui::Text("Version %s", v.det.version.c_str());
    wrapped_faded("A motion-controlled VR adventure in Dunwall.");
    dvr::ovl::ornament();
    heading("Credits");
    if (button("Pizza Parker / BioVRDev")) action = UiAction::CreditPizza;
    wrapped_faded("Mod development and continued support.");
    if (button("VOID / mohamad-balouza")) action = UiAction::CreditVoid;
    wrapped_faded("Mod development and continued support.");
    if (button("Gingas / GingasVRFO")) action = UiAction::CreditGingas;
    wrapped_faded("Creator of the original Dishonored VR mod.");
    dvr::ovl::ornament();
    heading("Support development");
    wrapped("If you like the mod, please consider donating! Donations are never required, but always appreciated! :)");
    if (button("Pizza Parker on Ko-fi", true)) action = UiAction::OpenKofi;
    return action;
}

UiAction draw_guide(ViewState& v)
{
    page_header("Controller bindings", "Quest 3 default layout. Custom shortcuts and SteamVR bindings can differ.");
    wrapped_faded("Quest 3 default layout. Custom shortcuts and SteamVR bindings can differ.");
    const auto& c = v.guideReturn == Screen::Setup ? v.choices : v.det.suggested;
    const int mod = c.preferences[Modifier] < 0 ? 1 : c.preferences[Modifier];
    const int flip = c.preferences[DpadFlip] < 0 ? 0 : c.preferences[DpadFlip];
    const int pause = c.preferences[PauseChord] < 0 ? 1 : c.preferences[PauseChord];
    const char* name = mod == 0 ? "off" : mod == 1 ? "right thumbrest" : mod == 2 ? "R3" : "left thumbrest";
    ImGui::TextWrapped("Current shortcuts: %s + %s stick | X + Y pause: %s",
                       name, flip ? "right" : "left", pause ? "on" : "off");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);
    dvr::ovl::slider_float("Zoom", &v.guideZoom, 1.0f, 3.0f, "%.1fx", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SameLine();
    if (button("Fit")) v.guideZoom = 1.0f;
    ImGui::SameLine(); ImGui::TextDisabled("Scroll to pan. Maximize for more room.");
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float bodyH = avail.y - ImGui::GetFrameHeightWithSpacing() * 1.8f;
    const float fitW = avail.x - ImGui::GetStyle().ScrollbarSize - ImGui::GetStyle().WindowPadding.x * 2;
    const float fitH = bodyH - ImGui::GetStyle().ScrollbarSize - ImGui::GetStyle().WindowPadding.y * 2;
    const float aspect = guideHeight ? (float)guideWidth / guideHeight : 1.5f;
    float width = fitW < fitH * aspect ? fitW : fitH * aspect;
    width *= v.guideZoom;
    ImGui::BeginChild("##field-guide", ImVec2(0, bodyH), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    if (guideTexture) {
        const float spare = ImGui::GetContentRegionAvail().x - width;
        if (spare > 0) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + spare * 0.5f);
        ImGui::Image((ImTextureID)(uintptr_t)guideTexture, ImVec2(width, width / aspect));
    } else wrapped("The embedded bindings image could not be loaded. Open the launcher log for details.");
    ImGui::EndChild();
    dvr::ovl::ornament();
    return UiAction::None;
}
} // namespace

void load_guide(ID3D11Device* device)
{
    release_guide();
    load_widget_art(device);
    guideTexture = resources::image(device, IDR_CONTROLLER_GUIDE, &guideWidth, &guideHeight);
    DVR_INFO("launcher: bindings image %s (%ux%u)", guideTexture ? "loaded" : "FAILED", guideWidth, guideHeight);
}
void release_guide()
{
    release_widget_art();
    if (guideTexture) guideTexture->Release();
    guideTexture = nullptr; guideWidth = guideHeight = 0;
}

UiAction draw(ViewState& v)
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar);
    dvr::ovl::backdrop();
    UiAction a = UiAction::None;
    const float factor=ImGui::GetFontSize()/16;
    brand_header(v.det.version.c_str());
    const float footerH = 64*factor;
    const float sidebarW = 165*factor;
    ImGui::BeginDisabled(v.busy || v.updateDownloading);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(0,0));
    ImGui::BeginChild("##sidebar", ImVec2(sidebarW, -footerH));
    const struct { const char* label; Screen screen; UiAction action; } nav[] = {
        {"Overview", Screen::Manage, UiAction::Overview},
        {"Settings", Screen::Setup, UiAction::ChangeSettings},
        {"Mods", Screen::Mods, UiAction::ShowMods},
        {"Bindings", Screen::Guide, UiAction::ShowGuide},
        {"Updates", Screen::Updates, UiAction::ShowUpdates},
        {"Help & about", Screen::About, UiAction::ShowAbout}
    };
    for (const auto& item : nav) {
        if (sidebar_button(item.label, v.screen == item.screen)) a = item.action;
    }
    ImGui::SetCursorPosY(ImGui::GetWindowHeight()-85*factor);
    wrapped_faded(v.det.game.store==discovery::Store::Gog ? "Dishonored - GOG" : "Dishonored - Steam");
    wrapped_faded(v.det.modInstalled ? (v.det.disabled ? "VR disabled" : "VR enabled") : "VR not installed");
    wrapped_faded("Settings apply next launch");
    ImGui::EndChild();
    ImGui::PopStyleVar();
    const auto sidebarEnd=ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(sidebarEnd.x+10*factor,ImGui::GetItemRectMin().y),ImVec2(sidebarEnd.x+10*factor,sidebarEnd.y),IM_COL32(116,100,66,119));
    ImGui::SameLine(0,25*factor);
    ImGui::BeginChild("##page", ImVec2(0, -footerH));
    const Choices beforeChoices=v.choices;
    UiAction content = UiAction::None;
    if (v.busy) spinner(v.busyText.c_str());
    else {
        if (v.det.embeddedLegacy) banner("This build contains retired diagnostics. Use a release build for play.");
        else if (v.det.config != "RelWithDebInfo") banner("This is a diagnostic build, not an optimized play build.");
        switch (v.screen) {
        case Screen::Setup: content = draw_setup(v); break;
        case Screen::Done: content = draw_done(v); break;
        case Screen::Manage: content = draw_manage(v); break;
        case Screen::Guide: content = draw_guide(v); break;
        case Screen::About: content = draw_about(v); break;
        case Screen::Mods: content = draw_mods(v); break;
        case Screen::Updates: content = draw_updates(v); break;
        }
    }
    if(!same_settings(beforeChoices,v.choices))v.settingsDirty=true;
    if (content != UiAction::None) a = content;
    ImGui::EndChild();
    ImGui::SetCursorPosY(ImGui::GetWindowHeight()-footerH);
    ImGui::Separator();ImGui::Dummy(ImVec2(0,7*factor));
    const auto rowStart=ImGui::GetCursorPos();
    const float playWidth=120*factor;
    const float gap=ImGui::GetStyle().ItemSpacing.x;
    float right=ImGui::GetWindowWidth()-ImGui::GetStyle().WindowPadding.x;
    ImGui::SetCursorPosX(right-playWidth);
    if(button("Play",true,v.det.gameFound && v.det.modInstalled && v.det.running==process::Running::No,playWidth))a=UiAction::Launch;
    dvr::ovl::tip(discovery::launch_label(v.det.game.store));
    right-=playWidth+gap;
    if(v.screen==Screen::Setup || v.settingsDirty) {
        const bool allowed=v.det.gameFound && v.det.running==process::Running::No && (v.det.gameWritable || v.det.needsElevation) && v.det.payloadOk;
        const char* label=v.det.modInstalled?"Apply settings":"Install VR";
        const float width=ImGui::CalcTextSize(label).x+ImGui::GetStyle().FramePadding.x*2;
        ImGui::SetCursorPos(ImVec2(right-width,rowStart.y));
        if(button(label,false,allowed,width))a=UiAction::Install;
        right-=width+gap;
    }
    const float closeWidth=75*factor;ImGui::SetCursorPos(ImVec2(right-closeWidth,rowStart.y));
    if(button("Close",false,true,closeWidth))a=UiAction::Close;
    right-=closeWidth+gap;
    ImGui::SetCursorPos(ImVec2(rowStart.x,rowStart.y+8*factor));
    ImGui::PushTextWrapPos(right);
    const char* status=v.notice.empty() ? (v.settingsDirty ? "Unsaved settings. Apply before playing." : "Settings apply at your next launch.") : v.notice.c_str();
    ImGui::PushFont(ImGui::GetFont(),14.0f);ImGui::TextDisabled("%s",status);ImGui::PopFont();
    ImGui::PopTextWrapPos();
    ImGui::EndDisabled();
    if(a==UiAction::Launch && v.settingsDirty){a=UiAction::None;v.confirmPlay=true;}
    if(v.confirmPlay && !v.headset.empty() && !v.headsetPicking) {
        ImGui::OpenPopup("Unsaved settings");
        if(ImGui::BeginPopupModal("Unsaved settings",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            wrapped("Your display or controller changes have not been saved.");
            if(button("Back to settings",true)){a=UiAction::ChangeSettings;v.confirmPlay=false;ImGui::CloseCurrentPopup();}
            ImGui::SameLine();
            if(button("Play with saved settings")){a=UiAction::Launch;v.confirmPlay=false;ImGui::CloseCurrentPopup();}
            ImGui::SameLine();
            if(button("Cancel")){v.confirmPlay=false;ImGui::CloseCurrentPopup();}
            ImGui::EndPopup();
        }
    }
    {
        UiAction picked = UiAction::None;
        headset_picker(v, &picked);
        if (picked != UiAction::None) a = picked;
    }
    if(v.updateDownloading) {
        ImGui::OpenPopup("Updating Dishonored VR");
        // Wrapped text cannot establish the width of an auto-sized window.
        const auto* viewport = ImGui::GetMainViewport();
        const float width = (viewport->WorkSize.x - 32 * factor < 520 * factor)
            ? viewport->WorkSize.x - 32 * factor : 520 * factor;
        ImGui::SetNextWindowSize(ImVec2(width, 0), ImGuiCond_Always);
        ImGui::SetNextWindowPos(viewport->GetWorkCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        if(ImGui::BeginPopupModal("Updating Dishonored VR",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            wrapped("Downloading and verifying the new launcher. It will restart and install the mod update.");
            ImGui::EndPopup();
        }
    } else if(ImGui::BeginPopupModal("Updating Dishonored VR",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    if(v.updatePopup && !v.releases.empty() && !v.headset.empty() && !v.headsetPicking) {
        ImGui::OpenPopup("An update is available");
        ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x * 0.78f,0),ImGuiCond_Appearing);
        if(ImGui::BeginPopupModal("An update is available",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            const auto& latest=v.releases.front();
            ImGui::Text("Dishonored VR %s",latest.version.c_str());
            ImGui::BeginChild("##release-summary",ImVec2(0,ImGui::GetTextLineHeightWithSpacing()*7));
            wrapped(latest.notes.empty()?"A new stable release is available.":latest.notes.substr(0,1600).c_str());
            ImGui::EndChild();
            if(dvr::ovl::checkbox("Overwrite INI on update (recommended)",&v.choices.overwriteSettings))a=UiAction::SaveUpdatePreference;
            if(!v.choices.overwriteSettings)banner("Keeping an old INI can break new updates. Leave overwrite enabled.");
            if(!v.updateMessage.empty())wrapped_faded(v.updateMessage.c_str());
            if(!latest.downloadable())banner("This release has no verified launcher download yet.");
            if(button(("Update to "+latest.version).c_str(),true,latest.downloadable() && !v.busy && !v.updateDownloading && v.det.running==process::Running::No)) {
                a=UiAction::DownloadUpdate;v.updatePopup=false;ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(button("Later")) {v.updatePopup=false;ImGui::CloseCurrentPopup();}
            ImGui::EndPopup();
        }
    }
    ImGui::End();
    return a;
}

} // namespace dvr::setup::ui
