param([string]$Folder,[string[]]$Kinds=@('candidate'),[switch]$RejectCheck,[switch]$TemplateProbe)
$ErrorActionPreference='Stop'
$app=New-Object -ComObject Word.Application
$app.Visible=$false;$app.DisplayAlerts=0;$app.AutomationSecurity=3
$doc=$null
try {
 if($TemplateProbe){
  $doc=$app.Documents.Add((Join-Path $Folder 'candidate.dotx'),$false,0,$false)
  $styles=@($doc.Styles|Where-Object {$_.NameLocal.StartsWith('TDC ')})
  if($styles.Count -ne 59){throw "Style count $($styles.Count)"}
  $doc.Content.Text='템플릿 독립 저장 확인';$doc.Content.Style=$doc.Styles.Item('TDC 본문 1')
  $doc.SaveAs2((Join-Path $Folder 'native-created.docx'),16);$doc.Close(0);$doc=$null
 }
 foreach($kind in $Kinds){
  $doc=$app.Documents.Open((Join-Path $Folder ($kind+'.docx')),$false,$true,$false)
  $rev=$doc.Revisions.Count;$doc.AcceptAllRevisions();$doc.TrackRevisions=$false
  foreach($toc in $doc.TablesOfContents){$toc.Update()};$doc.Fields.Update()|Out-Null;$doc.Repaginate()
  $records=New-Object System.Collections.Generic.List[object]
  foreach($p in $doc.Paragraphs){
   $txt=$p.Range.Text.Trim([char]13,[char]7)
   if($p.Style.NameLocal -match '^TDC 제목 [1-6]$' -and $txt.Length -gt 0){
    $records.Add(@{text=$txt;number=$p.Range.ListFormat.ListString;style=$p.Style.NameLocal;page=$p.Range.Information(3)})
   }
  }
  $records|ConvertTo-Json -Depth 8|Set-Content -LiteralPath (Join-Path $Folder ($kind+'-headings.json')) -Encoding utf8
  $doc.SaveAs2((Join-Path $Folder ($kind+'-accepted.docx')),16)
  $doc.ExportAsFixedFormat((Join-Path $Folder ($kind+'.pdf')),17,$false,0,0,1,1,0)
  Write-Output ($kind+' pages='+$doc.ComputeStatistics(2)+' revisions='+$rev+' tables='+$doc.Tables.Count+' headings='+$records.Count)
  $doc.Close(0);$doc=$null
 }
 if($RejectCheck){
  foreach($kind in $Kinds){
   $doc=$app.Documents.Open((Join-Path $Folder ($kind+'.docx')),$false,$true,$false)
   $doc.RejectAllRevisions();$doc.TrackRevisions=$false
   $doc.SaveAs2((Join-Path $Folder ($kind+'-rejected.docx')),16);$doc.Close(0);$doc=$null
   Write-Output ($kind+' rejection saved')
  }
 }
}finally{
 if($null -ne $doc){$doc.Close(0)};$app.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
