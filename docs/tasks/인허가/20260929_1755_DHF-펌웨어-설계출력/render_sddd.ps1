$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa/sddd-architecture'
$path=Join-Path (Get-Location) 'docs/참고/인허가/사운드1/DHF/03_설계 출력/SW/펌웨어/SDDD-FW-TD24A 소프트웨어 상세 설계서.docx'
$app=New-Object -ComObject Word.Application
$app.Visible=$false
$app.DisplayAlerts=0
$app.AutomationSecurity=3
$doc=$null
try {
 $doc=$app.Documents.Open($path,$false,$true,$false)
 $doc.TrackRevisions=$false
 $doc.AcceptAllRevisions()
 $null=$doc.Fields.Update()
 foreach($toc in $doc.TablesOfContents){$toc.Update()}
 $doc.Repaginate()
 $doc.ExportAsFixedFormat((Join-Path $qa 'SDDD-final-view.pdf'),17,$false,0,0,1,1,0)
 Write-Output ('Final-view pages: '+$doc.ComputeStatistics(2))
} finally {
 if($null -ne $doc){$doc.Close(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}
 $app.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
