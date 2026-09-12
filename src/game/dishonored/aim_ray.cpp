#define DVR_CAT ::dvr::log::Cat::present
#include "game/dishonored/aim_ray.h"
#include "core/vr/openxr_runtime.h"
#include "core/vr/openxr_input.h"
#include "core/framework/status.h"
#include "core/util/log.h"
#include <windows.h>
#include <imgui.h>
#include <cstdio>
#include <cstring>

namespace dvr::aim {
namespace {
Config g_config;
Ray g_ray;
const char* g_lastWhy = "";
uint64_t g_lastBeat = 0;
dvr::vr::AimVisualStats g_previous;
void log_status() {
    const auto s = dvr::vr::aim_visual_stats();
    DVR_INFO("crosshair: dot=%d laser=%d hand=%s fixed=%.2fm size=%.2fdeg ray=%s "
             "gen=%u renderer=%s publishes=%u submitted=%u dotFrames=%u beamFrames=%u "
             "runtimeLayerLimit=%u; guide only, no trace or projectile writes",
             g_config.dot, g_config.laser, g_config.hand ? "right" : "left",
             g_config.distanceM, g_config.sizeDeg, g_ray.why, g_ray.gen,
             dvr::vr::aim_visual_result_name(s.last), s.publishes, s.submitted,
             s.dotFrames, s.beamFrames, s.layerLimit);
}
} // namespace
Config config() { return g_config; }
Ray ray() { return g_ray; }
void configure(const Config& cfg, const char* origin) {
    if (cfg.hand < 0 || cfg.hand > 1 || !std::isfinite(cfg.distanceM) ||
        !std::isfinite(cfg.sizeDeg) || cfg.distanceM < 0.5f || cfg.distanceM > 50 ||
        cfg.sizeDeg < 0.05f || cfg.sizeDeg > 2) {
        DVR_WARN("crosshair: refused config from %s (hand=%d distance=%g size=%g); "
                 "keeping existing values, require left/right, 0.5..50m, 0.05..2deg",
                 origin, cfg.hand, cfg.distanceM, cfg.sizeDeg); return;
    }
    g_config = cfg;
    g_ray = Ray{};
    dvr::vr::set_aim_visual({}); // discard prior hand/config immediately, even mid-frame F10
    g_lastWhy = "";
    DVR_INFO("crosshair: config from %s Dot=%d Laser=%d Hand=%s DistanceM=%.2f SizeDeg=%.2f "
             "(XR LOCAL fixed-distance guide; native shots/reticle unchanged)", origin,
             cfg.dot, cfg.laser, cfg.hand ? "right" : "left", cfg.distanceM, cfg.sizeDeg);
}
void tick(bool gameplay, bool projectionWanted) {
    const auto now = GetTickCount64();
    const bool armed = g_config.dot || g_config.laser;
    dvr::vr::HandAimSample sample;
    g_ray = Ray{}; g_ray.hand = g_config.hand;
    if (!armed) g_ray.why = "off";
    else if (!dvr::vr::session_live()) g_ray.why = "no XR session";
    else if (!gameplay) g_ray.why = "menu/cinematic/gameplay unavailable";
    else if (!projectionWanted) g_ray.why = "method not requesting projection";
    else {
#if DVR_WITH_OPENXR
        sample = dvr::vr::input_hand_aim_sample(g_config.hand);
#endif
        g_ray = from_pose(g_config.hand, sample.aimValid, sample.aimPos, sample.aimQuat,
                          sample.generation, sample.stampMs, now);
    }
    auto out = visual(g_ray, g_config.dot, g_config.laser, g_config.distanceM, g_config.sizeDeg);
    dvr::vr::set_aim_visual(out);
    if (std::strcmp(g_lastWhy, g_ray.why)) {
        DVR_INFO("crosshair: ray %s (hand=%s, gen=%u); %s", g_ray.why,
                 g_config.hand ? "right" : "left", g_ray.gen,
                 !armed || !gameplay ? "zero visuals expected while off/in menus" : "renderer outcomes follow on beat");
        g_lastWhy = g_ray.why;
    }
    if (!armed || now - g_lastBeat < 1000) return;
    g_lastBeat = now;
    const Ray grip = from_pose(g_config.hand, sample.gripValid, sample.gripPos, sample.gripQuat,
                               sample.generation, sample.stampMs, now);
    float angle = -1;
    if (g_ray.ok && grip.ok) {
        float d = 0; for (int i = 0; i < 3; ++i) d += g_ray.dirXr[i] * grip.dirXr[i];
        angle = std::acos(d < -1 ? -1.0f : d > 1 ? 1.0f : d) * 57.2957795f;
    }
    const auto s = dvr::vr::aim_visual_stats();
    DVR_INFO("crosshair: hand=%s gen=%u age=%llu ms ray=%s aim=(%+.3f,%+.3f,%+.3f) "
             "grip=(%+.3f,%+.3f,%+.3f) aimGripDeg=%.2f (-1=unavailable; near zero is possible, not proof of aliasing) "
             "fixed=%.2fm barrelAngle=UNMEASURED (no calibrated weapon axis) "
             "window publish=%u submit=%u dot=%u beam=%u renderer=%s",
             g_config.hand ? "right" : "left", g_ray.gen, sample.stampMs ? now-sample.stampMs : 0,
             g_ray.why, g_ray.dirXr[0], g_ray.dirXr[1], g_ray.dirXr[2],
             grip.dirXr[0], grip.dirXr[1], grip.dirXr[2], angle, g_config.distanceM,
             s.publishes-g_previous.publishes, s.submitted-g_previous.submitted,
             s.dotFrames-g_previous.dotFrames, s.beamFrames-g_previous.beamFrames,
             dvr::vr::aim_visual_result_name(s.last));
    // Bounded enumeration of all outcomes, so an intermittently refused builder
    // cannot hide behind a later successful present. No per-present log spam.
    char reasons[512] = {}; int used = 0;
    for (int i = 0; i < (int)dvr::vr::AimVisualResult::Count; ++i) {
        const uint32_t n = s.outcomes[i] - g_previous.outcomes[i];
        if (n && used < (int)sizeof(reasons)-80)
            used += std::snprintf(reasons+used, sizeof(reasons)-used, " %s=%u;",
                dvr::vr::aim_visual_result_name((dvr::vr::AimVisualResult)i), n);
    }
    DVR_INFO("crosshair: renderer window:%s (pair-await is expected; submitted counts successful xrEndFrame, not visibility)",
             reasons[0] ? reasons : " none; zero is expected with no submit opportunities");
    g_previous = s;
}
void command(const char* args) {
    char a[32] = {}, b[32] = {}, extra[32] = {};
    std::sscanf(args, "%31s %31s %31s", a, b, extra);
    auto cfg = config(); bool changed = false;
    if (!extra[0]) {
        if ((!std::strcmp(a,"dot") || !std::strcmp(a,"laser")) &&
            (!std::strcmp(b,"on") || !std::strcmp(b,"off"))) {
            (a[0]=='d' ? cfg.dot : cfg.laser) = !std::strcmp(b,"on"); changed = true;
        } else if (!std::strcmp(a,"hand") && (!std::strcmp(b,"left") || !std::strcmp(b,"right"))) {
            cfg.hand = !std::strcmp(b,"right"); changed = true;
        } else if ((!std::strcmp(a,"distance") || !std::strcmp(a,"size")) && b[0]) {
            char trailing; float value;
            if (std::sscanf(b,"%f%c",&value,&trailing)==1) {
                (a[0]=='d' ? cfg.distanceM : cfg.sizeDeg)=value; changed=true;
            }
        } else if ((!a[0] || !std::strcmp(a,"status")) && !b[0]) { log_status(); return; }
    }
    if (changed) configure(cfg,"command seam");
    else DVR_WARN("crosshair: status | dot on|off | laser on|off | hand left|right | distance 0.5..50 | size 0.05..2");
    log_status();
}
void draw_ui() {
    auto cfg = config(); bool changed = false;
    ImGui::TextWrapped("Controller pointing guide. Fixed distance; shots still use the game's aim.");
    changed |= ImGui::Checkbox("Controller dot", &cfg.dot);
    changed |= ImGui::Checkbox("Controller beam", &cfg.laser);
    changed |= ImGui::RadioButton("Left hand", &cfg.hand, 0); ImGui::SameLine();
    changed |= ImGui::RadioButton("Right hand", &cfg.hand, 1);
    changed |= ImGui::SliderFloat("Guide distance (m)", &cfg.distanceM, 0.5f, 50.0f, "%.1f");
    changed |= ImGui::SliderFloat("Dot size (degrees)", &cfg.sizeDeg, 0.05f, 2.0f, "%.2f");
    if (changed) configure(cfg,"F10 Aim");
    ImGui::TextWrapped("Ray: %s. Renderer: %s.", g_ray.why,
        dvr::vr::aim_visual_result_name(dvr::vr::aim_visual_stats().last));
    ImGui::TextDisabled("No surface trace yet. Game reticle stays visible.");
}
void status(dvr::status::Writer& w) {
    const auto s = dvr::vr::aim_visual_stats();
    w.obj("crosshair"); w.kv("dot",g_config.dot); w.kv("laser",g_config.laser);
    w.kv("hand",g_config.hand ? "right" : "left");
    w.kv("distanceM",(double)g_config.distanceM); w.kv("sizeDeg",(double)g_config.sizeDeg);
    w.kv("ray",g_ray.why); w.kv("generation",(unsigned long)g_ray.gen);
    w.kv("renderer",dvr::vr::aim_visual_result_name(s.last));
    w.kv("submitted",(unsigned long)s.submitted); w.kv("dotFrames",(unsigned long)s.dotFrames);
    w.kv("beamFrames",(unsigned long)s.beamFrames); w.end_obj();
}
} // namespace dvr::aim
