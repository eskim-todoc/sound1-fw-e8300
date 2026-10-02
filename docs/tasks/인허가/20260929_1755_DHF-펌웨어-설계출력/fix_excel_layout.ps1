$ErrorActionPreference='Stop'
$task=$PSScriptRoot
$plans=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $task 'test_plan.json')|ConvertFrom-Json
$dest=Join-Path (Get-Location) 'docs\참고\인허가\사운드1\DHF\03_설계 출력\SW\펌웨어'
$app=New-Object -ComObject Excel.Application
$app.Visible=$false;$app.DisplayAlerts=$false;$app.EnableEvents=$false;$app.AutomationSecurity=3
try{
 foreach($p in $plans){
  $wb=$app.Workbooks.Open((Join-Path $dest $p.name),0,$false)
  try{
   $cover=$wb.Worksheets.Item(1)
   for($i=$cover.Shapes.Count;$i -ge 1;$i--){
    $shape=$cover.Shapes.Item($i);$anchor=$shape.TopLeftCell
    if($anchor.Row -ge 11 -and $anchor.Row -le 17 -and $anchor.Column -ge 9 -and $anchor.Column -le 12){Write-Output ('Removed copied signature: '+$p.kind+' '+$shape.Name);$shape.Delete()}
   }
   foreach($s in @($wb.Worksheets.Item($p.sheet),$wb.Worksheets.Item($p.sheet+'_변경 비교'))){
    $comp=$s.Name.EndsWith('_변경 비교');$n=$(if($comp){$p.diffphysical.Count}else{$p.physical.Count})
    $range=$s.Range($s.Cells.Item([int]$p.start,2),$s.Cells.Item([int]$p.start+$n-1,[int]$p.cols))
    $range.Borders.LineStyle=1;$range.Borders.Weight=2;$range.Borders.Color=0
    $range.Rows.AutoFit()|Out-Null
    for($r=[int]$p.start;$r -lt ([int]$p.start+$n);$r++){$s.Rows.Item($r).RowHeight=[Math]::Min(409,[double]$s.Rows.Item($r).RowHeight+5)}
    if($comp){for($i=0;$i -lt $n;$i++){$mark=$p.diffphysical[$i].mark;if($mark){$s.Range($s.Cells.Item([int]$p.start+$i,1),$s.Cells.Item([int]$p.start+$i,[int]$p.cols)).Interior.Color=$(if($mark -eq '[-]'){0xD6E4FC}else{0xD9F0E2})}}}
   }
   $wb.Save()
   foreach($s in @($cover,$wb.Worksheets.Item($p.sheet),$wb.Worksheets.Item($p.sheet+'_변경 비교'))){
    if($s.Index -ne 1){$s.PageSetup.PrintArea=$s.Range($s.Cells.Item([int]$p.start-2,1),$s.Cells.Item([int]$p.start+5,[int]$p.cols)).Address()}
    $s.ExportAsFixedFormat(0,(Join-Path $task ('qa\'+$p.kind+'-'+$s.Index+'.pdf')))
   }
   if($p.kind -eq 'SUVC'){
    $s=$wb.Worksheets.Item($p.sheet);$s.PageSetup.PrintArea='$B$98:$K$108'
    $s.ExportAsFixedFormat(0,(Join-Path $task 'qa\SUVC-vectors.pdf'))
   }
  }finally{$wb.Close($false)}
 }
}finally{$app.Quit();$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
