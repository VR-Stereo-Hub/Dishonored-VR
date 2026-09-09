#pragma once
#include "weapon_frame.h"
#include <stdio.h>
#include <string.h>

static inline int WeaponFrameTests()
{
    using namespace dvr::hf;
    using namespace dvr::wf;
    int failed = 0;
    const auto check = [&failed](const char* name, bool ok) {
        printf("weapon/%-32s %s\n", name, ok ? "PASS" : "FAIL");
        if (!ok) ++failed;
    };
    const Xform id = {identity3(), {0,0,0}};
    int bufferA = 0, bufferB = 0, target = 0;
    const Geometry geometry = {&bufferA,&bufferB,32,0,4,0,0,257,310,0};
    Geometry other = geometry;
    check("same_geometry_owned", same_geometry(geometry,other));
    other.start = 310;
    check("shared_buffer_other_range_native", !same_geometry(geometry,other));
    other = geometry; other.offset = 32;
    check("other_stream_offset_native", !same_geometry(geometry,other));
    other = geometry; other.base = 257;
    check("other_base_vertex_native", !same_geometry(geometry,other));
    check("current_eye_allowed", same_view(17,-1,17,-1));
    check("opposite_eye_refused", !same_view(17,-1,17,1));
    check("unknown_eye_refused", !same_view(17,0,17,0));
    check("stale_present_refused", !same_view(16,-1,17,-1));
    check("camera_color_pass_allowed", color_view(&target,&target,true,15));
    check("shadow_target_refused", !color_view(&target,&bufferA,true,15));
    check("depth_only_pass_refused", !color_view(&target,&target,true,0));
    check("other_viewport_refused", !color_view(&target,&target,false,15));
    Candidate candidates[2] = {{id,0,1}, {id,1,2}};
    candidates[0].predicted.t[0] = 70;
    Result r = match(id, candidates, 2, .25f, 1, 4);
    check("second_hand_is_considered", r.best == 1 && !r.ambiguous);
    candidates[0].predicted = id; candidates[0].predicted.t[0] = 1.01f;
    candidates[1].predicted.r.m[0] = 1.004f; candidates[1].predicted.t[0] = .9f;
    check("invalid_lower_score_cannot_win", match(id,candidates,2,.25f,1,4).best == 1);
    candidates[1].predicted = id;
    candidates[0].predicted = id;
    r = match(id, candidates, 2, .25f, 1, 4);
    check("zero_error_different_hands_refuse", r.ambiguous);
    candidates[0].hand = 1; candidates[0].assembly = 2;
    r = match(id, candidates, 2, .25f, 1, 4);
    check("same_assembly_tie_allowed", r.best >= 0 && !r.ambiguous);
    candidates[0].assembly = 1;
    check("same_hand_other_assembly_refuse", match(id,candidates,2,.25f,1,4).ambiguous);
    Candidate scaled = {id,1,2}; scaled.predicted.r.m[0] = 1.1f;
    check("scaled_duplicate_refused", match(id,&scaled,1,.25f,1,4).best < 0);
    Candidate stale = {id,1,2}; stale.predicted.t[2] = 2;
    check("moved_instance_revalidated", match(id,&stale,1,.25f,1,4).best < 0);
    Xform native = {{{0,-2,0, 2,0,0, 0,0,2}}, {100,200,300}};
    Xform drawRef = {{{0,0,1, 0,1,0, -1,0,0}}, {4,5,6}}, inv, k;
    check("affine_inverse_exists", inverse(native,&inv));
    const Xform roundtrip = xform_mul(native, inv);
    float error = 0;
    for (int i=0;i<9;++i) error += fabsf(roundtrip.r.m[i]-id.r.m[i]);
    for (int i=0;i<3;++i) error += fabsf(roundtrip.t[i]);
    check("scaled_inverse_roundtrip", error < .001f);
    check("full_bridge_exists", bridge(native,drawRef,&k));
    Xform memberRelative = id; memberRelative.t[0] = 10;
    const Xform predicted = xform_mul(k, xform_mul(native,memberRelative));
    const Xform expected = xform_mul(drawRef,memberRelative);
    error = 0;
    for (int i=0;i<9;++i) error += fabsf(predicted.r.m[i]-expected.r.m[i]);
    for (int i=0;i<3;++i) error += fabsf(predicted.t[i]-expected.t[i]);
    check("rotated_translated_scaled_bridge", error < .001f);
    Xform singular = id; singular.r.m[0] = 0;
    check("singular_bridge_refused", !bridge(singular,drawRef,&k));
    check("declared_palette_bounds", palette_range(6,225,0,231));
    check("old_250_register_overrun_refused", !palette_range(6,250,0,231));
    check("overlap_refused", !palette_range(6,228,0,231));
    check("missing_palette_refused", !palette_range(-1,0,0,231));
    check("non_camera_palette_allowed", palette_range(0,36,-1,36));
    float bank[256*4], copy[256*4];
    for (int i=0;i<256*4;++i) bank[i] = copy[i] = (float)i;
    Xform delta = drawRef;
    for (int reg=6;reg<6+225;reg+=3) compose_3x4(delta,copy+4*reg,bank+4*reg);
    check("projection_sentinel_untouched", memcmp(bank,copy,6*4*sizeof(float)) == 0);
    check("l2w_and_lighting_untouched", memcmp(bank+231*4,copy+231*4,25*4*sizeof(float)) == 0);
    // Two members transformed in their own spaces must receive the same
    // world correction and retain their relative assembly geometry.
    Xform member = id; member.t[0] = 17;
    inverse(member,&inv);
    const Xform dm = xform_mul(xform_mul(inv,delta),member);
    const Xform actual = xform_mul(member,dm), wanted = xform_mul(delta,member);
    error = 0;
    for(int i=0;i<9;++i) error += fabsf(actual.r.m[i]-wanted.r.m[i]);
    for(int i=0;i<3;++i) error += fabsf(actual.t[i]-wanted.t[i]);
    check("member_conjugation_preserves_assembly", error < .001f);
    Xform invK;
    inverse(k,&invK);
    const Xform nativeDelta = xform_mul(xform_mul(invK,delta),k);
    const Xform nativeMoved = xform_mul(k,xform_mul(nativeDelta,native));
    const Xform drawMoved = xform_mul(delta,xform_mul(k,native));
    error = 0;
    for(int i=0;i<9;++i) error += fabsf(nativeMoved.r.m[i]-drawMoved.r.m[i]);
    for(int i=0;i<3;++i) error += fabsf(nativeMoved.t[i]-drawMoved.t[i]);
    check("world_pass_receives_same_motion", error < .001f);

    // ---- VR-59: the fired bolt is a different instance ---------------------
    //
    // The failing case is a bolt fired into a nearby surface: same mesh, same
    // buffers, genuinely near the camera and genuinely near where the loaded
    // bolt draws. So every case below holds the DISTANCE inside the radius and
    // varies only the instance evidence - a suite that separated them by
    // distance would be testing the gate that already failed.
    const float inside = 5.0f, radius = 60.0f, outside = 900.0f;

    // The held bolt: its weapon is in that hand and it drew there this frame.
    check("held_bolt_corrects",
          held_instance(true, true, true, true, inside, radius) == INSTANCE_HELD);

    // THE BUG. The crossbow is stowed, so the loaded bolt has not drawn for
    // many presents - and the hand it belongs to is still publishing a fresh
    // correction every frame, because the hand is always drawn. Before VR-59
    // this returned HELD and a bolt in the ground followed the pistol.
    check("stowed_weapon_no_reference_refuses",
          held_instance(false, true, false, true, 0.0f, radius) == INSTANCE_STOWED);
    check("no_reference_alone_refuses",
          held_instance(true, true, false, true, 0.0f, radius) == INSTANCE_NO_REF);

    // Inside the radius and still refused, on the engine's own answer alone.
    // This is the case no threshold could ever have closed.
    check("stowed_inside_the_radius_still_refuses",
          held_instance(false, true, true, true, inside, radius) == INSTANCE_STOWED);

    // A second instance of a held weapon's mesh, drawn elsewhere.
    check("live_member_drawn_elsewhere_refuses",
          held_instance(true, true, true, true, outside, radius) == INSTANCE_ELSEWHERE);

    // AUTHORITY, NOT JUST REFUSAL. Only positive evidence of another instance
    // may overturn the relaxed view-model band or hand the buffers back. If
    // NO_REF counted as strong, a re-equipped sword could not relock at all,
    // and its uncorrected pass would draw a ghost copy meanwhile.
    check("stowed_is_a_strong_veto",
          instance_strong_veto(INSTANCE_STOWED));
    check("elsewhere_is_a_strong_veto",
          instance_strong_veto(INSTANCE_ELSEWHERE));
    check("no_reference_is_NOT_a_strong_veto",
          !instance_strong_veto(INSTANCE_NO_REF));
    check("held_is_not_a_veto",
          !instance_strong_veto(INSTANCE_HELD) && instance_corrects(INSTANCE_HELD));

    // EVERY REFUSAL MUST STOP THE CORRECTION. Three verdicts, one consequence.
    check("only_held_corrects",
          !instance_corrects(INSTANCE_STOWED) &&
          !instance_corrects(INSTANCE_ELSEWHERE) &&
          !instance_corrects(INSTANCE_NO_REF));

    // THE LEVERS RESTORE THE OLD BEHAVIOUR, which is what makes them an A/B.
    // With both off, the stowed bolt is corrected again - the pre-VR-59 bug,
    // reachable on purpose so a headset run can compare the two.
    check("levers_off_restore_the_old_behaviour",
          held_instance(false, false, false, false, 0.0f, radius) == INSTANCE_HELD);
    check("live_member_lever_off_ignores_the_engine_answer",
          held_instance(false, false, true, true, inside, radius) == INSTANCE_HELD);

    // A zero radius must not silently accept everything.
    check("zero_radius_refuses_any_offset",
          held_instance(true, true, true, true, 0.1f, 0.0f) == INSTANCE_ELSEWHERE);

    check("verdict_names_are_distinct",
          strcmp(instance_name(INSTANCE_HELD), instance_name(INSTANCE_STOWED)) &&
          strcmp(instance_name(INSTANCE_STOWED), instance_name(INSTANCE_ELSEWHERE)) &&
          strcmp(instance_name(INSTANCE_ELSEWHERE), instance_name(INSTANCE_NO_REF)));
    return failed;
}
