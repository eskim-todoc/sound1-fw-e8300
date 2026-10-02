$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa/unit-reconciliation'
$app=New-Object -ComObject Word.Application
$app.Visible=$false
$app.DisplayAlerts=0
$app.AutomationSecurity=3
try {
 foreach($name in @('SDDD-source','SDDD-before')) {
  $doc=$null
  try {
   $doc=$app.Documents.Open((Join-Path $qa ($name+'.docx')),$false,$true,$false)
   if($name -eq 'SDDD-before') {
    $doc.TrackRevisions=$false
    $doc.AcceptAllRevisions()
    $doc.SaveAs2((Join-Path $qa 'SDDD-before-accepted.docx'),16)
   }
   $doc.ExportAsFixedFormat((Join-Path $qa ($name+'.pdf')),17,$false,0,0,1,1,0)
   Write-Output ($name+': '+$doc.ComputeStatistics(2)+' pages')
  } finally {if($doc){$doc.Close(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}}
 }
} finally {$app.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
