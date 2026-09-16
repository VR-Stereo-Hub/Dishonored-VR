// VR-123: optional asynchronous D3D11 stage timing, present thread only.
#pragma once
struct ID3D11Device;
struct ID3D11DeviceContext;
namespace dvr::bridge_profile {
enum Stage { Conversion, EyeCopy, StageCount };
void set_enabled(bool on);
bool enabled();
void set_gameplay(bool on);
void present(); // one random stage opportunity per native Present, at most one disjoint bracket
void reset(); // before releasing the D3D11 device; no GPU wait
int begin(ID3D11Device*, ID3D11DeviceContext*, Stage, int eye);
void end(ID3D11DeviceContext*, Stage, int token);
class Scope {
    ID3D11DeviceContext* ctx_; Stage stage_; int token_;
public:
    Scope(ID3D11Device* d, ID3D11DeviceContext* c, Stage s, int eye)
        : ctx_(c), stage_(s), token_(begin(d,c,s,eye)) {}
    ~Scope() { if (token_>=0) end(ctx_,stage_,token_); }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
}
