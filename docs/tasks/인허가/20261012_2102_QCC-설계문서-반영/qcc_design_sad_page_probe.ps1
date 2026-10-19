$ErrorActionPreference='Stop'
$taskPath=$PSScriptRoot
$dhfPath='G:\공유 드라이브\20_프로젝트별\01_project sound1\08_DHF\03_설계 출력\SW\외부기'
$file=(Get-ChildItem -LiteralPath $dhfPath -Filter '*SAD*.docx' | Where-Object Name -Like '[[]E8300-QCC]*').FullName
$hashBefore=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
$app=New-Object -ComObject Word.Application
$app.Visible=$false
$app.DisplayAlerts=0
$app.AutomationSecurity=3
$doc=$null
try {
 $doc=$app.Documents.Open($file,$false,$true,$false)
 $beforeRevisions=$doc.Revisions.Count
 $doc.Revisions.AcceptAll()
 $doc.ShowRevisions=$false
 $doc.Repaginate()
 $pages=$doc.ComputeStatistics(2)
 $ranges=New-Object System.Collections.Generic.List[object]
 for($page=2;$page -le 6;$page++) {
  $begin=$doc.GoTo(1,1,$page).Start
  $end=$doc.GoTo(1,1,($page+1)).Start
  $region=$doc.Range($begin,$end)
  $paras=New-Object System.Collections.Generic.List[object]
  foreach($p in $region.Paragraphs) {
   $r=$p.Range
   $paras.Add([pscustomobject]@{start=$r.Start;end=$r.End;text=$r.Text;pageStart=$doc.Range($r.Start,$r.Start).Information(3);pageEnd=$doc.Range($r.End,$r.End).Information(3);pageBreakBefore=$p.Format.PageBreakBefore;keepWithNext=$p.Format.KeepWithNext;keepTogether=$p.Format.KeepTogether;widowControl=$p.Format.WidowControl;spaceBefore=$p.Format.SpaceBefore;spaceAfter=$p.Format.SpaceAfter;lineSpacing=$p.Format.LineSpacing;style=[string]$p.Style.NameLocal})
  }
  $ranges.Add([pscustomobject]@{page=$page;start=$begin;end=$end;paragraphs=$paras})
 }
 $sections=New-Object System.Collections.Generic.List[object]
 foreach($s in $doc.Sections){$sections.Add([pscustomobject]@{start=$s.Range.Start;end=$s.Range.End;sectionStart=$s.PageSetup.SectionStart;pageStart=$doc.Range($s.Range.Start,$s.Range.Start).Information(3)})}
 $tocs=New-Object System.Collections.Generic.List[object]
 foreach($t in $doc.TablesOfContents){$tocs.Add([pscustomobject]@{start=$t.Range.Start;end=$t.Range.End;pageEnd=$doc.Range($t.Range.End,$t.Range.End).Information(3);tail=$doc.Range([Math]::Max($t.Range.Start,$t.Range.End-240),$t.Range.End).Text})}
 $result=[pscustomobject]@{file=$file;sha256_before=$hashBefore;read_only=$doc.ReadOnly;accepted_in_memory_only=$true;revision_count_before=$beforeRevisions;pages=$pages;sections=$sections;tocs=$tocs;page_ranges=$ranges}
 $result | ConvertTo-Json -Depth 10 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskPath 'qa/qcc_design/SAD-blank-page-probe.json')
 Write-Output ('pages='+$pages+' sections='+$sections.Count+' TOCs='+$tocs.Count)
 $doc.TrackRevisions=$false
 $tailParagraph=$doc.TablesOfContents.Item(1).Range.Paragraphs.Last
 $tailParagraph.Format.LineSpacingRule=4
 $tailParagraph.Format.LineSpacing=1
 $doc.Repaginate()
 $experimentBeforeUpdate=[pscustomobject]@{pages=$doc.ComputeStatistics(2);tailText=$tailParagraph.Range.Text;tailLineSpacing=$tailParagraph.Format.LineSpacing;tailLineSpacingRule=$tailParagraph.Format.LineSpacingRule;tailPage=$doc.Range($tailParagraph.Range.Start,$tailParagraph.Range.Start).Information(3)}
 $doc.TablesOfContents.Item(1).Update()
 $doc.Repaginate()
 $tailAfter=$doc.TablesOfContents.Item(1).Range.Paragraphs.Last
 $experimentAfterUpdate=[pscustomobject]@{pages=$doc.ComputeStatistics(2);tailText=$tailAfter.Range.Text;tailLineSpacing=$tailAfter.Format.LineSpacing;tailLineSpacingRule=$tailAfter.Format.LineSpacingRule;tailPage=$doc.Range($tailAfter.Range.Start,$tailAfter.Range.Start).Information(3)}
 [pscustomobject]@{before=$experimentBeforeUpdate;after_toc_update=$experimentAfterUpdate;saved=$false} | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $taskPath 'qa/qcc_design/SAD-blank-page-experiment.json')
 Write-Output ('experiment pages='+$experimentBeforeUpdate.pages+' after TOC='+$experimentAfterUpdate.pages)
} finally {
 if($null -ne $doc){$doc.Close(0);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}
 $app.Quit(0)
 [void][Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
$hashAfter=(Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
if($hashAfter -ne $hashBefore){throw 'Unexpected saved deliverable mutation'}
