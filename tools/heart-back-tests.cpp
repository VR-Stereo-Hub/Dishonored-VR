#include "game/dishonored/hands/heart_back_data.h"
#include <algorithm>
#include <limits>
using namespace dvr::heart;
static int checks=0,failed=0;
static void check(bool yes,const char* name){++checks;if(!yes){++failed;printf("FAIL %s\n",name);}}
static bool load(const std::vector<uint8_t>& bytes){FILE* f=nullptr;tmpfile_s(&f);if(!f)return false;fwrite(bytes.data(),1,bytes.size(),f);rewind(f);Model m;bool ok=m.load(f,3);fclose(f);return ok;}
template<class T>static void put(std::vector<uint8_t>& b,size_t off,T value){memcpy(b.data()+off,&value,sizeof(value));}
int main(int argc,char** argv){
    std::vector<uint8_t> valid(16+3*sizeof(Vertex)+12);memcpy(valid.data(),"DVRHRT01",8);put<uint32_t>(valid,8,3);put<uint32_t>(valid,12,1);
    for(int i=0;i<3;++i){Vertex v{};v.p[i]=1;v.n[2]=1;v.t[0]=1;v.sign=1;v.weight[0]=1;memcpy(valid.data()+16+i*sizeof(v),&v,sizeof(v));put<uint32_t>(valid,16+3*sizeof(v)+i*4,i);}
    check(load(valid),"valid model");auto bad=valid;bad[0]='X';check(!load(bad),"magic");
    bad=valid;put<uint32_t>(bad,8,65537);check(!load(bad),"oversized vertices");
    bad=valid;put<uint32_t>(bad,12,131073);check(!load(bad),"oversized triangles");
    bad=valid;bad.pop_back();check(!load(bad),"truncated");bad=valid;bad.push_back(0);check(!load(bad),"trailing bytes");
    bad=valid;put<uint32_t>(bad,16+3*sizeof(Vertex),3);check(!load(bad),"index bounds");
    bad=valid;put<uint32_t>(bad,16+3*sizeof(Vertex),1);check(!load(bad),"duplicate triangle index");
    bad=valid;put<float>(bad,16,std::numeric_limits<float>::quiet_NaN());check(!load(bad),"nonfinite position");
    bad=valid;put<float>(bad,16+64,.5f);check(!load(bad),"weight normalization");
    bad=valid;put<int32_t>(bad,16+48,3);check(!load(bad),"bone bounds");
    bad=valid;put<float>(bad,16+20,0);check(!load(bad),"invalid normal");
    check(half(0)==0&&half(1)==0x3c00&&half(-1)==0xbc00&&half(.5f)==0x3800&&half(8)==0x4800,"half encoding known values");
    if(argc>1){FILE* f=nullptr;fopen_s(&f,argv[1],"rb");Model m;check(m.load(f,22),"local authored model");if(f)fclose(f);printf("local model: %zu vertices, %zu triangles\n",m.vertices.size(),m.indices.size()/3);}
    printf("Heart data: %d checks, %d failures\n",checks,failed);return failed?1:0;
}
