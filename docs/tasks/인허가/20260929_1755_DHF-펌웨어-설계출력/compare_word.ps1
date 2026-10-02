param([string]$Only='*',[string]$Iteration='1')
$ErrorActionPreference='Stop'
$task=$PSScriptRoot
$source='G:\공유 드라이브\20_프로젝트별\01_project sound1\04_docs\99_참고용DHF\03_설계 출력\SW\펌웨어'
$dest=Join-Path (Get-Location) 'docs\참고\인허가\사운드1\DHF\03_설계 출력\SW\펌웨어'
$qa=Join-Path $task 'qa\final'
$null=New-Item -ItemType Directory -Path $qa -Force
$app=New-Object -ComObject Word.Application
$app.Visible=$false;$app.DisplayAlerts=0;$app.AutomationSecurity=3
try {
 foreach($file in Get-ChildItem -LiteralPath (Join-Path $task 'revised') -Filter "$Only.docx") {
  $orig=$null;$rev=$null;$diff=$null
  try {
   $out=Join-Path $dest $file.Name
   if(Test-Path -LiteralPath $out){
    $backup=Join-Path $task ('qa\previous-'+$Iteration+'-'+$file.Name)
    if(Test-Path -LiteralPath $backup){throw 'Previous candidate already backed up; inspect before another replacement.'}
    Copy-Item -LiteralPath $out -Destination $backup
   }
   $orig=$app.Documents.Open((Join-Path $source $file.Name.Replace('TD24A','TD20B')),$false,$true,$false)
   $rev=$app.Documents.Open($file.FullName,$false,$true,$false)
   $diff=$app.CompareDocuments($orig,$rev,2,1,$false,$true,$true,$true,$true,$true,$true,$true,$false,$true,'김은수',$true)
   $diff.TrackRevisions=$true;$diff.ShowRevisions=$true
   foreach($toc in $diff.TablesOfContents){$toc.Update()}
   $diff.Fields.Update() | Out-Null
   $diff.SaveAs2($out,16)
   $count=$diff.Revisions.Count
   $diff.ExportAsFixedFormat((Join-Path $qa ($file.BaseName+'-markup.pdf')),17,$false,0,0,1,1,7)
   # Accept only in the unsaved QA instance; saved deliverable retains revisions.
   $diff.AcceptAllRevisions()
   foreach($toc in $diff.TablesOfContents){$toc.Update()}
   $diff.Fields.Update() | Out-Null
   $diff.ExportAsFixedFormat((Join-Path $qa ($file.BaseName+'.pdf')),17,$false,0,0,1,1,0)
   Write-Output ($file.Name+': revisions='+$count+', accepted pages='+$diff.ComputeStatistics(2))
  } finally {foreach($doc in @($diff,$rev,$orig)){if($null -ne $doc){$doc.Close(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}}}
 }
} finally {$app.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
