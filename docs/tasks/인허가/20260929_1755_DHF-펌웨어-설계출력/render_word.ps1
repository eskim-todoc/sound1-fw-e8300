param([string]$Directory,[string]$OutDir)
$ErrorActionPreference='Stop'
$null=New-Item -ItemType Directory -Path $OutDir -Force
$app=New-Object -ComObject Word.Application
$app.Visible=$false
$app.DisplayAlerts=0
$app.AutomationSecurity=3
try {
 foreach($f in Get-ChildItem -LiteralPath $Directory -Filter '*.docx') {
  if($f.Name.StartsWith('~')){continue}
  $doc=$null
  try {
   $doc=$app.Documents.Open($f.FullName,$false,$true,$false)
   $pdf=Join-Path $OutDir ($f.BaseName+'.pdf')
   $doc.ExportAsFixedFormat($pdf,17,$false,0,0,1,1,0)
   Write-Output ($f.Name+': pages='+$doc.ComputeStatistics(2))
  } finally {if($null -ne $doc){$doc.Close(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}}
 }
} finally {$app.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
