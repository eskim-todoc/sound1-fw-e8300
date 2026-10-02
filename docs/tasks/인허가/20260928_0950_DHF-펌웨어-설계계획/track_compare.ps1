$ErrorActionPreference='Stop'
$source='G:\공유 드라이브\20_프로젝트별\01_project sound1\04_docs\99_참고용DHF\01_설계 계획\SW\펌웨어'
$dest=Join-Path (Get-Location) 'docs\참고\인허가\사운드1\DHF\01_설계 계획\SW\펌웨어'
$qa=Join-Path (Get-Location) 'tmp\dhf-plans-20260928\tracked'
$null=New-Item -ItemType Directory -Path $qa -Force
$app=New-Object -ComObject Word.Application
$app.Visible=$false
$app.DisplayAlerts=0
$app.AutomationSecurity=3
try {
 foreach($file in Get-ChildItem -LiteralPath $source -Filter '*.docx') {
  $orig=$null; $rev=$null; $diff=$null
  try {
   $final=Join-Path $dest ($file.Name.Replace('TD20B','TD24A'))
   $out=Join-Path $dest ([IO.Path]::GetFileNameWithoutExtension($final)+'_변경추적.docx')
   if(Test-Path -LiteralPath $out){throw "Already exists: $out"}
   $orig=$app.Documents.Open($file.FullName,$false,$true,$false)
   $rev=$app.Documents.Open($final,$false,$true,$false)
   # Native Word comparison produces real revisions, including headers and images.
   $diff=$app.CompareDocuments($orig,$rev,2,1,$false,$true,$true,$true,$true,$true,$true,$true,$false,$true,'김은수',$true)
   $count=$diff.Revisions.Count
   if($count -eq 0){throw 'No revisions produced'}
   $diff.TrackRevisions=$true
   $diff.ShowRevisions=$true
   $diff.SaveAs2($out,16)
   $pdf=Join-Path $qa ([IO.Path]::GetFileNameWithoutExtension($out)+'.pdf')
   # Export with markup (wdExportDocumentWithMarkup = 7).
   $diff.ExportAsFixedFormat($pdf,17,$false,0,0,1,1,7)
   Write-Output ($file.Name+': revisions='+$count+' -> '+$out)
  } finally {
   foreach($doc in @($diff,$rev,$orig)){if($null -ne $doc){$doc.Close(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}}
  }
 }
} finally {$app.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
