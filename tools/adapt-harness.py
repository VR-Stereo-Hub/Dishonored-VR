"""One-shot: strip the BioShock -Game switch, process names and per-game paths
from the harness scripts copied from bioshock-vr, so they drive Dishonored.
Kept as the record of the adaptation. Run from the repo root."""
import io
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def rd(p):
    return io.open(os.path.join(ROOT, p), encoding="utf-8", newline="").read().replace("\r\n", "\n")


def wr(p, s):
    io.open(os.path.join(ROOT, p), "w", encoding="utf-8", newline="\n").write(s)


RULES = [
    # the -Game parameter goes away; scripts take -GamePath like install.ps1
    (r'\s*\[ValidateSet\("bs1", "bs2"(, "bsi")?\)\]\[string\]\$Game = "bs[12]",\n', "\n    [string]$GamePath = \"\",\n"),
    # the sibling-game guard does not exist here
    (r'# One game owns the headset at a time \(see tools\\lib\\assert-no-conflict\.ps1\)\.\nif \(\$Game -eq "bsi"\) \{\n    \. \(Join-Path \$PSScriptRoot "lib\\assert-no-conflict\.ps1"\)\n    Assert-NoConflictingGame -Game \$Game -Force:\$Force\n\}\n', ""),
    # process name switches
    (r'switch \(\$Game\) \{\n    "bs2" \{ \$procName = "Bioshock2HD" \}\n    "bsi" \{ \$procName = "BioShockInfinite" \}\n    default \{ \$procName = "BioshockHD" \}\n\}', '$procName = "Dishonored"'),
    (r'\$proc = switch \(\$Game\) \{ "bs2" \{ "Bioshock2HD" \} "bsi" \{ "BioShockInfinite" \} default \{ "BioshockHD" \} \}', '$proc = "Dishonored"'),
    (r'\$proc = if \(\$Game -eq "bs2"\) \{ "Bioshock2HD" \} else \{ "BioshockHD" \}', '$proc = "Dishonored"'),
    (r'\$dir  = if \(\$Game -eq "bs2"\) \{ "\$env:LOCALAPPDATA\\DishonoredVR\\bs2" \}\n\s*else \{ "\$env:LOCALAPPDATA\\DishonoredVR" \}', '$dir  = Get-DvrDataDir'),
    (r'\$log  = "\$dir\\dishonoredvr\.log"', '$log  = Get-DvrLogPath $GamePath'),
    (r'\$log      = Join-Path \$dir "dishonoredvr\.log"', '$log      = Get-DvrLogPath $GamePath'),
    (r'"\$env:LOCALAPPDATA\\DishonoredVR\\bsi\\dishonoredvr\.log"', '(Get-DvrLogPath)'),
    (r'Get-Process "BioShockInfinite"', 'Get-Process "Dishonored"'),
    (r' -Game bsi\.', '.'),
    (r' -Game \$Game', ''),
    (r'\[-Game bs2\|bsi\]', ''),
    (r'-Game bs[12i] ', ''),
    (r'BioshockHD window', 'Dishonored window'),
    (r"'\\\[b2r\\\] camera:'", "'heartbeat:'"),
    (r"'\\\[b1r\\\] camera:'", "'heartbeat:'"),
]

FILES = ["tools/game-key.ps1", "tools/game-click.ps1", "tools/game-shot.ps1", "tools/game-batch.ps1",
         "tools/eye-check.ps1", "tools/soak.ps1", "tools/xrsim-run.ps1", "tools/xrsim-cmd.ps1",
         "tools/xrsim-state.ps1", "tools/xrsim-shot.ps1", "tools/xrsim-install.ps1", "tools/xrsim-selftest.ps1"]

for p in FILES:
    s = rd(p)
    o = s
    for pat, rep in RULES:
        s = re.sub(pat, rep, s)
    # every script that resolves paths dot-sources the lib once, right after the param block
    if "Get-Dvr" in s and 'lib\\game-path.ps1' not in s:
        m = re.search(r"\nparam\((?:.|\n)*?\n\)\n", s)
        if m:
            s = s[:m.end()] + '. (Join-Path $PSScriptRoot "lib\\game-path.ps1")\n' + s[m.end():]
    # default sim dir under the data dir
    s = s.replace('[string]$Dir = "$env:LOCALAPPDATA\\DishonoredVR\\xrsim"', '[string]$Dir = "$env:LOCALAPPDATA\\DishonoredVR\\xrsim"')
    if s != o:
        wr(p, s)
        print("adapted", p)
    left = [ln for ln in s.split("\n") if re.search(r"\$Game\b|Bioshock|bs2|bsi", ln)]
    for ln in left:
        print("   leftover:", p, ln.strip()[:110])
