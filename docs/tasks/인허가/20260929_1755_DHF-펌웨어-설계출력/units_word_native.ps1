param([ValidateSet('Draft','Publish')][string]$Mode='Draft')
$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa\unit-reconciliation'
$candidate=Join-Path $qa 'SDDD-candidate.docx'
$out=Join-Path (Get-Location) 'docs\참고\인허가\사운드1\DHF\03_설계 출력\SW\펌웨어\SDDD-FW-TD24A 소프트웨어 상세 설계서.docx'
$app=New-Object -ComObject Word.Application
$app.Visible=$false;$app.DisplayAlerts=0;$app.AutomationSecurity=3
$orig=$null;$rev=$null;$diff=$null
try {
 $rev=$app.Documents.Open($candidate,$false,$false,$false)
 foreach($toc in $rev.TablesOfContents){$toc.Update()}
 $rev.Fields.Update()|Out-Null
 $rev.Save()
 $rev.ExportAsFixedFormat((Join-Path $qa 'SDDD-candidate.pdf'),17,$false,0,0,1,1,0)
 Write-Output ('Candidate pages: '+$rev.ComputeStatistics(2))
 if($Mode -eq 'Publish'){
  # Do not replace externally modified documents.
  $allowedHash=@((Get-FileHash -LiteralPath (Join-Path $qa 'SDDD-before.docx')).Hash)
  $verifiedPath=Join-Path $qa 'final-checks.json'
  if(Test-Path -LiteralPath $verifiedPath){
   $verified=Get-Content -Raw -Encoding UTF8 -LiteralPath $verifiedPath | ConvertFrom-Json
   $allowedHash+= $verified.sha256.SDDD
  }
  $readabilityChecks=Join-Path $PSScriptRoot 'qa\design-readability\final-checks.json'
  if(Test-Path -LiteralPath $readabilityChecks){
   $readability=Get-Content -Raw -Encoding UTF8 -LiteralPath $readabilityChecks | ConvertFrom-Json
   $allowedHash+= $readability.sha256.SDDD
  }
  if((Get-FileHash -LiteralPath $out).Hash -notin $allowedHash){throw 'Current SDDD differs from the preserved baseline and last verified publication.'}
  $orig=$app.Documents.Open((Join-Path $qa 'SDDD-source.docx'),$false,$true,$false)
  $diff=$app.CompareDocuments($orig,$rev,2,1,$true,$true,$true,$true,$true,$true,$true,$true,$false,$true,'김은수',$true)
  $diff.TrackRevisions=$true;$diff.ShowRevisions=$true
  foreach($toc in $diff.TablesOfContents){$toc.Update()}
  $diff.Fields.Update()|Out-Null
  $diff.SaveAs2($out,16)
  Write-Output ('Saved revisions: '+$diff.Revisions.Count)
  $diff.AcceptAllRevisions()
  foreach($toc in $diff.TablesOfContents){$toc.Update()}
  $diff.Fields.Update()|Out-Null
  $diff.SaveAs2((Join-Path $qa 'SDDD-final-accepted.docx'),16)
  $diff.ExportAsFixedFormat((Join-Path $qa 'SDDD-final.pdf'),17,$false,0,0,1,1,0)
  Write-Output ('Final accepted pages: '+$diff.ComputeStatistics(2))
  $diff.Close(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($diff);$diff=$null
  # Reject only a QA copy. The published file retains every tracked change.
  $diff=$app.Documents.Open($out,$false,$true,$false)
  $diff.RejectAllRevisions()
  $diff.TrackRevisions=$false
  $diff.SaveAs2((Join-Path $qa 'SDDD-rejected-qa.docx'),16)
 }
} finally {
 foreach($doc in @($diff,$rev,$orig)){if($null -ne $doc){$doc.Close(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}}
 $app.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
