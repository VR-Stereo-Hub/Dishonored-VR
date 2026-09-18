# Opt-in external collector. Never launches or kills the game; no global debugger registration.
param(
    [Parameter(Mandatory=$true)][string]$ProcDump,
    [Parameter(Mandatory=$true)][string]$GameDir,
    [Parameter(Mandatory=$true)][string]$DataDir,
    [ValidateSet('Crash','Memory')][string]$Mode='Crash',
    [ValidateRange(128,3800)][int]$MemoryMB=3000
)
$ErrorActionPreference='Stop'
$ProcDump=(Resolve-Path -LiteralPath $ProcDump).Path
$sig=Get-AuthenticodeSignature -LiteralPath $ProcDump
if($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') { throw 'ProcDump must have a valid Microsoft signature.' }
$gameExe=Join-Path (Resolve-Path -LiteralPath $GameDir).Path 'Dishonored.exe'
if(-not (Test-Path -LiteralPath $gameExe)) { throw 'Dishonored.exe missing from GameDir.' }
$session=Join-Path $DataDir ('support-watch\'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Path $session -Force | Out-Null
$dumpDir=Join-Path $DataDir 'dumps'
New-Item -ItemType Directory -Path $dumpDir -Force | Out-Null
Write-Output "Waiting for a user-launched game. Mode=$Mode. Evidence: $session"
$process=$null
while(-not $process) {
    $process=Get-Process -Name Dishonored -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $gameExe } | Select-Object -First 1
    if(-not $process){Start-Sleep -Seconds 1}
}
$targetId=$process.Id
$argsList=@('-accepteula','-n','1','-e','-t')
if($Mode -eq 'Memory') { $argsList+=@('-ma','-m',"$MemoryMB",'-s','1') } else { $argsList+='-mm' }
$dumpPath=Join-Path $dumpDir ("external-$Mode-$targetId-"+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.dmp')
$argsList+=@("$targetId",$dumpPath)
@{ pid=$targetId; mode=$Mode; memoryThresholdMB=$MemoryMB; exe=$gameExe; dumperSHA256=(Get-FileHash $ProcDump).Hash; args=$argsList; startedUtc=[DateTime]::UtcNow.ToString('o') } | ConvertTo-Json | Set-Content (Join-Path $session 'watch.json')
$quoted=@($argsList | ForEach-Object {'"'+$_+'"'})
$monitor=Start-Process -FilePath $ProcDump -ArgumentList $quoted -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $session 'procdump.txt') -RedirectStandardError (Join-Path $session 'procdump-errors.txt')
while(-not $monitor.HasExited) {
    try {
        $process.Refresh()
        if(-not $process.HasExited) {
            [pscustomobject]@{ utc=[DateTime]::UtcNow.ToString('o'); pid=$targetId; privateBytes=$process.PrivateMemorySize64; virtualBytes=$process.VirtualMemorySize64; workingSet=$process.WorkingSet64; handles=$process.HandleCount } | Export-Csv -LiteralPath (Join-Path $session 'memory.csv') -NoTypeInformation -Append
        }
    } catch { $_.Exception.Message | Add-Content (Join-Path $session 'watch-errors.txt') }
    Start-Sleep -Seconds 2
    $monitor.Refresh()
}
& (Join-Path $PSScriptRoot 'collect-support.ps1') -GameDir $GameDir -DataDir $DataDir -OutDir $session -NoOpen
Write-Output "Monitor finished. Exit=$($monitor.ExitCode). Dump: $dumpPath"
