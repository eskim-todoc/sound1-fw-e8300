param([switch]$RenderOnly)
$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa\suvc-practical-criteria'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'plan.json')|ConvertFrom-Json
$candidate=Join-Path $qa 'candidate.xlsx'
if(-not $RenderOnly){Copy-Item -LiteralPath (Join-Path $qa 'before.xlsx') -Destination $candidate}
$app=New-Object -ComObject Excel.Application
$app.Visible=$false;$app.DisplayAlerts=$false;$app.EnableEvents=$false;$app.AutomationSecurity=3
$wb=$null;$author=$null
function Put($s,$a,$v){$s.Range($a).Value2=[string]$v}
function Fit($s,$start,$last){
 $s.Rows.Item("${start}:$last").AutoFit()|Out-Null
 for($r=$start;$r -le $last;$r++){
  $h=[Math]::Max(40,[double]$s.Rows.Item($r).RowHeight+14)
  if($h -gt 409){throw "Text requires continued row: $($s.Name) $r $h"}
  $s.Rows.Item($r).RowHeight=$h
 }
}
try{
 $wb=$app.Workbooks.Open($candidate,0,[bool]$RenderOnly)
 if(-not $RenderOnly){
  $author=$app.Workbooks.Open((Join-Path $qa 'authored.xlsx'),0,$true)
  foreach($entry in @(@('기준서_유닛 검증','본문',$plan.physical.Count),@('기준서_유닛 검증_변경 비교','비교',$plan.diffphysical.Count))){
   $s=$wb.Worksheets.Item($entry[0]);$src=$author.Worksheets.Item($entry[1]);$n=[int]$entry[2]
   $oldLast=0
   for($r=30;$r -le $s.UsedRange.Rows.Count;$r++){
    if([string]$s.Cells.Item($r,4).Value2 -like 'SW-FW-UnitTest-*'){$oldLast=$r}
    elseif([string]$s.Cells.Item($r,5).Value2 -like '계속 *'){$oldLast=$r}
   }
   $oldCount=$oldLast-29;$last=29+$n
   if($n -gt $oldCount){$s.Rows.Item(($oldLast+1).ToString()+':'+$last).Insert()|Out-Null}
   elseif($n -lt $oldCount){$s.Rows.Item(($last+1).ToString()+':'+$oldLast).Delete()|Out-Null}
   $range=$s.Range('A30:K'+$last);$range.UnMerge();$s.Range('A30:K30').Copy()|Out-Null;$range.PasteSpecial(-4122)|Out-Null
   $range.ClearContents()|Out-Null;$range.Value2=$src.Range('A1:K'+$n).Value2
   $range.Font.Name='맑은 고딕';$range.Font.Size=11;$range.Font.Color=0;$range.Font.Bold=$false
   $range.Interior.Color=16777215;$range.WrapText=$true;$range.VerticalAlignment=-4160
   $s.Range('B30:G'+$last).HorizontalAlignment=-4108
   $s.Range('H30:K'+$last).HorizontalAlignment=-4131
   $s.Columns.Item('H').ColumnWidth=36;$s.Columns.Item('I').ColumnWidth=31
   $s.Columns.Item('J').ColumnWidth=56;$s.Columns.Item('K').ColumnWidth=64
   if($entry[0].EndsWith('_변경 비교')){
    $s.Columns.Item('A').ColumnWidth=5
    for($r=30;$r -le $last;$r++){
     $mark=[string]$s.Cells.Item($r,1).Value2
     if($mark -eq '[-]'){$s.Range('A'+$r+':K'+$r).Interior.Color=15198717}
     elseif($mark -eq '[+]'){$s.Range('A'+$r+':K'+$r).Interior.Color=15267562}
    }
   }
   $header=$author.Worksheets.Item('준비')
   for($i=0;$i -lt $plan.header.Count;$i++){
    Put $s ('C'+(15+$i)) $header.Cells.Item(1+$i,1).Value2
    Put $s ('D'+(15+$i)) $header.Cells.Item(1+$i,2).Value2
    $s.Rows.Item(15+$i).RowHeight=$(if($i -eq 0){30}else{42})
   }
   $s.Range('C15:K24').WrapText=$true
   $s.Range('C19:K19').Font.Color=255
   Fit $s 30 $last
   $s.PageSetup.PrintArea='$A$1:$K$'+($last+8);$s.PageSetup.PrintTitleRows='$28:$29'
   $s.PageSetup.Zoom=$false;$s.PageSetup.FitToPagesWide=1;$s.PageSetup.FitToPagesTall=$false
   $s.Activate();$app.ActiveWindow.FreezePanes=$false;$app.ActiveWindow.SplitRow=0;$app.ActiveWindow.SplitColumn=0
  }
  # Unit names may have been edited in the current detailed design. Keep the
  # existing list layout and code/evidence columns, update only unit display name.
  $list=$wb.Worksheets.Item('유닛 목록')
  foreach($u in $plan.units){
   for($r=2;$r -le $list.UsedRange.Rows.Count;$r++){
    if([string]$list.Cells.Item($r,4).Value2 -eq $u.su){Put $list ('E'+$r) $u.name;break}
   }
  }
  $wb.Worksheets.Item('기준서_유닛 검증').Activate();$app.CalculateFull();$wb.Save()
  $app.CutCopyMode=0
  Write-Output 'Candidate saved'
 }else{
  $app.CalculateFull()
  # Full main sheet plus each changed view. Rich OOXML runs must survive opening;
  # read-only render never rewrites them.
  $main=$wb.Worksheets.Item('기준서_유닛 검증');$main.ExportAsFixedFormat(0,(Join-Path $qa 'main-full.pdf'))
  foreach($entry in @(@('main-first','기준서_유닛 검증','B28:K34'),@('diff-first','기준서_유닛 검증_변경 비교','A28:K35'),@('equipment','기준서_유닛 검증','B13:K24'),@('numbers','기준서_유닛 검증','D46:K61'),@('hear','기준서_유닛 검증','B72:K73'),@('cm3','기준서_유닛 검증','B100:K109'),@('boot','기준서_유닛 검증','B137:K144'),@('list','유닛 목록','A1:G6'))){
   $s=$wb.Worksheets.Item($entry[1]);$s.PageSetup.PrintArea=$s.Range($entry[2]).Address()
   $s.PageSetup.PrintTitleRows='';$s.ExportAsFixedFormat(0,(Join-Path $qa ($entry[0]+'.pdf')))
  }
  Write-Output ('Excel native rendered: '+$plan.rows.Count+' tests')
 }
}finally{
 $app.CutCopyMode=0
 if($author){$author.Close($false)}
 if($wb){$wb.Close($false)}
 $app.Quit();$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
