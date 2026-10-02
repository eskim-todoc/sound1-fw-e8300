$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa'
$dest=Join-Path (Get-Location) 'docs\참고\인허가\사운드1\DHF\02_설계 입력\SW\펌웨어'
$excel=New-Object -ComObject Excel.Application
$excel.Visible=$false;$excel.DisplayAlerts=$false;$excel.AskToUpdateLinks=$false;$excel.AutomationSecurity=3
try {
 foreach($file in Get-ChildItem -LiteralPath $dest -Filter '*.xlsx') {
  $book=$null
  try {
   $book=$excel.Workbooks.Open($file.FullName,0,$true)
   foreach($sheet in $book.Worksheets) {
    $name=$file.BaseName.Split('-')[0]+'-'+$sheet.Index
    # QA print copies only; delivered workbook print settings remain original.
    $sheet.PageSetup.PrintArea=if($sheet.Index -eq 1){'$B$2:$O$37'}elseif($name.StartsWith('SRS')){'$B$3:$I$91'}else{'$B$22:$J$104'}
    $sheet.PageSetup.Zoom=$false
    $sheet.PageSetup.FitToPagesWide=1
    $sheet.PageSetup.FitToPagesTall=$false
    $sheet.PageSetup.Orientation=if($sheet.Index -eq 1){1}else{2}
    $sheet.PageSetup.PaperSize=if($name -eq 'SSTC-2'){8}else{9}
    $sheet.ExportAsFixedFormat(0,(Join-Path $qa ($name+'.pdf')))
   }
   Write-Output ('Excel rendered: '+$file.Name)
  } finally {if($null -ne $book){$book.Close($false);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($book)}}
 }
} finally {$excel.Quit();$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($excel)}
$power=New-Object -ComObject PowerPoint.Application
try {
 $deck=$power.Presentations.Open((Join-Path $qa '인공달팽이관 시스템 구성도.pptx'),$true,$false,$false)
 try {
  $png=Join-Path $qa 'slides';$null=New-Item -ItemType Directory -Path $png -Force
  foreach($slide in $deck.Slides){$slide.Export((Join-Path $png ('slide-'+$slide.SlideIndex+'.png')),'PNG',1280,720)}
  Write-Output 'PowerPoint rendered: 7 slides'
 } finally {$deck.Close();$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($deck)}
} finally {$power.Quit();$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($power)}
