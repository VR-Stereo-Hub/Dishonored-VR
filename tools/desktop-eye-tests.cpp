#include "../src/core/gfx/desktop_eye_policy.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace dvr::desktop_eye;
static unsigned checks = 0;
static void check(bool value, const char* why) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
struct Result { unsigned right = 0, switches = 0; };
// draw tags and actual pixels are independent: untagged singles are rendered
// from the left camera in the reported run. A delayed tag is not pixel identity.
static Result run(const std::vector<int>& tags, int lag, Source source, bool requirePin) {
    Policy policy;
    int heldPixels = 0, lastPixels = 0;
    bool warmed = false;
    Result result;
    for (size_t i = 0; i < tags.size(); ++i) {
        const int draw = tags[i];
        const int pixels = draw == 0 ? -1 : draw;
        const int delivered = i >= (size_t)lag ? tags[i-lag] : 0;
        const Action action = policy.decide(source, draw, delivered);
        int shown = pixels;
        if (action == Action::Snapshot) { heldPixels = pixels; warmed = true; }
        if (action == Action::Blit) shown = heldPixels;
        policy.copied(action, draw, true);
        if (warmed && shown > 0) ++result.right;
        if (lastPixels && shown != lastPixels) ++result.switches;
        lastPixels = shown;
        if (requirePin && warmed) check(shown == -1, "window changed to right pixels after snapshot");
    }
    return result;
}
int main(int argc, char** argv) {
    const std::vector<int> burst{-1,1,-1,1,0,-1,1,0,-1,1,0,-1,1};
    if (argc > 1 && !std::strcmp(argv[1], "--legacy-must-pin")) {
        run(burst, 1, Source::Tag, true); // deliberately fails: negative control
        return 0;
    }
    auto old = run(burst, 1, Source::Tag, false);
    check(old.right > 0 && old.switches > 2, "legacy delayed burst must expose wrong pixels and switches");
    check(run(burst, 0, Source::Tag, true).right == 0, "legacy synchronous burst must stay left");
    for (int lag : {0,1}) {
        for (auto sequence : {std::vector<int>{-1,1,-1,1,-1,1}, burst,
             std::vector<int>{-1,-1,1,-1,-1,1}, std::vector<int>{1,1,-1,1,1,-1},
             std::vector<int>{0,1,-1,0,0,0,0,-1,1}})
            run(sequence, lag, Source::Draw, true);
        // All 6561 sequences of length eight, including starts in either eye,
        // repeated eyes and zero streaks. No assumed alternation in the policy.
        for (unsigned bits = 0; bits < 6561; ++bits) {
            std::vector<int> sequence;
            unsigned v = bits;
            for (int j = 0; j < 8; ++j) { sequence.push_back(int(v % 3)-1); v /= 3; }
            // A menu timeout releases the pin. Restart the invariant after the
            // next successful left snapshot; direct policy checks below cover it.
            Policy p; int held = 0;
            for (size_t j = 0; j < sequence.size(); ++j) {
                int draw = sequence[j], tag = j >= (size_t)lag ? sequence[j-lag] : 0;
                Action action = p.decide(Source::Draw, draw, tag);
                bool active = p.valid || action == Action::Snapshot;
                int shown = action == Action::Blit ? held : (draw ? draw : -1);
                if (active) check(shown == -1, "exhaustive sequence leaks right pixels");
                if (action == Action::Snapshot) held = draw;
                p.copied(action, draw, true);
            }
        }
    }
    Policy p;
    check(p.decide(Source::Draw, 1, -1) == Action::None, "startup right has no valid image");
    p.copied(Action::Snapshot, -1, true);
    for (int i = 0; i < 3; ++i)
        check(p.decide(Source::Draw, 0, 1) == Action::Blit, "hold three unknown frames");
    check(p.decide(Source::Draw, 0, -1) == Action::None && !p.valid, "fourth unknown releases menu");
    check(p.decide(Source::Draw, 1, -1) == Action::None, "expired snapshot cannot reappear");
    p.copied(Action::Snapshot, -1, true);
    p.reset(); // used for device changes, Reset, resize, disable and source change
    check(p.decide(Source::Draw, 1, -1) == Action::None, "reset invalidates lifetime snapshots");
    p.copied(Action::Snapshot, -1, false);
    check(!p.valid, "failed snapshot is not initialized");
    p.copied(Action::Snapshot, -1, true);
    p.copied(Action::Blit, 1, false);
    check(p.valid && p.heldEye == -1, "failed blit preserves held source for retry");
    p.copied(Action::Snapshot, -1, false);
    check(!p.valid, "failed replacement snapshot invalidates potentially damaged surface");
    std::printf("PASS: %u assertions; legacy lag-1 burst right=%u switches=%u; draw sequences and lifecycle clean\n",
                checks, old.right, old.switches);
}
