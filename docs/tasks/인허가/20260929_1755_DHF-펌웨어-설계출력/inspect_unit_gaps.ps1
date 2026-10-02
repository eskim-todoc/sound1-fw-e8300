param([string]$Folder)
$ErrorActionPreference='Stop'
$app=New-Object -ComObject Word.Application
$app.Visible=$false;$app.DisplayAlerts=0;$app.AutomationSecurity=3
$doc=$null
try {
 $doc=$app.Documents.Open((Join-Path $Folder 'candidate-accepted.docx'),$false,$true,$false)
 $doc.Repaginate()
 $items=New-Object System.Collections.Generic.List[object]
 # Find the already confirmed headings, instead of paginating every table cell.
 $headings=Get-Content -Raw -Encoding utf8 -LiteralPath (Join-Path $Folder 'candidate-headings.json')|ConvertFrom-Json
 foreach($head in ($headings|Where-Object {$_.text.StartsWith('SU-ID-')})){
  $range=$doc.Content.Duplicate;$range.Find.ClearFormatting()
  if(-not $range.Find.Execute($head.text)){throw "Unit not found: $($head.text)"}
  $p=$range.Paragraphs.Item(1);$text=$p.Range.Text.Trim([char]13,[char]7)
  $prev=$p.Previous()
  $prevText=$prev.Range.Text.Trim([char]13,[char]7)
  $prevPage=$prev.Range.Information(3);$prevTop=$prev.Range.Information(6)
  $prevStart=$prev.Range.Start;$prevStyle=$prev.Style.NameLocal
  $page=$p.Range.Information(3);$top=$p.Range.Information(6)
  if($text.StartsWith('SU-ID-')){
   $items.Add(@{unit=$text;page=$page;top=$top;start=$p.Range.Start;
     prevText=$prevText;prevPage=$prevPage;prevTop=$prevTop;prevStart=$prevStart;prevStyle=$prevStyle;
     pageBreakBefore=$p.Format.PageBreakBefore})
  }
 }
 $items|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $Folder 'unit-gap-layout.json') -Encoding utf8
 $top=($items|Measure-Object -Property top -Minimum).Minimum
 $issues=@($items|Where-Object {$_.prevText -eq '' -and $_.prevStyle.StartsWith('TDC 본문') -and
     (($_.prevPage -eq $_.page -and $_.prevTop -le ($top+0.5)) -or $_.pageBreakBefore -ne 0)})
 Write-Output ('Units='+$items.Count+' first-content-top='+$top+' unnecessary-gaps='+$issues.Count)
 $issues|Select-Object unit,page,top,prevTop,pageBreakBefore|ConvertTo-Json -Depth 5
}finally{
 if($null -ne $doc){$doc.Close(0)};$app.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
