$ErrorActionPreference='Stop'
$qa=Join-Path $PSScriptRoot 'qa\unit-reconciliation'
$app=New-Object -ComObject Excel.Application
$app.Visible=$false;$app.DisplayAlerts=$false;$app.EnableEvents=$false;$app.AutomationSecurity=3
$book=$null
try {
 $book=$app.Workbooks.Open((Join-Path $qa 'SUVC-candidate.xlsx'),0,$true)
 $s=$book.Worksheets.Item('기준서_유닛 검증_변경 비교')
 $s.PageSetup.PrintArea='$A$28:$K$37'
 $s.ExportAsFixedFormat(0,(Join-Path $qa 'SUVC-rich-diff.pdf'))
 $book.Close($false);$book=$null
 $book=$app.Workbooks.Open((Join-Path $qa 'SITC-candidate.xlsx'),0,$false)
 $plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'tables-plan.json')|ConvertFrom-Json
 foreach($group in ($plan.sitc | Group-Object sheet)){
  $s=$book.Worksheets.Item($group.Name)
  foreach($r in ($group.Group.cell | ForEach-Object {[int]($_ -replace '[A-Z]','')} | Sort-Object -Unique)){
   $oldHeight=[double]$s.Rows.Item($r).RowHeight
   $s.Rows.Item($r).AutoFit()|Out-Null
   $s.Rows.Item($r).RowHeight=[Math]::Min(409,[Math]::Max($oldHeight,[double]$s.Rows.Item($r).RowHeight+10))
  }
 }
 $book.Save()
 foreach($s in @($book.Worksheets.Item(2),$book.Worksheets.Item(3))){
  $s.PageSetup.PrintArea='$A$23:$M$'+$(if($s.Index -eq 2){41}else{58})
  $s.ExportAsFixedFormat(0,(Join-Path $qa ('SITC-final-'+$s.Index+'.pdf')))
 }
} finally {
 if($book){$book.Close($false)}
 $app.Quit();$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
# Rejection is tested on a QA-only copy, never on the live tracked deliverable.
$word=New-Object -ComObject Word.Application
$word.Visible=$false;$word.DisplayAlerts=0;$word.AutomationSecurity=3
$doc=$null
try {
 $path=Join-Path (Get-Location) 'docs\참고\인허가\사운드1\DHF\03_설계 출력\SW\펌웨어\SDDD-FW-TD24A 소프트웨어 상세 설계서.docx'
 $doc=$word.Documents.Open($path,$false,$true,$false)
 $doc.RejectAllRevisions()
 $doc.SaveAs2((Join-Path $qa 'SDDD-rejected-qa.docx'),16)
} finally {
 if($doc){$doc.Close(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($doc)}
 $word.Quit(0);$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($word)
}
Write-Output 'QA workbook row-height repair, review exports and rejection QA copy completed.'
