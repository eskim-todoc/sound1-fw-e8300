$ErrorActionPreference='Stop'
$q=Join-Path $PSScriptRoot 'criteria-feedback-20260929'
$j=Get-Content -Raw -Encoding UTF8 -LiteralPath "$q\plan.json" | ConvertFrom-Json
$x=New-Object -ComObject Excel.Application
$x.Visible=$false;$x.DisplayAlerts=$false
function Put($cell,$v){if($null -eq $v){$cell.ClearContents() | Out-Null}elseif($v -is [ValueType]){$cell.Value2=[double]$v}else{$cell.Value2="'"+[string]$v}}
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
 $w=$x.Workbooks.Open("$q\before.xlsx",0,$false)
 $a=$x.Workbooks.Open("$q\authored.xlsx",0,$true)
 $s=$w.Worksheets.Item($j.sheet);$d=$w.Worksheets.Item($j.diffname)
 foreach($e in $j.edits){Put $s.Range($e.cell) $e.value}
 foreach($row in ($j.edits | ForEach-Object { [int]($_.cell -replace '\D','') } | Sort-Object -Unique)){FitRow $s $row}
 $i=25
 foreach($e in $j.entries){
  Put $d.Cells.Item($i,1) $e.mark
  for($c=2;$c -le 10;$c++){Put $d.Cells.Item($i,$c) $e.values[$c-2]}
  if($e.mark){$d.Range("A${i}:J$i").Interior.Color=$a.Worksheets.Item('비교').Cells.Item($i,1).Interior.Color}
  FitRow $d $i
  $i++
 }
 $a.Close($false)
 $w.SaveAs("$q\final.xlsx",51)
 $s.PageSetup.Orientation=2;$s.PageSetup.PaperSize=8;$s.PageSetup.Zoom=$false;$s.PageSetup.FitToPagesWide=1;$s.PageSetup.FitToPagesTall=$false
 $s.PageSetup.PrintArea='B45:J46';$s.ExportAsFixedFormat(0,"$q\led.pdf")
 $s.PageSetup.PrintArea='B70:J71';$s.ExportAsFixedFormat(0,"$q\bluetooth.pdf")
 $d.PageSetup.PrintArea='A63:J68';$d.ExportAsFixedFormat(0,"$q\diff.pdf")
 $w.Close($false)
}finally{$x.Quit()}
