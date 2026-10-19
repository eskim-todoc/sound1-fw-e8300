param([Parameter(Mandatory=$true)][string]$PlanPath)
$ErrorActionPreference='Stop'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath $PlanPath | ConvertFrom-Json
$records=New-Object System.Collections.Generic.List[object]
function Get-ProseRevisionSignature($document) {
 $parts=New-Object System.Collections.Generic.List[string]
 $tocCount=0
 for($revisionIndex=1;$revisionIndex -le $document.Revisions.Count;$revisionIndex++) {
  $revision=$document.Revisions.Item($revisionIndex)
  $revisionRange=$revision.Range
  if($null -eq $revisionRange){throw ('Revision range unavailable at '+$revisionIndex+' type='+$revision.Type+' author='+$revision.Author)}
  $insideToc=$false
  foreach($toc in $document.TablesOfContents){if($revisionRange.InRange($toc.Range)){$insideToc=$true;break}}
  if($insideToc){$tocCount++;continue}
  $date=$revision.Date
  if($null -eq $date){throw ('Revision date unavailable at '+$revisionIndex+' type='+$revision.Type+' author='+$revision.Author)}
  $parts.Add(([string]$revision.Type)+'|'+$revision.Author+'|'+$date.ToString('o')+'|'+[string]$revisionRange.Text)
 }
 return [pscustomobject]@{parts=@($parts|Sort-Object);outsideCount=$parts.Count;tocCount=$tocCount}
}
$app=New-Object -ComObject Word.Application
$app.Visible=$false;$app.DisplayAlerts=0;$app.AutomationSecurity=3
try {
 foreach($item in $plan.docs) {
  $doc=$null
  try {
   if([string]$item.path -notmatch '\[E8300-QCC\].+\.docx$'){throw 'Only the authorized QCC copies may be saved.'}
   $backup=Join-Path $plan.backupDir ($item.code+'.docx')
   if(-not(Test-Path -LiteralPath $backup)){Copy-Item -LiteralPath $item.path -Destination $backup}
   Write-Output ($item.code+' updating fields')
   $doc=$app.Documents.Open([string]$item.path,$false,$false,$false)
   $countBefore=$doc.Revisions.Count
   $signatureBefore=Get-ProseRevisionSignature $doc
   $trackBefore=$doc.TrackRevisions
   # Refresh calculated field results without adding prose revisions.
   # No revision is accepted/rejected in the saved document.
   $doc.TrackRevisions=$false
   $doc.ShowRevisions=$false
   $doc.ActiveWindow.View.ShowRevisionsAndComments=$false
   $doc.ActiveWindow.View.RevisionsFilter.View=0
   $doc.Repaginate()
   foreach($toc in $doc.TablesOfContents){$toc.Update()}
   [void]$doc.Fields.Update()
   foreach($story in $doc.StoryRanges){[void]$story.Fields.Update()}
   $doc.Repaginate()
   foreach($toc in $doc.TablesOfContents){$toc.UpdatePageNumbers()}
   $doc.TrackRevisions=$trackBefore
   $countAfter=$doc.Revisions.Count
   $signatureAfter=Get-ProseRevisionSignature $doc
   $delta=Compare-Object -ReferenceObject $signatureBefore.parts -DifferenceObject $signatureAfter.parts
   if($null -ne $delta){throw ($item.code+' revision content outside generated TOC changed; no save')}
   $doc.Save()
   $records.Add([pscustomobject]@{code=$item.code;path=$item.path;backup=$backup;revisionsBefore=$countBefore;revisionsAfter=$countAfter;proseRevisionsBefore=$signatureBefore.outsideCount;proseRevisionsAfter=$signatureAfter.outsideCount;proseRevisionSignaturesEqual=$true;tocRevisionsBefore=$signatureBefore.tocCount;tocRevisionsAfter=$signatureAfter.tocCount;trackRestored=$trackBefore;tocCount=$doc.TablesOfContents.Count;pages=$doc.ComputeStatistics(2)})
   $records|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 -LiteralPath $plan.fieldRecordPath
   Write-Output ($item.code+' saved fields, revisions='+$countAfter)
  } finally {if($null -ne $doc){$doc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}}
 }
 $records|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 -LiteralPath $plan.fieldRecordPath
} finally {$app.Quit(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
