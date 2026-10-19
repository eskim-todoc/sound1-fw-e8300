$ErrorActionPreference='Stop'
$qaPath=Join-Path $PSScriptRoot 'qa/sad-qcc-cleanup-20261016'
$app=New-Object -ComObject Word.Application
$app.Visible=$false
$app.DisplayAlerts=0
$app.AutomationSecurity=3
try {
 foreach($stem in @('candidate','reject-new','before')) {
  $doc=$null
  try {
   Write-Output "Opening $stem"
   $doc=$app.Documents.Open((Join-Path $qaPath "$stem.docx"),$false,$true,$false)
   $doc.TrackRevisions=$false
   # Acceptance is preview-only, never saved to the reviewed deliverable.
   $doc.Revisions.AcceptAll()
   $doc.Repaginate()
   [void]$doc.Fields.Update()
   foreach($toc in $doc.TablesOfContents){$toc.Update()}
   $doc.Repaginate()
   [IO.File]::WriteAllText((Join-Path $qaPath "$stem-word.txt"),[string]$doc.Content.Text,[Text.UTF8Encoding]::new($false))
   if($stem -eq 'candidate') {
    $doc.ExportAsFixedFormat((Join-Path $qaPath 'candidate.pdf'),17)
    Write-Output ('Candidate pages: '+$doc.ComputeStatistics(2))
   }
  } finally {if($null -ne $doc){$doc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}}
 }
} finally {$app.Quit(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
