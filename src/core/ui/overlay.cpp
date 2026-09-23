#include "core/ui/ovl_ui.h"
// VR-196: the legacy FOV lever (legacy_fov_control.inc) left the panel; [Camera] FovLever still works.
#include "core/framework/render_profile.h"
#include "game/dishonored/hands/sleeve_presets.h"
// core/ui/overlay.cpp - included by src/mod/dishonoredvr.cpp (unity build) until this
// module gets its own header and translation unit. Bodies are verbatim from
// the original single file; Line numbers in comments and docs refer to the original single file (src/dllmain.cpp at commit 48766c07, proxy build 38.92).


// ---- VR-174: the F10 panel from the motion controllers ------------------------------
// A port of the BioShock trilogy VR mod's src/core/ui/overlay.cpp (same ImGui 1.92.8,
// same runtime layer). docs/dishonored/F10_MOTION_CONTROLS.md has the plan and the four
// lessons it paid for; each is commented where it applies below.
#include <imgui_internal.h>   // SetActiveID: the slider tweak, see UpdateSliderTweak

// What the Win32 backend reported as the display size (the WINDOW CLIENT RECT) before
// it is overridden with the eye texture. Evidence only, for the geometry probe.
static ImVec2 g_ovlClientRect(0.0f, 0.0f);
// Was anything under the cursor last frame? Sampled at the end of the draw, read by the
// stick lane, which runs before NewFrame and cannot ask directly.
static bool g_ovlAnyHovered = false;
// A tracked controller owns the cursor (recency, 500 ms). While it does the real mouse
// is not queued.
static bool g_ovlCtrlPointing = false;
// The stick-driven slider tweak: wanted (set before NewFrame) and the item we activated.
static bool g_ovlTweakWant = false;
static ImGuiID g_ovlTweakId = 0;
static const float kOvlTweakMinPerSec = 2.0f, kOvlTweakMaxPerSec = 30.0f;

// Rotate v by the CONJUGATE of q (world -> head frame).
static void OvlRotateByConj(const float q[4], const float v[3], float out[3])
{
    const float x = -q[0], y = -q[1], z = -q[2], w = q[3];
    const float tx = 2.0f * (y * v[2] - z * v[1]);
    const float ty = 2.0f * (z * v[0] - x * v[2]);
    const float tz = 2.0f * (x * v[1] - y * v[0]);
    out[0] = v[0] + w * tx + (y * tz - z * ty);
    out[1] = v[1] + w * ty + (z * tx - x * tz);
    out[2] = v[2] + w * tz + (x * ty - y * tx);
}
// The XR forward (0,0,-1) rotated by q: the aim ray's direction in XR space.
static void OvlForwardOf(const float q[4], float out[3])
{
    const float v[3] = { 0.0f, 0.0f, -1.0f };
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    const float tx = 2.0f * (y * v[2] - z * v[1]);
    const float ty = 2.0f * (z * v[0] - x * v[2]);
    const float tz = 2.0f * (x * v[1] - y * v[0]);
    out[0] = v[0] + w * tx + (y * tz - z * ty);
    out[1] = v[1] + w * ty + (z * tx - x * tz);
    out[2] = v[2] + w * tz + (x * ty - y * tx);
}

// The cursor from the pointing controller, the click from its trigger, scroll and
// slider tweak from its stick. Runs AFTER ImGui_ImplWin32_NewFrame (which queues the real
// cursor) and BEFORE ImGui::NewFrame (which latches the queue): the only window in which
// injected input survives. The overlay's own thread, never the pad thread (31.1).
//
// EVERYTHING GOES THROUGH THE EVENT QUEUE. Since ImGui 1.87 io.MousePos / io.MouseDown[]
// are rebuilt from queued events inside NewFrame, so the direct writes the 31.0 pointer
// made were lost: the cursor survived only while the real mouse was still, and the
// trigger could never click (the queue always carried the real button state).
//
// THE PROJECTION. The panel is drawn flat into the eye texture, so the cursor is just the
// ray's angle off the head's forward, divided by the eye's tangents - the tangents the
// projection layer is TAGGED with (fov_audit), which are the symmetric FOV the game
// rendered with. If the cursor ever sits a constant factor off the ray, that factor is a
// mismatch between the rendered and the claimed FOV, not a redesign.
static void OvlInjectControllerPointer(float w, float h)
{
    if (!g_ovlPtrEnable) return;
    ImGuiIO& io = ImGui::GetIO();
    const int hand = (g_ovlPtrHand >= 0 && g_ovlPtrHand <= 1) ? g_ovlPtrHand : 1;
    const char* why = nullptr;
    bool haveRay = false;
    float px = 0.0f, py = 0.0f, ndcX = 0.0f, ndcY = 0.0f, tanH = 0.0f, tanV = 0.0f;
#if DVR_WITH_OPENXR
    dvr::vr::HeadPose head{}, aim{};
    if (!dvr::vr::peek_head_pose(head)) why = "no head pose";
    else if (!dvr::vr::get_hand_pose(hand, true, aim)) why = "pointing hand not tracked";
    else {
        float dirWorld[3], d[3];
        OvlForwardOf(&aim.qx, dirWorld);
        OvlRotateByConj(&head.qx, dirWorld, d);
        const float fwd = -d[2];                      // XR head frame: -Z forward
        dvr::vr::fov_audit(&tanH, &tanV, nullptr, nullptr, nullptr);
        if (!(fwd > 0.05f)) why = "the ray points behind the head";
        else if (!(tanH > 0.0f && tanV > 0.0f)) why = "no projection FOV yet (fov_audit is empty)";
        else if (!(w > 0.0f && h > 0.0f)) why = "no eye texture size";
        else {
            ndcX = (d[0] / fwd) / tanH;               // -1 left .. +1 right
            ndcY = (d[1] / fwd) / tanV;               // -1 down .. +1 up
            px = (ndcX * 0.5f + 0.5f) * w;
            py = (0.5f - ndcY * 0.5f) * h;
            haveRay = true;
        }
    }
#else
    why = "built without OpenXR";
#endif
    // HOLD THE LAST GOOD POSITION. The panel is drawn once per eye texture, so a pose read
    // that fails on alternate passes would draw the cursor in one eye only (the trilogy's
    // "cursor only renders in the right eye" report). A cached position keeps the eyes
    // together; OWNERSHIP still expires, so a controller set down hands the mouse back.
    static bool s_havePtr = false;
    static ImVec2 s_ptr(0.0f, 0.0f);
    static ULONGLONG s_lastRayMs = 0;
    const ULONGLONG nowMs = GetTickCount64();
    if (haveRay) { s_havePtr = true; s_ptr = ImVec2(px, py); s_lastRayMs = nowMs; }
    const bool pointing = s_lastRayMs != 0 && nowMs - s_lastRayMs < 500;
    if (pointing != g_ovlCtrlPointing) {
        Log("overlay: pointer owner %s (hand %s) - %s", pointing ? "CONTROLLER" : "MOUSE",
            hand ? "right" : "left",
            pointing ? "the ray drives the cursor, the trigger clicks, the stick scrolls and tweaks"
                     : (why ? why : "no ray for 500 ms"));
        g_ovlCtrlPointing = pointing;
    }
    DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Info, 5000,
        "overlay: pointer %s - ndc (%.3f,%.3f) -> px (%.0f,%.0f) of %.0fx%.0f, tangents %.3f/%.3f%s%s",
        haveRay ? "on the ray" : "held", ndcX, ndcY, s_ptr.x, s_ptr.y, w, h, tanH, tanV,
        why ? " | " : "", why ? why : "");
    if (!pointing) { g_ovlTweakWant = false; return; }

    dvr::vr::InputSnapshot in{};
#if DVR_WITH_OPENXR
    dvr::vr::input_snapshot(&in);
#endif
    if (s_havePtr) io.AddMousePosEvent(s_ptr.x, s_ptr.y);
    if (!in.active) { g_ovlTweakWant = false; return; }

    // The pointing hand's stick: Y scrolls, X tweaks whatever slider the cursor is over.
    // DOMINANT AXIS, so a scroll never nudges a value on the way past.
    const float* stick = hand ? in.lk : in.mv;
    const float trig = hand ? in.trigR : in.trigL;
    const float sx = stick[0], sy = stick[1];
    const float kDead = 0.25f;
    const bool xLane = fabsf(sx) > kDead && fabsf(sx) >= fabsf(sy);
    const bool yLane = fabsf(sy) > kDead && fabsf(sy) > fabsf(sx);
    const float dt = io.DeltaTime > 0.0f ? io.DeltaTime : 1.0f / 120.0f;

    // THE TWEAK. Sliders position ABSOLUTELY on a click (clicked_around_grab), so a
    // synthesised click-drag reset the value to the aim point. The keyboard path is
    // RELATIVE: it starts from the current value and one arrow press is 1% of the range.
    // The key is PULSED once per due step, so the rate is ours (squared: fine near the
    // centre, useful travel at full push) and not ImGui's repeat timer.
    static bool s_keyDown = false;
    static ImGuiKey s_key = ImGuiKey_RightArrow;
    static float s_stepAcc = 0.0f;
    const bool tweaking = xLane && (g_ovlTweakId != 0 || g_ovlAnyHovered);
    if (tweaking) {
        const float mag = fabsf(sx);
        s_stepAcc += (kOvlTweakMinPerSec + mag * mag * (kOvlTweakMaxPerSec - kOvlTweakMinPerSec)) * dt;
        const ImGuiKey want = sx > 0.0f ? ImGuiKey_RightArrow : ImGuiKey_LeftArrow;
        if (want != s_key && s_keyDown) { io.AddKeyEvent(s_key, false); s_keyDown = false; }
        s_key = want;
        if (s_keyDown) { io.AddKeyEvent(s_key, false); s_keyDown = false; }
        else if (s_stepAcc >= 1.0f) { s_stepAcc -= 1.0f; io.AddKeyEvent(s_key, true); s_keyDown = true; }
        if (s_stepAcc > 4.0f) s_stepAcc = 4.0f;                // never bank a burst
        g_ovlTweakWant = true;
        io.AddMouseButtonEvent(0, false);                     // never let the trigger steal the item
        return;
    }
    if (s_keyDown) { io.AddKeyEvent(s_key, false); s_keyDown = false; }
    s_stepAcc = 0.0f;
    g_ovlTweakWant = false;
    // Per SECOND: the panel draws once per eye texture and the rate swings with the scene.
    if (yLane) io.AddMouseWheelEvent(0.0f, sy * 2.5f * dt);
    io.AddMouseButtonEvent(0, trig > 0.5f);                   // half travel, past any rest noise
}

// Hold or release the tweak target. After NewFrame (HoveredWindow is updated there, and an
// ActiveId set before it would not survive) and before any slider is submitted.
static void OvlUpdateSliderTweak()
{
    ImGuiContext* ctx = ImGui::GetCurrentContext();
    if (!ctx) return;
    ImGuiContext& g = *ctx;
    static int s_steps = 0;
    if (g_ovlTweakWant && g_ovlTweakId == 0) {
        // HoveredIdPreviousFrame: NewFrame has moved this frame's HoveredId out of the way
        // and no item has been submitted yet.
        const ImGuiID id = g.HoveredIdPreviousFrame;
        if (id != 0 && g.HoveredWindow != nullptr) {
            ImGui::SetActiveID(id, g.HoveredWindow);
            g.ActiveIdSource = ImGuiInputSource_Keyboard;
            g.NavInputSource = ImGuiInputSource_Keyboard;       // picks the arrow keys
            g_ovlTweakId = id;
            s_steps = 0;
            Log("overlay: tweak START item 0x%08X (stick, 1%% of its range per step, %.0f-%.0f steps/s)",
                (unsigned)id, kOvlTweakMinPerSec, kOvlTweakMaxPerSec);
        }
    } else if (!g_ovlTweakWant && g_ovlTweakId != 0) {
        if (g.ActiveId == g_ovlTweakId) ImGui::ClearActiveID();  // only ever clear OUR item
        Log("overlay: tweak STOP item 0x%08X", (unsigned)g_ovlTweakId);
        g_ovlTweakId = 0;
    } else if (g_ovlTweakId != 0 && g.ActiveId != g_ovlTweakId) {
        Log("overlay: tweak LOST item 0x%08X (something else took it, or it is gone)",
            (unsigned)g_ovlTweakId);
        g_ovlTweakId = 0;
    }
}

// Where the window actually ends up, as FRACTIONS of the eye texture, so the number that
// gets baked in is not tied to one resolution. Debounced, on change.
static void OvlProbeWindowGeometry(float w, float h)
{
    static float s_lx = -1, s_ly = -1, s_lw = -1, s_lh = -1;
    static ULONGLONG s_nextMs = 0;
    const ImVec2 p = ImGui::GetWindowPos(), s = ImGui::GetWindowSize();
    const bool moved = fabsf(p.x - s_lx) > 1.0f || fabsf(p.y - s_ly) > 1.0f ||
                       fabsf(s.x - s_lw) > 1.0f || fabsf(s.y - s_lh) > 1.0f;
    if (!moved || !(w > 0 && h > 0)) return;
    const ULONGLONG now = GetTickCount64();
    if (now < s_nextMs) return;
    s_nextMs = now + 1000;
    s_lx = p.x; s_ly = p.y; s_lw = s.x; s_lh = s.y;
    Log("overlay: window pos %.0f,%.0f size %.0fx%.0f | eye texture %.0fx%.0f | fractions pos "
        "%.4f,%.4f size %.4f,%.4f | text scale %.2f", p.x, p.y, s.x, s.y, w, h,
        p.x / w, p.y / h, s.x / w, s.y / h, ImGui::GetStyle().FontScaleMain);
    static bool s_toldClip = false;
    if (!s_toldClip && g_ovlClientRect.x > 0.0f && g_ovlClientRect.y > 0.0f) {
        s_toldClip = true;
        Log("overlay: window client rect %.0fx%.0f vs eye texture %.0fx%.0f - before VR-174 ImGui "
            "drew in client-rect coordinates, so the panel covered %.0f%% x %.0f%% of the eye image "
            "from its top-left corner; it is now sized and placed against the eye texture",
            g_ovlClientRect.x, g_ovlClientRect.y, w, h,
            100.0f * g_ovlClientRect.x / w, 100.0f * g_ovlClientRect.y / h);
    }
}

#include "core/ui/overlay_tabs.inc"   // VR-196: one function per tab

static void OverlayFrame(uint32_t targetW, uint32_t targetH)
{
    if (!g_ovlVisible) return;
    if (!g_ovlInit) {
        if (!g_dev11 || !g_ctx11 || !g_gameWnd) { g_ovlVisible = false; return; }
        ImGui::CreateContext();
        dvr::ovl::load_fonts();                  // VR-197: Segoe UI body, Constantia headings
        dvr::ovl::apply_theme();                 // VR-197: the Dishonored palette and metrics
        ImGui::GetStyle().ScaleAllSizes(1.6f);   // readable at headset distance
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = NULL;                   // no imgui.ini clutter
        // NO EVENT TRICKLING. It exists to spread a burst of real-hardware events over
        // frames; ours are synthetic and already one per frame. With it on, the wheel
        // branch defers whenever the mouse moved - and a ray cursor moves every frame -
        // so scroll events queued up and drained after the stick was released.
        io.ConfigInputTrickleEventQueue = false;
        ImGui_ImplWin32_Init(g_gameWnd);
        ImGui_ImplDX11_Init(g_dev11, g_ctx11);
        InstallWindowSubclass("overlay init");   // 38.92: no-op if already on
        g_ovlInit = true;
        Log("overlay: initialized (F10 toggles; stick-click tap toggles when [Overlay] "
            "ControllerPointer=1, now %s)", g_ovlPtrEnable ? "on" : "off");
    }
    dvr::ovl::load_art(g_dev11);
    ImGuiIO& io = ImGui::GetIO();
    const float w = (float)targetW, h = (float)targetH;
    // Match the reference typography to the default square panel. An explicit
    // saved UiScale still wins; the player keeps control of text size.
    if (g_ovlUiScale <= 0.0f) {
        const float panelPixels = (w < h ? w : h) * .46f;
        float fs = panelPixels > 0 ? 1.54f * panelPixels / 1254.0f : 1.54f;
        if (fs < .8f) fs = .8f;
        if (fs > 2.5f) fs = 2.5f;
        g_ovlUiScale = fs;
        Log("overlay: text scale %.2f from reference panel %.0f px ([Overlay] UiScale overrides)",
            fs, panelPixels);
    }
    ImGui::GetStyle().FontScaleMain = g_ovlUiScale;   // 1.92: replaces io.FontGlobalScale
    io.MouseDrawCursor = true;                        // ImGui draws the cursor, both eyes
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    // THE DISPLAY IS THE EYE TEXTURE. The Win32 backend sets DisplaySize from the WINDOW
    // CLIENT RECT and the DX11 backend sets its viewport from DisplaySize, but the panel is
    // drawn into the eye texture - so it used to live in a client-rect-sized corner of it.
    // Point ImGui at the surface it renders to, and rescale the real cursor into it (as an
    // event: a direct io.MousePos write would be overwritten by NewFrame). The controller
    // queues after this and wins whenever a hand is tracked.
    if (w > 0.0f && h > 0.0f) {
        const ImVec2 client = io.DisplaySize;
        g_ovlClientRect = client;
        POINT cur{};
        if (!g_ovlCtrlPointing && client.x > 0.0f && client.y > 0.0f && g_gameWnd &&
            GetCursorPos(&cur) && ScreenToClient(g_gameWnd, &cur))
            io.AddMousePosEvent((float)cur.x * w / client.x, (float)cur.y * h / client.y);
        io.DisplaySize = ImVec2(w, h);
    }
    OvlInjectControllerPointer(io.DisplaySize.x, io.DisplaySize.y);
    ImGui::NewFrame();
    OvlUpdateSliderTweak();

    // Reference composition: centered square against the eye texture. A resize
    // changes the layout without changing widget behavior or the stored settings.
    const ImVec2 ds = io.DisplaySize;
    const ImGuiCond placeCond = InterlockedExchange(&g_ovlRecenter, 0) ? ImGuiCond_Always
                                                                       : ImGuiCond_FirstUseEver;
    // VR-206: square reference composition, centered and resizable. Settings scroll.
    const float shorter = ds.x < ds.y ? ds.x : ds.y;
    const float readableMin = (ImGui::CalcTextSize("RESET TO DEFAULTS").x + ImGui::GetStyle().FramePadding.x*2)*3
        + ImGui::GetStyle().ItemSpacing.x*2 + ImGui::GetStyle().WindowPadding.x*2;
    const float minSide = (readableMin < shorter*.95f) ? readableMin : shorter*.95f;
    const float panelSide = shorter*.46f > minSide ? shorter*.46f : minSide;
    ImGui::SetNextWindowPos(ImVec2((ds.x-panelSide)*0.5f, (ds.y-panelSide)*0.5f), placeCond);
    ImGui::SetNextWindowSize(ImVec2(panelSide, panelSide), placeCond);
    ImGui::SetNextWindowSizeConstraints(ImVec2(minSide,minSide), ImVec2(ds.x*0.95f,ds.y*0.95f));
    // VR-197: no ImGui title bar; OvlTopRow draws the themed title and the close button, and
    // the window still moves by dragging any empty part of it.
    ImGui::Begin("Dishonored VR", &g_ovlVisible,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);
    dvr::ovl::backdrop();
    OvlProbeWindowGeometry(ds.x, ds.y);
    if (g_ovlReticle && ds.x > 0.0f && ds.y > 0.0f) {   // the reticle hides behind this rectangle
        const ImVec2 wp = ImGui::GetWindowPos(), ws = ImGui::GetWindowSize();
        dvr::vr::set_aim_occluder(true, wp.x / ds.x, wp.y / ds.y, (wp.x + ws.x) / ds.x, (wp.y + ws.y) / ds.y);
    }

    OvlTopRow();   // VR-196: the view level, recenter, height, save (overlay_tabs.inc)
    ImGui::BeginDisabled(ConfigResetPending());
    OvlTabs();
    ImGui::Spacing();
    dvr::ovl::ornament();   // VR-197: the brass rule that closes the panel
    // VR-174: text size is perceptual, so it is a slider, saved at once.
    if (dvr::ovl::slider_float("Text size", &g_ovlUiScale, 0.8f, 2.5f, "%.2f")) {
        char v[16]; _snprintf(v, sizeof(v) - 1, "%.2f", g_ovlUiScale); v[sizeof(v) - 1] = 0;
        ConfigWriteKey("Overlay", "UiScale", v, "F10");
    }
    OvlTip("Size of this panel's text. Drag the window's edge to resize the panel itself.");
    ImGui::EndDisabled();
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled(g_ovlPtrEnable ? "F10 or a stick-click tap closes | point, trigger clicks, "
                                         "stick scrolls / nudges a slider"
                                       : "F10 closes");
    ImGui::PopTextWrapPos();
    // Sampled here, where it is meaningful; read by the stick lane before the next NewFrame.
    g_ovlAnyHovered = ImGui::IsAnyItemHovered() || ImGui::IsAnyItemActive();
    ImGui::End();

    // The cursor only where the panel is (or while a drag that started on it is held). Off the
    // panel the reticle is what the player is looking at, and a cursor drawn over the whole eye
    // image sat on top of it.
    io.MouseDrawCursor = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenBlockedByPopup) ||
                         ImGui::IsAnyItemActive();
    ImGui::Render();
}
