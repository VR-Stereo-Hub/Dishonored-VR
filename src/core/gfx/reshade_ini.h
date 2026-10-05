// Byte-exact edits of ReShade.ini and reads of a ReShade preset. Pure text in, text out.
//
// ReShade.ini is UTF-8 without a BOM and is the player's file: the profile API would read it
// as ANSI and rewrite a non-ASCII search path wrongly, so nothing here re-encodes anything.
// Only the named key's line changes; every other byte, and the file's line ending, is kept.
// ReShade writes a list as comma-separated items and doubles a comma inside an item.
#pragma once
#include <string>
#include <vector>
#include <cctype>
namespace dvr::reshade_ini {
namespace detail {
inline bool ieq(const std::string& a, const char* b) {
    size_t i = 0;
    for (; i < a.size() && b[i]; ++i) if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
    return i == a.size() && !b[i];
}
inline std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}
struct Found { size_t valueBegin = std::string::npos, valueEnd = 0, sectionEnd = std::string::npos; bool section = false; };
// valueBegin..valueEnd: the value's bytes (no line ending). sectionEnd: where a new key of
// that section would be inserted (the start of the line after its last non-blank line).
inline Found find(const std::string& text, const char* section, const char* key) {
    Found f; bool in = false; size_t pos = 0;
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) pos = 3;
    while (pos <= text.size()) {
        size_t eol = text.find('\n', pos);
        const size_t next = eol == std::string::npos ? text.size() : eol + 1;
        size_t end = eol == std::string::npos ? text.size() : eol;
        if (end > pos && text[end - 1] == '\r') --end;
        const std::string line = trim(text.substr(pos, end - pos));
        if (!line.empty() && line.front() == '[' && line.back() == ']') {
            if (in) return f;                       // the wanted section ended without the key
            in = ieq(line.substr(1, line.size() - 2), section);
            if (in) { f.section = true; f.sectionEnd = next; }
        } else if (in && !line.empty()) {
            f.sectionEnd = next;
            const size_t eq = text.find('=', pos);
            if (eq != std::string::npos && eq < end && ieq(trim(text.substr(pos, eq - pos)), key)) {
                f.valueBegin = eq + 1; f.valueEnd = end; return f;
            }
        }
        if (next >= text.size()) break;
        pos = next;
    }
    return f;
}
inline const char* eol_of(const std::string& text) { return text.find("\r\n") != std::string::npos || text.empty() ? "\r\n" : "\n"; }
}
inline bool get(const std::string& text, const char* section, const char* key, std::string* value) {
    const detail::Found f = detail::find(text, section, key);
    if (f.valueBegin == std::string::npos) return false;
    if (value) *value = detail::trim(text.substr(f.valueBegin, f.valueEnd - f.valueBegin));
    return true;
}
// True when the text changed. A missing key is added at the end of its section, a missing
// section at the end of the file.
inline bool set(std::string& text, const char* section, const char* key, const std::string& value) {
    const detail::Found f = detail::find(text, section, key);
    const char* eol = detail::eol_of(text);
    if (f.valueBegin != std::string::npos) {
        if (detail::trim(text.substr(f.valueBegin, f.valueEnd - f.valueBegin)) == value) return false;
        text.replace(f.valueBegin, f.valueEnd - f.valueBegin, value); return true;
    }
    const std::string line = std::string(key) + "=" + value + eol;
    if (f.section) {
        size_t at = f.sectionEnd;
        if (at > text.size()) at = text.size();
        if (at == text.size() && !text.empty() && text.back() != '\n') { text += eol; at = text.size(); }
        text.insert(at, line); return true;
    }
    if (!text.empty() && text.back() != '\n') text += eol;
    text += std::string("[") + section + "]" + eol + line;
    return true;
}
inline std::vector<std::string> split_list(const std::string& value) {
    std::vector<std::string> out; std::string item;
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] == ',') {
            if (i + 1 < value.size() && value[i + 1] == ',') { item += ','; ++i; continue; }   // an escaped comma
            out.push_back(detail::trim(item)); item.clear();
        } else item += value[i];
    }
    item = detail::trim(item);
    if (!item.empty() || !out.empty()) out.push_back(item);
    std::vector<std::string> kept;
    for (auto& s : out) if (!s.empty()) kept.push_back(s);
    return kept;
}
// Append the items the list lacks (compared without case, and without a leading ".\").
// Existing items and their order are the player's and are kept. Returns how many were added.
inline int add_list_items(std::string& text, const char* section, const char* key, const std::vector<std::string>& items) {
    std::string value; get(text, section, key, &value);
    const auto have = split_list(value);
    auto norm = [](std::string s) {
        for (auto& c : s) { if (c == '/') c = '\\'; c = (char)std::tolower((unsigned char)c); }
        if (s.rfind(".\\", 0) == 0) s.erase(0, 2);
        while (!s.empty() && s.back() == '\\') s.pop_back();
        return s;
    };
    int added = 0;
    for (const auto& item : items) {
        bool present = false;
        for (const auto& h : have) if (norm(h) == norm(item)) { present = true; break; }
        if (present) continue;
        std::string escaped;
        for (char c : item) { escaped += c; if (c == ',') escaped += ','; }
        value += (value.empty() ? "" : ",") + escaped; ++added;
    }
    if (added) set(text, section, key, value);
    return added;
}
// A preset's enabled techniques, "Name@File.fx", in the order of its `Techniques=` line.
inline std::vector<std::string> preset_techniques(const std::string& preset) {
    size_t pos = 0;
    if (preset.size() >= 3 && (unsigned char)preset[0] == 0xEF) pos = 3;
    while (pos < preset.size()) {
        size_t eol = preset.find('\n', pos);
        if (eol == std::string::npos) eol = preset.size();
        const std::string line = detail::trim(preset.substr(pos, eol - pos));
        if (!line.empty() && line.front() == '[') break;             // the global keys come before any section
        const size_t eq = line.find('=');
        if (eq != std::string::npos && detail::ieq(detail::trim(line.substr(0, eq)), "Techniques")) return split_list(line.substr(eq + 1));
        pos = eol + 1;
    }
    return {};
}
}
