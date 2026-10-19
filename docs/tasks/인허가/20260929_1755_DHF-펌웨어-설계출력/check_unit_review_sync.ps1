param([Parameter(Mandatory=$true)][string]$Folder)
$ErrorActionPreference='Stop'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $Folder 'sheet-edits.json') | ConvertFrom-Json
$base=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $Folder 'baseline.json') | ConvertFrom-Json
$referencePath=Join-Path (Split-Path $Folder) 'audit-report-dates-format-20261012/report-before.xlsx'
$app=New-Object -ComObject Excel.Application
$app.Visible=$false
$app.DisplayAlerts=$false
$app.AskToUpdateLinks=$false
$app.AutomationSecurity=3
$books=New-Object System.Collections.Generic.List[object]
$checks=New-Object System.Collections.Generic.List[object]
try {
 $reference=$app.Workbooks.Open($referencePath,0,$true)
 $books.Add($reference)
 $refSheet=$reference.Worksheets.Item(2)
 $report=$app.Workbooks.Open((Join-Path $Folder 'report.xlsx'),0,$true)
 $books.Add($report)
 $criterion=$app.Workbooks.Open((Join-Path $Folder 'criterion/criterion-candidate.xlsx'),0,$true)
 $books.Add($criterion)
 $rs=$report.Worksheets.Item(2)
 $cs=$criterion.Worksheets.Item(2)
 $oldCriterion=$app.Workbooks.Open((Join-Path $Folder 'criterion-before.xlsx'),0,$true)
 $books.Add($oldCriterion)
 foreach($address in @('L2','B5','D11','H12','F24')){
  $before=$oldCriterion.Worksheets.Item(1).Range($address)
  $after=$criterion.Worksheets.Item(1).Range($address)
  if([string]$before.Value2 -cne [string]$after.Value2 -or [string]$before.Font.Name -cne [string]$after.Font.Name -or [double]$before.Font.Size -ne [double]$after.Font.Size){throw "Cover changed: $address"}
 }
 foreach($item in $plan.restore_fills){
  $expected=[int]$refSheet.Range($item.cell).Interior.Color
  if([int]$rs.Range($item.cell).Interior.Color -ne $expected){throw "Report fill mismatch: $($item.cell)"}
  if($rs.Range($item.cell).Column -le 11 -and [int]$cs.Range($item.cell).Interior.Color -ne $expected){throw "Criterion fill mismatch: $($item.cell)"}
 }
 foreach($name in $base.commonDifferences){
  if([string]$rs.Range($name).Value2 -cne [string]$cs.Range($name).Value2){throw "Common text mismatch: $name"}
 }
 if([string]$cs.Range('C19').Value2 -cne [string]$rs.Range('C19').Value2){throw 'QCC marker differs'}
 if([int]$cs.Range('C19').Characters(2,1).Font.Color -ne [int]$rs.Range('C19').Characters(2,1).Font.Color){throw 'QCC marker color differs'}
 if($cs.UsedRange.Columns.Count -gt 11){throw 'Result columns remain in criterion'}
 foreach($entry in @(@{key='report';book=$report;sheet=$rs;last='M'},@{key='criterion';book=$criterion;sheet=$cs;last='K'})){
  $sheet=$entry.sheet
  $scratch=$entry.book.Worksheets.Add()
  foreach($address in $base.commonDifferences){
   $cell=$sheet.Range($address)
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
   $checks.Add(@{book=$entry.key;cell=$address;needed=$needed;available=[double]$area.Height})
  }
  $scratch.Delete()|Out-Null
  $sheet.PageSetup.Orientation=2
  $sheet.PageSetup.Zoom=$false
  $sheet.PageSetup.FitToPagesWide=1
  $sheet.PageSetup.FitToPagesTall=1
  $sheet.PageSetup.PrintTitleRows=''
  $sheet.PageSetup.PrintTitleColumns=''
  foreach($pair in @(@(791,792),@(809,811),@(1212,1213))){
   $range='K'+$pair[0]+':'+$entry.last+$pair[1]
   $sheet.PageSetup.PrintArea=$range
   $sheet.ExportAsFixedFormat(0,(Join-Path $Folder ($entry.key+'-'+$range.Replace(':','_')+'.pdf')),0,$false,$false)
  }
 }
 $checks|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $Folder 'native-review-checks.json')
 if(@($checks|Where-Object {$_.needed -gt $_.available+1}).Count -gt 0){throw 'Text clipped'}
 Write-Output (@{books=2;restoredColorChecks=$plan.restore_fills.Count;textChecks=$checks.Count;maxNeeded=($checks.needed|Measure-Object -Maximum).Maximum;clippedCells=0}|ConvertTo-Json -Compress)
}finally{
 for($i=$books.Count-1;$i -ge 0;$i--){$books[$i].Close($false)}
 $app.Quit()
 $null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
