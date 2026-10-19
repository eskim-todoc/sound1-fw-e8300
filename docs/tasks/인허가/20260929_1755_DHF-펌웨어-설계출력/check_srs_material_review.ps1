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
        $font=$cell.Characters($edit.requirement.Length+3,$edit.note.Length).Font
        $color=[Convert]::ToInt32($edit.color.Substring(0,2),16)+256*[Convert]::ToInt32($edit.color.Substring(2,2),16)+65536*[Convert]::ToInt32($edit.color.Substring(4,2),16)
        $bold=($edit.mode -eq 'e8300' -or $edit.base_bold)
        if($font.Color -ne $color -or $font.Size -ne $edit.notice_size -or [bool]$font.Bold -ne $bold) {throw "Notice formatting differs: $($edit.cell)"}
        if($edit.mode -ne 'e8300' -and $font.Underline -ne 2) {throw "Missing notice underline: $($edit.cell)"}
        if($edit.mode -eq 'e8300' -and $font.Underline -ne -4142) {throw "Unexpected E8300 underline: $($edit.cell)"}
        $ordinary=$cell.Characters(1,$edit.requirement.Length).Font
        if($ordinary.Size -ne $edit.base_size) {throw "Requirement font differs: $($edit.cell)"}
        $scratch.Cells.Clear()|Out-Null
        $cell.Copy($scratch.Range('A1'))|Out-Null
        $scratch.Columns.Item(1).ColumnWidth=$sheet.Columns.Item($cell.Column).ColumnWidth
        $scratch.Rows.Item(1).AutoFit()|Out-Null
        $checks.Add(@{cell=$edit.cell;id=$edit.id;row=$cell.Row;needed=[double]$scratch.Rows.Item(1).RowHeight;current=[double]$cell.EntireRow.RowHeight;mode=$edit.mode;size=$font.Size;color=$font.Color;bold=$font.Bold;underline=$font.Underline})
    }
    $scratch.Delete()|Out-Null
    $checks|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 -LiteralPath (Join-Path $Folder 'native-check.json')
    $sheet.PageSetup.Orientation=2
    $sheet.PageSetup.Zoom=$false
    $sheet.PageSetup.FitToPagesWide=1
    $sheet.PageSetup.FitToPagesTall=1
    $sheet.PageSetup.PrintTitleRows=''
    $sheet.PageSetup.PrintTitleColumns=''
    foreach($range in @('F20:I20','F49:I52','F55:I59','F60:I63','F64:I67','F69:I73','F83:I84')) {
        $sheet.PageSetup.PrintArea=$range
        $sheet.ExportAsFixedFormat(0,(Join-Path $Folder ('native-'+$range.Replace(':','_')+'.pdf')),0,$false,$false)
    }
    Write-Output ('Notice checks='+$checks.Count+'; insufficient heights='+@($checks|Where-Object {$_.needed -gt $_.current+1}).Count)
} finally {
    if($null -ne $book) {$book.Close($false)}
    $app.Quit()
    $null=[Runtime.InteropServices.Marshal]::ReleaseComObject($app)
}
