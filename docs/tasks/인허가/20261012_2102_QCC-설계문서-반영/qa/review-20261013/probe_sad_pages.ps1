param([string]$Target,[string]$RecordPath)
$ErrorActionPreference='Stop'
$app=New-Object -ComObject Word.Application
$app.Visible=$false;$app.DisplayAlerts=0;$app.AutomationSecurity=3
$doc=$null
try {
 $doc=$app.Documents.Open($Target,$false,$true,$false)
 $doc.Revisions.AcceptAll();$doc.ShowRevisions=$false;$doc.Repaginate()
 $rows=@()
 foreach($page in @(3,4,5)){
  $start=$doc.GoTo(1,1,$page).Start;$end=$doc.GoTo(1,1,($page+1)).Start
  $range=$doc.Range($start,$end)
  foreach($paragraph in $range.Paragraphs){
   $pf=$paragraph.Format
   $rows+=@{page=$page;start=$paragraph.Range.Start;end=$paragraph.Range.End;text=$paragraph.Range.Text;style=[string]$paragraph.Style.NameLocal;lineSpacing=$pf.LineSpacing;spaceBefore=$pf.SpaceBefore;spaceAfter=$pf.SpaceAfter;pageBreakBefore=$pf.PageBreakBefore;keepWithNext=$pf.KeepWithNext;keepTogether=$pf.KeepTogether;widowControl=$pf.WidowControl;xml=$paragraph.Range.WordOpenXML}
  }
 }
 $rows | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath $RecordPath
 $rows | Select-Object page,start,end,text,style,lineSpacing,spaceBefore,spaceAfter,pageBreakBefore,keepWithNext,keepTogether,widowControl | ConvertTo-Json -Depth 4 -EscapeHandling EscapeNonAscii
}finally{if($null-ne$doc){$doc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($doc)};$app.Quit(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
