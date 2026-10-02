$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa\unit-reconciliation'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'tables-plan.json')|ConvertFrom-Json
$outdir=Join-Path (Get-Location) 'docs\참고\인허가\사운드1\DHF\03_설계 출력\SW\펌웨어'
$candidate=Join-Path $qa 'SUVC-candidate.xlsx'
Copy-Item -LiteralPath (Join-Path $qa 'SUVC-source.xlsx') -Destination $candidate
$app=New-Object -ComObject Excel.Application
$app.Visible=$false;$app.DisplayAlerts=$false;$app.EnableEvents=$false;$app.AutomationSecurity=3
$wb=$null;$author=$null;$sitc=$null
function Put($sheet,$addr,$value){
 $cell=$sheet.Range($addr)
 if($null -eq $value){if($cell.MergeCells){$cell.MergeArea.ClearContents()|Out-Null}else{$cell.ClearContents()|Out-Null}}
 elseif($value -is [ValueType]){$cell.Value2=[double]$value}else{$cell.Value2="'$value"}
}
function Fit($sheet,$start,$end,$cols){
 $sheet.Range($sheet.Cells.Item($start,1),$sheet.Cells.Item($end,$cols)).Rows.AutoFit()|Out-Null
 for($r=$start;$r -le $end;$r++){
  $height=[Math]::Max(35,[double]$sheet.Rows.Item($r).RowHeight+14)
  if($height -gt 409){throw "Row too tall: $($sheet.Name) $r $height"}
  $sheet.Rows.Item($r).RowHeight=$height
 }
}
try{
 $wb=$app.Workbooks.Open($candidate,0,$false)
 $author=$app.Workbooks.Open((Join-Path $qa 'units-authored.xlsx'),0,$true)
 $cover=$wb.Worksheets.Item(1);$body=$wb.Worksheets.Item('기준서_유닛 검증')
 Put $cover 'L2' 'SUVC-FW-TD24A';Put $cover 'L3' 'Rev.0'
 Put $cover 'B5' (([string]$cover.Range('B5').Value2).Replace('TD20B-A-EXT/TD23D-A-EXT','TD24A-A-EXT').Replace('TD20B','TD24A').Replace('TD23D','TD24A'))
 Put $cover 'D12' '수석연구원';Put $cover 'H12' '김은수'
 foreach($cell in @('J12','M12','D14','H14','J14','M14','D16','H16','J16','M16','D24')){Put $cover $cell $null}
 Put $cover 'B24' 0
 foreach($r in 25..30){foreach($c in @('B','D','F')){Put $cover ($c+$r) $null}}
 # Remove only signature images anchored in the known approval grid.
 for($i=$cover.Shapes.Count;$i -ge 1;$i--){$sh=$cover.Shapes.Item($i);if($sh.TopLeftCell.Row -ge 12 -and $sh.TopLeftCell.Row -le 17){$sh.Delete()}}
 $body.Copy([Type]::Missing,$body);$comparison=$wb.Worksheets.Item($body.Index+1);$comparison.Name='기준서_유닛 검증_변경 비교'
 foreach($s in @($body,$comparison)){
  Put $s 'D7' 'TD24A-A-EXT';Put $s 'D8' $null;Put $s 'C11' '인공달팽이관 외부장치 Sound1 (TD24A-A-EXT)'
  Put $s 'C18' 'QCC 연계 시험 장치';Put $s 'D18' 'SPI 명령 및 I2S 오디오 입력/응답 확인'
  Put $s 'C19' 'QCC 펌웨어 자료';Put $s 'D19' 'QCC 내부 설계와 유닛 시험은 자료 확보 후 별도 추가'
  Put $s 'C21' 'I2S 오디오 입력 장치';Put $s 'D21' '외부 오디오 입력 경로 확인'
 }
 foreach($entry in @(@($body,'본문',$plan.suvc.physical.Count),@($comparison,'비교',$plan.suvc.diffphysical.Count))){
  $s=$entry[0];$src=$author.Worksheets.Item($entry[1]);$n=[int]$entry[2];$last=29+$n
  $s.Range('A30:K215').UnMerge()
  if($n -gt 186){$s.Rows.Item('216:'+($n+29)).Insert()|Out-Null}
  elseif($n -lt 186){$s.Rows.Item(($last+1).ToString()+':215').Delete()|Out-Null}
  $range=$s.Range('A30:K'+$last);$s.Range('A30:K30').Copy()|Out-Null;$range.PasteSpecial(-4122)|Out-Null
  $range.ClearContents()|Out-Null;$range.Value2=$src.Range('A1:K'+$n).Value2
  $range.WrapText=$true;$range.VerticalAlignment=-4160;$range.Font.Size=11;$range.Font.Name='맑은 고딕'
  Fit $s 30 $last 11
  if($s.Name.EndsWith('_변경 비교')){
   $s.Columns.Item(1).ColumnWidth=5
   for($r=30;$r -le $last;$r++){
    $mark=$s.Cells.Item($r,1).Value2
    $s.Range('A'+$r+':K'+$r).Interior.Color=$(if($mark -eq '[-]'){14277081}else{14286818})
   }
  }
  $s.PageSetup.PrintArea='$A$1:$K$'+($last+8)
  $s.PageSetup.PrintTitleRows='$28:$29';$s.PageSetup.Zoom=$false;$s.PageSetup.FitToPagesWide=1;$s.PageSetup.FitToPagesTall=$false
 }
 foreach($entry in @(@('유닛 목록','유닛 목록',$plan.suvc.code_rows.Count),@('원본 대응표','원본 대응표',$plan.suvc.original_rows.Count),@('1차본 대응표','1차본 대응표',$plan.suvc.first_draft_rows.Count))){
  $s=$wb.Worksheets.Add([Type]::Missing,$wb.Worksheets.Item($wb.Worksheets.Count));$s.Name=$entry[0]
  $src=$author.Worksheets.Item($entry[1]);$count=[int]$entry[2];$cols=$src.UsedRange.Columns.Count
  $range=$s.Range($s.Cells.Item(1,1),$s.Cells.Item($count,$cols));$range.Value2=$src.UsedRange.Value2
  $range.Font.Name='맑은 고딕';$range.Font.Size=11;$range.WrapText=$true;$range.VerticalAlignment=-4160
  $range.Borders.LineStyle=1;$range.Borders.Color=12632256
  $s.Range($s.Cells.Item(1,1),$s.Cells.Item(1,$cols)).Interior.Color=15132390
  $s.Rows.Item(1).Font.Bold=$true
  for($c=1;$c -le $cols;$c++){$s.Columns.Item($c).ColumnWidth=30}
  if($entry[0] -eq '원본 대응표'){$s.Columns.Item(7).ColumnWidth=65}
  if($entry[0] -eq '유닛 목록'){$s.Columns.Item(6).ColumnWidth=80;$s.Columns.Item(7).ColumnWidth=90}
  $range.Rows.AutoFit()|Out-Null
  for($r=1;$r -le $count;$r++){$s.Rows.Item($r).RowHeight=[Math]::Min(409,[double]$s.Rows.Item($r).RowHeight+8)}
  $range.AutoFilter()|Out-Null
  $s.PageSetup.Orientation=2;$s.PageSetup.PaperSize=9;$s.PageSetup.PrintArea=$range.Address();$s.PageSetup.PrintTitleRows='$1:$1';$s.PageSetup.Zoom=$false;$s.PageSetup.FitToPagesWide=1;$s.PageSetup.FitToPagesTall=$false
 }
 $body.Activate();$app.CalculateFull();$wb.Save()
 $body.ExportAsFixedFormat(0,(Join-Path $qa 'SUVC-body.pdf'))
 foreach($s in @($cover,$comparison,$wb.Worksheets.Item('유닛 목록'),$wb.Worksheets.Item('원본 대응표'),$wb.Worksheets.Item('1차본 대응표'))){
  $saved=$s.PageSetup.PrintArea
  if($s.Name -eq $comparison.Name){$s.PageSetup.PrintArea='$A$28:$K$37'}
  elseif($s.Index -gt 3){$s.PageSetup.PrintArea=$s.Range($s.Cells.Item(1,1),$s.Cells.Item(6,$s.UsedRange.Columns.Count)).Address()}
  $s.ExportAsFixedFormat(0,(Join-Path $qa ('SUVC-sheet-'+$s.Index+'.pdf')));$s.PageSetup.PrintArea=$saved
 }
 Write-Output ('SUVC physical rows: '+$plan.suvc.physical.Count)
 $wb.Close($false);$wb=$null

 $sitcPath=Join-Path $qa 'SITC-candidate.xlsx';Copy-Item -LiteralPath (Join-Path $qa 'SITC-before.xlsx') -Destination $sitcPath
 $sitc=$app.Workbooks.Open($sitcPath,0,$false)
 $src=$author.Worksheets.Item('SITC 참조')
 for($i=0;$i -lt $plan.sitc.Count;$i++){
  $edit=$plan.sitc[$i];$s=$sitc.Worksheets.Item($edit.sheet);$s.Range($edit.cell).Value2=$src.Cells.Item($i+1,3).Value2
 }
 foreach($group in ($plan.sitc | Group-Object sheet)){
  $s=$sitc.Worksheets.Item($group.Name)
  foreach($r in ($group.Group.cell | ForEach-Object {[int]($_ -replace '[A-Z]','')} | Sort-Object -Unique)){
   $oldHeight=[double]$s.Rows.Item($r).RowHeight
   $s.Rows.Item($r).AutoFit()|Out-Null
   $s.Rows.Item($r).RowHeight=[Math]::Min(409,[Math]::Max($oldHeight,[double]$s.Rows.Item($r).RowHeight+10))
  }
 }
 $app.CalculateFull();$sitc.Save()
 foreach($s in @($sitc.Worksheets.Item(2),$sitc.Worksheets.Item(3))){
  $saved=$s.PageSetup.PrintArea;$s.PageSetup.PrintArea='$A$23:$M$29'
  $s.ExportAsFixedFormat(0,(Join-Path $qa ('SITC-sheet-'+$s.Index+'.pdf')));$s.PageSetup.PrintArea=$saved
 }
 Write-Output ('SITC cells: '+$plan.sitc.Count)
} finally {
 foreach($book in @($sitc,$author,$wb)){if($book){$book.Close($false)}}
 $app.Quit();$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
