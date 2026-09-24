#include "core/util/log.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstdio>
namespace fs=std::filesystem;
int failed=0,passed=0;
#define CHECK(x) do {if(x)++passed;else {++failed;printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)
std::string read(const fs::path& p) {std::ifstream f(p);return {std::istreambuf_iterator<char>(f),{}};}
int main() {
    auto dir=fs::temp_directory_path()/("dvr-log-history-"+std::to_string(GetTickCount64()));fs::create_directories(dir);
    for(int run=1;run<=14;++run) {
        dvr::log::init(dir.string().c_str(),"dishonored_vr");
        dvr::log::write(dvr::log::Cat::core,dvr::log::Level::Info,"RUN-%02d",run);
        // Repeated initialization during one run must not rotate or truncate it.
        dvr::log::init(dir.string().c_str(),"dishonored_vr");dvr::log::shutdown();
    }
    int count=0;for(auto& file:fs::directory_iterator(dir))if(file.path().extension()==".log")++count;
    CHECK(count==10);
    for(int age=0;age<10;++age) {
        auto name=age==0?"dishonored_vr.log":age==1?"dishonored_vr.prev.log":"dishonored_vr.prev"+std::to_string(age)+".log";
        char expected[20];sprintf_s(expected,"RUN-%02d",14-age);
        auto text=read(dir/name);CHECK(text.find(expected)!=std::string::npos);CHECK(text.find("RUN-04")==std::string::npos);
    }
    const auto lockedFile=dir/"dishonored_vr.prev5.log";
    HANDLE locked=CreateFileA(lockedFile.string().c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    CHECK(locked!=INVALID_HANDLE_VALUE);
    dvr::log::init(dir.string().c_str(),"dishonored_vr");dvr::log::write(dvr::log::Cat::core,dvr::log::Level::Info,"RUN-15");dvr::log::shutdown();
    CloseHandle(locked);
    auto current=read(dir/"dishonored_vr.log");
    CHECK(current.find("RUN-14")!=std::string::npos);CHECK(current.find("RUN-15")!=std::string::npos);
    CHECK(current.find("rotation failed")!=std::string::npos);
    // Migrating a one-deep 1.0.0 history preserves both pre-existing sessions.
    auto migrated=dir/"migration";fs::create_directory(migrated);
    std::ofstream(migrated/"dishonored_vr.log")<<"OLD-CURRENT";
    std::ofstream(migrated/"dishonored_vr.prev.log")<<"OLD-PREVIOUS";
    dvr::log::init(migrated.string().c_str(),"dishonored_vr");dvr::log::shutdown();
    CHECK(read(migrated/"dishonored_vr.prev.log")=="OLD-CURRENT");CHECK(read(migrated/"dishonored_vr.prev2.log")=="OLD-PREVIOUS");
    printf("Log history: %d passed, %d failed; %s\n",passed,failed,dir.string().c_str());return failed?1:0;
}
