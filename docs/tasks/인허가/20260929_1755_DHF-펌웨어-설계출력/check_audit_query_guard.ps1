param([Parameter(Mandatory=$true)][string]$Folder)
$ErrorActionPreference='Stop'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $Folder 'sheet-edits.json') | ConvertFrom-Json
$app=New-Object -ComObject Excel.Application
$app.Visible=$false;$app.DisplayAlerts=$false;$app.AskToUpdateLinks=$false;$app.AutomationSecurity=3
$book=$null
try {
 foreach($kind in @('suvr','suvc')){
  $book=$app.Workbooks.Open((Join-Path $Folder ($kind+'.xlsx')),0,$true)
  $sheet=$book.Worksheets.Item(2)
  $scratch=$book.Worksheets.Add()
  $checks=New-Object System.Collections.Generic.List[object]
  $properties=@($plan.common.PSObject.Properties)
  if($kind -eq 'suvr'){$properties+=@($plan.report.PSObject.Properties)}
  foreach($entry in $properties){
   $cell=$sheet.Range($entry.Name)
   if([string]$cell.Value2 -cne [string]$entry.Value){throw "Text differs: $kind $($entry.Name)"}
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
   $idx=([string]$cell.Value2).IndexOf('[E8300 ')
   if($idx -ge 0){
    $font=$cell.Characters($idx+1,6).Font
    if($font.Color -ne 12611584 -or -not $font.Bold){throw "E8300 emphasis: $kind $($entry.Name)"}
   }
   $checks.Add(@{cell=$entry.Name;needed=$needed;available=[double]$area.Height;rowHeight=[double]$cell.EntireRow.RowHeight})
  }
  $scratch.Delete()|Out-Null
  foreach($addr in @('C19','D19')){
   $font=$sheet.Range($addr).Characters(2,5).Font
   if($font.Color -ne 192 -or -not $font.Bold -or $font.Size -ne 12 -or $font.Underline -ne 2){throw "QCC style $kind $addr"}
  }
  $checks|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $Folder ($kind+'-native-check.json'))
  $sheet.PageSetup.Orientation=2;$sheet.PageSetup.Zoom=$false
  $sheet.PageSetup.FitToPagesWide=1;$sheet.PageSetup.FitToPagesTall=1
  $sheet.PageSetup.PrintTitleRows='';$sheet.PageSetup.PrintTitleColumns=''
  $ranges=if($kind -eq 'suvr'){@('H809:J811','K809:M811')}else{@('H809:K811')}
  foreach($range in $ranges){
   $sheet.PageSetup.PrintArea=$range
   $sheet.ExportAsFixedFormat(0,(Join-Path $Folder ($kind+'-native-'+$range.Replace(':','_')+'.pdf')),0,$false,$false)
  }
  Write-Output ($kind+' undersized='+@($checks|Where-Object {$_.needed -gt $_.available+1}).Count+' maximum required='+($checks|Measure-Object -Property needed -Maximum).Maximum)
  $book.Close($false);$book=$null
 }
}finally{
 if($null -ne $book){$book.Close($false)}
 $app.Quit();$null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
