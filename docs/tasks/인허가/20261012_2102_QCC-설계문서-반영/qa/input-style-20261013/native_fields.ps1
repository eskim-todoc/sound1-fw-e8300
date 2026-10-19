param([string]$QaRoot,[string]$OnlyCode='')
$ErrorActionPreference='Stop'
$fieldPlan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $QaRoot 'authored.json') | ConvertFrom-Json
$fieldWord=New-Object -ComObject Word.Application
$fieldWord.Visible=$false;$fieldWord.DisplayAlerts=0;$fieldWord.AutomationSecurity=3
$fieldUser=@{name=$fieldWord.UserName;initials=$fieldWord.UserInitials};$records=@()
try{
 foreach($item in $fieldPlan.docs){
  if($OnlyCode -and $item.code -ne $OnlyCode){continue}
  $fieldPath=Join-Path $QaRoot ($item.code+'-fields.docx')
  Copy-Item -LiteralPath $item.candidate -Destination $fieldPath
  $d=$fieldWord.Documents.Open([string]$fieldPath,$false,$false)
  try{
   $track=$d.TrackRevisions;$before=$d.Revisions.Count
   $d.TrackRevisions=$false;$d.ShowRevisions=$false
   $d.ActiveWindow.View.ShowRevisionsAndComments=$false;$d.ActiveWindow.View.RevisionsFilter.View=0
   $d.Repaginate()
   foreach($toc in $d.TablesOfContents){$toc.Update()}
   [void]$d.Fields.Update()
   foreach($story in $d.StoryRanges){[void]$story.Fields.Update()}
   $d.Repaginate();foreach($toc in $d.TablesOfContents){$toc.UpdatePageNumbers()}
   $d.TrackRevisions=$track;$d.Save()
   $records+=@{code=$item.code;candidate=$fieldPath;beforeRevisions=$before;afterRevisions=$d.Revisions.Count;pages=$d.ComputeStatistics(2);allRevisionsAccepted=$false;userUnchanged=$true}
   Write-Output ($item.code+' fields updated, revisions '+$before+' -> '+$d.Revisions.Count)
  }finally{$d.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($d)}
 }
 if($fieldWord.UserName -ne $fieldUser.name -or $fieldWord.UserInitials -ne $fieldUser.initials){throw 'Global settings changed'}
 ConvertTo-Json -InputObject @($records) -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $QaRoot 'native-fields.json')
}finally{$fieldWord.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($fieldWord)}
