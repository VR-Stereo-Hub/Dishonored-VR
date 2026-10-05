// Local-only Heart material data. No extracted pixels are compiled into the mod.
#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <algorithm>
namespace dvr::heart {
constexpr unsigned kMaterialCount=5,kProbeCount=64;
struct ColorProbe {uint16_t x=0,y=0;};
struct TextureReference {
    uint32_t width=0,height=0;uint64_t hash=0;
    std::array<uint8_t,kProbeCount*3> rgb{};
};
struct ImageLevel {uint32_t width=0,height=0;std::vector<uint8_t> bgra;};
struct MaterialImage {
    std::array<ColorProbe,kProbeCount> probes{};
    std::vector<TextureReference> reference;
    std::vector<ImageLevel> levels;
};
inline bool power_two(uint32_t x){return x&&!(x&(x-1));}
inline bool read_bytes(FILE* f,void* p,size_t n){return n==0||fread(p,1,n,f)==n;}
struct MaterialAsset {
    std::array<MaterialImage,kMaterialCount> images;
    bool load(FILE* f){
        *this={};char magic[8];uint32_t count=0;
        if(!f||!read_bytes(f,magic,8)||memcmp(magic,"DVRHMT01",8)||!read_bytes(f,&count,4)||count!=kMaterialCount)return false;
        MaterialAsset result;
        for(auto& im:result.images){
            uint32_t nr=0,nl=0;
            if(!read_bytes(f,&nr,4)||!read_bytes(f,&nl,4)||nr<1||nr>7||nl<1||nl>11||!read_bytes(f,im.probes.data(),sizeof(im.probes)))return false;
            im.reference.resize(nr);im.levels.resize(nl);
            for(unsigned i=0;i<nr;++i){
                auto& r=im.reference[i];
                if(!read_bytes(f,&r.width,4)||!read_bytes(f,&r.height,4)||!read_bytes(f,&r.hash,8)||!read_bytes(f,r.rgb.data(),r.rgb.size()))return false;
                if(!power_two(r.width)||r.width!=r.height||r.width<64||r.width>4096)return false;
                if(i&&r.width!=im.reference[i-1].width/2)return false;
                if(!r.hash)return false;
            }
            for(unsigned i=0;i<nl;++i){
                auto& l=im.levels[i];
                if(!read_bytes(f,&l.width,4)||!read_bytes(f,&l.height,4)||!power_two(l.width)||l.width!=l.height||l.width>1024)return false;
                if(i&&(im.levels[i-1].width==1||l.width!=im.levels[i-1].width/2))return false;
                l.bgra.resize(size_t(l.width)*l.height*4);
                if(!read_bytes(f,l.bgra.data(),l.bgra.size()))return false;
            }
            if(im.levels.back().width!=1)return false;
        }
        if(fgetc(f)!=EOF)return false;
        *this=std::move(result);return true;
    }
};
inline uint64_t hash_byte(uint64_t h,uint8_t b){return (h^b)*1099511628211ull;}
// Every supported streaming level is matched against exact native compressed
// content, including high-information texels. No slot assumptions or fuzzy
// color thresholds are involved.
inline int identify_bc1(const MaterialAsset& asset,uint32_t w,uint32_t h,const void* data,size_t pitch,float* error=nullptr){
    if(!data||w!=h||!power_two(w)||w<64||w>4096||pitch<size_t((w+3)/4)*8)return -1;
    int matched=-1;
    for(unsigned m=0;m<kMaterialCount;++m){
        const auto& im=asset.images[m];const TextureReference* ref=nullptr;
        for(const auto& r:im.reference)if(r.width==w&&r.height==h){ref=&r;break;}
        if(!ref)continue;
        uint64_t hash=14695981039346656037ull;
        for(unsigned i=0;i<kProbeCount;++i){
            const unsigned x=unsigned(im.probes[i].x)*w/65536u,y=unsigned(im.probes[i].y)*h/65536u;
            const auto* b=(const uint8_t*)data+(y/4)*pitch+(x/4)*8;
            for(unsigned k=0;k<8;++k)hash=hash_byte(hash,b[k]);
        }
        if(hash==ref->hash){if(matched>=0)return -1;matched=(int)m;}
    }
    if(error)*error=matched>=0?0.f:-1.f;
    return matched;
}
}
