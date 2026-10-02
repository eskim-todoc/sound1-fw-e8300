$ErrorActionPreference='Stop'
$task=$PSScriptRoot
$source='G:\공유 드라이브\20_프로젝트별\01_project sound1\04_docs\99_참고용DHF\02_설계 입력\SW\펌웨어'
$dest=Join-Path (Get-Location) 'docs\참고\인허가\사운드1\DHF\02_설계 입력\SW\펌웨어'
$qa=Join-Path $task 'qa'
$null=New-Item -ItemType Directory -Path $qa -Force
$app=New-Object -ComObject Word.Application
$app.Visible=$false
$app.DisplayAlerts=0
$app.AutomationSecurity=3
$results=@()
try {
 foreach($file in Get-ChildItem -LiteralPath $source -Filter '*.docx') {
  $orig=$null; $rev=$null; $diff=$null
  try {
   $name=$file.Name.Replace('TD20B','TD24A')
   $final=Join-Path $dest $name
   $orig=$app.Documents.Open($file.FullName,$false,$true,$false)
   $rev=$app.Documents.Open((Join-Path $task ('build\'+$name)),$false,$true,$false)
   $diff=$app.CompareDocuments($orig,$rev,2,1,$false,$true,$true,$true,$true,$true,$true,$true,$false,$true,'김은수',$true)
   $count=$diff.Revisions.Count
   if($count -eq 0){throw 'No revisions produced'}
   $diff.TrackRevisions=$true
   $diff.ShowRevisions=$true
   $diff.SaveAs2($final,16)
   $diff.ExportAsFixedFormat((Join-Path $qa ($name+'.markup.pdf')),17,$false,0,0,1,1,7)
   # Private in-memory acceptance is only for rendering and equivalence validation.
   $diff.AcceptAllRevisions()
   $diff.TrackRevisions=$false
   $null=$diff.Fields.Update()
   foreach($toc in $diff.TablesOfContents){$toc.Update()}
   $diff.Repaginate()
   $diff.ExportAsFixedFormat((Join-Path $qa ($name+'.pdf')),17)
   $diff.SaveAs2((Join-Path $qa ('accepted-'+$name)),16)
   $results+=@{file=$name;revisions=$count;pages=$diff.ComputeStatistics(2)}
   Write-Output ($name+': revisions='+$count+' pages='+$diff.ComputeStatistics(2))
  } finally {
   foreach($doc in @($diff,$rev,$orig)){if($null -ne $doc){$doc.Close(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}}
  }
  $check=$null
  try {
   $check=$app.Documents.Open($final,$false,$true,$false)
   $check.RejectAllRevisions();$check.TrackRevisions=$false
   $check.SaveAs2((Join-Path $qa ('rejected-'+$name)),16)
  } finally {if($null -ne $check){$check.Close(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($check)}}
 }
} finally {$app.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
$results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $qa 'word-results.json') -Encoding UTF8
