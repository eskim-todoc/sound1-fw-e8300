$ErrorActionPreference='Stop'
$folder=Join-Path (Get-Location) 'tmp/scmp-qcc-final'
$null=New-Item -ItemType Directory -Path $folder -Force
$word=New-Object -ComObject Word.Application
$word.Visible=$false
$word.DisplayAlerts=0
$word.AutomationSecurity=3
try {
 $file=Join-Path (Get-Location) 'docs/참고/인허가/사운드1/DHF/01_설계 계획/SW/펌웨어/SCMP-FW-TD24A 소프트웨어 형상관리 계획서.docx'
 $doc=$word.Documents.Open($file,$false,$true,$false)
 $doc.ExportAsFixedFormat((Join-Path $folder 'SCMP-final.pdf'),17)
 Write-Output ('Revisions: '+$doc.Revisions.Count)
 $doc.Close(0)
} finally {$word.Quit(0)}
