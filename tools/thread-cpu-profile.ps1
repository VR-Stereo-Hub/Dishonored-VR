# Read-only process thread CPU accounting; no elevation, suspension or injection.
[CmdletBinding()]
param([Parameter(Mandatory)][int]$TargetProcessId,
      [ValidateRange(1,60)][int]$Seconds=20,
      [Parameter(Mandatory)][string]$Out)
$ErrorActionPreference='Stop'
Add-Type @"
using System;
using System.Runtime.InteropServices;
public static class DvrThreadRead {
 [DllImport("kernel32.dll")] public static extern IntPtr OpenThread(uint access,bool inherit,uint id);
 [DllImport("kernel32.dll")] public static extern bool CloseHandle(IntPtr h);
 [DllImport("kernel32.dll")] public static extern int GetThreadDescription(IntPtr h,out IntPtr desc);
 [DllImport("kernel32.dll")] public static extern IntPtr LocalFree(IntPtr p);
 [DllImport("ntdll.dll")] public static extern int NtQueryInformationThread(IntPtr h,int cls,out IntPtr addr,int size,IntPtr len);
}
"@
function Snapshot {
 $p=Get-Process -Id $TargetProcessId -ErrorAction Stop
 $rows=@{}
 foreach($t in $p.Threads) {
  try { $rows[[string]$t.Id]=[pscustomobject]@{Cpu=$t.TotalProcessorTime.TotalMilliseconds;User=$t.UserProcessorTime.TotalMilliseconds;Kernel=$t.PrivilegedProcessorTime.TotalMilliseconds} } catch {}
 }
 return $rows
}
$p=Get-Process -Id $TargetProcessId
$startIdentity=$p.StartTime.ToUniversalTime().ToString('o')
$modules=@($p.Modules | ForEach-Object {[pscustomobject]@{Name=$_.ModuleName;Base=$_.BaseAddress.ToInt64();Size=$_.ModuleMemorySize}})
$before=Snapshot
$timer=[Diagnostics.Stopwatch]::StartNew()
Start-Sleep -Seconds $Seconds
$after=Snapshot
$elapsed=$timer.Elapsed.TotalSeconds
$p=Get-Process -Id $TargetProcessId
if($p.StartTime.ToUniversalTime().ToString('o') -ne $startIdentity){throw 'Process identity changed'}
$rows=foreach($id in $after.Keys) {
 if(-not $before.ContainsKey($id)){continue}
 $name='';$start='unavailable';$h=[DvrThreadRead]::OpenThread(0x40,$false,[uint32]$id)
 if($h -ne [IntPtr]::Zero) {
  try {
   $desc=[IntPtr]::Zero
   if([DvrThreadRead]::GetThreadDescription($h,[ref]$desc) -eq 0 -and $desc -ne [IntPtr]::Zero){try{$name=[Runtime.InteropServices.Marshal]::PtrToStringUni($desc)}finally{[void][DvrThreadRead]::LocalFree($desc)}}
   $addr=[IntPtr]::Zero
   if([DvrThreadRead]::NtQueryInformationThread($h,9,[ref]$addr,[IntPtr]::Size,[IntPtr]::Zero) -eq 0){
    $v=$addr.ToInt64();$start=('0x{0:X}' -f $v)
    foreach($m in $modules){if($v -ge $m.Base -and $v -lt ($m.Base+$m.Size)){$start=('{0}+0x{1:X}' -f $m.Name,($v-$m.Base));break}}
   }
  } finally {[void][DvrThreadRead]::CloseHandle($h)}
 }
 $cpu=$after[$id].Cpu-$before[$id].Cpu
 [pscustomobject]@{ThreadId=$id;Description=$name;StartAddress=$start;CpuMs=$cpu;UserMs=$after[$id].User-$before[$id].User;KernelMs=$after[$id].Kernel-$before[$id].Kernel;OneCorePercent=100*$cpu/($elapsed*1000)}
}
$result=[pscustomobject]@{ProcessId=$TargetProcessId;ProcessStartUtc=$startIdentity;Seconds=$elapsed;CapturedUtc=[DateTime]::UtcNow.ToString('o');Limit='Thread start address is not a sampled executing stack; CPU time does not explain blocked time';Threads=@($rows|Sort-Object CpuMs -Descending)}
$result | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Out
$result.Threads | Select-Object -First 8 | Format-Table -AutoSize
