param([string]$Target, [string]$Evidence)
$ErrorActionPreference='Stop'
$qccProbeExcel=$null
$qccProbeBook=$null
$qccProbeStatus=[ordered]@{path=$Target; opened=$false; readOnly=$true; repaired=$false; error=$null}
try {
    $qccProbeExcel=New-Object -ComObject Excel.Application
    $qccProbeExcel.Visible=$false
    $qccProbeExcel.DisplayAlerts=$false
    $qccProbeExcel.AutomationSecurity=3
    $qccProbeBook=$qccProbeExcel.Workbooks.Open($Target,0,$true)
    $qccProbeStatus.opened=$true
    $qccProbeStatus.sheets=@($qccProbeBook.Worksheets | ForEach-Object {$_.Name})
    $qccProbeStatus.auxMerge=$qccProbeBook.Worksheets.Item('시험 보조자료').Range('A2').MergeArea.Address()
    $qccProbeStatus.auxHeaderHeight=$qccProbeBook.Worksheets.Item('시험 보조자료').Rows.Item(2).RowHeight
    $qccProbeStatus.auxRow190Height=$qccProbeBook.Worksheets.Item('시험 보조자료').Rows.Item(190).RowHeight
} catch {$qccProbeStatus.error=$_.Exception.Message}
finally {
    if($null-ne$qccProbeBook){$qccProbeBook.Close($false);[void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($qccProbeBook)}
    if($null-ne$qccProbeExcel){$qccProbeExcel.Quit();[void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($qccProbeExcel)}
    [GC]::Collect();[GC]::WaitForPendingFinalizers()
}
$qccProbeStatus | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath $Evidence
$qccProbeStatus | ConvertTo-Json -Depth 8 -EscapeHandling EscapeNonAscii
if(-not$qccProbeStatus.opened){exit 1}
