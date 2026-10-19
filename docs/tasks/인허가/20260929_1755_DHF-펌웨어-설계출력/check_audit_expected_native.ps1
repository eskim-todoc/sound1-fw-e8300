param([Parameter(Mandatory=$true)][string]$Folder, [switch]$DatesFormat)
$ErrorActionPreference='Stop'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $Folder 'sheet-edits.json') | ConvertFrom-Json
$app=New-Object -ComObject Excel.Application
$app.Visible=$false
$app.DisplayAlerts=$false
$app.AskToUpdateLinks=$false
$app.AutomationSecurity=3
$book=$null
try {
 $book=$app.Workbooks.Open((Join-Path $Folder 'report.xlsx'),0,$true)
 $sheet=$book.Worksheets.Item(2)
 $scratch=$book.Worksheets.Add()
 $checks=New-Object System.Collections.Generic.List[object]
 foreach($entry in $plan.edits.PSObject.Properties){
  $cell=$sheet.Range($entry.Name)
  if([string]$cell.Value2 -cne [string]$entry.Value){throw "Text differs: $($entry.Name)"}
  $area=$cell.MergeArea
  $scratch.Cells.Clear()|Out-Null
  $cell.Copy($scratch.Range('A1'))|Out-Null
  $scratch.UsedRange.UnMerge()|Out-Null
  $scratch.Columns.Item(1).ColumnWidth=[double]$sheet.Columns.Item($cell.Column).ColumnWidth
  for($i=0;$i -lt 3;$i++){
   $scratch.Columns.Item(1).ColumnWidth=[double][Math]::Min(255,[double]$scratch.Columns.Item(1).ColumnWidth*[double]$area.Width/[double]$scratch.Columns.Item(1).Width)
  }
  $scratch.Rows.Item(1).AutoFit()|Out-Null
  $needed=[double]$scratch.Rows.Item(1).RowHeight
  # Excel caps AutoFit at 409.5pt. Measure two text halves when that cap is reached.
  if($needed -ge 409.5){
   $lines=([string]$cell.Value2).Split("`n")
   $mid=[int][Math]::Floor($lines.Length/2)
   $scratch.Range('A1').Copy($scratch.Range('A2'))|Out-Null
   $scratch.Range('A1').Value2=($lines[0..($mid-1)] -join "`n")
   $scratch.Range('A2').Value2=($lines[$mid..($lines.Length-1)] -join "`n")
   $scratch.Rows.Item(1).AutoFit()|Out-Null
   $scratch.Rows.Item(2).AutoFit()|Out-Null
   $needed=[double]$scratch.Rows.Item(1).RowHeight+[double]$scratch.Rows.Item(2).RowHeight
  }
  $idx=([string]$cell.Value2).IndexOf('[E8300 보완]')
  if($idx -ge 0){
   if($DatesFormat){throw "Unexpected old prefix: $($entry.Name)"}
   $font=$cell.Characters($idx+1,10).Font
   if($font.Color -ne 12611584 -or -not $font.Bold){throw "Missing emphasis: $($entry.Name)"}
  }
  $checks.Add(@{cell=$entry.Name;needed=$needed;available=[double]$area.Height;rowHeight=[double]$cell.EntireRow.RowHeight})
 }
 if($DatesFormat){
  foreach($range in $plan.fills){
   $anchor=([string]$range).Split(':')[0]
   if([int]$sheet.Range($anchor).Interior.Color -ne 0xCCF2FF){throw "Missing yellow background: $anchor"}
  }
  foreach($address in $plan.clear_marker_fonts){
   $textCell=$sheet.Range($address)
   if([int]$textCell.Characters(2,1).Font.Color -ne 0){throw "Old font emphasis remains: $address"}
  }
 }
 $scratch.Delete()|Out-Null
 $checks|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $Folder 'native-checks.json')
 $sheet.PageSetup.Orientation=2
 $sheet.PageSetup.Zoom=$false
 $sheet.PageSetup.FitToPagesWide=1
 $sheet.PageSetup.FitToPagesTall=1
 $sheet.PageSetup.PrintTitleRows=''
 $sheet.PageSetup.PrintTitleColumns=''
 foreach($range in @('K791:M792','K809:M811','K1212:M1213')){
  $sheet.PageSetup.PrintArea=$range
  $sheet.ExportAsFixedFormat(0,(Join-Path $Folder ('native-'+$range.Replace(':','_')+'.pdf')),0,$false,$false)
 }
 Write-Output ($checks|ConvertTo-Json -Compress)
 if(@($checks|Where-Object {$_.needed -gt $_.available+1}).Count -gt 0){throw 'Text requires greater row height'}
}finally{
 if($null -ne $book){$book.Close($false)}
 $app.Quit()
 $null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
