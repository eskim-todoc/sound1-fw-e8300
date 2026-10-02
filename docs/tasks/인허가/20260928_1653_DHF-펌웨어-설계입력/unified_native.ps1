$ErrorActionPreference='Stop'
$q=Join-Path $PSScriptRoot 'unified-20260929'
$jobs=Get-Content -Raw -Encoding UTF8 -LiteralPath "$q\plan.json" | ConvertFrom-Json
$oldPlan=Get-Content -Raw -Encoding UTF8 -LiteralPath "$PSScriptRoot\review-20260929\plan.json" | ConvertFrom-Json
$x=New-Object -ComObject Excel.Application
$x.Visible=$false;$x.DisplayAlerts=$false;$x.AskToUpdateLinks=$false
function WriteLiteral($cell,$value){if($null -eq $value){$cell.ClearContents() | Out-Null}else{$cell.Value2="'"+[string]$value}}
try{foreach($j in $jobs){
 $w=$x.Workbooks.Open($j.backup,0,$false)
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('review-20260929\'+$j.name)) -Destination "$q\$($j.kind)-plain.xlsx" -Force
 $source=$x.Workbooks.Open("$q\$($j.kind)-plain.xlsx",0,$true)
 $auth=$x.Workbooks.Open("$q\$($j.kind)-authored.xlsx",0,$true)
 # Restore only diff fill overlays, retaining user content and other formatting.
 $prior=$oldPlan | Where-Object kind -eq $j.kind
 foreach($c in $prior.colors){$w.Worksheets.Item($c.sheet).Range($c.cell).Interior.ColorIndex=$source.Worksheets.Item($c.sheet).Range($c.cell).Interior.ColorIndex
  if($source.Worksheets.Item($c.sheet).Range($c.cell).Interior.ColorIndex -ge 0){$w.Worksheets.Item($c.sheet).Range($c.cell).Interior.Color=$source.Worksheets.Item($c.sheet).Range($c.cell).Interior.Color}}
 $source.Close($false)
 $w.Worksheets.Item('변경 비교').Delete()
 $s=$w.Worksheets.Item($j.sheet)
 $s.Copy([Type]::Missing,$w.Worksheets.Item($w.Worksheets.Count));$snap=$w.Worksheets.Item($w.Worksheets.Count);$snap.Name='작업용사본'
 $last=[char](64+$j.cols)
 $snap.Range("B$($j.start):$last$($j.end)").UnMerge()
 $s.Range("B$($j.start):$last$($j.end)").UnMerge()
 foreach($r in $j.body){
  $snap.Range("B$($r.oldrow):$last$($r.oldrow)").Copy($s.Range("B$($r.row):$last$($r.row)"))
  $s.Rows.Item([int]$r.row).RowHeight=$snap.Rows.Item([int]$r.oldrow).RowHeight
  for($c=2;$c -le $j.cols;$c++){WriteLiteral $s.Cells.Item([int]$r.row,$c) $r.values[$c-2]}
  if($j.kind -eq 'SRS' -and $null -ne $r.id){$s.Cells.Item([int]$r.row,7).Formula='="SW-FW-RID-"&'+$r.id}
 }
 foreach($m in $j.merges){$s.Range($m).Merge()}
 $snap.Delete()
 $s.Copy([Type]::Missing,$w.Worksheets.Item($w.Worksheets.Count));$d=$w.Worksheets.Item($w.Worksheets.Count);$d.Name=$j.diffname
 $lastRow=$d.UsedRange.Rows.Count+20
 $d.Range("A$($j.start):$last$lastRow").UnMerge()
 $d.Range("A$($j.start):$last$lastRow").Clear()
 $d.Columns.Item(1).ColumnWidth=6
 $d.Range("A1:$last"+'1').UnMerge();$d.Range("A1:$last"+'1').Merge()
 $d.Range('A1').Value2='[-] 원문 / [+] 수정문 / 표시 없음: 유지. 원본 본문과 비교, 표지 제외.'
 $d.Range('A1').Font.Size=10;$d.Rows.Item(1).RowHeight=24
 $index=0;$bounds=@{}
 foreach($e in $j.entries){
  $r=$j.start+$index;$br=[int]$e.bodyrow
  $s.Range("B${br}:$last$br").Copy($d.Range("B${r}:$last$r"))
  $d.Range("B${r}:$last$r").UnMerge()
  for($c=2;$c -le $j.cols;$c++){WriteLiteral $d.Cells.Item($r,$c) $e.values[$c-2]}
  WriteLiteral $d.Cells.Item($r,1) $e.mark
  $d.Cells.Item($r,1).Font.Bold=$true;$d.Cells.Item($r,1).HorizontalAlignment=-4108
  $d.Range("B${r}:$last$r").WrapText=$true
  $d.Rows.Item($r).RowHeight=$s.Rows.Item($br).RowHeight
  if($e.mark){$d.Range("A${r}:$last$r").Interior.Color=$auth.Worksheets.Item($j.diffname).Cells.Item($index+1,1).Interior.Color}
  if(-not $bounds.ContainsKey($br)){$bounds[$br]=@($r,$r)}else{$bounds[$br][1]=$r}
  $index++
 }
 # Recreate the original grouped headers across their corresponding unified rows.
 if($j.kind -eq 'SRS'){
  foreach($m in $j.merges){
   $range=$s.Range($m);$a=[int]$range.Row;$b=$a+$range.Rows.Count-1;$c=[int]$range.Column;$z=$c+$range.Columns.Count-1
   if($a -eq $b){foreach($dr in $bounds[$a][0]..$bounds[$a][1]){$d.Range($d.Cells.Item($dr,$c),$d.Cells.Item($dr,$z)).Merge()}}
   else{$d.Range($d.Cells.Item($bounds[$a][0],$c),$d.Cells.Item($bounds[$b][1],$z)).Merge()}
  }
 }
 # Keep the non-test sections in order too, including their changed lines.
 if($j.kind -eq 'SSTC'){
  for($r=105;$r -le $s.UsedRange.Rows.Count;$r++){
   $dr=$j.start+$index;$extra=$j.extras | Where-Object row -eq $r
   if($extra){
    foreach($which in @('old','new')){
     $s.Range("B${r}:$last$r").Copy($d.Range("B${dr}:$last$dr"));$d.Range("B${dr}:$last$dr").UnMerge()
     for($c=2;$c -le $j.cols;$c++){WriteLiteral $d.Cells.Item($dr,$c) $extra.($which)[$c-2]}
     $d.Cells.Item($dr,1).Value2=$(if($which -eq 'old'){'[-]'}else{'[+]'})
     $d.Range("A${dr}:$last$dr").Interior.Color=$(if($which -eq 'old'){14214396}else{14282722})
     $index++;$dr++
    }
   }else{$s.Range("B${r}:$last$r").Copy($d.Range("B${dr}:$last$dr"));$index++}
  }
 }
 $end=$j.start+$index-1
 $d.PageSetup.PrintArea="A1:$last$end";$d.PageSetup.PrintTitleRows=$(if($j.kind -eq 'SRS'){'$4:$5'}else{'$23:$24'})
 $d.Activate();$x.ActiveWindow.Zoom=70
 $d.Range("F$($j.start)").Select();$x.ActiveWindow.FreezePanes=$true
 $auth.Close($false)
 $w.SaveAs("$q\$($j.kind)-final.xlsx",51)
 # Private QA crops; do not save altered print areas.
 $s.PageSetup.PrintArea=$(if($j.kind -eq 'SRS'){'B47:I58'}else{'B66:J76'})
 $s.ExportAsFixedFormat(0,"$q\$($j.kind)-body.pdf")
 $d.PageSetup.PrintArea=$(if($j.kind -eq 'SRS'){'A4:I13'}else{'A23:J28'})
 $d.ExportAsFixedFormat(0,"$q\$($j.kind)-diff.pdf")
 $w.Close($false);Write-Output "$($j.kind) completed"
}}finally{$x.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($x)}
