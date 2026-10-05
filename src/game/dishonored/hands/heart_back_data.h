// Bounded local model data. No game-derived geometry is compiled or shipped.
#pragma once
#include <vector>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cmath>
namespace dvr::heart {
inline uint16_t half(float value){
    const uint16_t sign=value<0?0x8000:0;float v=fabsf(value);
    if(v<.00006103515625f)return sign|(uint16_t)lroundf(v*16777216.f);
    int e=0;float f=frexpf(v,&e);int mant=(int)lroundf((2*f-1)*1024);
    if(mant==1024){mant=0;++e;}
    return sign|(uint16_t)((e+14)<<10)|(uint16_t)mant;
}
struct Vertex {float p[3],n[3],t[3],sign,uv[2];int32_t bone[4];float weight[4];};
static_assert(sizeof(Vertex)==80,"Heart vertex disk layout");
struct Model {
    std::vector<Vertex> vertices;std::vector<uint32_t> indices;
    bool load(FILE* f,unsigned bones){
        vertices.clear();indices.clear();char magic[8];uint32_t count[2];
        if(!f||!bones||bones>128||fread(magic,1,8,f)!=8||memcmp(magic,"DVRHRT01",8)||fread(count,4,2,f)!=2)return false;
        if(count[0]<3||count[0]>65536||count[1]<1||count[1]>131072)return false;
        std::vector<Vertex> v(count[0]);std::vector<uint32_t> ix(size_t(count[1])*3);
        if(fread(v.data(),sizeof(Vertex),v.size(),f)!=v.size()||fread(ix.data(),4,ix.size(),f)!=ix.size()||fgetc(f)!=EOF)return false;
        for(const auto& a:v){
            float nn=0,tt=0,nt=0,sum=0;
            for(int j=0;j<3;++j){
                if(!std::isfinite(a.p[j])||fabsf(a.p[j])>100||!std::isfinite(a.n[j])||!std::isfinite(a.t[j]))return false;
                nn+=a.n[j]*a.n[j];tt+=a.t[j]*a.t[j];nt+=a.n[j]*a.t[j];
            }
            if(fabsf(nn-1)>.03f||fabsf(tt-1)>.03f||fabsf(nt)>.03f||!std::isfinite(a.sign)||fabsf(fabsf(a.sign)-1)>.001f)return false;
            for(float u:a.uv)if(!std::isfinite(u)||fabsf(u)>8)return false;
            for(int j=0;j<4;++j){
                if(a.bone[j]<0||a.bone[j]>=(int)bones||!std::isfinite(a.weight[j])||a.weight[j]<0||a.weight[j]>1)return false;
                for(int k=0;k<j;++k)if(a.weight[j]>0&&a.weight[k]>0&&a.bone[k]==a.bone[j])return false;
                sum+=a.weight[j];
            }
            if(fabsf(sum-1)>.001f)return false;
        }
        for(uint32_t i:ix)if(i>=v.size())return false;
        for(size_t i=0;i<ix.size();i+=3)if(ix[i]==ix[i+1]||ix[i]==ix[i+2]||ix[i+1]==ix[i+2])return false;
        vertices.swap(v);indices.swap(ix);return true;
    }
};
}
