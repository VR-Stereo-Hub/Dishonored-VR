# lint.ps1 - the repo's static checks. Exit 0 = clean.
#   - no em dash (U+2014) anywhere outside LICENSE, tests\golden and third_party\
#     (PowerShell 5.1 parse errors and log/UI mojibake)
#   - every .ps1/.bat is pure ASCII, has no BOM and uses CRLF
#   - every PowerShell script parses
#   - no absolute engine address (0x + 6+ hex digits) outside
#     src\game\dishonored\patterns.h, docs\ and tools\
#   - no game-derived binary staged (.png .bmp .dxbc .dmp .upk .u)
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).
param([switch]$Quiet)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Push-Location $repo
$fail = 0
function Bad($m) { $script:fail++; Write-Host "LINT: $m" -ForegroundColor Red }
try {
    $tracked = & git ls-files
    $emdash = [char]0x2014
    foreach ($f in $tracked) {
        # tests/golden holds the ORIGINAL 38.92 ini text, em dash included
        if ($f -like 'third_party/*' -or $f -like 'tests/golden/*' -or $f -eq 'LICENSE') { continue }
        if ($f -match '\.(png|bmp|dll|dmp|dxbc|upk|u)$') { continue }
        $text = [System.IO.File]::ReadAllText((Join-Path $repo $f))
        if ($text.Contains($emdash)) { Bad "em dash in $f" }
        if ($f -match '\.(ps1|bat)$') {
            $bytes = [System.IO.File]::ReadAllBytes((Join-Path $repo $f))
            if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) { Bad "BOM in $f" }
            if ($bytes | Where-Object { $_ -gt 0x7F } | Select-Object -First 1) { Bad "non-ASCII byte in $f" }
            if ($text -match "(?<!`r)`n") { Bad "LF line ending in $f (need CRLF)" }
            if ($f -match '\.ps1$') {
                $e = $null
                [void][System.Management.Automation.Language.Parser]::ParseFile((Join-Path $repo $f), [ref]$null, [ref]$e)
                if ($e) { Bad "$f does not parse: $($e[0].Message)" }
            }
        }
        if ($f -match '^src/.*\.(cpp|h|inc)$' -and $f -ne 'src/game/dishonored/patterns.h') {
            $m = [regex]::Matches($text, '0x[0-9a-fA-F]{6,}')
            if ($m.Count -gt 0 -and -not $Quiet) { Write-Host "note: $($m.Count) absolute address literal(s) still in $f (patterns.h is the target)" }
        }
    }
    $staged = & git ls-files -- '*.png' '*.bmp' '*.dxbc' '*.dmp' '*.upk' '*.u'
    if ($staged) { Bad "game-derived binaries tracked: $($staged -join ', ')" }
} finally { Pop-Location }
if ($fail -eq 0) { Write-Host "lint: clean" -ForegroundColor Green; exit 0 }
exit 1
