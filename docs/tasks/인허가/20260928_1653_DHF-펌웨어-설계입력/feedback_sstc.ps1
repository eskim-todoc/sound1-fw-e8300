$ErrorActionPreference='Stop'
$q=Join-Path $PSScriptRoot 'feedback-sstc-20260929'
$j=Get-Content -Raw -Encoding UTF8 -LiteralPath "$q\plan.json" | ConvertFrom-Json
$x=New-Object -ComObject Excel.Application
$x.Visible=$false;$x.DisplayAlerts=$false
function Put($cell,$v){if($null -eq $v){$cell.ClearContents() | Out-Null}else{$cell.Value2="'"+[string]$v}}
function FitRow($sheet,$row){
 $height=[double]$sheet.Rows.Item([int]$row).RowHeight
 for($c=2;$c -le 10;$c++){
  $cell=$sheet.Cells.Item([int]$row,$c);$v=[string]$cell.Value2
  $lines=0;foreach($line in $v.Split("`n")){$units=0;foreach($ch in $line.ToCharArray()){$units+=$(if([int]$ch -gt 127){2}else{1})};$lines+=[Math]::Max(1,[Math]::Ceiling($units/[Math]::Max(4,($cell.ColumnWidth-2))))}
  $height=[Math]::Max($height,$lines*([double]$cell.Font.Size+4)+10)
 }
 $sheet.Range("A${row}:J$row").RowHeight=[int][Math]::Min(409,[Math]::Ceiling($height))
}
try{
 $w=$x.Workbooks.Open("$q\before.xlsx",0,$false);$a=$x.Workbooks.Open("$q\authored.xlsx",0,$true)
 $s=$w.Worksheets.Item($j.sheet)
 $s.Rows.Item([int]$j.insertRow).Insert() | Out-Null
 $r=[int]$j.insertRow;$p=$r-1;$s.Range("B${p}:J$p").Copy($s.Range("B${r}:J$r")) | Out-Null
 foreach($entry in $j.body){
  $r=[int]$entry.row
  for($c=2;$c -le 10;$c++){if($s.Cells.Item($r,$c).Value2 -cne $entry.values[$c-2]){Put $s.Cells.Item($r,$c) $entry.values[$c-2]}}
  $s.Range("B${r}:J$r").WrapText=$true
  FitRow $s $r
 }
 $w.Worksheets.Item($j.diffname).Delete()
 $s.Copy([Type]::Missing,$w.Worksheets.Item($w.Worksheets.Count));$d=$w.Worksheets.Item($w.Worksheets.Count);$d.Name=$j.diffname
 $extra=$j.entries.Count-81
 if($extra -gt 0){$d.Rows.Item("106:$([int](105+$extra))").Insert() | Out-Null}
 $end=24+$j.entries.Count
 $d.Range("A25:J$end").Clear() | Out-Null
 $d.Columns.Item(1).ColumnWidth=6
 $d.Range('A1:J1').UnMerge() | Out-Null;$d.Range('A1:J1').Merge() | Out-Null
 $d.Range('A1').Value2='[-] 원문 / [+] 수정문 / 표시 없음: 유지. 원본 시험 항목과 비교하며 표지는 제외.'
 $d.Range('A1').Font.Size=10;$d.Rows.Item(1).RowHeight=24
 $i=0
 foreach($e in $j.entries){
  $r=25+$i;$br=[int]$e.bodyrow
  $s.Range("B${br}:J$br").Copy($d.Range("B${r}:J$r")) | Out-Null
  for($c=2;$c -le 10;$c++){Put $d.Cells.Item($r,$c) $e.values[$c-2]}
  Put $d.Cells.Item($r,1) $e.mark;$d.Cells.Item($r,1).Font.Bold=$true;$d.Cells.Item($r,1).HorizontalAlignment=-4108
  $d.Range("A${r}:J$r").RowHeight=[int][Math]::Ceiling([double]$s.Rows.Item($br).RowHeight)
  if($e.mark){$d.Range("A${r}:J$r").Interior.Color=$a.Worksheets.Item('비교').Cells.Item($i+1,1).Interior.Color}
  FitRow $d $r
  $i++
 }
 $a.Close($false)
 $d.PageSetup.PrintArea="A1:J$($d.UsedRange.Rows.Count)";$d.PageSetup.PrintTitleRows='$23:$24'
 $d.PageSetup.Orientation=2;$d.PageSetup.PaperSize=8;$d.PageSetup.Zoom=$false;$d.PageSetup.FitToPagesWide=1;$d.PageSetup.FitToPagesTall=$false
 $d.Activate();$x.ActiveWindow.Zoom=70;$d.Range('D25').Select();$x.ActiveWindow.FreezePanes=$true
 $w.SaveAs("$q\final.xlsx",51)
 $s.PageSetup.Orientation=2;$s.PageSetup.PaperSize=8;$s.PageSetup.Zoom=$false;$s.PageSetup.FitToPagesWide=1;$s.PageSetup.FitToPagesTall=$false
 $s.PageSetup.PrintArea='B68:J81';$s.ExportAsFixedFormat(0,"$q\body.pdf")
 $d.PageSetup.PrintArea='A23:J28';$d.ExportAsFixedFormat(0,"$q\diff.pdf")
 $w.Close($false)
}finally{$x.Quit()}
