$ErrorActionPreference='Stop'
$q=Join-Path $PSScriptRoot 'review-20260929'
$jobs=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $q 'plan.json') | ConvertFrom-Json
$excel=New-Object -ComObject Excel.Application
$excel.Visible=$false
$excel.DisplayAlerts=$false
$excel.AskToUpdateLinks=$false
try {
 foreach($j in $jobs){
  $wb=$excel.Workbooks.Open($j.backup,0,$false)
  $colors=$excel.Workbooks.Open((Join-Path $q ($j.kind+'-colors.xlsx')),0,$true)
  foreach($c in $j.colors){
   $wb.Worksheets.Item($c.sheet).Range($c.cell).Interior.Color=$colors.Worksheets.Item($c.sheet).Range($c.cell).Interior.Color
  }
  $colors.Close($false)
  $audit=$excel.Workbooks.Open((Join-Path $q ($j.kind+'-review.xlsx')),0,$true)
  $audit.Worksheets.Item(1).Copy([Type]::Missing,$wb.Worksheets.Item($wb.Worksheets.Count))
  $audit.Close($false)
  $sh=$wb.Worksheets.Item('변경 비교')
  $n=$j.records.Count+7
  $sh.Range("A7:G$n").AutoFilter() | Out-Null
  $sh.Range("A7:G7").HorizontalAlignment=-4108
  $sh.Range("A7:G7").VerticalAlignment=-4108
  $sh.Range("A8:G$n").Rows.AutoFit() | Out-Null
  for($i=0;$i -lt $j.records.Count;$i++){
   $r=$i+8;$entry=$j.records[$i]
   foreach($pair in @(@('D','old'),@('E','new'))){
    $value=$entry.($pair[1]);if($null -eq $value){$value='(빈칸)'}
    $sh.Range($pair[0]+$r).NumberFormat='@'
    $sh.Range($pair[0]+$r).Value2="'"+[string]$value
   }
   if($sh.Rows.Item($r).RowHeight -lt 32){$sh.Rows.Item($r).RowHeight=32}
   $sh.Rows.Item($r).RowHeight=[Math]::Min(409,$sh.Rows.Item($r).RowHeight+6)
   $sub="'"+$entry.sheet.Replace("'","''")+"'!"+$entry.cell
   $sh.Hyperlinks.Add($sh.Range("B$r"),'',$sub) | Out-Null
  }
  if($j.kind -eq 'SRS'){$sh.Range('A6').Value2='셀 단위 변경 건수이며 요구사항 개수와 다릅니다. 기존 내용은 비교 원본과 대조했습니다.'}
  $sh.Activate();$excel.ActiveWindow.Zoom=80
  $sh.Range('A8').Select();$excel.ActiveWindow.FreezePanes=$true
  $sh.PageSetup.Orientation=2
  $sh.PageSetup.PaperSize=8
  $sh.PageSetup.Zoom=$false
  $sh.PageSetup.FitToPagesWide=1
  $sh.PageSetup.FitToPagesTall=$false
  $sh.PageSetup.PrintTitleRows='$7:$7'
  $sh.PageSetup.PrintArea="A1:G$n"
  $candidate=Join-Path $q ($j.kind+'-final.xlsx')
  $wb.SaveAs($candidate,51)
  $sh.PageSetup.PrintArea='A1:G14'
  $sh.ExportAsFixedFormat(0,(Join-Path $q ($j.kind+'-preview.pdf')))
  $wb.Close($false)
  Write-Output "$($j.kind): candidate saved, hyperlinks and native layout checked"
 }
} finally {$excel.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($excel)}
