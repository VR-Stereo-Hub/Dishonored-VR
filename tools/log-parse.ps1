# log-parse.ps1 - summarise a dishonored_vr.log: build, backend verdict, fork
# exports, config lines, XR bring-up, every WARN/ERROR line, and the tail.
#   .\tools\log-parse.ps1                     # the installed game's log
#   .\tools\log-parse.ps1 -Path some.log      # a user's report
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([string]$Path = "", [int]$Tail = 20)
. (Join-Path $PSScriptRoot "lib\game-path.ps1")
if (-not $Path) { $Path = Get-DvrLogPath }
if (-not (Test-Path $Path)) { throw "no log at $Path" }
$lines = Get-Content $Path
"file: $Path ($($lines.Count) lines)"
function Section($title, $pattern, $max = 12) {
    $hits = @($lines | Where-Object { $_ -match $pattern })
    if ($hits.Count -eq 0) { return }
    ""
    "--- $title ($($hits.Count))"
    $hits | Select-Object -First $max
    if ($hits.Count -gt $max) { "    ... $($hits.Count - $max) more" }
}
Section "build"        "proxy loaded" 2
Section "backend"      "config: VR backend|backend:|probe" 6
Section "config"       "\[cfg\]" 40
Section "openxr"       "xr: " 25
Section "openvr"       "openvr|SteamVR|VR_Init" 10
Section "hooks"        "INSTALLED|REFUSING|hooked|hook installed" 12
Section "warnings"     "\[W\]|\[E\]|REFUSING|FAILED|failed|EXCEPTION" 40
""
"--- tail ($Tail)"
$lines | Select-Object -Last $Tail
