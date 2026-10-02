param([string]$Stage='before')
$taskDir=$PSScriptRoot
$rootDir=(Get-Item $taskDir).Parent.Parent.Parent.Parent.FullName
$file=Join-Path $rootDir 'docs\참고\인허가\사운드1\DHF\03_설계 출력\SW\펌웨어\SSTC-FW-TD24A 소프트웨어 시스템 시험 기준서.xlsx'
$qa=Join-Path $taskDir 'qa\sstc-id63'
New-Item -ItemType Directory -Path $qa -Force | Out-Null
$app=New-Object -ComObject Excel.Application
$app.Visible=$false
$app.DisplayAlerts=$false
try {
 $book=$app.Workbooks.Open($file,0,$true)
 foreach($spec in @(@('기준서_시스템 시험','G109:J109','body'),@('기준서_시스템 시험_변경 비교','G171:J172','diff'))) {
  $sheet=$book.Worksheets.Item($spec[0])
  $sheet.PageSetup.PrintArea=$spec[1]
  $sheet.PageSetup.PrintTitleRows=''
  $sheet.PageSetup.Zoom=$false
  $sheet.PageSetup.FitToPagesWide=1
  $sheet.PageSetup.FitToPagesTall=1
  $sheet.ExportAsFixedFormat(0,(Join-Path $qa ($Stage+'-'+$spec[2]+'.pdf')))
 }
} finally { if($book){$book.Close($false)}; $app.Quit() }
