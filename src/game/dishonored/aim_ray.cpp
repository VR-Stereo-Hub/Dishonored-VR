#define DVR_CAT ::dvr::log::Cat::present
#include "game/dishonored/aim_ray.h"
#include "game/dishonored/hands/bolt_axis.h"
#include "game/dishonored/hands/hand_frame.h"   // VR-57: follow_trim_ray
#include "core/vr/openxr_runtime.h"
#include "core/vr/openxr_input.h"
#include "core/framework/status.h"
#include "core/util/log.h"
#include <windows.h>
#include <imgui.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <atomic>

namespace dvr::aim {
namespace {
Config g_config;
Ray g_ray;
std::mutex g_fireMutex;
FireFrame g_fireFrame;
std::atomic<bool> g_fireRequested{false};
std::atomic<bool> g_modelRequested{false};
const char* g_lastWhy = "";
uint64_t g_lastBeat = 0;
dvr::vr::AimVisualStats g_previous;
// VR-57 FollowHandTrim: why the transport did or did not happen, and which
// calibration revision it used. Present lane only.
const char* g_followWhy = g_config.modelRay ? "model geometry overrides trim transport" : "off";
bool        g_followUsed = false;
uint32_t    g_followRev = 0;
bool        g_modelRayUsed = false;   // the measured axis supplied this frame's ray
void log_status() {
    const auto s = dvr::vr::aim_visual_stats();
    const auto c = dvr::vr::control_dot_stats();
    DVR_INFO("crosshair: control dot=%d - frames %u, dots drawn %u, refused: noproj=%u "
             "noviews=%u notex=%u budget=%u geom=%u (frames counts PRESENTS while it is "
             "armed, dots counts QUADS submitted, so dots is about twice frames when both "
             "distances build; all zero while it is off, by design)",
             g_config.controlDot, c.frames, c.dots, c.refusedNoProjection,
             c.refusedNoViews, c.refusedNoTexture, c.refusedBudget, c.refusedGeometry);
    DVR_INFO("crosshair: dot=%d laser=%d hand=%s fixed=%.2fm size=%.2fdeg ray=%s "
             "gen=%u renderer=%s publishes=%u submitted=%u dotFrames=%u beamFrames=%u "
             "runtimeLayerLimit=%u; fixed-distance guide; FireFromHand separately controls native launch",
             g_config.dot, g_config.laser, g_config.hand ? "right" : "left",
             g_config.distanceM, g_config.sizeDeg, g_ray.why, g_ray.gen,
             dvr::vr::aim_visual_result_name(s.last), s.publishes, s.submitted,
             s.dotFrames, s.beamFrames, s.layerLimit);
}
} // namespace
Config config() { return g_config; }
bool model_ray_requested() { return g_modelRequested.load(); }
FireFrame fire_frame() { std::lock_guard<std::mutex> lock(g_fireMutex); return g_fireFrame; }
Ray ray() { return fire_frame().ray; }
void request_fire_ray(bool enabled) { g_fireRequested.store(enabled); }
void configure(const Config& cfg, const char* origin) {
    if (cfg.hand < 0 || cfg.hand > 1 || !std::isfinite(cfg.distanceM) ||
        !std::isfinite(cfg.sizeDeg) || cfg.distanceM < 0.5f || cfg.distanceM > 50 ||
        cfg.sizeDeg < 0.05f || cfg.sizeDeg > 2) {
        DVR_WARN("crosshair: refused config from %s (hand=%d distance=%g size=%g); "
                 "keeping existing values, require left/right, 0.5..50m, 0.05..2deg",
                 origin, cfg.hand, cfg.distanceM, cfg.sizeDeg); return;
    }
    g_config = cfg;
    g_modelRequested.store(cfg.modelRay);
    g_ray = Ray{};
    { std::lock_guard<std::mutex> lock(g_fireMutex); g_fireFrame = {}; }
    dvr::vr::set_aim_visual({}); // discard prior hand/config immediately, even mid-frame F10
    // The control dot is head-anchored and lives entirely in the runtime: it must
    // keep drawing when the hand ray is refused, which is half of what it is for.
    dvr::vr::set_control_dot({cfg.controlDot, 1.5f, cfg.distanceM, cfg.sizeDeg});
    g_lastWhy = "";
    DVR_INFO("crosshair: config from %s Dot=%d Laser=%d Hand=%s DistanceM=%.2f SizeDeg=%.2f "
             "ControlDot=%d (XR LOCAL fixed-distance guide; FireFromHand independently controls launch%s)",
             origin, cfg.dot, cfg.laser, cfg.hand ? "right" : "left", cfg.distanceM,
             cfg.sizeDeg, cfg.controlDot,
             cfg.controlDot ? "; the CONTROL dot is head-anchored straight ahead at "
                              "1.50 m and DistanceM, no controller in it - it must land on "
                              "the centre of the game's own image"
                            : "");
}
void tick(bool gameplay, bool projectionWanted) {
    const auto now = GetTickCount64();
    const bool armed = g_config.dot || g_config.laser || g_config.controlDot || g_fireRequested.load();
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
    // VR-57: THE TRANSPORT. One place, before BOTH publications, so the visual and
    // fire_frame() cannot diverge and no consumer re-reads the AIM pose to rebuild
    // the old ray behind our back.
    //
    // R_C and the untrimmed palm origin come from g_devPose[3+hand], the GRIP pose
    // the hand is actually built from - not the AIM pose this ray is seeded with.
    // They are 60 degrees apart on this hardware and substituting one silently
    // rotates everything.
    g_followWhy = "off";
    g_followUsed = false;
    // THE LADDER, in precedence order, and it never ends at head aim.
    //
    // 1. the measured model axis, when the weapon has one
    // 2. the controller ray carried by the hand trim, when it does not
    //
    // Falling back to the engine's own HEAD aim was the old behaviour and it was
    // wrong as a fallback: a weapon with no measurable geometry is still held in a
    // tracked hand, so the hand's own ray is always a better answer than the head's.
    // The pistol is the case that proves it - its loaded bullet has no usable
    // transform and its body mesh is far past the geometry reader's limits, so it
    // will never have a measured axis, and it should still aim where it is pointed.
    bool modelUsed = false;
    if (g_config.modelRay && g_ray.ok) {
        const auto model=dvr::hands::model_ray_snapshot(g_config.hand);
        const auto cal=dvr::hands::trim_snapshot(g_config.hand);
        dvr::hf::Mat3 rc,g;
        for(int i=0;i<9;++i){rc.m[i]=cal.R_C[i];g.m[i]=cal.G[i];}
        float mo[3],md[3];
        if(cal.ok && std::isfinite(cal.handToWorldScale) && cal.handToWorldScale>0 &&
           model.ok && now>=model.sampleMs && now-model.sampleMs<=250 &&
           dvr::hf::palm_ray_to_xr(rc,g,cal.p0,cal.trimRdeg,cal.trimTm,
                                   model.originPalm,model.dirPalm,mo,md)) {
            for(int i=0;i<3;++i){
                g_ray.originXr[i]=cal.headPos[i]+cal.handToWorldScale*(mo[i]-cal.headPos[i]);
                g_ray.dirXr[i]=md[i];
            }
            g_ray.why="measured model axis";
            modelUsed = true;
        }
    }
    g_modelRayUsed = modelUsed;

    // WAITING FOR THE AXIS IS NOT A REASON TO SHOW THE OTHER RAY.
    //
    // The controller ray carries the AIM-pose baseline offset, so showing it while the
    // model axis is still settling guarantees a visible jump the moment the axis
    // latches - which is exactly what the tester saw: correct for a second, then far
    // to the left for four or five, then correct again as the latch landed. The
    // apparent "correction" was the latch arriving, not a fault healing.
    //
    // So the controller ray is the fallback for ModelRay being OFF, not for ModelRay
    // being on and not yet ready. While it is pending, no guide is shown at all. No
    // guide is honest; a guide in the wrong place that later moves is not, and the
    // stated requirement is that a laser which starts correct must never move.
    if (g_config.modelRay && !modelUsed && g_ray.ok) {
        g_ray.ok = false;
        g_ray.why = "waiting for the shared bolt axis to settle (equip the crossbow "
                    "with ordinary bolts once; no guide is shown until it is measured, "
                    "so it cannot appear in the wrong place and then move)";
    }
    if (!modelUsed && !g_config.modelRay && g_config.followHandTrim && g_ray.ok) {
        const int h = g_config.hand;
        const dvr::hands::TrimSnapshot cal = dvr::hands::trim_snapshot(h);
        if (!cal.ok) {
            g_followWhy = cal.why;
            g_ray.ok = false; g_ray.why = cal.why;
        } else {
            dvr::hf::FollowTrimIn in;
            for (int i = 0; i < 9; i++) { in.R_C.m[i] = cal.R_C[i]; in.G.m[i] = cal.G[i]; }
            for (int r = 0; r < 3; r++) {
                in.p0[r]       = cal.p0[r];
                in.trimRdeg[r] = cal.trimRdeg[r];
                in.trimTm[r]   = cal.trimTm[r];
                in.origin0[r]  = g_ray.originXr[r];
                in.dir0[r]     = g_ray.dirXr[r];
            }
            dvr::hf::FollowTrimOut outT;
            if (!dvr::hf::follow_trim_ray(in, outT)) {
                g_followWhy = outT.why;
                // REFUSE the ray rather than publish the untransported one while the
                // mode claims to follow the hand. A guide that silently shows the
                // controller while firing consumes something else is worse than no
                // guide, and the native fire hook keeps native aim through its own
                // guards when the ray is invalid.
                g_ray.ok = false;
                g_ray.why = "follow-hand-trim refused";
            } else {
                for (int r = 0; r < 3; r++) {
                    g_ray.originXr[r] = outT.origin[r];
                    g_ray.dirXr[r]    = outT.dir[r];
                }
                g_followUsed = !outT.identity;
                g_followRev  = cal.revision;
                g_followWhy  = outT.identity ? "zero trim, ray unchanged"
                                             : "following the hand trim";
            }
        }
        if (!g_followUsed && g_ray.ok && std::strcmp(g_followWhy, "zero trim, ray unchanged"))
            DVR_LOG_EVERY_MS(DVR_CAT, ::dvr::log::Level::Warn, 5000,
                "crosshair/follow: FollowHandTrim is on but the ray is NOT following "
                "the hand - %s. The shared ray is unavailable; "
                "native firing retains its original direction.",
                g_followWhy);
    }

    FireFrame frame;
    frame.ray = g_ray; frame.distanceM = g_config.distanceM;
    dvr::vr::HeadPose fireHead;
    if (g_ray.ok && dvr::vr::peek_head_pose(fireHead)) {
        frame.headValid = true;
        frame.headPos[0] = fireHead.px; frame.headPos[1] = fireHead.py; frame.headPos[2] = fireHead.pz;
        frame.headQuat[0] = fireHead.qx; frame.headQuat[1] = fireHead.qy;
        frame.headQuat[2] = fireHead.qz; frame.headQuat[3] = fireHead.qw;
    }
    { std::lock_guard<std::mutex> lock(g_fireMutex); g_fireFrame = frame; }
    // controlDot never reaches this publication: it is built in the runtime from the
    // located views, so it cannot borrow the hand ray's freshness or its validity.
    auto out = visual(g_ray, g_config.dot, g_config.laser, g_config.distanceM, g_config.sizeDeg);
    if (g_config.bothPoses) {
        // BOTH rays, with the two ENDPOINTS published first so a tight layer
        // budget cannot drop the second one and hide half the comparison.
        // The fat dot and beam are the AIM pose, the small ones the GRIP pose.
        const Ray gripRay = from_pose(g_config.hand, sample.gripValid, sample.gripPos,
                                      sample.gripQuat, sample.generation, sample.stampMs, now);
        out = visual(g_ray, g_config.dot, false, g_config.distanceM, g_config.sizeDeg);
        visual_append(out, gripRay, g_config.dot, false, g_config.distanceM,
                      g_config.sizeDeg * 0.6f);
        visual_append(out, g_ray, false, g_config.laser, g_config.distanceM, g_config.sizeDeg);
        visual_append(out, gripRay, false, g_config.laser, g_config.distanceM,
                      g_config.sizeDeg * 0.6f);
        out.enabled = g_config.dot || g_config.laser;
        out.valid = g_ray.ok;
        out.generation = g_ray.gen; out.sampleMs = g_ray.sampleMs;
    }
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
    // The RAW XR direction is not what the eye measures: a dot placed in LOCAL
    // space appears at the angle off the HEAD, so the head-relative pair is the
    // number a report of "45 degrees left" can be compared against. A raw -34
    // with the head yawed 22 left is a head-relative -12 and the two readings
    // have been confused once already.
    // TWO head-relative bearings, because they are different quantities and the
    // difference between them matters:
    //   dirAz/dirEl  - where the controller POINTS, the ray turned into the head's
    //                  frame. This is what "point it straight ahead and it should
    //                  read zero" is about.
    //   dotAz/dotEl  - where the DOT APPEARS, the bearing of the dot's own position
    //                  seen from the head. The dot sits at the controller's position
    //                  plus the ray, and the controller is not at the eye, so this
    //                  differs from the pointing direction by the hand's offset -
    //                  up to about atan(|hand - head| / distance). THIS is the one
    //                  the tester's eyes measure against the control dot.
    // The head-anchored control dot is at bearing 0 by construction and has been
    // confirmed in a headset to sit on the game's own crosshair, so dotAz/dotEl is
    // the predicted separation between the two dots, in degrees, and the tester's
    // report either matches it or refutes it.
    float dirAz = 0, dirEl = 0, dotAz = 0, dotEl = 0; bool haveRel = false;
    dvr::vr::HeadPose head;
    if (g_ray.ok && dvr::vr::peek_head_pose(head)) {
        const float fwdLocal[3] = {0, 0, -1}, upW[3] = {0, 1, 0};
        float hf[3]; dvr::xrmath::quat_rotate(head.qx, head.qy, head.qz, head.qw, fwdLocal, hf);
        float right[3] = {hf[1]*upW[2]-hf[2]*upW[1], hf[2]*upW[0]-hf[0]*upW[2],
                          hf[0]*upW[1]-hf[1]*upW[0]};
        float rn = std::sqrt(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);
        if (rn > 0.2f) {
            for (int i = 0; i < 3; ++i) right[i] /= rn;
            float up[3] = {right[1]*hf[2]-right[2]*hf[1], right[2]*hf[0]-right[0]*hf[2],
                           right[0]*hf[1]-right[1]*hf[0]};
            const float headPos[3] = {head.px, head.py, head.pz};
            // The dot's position is exactly what visual() publishes for the endpoint.
            const float dotPos[3] = {
                g_ray.originXr[0] + g_config.distanceM * g_ray.dirXr[0],
                g_ray.originXr[1] + g_config.distanceM * g_ray.dirXr[1],
                g_ray.originXr[2] + g_config.distanceM * g_ray.dirXr[2]};
            float toDot[3] = {dotPos[0]-headPos[0], dotPos[1]-headPos[1], dotPos[2]-headPos[2]};
            const float dn = std::sqrt(toDot[0]*toDot[0]+toDot[1]*toDot[1]+toDot[2]*toDot[2]);
            auto bearing = [&](const float v[3], float& az, float& el) {
                float f = 0, rr = 0, uu = 0;
                for (int i = 0; i < 3; ++i) {
                    f += v[i]*hf[i]; rr += v[i]*right[i]; uu += v[i]*up[i];
                }
                az = std::atan2(rr, f) * 57.2957795f;
                el = std::asin(uu < -1 ? -1.0f : uu > 1 ? 1.0f : uu) * 57.2957795f;
            };
            bearing(g_ray.dirXr, dirAz, dirEl);
            if (dn > 0.05f) {
                for (int i = 0; i < 3; ++i) toDot[i] /= dn;
                bearing(toDot, dotAz, dotEl);
                haveRel = true;
            }
        }
    }
    DVR_INFO("crosshair: ray source = %s. The ladder is the measured model axis "
             "first, then the controller ray carried by the hand trim, and it never "
             "falls back to the head: a weapon with no measurable geometry is still "
             "held in a tracked hand. A weapon whose mesh the geometry reader cannot "
             "take - the pistol's is far past its vertex limit - therefore still aims "
             "where it is pointed.",
             g_modelRayUsed ? "MEASURED MODEL AXIS"
                            : (g_config.modelRay ? "controller ray (no measured axis "
                                                   "for this weapon)"
                                                 : "controller ray (model ray off)"));
    DVR_INFO("crosshair: follow-hand-trim %s - %s (calibration revision %u). When "
             "this is following, the dot, the beam and the native shot all move with "
             "the hand trim because they consume ONE published ray; when it is not, "
             "the shared ray may be refused. It does not make the beam and the bolt the "
             "same line: the bolt still starts at the engine's own spawn point.",
             g_config.followHandTrim ? "ENABLED" : "off",
             g_followUsed ? "following the hand" : g_followWhy, g_followRev);
    DVR_INFO("modelray: %s; ray=%s; model axis is derived from held bolt geometry, not AIM pose",
             g_config.modelRay ? "ENABLED" : "off", g_ray.why);
    DVR_INFO("crosshair: POINTS az %+.1f el %+.1f deg | DOT APPEARS az %+.1f el %+.1f deg "
             "(%s; positive az is the head's RIGHT, and the head-anchored control dot is "
             "at 0,0 by construction). DOT APPEARS is the separation between the two dots "
             "the headset shows, so it is the prediction this run can refute; POINTS is "
             "where the controller aims and should read near 0,0 when it is sighted along "
             "the line of sight. They differ by the hand's offset from the eye, which is "
             "geometry and not an error. Neither is the RAW XR direction below - that is "
             "in LOCAL space and carries the head's own yaw.",
             dirAz, dirEl, dotAz, dotEl,
             haveRel ? "measured" : "UNAVAILABLE - no ray, no head pose, or the dot is at "
                                    "the head: every number on this line is meaningless");
    DVR_INFO("crosshair: hand=%s gen=%u age=%llu ms ray=%s aim=(%+.3f,%+.3f,%+.3f) "
             "grip=(%+.3f,%+.3f,%+.3f) aimGripDeg=%.2f (-1=unavailable; near zero is possible, not proof of aliasing) "
             "fixed=%.2fm axisSource: see modelray status "
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
        } else if (!std::strcmp(a,"follow") &&
               (!std::strcmp(b,"on") || !std::strcmp(b,"off"))) {
        cfg.followHandTrim = !std::strcmp(b,"on"); changed = true;
    } else if (!std::strcmp(a,"control") &&
                   (!std::strcmp(b,"on") || !std::strcmp(b,"off"))) {
            cfg.controlDot = !std::strcmp(b,"on"); changed = true;
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
    else DVR_WARN("crosshair: status | dot on|off | laser on|off | control on|off | "
                  "follow on|off | "
                  "hand left|right | distance 0.5..50 | size 0.05..2");
    log_status();
}
void draw_ui() {
    auto cfg = config(); bool changed = false;
    ImGui::TextWrapped("Controller pointing guide at a fixed distance. The crossbow toggle above aims its launch through this endpoint.");
    changed |= ImGui::Checkbox("Controller dot", &cfg.dot);
    changed |= ImGui::Checkbox("Controller beam", &cfg.laser);
    changed |= ImGui::RadioButton("Left hand", &cfg.hand, 0); ImGui::SameLine();
    changed |= ImGui::RadioButton("Right hand", &cfg.hand, 1);
    changed |= ImGui::SliderFloat("Guide distance (m)", &cfg.distanceM, 0.5f, 50.0f, "%.1f");
    changed |= ImGui::SliderFloat("Dot size (degrees)", &cfg.sizeDeg, 0.05f, 2.0f, "%.2f");
    changed |= ImGui::Checkbox("Ray follows the hand trim", &cfg.followHandTrim);
    changed |= ImGui::Checkbox("Ray from loaded bolt geometry", &cfg.modelRay);
    ImGui::TextWrapped("On, the dot, beam and shot move with the numpad hand trim "
                       "instead of the bare controller. Not a measured barrel axis: "
                       "it carries the trim onto the existing aim ray.");
    changed |= ImGui::Checkbox("CONTROL dot (head-anchored, no controller)", &cfg.controlDot);
    ImGui::TextWrapped("The larger control dot marks the head direction at the guide distance. "
                       "The controller dot is a fixed endpoint, not a predicted ballistic impact.");
    if (changed) configure(cfg,"F10 Aim");
    ImGui::TextWrapped("Ray: %s. Renderer: %s.", g_ray.why,
        dvr::vr::aim_visual_result_name(dvr::vr::aim_visual_stats().last));
    ImGui::TextDisabled("No surface trace yet. Game reticle stays visible.");
}
void status(dvr::status::Writer& w) {
    const auto s = dvr::vr::aim_visual_stats();
    w.obj("crosshair"); w.kv("dot",g_config.dot); w.kv("laser",g_config.laser);
    w.kv("hand",g_config.hand ? "right" : "left");
    w.kv("controlDot",g_config.controlDot);
    w.kv("followHandTrim",g_config.followHandTrim);
    w.kv("modelRay",g_config.modelRay);
    w.kv("modelRayUsed",g_modelRayUsed);
    w.kv("followingHand",g_followUsed); w.kv("followWhy",g_followWhy);
    {   const auto c = dvr::vr::control_dot_stats();
        w.kv("controlDotFrames",(unsigned long)c.frames);
        w.kv("controlDotsDrawn",(unsigned long)c.dots); }
    w.kv("distanceM",(double)g_config.distanceM); w.kv("sizeDeg",(double)g_config.sizeDeg);
    w.kv("ray",g_ray.why); w.kv("generation",(unsigned long)g_ray.gen);
    w.kv("renderer",dvr::vr::aim_visual_result_name(s.last));
    w.kv("submitted",(unsigned long)s.submitted); w.kv("dotFrames",(unsigned long)s.dotFrames);
    w.kv("beamFrames",(unsigned long)s.beamFrames); w.end_obj();
}
} // namespace dvr::aim
