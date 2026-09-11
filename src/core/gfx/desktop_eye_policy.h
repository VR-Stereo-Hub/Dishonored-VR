// Pure desktop pin policy. Shared by the D3D9 implementation and host tests.
#pragma once

namespace dvr::desktop_eye {
enum class Source { Tag, Draw };
enum class Action { None, Snapshot, Blit };

struct Policy {
    bool valid = false;
    int heldEye = 0; // provenance, 0 = unknown; never infer an eye from the action
    unsigned zeros = 0;

    void reset() { valid = false; heldEye = 0; zeros = 0; }
    Action decide(Source source, int draw, int tag) {
        const int eye = source == Source::Draw ? draw : tag;
        if (source == Source::Draw) {
            // Permit menus/loads after three consecutive unknown presents.
            // Tagged repeats do not expire a valid snapshot.
            if (eye == 0) {
                if (zeros < 4) ++zeros;
                if (zeros > 3) { valid = false; heldEye = 0; }
            } else zeros = 0;
        }
        if (eye < 0) return Action::Snapshot;
        if (valid && (eye > 0 || (source == Source::Draw && zeros <= 3)))
            return Action::Blit;
        return Action::None;
    }
    // Only successful copies establish provenance. Failure must not bless an
    // uninitialized surface after Reset, resize, device change or a live toggle.
    void copied(Action action, int draw, bool success) {
        if (action == Action::Snapshot) {
            valid = success;
            heldEye = success ? draw : 0;
        }
    }
};
} // namespace dvr::desktop_eye
