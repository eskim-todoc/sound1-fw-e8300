param([string]$WorkbookPath)
$ErrorActionPreference='Stop'
$hereTask=$PSScriptRoot
if(-not $WorkbookPath){$WorkbookPath=Join-Path $hereTask 'candidate.xlsx'}
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $hereTask 'plan.json') | ConvertFrom-Json
$before=(Get-FileHash -Algorithm SHA256 -LiteralPath $WorkbookPath).Hash
$app=New-Object -ComObject Excel.Application
$app.Visible=$false;$app.DisplayAlerts=$false;$app.AutomationSecurity=3
$book=$null
try {
 $book=$app.Workbooks.Open($WorkbookPath,0,$true)
 $sheet=$book.Worksheets.Item('보고서_시스템 시험')
 $photos=@()
 foreach($shape in $sheet.Shapes){
   $photos+= [pscustomobject]@{name=$shape.Name;row=$shape.TopLeftCell.Row;column=$shape.TopLeftCell.Column;heightCm=([double]$shape.Height/72*2.54);widthCm=([double]$shape.Width/72*2.54);top=[double]$shape.Top;left=[double]$shape.Left;rightPt=([double]$shape.Left+[double]$shape.Width);columnRightPt=([double]$sheet.Range('K1').Left+[double]$sheet.Range('K1').Width)}
 }
 $sheet.PageSetup.PrintArea='J32:L'+$plan.rowMap.'284'.last
 $sheet.PageSetup.Zoom=$false;$sheet.PageSetup.FitToPagesWide=1;$sheet.PageSetup.FitToPagesTall=$false
 $sheet.ExportAsFixedFormat(0,(Join-Path $hereTask 'native-results.pdf'),0,$true,$false)
 $sheet.PageSetup.PrintArea='B32:L38'
 $sheet.ExportAsFixedFormat(0,(Join-Path $hereTask 'native-first-full.pdf'),0,$true,$false)
 $aux=$book.Worksheets.Item('시험 보조자료')
 $aux.PageSetup.PrintArea='A1:D12';$aux.PageSetup.Zoom=$false;$aux.PageSetup.FitToPagesWide=1;$aux.PageSetup.FitToPagesTall=$false
 $aux.ExportAsFixedFormat(0,(Join-Path $hereTask 'native-support.pdf'),0,$true,$false)
 $comp=$book.Worksheets.Item('보고서_시스템 시험_비교')
 $comp.PageSetup.PrintArea='B305:L312';$comp.PageSetup.Zoom=$false;$comp.PageSetup.FitToPagesWide=1;$comp.PageSetup.FitToPagesTall=$false
 $comp.ExportAsFixedFormat(0,(Join-Path $hereTask 'native-comparison.pdf'),0,$true,$false)
 $result=[pscustomobject]@{opened=$true;readOnly=$book.ReadOnly;worksheetCount=$book.Worksheets.Count;shapeCount=$sheet.Shapes.Count;photos=$photos;sourceSHA256=$before;printArea=$book.Worksheets.Item('보고서_시스템 시험').PageSetup.PrintArea;checkedAt=(Get-Date).ToString('o')}
 $result | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $hereTask 'native-qa.json')
} finally {
 if($null -ne $book){$book.Close($false);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($book)}
 $app.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
$after=(Get-FileHash -Algorithm SHA256 -LiteralPath $WorkbookPath).Hash
if($before -ne $after){throw 'Read-only QA changed candidate'}
Write-Output ('Native Excel read-only QA completed; source unchanged: '+$after)
