param([Parameter(Mandatory=$true)][string]$Folder)
$ErrorActionPreference='Stop'
$excel=New-Object -ComObject Excel.Application
$excel.Visible=$false
$excel.DisplayAlerts=$false
$excel.AutomationSecurity=3
$book=$null
try {
 foreach($kind in @('suvr','suvc')) {
  $book=$excel.Workbooks.Open((Join-Path $Folder ($kind+'.xlsx')),0,$true)
  $sheet=$book.Worksheets.Item(2)
  $checks=New-Object System.Collections.Generic.List[object]
  foreach($addr in @('C19','D19','I809','L792','L1213','M789','M1208')) {
   if($kind -eq 'suvc' -and $addr -match '^[LM]'){continue}
   $cell=$sheet.Range($addr)
   $txt=[string]$cell.Value2
   $idx=$txt.IndexOf('[E8300 ')
   if($addr -in @('C19','D19')) {
    $font=$cell.Characters(2,5).Font
    if($font.Color -ne 192 -or $font.Bold -ne $true -or $font.Size -ne 12 -or $font.Underline -ne 2){throw "QCC format $kind/$addr color=$($font.Color) bold=$($font.Bold) size=$($font.Size) underline=$($font.Underline)"}
    $checks.Add(@{cell=$addr;color=$font.Color;size=$font.Size;bold=$font.Bold;underline=$font.Underline})
   } elseif($idx -ge 0) {
    $font=$cell.Characters($idx+1,6).Font
    if($font.Color -ne 12611584 -or $font.Bold -ne $true){throw "E8300 format $kind/$addr color=$($font.Color)"}
    $checks.Add(@{cell=$addr;color=$font.Color;bold=$font.Bold})
   }
  }
  $checks|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $Folder ($kind+'-native-format.json'))
  $sheet.PageSetup.Orientation=2
  $sheet.PageSetup.Zoom=$false
  $sheet.PageSetup.FitToPagesWide=1
  $sheet.PageSetup.FitToPagesTall=1
  $sheet.PageSetup.PrintTitleRows=''
  $sheet.PageSetup.PrintTitleColumns=''
  $ranges=if($kind -eq 'suvr'){@('C19:F19','K789:M792','H809:M811','K1208:M1213')}else{@('C19:F19','H789:K792','H809:K811','H1208:K1213')}
  foreach($range in $ranges) {
   $sheet.PageSetup.PrintArea=$range
   $sheet.ExportAsFixedFormat(0,(Join-Path $Folder ($kind+'-native-'+$range.Replace(':','_')+'.pdf')),0,$false,$false)
  }
  Write-Output ($kind+' native load / rich-text check / exports complete')
  $book.Close($false);$book=$null
 }
} finally {
 if($null -ne $book){$book.Close($false)}
 $excel.Quit();$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($excel)
}

$word=New-Object -ComObject Word.Application
$word.Visible=$false;$word.DisplayAlerts=0;$word.AutomationSecurity=3
$doc=$null
try {
 $doc=$word.Documents.Open((Join-Path $Folder 'sddd-before.docx'),$false,$true,$false)
 $doc.RejectAllRevisions();$doc.TrackRevisions=$false
 $doc.SaveAs2((Join-Path $Folder 'sddd-before-rejected.docx'),16)
 $doc.Close(0);$doc=$null
 Write-Output 'SDDD original rejection saved'
} finally {
 if($null -ne $doc){$doc.Close(0)}
 $word.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($word)
}
