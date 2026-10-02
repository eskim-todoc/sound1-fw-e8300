$ErrorActionPreference='Stop'
$target=Join-Path (Get-Location) 'docs\참고\인허가\사운드1\DHF\02_설계 입력\SW\펌웨어\SR-FW-TD24A 소프트웨어 요구사항.docx'
$qa=Join-Path $PSScriptRoot 'qa-20260929'
$null=New-Item -ItemType Directory -Path $qa -Force
$backup=Join-Path $qa 'before.docx'
if(Test-Path -LiteralPath $backup){throw 'Backup already exists'}
Copy-Item -LiteralPath $target -Destination $backup
$baseline=(Get-FileHash -LiteralPath $target).Hash
$app=New-Object -ComObject Word.Application
$app.Visible=$false;$app.DisplayAlerts=0;$app.AutomationSecurity=3
$doc=$null
try {
 $doc=$app.Documents.Open($backup,$false,$false,$false)
 $start=$null
 foreach($p in $doc.Paragraphs){
  if($p.Range.Text.Trim() -eq '소프트웨어 요구사항 검증'){$start=$p.Range.Start;break}
 }
 if($null -eq $start){throw 'Section 5 heading not found'}
 $scope=$doc.Range($start,$doc.Content.End)
 $count=$scope.Revisions.Count
 if($count -eq 0){throw 'Expected source-restoration revisions not found'}
 $scope.Revisions.RejectAll()
 $doc.TrackRevisions=$true
 $doc.ShowRevisions=$true
 $candidate=Join-Path $qa 'updated.docx'
 $doc.SaveAs2($candidate,16)
 $doc.ExportAsFixedFormat((Join-Path $qa 'markup.pdf'),17,$false,0,0,1,1,7)
 $doc.AcceptAllRevisions();$doc.TrackRevisions=$false
 $null=$doc.Fields.Update();foreach($toc in $doc.TablesOfContents){$toc.Update()}
 $doc.Repaginate()
 $doc.ExportAsFixedFormat((Join-Path $qa 'accepted.pdf'),17)
 Write-Output ('Restored section revisions: '+$count)
} finally {
 if($null -ne $doc){$doc.Close(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}
 $app.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
if((Get-FileHash -LiteralPath $target).Hash -ne $baseline){throw 'Target changed during edit'}
Write-Output 'Candidate ready for verification; final file not overwritten yet.'
