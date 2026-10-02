param([string]$Folder,[string[]]$Kinds=@('before','candidate'))
$ErrorActionPreference='Stop'
$app=New-Object -ComObject Word.Application
$app.Visible=$false
$app.DisplayAlerts=0
$app.AutomationSecurity=3
$doc=$null
try {
 foreach($kind in $Kinds){
  $path=Join-Path $Folder ($kind+'.docx')
  $doc=$app.Documents.Open($path,$false,$true,$false)
  $revisions=$doc.Revisions.Count
  $doc.AcceptAllRevisions()
  $doc.TrackRevisions=$false
  foreach($toc in $doc.TablesOfContents){$toc.Update()}
  $doc.Fields.Update()|Out-Null
  $doc.Repaginate()
  $records=New-Object System.Collections.Generic.List[object]
  foreach($p in $doc.Paragraphs){
   if($p.OutlineLevel -le 6 -and $p.Range.Text.Trim().Length -gt 0){
    $records.Add(@{text=$p.Range.Text.Trim([char]13,[char]7);number=$p.Range.ListFormat.ListString;level=$p.OutlineLevel;page=$p.Range.Information(3)})
   }
  }
  $records|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $Folder ($kind+'-headings.json')) -Encoding utf8
  $doc.SaveAs2((Join-Path $Folder ($kind+'-accepted.docx')),16)
  $doc.ExportAsFixedFormat((Join-Path $Folder ($kind+'.pdf')),17,$false,0,0,1,1,0)
  Write-Output ($kind+' pages='+$doc.ComputeStatistics(2)+' revisions='+$revisions+' headings='+$records.Count)
  $doc.Close(0)
  $doc=$app.Documents.Open($path,$false,$true,$false)
  $doc.RejectAllRevisions()
  $doc.TrackRevisions=$false
  $doc.SaveAs2((Join-Path $Folder ($kind+'-rejected.docx')),16)
  $doc.Close(0)
  $doc=$null
 }
} finally {
 if($null -ne $doc){$doc.Close(0)}
 $app.Quit(0)
 $null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
