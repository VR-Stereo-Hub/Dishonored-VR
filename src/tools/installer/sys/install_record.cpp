// tools/installer/sys/install_record.cpp - see install_record.h. A hand-rolled
// reader for our own flat object: "key": "string" | number | true | false.
#include "sys/install_record.h"
#include "sys/fs.h"
#include <vector>

namespace dvr::setup {

namespace {
std::string esc(const std::string& s)
{
    std::string r;
    for (char c : s) {
        switch (c) {
        case '"': r += "\\\""; break;
        case '\\': r += "\\\\"; break;
        case '\n': r += "\\n"; break;
        case '\r': r += "\\r"; break;
        case '\t': r += "\\t"; break;
        default: r += c;
        }
    }
    return r;
}
void skip_ws(const std::string& s, size_t* i) { while (*i < s.size() && (s[*i] == ' ' || s[*i] == '\t' || s[*i] == '\r' || s[*i] == '\n')) ++*i; }
bool read_string(const std::string& s, size_t* i, std::string* out)
{
    if (*i >= s.size() || s[*i] != '"') return false;
    ++*i;
    out->clear();
    while (*i < s.size() && s[*i] != '"') {
        if (s[*i] == '\\' && *i + 1 < s.size()) {
            const char e = s[*i + 1];
            *out += e == 'n' ? '\n' : e == 'r' ? '\r' : e == 't' ? '\t' : e;
            *i += 2;
        } else {
            *out += s[(*i)++];
        }
    }
    if (*i >= s.size()) return false;
    ++*i;
    return true;
}
}

std::string record_to_json(const InstallRecord& r)
{
    std::string j = "{\r\n";
    j += "  \"version\": \"" + esc(r.version) + "\",\r\n";
    j += "  \"buildId\": \"" + esc(r.buildId) + "\",\r\n";
    j += "  \"config\": \"" + esc(r.config) + "\",\r\n";
    j += "  \"d3d9Sha256\": \"" + esc(r.dllSha256) + "\",\r\n";
    j += "  \"installedUtc\": \"" + esc(r.installedUtc) + "\",\r\n";
    j += "  \"runtime\": \"" + esc(r.runtime) + "\",\r\n";
    j += "  \"quality\": \"" + esc(r.quality) + "\",\r\n";
    j += fs::format("  \"renderWidth\": %d,\r\n  \"renderHeight\": %d,\r\n", r.width, r.height);
    j += std::string("  \"elevated\": ") + (r.elevated ? "true" : "false") + "\r\n";
    j += "}\r\n";
    return j;
}

bool record_from_json(const std::string& s, InstallRecord* out)
{
    *out = InstallRecord();
    size_t i = 0;
    skip_ws(s, &i);
    if (i >= s.size() || s[i] != '{') return false;
    ++i;
    for (;;) {
        skip_ws(s, &i);
        if (i < s.size() && s[i] == '}') break;
        std::string key;
        if (!read_string(s, &i, &key)) return false;
        skip_ws(s, &i);
        if (i >= s.size() || s[i] != ':') return false;
        ++i;
        skip_ws(s, &i);
        std::string sval; int ival = 0; bool bval = false; bool isStr = false, isBool = false;
        if (i < s.size() && s[i] == '"') { if (!read_string(s, &i, &sval)) return false; isStr = true; }
        else if (s.compare(i, 4, "true") == 0) { bval = true; isBool = true; i += 4; }
        else if (s.compare(i, 5, "false") == 0) { bval = false; isBool = true; i += 5; }
        else {
            size_t j = i;
            while (j < s.size() && (s[j] == '-' || (s[j] >= '0' && s[j] <= '9'))) ++j;
            if (j == i) return false;
            ival = atoi(s.substr(i, j - i).c_str());
            i = j;
        }
        if (key == "version" && isStr) out->version = sval;
        else if (key == "buildId" && isStr) out->buildId = sval;
        else if (key == "config" && isStr) out->config = sval;
        else if (key == "d3d9Sha256" && isStr) out->dllSha256 = sval;
        else if (key == "installedUtc" && isStr) out->installedUtc = sval;
        else if (key == "runtime" && isStr) out->runtime = sval;
        else if (key == "quality" && isStr) out->quality = sval;
        else if (key == "renderWidth" && !isStr) out->width = ival;
        else if (key == "renderHeight" && !isStr) out->height = ival;
        else if (key == "elevated" && isBool) out->elevated = bval;
        skip_ws(s, &i);
        if (i < s.size() && s[i] == ',') { ++i; continue; }
        if (i < s.size() && s[i] == '}') break;
        return false;
    }
    out->valid = !out->version.empty() && !out->dllSha256.empty();
    return out->valid;
}

bool read_record(const std::wstring& gameDir, InstallRecord* out)
{
    std::vector<uint8_t> bytes;
    if (!fs::read_file(fs::join(gameDir, kRecordName), &bytes, nullptr)) return false;
    return record_from_json(std::string((const char*)bytes.data(), bytes.size()), out);
}

bool write_record(const std::wstring& gameDir, const InstallRecord& r, DWORD* err)
{
    const std::string j = record_to_json(r);
    return fs::write_file_atomic(fs::join(gameDir, kRecordName), j.data(), j.size(), err);
}

} // namespace dvr::setup
