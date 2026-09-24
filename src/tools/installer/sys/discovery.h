#pragma once
#include <string>
#include <vector>
namespace dvr::setup::discovery {
enum class Store { Unknown, Steam, Gog };
struct Game {
    std::wstring dir, root, gogId;
    Store store=Store::Unknown;
    bool valid=false, unsupported64=false;
    std::string note;
};
Game inspect(const std::wstring& chosen);
std::vector<Game> find_games();
std::wstring galaxy_path();
struct Launch { std::wstring target,args; std::string error; };
Launch launch_command(const Game& game,const std::wstring& galaxy);
const char* launch_label(Store store);
}
