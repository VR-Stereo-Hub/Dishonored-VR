// tools/installer/ui/screens.cpp - see screens.h.
#include "ui/screens.h"
#include "ui/widgets.h"
#include "sys/fs.h"
#include "core/ui/ovl_ui.h"
#include <stdio.h>
#include <string>

namespace dvr::setup::ui {

namespace {
std::string n(const std::wstring& w) { return fs::narrow(w); }

const char* kRuntimeTips[3] = {
    "Pins Virtual Desktop's own OpenXR runtime (VDXR) for this game. In the Streamer set 90 Hz and SSW off: the tested render size was judged at 90 and ghosts at 120.",
    "Index, Vive, WMR through SteamVR, Quest over Link or Steam Link. The mod brings its own bridge (dvr_steamvr32.dll); start SteamVR before the game.",
    "The mod tries the 32-bit OpenXR runtime Windows registers and falls back to the SteamVR bridge when there is none. Pick this when unsure.",
};
const char* kQualityTips[3] = {
    "75% of the tested pixels, both axes scaled together. For an 8 GB card, or when Balanced stutters.",
    "2750x2850 per eye, the size this build was tuned and judged at, with the headset at 90 Hz. Start here.",
    "120% of the tested pixels. Sharper and slower; judged on a 4070 Ti SUPER class card. Not the place to start.",
};

void game_section(ViewState& v, UiAction* action)
{
    if (!heading("Game", "Where Dishonored.exe is. The mod goes beside it.")) return;
    const float bw = ImGui::CalcTextSize("Change...").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::AlignTextToFramePadding();
    ImGui::BeginChild("##gamepath", ImVec2(ImGui::GetContentRegionAvail().x - bw - ImGui::GetStyle().ItemSpacing.x, ImGui::GetFrameHeight()),
                      ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
    ImGui::AlignTextToFramePadding();
    if (v.det.gameFound) path_text(n(v.det.gameDir).c_str());
    else ImGui::TextDisabled("Not found");
    ImGui::EndChild();
    ImGui::SameLine();
    if (button("Change...", false, !v.busy)) *action = UiAction::Browse;
    dvr::ovl::tip("Point at the folder holding Dishonored.exe (Binaries\\Win32) if it is not in a Steam library, or on Epic.");

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
    status_slot("##gamestatus", 2, status.c_str(), colour);
}

void headset_section(ViewState& v)
{
    if (!heading("Headset", "Which OpenXR runtime the mod talks to. This writes [VR] Runtime and XrRuntimeJson in dishonored_vr.ini.")) return;
    const char* labels[3] = { runtime_label(Runtime::Vdxr), runtime_label(Runtime::SteamVr), runtime_label(Runtime::Auto) };
    const int hit = pill_row(labels, 3, (int)v.choices.runtime, kRuntimeTips);
    if (hit >= 0 && !v.busy) v.choices.runtime = (Runtime)hit;
    std::string line;
    if (v.choices.runtime == Runtime::Vdxr)
        line = v.det.vdxrPresent ? "Virtual Desktop Streamer found. Set it to 90 Hz with SSW off."
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
    if (!heading("Render quality", "How many pixels the game renders per eye. This writes [Screen] RenderWidth and RenderHeight; the F10 panel's Display tab can change it later.")) return;
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
    line += " Set the headset to 90 Hz.";
    status_slot("##qualitystatus", 1, line.c_str());
}

void advanced_section(ViewState& v)
{
    if (v.advancedOpen) ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    if (!heading("Advanced", "The exact pixel budget, the same slider as the F10 Display tab.", false)) return;
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
    page_header(v.changingSettings ? "Change the headset and render size for the installed mod."
                                   : "Installs the VR mod beside the game and sets it up for your headset.");
    if (v.det.embeddedLegacy) banner("This build carries the retired diagnostics (legacy): it stalls on every trigger pull. Use a release build for play.");
    else if (v.det.config != "RelWithDebInfo") banner(("This is a " + v.det.config + " build of the installer and the mod: slower, for testing, not for play.").c_str());

    if (v.busy) {
        spinner(v.busyText.c_str());
        return action;
    }
    game_section(v, &action);
    headset_section(v);
    quality_section(v);
    advanced_section(v);

    footer_begin(ImGui::GetFrameHeightWithSpacing() * 2.6f);
    if (!v.notice.empty()) wrapped_faded(v.notice.c_str());
    else ImGui::TextDisabled("Nothing else to set up: every other setting lives in the F10 panel inside the game.");
    const bool canGo = v.det.gameFound && v.det.running == process::Running::No && (v.det.gameWritable || v.det.needsElevation) && v.det.payloadOk;
    if (v.changingSettings) {
        if (footer_button("Apply", true, canGo || (v.det.gameFound && v.det.iniExists))) action = UiAction::Install;
        if (footer_button("Back")) action = UiAction::CancelChange;
    } else {
        if (footer_button(v.det.needsElevation ? "Install (administrator)" : "Install", true, canGo)) action = UiAction::Install;
        if (footer_button("Cancel")) action = UiAction::Close;
    }
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
                    : "Installed and ready. Launch the game from Steam.";
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
    if (showPlay) below += headingH + 3.0f * (2.0f * lineH + st.ItemSpacing.y) + st.ItemSpacing.y;
    if (v.report.baselinePending) below += ImGui::GetFrameHeightWithSpacing() + st.ItemSpacing.y;
    float listH = ImGui::GetContentRegionAvail().y - below;
    if (listH < lineH * 4.0f) listH = lineH * 4.0f;
    if (heading("What happened", nullptr)) {
        ImGui::BeginChild("##steps", ImVec2(0, listH), ImGuiChildFlags_None);
        for (const auto& s : v.report.steps) step_row(s);
        ImGui::EndChild();
    }
    if (showPlay && heading("Before you play", nullptr)) {
        static const StepResult tips[] = {
            { "Launch Dishonored from Steam.", "A direct Dishonored.exe launch crashes at the main menu.", StepStatus::Ok },
            { "F5 recenters. F10 opens the settings panel; everything else is tuned there.", "Height, hands, reticle, comfort, HUD and display, with Basic, Advanced and Debug views.", StepStatus::Ok },
            { "Turn Motion Blur off in the game's own options.", "It lives in the Steam profile, not in a file this installer can write.", StepStatus::Ok },
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
    if (!v.notice.empty()) wrapped_faded(v.notice.c_str());
    else ImGui::TextDisabled("%s", ("Log: " + v.logPath).c_str());
    if (showPlay) { if (footer_button("Launch Dishonored", true, !v.busy)) action = UiAction::Launch; }
    if (footer_button("Close")) action = UiAction::Close;
    if (footer_button("Open game folder", false, v.det.gameFound)) action = UiAction::OpenGameFolder;
    return action;
}

UiAction draw_manage(ViewState& v)
{
    UiAction action = UiAction::None;
    const bool sameBuild = v.det.installedIsEmbedded();
    std::string sub = v.det.record.valid
        ? fs::format("Installed: %s (build %s), %s.", v.det.record.version.c_str(), v.det.record.buildId.c_str(), v.det.record.installedUtc.substr(0, 10).c_str())
        : "Installed by hand (no install record).";
    sub += sameBuild ? " This is the same build." : fs::format(" This installer carries %s (build %s).", v.det.version.c_str(), v.det.buildId.c_str());
    page_header(sub.c_str());
    if (v.det.embeddedLegacy) banner("This build carries the retired diagnostics (legacy): it stalls on every trigger pull. Use a release build for play.");
    else if (v.det.config != "RelWithDebInfo") banner(("This is a " + v.det.config + " build of the installer and the mod: slower, for testing, not for play.").c_str());
    if (v.busy) { spinner(v.busyText.c_str()); return action; }

    if (heading("Game", nullptr)) {
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
        status_slot("##managestatus", 2, line.c_str());
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
            const std::string updateLabel = sameBuild ? "Reinstall this build" : fs::format("Update to %s (build %s)", v.det.version.c_str(), v.det.buildId.c_str());
            if (button(updateLabel.c_str(), !sameBuild, idle, half)) action = UiAction::Update;
            dvr::ovl::tip(sameBuild ? "Writes the same three DLLs again. Your settings stay." : "Replaces the three DLLs with this installer's. Your settings stay.");
            ImGui::SameLine();
            if (button("Change settings", false, v.det.iniExists, half)) action = UiAction::ChangeSettings;
            dvr::ovl::tip("The headset runtime and the render size, and the four game settings again if the game's own video options reset them.");
            if (button(v.det.disabled ? "Enable VR" : "Disable VR", false, true, half)) action = UiAction::ToggleDisable;
            dvr::ovl::tip("Disable leaves the mod installed and switches it off with a disable_vr.txt beside the game, so Dishonored runs flat. Enable removes the file.");
            ImGui::SameLine();
            if (button("Collect support bundle", false, v.det.iniExists, half)) action = UiAction::CollectSupport;
            dvr::ovl::tip("Zips the mod's log, ini and crash report into a folder on your Desktop for a bug report. Nothing is uploaded.");
            if (button("Uninstall", false, idle, half)) action = UiAction::Uninstall;
            dvr::ovl::tip("Removes the mod's files from the game folder. Your dishonored_vr.ini is kept unless you say otherwise.");
            ImGui::SameLine();
            if (button("Open game folder", false, true, half)) action = UiAction::OpenGameFolder;
        }
    }

    footer_begin(ImGui::GetFrameHeightWithSpacing() * 2.6f);
    if (!v.notice.empty()) wrapped_faded(v.notice.c_str());
    else ImGui::TextDisabled("%s", ("Log: " + v.logPath).c_str());
    if (footer_button("Close")) action = UiAction::Close;
    if (footer_button("Releases page")) action = UiAction::OpenReleases;
    if (footer_button("Check again")) action = UiAction::Rescan;
    return action;
}
} // namespace

UiAction draw(ViewState& v)
{
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar);
    UiAction a = UiAction::None;
    switch (v.screen) {
    case Screen::Setup:  a = draw_setup(v); break;
    case Screen::Done:   a = draw_done(v); break;
    case Screen::Manage: a = draw_manage(v); break;
    }
    ImGui::End();
    return a;
}

} // namespace dvr::setup::ui
