param([string]$Only='*',[string]$Iteration='1')
$ErrorActionPreference='Stop'
$task=$PSScriptRoot
$plans=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $task 'test_plan.json') | ConvertFrom-Json
$dest=Join-Path (Get-Location) 'docs\참고\인허가\사운드1\DHF\03_설계 출력\SW\펌웨어'
$app=New-Object -ComObject Excel.Application
$app.Visible=$false;$app.DisplayAlerts=$false;$app.EnableEvents=$false;$app.AutomationSecurity=3
function Put($sheet,$addr,$value){$cell=$sheet.Range($addr);if($null -eq $value){if($cell.MergeCells){$cell.MergeArea.ClearContents()|Out-Null}else{$cell.ClearContents()|Out-Null}}elseif($value -is [ValueType]){$cell.Value2=[double]$value}else{$cell.Value2="'$value"}}
function FitRows($sheet,$start,$count,$cols){
 for($r=$start;$r -lt ($start+$count);$r++){
  $max=2
  for($c=2;$c -le $cols;$c++){
   $text=[string]$sheet.Cells.Item($r,$c).Value2
   $width=[double]$sheet.Cells.Item($r,$c).Width
   $lines=0
   foreach($line in $text.Split("`n")){
    $cost=0;foreach($ch in $line.ToCharArray()){$cost+= $(if([int]$ch -gt 127){8.8}else{4.7})}
    $lines += [Math]::Max(1,[Math]::Ceiling($cost/[Math]::Max(20,$width-10)))
   }
   $max=[Math]::Max($max,$lines)
  }
  $sheet.Rows.Item($r).RowHeight=[Math]::Min(409,[Math]::Max(35,($max*13)+12))
 }
}
try{
 foreach($p in $plans){
  if($p.kind -notlike $Only){continue}
  $out=Join-Path $dest $p.name
  if(Test-Path -LiteralPath $out){if((Get-FileHash -LiteralPath $out).Hash -ne (Get-FileHash -LiteralPath $p.source).Hash){
   $previous=Join-Path $task ('qa\previous-'+$Iteration+'-'+$p.name)
   if(Test-Path -LiteralPath $previous){throw "Previous already exists: $previous"}
   Copy-Item -LiteralPath $out -Destination $previous
  }}
  Copy-Item -LiteralPath $p.source -Destination $out
  $wb=$null;$author=$null
  try{
   $wb=$app.Workbooks.Open($out,0,$false)
   $author=$app.Workbooks.Open((Join-Path $task ($p.kind+'-authored.xlsx')),0,$true)
   $cover=$wb.Worksheets.Item(1);$sheet=$wb.Worksheets.Item($p.sheet)
   Put $cover 'L2' ($p.kind+'-FW-TD24A');Put $cover 'L3' 'Rev.0'
   Put $cover 'B5' (([string]$cover.Range('B5').Value2).Replace('TD20B-A-EXT/TD23D-A-EXT','TD24A-A-EXT').Replace('TD20B','TD24A').Replace('TD23D','TD24A'))
   Put $cover 'D12' '수석연구원';Put $cover 'H12' '김은수'
   foreach($cell in @('J12','M12','D14','H14','J14','M14','D16','H16','J16','M16','D24')){Put $cover $cell $null}
   Put $cover 'B24' 0
   # Clear old subsequent revision data, retaining the revision grid.
   foreach($r in 25..30){foreach($c in @('B','D','F')){Put $cover ($c+$r) $null}}
   # A fresh source file remains in its original location. Both views use copied native source formatting.
   $sheet.Copy([Type]::Missing,$sheet)
   $comparison=$wb.Worksheets.Item($sheet.Index+1);$comparison.Name=$p.sheet+'_변경 비교'
   foreach($s in @($sheet,$comparison)){
    Put $s 'D7' 'TD24A-A-EXT';Put $s 'D8' $null
    Put $s 'C11' '인공달팽이관 외부장치 Sound1 (TD24A-A-EXT)'
    if($p.kind -in @('SUVC','SITC')){
      Put $s 'C18' 'QCC 연계 시험 장치';Put $s 'D18' 'SPI 명령 및 I2S 오디오 입력/응답 확인'
      Put $s 'C19' 'QCC 펌웨어 자료';Put $s 'D19' 'QCC 내부 설계와 유닛 시험은 자료 확보 후 별도 추가'
      Put $s 'C21' 'I2S 오디오 입력 장치';Put $s 'D21' '외부 오디오 입력 경로 확인'
    }
   }
   $count=$p.physical.Count;$dc=$p.diffphysical.Count
   foreach($pair in @(@($sheet,$count,1),@($comparison,$dc,2))){
    $s=$pair[0];$n=[int]$pair[1];$as=$author.Worksheets.Item([int]$pair[2]);$start=[int]$p.start;$end=[int]$p.end
    $s.Range($s.Cells.Item($start,1),$s.Cells.Item($end,[int]$p.cols)).UnMerge()
    $originalCount=$end-$start+1
    if($n -gt $originalCount){$s.Rows.Item(($end+1).ToString()+':'+($end+$n-$originalCount)).Insert()|Out-Null}
    $target=$s.Range($s.Cells.Item($start,1),$s.Cells.Item($start+$n-1,[int]$p.cols))
    $template=$s.Range($s.Cells.Item($start,1),$s.Cells.Item($start,[int]$p.cols))
    $template.Copy()|Out-Null;$target.PasteSpecial(-4122)|Out-Null
    $target.ClearContents()|Out-Null
    $target.Value2=$as.Range($as.Cells.Item(1,1),$as.Cells.Item($n,[int]$p.cols)).Value2
    $target.WrapText=$true;$target.VerticalAlignment=-4160
    if($s.Name.EndsWith('_변경 비교')){
      $s.Columns.Item(1).ColumnWidth=5
      for($i=0;$i -lt $dc;$i++){
       $mark=$p.diffphysical[$i].mark
       if($mark){$s.Range($s.Cells.Item($start+$i,1),$s.Cells.Item($start+$i,[int]$p.cols)).Interior.Color=$(if($mark -eq '[-]'){14277081}else{14286818})}
      }
    }
    FitRows $s $start $n ([int]$p.cols)
    $s.PageSetup.PrintArea=$s.Range($s.Cells.Item(1,1),$s.Cells.Item($start+$n-1,[int]$p.cols)).Address()
    $s.PageSetup.PrintTitleRows='$'+($start-2)+':$'+($start-1)
    $s.PageSetup.Zoom=$false;$s.PageSetup.FitToPagesWide=1;$s.PageSetup.FitToPagesTall=$false
   }
   $app.CalculateFull();$wb.Save()
   foreach($s in @($cover,$sheet,$comparison)){
    $saved=$s.PageSetup.PrintArea
    if($s.Name -ne $cover.Name){$s.PageSetup.PrintArea=$s.Range($s.Cells.Item([int]$p.start-2,1),$s.Cells.Item([int]$p.start+5,[int]$p.cols)).Address()}
    $s.ExportAsFixedFormat(0,(Join-Path $task ('qa\'+$p.kind+'-'+$s.Index+'.pdf')))
    $s.PageSetup.PrintArea=$saved
   }
   Write-Output ($p.kind+': tests='+$p.rows.Count+', rows='+$count+', compared='+$dc)
  }finally{if($author){$author.Close($false)};if($wb){$wb.Close($false)}}
 }
}finally{$app.Quit();$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)}
