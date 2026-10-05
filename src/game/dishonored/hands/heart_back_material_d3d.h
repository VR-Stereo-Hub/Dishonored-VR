#pragma once
#include "heart_back_material.h"
#include <d3d9.h>
namespace dvr::heart {
inline bool upload_image(IDirect3DDevice9* dev,const MaterialImage& image,IDirect3DTexture9** out){
    if(!dev||!out||image.levels.empty())return false;
    *out=nullptr;const auto& first=image.levels.front();IDirect3DTexture9* tex=nullptr;
    if(FAILED(dev->CreateTexture(first.width,first.height,(UINT)image.levels.size(),0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&tex,nullptr))||!tex)return false;
    for(unsigned level=0;level<image.levels.size();++level){
        const auto& src=image.levels[level];D3DLOCKED_RECT lr{};
        if(FAILED(tex->LockRect(level,&lr,nullptr,0))){tex->Release();return false;}
        bool valid=lr.pBits&&lr.Pitch>=int(src.width*4);
        if(valid)for(unsigned y=0;y<src.height;++y)memcpy((uint8_t*)lr.pBits+size_t(y)*lr.Pitch,src.bgra.data()+size_t(y)*src.width*4,src.width*4);
        const HRESULT hr=tex->UnlockRect(level);
        if(!valid||FAILED(hr)){tex->Release();return false;}
    }
    *out=tex;return true;
}
// Snapshot and restore only around the added cap draw. Temporary GetTexture
// references end here; no game COM object is retained by the material cache.
class MaterialBinding {
    IDirect3DDevice9* dev_=nullptr;
    std::array<IDirect3DBaseTexture9*,16> original_{};
    std::array<IDirect3DTexture9*,16> replacement_{};
    uint32_t changed_=0;
public:
    uint32_t roles=0,stages=0;
    MaterialBinding()=default;
    MaterialBinding(const MaterialBinding&)=delete;
    MaterialBinding& operator=(const MaterialBinding&)=delete;
    template<class Classifier>bool prepare(IDirect3DDevice9* dev,const std::array<IDirect3DTexture9*,kMaterialCount>& images,Classifier classify){
        if(!dev||dev_)return false;
        dev_=dev;
        for(unsigned i=0;i<16;++i){
            if(FAILED(dev_->GetTexture(i,&original_[i])))return false;
            if(!original_[i]||original_[i]->GetType()!=D3DRTYPE_TEXTURE)continue;
            const int role=classify((IDirect3DTexture9*)original_[i]);
            if(role<0)continue;
            if(role>=int(kMaterialCount)||!images[role])return false;
            replacement_[i]=images[role];roles|=1u<<role;stages|=1u<<i;
        }
        return true;
    }
    bool apply(){
        for(unsigned i=0;i<16;++i)if(replacement_[i]){
            changed_|=1u<<i;
            if(FAILED(dev_->SetTexture(i,replacement_[i])))return false;
        }
        return true;
    }
    bool restore(){
        bool ok=true;
        for(unsigned i=0;i<16;++i)if(changed_&(1u<<i)){
            if(SUCCEEDED(dev_->SetTexture(i,original_[i])))changed_&=~(1u<<i);else ok=false;
        }
        return ok;
    }
    ~MaterialBinding(){restore();for(auto* p:original_)if(p)p->Release();}
};
}
