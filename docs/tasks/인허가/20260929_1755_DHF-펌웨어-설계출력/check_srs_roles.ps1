param([Parameter(Mandatory=$true)][string]$Folder)
$ErrorActionPreference='Stop'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $Folder 'plan.json')|ConvertFrom-Json
$app=New-Object -ComObject Excel.Application
$app.Visible=$false;$app.DisplayAlerts=$false;$app.AskToUpdateLinks=$false;$app.AutomationSecurity=3
$book=$null
try{
 $book=$app.Workbooks.Open((Join-Path $Folder 'candidate.xlsx'),0,$true)
 $sheet=$book.Worksheets.Item($plan.sheet)
 $scratch=$book.Worksheets.Add()
 $checks=New-Object System.Collections.Generic.List[object]
 foreach($item in $plan.edits){
  $cell=$sheet.Range($item.cell)
  if([string]$cell.Value2 -cne [string]$item.value){throw "Text differs: $($item.cell)"}
  $font=$cell.Characters(1,([string]$cell.Value2).Length).Font
  $color=if($item.role -eq 'qcc'){192}else{12611584}
  if($font.Color -ne $color -or -not $font.Bold -or $font.Size -ne $item.size){throw "Font differs: $($item.cell) color=$($font.Color) size=$($font.Size) bold=$($font.Bold)"}
  if($item.role -eq 'qcc' -and $font.Underline -ne 2){throw "Missing underline: $($item.cell)"}
  $scratch.Cells.Clear()|Out-Null
  $cell.Copy($scratch.Range('A1'))|Out-Null
  $scratch.Columns.Item(1).ColumnWidth=$sheet.Columns.Item($cell.Column).ColumnWidth
  $scratch.Rows.Item(1).AutoFit()|Out-Null
  $checks.Add(@{cell=$item.cell;row=$cell.Row;needed=[double]$scratch.Rows.Item(1).RowHeight;current=[double]$cell.EntireRow.RowHeight;color=$font.Color;size=$font.Size;bold=$font.Bold;underline=$font.Underline})
 }
 $scratch.Delete()|Out-Null
 $checks|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $Folder 'native-check.json')
 $sheet.PageSetup.Orientation=2;$sheet.PageSetup.Zoom=$false
 $sheet.PageSetup.FitToPagesWide=1;$sheet.PageSetup.FitToPagesTall=1
 $sheet.PageSetup.PrintTitleRows='';$sheet.PageSetup.PrintTitleColumns=''
 foreach($range in @('F20:I20','F49:I57','F60:I69')){
  $sheet.PageSetup.PrintArea=$range
  $sheet.ExportAsFixedFormat(0,(Join-Path $Folder ('native-'+$range.Replace(':','_')+'.pdf')),0,$false,$false)
 }
 Write-Output ('Font checks=12; undersized='+@($checks|Where-Object {$_.needed -gt $_.current+1}).Count)
}finally{
 if($null -ne $book){$book.Close($false)}
 $app.Quit();$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
