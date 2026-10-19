param([string]$PlanPath,[string]$RecordPath)
$ErrorActionPreference='Stop'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath $PlanPath | ConvertFrom-Json
$records=New-Object System.Collections.Generic.List[object]
$app=New-Object -ComObject Excel.Application
$app.Visible=$false
$app.DisplayAlerts=$false
$app.AutomationSecurity=3
try {
 foreach($item in $plan){
  $book=$null
  $row=[ordered]@{path=$item.path;mode=$item.mode;opened=$false;readOnly=$true;error=$null}
  try{$book=$app.Workbooks.Open([string]$item.path,0,$true);$row.opened=$true;$row.sheets=@($book.Worksheets | ForEach-Object {$_.Name})}
  catch{$row.error=$_.Exception.Message}
  finally{if($null-ne$book){$book.Close($false);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($book)}}
  $records.Add([pscustomobject]$row)
 }
}finally{$app.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($app);[GC]::Collect();[GC]::WaitForPendingFinalizers()}
$records | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath $RecordPath
$records | ConvertTo-Json -Depth 6 -EscapeHandling EscapeNonAscii
