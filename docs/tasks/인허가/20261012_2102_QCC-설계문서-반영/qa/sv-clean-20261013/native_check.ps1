param([Parameter(Mandatory=$true)][string]$PlanPath)
$ErrorActionPreference='Stop'
$svNative=Get-Content -Raw -Encoding UTF8 -LiteralPath $PlanPath | ConvertFrom-Json
$svRecords=New-Object System.Collections.Generic.List[object]
$svExcel=New-Object -ComObject Excel.Application
$svExcel.Visible=$false
$svExcel.DisplayAlerts=$false
$svExcel.AutomationSecurity=3
try {
 foreach($svItem in $svNative.books){
  $svWorkbook=$null
  try {
   $svHash=(Get-FileHash -LiteralPath $svItem.path).Hash
   if($svHash -ne $svItem.sha256){throw 'Candidate hash changed'}
   $svWorkbook=$svExcel.Workbooks.Open([string]$svItem.path,0,$true)
   $svTabs=New-Object System.Collections.Generic.List[object]
   foreach($svSheet in $svWorkbook.Worksheets){
    try {
    $svTabs.Add([pscustomobject]@{name=$svSheet.Name;index=$svSheet.Index;comments=$svSheet.Comments.Count;shapes=$svSheet.Shapes.Count;printArea=$svSheet.PageSetup.PrintArea})
    }finally{[void][Runtime.InteropServices.Marshal]::ReleaseComObject($svSheet)}
   }
   $svFormulaResults=New-Object System.Collections.Generic.List[object]
   $svInitiallySelected=@($svWorkbook.Windows.Item(1).SelectedSheets | ForEach-Object { $_.Name })
   if(@($svInitiallySelected | Where-Object { $_ -like '*_비교' }).Count -gt 0){throw 'Comparison sheet unexpectedly selected on open'}
   foreach($svFormula in $svItem.formulas){
    $svSheet=$svWorkbook.Worksheets.Item([string]$svFormula.sheet)
    try {
     $svCell=$svSheet.Range([string]$svFormula.cell)
     try {
      $svValue=[string]$svCell.Value2
      if($svValue -ne [string]$svFormula.expected){throw ('Native formula value mismatch: '+$svFormula.cell+' '+$svValue)}
      $svFormulaResults.Add([pscustomobject]@{sheet=$svFormula.sheet;cell=$svFormula.cell;value=$svValue;pass=$true})
     }finally{[void][Runtime.InteropServices.Marshal]::ReleaseComObject($svCell)}
    }finally{[void][Runtime.InteropServices.Marshal]::ReleaseComObject($svSheet)}
   }
   $svExports=New-Object System.Collections.Generic.List[object]
   foreach($svPreview in $svItem.previews){
    $svSheet=$svWorkbook.Worksheets.Item([string]$svPreview.sheet)
    try {
     $svSheet.Select()
     if($svWorkbook.Windows.Item(1).SelectedSheets.Count -ne 1){throw 'Preview requires one selected sheet'}
     $svOriginalArea=$svSheet.PageSetup.PrintArea
     $svSheet.PageSetup.PrintArea=[string]$svPreview.range
     $svSheet.PageSetup.Zoom=$false
     $svSheet.PageSetup.FitToPagesWide=1
     $svSheet.PageSetup.FitToPagesTall=$false
     $svSheet.ExportAsFixedFormat(0,[string]$svPreview.pdf,0,$true,$false)
     $svExports.Add([pscustomobject]@{sheet=$svPreview.sheet;range=$svPreview.range;pdf=$svPreview.pdf;scope=$svPreview.scope;originalPrintArea=$svOriginalArea})
     Write-Output ($svItem.code+' '+$svPreview.sheet+' exported')
    }finally{[void][Runtime.InteropServices.Marshal]::ReleaseComObject($svSheet)}
   }
   if((Get-FileHash -LiteralPath $svItem.path).Hash -ne $svHash){throw 'Read-only source changed'}
   $svRecords.Add([pscustomobject]@{code=$svItem.code;path=$svItem.path;sha256=$svHash;readOnly=$svWorkbook.ReadOnly;initiallySelectedSheets=$svInitiallySelected;worksheets=$svTabs;formulas=$svFormulaResults;previews=$svExports;unchanged=$true;checkedAt=(Get-Date).ToString('o')})
   $svRecords | ConvertTo-Json -Depth 10 | Set-Content -Encoding UTF8 -LiteralPath $svNative.recordPath
  }finally {
   if($null -ne $svWorkbook){$svWorkbook.Close($false);[void][Runtime.InteropServices.Marshal]::ReleaseComObject($svWorkbook)}
  }
 }
}finally{$svExcel.Quit();[void][Runtime.InteropServices.Marshal]::ReleaseComObject($svExcel)}
