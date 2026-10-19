param([Parameter(Mandatory=$true)][string]$PlanPath)
$ErrorActionPreference='Stop'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath $PlanPath | ConvertFrom-Json
$records=New-Object System.Collections.Generic.List[object]
$app=New-Object -ComObject Excel.Application
$app.Visible=$false;$app.DisplayAlerts=$false;$app.AutomationSecurity=3
try {
 foreach($item in $plan.books) {
  $book=$null;$sheet=$null
  try {
   Write-Output ($item.code+' opening')
   $sourceHash=(Get-FileHash -Algorithm SHA256 -LiteralPath ([string]$item.path)).Hash
   $book=$app.Workbooks.Open([string]$item.path,0,$true)
   $sheet=$book.Worksheets.Item([string]$item.sheet)
   $originalPrintArea=$sheet.PageSetup.PrintArea
   foreach($part in $item.parts) {
    # Only adjust the read-only preview print area; source print settings remain.
    $sheet.PageSetup.PrintArea=[string]$part.range
    $sheet.PageSetup.Zoom=$false
    $sheet.PageSetup.FitToPagesWide=1
    $sheet.PageSetup.FitToPagesTall=$false
    $sheet.ExportAsFixedFormat(0,[string]$part.pdf,0,$true,$false)
    $afterHash=(Get-FileHash -Algorithm SHA256 -LiteralPath ([string]$item.path)).Hash
    if($sourceHash -ne $afterHash){throw ($item.code+' source changed during read-only render')}
    $records.Add([pscustomobject]@{code=$item.code;path=$item.path;sheet=$item.sheet;range=$part.range;pdf=$part.pdf;sourceSHA256=$sourceHash;sourceUnchanged=$true;originalPrintArea=$originalPrintArea;scope='Read-only Excel preview; source print settings preserved';checkedAt=(Get-Date).ToString('o')})
    Write-Output ($item.code+' '+$part.range+' exported')
   }
  } finally {
   if($null -ne $sheet){[void][Runtime.InteropServices.Marshal]::ReleaseComObject($sheet)}
   if($null -ne $book){$book.Close($false);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($book)}
  }
 }
 $records | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath $plan.recordPath
} finally {$app.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
