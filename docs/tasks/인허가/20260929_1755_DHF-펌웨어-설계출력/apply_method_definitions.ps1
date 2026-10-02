param([switch]$RenderOnly)
$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa\method-definitions'
$p=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'plan.json')|ConvertFrom-Json
$app=New-Object -ComObject Excel.Application
$app.Visible=$false;$app.DisplayAlerts=$false;$app.EnableEvents=$false;$app.AutomationSecurity=3
try{
 foreach($b in $p.books){
  $dir=Join-Path $qa $b.key;$candidate=Join-Path $dir 'candidate.xlsx'
  if(-not $RenderOnly){Copy-Item -LiteralPath (Join-Path $dir 'before.xlsx') -Destination $candidate}
  $wb=$null;$author=$null
  try{
   $wb=$app.Workbooks.Open($candidate,0,[bool]$RenderOnly)
   $main=$wb.Worksheets.Item($b.sheet);$diff=$wb.Worksheets.Item($b.comparison)
   $col=[char](64+[int]$b.cols)
   if(-not $RenderOnly){
    $author=$app.Workbooks.Open((Join-Path $dir 'authored.xlsx'),0,$true)
    $updates=$author.Worksheets.Item('변경 셀')
    for($i=1;$i -le $b.changes.Count;$i++){
     $r=[int]$updates.Cells.Item($i,1).Value2;$c=[int]$updates.Cells.Item($i,2).Value2
     $main.Cells.Item($r,$c).Value2=$updates.Cells.Item($i,3).Value2
    }
    $newlast=[int]$b.start+$b.diff.Count-1;$oldlast=[int]$b.old_diff_last
    if($newlast -gt $oldlast){$diff.Rows.Item(($oldlast+1).ToString()+':'+$newlast).Insert()|Out-Null}
    elseif($newlast -lt $oldlast){$diff.Rows.Item(($newlast+1).ToString()+':'+$oldlast).Delete()|Out-Null}
    $range=$diff.Range('A'+$b.start+':'+$col+$newlast);$range.UnMerge()
    $diff.Range('A'+$b.start+':'+$col+$b.start).Copy()|Out-Null;$range.PasteSpecial(-4122)|Out-Null
    $range.ClearContents();$author.Worksheets.Item('비교').Range('A1:'+ $col+$b.diff.Count).Copy()|Out-Null;$range.PasteSpecial(-4163)|Out-Null
    $range.WrapText=$true;$range.Font.Color=0;$range.Font.Bold=$false;$range.Font.Name='맑은 고딕';$range.Font.Size=11;$range.VerticalAlignment=-4160
    for($r=[int]$b.start;$r -le $newlast;$r++){
     $mark=[string]$diff.Cells.Item($r,1).Value2
     $diff.Range('A'+$r+':'+$col+$r).Interior.Color=$(if($mark -eq '[-]'){15198717}elseif($mark -eq '[+]'){15267562}else{16777215})
    }
    foreach($s in @($main,$diff)){
     $legend=$s.Range('C12:'+ $col+'12');$legend.UnMerge();$legend.Merge();$legend.Value2=$author.Worksheets.Item('정의').Cells.Item(1,1).Value2
     $legend.Font.Name='맑은 고딕';$legend.Font.Size=11;$legend.Font.Bold=$false;$legend.Font.Color=0;$legend.WrapText=$true
     $legend.HorizontalAlignment=-4131;$legend.VerticalAlignment=-4160;$s.Rows.Item(12).RowHeight=105
     if($b.key -eq 'unit'){$s.Range('D23:F23').Value2='시험 방법은 위의 I/A/D/T 정의를 따른다. RTT 결과가 누락되면 판정하지 않고 다시 확인한다.';$s.Rows.Item(23).RowHeight=54}
    }
    foreach($s in @($main,$diff)){
     $last=$(if($s.Name -eq $main.Name){[int]$b.last}else{$newlast})
     $s.Rows.Item($b.start.ToString()+':'+$last).AutoFit()|Out-Null
     for($r=[int]$b.start;$r -le $last;$r++){
      $h=[double]$s.Rows.Item($r).RowHeight
      if($h -ge 409){throw "Row cannot fit: $($b.key) $($s.Name) $r"}
      $s.Rows.Item($r).RowHeight=[Math]::Max(48,$h+10)
     }
     if($s.Name -eq $diff.Name){$s.PageSetup.PrintArea='$A$1:$'+$col+'$'+($s.UsedRange.Row+$s.UsedRange.Rows.Count-1)}
     if($b.key -eq 'input'){
      $s.PageSetup.Orientation=2;$s.PageSetup.Zoom=$false;$s.PageSetup.FitToPagesWide=1;$s.PageSetup.FitToPagesTall=$false
      $s.PageSetup.PrintArea='$A$1:$J$'+($s.UsedRange.Row+$s.UsedRange.Rows.Count-1)
     }
    }
    $app.CalculateFull();$wb.Save();$app.CutCopyMode=0
   }else{
    $main.ExportAsFixedFormat(0,(Join-Path $dir 'main.pdf'))
    foreach($entry in @(@('legend',$main,('B10:'+ $col+'16')),@('diff',$diff,('A'+($b.start-2)+':'+$col+($b.start+3))))){
     $s=$entry[1];$s.PageSetup.PrintArea=$entry[2];$s.PageSetup.PrintTitleRows='';$s.PageSetup.Zoom=$false;$s.PageSetup.FitToPagesWide=1;$s.PageSetup.FitToPagesTall=1
     $s.ExportAsFixedFormat(0,(Join-Path $dir ($entry[0]+'.pdf')))
    }
   }
   Write-Output ($b.key+' completed')
  }finally{if($author){$author.Close($false)};if($wb){$wb.Close($false)}}
 } 
}finally{$app.Quit();$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
