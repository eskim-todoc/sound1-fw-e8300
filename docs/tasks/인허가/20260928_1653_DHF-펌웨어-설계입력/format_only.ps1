$ErrorActionPreference='Stop'
$q=Join-Path $PSScriptRoot 'format-only-20260929'
$x=New-Object -ComObject Excel.Application
$x.Visible=$false;$x.DisplayAlerts=$false
try{
 $w=$x.Workbooks.Open("$q\before.xlsx",0,$false)
 $a=$x.Workbooks.Open("$q\authored.xlsx",0,$true)
 $s=$w.Worksheets.Item(3);$b=$w.Worksheets.Item(2)
 foreach($r in @(15,13,11)){
  $id=$s.Cells.Item($r,7).Value2
  $br=0;for($i=6;$i -le 91;$i++){if($b.Cells.Item($i,7).Value2 -eq $id){$br=$i;break}}
  if(!$br){throw 'ID not found'}
  $s.Cells.Item($r,1).Value2=$a.Worksheets.Item(3).Cells.Item($r,1).Value2
  for($c=1;$c -le 9;$c++){
   if($s.Cells.Item($r,$c).MergeArea.Rows.Count -gt 1){continue}
   $s.Cells.Item($r,$c).Interior.ColorIndex=$b.Cells.Item($br,$c).Interior.ColorIndex
   if($b.Cells.Item($br,$c).Interior.ColorIndex -ge 0){$s.Cells.Item($r,$c).Interior.Color=$b.Cells.Item($br,$c).Interior.Color}
  }
  $s.Rows.Item($r-1).Delete() | Out-Null
 }
 $a.Close($false)
 $w.SaveAs("$q\final.xlsx",51)
 $s.PageSetup.PrintArea='A4:I15';$s.ExportAsFixedFormat(0,"$q\preview.pdf")
 $w.Close($false)
}finally{$x.Quit()}
