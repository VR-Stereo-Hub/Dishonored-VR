#pragma once
#include <cstdint>
#include <cstring>

namespace dvr::vitals {
enum Mode { Window, Hand, Attached, Model };
inline const char* name(Mode m) {
    const char* names[] = {"window", "hand", "attached", "model"};
    return names[m];
}
inline Mode migrate(const char* value, int split, bool attach, bool scene) {
    for (int i = Window; i <= Model; ++i)
        if (value && !std::strcmp(value, name((Mode)i))) return (Mode)i;
    if (scene) return Model;
    if (attach) return Attached;
    return split == 0 ? Window : Hand; // absent legacy key (-1) defaults to hand
}
// A diagnostic square is never evidence that the textured bars drew. Both eyes
// must have accepted a textured draw before suppressing a binocular XR quad.
struct DrawProof {
    uint64_t eyeMs[2] = {};
    void reset() { eyeMs[0] = eyeMs[1] = 0; }
    void drawn(int eye, uint64_t now) { if (eye >= 0 && eye < 2) eyeMs[eye] = now; }
    bool fresh(uint64_t now) const {
        return eyeMs[0] && eyeMs[1] && now >= eyeMs[0] && now >= eyeMs[1] &&
            now - eyeMs[0] < 250 && now - eyeMs[1] < 250;
    }
};
}
