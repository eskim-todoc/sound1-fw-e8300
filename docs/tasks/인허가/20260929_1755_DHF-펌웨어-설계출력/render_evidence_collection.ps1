$ErrorActionPreference = 'Stop'
$qa = Join-Path $PSScriptRoot 'qa/evidence-collection'
$baseline = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'baseline.json') | ConvertFrom-Json
$patches = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $qa 'patch-plan.json') | ConvertFrom-Json
$excel = $null
$book = $null
$results = @()
try {
    # Do not attach to or save the user's Excel instance.
    $excel = New-Object -ComObject Excel.Application
    $excel.Visible = $false
    $excel.DisplayAlerts = $false
    $excel.AskToUpdateLinks = $false
    $excel.AutomationSecurity = 3
    foreach ($b in $baseline.books) {
        $changes = @($patches | Where-Object key -eq $b.key)
        if ($changes.Count -eq 0) { continue }
        $book = $excel.Workbooks.Open((Join-Path $qa ($b.key + '-candidate.xlsx')), 0, $true)
        $sheet = $book.Worksheets.Item($b.sheet)
        $mainChanges = @($changes | Where-Object sheet -eq $b.sheet)
        $rows = @($mainChanges | ForEach-Object { [int]($_.cell -replace '[A-Z]', '') } | Sort-Object -Unique)
        $last = [string][char](64 + $b.maxCol)
        $first = if ($b.key -like '*report') { [string][char](64 + $b.common) } elseif ($b.key -like '*unit*') { 'H' } elseif ($b.key -like '*integration*') { 'J' } else { 'G' }
        # One contiguous range avoids Excel's multi-area PrintArea length limit.
        $areas = @($rows | ForEach-Object { $first + $_ + ':' + $last + $_ })
        $sheet.PageSetup.PrintArea = $first + $rows[0] + ':' + $last + $rows[-1]
        $sheet.PageSetup.PrintTitleRows = ''
        $sheet.PageSetup.PrintTitleColumns = ''
        $sheet.PageSetup.Orientation = 2
        $sheet.PageSetup.PaperSize = 9
        $sheet.PageSetup.Zoom = $false
        $sheet.PageSetup.FitToPagesWide = 1
        $sheet.PageSetup.FitToPagesTall = $false
        $sheet.PageSetup.LeftMargin = 12
        $sheet.PageSetup.RightMargin = 12
        $sheet.PageSetup.TopMargin = 12
        $sheet.PageSetup.BottomMargin = 12
        if ($areas.Count) { $sheet.ExportAsFixedFormat(0, (Join-Path $qa ($b.key + '-native.pdf')), 0, $true, $false) }
        $shapeCounts = @()
        foreach ($s in $book.Worksheets) { $shapeCounts += [pscustomobject]@{sheet=$s.Name; shapes=$s.Shapes.Count} }
        $results += [pscustomobject]@{key=$b.key; readonly=$book.ReadOnly; rows=$sheet.UsedRange.Rows.Count; columns=$sheet.UsedRange.Columns.Count; changedRows=$rows; sheets=$shapeCounts}
        $book.Close($false)
        [void][Runtime.InteropServices.Marshal]::ReleaseComObject($book)
        $book = $null
    }
    $results | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath (Join-Path $qa 'native-checks.json')
    $results | ConvertTo-Json -Depth 8
} finally {
    if ($null -ne $book) { $book.Close($false); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($book) }
    if ($null -ne $excel) { $excel.Quit(); [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excel) }
}
