$ErrorActionPreference='Stop'
$q=Join-Path $PSScriptRoot 'feedback-srs-20260929'
$j=Get-Content -Raw -Encoding UTF8 -LiteralPath "$q\plan.json" | ConvertFrom-Json
$x=New-Object -ComObject Excel.Application
$x.Visible=$false;$x.DisplayAlerts=$false
function Text($cell,$v){if($null -eq $v){$cell.ClearContents() | Out-Null}else{$cell.Value2="'"+[string]$v}}
try{
 $w=$x.Workbooks.Open("$q\before.xlsx",0,$false);$a=$x.Workbooks.Open("$q\authored.xlsx",0,$true)
 $s=$w.Worksheets.Item($j.sheet)
 $s.Rows.Item(62).Insert() | Out-Null
 $s.Range('F61:I61').Copy($s.Range('F62:I62')) | Out-Null
 foreach($r in $j.body){
  $id=$r.values[5]
  if($j.edits.PSObject.Properties.Name -contains $id -or $id -eq 'SW-FW-RID-81'){
   Text $s.Cells.Item([int]$r.row,6) $r.values[4]
   if($id -in @('SW-FW-RID-53','SW-FW-RID-81')){Text $s.Cells.Item([int]$r.row,8) $r.values[6]}
   if($id -eq 'SW-FW-RID-81'){$s.Cells.Item([int]$r.row,7).Formula='="SW-FW-RID-"&81';$s.Cells.Item([int]$r.row,9).ClearContents() | Out-Null}
   $cell=$s.Cells.Item([int]$r.row,6);$cell.WrapText=$true
   $lines=0;foreach($line in ([string]$r.values[4]).Split("`n")){$units=0;foreach($c in $line.ToCharArray()){$units+=$(if([int]$c -gt 127){2}else{1})};$lines+=[Math]::Max(1,[Math]::Ceiling($units/($cell.ColumnWidth-2)))}
   $s.Rows.Item([int]$r.row).RowHeight=[Math]::Min(409,[Math]::Max($s.Rows.Item([int]$r.row).RowHeight,$lines*15+10))
  }
 }
 $w.Worksheets.Item($j.diffname).Delete()
 $s.Copy([Type]::Missing,$w.Worksheets.Item($w.Worksheets.Count));$snap=$w.Worksheets.Item($w.Worksheets.Count);$snap.Name='임시서식';$snap.Range('B6:I92').UnMerge() | Out-Null
 $s.Copy([Type]::Missing,$w.Worksheets.Item($w.Worksheets.Count));$d=$w.Worksheets.Item($w.Worksheets.Count);$d.Name=$j.diffname
 $d.Range('A6:I250').UnMerge() | Out-Null;$d.Range('A6:I250').Clear() | Out-Null
 $d.Columns.Item(1).ColumnWidth=6
 $d.Range('A1:I1').UnMerge() | Out-Null;$d.Range('A1:I1').Merge() | Out-Null
 $d.Range('A1').Value2='[-] 원문 / [+] 수정문 / 표시 없음: 유지. 식별자 구분자만 변경된 경우는 표시하지 않음.'
 $d.Range('A1').Font.Size=10;$d.Rows.Item(1).RowHeight=24
 $bounds=@{};$i=0
 foreach($e in $j.entries){
  $r=$i+6;$br=[int]$e.bodyrow
  $snap.Range("B${br}:I$br").Copy($d.Range("B${r}:I$r")) | Out-Null
  for($c=2;$c -le 9;$c++){Text $d.Cells.Item($r,$c) $e.values[$c-2]}
  Text $d.Cells.Item($r,1) $e.mark
  $d.Cells.Item($r,1).Font.Bold=$true;$d.Cells.Item($r,1).HorizontalAlignment=-4108
  $height=[double]$s.Rows.Item([int]$br).RowHeight
  $d.Range("A${r}:I$r").RowHeight=[int][Math]::Ceiling($height)
  if($e.mark){$d.Range("A${r}:I$r").Interior.Color=$a.Worksheets.Item('비교').Cells.Item($i+1,1).Interior.Color}
  if(!$bounds.ContainsKey($br)){$bounds[$br]=@($r,$r)}else{$bounds[$br][1]=$r}
  $i++
 }
 foreach($m in $j.merges){
  $r1=[int]$m[0];$r2=[int]$m[1];$c1=[int]$m[2];$c2=[int]$m[3]
  if($c1 -ge 6){continue}
  if($r1 -eq $r2){foreach($r in $bounds[$r1][0]..$bounds[$r1][1]){$d.Range($d.Cells.Item($r,$c1),$d.Cells.Item($r,$c2)).Merge() | Out-Null}}
  else{$area=$d.Range($d.Cells.Item($bounds[$r1][0],$c1),$d.Cells.Item($bounds[$r2][1],$c2));$area.Merge() | Out-Null;$area.Interior.Color=15790320}
 }
 $snap.Delete();$a.Close($false)
 $d.PageSetup.Orientation=2;$d.PageSetup.PaperSize=8;$d.PageSetup.Zoom=$false;$d.PageSetup.FitToPagesWide=1;$d.PageSetup.FitToPagesTall=$false
 $d.PageSetup.PrintArea="A1:I$($j.entries.Count+5)";$d.PageSetup.PrintTitleRows='$4:$5'
 $d.Activate();$x.ActiveWindow.Zoom=75;$d.Range('F6').Select();$x.ActiveWindow.FreezePanes=$true
 $w.SaveAs("$q\final.xlsx",51)
 $s.PageSetup.PrintArea='B49:I64';$s.PageSetup.Zoom=$false;$s.PageSetup.FitToPagesWide=1;$s.PageSetup.FitToPagesTall=$false
 $s.ExportAsFixedFormat(0,"$q\body.pdf")
 $d.PageSetup.PrintArea='A4:I15';$d.ExportAsFixedFormat(0,"$q\diff.pdf")
 $w.Close($false)
}finally{$x.Quit()}
