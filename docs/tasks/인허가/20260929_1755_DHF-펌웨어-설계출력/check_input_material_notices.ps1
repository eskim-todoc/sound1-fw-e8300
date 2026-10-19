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
    foreach($item in $plan.edits) {
        $cell=$sheet.Range($item.cell)
        if([string]$cell.Value2 -cne [string]$item.after) { throw "Text differs: $($item.cell)" }
        $font=$cell.Characters($item.before.Length+3,$item.note.Length).Font
        if($font.Color -ne 192 -or $font.Size -ne ($item.base_size+2) -or $font.Underline -ne 2 -or $font.Bold) { throw "Notice font differs: $($item.cell)" }
        $ordinary=$cell.Characters(1,$item.before.Length).Font
        if($ordinary.Size -ne $item.base_size -or $ordinary.Bold) { throw "Ordinary text differs: $($item.cell)" }
        $scratch.Cells.Clear()|Out-Null
        $cell.Copy($scratch.Range('A1'))|Out-Null
        $scratch.Columns.Item(1).ColumnWidth=$sheet.Columns.Item($cell.Column).ColumnWidth
        $scratch.Rows.Item(1).AutoFit()|Out-Null
        $checks.Add(@{cell=$item.cell;row=$cell.Row;needed=[double]$scratch.Rows.Item(1).RowHeight;current=[double]$cell.EntireRow.RowHeight;size=$font.Size;color=$font.Color;bold=$font.Bold;underline=$font.Underline})
    }
    $scratch.Delete()|Out-Null
    $checks|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $Folder 'native-check.json')
    $sheet.PageSetup.Orientation=2
    $sheet.PageSetup.Zoom=$false
    $sheet.PageSetup.FitToPagesWide=1
    $sheet.PageSetup.FitToPagesTall=1
    $sheet.PageSetup.PrintTitleRows=''
    $sheet.PageSetup.PrintTitleColumns=''
    foreach($range in @('F20:I20','F49:I52','F55:I57','F60:I65','F69:I69')) {
        $sheet.PageSetup.PrintArea=$range
        $sheet.ExportAsFixedFormat(0,(Join-Path $Folder ('native-'+$range.Replace(':','_')+'.pdf')),0,$false,$false)
    }
    Write-Output ('Notice checks=12; insufficient heights='+@($checks|Where-Object {$_.needed -gt $_.current+1}).Count)
} finally {
    if($null -ne $book) {$book.Close($false)}
    $app.Quit()
    $null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
