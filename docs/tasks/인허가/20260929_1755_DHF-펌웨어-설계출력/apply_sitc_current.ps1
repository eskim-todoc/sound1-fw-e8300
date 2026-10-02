param([switch]$RenderOnly)
$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa\sitc-current-design'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'plan.json')|ConvertFrom-Json
$candidate=Join-Path $qa 'candidate.xlsx'
if(-not $RenderOnly){Copy-Item -LiteralPath (Join-Path $qa 'before.xlsx') -Destination $candidate}
$app=New-Object -ComObject Excel.Application
$app.Visible=$false;$app.DisplayAlerts=$false;$app.EnableEvents=$false;$app.AutomationSecurity=3
$wb=$null;$author=$null
try{
 $wb=$app.Workbooks.Open($candidate,0,[bool]$RenderOnly)
 if(-not $RenderOnly){
  $author=$app.Workbooks.Open((Join-Path $qa 'authored.xlsx'),0,$true)
  foreach($entry in @(@('기준서_통합 시험','본문',17),@('기준서_통합 시험_변경 비교','비교',34))){
   $s=$wb.Worksheets.Item($entry[0]);$src=$author.Worksheets.Item($entry[1]);$n=[int]$entry[2];$last=24+$n
   $oldLast=[int]$s.UsedRange.Rows.Count
   if($last -gt $oldLast){$s.Rows.Item(($oldLast+1).ToString()+':'+$last).Insert()|Out-Null}
   elseif($last -lt $oldLast){$s.Rows.Item(($last+1).ToString()+':'+$oldLast).Delete()|Out-Null}
   $range=$s.Range('A25:M'+$last);$range.UnMerge();$s.Range('A25:M25').Copy()|Out-Null;$range.PasteSpecial(-4122)|Out-Null
   $range.ClearContents()|Out-Null;$range.Value2=$src.Range('A1:M'+$n).Value2
   $range.Font.Name='맑은 고딕';$range.Font.Size=11;$range.Font.Color=0;$range.Font.Bold=$false
   $range.Interior.Color=16777215;$range.WrapText=$true;$range.VerticalAlignment=-4160
   $s.Range('B25:I'+$last).HorizontalAlignment=-4108;$s.Range('J25:M'+$last).HorizontalAlignment=-4131
   # Keep the original column layout; fit only changed test/equipment rows.
   $s.Rows.Item('25:'+$last).AutoFit()|Out-Null
   for($r=25;$r -le $last;$r++){
    $h=[Math]::Max(50,[double]$s.Rows.Item($r).RowHeight+12)
    if($h -gt 409){throw "Continued row required: $($s.Name) $r $h"}
    $s.Rows.Item($r).RowHeight=$h
    if($entry[0].EndsWith('_변경 비교')){
      $s.Range('A'+$r+':M'+$r).Interior.Color=$(if($r%2 -eq 1){15198717}else{15267562})
    }
   }
   $header=$author.Worksheets.Item('준비')
   for($r=15;$r -le 21;$r++){
    if($r -ge 20){$s.Range('C18:H18').Copy()|Out-Null;$s.Range('C'+$r+':H'+$r).PasteSpecial(-4122)|Out-Null;$s.Range('D'+$r+':H'+$r).Merge()}
    $s.Cells.Item($r,3).Value2=$header.Cells.Item($r-14,1).Value2
    $s.Cells.Item($r,4).Value2=$header.Cells.Item($r-14,2).Value2
    $s.Range('C'+$r+':H'+$r).WrapText=$true;$s.Rows.Item($r).RowHeight=$(if($r -eq 15){24}elseif($r -eq 17){60}else{42})
   }
   $s.Range('C19:H19').Font.Color=255
   $s.Cells.Item(23,4).Value2='검증할 소프트웨어 항목 ID'
   $s.Range('E23:E24').Merge();$s.Cells.Item(23,5).Value2='시험 항목'
   $s.Range('E23:E24').HorizontalAlignment=-4108;$s.Range('E23:E24').VerticalAlignment=-4108
   $s.PageSetup.PrintArea='$A$1:$M$'+$last;$s.PageSetup.PrintTitleRows='$23:$24'
   $s.PageSetup.Zoom=$false;$s.PageSetup.FitToPagesWide=1;$s.PageSetup.FitToPagesTall=$false
  }
  $wb.Worksheets.Item('기준서_통합 시험').Activate();$app.CalculateFull();$wb.Save();$app.CutCopyMode=0
  Write-Output 'Candidate saved; original layout/IDs retained'
 }else{
  $app.CalculateFull()
  $main=$wb.Worksheets.Item('기준서_통합 시험');$main.ExportAsFixedFormat(0,(Join-Path $qa 'main-full.pdf'))
  $layout=@()
  for($r=25;$r -le 41;$r++){$layout+=@{row=$r;height=[double]$main.Rows.Item($r).RowHeight;test=[string]$main.Cells.Item($r,3).Value2}}
  $layout|ConvertTo-Json -Depth 4|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'native-layout.json')
  foreach($entry in @(@('equipment','기준서_통합 시험','B15:M21'),@('diff-first','기준서_통합 시험_변경 비교','A23:M28'),@('main-first','기준서_통합 시험','B23:M26'),@('main-audio','기준서_통합 시험','B29:M30'),@('main-qcc','기준서_통합 시험','B36:M38'),@('main-nofm','기준서_통합 시험','B39:M39'),@('main-boot','기준서_통합 시험','B40:M41'))){
   $s=$wb.Worksheets.Item($entry[1]);$s.PageSetup.PrintArea=$s.Range($entry[2]).Address();$s.PageSetup.PrintTitleRows=''
   $s.ExportAsFixedFormat(0,(Join-Path $qa ($entry[0]+'.pdf')))
  }
  Write-Output 'Full main and comparison details rendered without saving'
 }
}finally{
 $app.CutCopyMode=0
 if($author){$author.Close($false)}
 if($wb){$wb.Close($false)}
 $app.Quit();$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
