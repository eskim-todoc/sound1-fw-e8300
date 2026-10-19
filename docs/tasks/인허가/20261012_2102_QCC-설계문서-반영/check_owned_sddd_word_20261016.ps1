$ErrorActionPreference='Stop'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public class SdddRm {
 [StructLayout(LayoutKind.Sequential)] public struct UP { public int pid; public System.Runtime.InteropServices.ComTypes.FILETIME time; }
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] public struct PI { public UP process; [MarshalAs(UnmanagedType.ByValTStr,SizeConst=256)]public string app; [MarshalAs(UnmanagedType.ByValTStr,SizeConst=64)]public string svc; public uint type; public uint status; public uint session; [MarshalAs(UnmanagedType.Bool)]public bool restartable; }
 [DllImport("rstrtmgr.dll",CharSet=CharSet.Unicode)]public static extern int RmStartSession(out uint handle,int flags,string key);
 [DllImport("rstrtmgr.dll",CharSet=CharSet.Unicode)]public static extern int RmRegisterResources(uint handle,uint nf,string[] files,uint np,UP[] apps,uint ns,string[] services);
 [DllImport("rstrtmgr.dll")]public static extern int RmGetList(uint handle,out uint need,ref uint count,[In,Out]PI[] info,ref uint reasons);
 [DllImport("rstrtmgr.dll")]public static extern int RmEndSession(uint handle);
}
'@
$qaFile=Join-Path $PSScriptRoot 'qa/sddd-qcc-cleanup-20261016/candidate.docx'
[uint32]$sessionHandle=0
[uint32]$need=0
[uint32]$count=0
[uint32]$reason=0
if([SdddRm]::RmStartSession([ref]$sessionHandle,0,[Guid]::NewGuid().ToString('N')) -ne 0){throw 'RM start failed'}
try {
 if([SdddRm]::RmRegisterResources($sessionHandle,1,@($qaFile),0,$null,0,$null) -ne 0){throw 'RM register failed'}
 $ret=[SdddRm]::RmGetList($sessionHandle,[ref]$need,[ref]$count,$null,[ref]$reason)
 $count=$need
 $items=[SdddRm+PI[]]::new($count)
 $ret=[SdddRm]::RmGetList($sessionHandle,[ref]$need,[ref]$count,$items,[ref]$reason)
 if($ret -ne 0){throw "RM list $ret"}
 $result=@($items | ForEach-Object {[pscustomobject]@{pid=$_.process.pid;app=$_.app;file=$qaFile}})
 $result | ConvertTo-Json
 # Only stop this task's stranded, hidden, read-only validation instance.
 $owned=$items | Where-Object {$_.process.pid -eq 4444}
 if($owned) {
  $proc=Get-Process -Id 4444
  if($proc.ProcessName -eq 'WINWORD' -and $proc.StartTime.ToString('yyyy-MM-dd HH:mm:ss') -eq '2026-10-16 16:43:37' -and !$proc.MainWindowTitle){
   Stop-Process -Id 4444
   Write-Output 'Stopped owned read-only QA Word PID 4444; no G deliverable was open in this validation script.'
  } else {throw 'Ownership changed; did not stop process'}
 }
} finally {[void][SdddRm]::RmEndSession($sessionHandle)}
