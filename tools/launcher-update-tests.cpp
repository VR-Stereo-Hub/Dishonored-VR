#include "sys/updates.h"
#include "sys/discovery.h"
#include "sys/fs.h"
#include "sys/process.h"
#include <json/json.h>
#include <cstdio>
#include <filesystem>
#include <limits>
using namespace dvr::setup;
int passed=0,failed=0;
#define CHECK(x) do {if(x)++passed;else{++failed;std::printf("FAIL line %d: %s\n",__LINE__,#x);}}while(0)
std::string fixture(const std::string& tag="v1.0.1") {
    Json::Value j(Json::arrayValue),r,a;
    r["draft"]=false;r["prerelease"]=false;r["tag_name"]=tag;r["body"]="Fixed camera FOV.\nUnicode: \xe2\x9c\x93";
    r["published_at"]="2026-09-24T00:00:00Z";
    a["name"]="DishonoredVR-Launcher-"+tag+".exe";
    a["browser_download_url"]="https://github.com/VR-Stereo-Hub/Dishonored-VR/releases/download/"+tag+"/"+a["name"].asString();
    a["digest"]="sha256:"+std::string(64,'a');a["size"]=100;r["assets"].append(a);j.append(r);
    Json::StreamWriterBuilder b;return Json::writeString(b,j);
}
int wmain(int argc,wchar_t** argv) {
    CHECK(argc>=3);if(argc<3)return 1;
    const auto currentVersion=fs::narrow(argv[2]);
    uint32_t v[3];
    CHECK(updates::version("1.0.1",v) && v[2]==1);
    CHECK(updates::newer("v1.10.0","1.9.99"));
    CHECK(!updates::newer("1.0.1","1.0.1"));
    CHECK(!updates::newer("1.0.0","1.0.1"));
    for(const char* bad:{"","1.0","1.0.1.2","1.0.1-beta","1.0.-1","1.0.4294967296","../../1.0.0","1.0.1 "})CHECK(!updates::version(bad,v));
    std::vector<updates::Release> releases;std::string error;
    CHECK(updates::parse_releases(fixture(),&releases,&error));CHECK(releases.size()==1 && releases[0].downloadable());
    CHECK(releases[0].notes.find("Unicode")!=std::string::npos);
    for(const std::string bad:{"not json","{}","[]garbage","[null,5,\"text\"]"}) {
        bool ok=updates::parse_releases(bad,&releases,&error);CHECK(!ok || releases.empty());
    }
    auto base=fixture();
    for(const auto& pair:std::vector<std::pair<std::string,std::string>>{{"\"draft\" : false","\"draft\" : true"},{"\"prerelease\" : false","\"prerelease\" : true"}}) {
        auto data=base;auto at=data.find(pair.first);CHECK(at!=std::string::npos);if(at!=std::string::npos)data.replace(at,pair.first.size(),pair.second);
        CHECK(updates::parse_releases(data,&releases,&error) && releases.empty());
    }
    auto badUrl=base;badUrl.replace(badUrl.find("https://github.com/"),19,"https://evil.test/");
    CHECK(updates::parse_releases(badUrl,&releases,&error) && !releases[0].downloadable());
    auto noDigest=base;noDigest.replace(noDigest.find("sha256:"),7,"sha512:");
    CHECK(updates::parse_releases(noDigest,&releases,&error) && !releases[0].downloadable());
    auto multiple="["+fixture("v1.0.1").substr(1);auto second=fixture("v1.10.0");
    multiple=multiple.substr(0,multiple.find_last_of(']'))+","+second.substr(second.find('[')+1);
    CHECK(updates::parse_releases(multiple,&releases,&error) && releases.front().version=="1.10.0");
    const auto scratch=fs::join(fs::temp_dir(),L"dvr-launcher-update-tests-"+std::to_wstring(GetTickCount64()));
    std::filesystem::create_directories(scratch);
    const std::wstring exe=argv[1];updates::Release actual;actual.version=currentVersion;actual.size=fs::file_size(exe);actual.assetUrl="verified fixture";CHECK(fs::sha256_file(exe,&actual.sha256,nullptr));
    CHECK(updates::verify(exe,actual,&error));
    ++actual.size;CHECK(!updates::verify(exe,actual,&error));--actual.size;
    actual.version="999.999.999";CHECK(!updates::verify(exe,actual,&error));actual.version=currentVersion;
    auto originalHash=actual.sha256;actual.sha256[0]=actual.sha256[0]=='a'?'b':'a';CHECK(!updates::verify(exe,actual,&error));actual.sha256=originalHash;
    const auto target=fs::join(scratch,L"DishonoredVR-Launcher.exe");
    std::vector<uint8_t> old={1,2,3,4};CHECK(fs::write_file_atomic(target,old.data(),old.size(),nullptr));
    CHECK(!updates::replace_launcher(exe,target,std::string(64,'0'),0,&error));CHECK(fs::file_size(target)==4);
    HANDLE locked=CreateFileW(target.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
    CHECK(locked!=INVALID_HANDLE_VALUE);
    CHECK(!updates::replace_launcher(exe,target,originalHash,0,&error));CHECK(fs::file_size(target)==4);CloseHandle(locked);
    CHECK(updates::replace_launcher(exe,target,originalHash,0,&error));CHECK(updates::verify(target,actual,&error));
    CHECK(!updates::replace_launcher(exe,fs::join(scratch,L"unrelated.exe"),originalHash,0,&error));
    const auto root=fs::join(scratch,L"GOG game with spaces");const auto win32=fs::join(root,L"Binaries\\Win32");
    std::filesystem::create_directories(win32);CHECK(fs::copy_file(fs::join(fs::system_dir(),L"ping.exe"),fs::join(win32,L"Dishonored.exe"),nullptr));
    CHECK(discovery::inspect(root).valid);CHECK(discovery::inspect(fs::join(root,L"Binaries")).valid);CHECK(discovery::inspect(fs::join(win32,L"Dishonored.exe")).valid);
    CHECK(!discovery::inspect(L"").valid);CHECK(!discovery::inspect(fs::join(scratch,L"missing")).valid);
    const std::string info="{\"gameId\":\"12345\",\"rootGameId\":\"12345\"}";
    CHECK(fs::write_file_atomic(fs::join(root,L"goggame-12345.info"),info.data(),info.size(),nullptr));
    auto game=discovery::inspect(root);CHECK(game.store==discovery::Store::Gog && game.gogId==L"12345" && game.root==root);
    auto launch=discovery::launch_command(game,L"C:\\GOG Galaxy\\GalaxyClient.exe");
    CHECK(launch.error.empty() && launch.args.find(L"/command=runGame /gameId=12345 /path=\"")!=std::wstring::npos);
    CHECK(!discovery::launch_command(game,L"").error.empty());
    game.store=discovery::Store::Steam;CHECK(discovery::launch_command(game,L"").target==L"steam://rungameid/205100");
    const auto win64=fs::join(root,L"Binaries\\Win64");std::filesystem::create_directories(win64);
    CHECK(discovery::inspect(win64).unsupported64);
    CHECK(fs::copy_file(fs::join(fs::system_dir(),L"ping.exe"),fs::join(win64,L"Dishonored.exe"),nullptr));
    CHECK(discovery::inspect(win64).unsupported64 && !discovery::inspect(win64).valid);
    // Also detect a 64-bit executable placed in a misleading Win32 directory.
    auto native=fs::join(fs::env(L"SystemRoot"),L"Sysnative\\ping.exe");
    CHECK(fs::copy_file(native,fs::join(win32,L"Dishonored.exe"),nullptr));CHECK(discovery::inspect(win32).unsupported64);
    if(argc>3 && std::wstring(argv[3])==L"--live") {
        auto check=updates::check();CHECK(check.online && !check.releases.empty());
        if(!check.releases.empty()) {
            std::wstring downloaded;CHECK(updates::download(check.releases.front(),&downloaded,&error));
            if(!downloaded.empty())CHECK(updates::verify(downloaded,check.releases.front(),&error));
            std::printf("Live GitHub: %s; verified download: %s\n",check.releases.front().version.c_str(),fs::narrow(downloaded).c_str());
        }
        if(!error.empty())std::printf("Last diagnostic: %s\n",error.c_str());
    }
    std::printf("Launcher update/discovery: %d passed, %d failed. Fixtures: %s\n",passed,failed,fs::narrow(scratch).c_str());
    return failed?1:0;
}
