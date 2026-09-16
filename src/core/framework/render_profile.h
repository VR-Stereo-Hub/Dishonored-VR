#pragma once
namespace dvr::render_profile {
enum Kind { LayoutRefresh, Reflection, Bytecode, WeaponDraw, BufferQueries, AnimationWeight, Count };
void set_enabled(bool on);
bool enabled();
void tick(bool gameplay);
class Scope {
    Kind kind;
    double start = 0;
    bool measured = false;
public:
    explicit Scope(Kind k);
    ~Scope() { finish(); }
    void finish();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};
}
