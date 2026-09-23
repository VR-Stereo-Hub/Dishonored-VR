// tools/installer/sys/game_ini.h - the game's own ini files, edited the way
// tools/setup-game-ini.ps1 edits them (Set-IniKeyInSection), because the
// engine reads them with rules the private-profile API does not follow:
//  - the same key exists in more than one section (DepthOfField and Fullscreen
//    are in [SystemSettings] AND [SystemSettingsMobile]; UseVsync also in all
//    four [AppCompatBucketN]), so a match is section-first;
//  - they are CRLF, and a regex that eats the \r produces a mixed file;
//  - the value's case matters to the engine (FALSE vs False), so a value is
//    compared case-sensitively while section and key match case-insensitively
//    (PowerShell's -eq / -match against -ceq, which is what the script does);
//  - a key the file does not carry is appended at the end of its section
//    (VR-117), a section the file does not carry is a refusal with nothing
//    written.
// Every other byte of the file is preserved: the BOM if any, each line's own
// line ending, blank lines, comments. A UTF-16 file (UE3 rewrites a config as
// UTF-16 once a non-ANSI character enters it) is edited as UTF-8 and written
// back as UTF-16.
#pragma once
#include <windows.h>
#include <stdint.h>
#include <string>
#include <vector>

namespace dvr::setup {

class GameIni {
public:
    enum class Result { Unchanged, Changed, NoSection };

    static bool load(const std::wstring& path, GameIni* out, std::wstring* err);
    // Parses bytes already in memory (the unit tests). encoding is set by the BOM.
    void parse(const std::vector<uint8_t>& bytes);
    // note receives "[Section] Key: old -> new" / "... already set" / "... was MISSING - appended".
    Result set(const std::string& section, const std::string& key, const std::string& value, std::string* note);
    bool dirty() const { return dirty_; }
    std::vector<uint8_t> serialise() const;
    // Writes atomically. Returns true when nothing was dirty (no write) too.
    bool save(std::wstring* err);
    const std::wstring& path() const { return path_; }

private:
    std::wstring path_;
    std::vector<uint8_t> bom_;          // EF BB BF, or FF FE, or empty
    bool utf16_ = false;
    std::vector<std::string> lines_;    // each WITH its own line ending, if it had one
    std::string eol_ = "\r\n";          // the file's dominant ending, for appended lines
    bool dirty_ = false;

    struct Section { size_t header; size_t end; };   // [header, end) indices into lines_
    bool find_section(const std::string& name, Section* out) const;
    static std::string body(const std::string& line);      // without the line ending
    static std::string ending(const std::string& line);    // "" | "\n" | "\r\n"
};

// The four values the mod depends on, with the reasons in setup-game-ini.ps1.
struct BaselineWrite { const char* file; const char* section; const char* key; const char* value; };
extern const BaselineWrite kVrBaseline[4];

} // namespace dvr::setup
