# Sample the game's 3D engine utilisation once a second, stamped with the same clock the mod's
# log uses (GetTickCount), so each perf: tick window can be read against the GPU's busy share.
param([int]$Seconds = 40, [string]$Out = "D:\dvr-data\logs\vr160-gpu.csv")
$gpid = (Get-Process Dishonored).Id
$rows = @()
for ($i = 0; $i -lt $Seconds; $i++) {
    $s = Get-Counter -Counter "\GPU Engine(pid_${gpid}_*engtype_3D)\Utilization Percentage" -SampleInterval 1 -MaxSamples 1
    $v = ($s.CounterSamples | Measure-Object CookedValue -Sum).Sum
    $tick = [uint32]([Environment]::TickCount -band 0x7fffffff)
    $rows += "{0},{1:N1}" -f $tick, $v
}
$rows | Set-Content -Path $Out -Encoding ascii
"wrote $($rows.Count) samples to $Out"
