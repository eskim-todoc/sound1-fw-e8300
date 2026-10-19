param([Parameter(Mandatory=$true)][string]$Folder)
$ErrorActionPreference='Stop'
$plan=Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $Folder 'plan.json')|ConvertFrom-Json
$app=New-Object -ComObject Excel.Application
$app.Visible=$false
$app.DisplayAlerts=$false
$app.AskToUpdateLinks=$false
$app.AutomationSecurity=3
$book=$null
try {
    $book=$app.Workbooks.Open((Join-Path $Folder 'candidate.xlsx'),0,$true)
    $sheet=$book.Worksheets.Item($plan.sheet)
    $scratch=$book.Worksheets.Add()
    $checks=New-Object System.Collections.Generic.List[object]
    foreach($edit in $plan.edits) {
        $cell=$sheet.Range($edit.cell)
        if([string]$cell.Value2 -cne [string]$edit.after) {throw "Text differs: $($edit.cell)"}
        if($cell.Font.Size -ne 10 -or $cell.Font.Color -ne 0 -or $cell.Font.Bold -or $cell.Font.Underline -ne -4142) {throw "Ordinary formatting differs: $($edit.cell)"}
        $scratch.Cells.Clear()|Out-Null
        $cell.Copy($scratch.Range('A1'))|Out-Null
        $scratch.Columns.Item(1).ColumnWidth=$sheet.Columns.Item($cell.Column).ColumnWidth
        $scratch.Rows.Item(1).AutoFit()|Out-Null
        $needed=[double]$scratch.Rows.Item(1).RowHeight
        $current=[double]$cell.EntireRow.RowHeight
        $checks.Add(@{cell=$edit.cell;id=$edit.id;needed=$needed;current=$current;fits=($needed -le $current+1)})
    }
    $scratch.Delete()|Out-Null
    $checks|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $Folder 'native-check.json')
    $sheet.PageSetup.Orientation=2
    $sheet.PageSetup.Zoom=$false
    $sheet.PageSetup.FitToPagesWide=1
    $sheet.PageSetup.FitToPagesTall=1
    $sheet.PageSetup.PrintTitleRows=''
    $sheet.PageSetup.PrintTitleColumns=''
    foreach($range in @('F58:I59','F63:I63','F66:I66','F72:I72','F83:I83')) {
        $sheet.PageSetup.PrintArea=$range
        $sheet.ExportAsFixedFormat(0,(Join-Path $Folder ('native-'+$range.Replace(':','_')+'.pdf')),0,$false,$false)
    }
    Write-Output ('Six cell checks='+$checks.Count+'; insufficient heights='+@($checks|Where-Object {-not $_.fits}).Count)
} finally {
    if($null -ne $book) {$book.Close($false)}
    $app.Quit()
    $null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
