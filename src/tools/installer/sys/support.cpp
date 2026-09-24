#include "sys/support.h"
#include "sys/fs.h"
#include "sys/process.h"
#include "sys/resources.h"
#include "payload_ids.h"
#include "core/util/log.h"
namespace dvr::setup::support {
bool collect(const std::wstring& gameDir, const std::wstring& outDir, bool openFolder, std::string* notice)
{
    const resources::Blob script = resources::rcdata(IDR_COLLECT_SUPPORT);
    if(!script.ok()) { *notice="This launcher is missing the support collector. Download the launcher again.";return false; }
    // VR-215: CreateDirectory only made the leaf, failing on a fresh profile.
    // TEMP can also point at a deleted or blocked folder; try local app data next.
    std::wstring dir,ps1,out;
    DWORD err=0;
    const auto leaf=L"support-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    for(const auto& root:{fs::join(fs::temp_dir(),L"DishonoredVR-Launcher"),
                         fs::join(fs::known_folder(FOLDERID_LocalAppData),L"DishonoredVR\\SupportCollector")}) {
        dir=fs::join(root,leaf);ps1=fs::join(dir,L"collect-support.ps1");out=fs::join(dir,L"collect-support.out.txt");
        if(fs::make_dirs(dir,&err) && fs::write_file_atomic(ps1,script.data,script.size,&err)) {err=0;break;}
        DVR_WARN("launcher: collector staging failed at %s: %s",fs::narrow(dir).c_str(),fs::narrow(fs::win_error_text(err)).c_str());
    }
    if(err) {
        *notice="Could not prepare support collection in "+fs::narrow(dir)+": "+fs::narrow(fs::win_error_text(err));return false;
    }
    dvr::log::flush();
    // This launcher is 32-bit: Sysnative reaches the full system PowerShell on
    // 64-bit Windows. Never resolve an executable from Downloads or PATH.
    wchar_t windows[MAX_PATH]{}; GetWindowsDirectoryW(windows, MAX_PATH);
    std::wstring powershell = fs::join(windows, L"Sysnative\\WindowsPowerShell\\v1.0\\powershell.exe");
    if (!fs::is_file(powershell)) powershell = fs::join(windows, L"System32\\WindowsPowerShell\\v1.0\\powershell.exe");
    std::wstring cmd = process::quote_arg(powershell) + L" -NoProfile -NonInteractive -ExecutionPolicy Bypass -File " + process::quote_arg(ps1) + L" -GameDir " + process::quote_arg(gameDir);
    if (!outDir.empty()) cmd += L" -OutDir " + process::quote_arg(outDir);
    if (!openFolder) cmd += L" -NoOpen";
    DWORD code = 0;
    if (!process::run_wait(cmd, &code, &err, 300000, out)) {
        *notice = "Could not run the collector: " + fs::narrow(fs::win_error_text(err)); return false;
    }
    std::vector<uint8_t> bytes; fs::read_file(out, &bytes, nullptr);
    const std::string text(bytes.begin(), bytes.end());
    const size_t at = text.find("Support ZIP: ");
    if (code == 0 && at != std::string::npos) {
        const size_t end = text.find_first_of("\r\n", at);
        *notice = "Support bundle: " + text.substr(at + 13, end == std::string::npos ? std::string::npos : end - at - 13);
        DVR_INFO("launcher: %s", notice->c_str()); return true;
    }
    DVR_ERROR("launcher: collector exit %lu; output at %s\n%s", code, fs::narrow(out).c_str(), text.c_str());
    const size_t start = text.find_first_not_of(" \t\r\n");
    const size_t end = start == std::string::npos ? start : text.find_first_of("\r\n", start);
    *notice = fs::format("Collector failed (exit %lu). ", code) + (start == std::string::npos ? "No output was returned." : text.substr(start, end - start));
    *notice += " Open the launcher log for details.";
    return false;
}
}
