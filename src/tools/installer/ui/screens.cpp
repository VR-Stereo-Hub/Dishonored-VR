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

const char* kRuntimeTips[3] = {
    "Pins Virtual Desktop's own OpenXR runtime (VDXR) for this game. Recommended: 120 Hz, or 144 Hz with Virtual Desktop Beta. Keep SSW off.",
    "Index, Vive, WMR through SteamVR, Quest over Link or Steam Link. The mod brings its own bridge (dvr_steamvr32.dll); start SteamVR before the game.",
    "The mod tries the 32-bit OpenXR runtime Windows registers and falls back to the SteamVR bridge when there is none. Pick this when unsure.",
};
const char* kQualityTips[3] = {
    "75% of the tested pixels, both axes scaled together. For an 8 GB card, or when Balanced stutters.",
    "2750x2850 per eye. The recommended starting resolution.",
    "120% of the tested pixels. Sharper and slower; judged on a 4070 Ti SUPER class card. Not the place to start.",
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

void headset_section(ViewState& v)
{
    if (!heading("Headset", "Which OpenXR runtime the mod talks to. This writes [VR] Runtime and XrRuntimeJson in dishonored_vr.ini.")) return;
    const char* labels[3] = { runtime_label(Runtime::Vdxr), runtime_label(Runtime::SteamVr), runtime_label(Runtime::Auto) };
    const int hit = pill_row(labels, 3, (int)v.choices.runtime, kRuntimeTips);
    if (hit >= 0 && !v.busy) v.choices.runtime = (Runtime)hit;
    std::string line;
    if (v.choices.runtime == Runtime::Vdxr)
        line = v.det.vdxrPresent ? "Virtual Desktop Streamer found. Use 120 Hz, or 144 Hz with VD Beta. SSW off."
                                 : "Virtual Desktop Streamer was not found on this PC; the mod will choose the runtime itself until it is installed.";
    else if (v.choices.runtime == Runtime::SteamVr)
        line = v.det.steamvrPresent ? "SteamVR found. Start it before the game." : "SteamVR was not found in a Steam library; install it, and start it before the game.";
    else
        line = v.det.activeRuntime.empty() ? "No 32-bit OpenXR runtime is registered; the SteamVR bridge will be used."
                                           : "Windows' 32-bit OpenXR runtime is " + n(v.det.activeRuntime) + ".";
    status_slot("##headsetstatus", 1, line.c_str());
}

void quality_section(ViewState& v)
{
    if (!heading("Render quality", "How many pixels the game renders per eye. This writes [Screen] RenderWidth and RenderHeight; the in-game Display tab can change it later.")) return;
    char labels[3][48];
    const Size sp = size_for_percent(kPerformancePercent), sb = size_for_percent(kBalancedPercent), sq = size_for_percent(kQualityPercent);
    snprintf(labels[0], sizeof(labels[0]), "%s  %ux%u", quality_label(Quality::Performance), sp.w, sp.h);
    snprintf(labels[1], sizeof(labels[1]), "%s  %ux%u", quality_label(Quality::Balanced), sb.w, sb.h);
    snprintf(labels[2], sizeof(labels[2]), "%s  %ux%u", quality_label(Quality::Quality), sq.w, sq.h);
    const char* ptrs[3] = { labels[0], labels[1], labels[2] };
    const int sel = v.choices.quality == Quality::Custom ? -1 : (int)v.choices.quality;
    const int hit = pill_row(ptrs, 3, sel, kQualityTips);
    if (hit >= 0 && !v.busy) v.choices.choose((Quality)hit);
    std::string line;
    if (v.det.gpu.known()) {
        line = "GPU: " + n(v.det.gpu.name);
        if (v.det.gpu.budgetBytes) line += fs::format(" (%llu GB)", (unsigned long long)((v.det.gpu.budgetBytes + (1ull << 29)) >> 30));
        line += v.det.suggested.quality == Quality::Performance && !v.det.modInstalled
            ? ". Performance is the safer start on this card." : ". Balanced is where this build was judged.";
    } else {
        line = "No graphics adapter answered; Balanced is where this build was judged.";
    }
    line += " Recommended: 120 Hz, or 144 Hz with VD Beta.";
    status_slot("##qualitystatus", 1, line.c_str());
}

void preference_checkbox(ViewState& v, int id, const char* tip)
{
    const Preference& p = kPreferences[id];
    const int stored = v.choices.preferences[id] < 0 ? p.fallback : v.choices.preferences[id];
    bool on = p.inverted ? !stored : stored != 0;
    if (dvr::ovl::checkbox(p.label, &on))
        v.choices.preferences[id] = p.inverted ? !on : on;
    dvr::ovl::tip(tip);
}

void preferences_section(ViewState& v)
{
    if (heading("Play preferences", "Saved for the next launch. L3 + R3 opens these settings in game.")) {
        preference_checkbox(v, Mirror, "Shows the game on your monitor. Off by default on every runtime.");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.5f);
        preference_checkbox(v, Crouch, "Duck in your room to crouch. The controller crouch button still works.");
        preference_checkbox(v, Rain, "Hides only the close rain layer. Sky rain and ground splashes remain.");
        wrapped_faded("Turning the desktop mirror off can give a huge performance boost with any runtime.");
        wrapped_faded("SteamVR: if the game crashes on startup, turn Desktop mirror ON and try again.");
    }
    if (v.controlsOpen) ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (heading("Controller shortcuts", "Choose controls that exist on your controllers; the headset name alone is not enough.", false)) {
        const char* names[] = { "Off", "Right thumbrest", "R3 (right stick click)", "Left thumbrest" };
        const int modes[] = { 0, 1, 2, 4 };
        const int modifier = v.choices.preferences[Modifier] < 0 ? 1 : v.choices.preferences[Modifier];
        int sel = modifier == 4 ? 3 : (modifier >= 0 && modifier <= 2 ? modifier : 0);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
        if (ImGui::Combo("D-pad modifier", &sel, names, 4)) v.choices.preferences[Modifier] = modes[sel];
        dvr::ovl::tip("Hold the modifier and move a stick for item shortcuts. R3 takes over its health-elixir hold.");
        const int beforeFlip = v.choices.preferences[DpadFlip];
        preference_checkbox(v, DpadFlip, "Choose which stick becomes the D-pad while the modifier is held.");
        if (v.choices.preferences[DpadFlip] != beforeFlip) {
            const int mod = v.choices.preferences[Modifier] < 0 ? 1 : v.choices.preferences[Modifier];
            if (mod == 1 || mod == 4) v.choices.preferences[Modifier] = v.choices.preferences[DpadFlip] ? 4 : 1;
        }
        preference_checkbox(v, PauseChord, "Press X and Y together to pause when the runtime reserves the menu button.");
        wrapped_faded("Touch: thumbrest shortcuts. Index: choose R3 for D-pad shortcuts; the shipped binding has no thumbrest action.");
        wrapped_faded("Vive / WMR: some face buttons are missing. Remap actions in SteamVR; these layouts are not headset-validated.");
        wrapped_faded("Beyond uses the controller profile you pair. Pico / PSVR2 need a compatible runtime or SteamVR binding; native support is not assumed.");
    }
}

void advanced_section(ViewState& v)
{
    if (v.advancedOpen) ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (!heading("Advanced", "The exact pixel budget, the same slider as the in-game Display tab.", false)) return;
    float pct = v.choices.quality == Quality::Custom ? v.choices.pixelPercent : percent_for_quality(v.choices.quality, v.choices.pixelPercent);
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
    if (ImGui::SliderFloat("Total pixels (%)", &pct, 50.0f, 200.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp) && !v.busy)
        v.choices.choose_percent(pct);
    dvr::ovl::tip("100% = 2750x2850. Higher is sharper and slower. Both axes scale together.");
    const Size s = v.choices.size();
    ImGui::SameLine();
    ImGui::TextDisabled("%ux%u per eye%s", s.w, s.h, (v.choices.exact.w && v.choices.exact.h) ? " (as set now)" : "");
}

UiAction draw_setup(ViewState& v)
{
    UiAction action = UiAction::None;
    page_header(v.changingSettings ? "Choose how you play. Your other in-game settings stay as they are."
                                   : "Installs the VR mod beside the game and sets it up for your headset.");
    if (v.det.embeddedLegacy) banner("This build carries the retired diagnostics (legacy): it stalls on every trigger pull. Use a release build for play.");
    else if (v.det.config != "RelWithDebInfo") banner(("This is a " + v.det.config + " build of the launcher and the mod: slower, for testing, not for play.").c_str());

    if (v.busy) {
        spinner(v.busyText.c_str());
        return action;
    }
    ImGui::BeginChild("##setup-body", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 3.4f));
    game_section(v, &action);
    headset_section(v);
    quality_section(v);
    preferences_section(v);
    advanced_section(v);
    ImGui::EndChild();

    footer_begin(ImGui::GetFrameHeightWithSpacing() * 2.6f);
    if (!v.notice.empty()) status_slot("##footer-notice", 1, v.notice.c_str());
    else ImGui::TextDisabled("L3 + R3 opens settings; hold both sticks for the VD overlay.");
    const bool canGo = v.det.gameFound && v.det.running == process::Running::No && (v.det.gameWritable || v.det.needsElevation) && v.det.payloadOk;
    if (v.changingSettings) {
        if (footer_button("Apply", true, canGo || (v.det.gameFound && v.det.iniExists))) action = UiAction::Install;
        if (footer_button("Back")) action = UiAction::CancelChange;
    } else {
        if (footer_button(v.det.needsElevation ? "Install (administrator)" : "Install", true, canGo)) action = UiAction::Install;
        if (footer_button("Cancel")) action = UiAction::Close;
    }
    if (footer_button("Bindings")) action = UiAction::ShowGuide;
    if (footer_button("About")) action = UiAction::ShowAbout;
    return action;
}

UiAction draw_done(ViewState& v)
{
    UiAction action = UiAction::None;
    const bool failed = !v.report.ok;
    const char* sub = failed ? "Something did not go through. Nothing below the failed step was done."
                    : v.lastOp == "uninstall" ? "The mod is gone from the game folder."
                    : v.lastOp == "change" ? "Settings written. They apply at the next launch."
                    : v.report.baselinePending ? "Installed. One thing is left, and this window does it for you."
                    : "Installed and ready. Launch from Steam, GOG, or the button below.";
    page_header(sub);
    if (v.busy) { spinner(v.busyText.c_str()); return action; }

    const float footerH = ImGui::GetFrameHeightWithSpacing() * 2.6f;
    const bool showPlay = !failed && v.lastOp != "uninstall";
    // The step list takes what the fixed blocks below it leave: the tips (a heading
    // and three two-line rows), the waiting row, the footer.
    const ImGuiStyle& st = ImGui::GetStyle();
    const float lineH = ImGui::GetTextLineHeightWithSpacing();
    const float headingH = ImGui::GetFrameHeight() + st.ItemSpacing.y;
    float below = footerH + st.WindowPadding.y + headingH;
    if (showPlay) below += ImGui::GetFrameHeightWithSpacing() + headingH + 2.0f * (2.0f * lineH + st.ItemSpacing.y) + st.ItemSpacing.y;
    if (v.report.baselinePending) below += ImGui::GetFrameHeightWithSpacing() + st.ItemSpacing.y;
    float listH = ImGui::GetContentRegionAvail().y - below;
    if (listH < lineH * 4.0f) listH = lineH * 4.0f;
    if (heading("Current Settings", nullptr)) {
        ImGui::BeginChild("##steps", ImVec2(0, listH), ImGuiChildFlags_None);
        for (const auto& s : v.report.steps) step_row(s);
        ImGui::EndChild();
    }
    if (showPlay) {
        if (button("Create desktop shortcut")) action = UiAction::DesktopShortcut;
        ImGui::SameLine();
        if (button("Create Start menu shortcut")) action = UiAction::StartShortcut;
    }
    if (showPlay && heading("Before you play", nullptr)) {
        static const StepResult tips[] = {
            { "Launch from Steam or GOG, or use Launch via store at the bottom.", "Use the store that owns your selected installation.", StepStatus::Ok },
            { "L3 + R3 opens the settings panel. F5 recenters.", "Height, hands, reticle, comfort, HUD and display, with Basic, Advanced and Debug views.", StepStatus::Ok },
        };
        for (const auto& t : tips) step_row(t);
    }
    if (v.report.baselinePending) {
        ImGui::PushStyleColor(ImGuiCol_Text, col_brass_hi());
        ImGui::TextUnformatted(v.det.configExists ? "The game has run. Applying its settings..." : "Waiting for the game's first run to finish (this window checks every few seconds).");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (button("Apply now", false, v.det.configExists)) action = UiAction::ApplyBaseline;
        dvr::ovl::tip("Applies the four game settings the mod depends on (frame smoothing, depth of field, vsync, mouse smoothing) as soon as the game's settings folder exists.");
    }

    footer_begin(footerH);
    if (!v.notice.empty()) status_slot("##footer-notice", 1, v.notice.c_str());
    else ImGui::TextDisabled("%s", ("Log: " + v.logPath).c_str());
    if (showPlay) { if (footer_button(discovery::launch_label(v.det.game.store), true, v.det.running == process::Running::No)) action = UiAction::Launch; }
    if (footer_button("Close")) action = UiAction::Close;
    if (footer_button("Bindings")) action = UiAction::ShowGuide;
    if (footer_button("About")) action = UiAction::ShowAbout;
    return action;
}

UiAction draw_manage(ViewState& v)
{
    UiAction action = UiAction::None;
    const bool sameBuild = v.det.installedIsEmbedded();
    std::string sub = v.det.record.valid
        ? fs::format("Installed: %s (build %s), %s.", v.det.record.version.c_str(), v.det.record.buildId.c_str(), v.det.record.installedUtc.substr(0, 10).c_str())
        : "Installed by hand (no install record).";
    sub += sameBuild ? " This is the same build." : fs::format(" This launcher carries %s (build %s).", v.det.version.c_str(), v.det.buildId.c_str());
    page_header(sub.c_str());
    if (v.det.embeddedLegacy) banner("This build carries the retired diagnostics (legacy): it stalls on every trigger pull. Use a release build for play.");
    else if (v.det.config != "RelWithDebInfo") banner(("This is a " + v.det.config + " build of the launcher and the mod: slower, for testing, not for play.").c_str());
    if (v.busy) { spinner(v.busyText.c_str()); return action; }

    ImGui::BeginChild("##manage-body", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 3.4f));
    game_section(v,&action);
    if (heading("Current Settings", nullptr)) {
        wrapped(n(v.det.gameDir).c_str());
        std::string line;
        if (v.det.disabled) line = "VR is DISABLED (disable_vr.txt is beside the game). ";
        else line = "VR is enabled. ";
        if (v.det.iniExists) {
            line += fs::format("Headset: %s. Render size: %ux%u per eye.", runtime_label(v.det.iniRuntime), v.det.iniSize.w, v.det.iniSize.h);
        } else {
            line += "No dishonored_vr.ini yet; the mod writes the tested one at the first launch.";
        }
        if (v.det.running == process::Running::Yes) line += " Dishonored is running: quit it before updating or removing.";
        status_slot("##managestatus", 1, line.c_str());
    }
    if (heading("Actions", nullptr)) {
        const bool idle = v.det.running == process::Running::No;
        const float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (v.confirmUninstall) {
            ImGui::PushStyleColor(ImGuiCol_Text, col_brass_hi());
            wrapped("Remove the mod from the game folder? The game runs flat again; a d3d9.dll that was there before is put back.");
            ImGui::PopStyleColor();
            ImGui::Checkbox("Also delete dishonored_vr.ini (my settings)", &v.deleteIni);
            if (button("Remove the mod", true, idle, half)) action = UiAction::ConfirmUninstall;
            ImGui::SameLine();
            if (button("Keep it", false, true, half)) action = UiAction::CancelUninstall;
        } else {
            if (dvr::ovl::checkbox("Overwrite INI and in-game settings on update (recommended)", &v.choices.overwriteSettings))
                action = UiAction::SaveUpdatePreference;
            if(v.choices.overwriteSettings) wrapped_faded("Updates use the new defaults. Your current settings are backed up beside the INI.");
            else banner("WARNING: keeping an old INI can break new updates. Leave overwrite enabled unless you understand the changes.");
            const std::string updateLabel = sameBuild ? "Reinstall " + v.det.version : "Update to " + v.det.version;
            if (button(updateLabel.c_str(), !sameBuild, idle, half)) action = UiAction::Update;
            dvr::ovl::tip("Installs this launcher's build. The overwrite toggle above controls whether your settings are reset.");
            ImGui::SameLine();
            if (button("Change settings", false, v.det.iniExists, half)) action = UiAction::ChangeSettings;
            dvr::ovl::tip("Runtime, resolution, mirror, comfort and controller shortcuts. Your other in-game settings stay.");
            if (button(v.det.disabled ? "Enable VR" : "Disable VR", false, true, half)) action = UiAction::ToggleDisable;
            dvr::ovl::tip("Disable leaves the mod installed and switches it off with a disable_vr.txt beside the game, so Dishonored runs flat. Enable removes the file.");
            ImGui::SameLine();
            if (button("Collect logs", false, v.det.gameFound, half)) action = UiAction::CollectSupport;
            dvr::ovl::tip("Collects up to ten game logs, newest first, with settings and crash details. ZIP stays below 24 MB; oversized current logs keep their header and latest events. Nothing is uploaded.");
            if (button("Create desktop shortcut", false, true, half)) action = UiAction::DesktopShortcut;
            dvr::ovl::tip("Keeps a copy of this launcher in your user profile, then creates a Desktop shortcut.");
            ImGui::SameLine();
            if (button("Create Start menu shortcut", false, true, half)) action = UiAction::StartShortcut;
            dvr::ovl::tip("Adds this launcher to your own Start menu. No administrator rights needed.");
            if (button("Uninstall", false, idle, half)) action = UiAction::Uninstall;
            dvr::ovl::tip("Removes the mod's files from the game folder. Your dishonored_vr.ini is kept unless you say otherwise.");
            ImGui::SameLine();
            if (button("Open game folder", false, true, half)) action = UiAction::OpenGameFolder;
        }
    }

    ImGui::EndChild();
    footer_begin(ImGui::GetFrameHeightWithSpacing() * 2.6f);
    if (!v.notice.empty()) status_slot("##footer-notice", 1, v.notice.c_str());
    else ImGui::TextDisabled("Settings apply at your next launch.");
    if (footer_button(discovery::launch_label(v.det.game.store), true, v.det.gameFound && v.det.running == process::Running::No)) action = UiAction::Launch;
    if (footer_button("Close")) action = UiAction::Close;
    if (footer_button("Open log")) action = UiAction::OpenLog;
    if (footer_button("Bindings")) action = UiAction::ShowGuide;
    if (footer_button("About")) action = UiAction::ShowAbout;
    return action;
}
UiAction draw_about(ViewState& v)
{
    UiAction action = UiAction::None;
    page_header("About Dishonored VR");
    ImGui::BeginChild("##about-body", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 1.8f));
    ImGui::Text("Version %s", v.det.version.c_str());
    wrapped_faded("A motion-controlled VR adventure in Dunwall.");
    dvr::ovl::ornament();
    ImGui::TextUnformatted("Credits");
    if (button("Pizza Parker / BioVRDev")) action = UiAction::CreditPizza;
    wrapped_faded("Mod development and continued support.");
    if (button("VOID / mohamad-balouza")) action = UiAction::CreditVoid;
    wrapped_faded("Mod development and contributions.");
    if (button("Gingas / GingasVRFO")) action = UiAction::CreditGingas;
    wrapped_faded("Creator of the original Dishonored VR mod.");
    dvr::ovl::ornament();
    if(button("Check for updates",true,!v.updateChecking && !v.updateDownloading && !v.busy))action=UiAction::CheckUpdates;
    wrapped_faded(v.updateMessage.empty()?"Checks for the latest stable release on GitHub.":v.updateMessage.c_str());
    if(!v.releases.empty() && updates::newer(v.releases.front().version,v.det.version)) {
        const auto& latest=v.releases.front();
        if(button(("Update to "+latest.version).c_str(),true,latest.downloadable() && !v.busy && !v.updateDownloading && v.det.running==process::Running::No))action=UiAction::DownloadUpdate;
        if(!latest.downloadable())banner("The release does not have a verified launcher download yet. Try again later.");
    }
    if(heading("Changelog history", "Recent stable GitHub releases. Saved locally for offline reading.")) {
        if(v.releases.empty()) {
            const auto notes=resources::rcdata(IDR_RELEASE_NOTES);
            if(notes.ok()) {const std::string text((const char*)notes.data,notes.size);wrapped(text.c_str());}
            else wrapped_faded("Check for updates to load release history.");
        }
        for(const auto& release:v.releases) {
            const std::string label=release.version+"  "+release.published;
            if(ImGui::CollapsingHeader(label.c_str()))wrapped(release.notes.empty()?"No changelog was provided for this release.":release.notes.c_str());
        }
    }
    if (button("GitHub releases")) action = UiAction::OpenReleases;
    dvr::ovl::ornament();
    wrapped("If you're enjoying the mod and feeling generous, you can");
    if (button("support it on Ko-fi", true)) action = UiAction::OpenKofi;
    wrapped("Thank you, it genuinely helps. Every donation goes toward the AI bills that make this work possible and into further development of this mod and the ones after it. Donating is never expected, and the mod will always be free.");
    ImGui::EndChild();
    dvr::ovl::ornament();
    if (button("Back to launcher")) action = UiAction::BackFromGuide;
    return action;
}

UiAction draw_guide(ViewState& v)
{
    page_header("Bindings");
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
    return button("Back to launcher", true) ? UiAction::BackFromGuide : UiAction::None;
}
} // namespace

void load_guide(ID3D11Device* device)
{
    release_guide();
    guideTexture = resources::image(device, IDR_CONTROLLER_GUIDE, &guideWidth, &guideHeight);
    DVR_INFO("launcher: bindings image %s (%ux%u)", guideTexture ? "loaded" : "FAILED", guideWidth, guideHeight);
}
void release_guide()
{
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
    switch (v.screen) {
    case Screen::Setup:  a = draw_setup(v); break;
    case Screen::Done:   a = draw_done(v); break;
    case Screen::Manage: a = draw_manage(v); break;
    case Screen::Guide: a = draw_guide(v); break;
    case Screen::About: a = draw_about(v); break;
    }
    if(v.updateDownloading) {
        ImGui::OpenPopup("Updating Dishonored VR");
        if(ImGui::BeginPopupModal("Updating Dishonored VR",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            wrapped("Downloading and verifying the new launcher. It will restart and install the mod update.");
            ImGui::EndPopup();
        }
    } else if(ImGui::BeginPopupModal("Updating Dishonored VR",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::CloseCurrentPopup();ImGui::EndPopup();
    }
    if(v.updatePopup && !v.releases.empty()) {
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
