# tool-paths.ps1 - where THIS machine keeps the offline tools (IDA, Blender, UModel, ...).
# Dot-source it from any script that needs one of them:
#   . (Join-Path $PSScriptRoot "lib\tool-paths.ps1")
#   $idat = Get-DvrTool idat            # throws with the fix if it is not configured
#
# The answers live in a LOCAL, per-user preference file that is never committed:
#   $env:DVR_TOOLS_FILE                 (explicit override)
#   %LOCALAPPDATA%\DishonoredVR\dev-tools.json   (default)
# It does NOT follow DVR_DATA_DIR: that directory is the mod's bulk data and may move
# to another drive for capture space; a dozen tool paths belong to the user profile.
#
# The repo knows only how to LOOK for a tool (the catalog below); the file records
# what was FOUND or what the user SET. A "user" entry is never overwritten by
# detection. tools\tool-paths.ps1 is the command-line front end (-Init/-Set/-Get).
# Several of these tools are paid or third-party software: they are referenced by
# path, never copied into the tree.
# NOTE: keep this file pure ASCII (PowerShell 5.1 misreads BOM-less UTF-8).

# Captured at dot-source time: inside a function or a catalog scriptblock,
# $PSScriptRoot/$PSCommandPath are not reliably this file's.
$script:DvrToolLibDir = $PSScriptRoot

function Get-DvrToolsFile {
    if ($env:DVR_TOOLS_FILE) { return $env:DVR_TOOLS_FILE }
    return Join-Path $env:LOCALAPPDATA "DishonoredVR\dev-tools.json"
}

# The main checkout, also when called from a linked worktree: ignored tool folders
# (build\hud-assets\tools, tools\uscript\_ueexplorer) exist only there.
function Get-DvrMainCheckout {
    $repo = Split-Path -Parent (Split-Path -Parent $script:DvrToolLibDir)
    try {
        $common = (& git -C $repo rev-parse --path-format=absolute --git-common-dir 2>$null)
        if ($LASTEXITCODE -eq 0 -and $common) { return (Split-Path -Parent $common.Trim()) }
    } catch {}
    return $repo
}

# Highest-versioned match of a wildcard path, or $null.
function Find-DvrGlob {
    param([string[]]$Patterns)
    foreach ($p in $Patterns) {
        if (-not $p) { continue }
        $hits = @(Get-Item -Path $p -ErrorAction SilentlyContinue | Sort-Object FullName -Descending)
        if ($hits.Count -gt 0) { return $hits[0].FullName }
    }
    return $null
}

function Find-DvrOnPath {
    param([string]$Exe)
    $c = Get-Command $Exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($c) { return $c.Source }
    return $null
}

# The catalog: name -> what it is and where a default install puts it. Order is the
# display order. 'kind' is file or dir. 'required' marks the tools a workflow needs;
# the rest are listed so a session knows they exist on this machine.
function Get-DvrToolCatalog {
    # Script scope, not locals: the find blocks run later, from the caller's scope.
    $script:pf = $env:ProgramFiles; $script:pf86 = ${env:ProgramFiles(x86)}
    $script:main = Get-DvrMainCheckout; $script:docs = [Environment]::GetFolderPath('MyDocuments')
    return [ordered]@{
        idat            = @{ kind='file'; group='IDA';     what='IDA headless binary (idat.exe; IDA 9 has no idat64)'
                             find={ Find-DvrGlob @("$pf\IDA Professional *\idat.exe","$pf\IDA Pro*\idat.exe","$pf\IDA*\idat.exe","C:\IDA*\idat.exe") } }
        ida_workspace   = @{ kind='dir';  group='IDA';     what='staged exe copy + .i64 database (bin\) and script outputs (out\); NEVER in the repo'
                             find={ Join-Path $env:LOCALAPPDATA "DishonoredVR\ida" }; create=$true }
        blender         = @{ kind='file'; group='Models';  what='Blender (run headless: -b --python)'
                             find={ $x = Find-DvrGlob @("$pf\Blender Foundation\Blender *\blender.exe","$pf86\Steam\steamapps\common\Blender\blender.exe"); if ($x) { $x } else { Find-DvrOnPath blender.exe } } }
        psk_addon_zip   = @{ kind='file'; group='Models';  what='io_scene_psk_psa extension zip (PSK/PSA import/export for Blender)'
                             find={ Find-DvrGlob @("$main\build\model-tools\io_scene_psk_psa-*.zip","$docs\*\tools\io_scene_psk_psa-*.zip") } }
        umodel          = @{ kind='file'; group='Models';  what='UE Viewer / UModel (cooked .upk -> PSK/PSA/TGA)'
                             find={ $x = Find-DvrGlob @("$main\build\model-tools\umodel.exe","$main\build\hud-assets\tools\umodel.exe","$docs\*\tools\umodel.exe"); if ($x) { $x } else { Find-DvrOnPath umodel.exe } } }
        model_workspace = @{ kind='dir';  group='Models';  what='extracted meshes, .blend projects, exports, previews; NEVER in the repo'
                             find={ $x = Find-DvrGlob @("$docs\Dishonored-VR-Arms","$docs\DishonoredVR-Models"); if ($x) { $x } else { Join-Path $docs "DishonoredVR-Models" } }; create=$true }
        ffdec           = @{ kind='file'; group='Content'; what='JPEXS FFDec CLI (Scaleform .gfx -> XML, scripts, frames)'
                             find={ Find-DvrGlob @("$pf\FFDec\ffdec-cli.exe","$pf86\FFDec\ffdec-cli.exe","$main\build\hud-assets\tools\ffdec\ffdec-cli.exe","$main\build\hud-assets\tools\ffdec\ffdec.jar") } }
        ueexplorer      = @{ kind='dir';  group='Content'; what='UE Explorer 1.6.2 + our ExportScripts.exe (UELib batch decompiler)'
                             find={ Find-DvrGlob @("$main\tools\uscript\_ueexplorer\ue-explorer") } }
        uscript_corpus  = @{ kind='dir';  group='Content'; what='decompiled UnrealScript dump (declarations + defaultproperties)'
                             find={ Find-DvrGlob @("$main\tools\uscript\dishonored") } }
        game_dir        = @{ kind='dir';  group='Game';    what='folder holding Dishonored.exe (lib\game-path.ps1 resolves it the same way)'
                             find={ try { . (Join-Path $script:DvrToolLibDir 'game-path.ps1'); Get-DvrGamePath } catch { $null } } }
        cooked_dir      = @{ kind='dir';  group='Game';    what='DishonoredGame\CookedPCConsole (the .upk packages UModel reads)'
                             find={ try { . (Join-Path $script:DvrToolLibDir 'game-path.ps1'); $g = Get-DvrGamePath; Join-Path (Split-Path -Parent (Split-Path -Parent $g)) 'DishonoredGame\CookedPCConsole' } catch { $null } } }
        python          = @{ kind='file'; group='Runtime'; what='Python 3 for tools\*.py (capstone for disasm-rva.py)'
                             find={ $x = Find-DvrOnPath py.exe; if ($x) { $x } else { Find-DvrOnPath python.exe } } }
        java            = @{ kind='file'; group='Runtime'; what='Java (ffdec.jar fallback)'
                             find={ Find-DvrOnPath java.exe } }
        pix             = @{ kind='file'; group='Debug';   what='PIX for Windows (DX SDK June 2010) - the D3D9 frame debugger; RenderDoc has no D3D9'
                             find={ Find-DvrGlob @("$pf86\Microsoft DirectX SDK (June 2010)\Utilities\bin\x86\PIXWin.exe") } }
        cheatengine     = @{ kind='file'; group='Debug';   what='Cheat Engine (live memory scan / structure dissect)'
                             find={ Find-DvrGlob @("$pf\Cheat Engine*\Cheat Engine.exe","$pf86\Cheat Engine*\Cheat Engine.exe") } }
        hxd             = @{ kind='file'; group='Debug';   what='HxD hex editor'
                             find={ Find-DvrGlob @("$pf\HxD\HxD.exe","$pf86\HxD\HxD.exe") } }
        x32dbg          = @{ kind='file'; group='Debug';   what='x64dbg suite, 32-bit debugger (optional)'
                             find={ $x = Find-DvrGlob @("$pf\x64dbg\release\x32\x32dbg.exe","C:\x64dbg\release\x32\x32dbg.exe"); if ($x) { $x } else { Find-DvrOnPath x32dbg.exe } } }
        renderdoc       = @{ kind='file'; group='Debug';   what='RenderDoc (D3D11 side only: the mod''s compositor textures, not the game)'
                             find={ Find-DvrGlob @("$pf\RenderDoc\qrenderdoc.exe") } }
        ghidra          = @{ kind='dir';  group='Debug';   what='Ghidra (optional free alternative to IDA)'
                             find={ Find-DvrGlob @("$pf\ghidra*","C:\ghidra*") } }
    }
}

function Read-DvrToolsFile {
    $f = Get-DvrToolsFile
    $map = [ordered]@{}
    if (Test-Path -LiteralPath $f) {
        $raw = Get-Content -LiteralPath $f -Raw
        if ($raw.Trim()) {
            $j = $raw | ConvertFrom-Json
            if ($j.tools) { foreach ($p in $j.tools.PSObject.Properties) { $map[$p.Name] = $p.Value } }
        }
    }
    return $map
}

function Write-DvrToolsFile {
    param([System.Collections.IDictionary]$Map)
    $f = Get-DvrToolsFile
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $f) | Out-Null
    $doc = [ordered]@{
        schema  = 1
        comment = 'Local tool locations for the Dishonored VR repo. Per user, never committed. Edit with tools\tool-paths.ps1 -Set name=path; source=user entries are never re-detected.'
        updated = (Get-Date -Format s)
        tools   = $Map
    }
    # UTF-8 without BOM so Python's json and IDA/Blender scripts read it cleanly.
    [IO.File]::WriteAllText($f, ($doc | ConvertTo-Json -Depth 5), (New-Object Text.UTF8Encoding($false)))
}

# The path for one tool, or throw with the exact fix. -Optional returns $null instead.
function Get-DvrTool {
    param([Parameter(Mandatory)][string]$Name, [switch]$Optional)
    $map = Read-DvrToolsFile
    $cat = Get-DvrToolCatalog
    $p = $null
    if ($map.Contains($Name) -and $map[$Name].path) { $p = [string]$map[$Name].path }
    elseif ($cat.Contains($Name)) { $p = & $cat[$Name].find }
    if ($p -and (Test-Path -LiteralPath $p)) { return $p }
    if ($p -and $cat.Contains($Name) -and $cat[$Name].create) {
        New-Item -ItemType Directory -Force -Path $p | Out-Null
        return $p
    }
    if ($Optional) { return $null }
    $why = if ($p) { "configured as '$p' but that path does not exist" } else { 'not configured and not found in any default location' }
    throw "Tool '$Name' $why. Fix: .\tools\tool-paths.ps1 -Set $Name=<path>   (file: $(Get-DvrToolsFile))"
}
