# Offline symbol resolution against a hash-verified archived DLL/PDB pair.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Candidate,
      [Parameter(Mandatory)][uint32[]]$Rvas,
      [Parameter(Mandatory)][string]$Out,
      [string]$DbgHelpPath="${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\dbghelp.dll")
$ErrorActionPreference='Stop'
if([IntPtr]::Size -ne 8){throw 'Run with 64-bit PowerShell'}
$dir=(Resolve-Path -LiteralPath $Candidate).Path
$manifest=Get-Content -LiteralPath (Join-Path $dir 'manifest.json') -Raw | ConvertFrom-Json
foreach($pair in @(@('d3d9.dll','dllSHA256'),@('d3d9.pdb','pdbSHA256'))){
 if((Get-FileHash -LiteralPath (Join-Path $dir $pair[0])).Hash -ne $manifest.($pair[1])){throw "Archive hash mismatch: $($pair[0])"}
}
$dbg=(Resolve-Path -LiteralPath $DbgHelpPath).Path.Replace('\','\\')
$source=@'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class DvrSymbols {
 [StructLayout(LayoutKind.Sequential)] struct Line { public uint Size; public IntPtr Key; public uint Number; public IntPtr File; public ulong Address; }
 public class Result { public uint Rva; public string Symbol,File; public uint LineNumber,Size; public ulong Displacement; public int Error; }
 [DllImport("__DBG__",SetLastError=true,CharSet=CharSet.Ansi)] static extern bool SymInitialize(IntPtr h,string path,bool invade);
 [DllImport("__DBG__")] static extern bool SymCleanup(IntPtr h);
 [DllImport("__DBG__")] static extern uint SymSetOptions(uint opts);
 [DllImport("__DBG__",SetLastError=true,CharSet=CharSet.Ansi)] static extern ulong SymLoadModuleEx(IntPtr h,IntPtr file,string image,string module,ulong addr,uint size,IntPtr data,uint flags);
 [DllImport("__DBG__",SetLastError=true)] static extern bool SymFromAddr(IntPtr h,ulong addr,out ulong disp,IntPtr sym);
 [DllImport("__DBG__",SetLastError=true)] static extern bool SymGetLineFromAddr64(IntPtr h,ulong addr,out uint disp,ref Line line);
 public static Result[] Resolve(string dir,uint[] rvas) {
  var handle=new IntPtr(0x125); // Unique symbol-session key; no target process opened.
  // Exact symbols, no prompts/critical-error dialogs, ignore environment path.
  SymSetOptions(0x400u|0x2u|0x10u|0x200u|0x80000u|0x1000u);
  if(!SymInitialize(handle,dir,false))throw new Win32Exception();
  IntPtr buffer=IntPtr.Zero;
  try {
   ulong addr=SymLoadModuleEx(handle,IntPtr.Zero,System.IO.Path.Combine(dir,"d3d9.dll"),"dvr",0x10000000,0,IntPtr.Zero,0);
   if(addr==0)throw new Win32Exception();
   buffer=Marshal.AllocHGlobal(88+1024);
   var output=new Result[rvas.Length];
   for(int i=0;i<rvas.Length;i++) {
    var r=new Result(); r.Rva=rvas[i];
    Marshal.WriteInt32(buffer,0,88); Marshal.WriteInt32(buffer,80,1024);
    ulong displacement;
    if(SymFromAddr(handle,addr+rvas[i],out displacement,buffer)) {
     r.Displacement=displacement; r.Size=(uint)Marshal.ReadInt32(buffer,28);
     r.Symbol=Marshal.PtrToStringAnsi(IntPtr.Add(buffer,84));
     var line=new Line();line.Size=(uint)Marshal.SizeOf(typeof(Line));uint offset;
     if(SymGetLineFromAddr64(handle,addr+rvas[i],out offset,ref line)) {r.File=Marshal.PtrToStringAnsi(line.File);r.LineNumber=line.Number;}
    } else r.Error=Marshal.GetLastWin32Error();
    output[i]=r;
   }
   return output;
  } finally {if(buffer!=IntPtr.Zero)Marshal.FreeHGlobal(buffer);SymCleanup(handle);}
 }
}
'@
Add-Type ($source.Replace('__DBG__',$dbg))
$result=[DvrSymbols]::Resolve($dir,$Rvas)
[pscustomobject]@{Build=$manifest.build;DllSHA256=$manifest.dllSHA256;PdbSHA256=$manifest.pdbSHA256;Limit='Leaf symbol lookup only, not an inclusive stack profile; optimized/inlined code attribution can be incomplete.';Symbols=@($result)} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $Out
$result | Select-Object -First 8 Rva,Symbol,LineNumber,Error | Format-Table -AutoSize
