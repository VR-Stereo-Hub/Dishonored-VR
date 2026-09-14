// Standalone cinematic rotation checks. Never loads or launches the game.
#include "../src/game/dishonored/cinematic_math.h"
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>

namespace {
constexpr double pi = 3.14159265358979323846;
constexpr double unit = 2.0 * pi / 65536.0;
int failures = 0;
void check(bool ok, const char* name) {
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}
bool close(const dvr::cine::Matrix& a, const dvr::cine::Matrix& b,
           double tolerance = 1e-8) {
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            if (!std::isfinite(a.m[r][c]) ||
                std::fabs(a.m[r][c] - b.m[r][c]) > tolerance) return false;
    return true;
}
dvr::cine::Matrix decoded(const int32_t angles[3]) {
    return dvr::cine::rotation(angles[0] * unit, angles[1] * unit, angles[2] * unit);
}
}

int main() {
    using namespace dvr::cine;
    const Matrix identity = {{{1,0,0},{0,1,0},{0,0,1}}};
    // Explicit UE axes, independent of the implementation's trig formulas.
    const Matrix yaw90 = {{{0,-1,0},{1,0,0},{0,0,1}}};
    const Matrix pitch90 = {{{0,0,-1},{0,1,0},{1,0,0}}};
    const Matrix roll90 = {{{1,0,0},{0,0,1},{0,-1,0}}};
    check(close(rotation(0,0,0), identity), "zero rotation is identity");
    check(close(rotation(0,pi/2,0), yaw90), "positive yaw turns forward to right");
    check(close(rotation(pi/2,0,0), pitch90), "positive pitch lifts forward");
    check(close(rotation(0,0,pi/2), roll90), "positive roll tips right down");
    const Matrix arbitrary = rotation(0.31,-1.27,0.48);
    check(close(multiply(transpose(arbitrary), arbitrary), identity),
          "transpose is inverse for arbitrary reference");

    const int32_t authored[3] = {1900,17000,-5300};
    int32_t out[3] = {};
    Matrix basis = {};
    check(compose(authored, arbitrary, arbitrary, out, &basis) &&
          close(basis, decoded(authored)) && close(decoded(out), basis, 0.0002),
          "neutral head at arbitrary reference preserves authored orientation");

    const int32_t authoredRoll[3] = {0,0,16384};
    // Authored roll applied after local head pitch maps forward to +Y.
    const Matrix rollThenPitch = {{{0,0,-1},{1,0,0},{0,-1,0}}};
    const Matrix wrongOrder = {{{0,1,0},{0,0,1},{1,0,0}}};
    check(compose(authoredRoll, identity, pitch90, out, &basis) &&
          close(basis, rollThenPitch) && !close(basis, wrongOrder) &&
          close(decoded(out), basis, 0.0002),
          "authored roll plus local head pitch preserves noncommuting order");

    int32_t first[3] = {};
    Matrix firstBasis = {};
    const Matrix head = rotation(-0.23,0.72,0.11);
    bool stable = compose(authored, arbitrary, head, first, &firstBasis);
    for (int i = 0; i < 1000; ++i) {
        stable = compose(authored, arbitrary, head, out, &basis) && stable;
        stable = stable && out[0] == first[0] && out[1] == first[1] &&
                 out[2] == first[2] && close(basis, firstBasis);
    }
    check(stable, "repeated composition from authored input does not accumulate");

    const int32_t wrap[3] = {0,32760,0};
    const Matrix yawStep = rotation(0,32*unit,0);
    const Matrix expectedWrap = rotation(0,-32744*unit,0);
    check(compose(wrap, identity, yawStep, out, &basis) &&
          close(basis, expectedWrap) && close(decoded(out), expectedWrap, 0.0002),
          "yaw crossing signed rotator boundary keeps orientation continuous");

    bool poles = true;
    const int32_t zero[3] = {};
    for (double pitch : {pi/2-1e-7, pi/2, -pi/2+1e-7, -pi/2}) {
        const Matrix pole = rotation(pitch,0.61,-0.39);
        poles = compose(zero, identity, pole, out, &basis) && poles;
        poles = poles && close(basis,pole,0.0003) && close(decoded(out),pole,0.0003);
    }
    check(poles, "near vertical and exact pole outputs reconstruct full orientation");

    Matrix invalid = identity;
    invalid.m[1][2] = std::numeric_limits<double>::quiet_NaN();
    check(!compose(authored,identity,invalid,out,&basis), "NaN head is refused");
    check(!compose(authored,invalid,identity,out,&basis), "NaN reference is refused");
    std::printf("Cinematic head math: %d failure(s)\n", failures);
    return failures ? 1 : 0;
}
