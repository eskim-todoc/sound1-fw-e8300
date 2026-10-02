param([switch]$BaselineOnly,[switch]$CandidateOnly,[switch]$RejectCheck)
$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa\design-readability\common-style'
$app=New-Object -ComObject Word.Application
$app.Visible=$false; $app.DisplayAlerts=0; $app.AutomationSecurity=3
$doc=$null
try {
 foreach($kind in @('before','candidate')) {
  if($BaselineOnly -and $kind -ne 'before'){continue}
  if($CandidateOnly -and $kind -ne 'candidate'){continue}
  $doc=$app.Documents.Open((Join-Path $qa ($kind+'.docx')),$false,$true,$false)
  $count=$doc.Revisions.Count
  $doc.AcceptAllRevisions();$doc.TrackRevisions=$false
  $records=New-Object System.Collections.Generic.List[object]
  foreach($p in $doc.Paragraphs) {
   $txt=$p.Range.Text.Trim([char]13,[char]7)
   $style=$p.Style.NameLocal
   if(($style -match '^Heading [1-6]$|^제목 [1-6]$|^TDC 제목 [1-6]$') -and $txt.Length -gt 0){
    $records.Add(@{text=$txt;number=$p.Range.ListFormat.ListString;style=$style;outline=$p.OutlineLevel;size=$p.Range.Font.Size})
   }
  }
  $records | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $qa ($kind+'-headings.json')) -Encoding utf8
  foreach($toc in $doc.TablesOfContents){$toc.Update()}
  $doc.Fields.Update()|Out-Null
  $doc.Repaginate()
  $doc.SaveAs2((Join-Path $qa ($kind+'-accepted.docx')),16)
  $doc.ExportAsFixedFormat((Join-Path $qa ($kind+'.pdf')),17,$false,0,0,1,1,0)
  Write-Output ($kind+' pages='+$doc.ComputeStatistics(2)+' revisions_before='+$count+' headings='+$records.Count)
  $doc.Close(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($doc);$doc=$null
 }
 if($RejectCheck){
  foreach($kind in @('before','candidate')){
   $doc=$app.Documents.Open((Join-Path $qa ($kind+'.docx')),$false,$true,$false)
   $doc.RejectAllRevisions();$doc.TrackRevisions=$false
   $doc.SaveAs2((Join-Path $qa ($kind+'-rejected.docx')),16)
   $doc.Close(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($doc);$doc=$null
  }
 }
} finally {
 if($null -ne $doc){$doc.Close(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}
 $app.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
