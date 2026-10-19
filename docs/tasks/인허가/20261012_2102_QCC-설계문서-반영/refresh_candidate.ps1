param([Parameter(Mandatory=$true)][string]$Source,[Parameter(Mandatory=$true)][string]$Candidate,[switch]$KeepTocEntries)
$ErrorActionPreference='Stop'
if($Source -notmatch '\[E8300-QCC\].+\.docx$'){throw 'QCC copy required'}
Copy-Item -LiteralPath $Source -Destination $Candidate
$app=New-Object -ComObject Word.Application
$app.Visible=$false;$app.DisplayAlerts=0;$app.AutomationSecurity=3
$doc=$null
try {
 $doc=$app.Documents.Open($Candidate,$false,$false,$false)
 $track=$doc.TrackRevisions
 $before=$doc.Revisions.Count
 $doc.TrackRevisions=$false
 $doc.ShowRevisions=$false
 $doc.ActiveWindow.View.ShowRevisionsAndComments=$false
 $doc.ActiveWindow.View.RevisionsFilter.View=0
 $doc.Repaginate()
 foreach($toc in $doc.TablesOfContents){if($KeepTocEntries){$toc.UpdatePageNumbers()}else{$toc.Update()}}
 if($KeepTocEntries){
  # Titles/entries are unchanged. Preserve the inspected TOC paragraph layout.
  foreach($field in $doc.Fields){if($field.Code.Text -notmatch '^\s*TOC\s'){[void]$field.Update()}}
  foreach($story in $doc.StoryRanges){foreach($field in $story.Fields){if($field.Code.Text -notmatch '^\s*TOC\s'){[void]$field.Update()}}}
 }else{
  [void]$doc.Fields.Update()
  foreach($story in $doc.StoryRanges){[void]$story.Fields.Update()}
 }
 $doc.Repaginate()
 foreach($toc in $doc.TablesOfContents){$toc.UpdatePageNumbers()}
 $doc.TrackRevisions=$track
 $doc.Save()
 [pscustomobject]@{source=$Source;candidate=$Candidate;revisionsBefore=$before;revisionsAfter=$doc.Revisions.Count;trackRestored=$track;pages=$doc.ComputeStatistics(2);tocEntriesPreserved=[bool]$KeepTocEntries;scope='QA copy only; no revisions accepted or rejected'} | ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 -LiteralPath ($Candidate+'.fields.json')
} finally {
 if($null -ne $doc){$doc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}
 $app.Quit(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
