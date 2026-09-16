#pragma once
namespace dvr::native_profile {
enum Kind { IndexedHook, PrimitiveHook, NativeIndexed, NativePrimitive, ConstHook, NativeConst, TargetHook, NativeTarget, VbLockInclusive, IbLockInclusive, TexLockRectInclusive, TexUnlockRectInclusive, CubeLockRectInclusive, CubeUnlockRectInclusive, VolLockBoxInclusive, VolUnlockBoxInclusive, SurfLockRectInclusive, SurfUnlockRectInclusive, SetViewportInclusive, SetRenderStateInclusive, SetTextureInclusive, SetVertexDeclarationInclusive, SetVertexShaderInclusive, SetTransformInclusive, SetPixelShaderInclusive, SetStreamSourceInclusive, DrawPrimitiveUPInclusive, DrawIndexedPrimitiveUPInclusive, Count };
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
