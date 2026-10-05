// Attachment corrections use the current native frame, not the render snapshot's origin.
#pragma once
#include "weapon_frame.h"
#include <cmath>

namespace dvr { namespace fx {
// The published component pair establishes parent-local -> arm-local without retaining
// a world origin. Use the full transform, including native component scale.
static inline bool parent_draw_reference(const hf::Xform& drawArm,
                                         const hf::Xform& publishedArm,
                                         const hf::Xform& publishedParent,
                                         hf::Xform* drawParent)
{
    hf::Xform invArm;
    if (!wf::inverse(publishedArm, &invArm)) return false;
    *drawParent = hf::xform_mul(drawArm, hf::xform_mul(invArm, publishedParent));
    return true; // world_correction validates the resulting matrix before use
}

static inline bool world_correction(const hf::Xform& drawCorrection,
                                    const hf::Xform& drawReference,
                                    const hf::Xform& currentNativeReference,
                                    hf::Xform* world)
{
    hf::Xform invDraw, invNative;
    if (!wf::inverse(drawReference, &invDraw) ||
        !wf::inverse(currentNativeReference, &invNative)) return false;
    const auto local = hf::xform_mul(hf::xform_mul(invDraw, drawCorrection), drawReference);
    *world = hf::xform_mul(hf::xform_mul(currentNativeReference, local), invNative);
    for (float x : world->r.m) if (!std::isfinite(x)) return false;
    for (float x : world->t) if (!std::isfinite(x)) return false;
    return true;
}
}}
